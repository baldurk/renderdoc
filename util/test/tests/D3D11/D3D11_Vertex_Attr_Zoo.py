import copy
import rdtest
import renderdoc as rd


class D3D11_Vertex_Attr_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Vertex_Attr_Zoo'

    def check_capture(self):
        action = self.find_action("Draw")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, False)

        ref = {
            0: {
                'SNORM': [1.0, -1.0, 1.0, -1.0],
                'UNORM': [12345.0/65535.0, 6789.0/65535.0, 1234.0/65535.0, 567.0/65535.0],
                'UINT': [12345, 6789, 1234, 567],
                'ARRAY0': [1.0, 2.0],
                'ARRAY1': [3.0, 4.0],
                'ARRAY2': [5.0, 6.0],
                'MATRIX0': [7.0, 8.0],
                'MATRIX1': [9.0, 10.0],
            },
            1: {
                'SNORM': [32766.0/32767.0, -32766.0/32767.0, 16000.0/32767.0, -16000.0/32767.0],
                'UNORM': [56.0/65535.0, 7890.0/65535.0, 123.0/65535.0, 4567.0/65535.0],
                'UINT': [56, 7890, 123, 4567],
                'ARRAY0': [11.0, 12.0],
                'ARRAY1': [13.0, 14.0],
                'ARRAY2': [15.0, 16.0],
                'MATRIX0': [17.0, 18.0],
                'MATRIX1': [19.0, 20.0],
            },
            2: {
                'SNORM': [5.0/32767.0, -5.0/32767.0, 0.0, 0.0],
                'UNORM': [8765.0/65535.0, 43210.0/65535.0, 987.0/65535.0, 65432.0/65535.0],
                'UINT': [8765, 43210, 987, 65432],
                'ARRAY0': [21.0, 22.0],
                'ARRAY1': [23.0, 24.0],
                'ARRAY2': [25.0, 26.0],
                'MATRIX0': [27.0, 28.0],
                'MATRIX1': [29.0, 30.0],
            },
        }

        in_ref = copy.deepcopy(ref)
        vsout_ref = copy.deepcopy(ref)
        gsout_ref = ref

        vsout_ref[0]['SV_Position'] = [-0.5, 0.5, 0.0, 1.0]
        gsout_ref[0]['SV_Position'] = [0.5, -0.5, 0.4, 1.2]

        vsout_ref[1]['SV_Position'] = [0.0, -0.5, 0.0, 1.0]
        gsout_ref[1]['SV_Position'] = [-0.5, 0.0, 0.4, 1.2]

        vsout_ref[2]['SV_Position'] = [0.5, 0.5, 0.0, 1.0]
        gsout_ref[2]['SV_Position'] = [0.5, 0.5, 0.4, 1.2]

        self.check_mesh_data(in_ref, self.get_vsin(action))
        rdtest.log.success("Vertex input data is as expected")

        self.check_mesh_data(vsout_ref, self.get_postvs(action, rd.MeshDataStage.VSOut))

        rdtest.log.success("Vertex output data is as expected")

        self.check_mesh_data(gsout_ref, self.get_postvs(action, rd.MeshDataStage.GSOut))

        rdtest.log.success("Geometry output data is as expected")

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Triangle picked value is as expected")
