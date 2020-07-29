import renderdoc as rd
import rdtest


class D3D12_Existing_Heap(rdtest.TestCase):
    demos_test_name = 'D3D12_Existing_Heap'

    def check_capture(self):
        last_draw: rd.DrawcallDescription = self.get_last_draw()

        self.controller.SetFrameEvent(last_draw.eventId, True)

        self.check_triangle(out=last_draw.copyDestination)

        draw = self.find_draw("Draw")

        self.controller.SetFrameEvent(draw.eventId, False)

        postvs_data = self.get_postvs(draw, rd.MeshDataStage.VSOut, 0, draw.numIndices)

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
                'idx': 1,
                'SV_POSITION': [0.0, 0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'SV_POSITION': [0.5, -0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [1.0, 0.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)