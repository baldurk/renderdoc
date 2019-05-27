import renderdoc as rd
import rdtest


class D3D11_Untyped_Backbuffer_Descriptor(rdtest.TestCase):
    demos_test_name = 'D3D11_Untyped_Backbuffer_Descriptor'

    def check_capture(self):
        # Make an output so we can pick pixels
        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        self.check(out is not None)

        # find the first draw
        draw = self.find_draw("Draw")

        # check the centre pixel of the viewport is white
        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        tex = rd.TextureDisplay()
        tex.resourceId = pipe.GetOutputTargets()[0].resourceId
        out.SetTextureDisplay(tex)

        view: rd.Viewport = pipe.GetViewport(0)

        picked: rd.PixelValue = out.PickPixel(tex.resourceId, False,
                                              int(view.width / 2), int(view.height / 2), 0, 0, 0)

        if not rdtest.value_compare(picked.floatValue, [1.0, 1.0, 1.0, 1.0]):
            raise rdtest.TestFailureException("Picked value {} doesn't match expectation".format(picked.floatValue))

        rdtest.log.success("Picked value for first draw is as expected")

        # find the second draw
        draw = self.find_draw("Draw", draw.eventId+1)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        tex.resourceId = pipe.GetOutputTargets()[0].resourceId
        out.SetTextureDisplay(tex)

        view: rd.Viewport = pipe.GetViewport(0)

        picked: rd.PixelValue = out.PickPixel(tex.resourceId, False,
                                              int(view.width / 2), int(view.height / 2), 0, 0, 0)

        if not rdtest.value_compare(picked.floatValue, [1.0, 1.0, 1.0, 1.0]):
            raise rdtest.TestFailureException("Picked value {} doesn't match expectation".format(picked.floatValue))

        rdtest.log.success("Picked value for second draw is as expected")
