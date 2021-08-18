import renderdoc as rd
import rdtest


class D3D12_Large_Buffer(rdtest.TestCase):
    demos_test_name = 'D3D12_Large_Buffer'

    def check_capture(self):
        action = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, False)

        vsin_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'POSITION': [-0.5, -0.5, 0.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [0.0, 0.0],
            },
            1: {
                'vtx': 1,
                'idx': 1000000,
                'POSITION': [0.0, 0.5, 0.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2345678,
                'POSITION': [0.5, -0.5, 0.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [1.0, 0.0],
            },
        }

        self.check_mesh_data(vsin_ref, self.get_vsin(action))

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'SV_POSITION': [-0.5, -0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [0.0, 0.0],
            },
            1: {
                'vtx': 1,
                'idx': 1000000,
                'SV_POSITION': [0.0, 0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2345678,
                'SV_POSITION': [0.5, -0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [1.0, 0.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

