import rdtest


class GL_DX_Interop(rdtest.TestCase):
    demos_test_name = 'GL_DX_Interop'
    demos_frame_cap = 7

    def check_capture(self):
        self.check_final_backbuffer()

