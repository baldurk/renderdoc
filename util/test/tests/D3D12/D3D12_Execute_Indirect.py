import renderdoc as rd
import rdtest


class D3D12_Execute_Indirect(rdtest.TestCase):
    demos_test_name = 'D3D12_Execute_Indirect'

    def check_capture(self):
        action = self.find_action("IndirectDraw")

        self.controller.SetFrameEvent(action.eventId, False)

        # Should be a green triangle in the centre of the screen on a red background
        self.check_triangle(back=[1.0, 0.0, 0.0, 1.0])