import renderdoc as rd
import rdtest


class VK_Descriptor_Variable_Count(rdtest.TestCase):
    demos_test_name = 'VK_Descriptor_Variable_Count'

    def check_capture(self):
        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        self.check_triangle(out=last_action.copyDestination)
