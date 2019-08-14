import renderdoc as rd
import rdtest


class VK_Imageless_Framebuffer(rdtest.TestCase):
    demos_test_name = 'VK_Imageless_Framebuffer'

    def check_capture(self):
        draw = self.find_draw("Draw")

        self.check(draw is not None)

        self.controller.SetFrameEvent(draw.eventId, False)

        # Make an output so we can pick pixels
        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        tex = rd.TextureDisplay()
        tex.resourceId = pipe.GetOutputTargets()[0].resourceId
        out.SetTextureDisplay(tex)

        texdetails = self.get_texture(tex.resourceId)

        picked: rd.PixelValue = out.PickPixel(tex.resourceId, False,
                                              int(texdetails.width / 2), int(texdetails.height / 2), 0, 0, 0)

        if not rdtest.value_compare(picked.floatValue, [1.0, 0.0, 0.0, 1.0]):
            raise rdtest.TestFailureException("Picked value {} doesn't match expectation".format(picked.floatValue))

        rdtest.log.success("picked value is as expected")

        out.Shutdown()
