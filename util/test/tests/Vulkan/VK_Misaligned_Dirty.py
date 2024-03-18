import renderdoc as rd
import struct
import rdtest


class VK_Misaligned_Dirty(rdtest.TestCase):
    demos_test_name = 'VK_Misaligned_Dirty'

    def get_capture_options(self):
        opts = rdtest.TestCase.get_capture_options(self)
        opts.apiValidation = True
        return opts

    def get_replay_options(self):
        opts = rdtest.TestCase.get_replay_options(self)
        # Set a balanced optimisation level to ensure that written ranges are cleared instead of being either restored
        # or ignored
        opts.optimisation = rd.ReplayOptimisationLevel.Balanced
        return opts

    def check_capture(self):
        action = self.find_action("Draw")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, False)

        self.check(len(self.controller.GetFrameInfo().debugMessages) == 0)
        self.check(len(self.controller.GetDebugMessages()) == 0)

        rdtest.log.success("No debug messages found")

        pipe: rd.PipeState = self.controller.GetPipelineState()

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        val = 2.0 / 3.0

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [-val, val, val, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_Position': [0.0, -val, val, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'gl_Position': [val, val, val, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        rdtest.log.success("vertex output is as expected")

        tex = pipe.GetOutputTargets()[0].resource

        texdetails = self.get_texture(tex)

        coords = [
            [int(texdetails.width * 1 / 3) + 5, int(texdetails.height * 2 / 3) - 5],
            [int(texdetails.width * 1 / 2) + 0, int(texdetails.height * 1 / 3) + 5],
            [int(texdetails.width * 2 / 3) + 5, int(texdetails.height * 2 / 3) - 5],
        ]

        for coord in coords:
            self.check_pixel_value(tex, coord[0], coord[1], [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("picked values are as expected")

        checkpoint1 = self.find_action("First Submit")
        checkpoint2 = self.find_action("Second Submit")
        checkpoint3 = self.find_action("Third Submit")

        self.check(checkpoint1 is not None)
        self.check(checkpoint2 is not None)
        self.check(checkpoint3 is not None)

        resources = self.controller.GetResources()

        copy_src = None
        vb = None

        for r in resources:
            if r.name == 'copy_src':
                copy_src = r.resourceId
            elif r.name == 'vb':
                vb = r.resourceId

        self.check(copy_src is not None)
        self.check(vb is not None)

        self.controller.SetFrameEvent(checkpoint1.eventId, False)

        val = struct.unpack('f', self.controller.GetBufferData(copy_src, 116, 4))
        self.check(val[0] == 11.0)

        self.controller.SetFrameEvent(checkpoint2.eventId, False)

        val = struct.unpack('f', self.controller.GetBufferData(copy_src, 116, 4))
        self.check(val[0] == 12.0)
        val = struct.unpack('f', self.controller.GetBufferData(vb, 116, 4))
        self.check(val[0] == 12.0)

        self.controller.SetFrameEvent(checkpoint3.eventId, False)

        val = struct.unpack('f', self.controller.GetBufferData(copy_src, 116, 4))
        self.check(val[0] == 11.0)

        rdtest.log.success("buffers have correct values in both submits")
