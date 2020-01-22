import renderdoc as rd
import rdtest


class D3D12_Execute_Indirect(rdtest.TestCase):
    demos_test_name = 'D3D12_Execute_Indirect'

    def check_capture(self):
        draw = self.find_draw("IndirectDraw")

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        # Should be a green triangle in the centre of the screen
        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.5, 0.3, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.3, 0.7, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.7, 0.7, [0.0, 1.0, 0.0, 1.0])

        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.3, 0.5, [1.0, 0.0, 0.0, 1.0])
        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.7, 0.5, [1.0, 0.0, 0.0, 1.0])
        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.5, 0.8, [1.0, 0.0, 0.0, 1.0])
        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.5, 0.2, [1.0, 0.0, 0.0, 1.0])

        rdtest.log.success("Picked values are as expected")