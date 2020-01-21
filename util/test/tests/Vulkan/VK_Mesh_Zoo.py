import rdtest
import renderdoc as rd


class VK_Mesh_Zoo(rdtest.TestCase):
    demos_test_name = 'VK_Mesh_Zoo'

    def __init__(self):
        rdtest.TestCase.__init__(self)
        self.zoo_helper = rdtest.Mesh_Zoo()

    def check_capture(self):
        self.zoo_helper.check_capture(self.capture_filename, self.controller)
