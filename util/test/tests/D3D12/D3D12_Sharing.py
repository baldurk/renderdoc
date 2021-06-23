import renderdoc as rd
import rdtest


class D3D12_Sharing(rdtest.TestCase):
    demos_test_name = 'D3D12_Sharing'

    def check_capture(self):
        action = self.find_action("Copy")

        action: rd.ActionDescription = action.next

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        # Should be white in the top left, green in the bottom right, and red elsewhere
        self.check_pixel_value(action.copyDestination, 0.2, 0.2, [1.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(action.copyDestination, 0.3, 0.3, [1.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(action.copyDestination, 0.4, 0.4, [1.0, 1.0, 1.0, 1.0])

        self.check_pixel_value(action.copyDestination, 0.6, 0.6, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(action.copyDestination, 0.7, 0.7, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(action.copyDestination, 0.8, 0.8, [0.0, 1.0, 0.0, 1.0])

        self.check_pixel_value(action.copyDestination, 0.1, 0.6, [1.0, 0.0, 0.0, 1.0])
        self.check_pixel_value(action.copyDestination, 0.1, 0.7, [1.0, 0.0, 0.0, 1.0])
        self.check_pixel_value(action.copyDestination, 0.1, 0.8, [1.0, 0.0, 0.0, 1.0])

        self.check_pixel_value(action.copyDestination, 0.6, 0.1, [1.0, 0.0, 0.0, 1.0])
        self.check_pixel_value(action.copyDestination, 0.7, 0.1, [1.0, 0.0, 0.0, 1.0])
        self.check_pixel_value(action.copyDestination, 0.8, 0.1, [1.0, 0.0, 0.0, 1.0])

        rdtest.log.success("Picked values are as expected")