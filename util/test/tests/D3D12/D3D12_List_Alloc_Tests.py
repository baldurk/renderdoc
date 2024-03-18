import struct
import math
import renderdoc as rd
import rdtest


class D3D12_List_Alloc_Tests(rdtest.TestCase):
    demos_test_name = 'D3D12_List_Alloc_Tests'
    demos_frame_cap = 40

    def check_capture(self):
        action = self.find_action("Draw")

        self.check(action is not None)

        action = action.next

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Captured loaded with color as expected")