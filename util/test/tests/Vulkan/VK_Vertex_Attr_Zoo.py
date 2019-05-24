import copy
import rdtest
import renderdoc as rd


class VK_Vertex_Attr_Zoo(rdtest.TestCase):
    demos_test_name = 'VK_Vertex_Attr_Zoo'

    def check_capture(self):
        draw = self.find_draw("Draw")

        self.check(draw is not None)

        self.controller.SetFrameEvent(draw.eventId, False)

        # Make an output so we can pick pixels
        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        self.check(out is not None)

        ref = {
            0: {
                'SNorm': [1.0, -1.0, 1.0, -1.0],
                'UNorm': [12345.0/65535.0, 6789.0/65535.0, 1234.0/65535.0, 567.0/65535.0],
                'UScaled': [12345.0, 6789.0, 1234.0, 567.0],
                'UInt': [12345, 6789, 1234, 567],
                'Double': [9.8765432109, -5.6789012345],
                'Array[0]': [1.0, 2.0],
                'Array[1]': [3.0, 4.0],
                'Array[2]': [5.0, 6.0],
                'Matrix:row0': [7.0, 8.0],
                'Matrix:row1': [9.0, 10.0],
            },
            1: {
                'SNorm': [32766.0/32767.0, -32766.0/32767.0, 16000.0/32767.0, -16000.0/32767.0],
                'UNorm': [56.0/65535.0, 7890.0/65535.0, 123.0/65535.0, 4567.0/65535.0],
                'UScaled': [56.0, 7890.0, 123.0, 4567.0],
                'UInt': [56, 7890, 123, 4567],
                'Double': [-7.89012345678, 6.54321098765],
                'Array[0]': [11.0, 12.0],
                'Array[1]': [13.0, 14.0],
                'Array[2]': [15.0, 16.0],
                'Matrix:row0': [17.0, 18.0],
                'Matrix:row1': [19.0, 20.0],
            },
            2: {
                'SNorm': [5.0/32767.0, -5.0/32767.0, 0.0, 0.0],
                'UNorm': [8765.0/65535.0, 43210.0/65535.0, 987.0/65535.0, 65432.0/65535.0],
                'UScaled': [8765.0, 43210.0, 987.0, 65432.0],
                'UInt': [8765, 43210, 987, 65432],
                'Double': [0.1234567890123, 4.5678901234],
                'Array[0]': [21.0, 22.0],
                'Array[1]': [23.0, 24.0],
                'Array[2]': [25.0, 26.0],
                'Matrix:row0': [27.0, 28.0],
                'Matrix:row1': [29.0, 30.0],
            },
        }

        # Copy the ref values and prepend 'In'
        in_ref = {}
        for idx in ref:
            in_ref[idx] = {}
            for key in ref[idx]:
                in_ref[idx]['In' + key] = ref[idx][key]

        # Copy the ref values and prepend 'Out'
        out_ref = {}
        for idx in ref:
            out_ref[idx] = {}
            for key in ref[idx]:
                out_ref[idx]['Out' + key] = ref[idx][key]

        vsout_ref = copy.deepcopy(out_ref)
        gsout_ref = out_ref

        vsout_ref[0]['gl_PerVertex.gl_Position'] = [-0.5, 0.5, 0.0, 1.0]
        gsout_ref[0]['gl_PerVertex.gl_Position'] = [0.5, -0.5, 0.4, 1.2]

        vsout_ref[1]['gl_PerVertex.gl_Position'] = [0.0, -0.5, 0.0, 1.0]
        gsout_ref[1]['gl_PerVertex.gl_Position'] = [-0.5, 0.0, 0.4, 1.2]

        vsout_ref[2]['gl_PerVertex.gl_Position'] = [0.5, 0.5, 0.0, 1.0]
        gsout_ref[2]['gl_PerVertex.gl_Position'] = [0.5, 0.5, 0.4, 1.2]

        self.check_mesh_data(in_ref, self.get_vsin(draw))
        rdtest.log.success("Vertex input data is as expected")

        self.check_mesh_data(vsout_ref, self.get_postvs(rd.MeshDataStage.VSOut))

        rdtest.log.success("Vertex output data is as expected")

        # This is optional to account for drivers without XFB
        postgs_data = self.get_postvs(rd.MeshDataStage.GSOut)
        if len(postgs_data) > 0:
            self.check_mesh_data(gsout_ref, postgs_data)

            rdtest.log.success("Geometry output data is as expected")
        else:
            rdtest.log.print("Geometry output not tested")

        pipe: rd.PipeState = self.controller.GetPipelineState()

        tex = rd.TextureDisplay()
        tex.resourceId = pipe.GetOutputTargets()[0].resourceId
        out.SetTextureDisplay(tex)

        texdetails = self.get_texture(tex.resourceId)

        picked: rd.PixelValue = out.PickPixel(tex.resourceId, False,
                                              int(texdetails.width / 2), int(texdetails.height / 2), 0, 0, 0)

        if not rdtest.value_compare(picked.floatValue, [0.0, 1.0, 0.0, 1.0]):
            raise rdtest.TestFailureException("Picked value {} doesn't match expectation".format(picked.floatValue))

        rdtest.log.success("Triangle picked value is as expected")

        out.Shutdown()
