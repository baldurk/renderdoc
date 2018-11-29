import rdtest


class GL_DX_Interop(rdtest.TestCase):
    platform = 'win32'

    def get_capture(self):
        return rdtest.run_and_capture("demos_x64", "DX_Interop", 7)

    def check_capture(self):
        self.check_final_backbuffer()

        self.check_export(self.capture_filename)

