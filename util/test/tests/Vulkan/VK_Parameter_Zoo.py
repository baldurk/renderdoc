import renderdoc as rd
import rdtest


class VK_Parameter_Zoo(rdtest.TestCase):
    demos_test_name = 'VK_Parameter_Zoo'

    def check_capture(self):
        draw = self.get_last_draw()

        self.check(draw is not None)

        draw = draw.previous

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])
