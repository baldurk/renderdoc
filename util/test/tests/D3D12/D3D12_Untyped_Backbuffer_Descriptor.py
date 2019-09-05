import renderdoc as rd
import rdtest


class D3D12_Untyped_Backbuffer_Descriptor(rdtest.TestCase):
    demos_test_name = 'D3D12_Untyped_Backbuffer_Descriptor'

    def check_capture(self):
        # find the first draw
        draw = self.find_draw("Draw")

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.25, 0.5, [1.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Picked value for first draw is as expected")

        # find the second draw
        draw = self.find_draw("Draw", draw.eventId+1)

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.75, 0.5, [1.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Picked value for second draw is as expected")
