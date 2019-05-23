import renderdoc as rd
import rdtest


class VK_Resource_Lifetimes(rdtest.TestCase):
    demos_test_name = 'VK_Resource_Lifetimes'

    def check_capture(self):
        self.check_final_backbuffer()
