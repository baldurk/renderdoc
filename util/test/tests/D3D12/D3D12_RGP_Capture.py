import renderdoc as rd
import rdtest
import os

try:
    import tkinter
except ImportError as ex:
    tkinter = None

class D3D12_RGP_Capture(rdtest.TestCase):
    demos_test_name = 'D3D12_Simple_Triangle'

    def check_support(self):
        if tkinter is None:
            return False, 'tkinter is required but not available'
        
        return super().check_support()

    def check_capture(self):
        apiprops: rd.APIProperties = self.controller.GetAPIProperties()

        if not apiprops.rgpCapture:
            rdtest.log.print("RGP capture not tested")
            return


        # On D3D12 we need to create a real window
        window = tkinter.Tk()
        window.geometry("1280x720")

        path = self.controller.CreateRGPProfile(rd.CreateWin32WindowingData(int(window.frame(), 16)))

        rdtest.log.print("RGP capture created: '{}'".format(path))

        if os.path.exists(path) and os.path.getsize(path) > 100:
            rdtest.log.success("RGP capture created successfully")
        else:
            raise rdtest.TestFailureException("RGP capture failed")

