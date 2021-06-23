import renderdoc as rd
import rdtest


class D3D12_Swapchain_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Swapchain_Zoo'

    def check_capture(self):
        action1 = self.find_action("Draw 1")
        action2 = self.find_action("Draw 2")

        self.controller.SetFrameEvent(action1.next.eventId, False)

        self.check_triangle(back=[0.0, 0.0, 0.0, 1.0])

        rdtest.log.success("Triangle in first action (on first window) is OK")

        self.controller.SetFrameEvent(action2.next.eventId, False)

        self.check_triangle(back=[0.0, 0.0, 0.0, 1.0])

        rdtest.log.success("Triangle in second action (on second window) is OK")