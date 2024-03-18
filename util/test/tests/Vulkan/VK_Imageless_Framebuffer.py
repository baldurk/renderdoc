import renderdoc as rd
import rdtest


class VK_Imageless_Framebuffer(rdtest.TestCase):
    demos_test_name = 'VK_Imageless_Framebuffer'

    def check_capture(self):
        action = self.find_action("Draw")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("picked value is as expected")
