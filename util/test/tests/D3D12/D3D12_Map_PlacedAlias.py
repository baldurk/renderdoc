import renderdoc as rd
import rdtest


class D3D12_Map_PlacedAlias(rdtest.TestCase):
    demos_test_name = 'D3D12_Map_PlacedAlias'

    def check_capture(self):
        action = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, True)

        self.check_triangle()