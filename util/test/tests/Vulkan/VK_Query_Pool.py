import renderdoc as rd
import rdtest


class VK_Query_Pool(rdtest.TestCase):
    demos_test_name = 'VK_Query_Pool'

    def check_capture(self):
        last_draw: rd.DrawcallDescription = self.get_last_draw()

        self.controller.SetFrameEvent(last_draw.eventId, True)

        self.check_triangle(out=last_draw.copyDestination)
