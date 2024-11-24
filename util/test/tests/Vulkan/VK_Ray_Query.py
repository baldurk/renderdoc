import renderdoc as rd
import rdtest

class VK_Ray_Query(rdtest.TestCase):
    demos_test_name = 'VK_Ray_Query'

    def check_capture(self):
        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        out = last_action.copyDestination

        background_color = [0.1, 0.1, 0.1, 1.0]
        shadow_color = [0.6, 0.6, 0.6, 1.0]
        tri_color = [1.0, 1.0, 1.0, 1.0]

        # inside edge of triangle
        self.check_pixel_value(out, 200, 193, tri_color)
        self.check_pixel_value(out, 143, 107, tri_color)
        self.check_pixel_value(out, 257, 107, tri_color)

        # outside edge of triangle, in shadow
        self.check_pixel_value(out, 200, 195, shadow_color)

        # outer edge of shadow
        self.check_pixel_value(out, 200, 220, shadow_color)

        # below tri not in shadow
        self.check_pixel_value(out, 200, 225, tri_color)

        # background coords outside of lower triangle
        self.check_pixel_value(out, 200, 29, background_color)
        self.check_pixel_value(out, 39, 271, background_color)
        self.check_pixel_value(out, 361, 271, background_color)
