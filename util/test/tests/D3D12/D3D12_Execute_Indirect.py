import renderdoc as rd
import rdtest


class D3D12_Execute_Indirect(rdtest.TestCase):
    demos_test_name = 'D3D12_Execute_Indirect'

    def check_capture(self):
        from_eid = 0
        for i in range(8):
            action = self.find_action("IndirectDraw", from_eid)
            self.controller.SetFrameEvent(action.eventId, False)
            # Should be a green triangle in the centre of the screen on a red background
            self.check_triangle(back=[1.0, 0.0, 0.0, 1.0])
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
            from_eid = action.eventId + 1