import renderdoc as rd
import rdtest


class VK_Descriptor_Variable_Count(rdtest.TestCase):
    demos_test_name = 'VK_Descriptor_Variable_Count'

    def check_capture(self):
        last_draw: rd.DrawcallDescription = self.get_last_draw()

        self.controller.SetFrameEvent(last_draw.eventId, True)

        self.check_triangle(out=last_draw.copyDestination)
