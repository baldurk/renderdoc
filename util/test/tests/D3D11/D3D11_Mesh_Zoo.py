import rdtest


class D3D11_Mesh_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Mesh_Zoo'

    def __init__(self):
        rdtest.TestCase.__init__(self)
        self.zoo_helper = rdtest.Mesh_Zoo()

    def check_capture(self):
        assert self.controller is not None
        self.zoo_helper.check_capture(self.capture_filename, self.controller)
