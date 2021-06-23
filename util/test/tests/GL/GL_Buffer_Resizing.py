import renderdoc as rd
import rdtest


class GL_Buffer_Resizing(rdtest.TestCase):
    demos_test_name = 'GL_Buffer_Resizing'
    demos_frame_cap = 10

    def check_capture(self):
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

        action = self.get_first_action()

        idx = 0

        while True:
            action: rd.ActionDescription = self.find_action('glDraw', action.eventId+1)

            if action is None:
                break

            self.controller.SetFrameEvent(action.eventId, True)

            self.check_triangle(out=action.outputs[0])

            postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

            self.check_mesh_data(postvs_ref, postvs_data)

            idx = idx + 1
            rdtest.log.success('Draw {} at {} is correct'.format(idx, action.eventId))
