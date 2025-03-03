import rdtest
import renderdoc as rd


class VK_Buffer_Truncation(rdtest.Buffer_Truncation):
    demos_test_name = 'VK_Buffer_Truncation'
    internal = False
    draw_action = None

    def check_capture(self):
        self.draw_action = self.find_action("Draw")
        super().check_capture()

        rdtest.log.print("Repeating test with index buffer subrange bound via vkCmdBindIndexBuffer2KHR")
        self.draw_action = self.find_action("Draw", self.draw_action.eventId+1)
        super().check_capture()
