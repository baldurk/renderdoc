from __future__ import annotations
from typing import Callable, List, Tuple

import renderdoc as rd
import rdtest


class ActionRef:
    def __init__(
        self,
        *,
        base: int,
        pos: List[Tuple[float, float, float]],
        pixels: List[List[Tuple[int, int]]],
        restarts: List[int] | None = None,
    ):
        self.base = base
        self.pos = pos
        self.pixels = pixels
        if restarts is None:
            self.restarts = []
        else:
            self.restarts = restarts


# Not a real test, re-used by API-specific tests
class Draw_Zoo(rdtest.TestCase):
    internal = True

    def vid(self, action: rd.ActionDescription, val: int):
        # For D3D, Vertex ID is either 0-based for non indexed calls, or the raw index. That means no baseVertex applied
        if rd.IsD3D(self.props.pipelineType):
            if action.flags & rd.ActionFlags.Indexed:
                return float(val - action.baseVertex)
            return float(val)

        # For GL and vulkan it includes all offsets - so for non-indexed that includes vertexOffset, and for indexed
        # that includes baseVertex
        if action.flags & rd.ActionFlags.Indexed:
            return float(val)
        return float(val + action.vertexOffset)

    def iid(self, action: rd.ActionDescription, val: int):
        # See above - similar for instance ID, but only vulkan includes the instanceOffset in instance ID

        if (self.props.pipelineType == rd.GraphicsAPI.Vulkan) and action.flags & rd.ActionFlags.Instanced:
            return float(val + action.instanceOffset)

        return float(val)

    def check_action(self, action: rd.ActionDescription, ref_data: ActionRef):
        rdtest.log.print(f"Checking action {action.eventId}")

        self.set_event(action.eventId, True)

        self.pipe = self.controller.GetPipelineState()

        refl = self.pipe.GetShaderReflection(rd.ShaderStage.Vertex)

        num_verts = len(ref_data.pos)

        vsin_pos_name = 'pos'
        for sig in refl.inputSignature:
            if 'pos' in sig.varName.lower() or 'pos' in sig.semanticName.lower():
                vsin_pos_name = sig.varName
                if vsin_pos_name == '':
                    vsin_pos_name = sig.semanticName
                break

        vsout_pos_name = 'pos'
        for sig in refl.outputSignature:
            if sig.systemValue == rd.ShaderBuiltin.Position:
                vsout_pos_name = sig.varName
                if vsout_pos_name == '':
                    vsout_pos_name = sig.semanticName
                break

        out_tex = self.pipe.GetOutputTargets()[0].resource

        vsin_ref: rdtest.MeshReference = {}
        restarts = ref_data.restarts

        ib = self.pipe.GetIBuffer()

        striprestart_index = self.pipe.GetRestartIndex() & ((1 << (ib.byteStride*8)) - 1)

        for v in range(num_verts):
            if v in restarts:
                vsin_ref[v] = {
                    'vtx': v,
                    'idx': striprestart_index,
                }
            else:
                vsin_ref[v] = {
                    'vtx': v,
                    'idx': ref_data.base+v,
                    vsin_pos_name: ref_data.pos[v],
                }

        self.check_mesh_data(vsin_ref, self.get_vsin(action))

        num_instances = action.numInstances
        if not (action.flags & rd.ActionFlags.Instanced):
            num_instances = 1

        rdtest.log.success("Checked vertex in data")

        for inst in range(num_instances):
            conv_out_pos: Callable[[rdtest.VectorValue], rdtest.VectorValue] = lambda x: [x[0] + float(inst) * 0.5, x[1], x[2], 1.0]

            vsout_ref: rdtest.MeshReference = {}

            for v in range(num_verts):
                if v in restarts:
                    vsout_ref[v] = {
                        'vtx': v,
                        'idx': striprestart_index,
                    }
                else:
                    vsout_ref[v] = {
                        'vtx': v,
                        'idx': ref_data.base+v,
                        vsout_pos_name: conv_out_pos(ref_data.pos[v]),
                        'VID': self.vid(action, ref_data.base+v),
                        'IID': self.iid(action, inst),
                    }

            postvs = self.get_postvs(action, rd.MeshDataStage.VSOut, instance=inst)

            self.check_mesh_data(vsout_ref, postvs)

            rdtest.log.success(f"Checked vertex out data in instance {inst}")

            for vtx in range(num_verts):
                if vtx in restarts:
                    continue

                idx = vsout_ref[vtx]['idx']

                assert isinstance(idx, int)

                self.check_vertex_debug(vtx, idx, inst, postvs)

            for vert, coord in enumerate(ref_data.pixels[inst]):
                if coord[0] == 0 and coord[1] == 0:
                    continue
                col = postvs[vert]['COLOR']
                tex = postvs[vert]['TEXCOORD']

                assert rdtest.is_vector(col)
                assert rdtest.is_vector(tex)

                val = (self.vid(action, ref_data.base + vert), self.iid(action, inst), float(inst) * 0.5,
                       col[1] + tex[0])
                self.check_pixel_value(out_tex, coord[0], coord[1], val, eps=0.3)

            rdtest.log.success(f"Checked pixels in instance {inst}")

        rdtest.log.success(f"Checked action {action.eventId}")

    def check_capture(self):
        test_marker = self.find_action("Test")
        assert test_marker
        self.check_capture_action(test_marker)

    def check_capture_action(self, marker: rd.ActionDescription):
        self.props = self.controller.GetAPIProperties()

        action = marker.nextAction
        assert action is not None

        rdtest.log.begin_section("Non-indexed, non-instanced cases")

        # Basic case
        ref = ActionRef(
            base=0,
            pos=[(-0.5, 0.5, 0.0), (0.0, -0.5, 0.0), (0.5, 0.5, 0.0)],
            pixels=[[(12, 12), (24, 34), (35, 12)]]
        )

        self.check_action(action, ref)
        assert action.vertexOffset == 0
        assert self.pipe.GetVBuffers()[0].byteOffset == 0
        action = action.nextAction
        assert action is not None

        # Vertex offset in the action
        ref = ActionRef(
            base=0,
            pos=[(-0.5, -0.5, 0.0), (0.0, 0.5, 0.0), (0.5, -0.5, 0.0)],
            pixels=[[(60, 35), (72, 13), (83, 35)]]
        )

        self.check_action(action, ref)
        assert action.vertexOffset > 0
        assert self.pipe.GetVBuffers()[0].byteOffset == 0
        action = action.nextAction
        assert action is not None

        # Vertex offset in action and in vertex binding
        ref = ActionRef(
            base=0,
            pos=[(-0.5, 0.0, 0.0), (0.0, -0.5, 0.0), (0.0, 0.5, 0.0)],
            pixels=[[(108, 23), (119, 35), (119, 13)]]
        )

        self.check_action(action, ref)
        assert action.vertexOffset > 0
        assert self.pipe.GetVBuffers()[0].byteOffset > 0
        action = action.nextAction
        assert action is not None

        rdtest.log.end_section("Non-indexed, non-instanced cases")

        rdtest.log.begin_section("indexed, non-instanced")

        # Basic case
        ref = ActionRef(
            base=0,
            pos=[(-0.5, 0.5, 0.0), (0.0, -0.5, 0.0), (0.5, 0.5, 0.0)],
            pixels=[[(12, 60), (24, 82), (35, 60)]]
        )

        self.check_action(action, ref)
        assert action.indexOffset == 0
        assert action.baseVertex == 0
        assert action.vertexOffset == 0
        assert self.pipe.GetVBuffers()[0].byteOffset == 0
        assert self.pipe.GetIBuffer().byteOffset == 0
        action = action.nextAction
        assert action is not None

        # first index in the action
        ref = ActionRef(
            base=5,
            pos=[(-0.5, -0.5, 0.0), (0.0, 0.5, 0.0), (0.5, -0.5, 0.0)],
            pixels=[[(60, 83), (72, 61), (83, 83)]]
        )

        self.check_action(action, ref)
        assert action.indexOffset > 0
        assert action.baseVertex == 0
        assert action.vertexOffset == 0
        assert self.pipe.GetVBuffers()[0].byteOffset == 0
        assert self.pipe.GetIBuffer().byteOffset == 0
        action = action.nextAction
        assert action is not None

        # first index and base vertex in the action
        ref = ActionRef(
            base=13,
            pos=[(-0.5, 0.0, 0.0), (0.0, -0.5, 0.0), (0.0, 0.5, 0.0)],
            pixels=[[(108, 71), (119, 83), (119, 61)]]
        )

        self.check_action(action, ref)
        assert action.indexOffset > 0
        assert action.baseVertex < 0
        assert action.vertexOffset == 0
        assert self.pipe.GetVBuffers()[0].byteOffset == 0
        assert self.pipe.GetIBuffer().byteOffset == 0
        action = action.nextAction
        assert action is not None

        # first index and base vertex in the action, and vertex binding offset
        ref = ActionRef(
            base=3,
            pos=[(-0.5, 0.0, 0.0), (0.0, -0.5, 0.0), (0.0, 0.5, 0.0)],
            pixels=[[(156, 71), (167, 83), (167, 61)]]
        )

        self.check_action(action, ref)
        assert action.indexOffset > 0
        assert action.baseVertex < 0
        assert action.vertexOffset == 0
        assert self.pipe.GetVBuffers()[0].byteOffset > 0
        assert self.pipe.GetIBuffer().byteOffset == 0
        action = action.nextAction
        assert action is not None

        # first index and base vertex in the action, and vertex & index binding offset
        ref = ActionRef(
            base=4,
            pos=[(0.0, -0.5, 0.0), (0.5, 0.0, 0.0), (0.0, 0.5, 0.0)],
            pixels=[[(216, 82), (226, 71), (216, 61)]]
        )

        self.check_action(action, ref)
        assert action.indexOffset > 0
        assert action.baseVertex < 0
        assert action.vertexOffset == 0
        assert self.pipe.GetVBuffers()[0].byteOffset > 0
        # OpenGL doesn't support offset on index buffer bindings
        if self.props.pipelineType != rd.GraphicsAPI.OpenGL:
            assert self.pipe.GetIBuffer().byteOffset > 0
        action = action.nextAction
        assert action is not None

        # Skip indexed strips for now
        ref = ActionRef(
            base=30,
            pos=[
                (-0.5, 0.2, 0.0), (-0.5, 0.0, 0.0),
                (-0.3, 0.2, 0.0), (-0.3, 0.0, 0.0),
                (-0.1, 0.2, 0.0),
                (0.0, 0.0, 0.0),  # restart
                (0.1, 0.2, 0.0), (0.1, 0.0, 0.0),
                (0.3, 0.2, 0.0), (0.3, 0.0, 0.0),
                (0.5, 0.2, 0.0), (0.5, 0.0, 0.0),
            ],
            restarts=[5],
            pixels=[[(252, 67), (252, 71), (256, 67)]]
        )

        self.check_action(action, ref)
        action = action.nextAction
        assert action is not None

        ref = ActionRef(
            base=30,
            pos=[
                (-0.5, 0.2, 0.0), (-0.5, 0.0, 0.0),
                (-0.3, 0.2, 0.0), (-0.3, 0.0, 0.0),
                (-0.1, 0.2, 0.0),
                (0.0, 0.0, 0.0),  # restart
                (0.1, 0.2, 0.0), (0.1, 0.0, 0.0),
                (0.3, 0.2, 0.0), (0.3, 0.0, 0.0),
                (0.5, 0.2, 0.0), (0.5, 0.0, 0.0),
            ],
            restarts=[5],
            pixels=[[(300, 67), (300, 71), (304, 67)]]
        )

        self.check_action(action, ref)
        action = action.nextAction
        assert action is not None

        rdtest.log.end_section("indexed, non-instanced")

        rdtest.log.begin_section("non-indexed, instanced")

        # Basic case
        ref = ActionRef(
            base=0,
            pos=[(-0.5, 0.5, 0.0), (0.0, -0.5, 0.0), (0.5, 0.5, 0.0)],
            pixels=[
                [(12, 108), (24, 130), (0, 0)],
                [(24, 108), (36, 130), (47, 108)],
            ]
        )

        self.check_action(action, ref)
        assert action.instanceOffset == 0
        assert self.pipe.GetVBuffers()[1].byteOffset == 0
        action = action.nextAction
        assert action is not None

        # instance offset in the action
        ref = ActionRef(
            base=0,
            pos=[(-0.5, -0.5, 0.0), (0.0, 0.5, 0.0), (0.5, -0.5, 0.0)],
            pixels=[
                [(60, 131), (72, 109), (0, 0)],
                [(72, 131), (84, 109), (95, 131)],
            ]
        )

        self.check_action(action, ref)
        assert action.instanceOffset > 0
        assert self.pipe.GetVBuffers()[1].byteOffset == 0
        action = action.nextAction
        assert action is not None

        # instance offset in the action and offset on the instanced VB
        ref = ActionRef(
            base=0,
            pos=[(-0.5, 0.0, 0.0), (0.0, -0.5, 0.0), (0.0, 0.5, 0.0)],
            pixels=[
                [(108, 120), (119, 131), (119, 108)],
                [(120, 120), (131, 131), (131, 108)],
            ]
        )

        self.check_action(action, ref)
        assert action.instanceOffset > 0
        assert self.pipe.GetVBuffers()[1].byteOffset > 0
        action = action.nextAction
        assert action is not None

        rdtest.log.end_section("non-indexed, instanced")

        rdtest.log.begin_section("indexed, instanced")

        # Basic case
        ref = ActionRef(
            base=5,
            pos=[(-0.5, -0.5, 0.0), (0.0, 0.5, 0.0), (0.5, -0.5, 0.0)],
            pixels=[
                [(12, 179), (24, 157), (0, 0)],
                [(24, 179), (36, 157), (47, 179)],
            ]
        )

        self.check_action(action, ref)
        assert action.instanceOffset == 0
        assert self.pipe.GetVBuffers()[1].byteOffset == 0
        action = action.nextAction
        assert action is not None

        # instance offset in the action
        ref = ActionRef(
            base=13,
            pos=[(-0.5, 0.0, 0.0), (0.0, -0.5, 0.0), (0.0, 0.5, 0.0)],
            pixels=[
                [(60, 168), (71, 179), (71, 156)],
                [(72, 168), (83, 179), (83, 156)],
            ]
        )

        self.check_action(action, ref)
        assert action.instanceOffset > 0
        assert self.pipe.GetVBuffers()[1].byteOffset == 0
        action = action.nextAction
        assert action is not None

        # instance offset in the action and offset on the instanced VB
        ref = ActionRef(
            base=23,
            pos=[(0.0, -0.5, 0.0), (0.5, 0.0, 0.0), (0.0, 0.5, 0.0)],
            pixels=[
                [(120, 178), (130, 168), (120, 157)],
                [(132, 178), (142, 168), (132, 157)],
            ]
        )

        self.check_action(action, ref)
        assert action.instanceOffset > 0
        assert self.pipe.GetVBuffers()[1].byteOffset > 0
        action = action.nextAction
        assert action is not None

        rdtest.log.end_section("indexed, instanced")
