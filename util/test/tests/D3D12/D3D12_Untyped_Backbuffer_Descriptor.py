import renderdoc as rd
import rdtest


class D3D12_Untyped_Backbuffer_Descriptor(rdtest.TestCase):
    demos_test_name = 'D3D12_Untyped_Backbuffer_Descriptor'

    def check_capture(self):
        # find the first action
        action = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.25, 0.5, [1.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Picked value for first action is as expected")

        # find the second action
        action = self.find_action("Draw", action.eventId+1)

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.75, 0.5, [1.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Picked value for second action is as expected")
