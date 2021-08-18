import renderdoc as rd
import rdtest


class GL_Large_Buffer(rdtest.TestCase):
    demos_test_name = 'GL_Large_Buffer'

    def check_capture(self):
        action = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, False)

        vsin_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'Position': [-0.5, -0.5, 0.0],
                'Color': [0.0, 1.0, 0.0, 1.0],
                # GL may eliminate UV as unused
                #'UV': [0.0, 0.0],
            },
            1: {
                'vtx': 1,
                'idx': 1000000,
                'Position': [0.0, 0.5, 0.0],
                'Color': [0.0, 1.0, 0.0, 1.0],
                #'UV': [0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 61982400,
                'Position': [0.5, -0.5, 0.0],
                'Color': [0.0, 1.0, 0.0, 1.0],
                #'UV': [1.0, 0.0],
            },
        }

        self.check_mesh_data(vsin_ref, self.get_vsin(action))

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
                'idx': 1000000,
                'gl_Position': [0.0, 0.5, 0.0, 1.0],
                'v2f_block.pos': [0.0, 0.5, 0.0, 1.0],
                'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                'v2f_block.uv': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 61982400,
                'gl_Position': [0.5, -0.5, 0.0, 1.0],
                'v2f_block.pos': [0.5, -0.5, 0.0, 1.0],
                'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                'v2f_block.uv': [1.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)
