import renderdoc as rd
import rdtest


class D3D12_Sharing(rdtest.TestCase):
    demos_test_name = 'D3D12_Sharing'

    def check_capture(self):
        markers = ["Draw", "Copy"]
        for marker in markers:
            action = self.find_action(marker)

            action: rd.ActionDescription = action.next

            self.controller.SetFrameEvent(action.eventId, False)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            rdtest.log.print(f"Checking Event: {marker}")
            draw = marker == "Draw"
            target = action.outputs[0] if draw else action.copyDestination

            # Draw : Should be red in the top left
            # Copy : Should be white in the top left
            expected = [1.0, 0.0, 0.0, 1.0] if draw else [1.0, 1.0, 1.0, 1.0]
            self.check_pixel_value(target, 0.2, 0.2, expected)
            self.check_pixel_value(target, 0.3, 0.3, expected)
            self.check_pixel_value(target, 0.4, 0.4, expected)

            # green in the bottom right
            expected = [0.0, 1.0, 0.0, 1.0]
            self.check_pixel_value(target, 0.6, 0.6, expected)
            self.check_pixel_value(target, 0.7, 0.7, expected)
            self.check_pixel_value(target, 0.8, 0.8, expected)

            # red elsewhere
            expected = [1.0, 0.0, 0.0, 1.0]
            self.check_pixel_value(target, 0.1, 0.6, expected)
            self.check_pixel_value(target, 0.1, 0.7, expected)
            self.check_pixel_value(target, 0.1, 0.8, expected)

            expected = [1.0, 0.0, 0.0, 1.0]
            self.check_pixel_value(target, 0.6, 0.1, expected)
            self.check_pixel_value(target, 0.7, 0.1, expected)
            self.check_pixel_value(target, 0.8, 0.1, expected)

        rdtest.log.success("Picked values are as expected")