import renderdoc as rd
import rdtest


class GL_Resource_Lifetimes(rdtest.TestCase):
    demos_test_name = 'GL_Resource_Lifetimes'

    def check_capture(self):
        self.check_final_backbuffer()
