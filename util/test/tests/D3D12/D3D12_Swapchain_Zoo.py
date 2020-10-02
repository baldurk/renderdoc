import renderdoc as rd
import rdtest


class D3D12_Swapchain_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Swapchain_Zoo'

    def check_capture(self):
        draw1 = self.find_draw("Draw 1")
        draw2 = self.find_draw("Draw 2")

        self.controller.SetFrameEvent(draw1.next.eventId, False)

        self.check_triangle(back=[0.0, 0.0, 0.0, 1.0])

        rdtest.log.success("Triangle in first draw (on first window) is OK")

        self.controller.SetFrameEvent(draw2.next.eventId, False)

        self.check_triangle(back=[0.0, 0.0, 0.0, 1.0])

        rdtest.log.success("Triangle in second draw (on second window) is OK")