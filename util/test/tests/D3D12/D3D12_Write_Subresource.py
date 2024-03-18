import renderdoc as rd
import rdtest


class D3D12_Write_Subresource(rdtest.TestCase):
    demos_test_name = 'D3D12_Write_Subresource'

    def check_capture(self):
        action = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        # Should be black around the sides, white in the centre
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.05, 0.05, [0.0, 0.0, 0.0, 0.0])
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, [1.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Picked values are as expected")