import renderdoc as rd
import rdtest


class VK_Query_Pool(rdtest.TestCase):
    demos_test_name = 'VK_Query_Pool'

    def check_capture(self):
        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        self.check_triangle(out=last_action.copyDestination)
