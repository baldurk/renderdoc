import renderdoc as rd
import rdtest


class VK_Misaligned_Dirty(rdtest.TestCase):
    demos_test_name = 'VK_Misaligned_Dirty'

    def get_replay_options(self):
        opts = rd.ReplayOptions()
        # Set a balanced optimisation level to ensure that written ranges are cleared instead of being either restored
        # or ignored
        opts.optimisation = rd.ReplayOptimisationLevel.Balanced
        return opts

    def check_capture(self):
        draw = self.find_draw("Draw")

        self.check(draw is not None)

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        postvs_data = self.get_postvs(rd.MeshDataStage.VSOut, 0, draw.numIndices)

        val = 2.0 / 3.0

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_PerVertex.gl_Position': [-val, val, val, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_PerVertex.gl_Position': [0.0, -val, val, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'gl_PerVertex.gl_Position': [val, val, val, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        rdtest.log.success("vertex output is as expected")

        tex = pipe.GetOutputTargets()[0].resourceId

        texdetails = self.get_texture(tex)

        coords = [
            [int(texdetails.width * 1 / 3) + 5, int(texdetails.height * 2 / 3) - 5],
            [int(texdetails.width * 1 / 2) + 0, int(texdetails.height * 1 / 3) + 5],
            [int(texdetails.width * 2 / 3) + 5, int(texdetails.height * 2 / 3) - 5],
        ]

        for coord in coords:
            self.check_pixel_value(tex, coord[0], coord[1], [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("picked values are as expected")
