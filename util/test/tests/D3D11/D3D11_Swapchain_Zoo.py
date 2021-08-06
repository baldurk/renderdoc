import renderdoc as rd
import rdtest


class D3D11_Swapchain_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Swapchain_Zoo'
    demos_frame_cap = 50
    # Take 10 captures because we don't know the present order, but only expect 1
    demos_frame_count = 10
    demos_captures_expected = 1

    def check_capture(self):
        action = self.find_action("DrawIndexed")

        while action is not None:
            self.controller.SetFrameEvent(action.eventId, False)

            self.check_triangle(back=[0.0, 0.0, 0.0, 1.0])

            rdtest.log.success("OK at {}".format(action.previous.customName))

            action = self.find_action("DrawIndexed", action.eventId+1)