import renderdoc as rd
import rdtest


class GL_Buffer_Spam(rdtest.TestCase):
    demos_test_name = 'GL_Buffer_Spam'

    def check_capture(self):
        # Check that export works
        self.check_export(self.capture_filename)

        action = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, False)

        # Check that we get green in the centre of the screen, indicating that the
        # triangle's buffer serialised with the right data and rendered

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])
