import copy
import rdtest
import renderdoc as rd


class VK_Vertex_Attr_Zoo(rdtest.TestCase):
    demos_test_name = 'VK_Vertex_Attr_Zoo'

    def check_capture(self):
        draw = self.find_draw("Draw")

        self.check(draw is not None)

        self.controller.SetFrameEvent(draw.eventId, False)

        ref = {
            0: {
                'SNorm': [1.0, -1.0, 1.0, -1.0],
                'UNorm': [12345.0/65535.0, 6789.0/65535.0, 1234.0/65535.0, 567.0/65535.0],
                'UScaled': [12345.0, 6789.0, 1234.0, 567.0],
                'UInt': [12345, 6789],
                'UInt1': [1234],
                'UInt2': [567],
                'Double': [9.8765432109, -5.6789012345],
                'Array[0]': [1.0, 2.0],
                'Array[1]': [3.0, 4.0],
                'Matrix:col0': [7.0, 8.0],
                'Matrix:col1': [9.0, 10.0],
            },
            1: {
                'SNorm': [32766.0/32767.0, -32766.0/32767.0, 16000.0/32767.0, -16000.0/32767.0],
                'UNorm': [56.0/65535.0, 7890.0/65535.0, 123.0/65535.0, 4567.0/65535.0],
                'UScaled': [56.0, 7890.0, 123.0, 4567.0],
                'UInt': [56, 7890],
                'UInt1': [123],
                'UInt2': [4567],
                'Double': [-7.89012345678, 6.54321098765],
                'Array[0]': [11.0, 12.0],
                'Array[1]': [13.0, 14.0],
                'Matrix:col0': [17.0, 18.0],
                'Matrix:col1': [19.0, 20.0],
            },
            2: {
                'SNorm': [5.0/32767.0, -5.0/32767.0, 0.0, 0.0],
                'UNorm': [8765.0/65535.0, 43210.0/65535.0, 987.0/65535.0, 65432.0/65535.0],
                'UScaled': [8765.0, 43210.0, 987.0, 65432.0],
                'UInt': [8765, 43210],
                'UInt1': [987],
                'UInt2': [65432],
                'Double': [0.1234567890123, 4.5678901234],
                'Array[0]': [21.0, 22.0],
                'Array[1]': [23.0, 24.0],
                'Matrix:col0': [27.0, 28.0],
                'Matrix:col1': [29.0, 30.0],
            },
        }

        doubles = self.find_draw('DoublesEnabled') is not None

        # Copy the ref values and prepend 'In'
        in_ref = {}
        for idx in ref:
            in_ref[idx] = {}
            for key in ref[idx]:
                if 'UInt' in key:
                    continue
                if not doubles and 'Double' in key:
                    continue
                in_ref[idx]['In' + key] = ref[idx][key]

            in_ref[idx]['InUInt2'] = ref[idx]['UInt'] + ref[idx]['UInt1'] + ref[idx]['UInt2']

        # Copy the ref values and prepend 'Out'
        out_ref = {}
        for idx in ref:
            out_ref[idx] = {}
            for key in ref[idx]:
                if not doubles and 'Double' in key:
                    continue
                out_ref[idx]['Out' + key] = ref[idx][key]

        vsout_ref = copy.deepcopy(out_ref)
        gsout_ref = out_ref

        vsout_ref[0]['gl_PerVertex_var.gl_Position'] = [-0.5, 0.5, 0.0, 1.0]
        gsout_ref[0]['gl_PerVertex_var.gl_Position'] = [0.5, -0.5, 0.4, 1.2]

        vsout_ref[1]['gl_PerVertex_var.gl_Position'] = [0.0, -0.5, 0.0, 1.0]
        gsout_ref[1]['gl_PerVertex_var.gl_Position'] = [-0.5, 0.0, 0.4, 1.2]

        vsout_ref[2]['gl_PerVertex_var.gl_Position'] = [0.5, 0.5, 0.0, 1.0]
        gsout_ref[2]['gl_PerVertex_var.gl_Position'] = [0.5, 0.5, 0.4, 1.2]

        self.check_mesh_data(in_ref, self.get_vsin(draw))
        rdtest.log.success("Vertex input data is as expected")

        self.check_mesh_data(vsout_ref, self.get_postvs(draw, rd.MeshDataStage.VSOut))

        rdtest.log.success("Vertex output data is as expected")

        # This is optional to account for drivers without XFB
        postgs_data = self.get_postvs(draw, rd.MeshDataStage.GSOut)
        if len(postgs_data) > 0:
            self.check_mesh_data(gsout_ref, postgs_data)

            rdtest.log.success("Geometry output data is as expected")
        else:
            rdtest.log.print("Geometry output not tested")

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Triangle picked value is as expected")

        # Step to the next draw with awkward struct/array outputs
        self.controller.SetFrameEvent(draw.next.eventId, False)

        ref = {
            0: {
                'outData.outStruct.a': [1.1],
                'outData.outStruct.c.foo[0]': [4.4],
                'outData.outStruct.c.foo[1]': [5.5],
                'outData.outStruct.d[0].foo': [6.6],
                'outData.outStruct.d[1].foo': [7.7],
                'outData.outStruct.b[0][0]': [2.2],
                'outData.outStruct.b[0][1]': [3.3],
                'outData.outStruct.b[0][2]': [8.8],
                'outData.outStruct.b[1][0]': [9.9],
                'outData.outStruct.b[1][1]': [9.1],
                'outData.outStruct.b[1][2]': [8.2],
            },
        }

        self.check_mesh_data(ref, self.get_postvs(draw, rd.MeshDataStage.VSOut))

        rdtest.log.success("Nested vertex output data is as expected")

        # The array-of-structs or array-of-arrays data is a broken in transform feedback
        del ref[0]['outData.outStruct.b[0][0]']
        del ref[0]['outData.outStruct.b[0][1]']
        del ref[0]['outData.outStruct.b[0][2]']
        del ref[0]['outData.outStruct.b[1][0]']
        del ref[0]['outData.outStruct.b[1][1]']
        del ref[0]['outData.outStruct.b[1][2]']

        del ref[0]['outData.outStruct.d[0].foo']
        del ref[0]['outData.outStruct.d[1].foo']

        self.check_mesh_data(ref, self.get_postvs(draw, rd.MeshDataStage.GSOut))

        rdtest.log.success("Nested geometry output data is as expected")
