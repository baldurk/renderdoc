import renderdoc as rd
import rdtest


class VK_Dedicated_Allocation(rdtest.TestCase):
    demos_test_name = 'VK_Dedicated_Allocation'

    def check_capture(self):
        action = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, True)

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [-0.5, 0.5, 0.0, 1.0],
                'vertOut.pos': [-0.5, 0.5, 0.0, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_Position': [0.0, -0.5, 0.0, 1.0],
                'vertOut.pos': [0.0, -0.5, 0.0, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'gl_Position': [0.5, 0.5, 0.0, 1.0],
                'vertOut.pos': [0.5, 0.5, 0.0, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [1.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        rdtest.log.success('Mesh data is as expected')

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 155, 195, [1.0, 0.0, 0.09, 1.0], eps=1.0/255.0)
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 190, 195, [0.0, 1.0, 0.09, 1.0], eps=1.0/255.0)
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 255, 195, [1.0, 0.0, 0.09, 1.0], eps=1.0/255.0)
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 230, 150, [0.723, 1.0, 1.0, 1.0], eps=1.0/255.0)

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 190, 80, [0.2, 0.2, 0.2, 1.0], eps=1.0/255.0)
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 200, 80, [0.723, 1.0, 1.0, 1.0], eps=1.0/255.0)
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 210, 80, [0.2, 0.2, 0.2, 1.0], eps=1.0/255.0)
