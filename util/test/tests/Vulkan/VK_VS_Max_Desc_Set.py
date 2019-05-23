import rdtest
import renderdoc as rd


class VK_VS_Max_Desc_Set(rdtest.TestCase):
    demos_test_name = 'VK_VS_Max_Desc_Set'

    def check_capture(self):

        draw = self.find_draw("Draw")

        self.check(draw is not None)

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        # We only need to check the color output for the first vertex - if we got that, the test succeeded.
        # We're not testing VS out fetch in general here, just that it works when there's no spare descriptor set
        postvs_data = self.get_postvs(rd.MeshDataStage.VSOut, 0, 1)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'vertOut.col': [1.0, 0.2, 0.75, 0.8],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)
