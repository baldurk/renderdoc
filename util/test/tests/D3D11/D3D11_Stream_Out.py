import rdtest
import struct
import renderdoc as rd


class D3D11_Stream_Out(rdtest.TestCase):
    demos_test_name = 'D3D11_Stream_Out'

    def check_capture(self):
        draw = self.find_draw("Draw")

        self.check(draw is not None)

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        # Get the input data as our reference

        # First draw should have data at offset 0. First buffer has positions, second has colors (with doubled stride)

        vsin = self.get_vsin(draw)

        pos = [(*v['POSITION'], 1.0) for v in vsin]
        col = [v['COLOR'] for v in vsin]

        d3d11pipe: rd.D3D11State = self.controller.GetD3D11PipelineState()

        so: rd.D3D11StreamOut = d3d11pipe.streamOut
        so_bytes = self.controller.GetBufferData(so.outputs[0].resourceId, so.outputs[0].byteOffset, 0)

        for i,p in enumerate(pos):
            so_p = struct.unpack_from("4f", so_bytes, 0 + 4*4*i)
            if not rdtest.value_compare(p, so_p):
                raise rdtest.TestFailureException("Streamed-out position {} doesn't match expected {}".format(so_p, p))

        so_bytes = self.controller.GetBufferData(so.outputs[1].resourceId, so.outputs[1].byteOffset, 0)

        for i,c in enumerate(col):
            so_c = struct.unpack_from("4f", so_bytes, 0 + 8*4*i)
            if not rdtest.value_compare(c, so_c):
                raise rdtest.TestFailureException("Streamed-out color {} doesn't match expected {}".format(so_c, c))

        draw_auto = self.find_draw("DrawAuto", draw.eventId)

        # First draw should be 3 vertices
        if not rdtest.value_compare(draw_auto.numIndices, 3):
            raise rdtest.TestFailureException("First DrawAuto() draws {} vertices".format(draw_auto.numIndices))

        draw_auto = self.find_draw("DrawAuto", draw_auto.eventId+1)

        # Second draw should be 6 vertices (3 vertices, instanced twice
        if not rdtest.value_compare(draw_auto.numIndices, 6):
            raise rdtest.TestFailureException("Second DrawAuto() draws {} vertices".format(draw_auto.numIndices))
        if not rdtest.value_compare(draw_auto.numInstances, 1):
            raise rdtest.TestFailureException("Second DrawAuto() draws {} instances".format(draw_auto.numInstances))

        rdtest.log.success("First draw stream-out data is correct")
