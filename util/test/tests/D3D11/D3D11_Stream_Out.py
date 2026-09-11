import rdtest
import struct


class D3D11_Stream_Out(rdtest.TestCase):
    demos_test_name = 'D3D11_Stream_Out'

    def check_capture(self):
        action = self.find_action("Draw")

        assert action is not None

        self.set_event(action.eventId, False)

        pipe = self.controller.GetPipelineState()

        # Get the input data as our reference

        # First action should have data at offset 0. First buffer has positions, second has colors (with doubled stride)

        vsin = self.get_vsin(action)

        pos = [
            (*v["POSITION"], 1.0)
            for v in vsin
            if isinstance(v["POSITION"], tuple) or isinstance(v["POSITION"], list)
        ]
        col = [v["COLOR"] for v in vsin]

        d3d11pipe = self.controller.GetD3D11PipelineState()

        so = d3d11pipe.streamOut
        so_bytes = self.controller.GetBufferData(so.outputs[0].resourceId, so.outputs[0].byteOffset, 0)

        for i,p in enumerate(pos):
            so_p = struct.unpack_from("4f", so_bytes, 0 + 4*4*i)
            if not rdtest.value_compare(p, so_p):
                raise rdtest.TestFailureException(f"Streamed-out position {so_p} doesn't match expected {p}")

        so_bytes = self.controller.GetBufferData(so.outputs[1].resourceId, so.outputs[1].byteOffset, 0)

        for i,c in enumerate(col):
            assert c is not None
            so_c = struct.unpack_from("4f", so_bytes, 0 + 8*4*i)
            if not rdtest.value_compare(c, so_c):
                raise rdtest.TestFailureException(f"Streamed-out color {so_c} doesn't match expected {c}")

        action_auto = self.find_action("DrawAuto", action.eventId)

        # First action should be 3 vertices
        if not rdtest.value_compare(action_auto.numIndices, 3):
            raise rdtest.TestFailureException(f"First DrawAuto() actions {action_auto.numIndices} vertices")

        action_auto = self.find_action("DrawAuto", action_auto.eventId+1)

        # Second action should be 6 vertices (3 vertices, instanced twice
        if not rdtest.value_compare(action_auto.numIndices, 6):
            raise rdtest.TestFailureException(f"Second DrawAuto() actions {action_auto.numIndices} vertices")
        if not rdtest.value_compare(action_auto.numInstances, 1):
            raise rdtest.TestFailureException(f"Second DrawAuto() actions {action_auto.numInstances} instances")

        rdtest.log.success("First action stream-out data is correct")
