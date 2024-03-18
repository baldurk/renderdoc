import renderdoc as rd
import rdtest


class D3D11_Deferred_Map(rdtest.TestCase):
    demos_test_name = 'D3D11_Deferred_Map'

    def check_capture(self):
        # Check at the last action
        action = self.get_last_action()

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        # Top half should be red, bottom half should be green
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.25, [1.0, 0.0, 0.0, 1.0])
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.75, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Screen output is as expected")
