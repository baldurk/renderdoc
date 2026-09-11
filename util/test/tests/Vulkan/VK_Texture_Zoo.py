import rdtest


class VK_Texture_Zoo(rdtest.TestCase):
    slow_test = True
    demos_test_name = 'VK_Texture_Zoo'

    def __init__(self):
        rdtest.TestCase.__init__(self)
        self.zoo_helper = rdtest.Texture_Zoo()

    def check_capture(self):
        assert self.controller is not None
        # This takes ownership of the controller and shuts it down when it's finished
        self.zoo_helper.worker_thread = self.worker_thread
        self.zoo_helper.check_capture(self.capture_filename, self)
        self.controller = None
