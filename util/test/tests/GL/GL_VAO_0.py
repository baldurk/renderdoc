import rdtest
import renderdoc as rd


class GL_VAO_0(rdtest.TestCase):
    demos_test_name = 'GL_VAO_0'

    def check_capture(self):
        self.check_final_backbuffer()

        draw = self.find_draw("Draw")

        # There are 4 draws with variations on client-memory VBs or IBs
        for i in range(0, 4):
            self.check(draw is not None)

            self.controller.SetFrameEvent(draw.eventId, False)

            postvs_data = self.get_postvs(rd.MeshDataStage.VSOut, 0, draw.numIndices)

            postvs_ref = {
                0: {
                    'vtx': 0,
                    'idx': 0,
                    'gl_Position': [-0.5, -0.5, 0.0, 1.0],
                    'v2f_block.pos': [-0.5, -0.5, 0.0, 1.0],
                    'v2f_block.col': [1.0, 0.0, 0.0, 1.0],
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
                    'v2f_block.col': [0.0, 0.0, 1.0, 1.0],
                    'v2f_block.uv': [1.0, 0.0, 0.0, 1.0],
                },
            }

            self.check_mesh_data(postvs_ref, postvs_data)

            draw = draw.next

