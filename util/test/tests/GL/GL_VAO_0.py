import rdtest
import renderdoc as rd


class GL_VAO_0(rdtest.TestCase):
    demos_test_name = 'GL_VAO_0'

    def check_capture(self):
        action = self.find_action("Draw")

        # There are 4 actions with variations on client-memory VBs or IBs
        for i in range(0, 4):
            self.check(action is not None)

            self.controller.SetFrameEvent(action.eventId, False)

            pipe: rd.PipeState = self.controller.GetPipelineState()
            vp: rd.Viewport = pipe.GetViewport(0)

            self.check_triangle(vp=(vp.x, vp.y, vp.width, vp.height))

            postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

            postvs_ref = {
                0: {
                    'vtx': 0,
                    'idx': 0,
                    'gl_Position': [-0.5, -0.5, 0.0, 1.0],
                    'v2f_block.pos': [-0.5, -0.5, 0.0, 1.0],
                    'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                    'v2f_block.uv': [0.0, 0.0, 0.0, 1.0],
                },
                1: {
                    'vtx': 1,
                    'idx': 1,
                    'gl_Position': [0.0, 0.5, 0.0, 1.0],
                    'v2f_block.pos': [0.0, 0.5, 0.0, 1.0],
                    'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                    'v2f_block.uv': [0.0, 1.0, 0.0, 1.0],
                },
                2: {
                    'vtx': 2,
                    'idx': 2,
                    'gl_Position': [0.5, -0.5, 0.0, 1.0],
                    'v2f_block.pos': [0.5, -0.5, 0.0, 1.0],
                    'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                    'v2f_block.uv': [1.0, 0.0, 0.0, 1.0],
                },
            }

            self.check_mesh_data(postvs_ref, postvs_data)

            action = action.next

        action = self.find_action("Instanced")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, False)

        # Each instance should have color output of 0.5 * instance in blue
        for i in range(0, action.numInstances):
            postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices, i)

            postvs_ref = {
                0: {
                    'vtx': 0,
                    'idx': 0,
                    'v2f_block.col': [0.0, 0.0, 0.5*(i+1), 0.0],
                },
            }

            self.check_mesh_data(postvs_ref, postvs_data)

            rdtest.log.success('Instance {} is OK'.format(i))
