import renderdoc as rd
import rdtest


class D3D12_Resource_Lifetimes(rdtest.TestCase):
    demos_test_name = 'D3D12_Resource_Lifetimes'

    def check_capture(self):
        self.check_final_backbuffer()
