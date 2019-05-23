import renderdoc as rd
import rdtest


class GL_Separable_Geometry_Shaders(rdtest.TestCase):
    demos_test_name = 'GL_Separable_Geometry_Shaders'

    def check_capture(self):
        draw = self.find_draw("Draw")

        self.controller.SetFrameEvent(draw.eventId, False)

        postvs_data = self.get_postvs(rd.MeshDataStage.VSOut, 0, draw.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [-0.5, -0.5, 0.0, 1.0],
                'v2f_block.col': [1.0, 0.0, 0.0, 1.0],
                'v2f_block.uv': [0.0, 0.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_Position': [0.0, 0.5, 0.0, 1.0],
                'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                'v2f_block.uv': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'gl_Position': [0.5, -0.5, 0.0, 1.0],
                'v2f_block.col': [0.0, 0.0, 1.0, 1.0],
                'v2f_block.uv': [1.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        postgs_data = self.get_postvs(rd.MeshDataStage.GSOut, 0, draw.numIndices*3)

        postgs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [0.2, -0.5, 0.0, 1.0],
                'v2f_block.col': [1.0, 0.0, 0.0, 1.0],
                'v2f_block.uv': [0.0, 0.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_Position': [0.7, 0.5, 0.0, 1.0],
                'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                'v2f_block.uv': [0.0, 1.0, 0.0, 1.0],
            },
            4: {
                'vtx': 4,
                'idx': 4,
                'gl_Position': [-0.7, 0.5, 0.0, 1.0],
                'v2f_block.col': [1.0, 0.0, 1.0, 0.0],
                'v2f_block.uv': [0.0, 1.0, 0.0, 1.0],
            },
            5: {
                'vtx': 5,
                'idx': 5,
                'gl_Position': [-0.2, -0.5, 0.0, 1.0],
                'v2f_block.col': [1.0, 1.0, 0.0, 0.0],
                'v2f_block.uv': [1.0, 0.0, 0.0, 1.0],
            },
            8: {
                'vtx': 8,
                'idx': 8,
                'gl_Position': [0.5, 0.2, 0.0, 1.0],
                'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                'v2f_block.uv': [1.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postgs_ref, postgs_data)
