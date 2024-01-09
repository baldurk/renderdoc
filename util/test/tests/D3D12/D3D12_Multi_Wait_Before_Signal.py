import renderdoc as rd
import rdtest

class D3D12_Multi_Wait_Before_Signal(rdtest.TestCase):
    demos_test_name = 'D3D12_Multi_Wait_Before_Signal'

    def check_capture(self):
        draw_marker: rd.ActionDescription = self.find_action("Last draw")
        self.controller.SetFrameEvent(draw_marker.previous.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        tex = pipe.GetOutputTargets()[0].resourceId
        self.check_pixel_value(tex, 270, 194, [0.20117, 0.20117, 0.20117, 0.0])
        self.check_pixel_value(tex, 180, 170, [0.5031, 0.25, 1.0, 1.0])
        self.check_pixel_value(tex, 180, 194, [0.25, 0.75391, 1.0, 1.0])

