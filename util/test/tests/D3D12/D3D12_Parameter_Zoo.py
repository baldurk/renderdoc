import struct
import math
import renderdoc as rd
import rdtest


class D3D12_Parameter_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Parameter_Zoo'

    def check_capture(self):
        action = self.find_action("Color Draw")

        self.check(action is not None)

        action = action.next

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Captured loaded with color as expected")

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'SV_POSITION': [-0.5, -0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [0.0, 0.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'SV_POSITION': [0.0, 0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'SV_POSITION': [0.5, -0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [1.0, 0.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        rdtest.log.success("Mesh data is correct")

        tex = rd.TextureDisplay()
        tex.overlay = rd.DebugOverlay.Drawcall
        tex.resourceId = pipe.GetOutputTargets()[0].resource

        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100),
                                                            rd.ReplayOutputType.Texture)

        out.SetTextureDisplay(tex)

        out.Display()

        overlay_id = out.GetDebugOverlayTexID()

        v = pipe.GetViewport(0)

        self.check_pixel_value(overlay_id, int(0.5 * v.width), int(0.5 * v.height), [0.8, 0.1, 0.8, 1.0],
                               eps=1.0 / 256.0)

        out.Shutdown()

        res = self.get_resource_by_name("Sampler Heap")

        sdfile = self.controller.GetStructuredFile()

        chunk = sdfile.chunks[res.initialisationChunks[-1]]

        desc1234 = chunk.GetChild(2).GetChild(1234).GetChild(3)

        rdtest.log.comment('desc1234: ' + desc1234.name)

        # filter
        self.check(desc1234.GetChild(0).AsString() == 'D3D12_FILTER_ANISOTROPIC')
        self.check(desc1234.GetChild(0).AsInt() == 0x55)

        # wrapping
        self.check(desc1234.GetChild(1).AsString() == 'D3D12_TEXTURE_ADDRESS_MODE_BORDER')
        self.check(desc1234.GetChild(1).AsInt() == 4)

        # MaxAnisotropy
        self.check(desc1234.GetChild(1).AsInt() == 4)

        # MinLod
        self.check(desc1234.GetChild(8).AsFloat() == 1.5)

        rdtest.log.success("Overlay color is as expected")