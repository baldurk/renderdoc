import struct
import math
import renderdoc as rd
import rdtest


class D3D11_Refcount_Check(rdtest.TestCase):
    demos_test_name = 'D3D11_Refcount_Check'

    def get_capture_options(self):
        # Ensure we enable API validation since the test relies on being able to query it
        ret = rd.CaptureOptions()

        ret.apiValidation = True
        ret.debugOutputMute = False

        return ret


    def check_capture(self):
        action = self.find_action("Color Draw")

        self.check(action is not None)

        action = action.next

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Captured loaded with color as expected")