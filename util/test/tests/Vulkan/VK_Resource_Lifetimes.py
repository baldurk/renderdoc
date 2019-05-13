import renderdoc as rd
import rdtest


class VK_Resource_Lifetimes(rdtest.TestCase):
    def get_capture(self):
        return rdtest.run_and_capture("demos_x64", "VK_Resource_Lifetimes", 5)

    def check_capture(self):
        self.check_final_backbuffer()
