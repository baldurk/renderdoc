import renderdoc as rd
import rdtest


class D3D11_Parameter_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Parameter_Zoo'
    demos_frame_cap = 50
    demos_frame_count = 10

    def check_capture(self):
        rdtest.log.success("Got {} captures as expected".format(self.demos_frame_count))
        # if we successfully got all the captures, so far that's all we care about
        pass
