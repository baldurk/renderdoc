import renderdoc as rd
import rdtest


class D3D11_Parameter_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Parameter_Zoo'
    demos_frame_cap = 50
    demos_frame_count = 10

    def check_capture(self):
        rdtest.log.success("Got {} captures as expected".format(self.demos_frame_count))

        action = self.find_action("Draw")
        self.check(action is not None)
        self.controller.SetFrameEvent(action.eventId, False)
        
        pipe: rd.PipeState = self.controller.GetPipelineState()

        tex = rd.TextureDisplay()
        tex.overlay = rd.DebugOverlay.Drawcall
        tex.resourceId = pipe.GetOutputTargets()[0].resourceId

        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100),
                                                            rd.ReplayOutputType.Texture)

        out.SetTextureDisplay(tex)

        out.Display()

        overlay_id = out.GetDebugOverlayTexID()

        v = pipe.GetViewport(0)

        self.check_pixel_value(overlay_id, int(0.5 * v.width), int(0.5 * v.height), [0.8, 0.1, 0.8, 1.0],
                               eps=1.0 / 256.0)

        out.Shutdown()

        rdtest.log.success("Overlay color is as expected")
