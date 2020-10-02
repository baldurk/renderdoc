import renderdoc as rd
import rdtest


class D3D11_Swapchain_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Swapchain_Zoo'
    demos_frame_cap = 50
    demos_frame_count = 10

    def check_capture(self):
        draw = self.find_draw("DrawIndexed")

        while draw is not None:
            self.controller.SetFrameEvent(draw.eventId, False)

            self.check_triangle(back=[0.0, 0.0, 0.0, 1.0])

            rdtest.log.success("OK at {}".format(draw.previous.name))

            draw = self.find_draw("DrawIndexed", draw.eventId+1)