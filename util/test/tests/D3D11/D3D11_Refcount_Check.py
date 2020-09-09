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
        draw = self.find_draw("Color Draw")

        self.check(draw is not None)

        draw = draw.next

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Captured loaded with color as expected")