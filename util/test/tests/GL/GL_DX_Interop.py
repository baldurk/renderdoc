import renderdoc as rd
import rdtest


class GL_DX_Interop(rdtest.TestCase):
    demos_test_name = 'GL_DX_Interop'
    demos_frame_cap = 4

    def check_capture(self):
        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        out = last_action.copyDestination

        # There should be N+1 rings of green/red with the base colour in the middle
        x,y = 1.0, 1.0
        for ring in range(self.demos_frame_cap+1):
            self.check_pixel_value(out, (0.5 - 0.5 * x) + 0.01, (0.5 - 0.5 * y) + 0.01, [1.0, 0.0, 0.0, 1.0])
            self.check_pixel_value(out, (0.5 - 0.5 * x) + 0.01, (0.5 + 0.5 * y) - 0.01, [1.0, 0.0, 0.0, 1.0])
            self.check_pixel_value(out, (0.5 + 0.5 * x) - 0.01, (0.5 - 0.5 * y) + 0.01, [1.0, 0.0, 0.0, 1.0])
            self.check_pixel_value(out, (0.5 + 0.5 * x) - 0.01, (0.5 + 0.5 * y) - 0.01, [1.0, 0.0, 0.0, 1.0])

            x *= 0.8
            y *= 0.8

            self.check_pixel_value(out, (0.5 - 0.5 * x) + 0.01, (0.5 - 0.5 * y) + 0.01, [0.0, 1.0, 0.0, 1.0])
            self.check_pixel_value(out, (0.5 - 0.5 * x) + 0.01, (0.5 + 0.5 * y) - 0.01, [0.0, 1.0, 0.0, 1.0])
            self.check_pixel_value(out, (0.5 + 0.5 * x) - 0.01, (0.5 - 0.5 * y) + 0.01, [0.0, 1.0, 0.0, 1.0])
            self.check_pixel_value(out, (0.5 + 0.5 * x) - 0.01, (0.5 + 0.5 * y) - 0.01, [0.0, 1.0, 0.0, 1.0])

            x *= 0.8
            y *= 0.8

        self.check_pixel_value(out, 0.5, 0.5, [0.2, 0.2, 0.2, 1.0])
