import renderdoc as rd
import rdtest
import os


class VK_RGP_Capture(rdtest.TestCase):
    demos_test_name = 'VK_Simple_Triangle'

    def check_capture(self):
        apiprops: rd.APIProperties = self.controller.GetAPIProperties()

        if not apiprops.rgpCapture:
            rdtest.log.print("RGP capture not tested")
            return

        path = self.controller.CreateRGPProfile(rd.CreateHeadlessWindowingData(100, 100))

        if os.path.exists(path) and os.path.getsize(path) > 100:
            rdtest.log.success("RGP capture created successfully")
        else:
            raise rdtest.TestFailureException("RGP capture failed")

