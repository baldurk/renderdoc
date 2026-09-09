import renderdoc as rd
import rdtest


class D3D12_Memory_Overlap(rdtest.TestCase):
    demos_test_name = 'D3D12_Memory_Overlap'

    def check_capture(self):
        last_action = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        self.check_triangle(out=last_action.copyDestination)
