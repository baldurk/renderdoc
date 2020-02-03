import renderdoc as rd
import rdtest


class VK_Resource_Lifetimes(rdtest.TestCase):
    demos_test_name = 'VK_Resource_Lifetimes'
    demos_frame_cap = 200

    def check_capture(self):
        self.check_final_backbuffer()

        # Check for resource leaks
        if len(self.controller.GetResources()) > 75:
            raise rdtest.TestFailureException(
                "Too many resources found: {}".format(len(self.controller.GetResources())))

