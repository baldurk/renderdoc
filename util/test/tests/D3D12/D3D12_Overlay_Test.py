import rdtest
import renderdoc as rd


class D3D12_Overlay_Test(rdtest.Overlay_Test):
    demos_test_name = 'D3D12_Overlay_Test'
    internal = False

    def check_capture(self):
        out = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        for base_event_name in ["sm5.0", "sm5.1", "sm6.0"]:
            base = self.find_action(base_event_name)

            if base is None:
                continue

            base_event = base.eventId

            rdtest.log.print(f"Checking tests on {base_event_name}")

            super(D3D12_Overlay_Test, self).check_overlay_capture(base_event)

            rdtest.log.success(f"Base tests worked on {base_event_name}")

            # Don't check any pixel values, but ensure all overlays at least work with no viewport/scissor bound
            sub_marker = self.find_action("NoView draw", base_event)
            self.controller.SetFrameEvent(sub_marker.nextAction.eventId, True)

            pipe = self.controller.GetPipelineState()

            tex = rd.TextureDisplay()
            tex.resourceId = pipe.GetOutputTargets()[0].resource

            for overlay in rd.DebugOverlay:
                if overlay == rd.DebugOverlay.NoOverlay:
                    continue

                # These overlays are just displaymodes really, not actually separate overlays
                if overlay == rd.DebugOverlay.NaN or overlay == rd.DebugOverlay.Clipping:
                    continue

                if overlay == rd.DebugOverlay.ClearBeforeDraw or overlay == rd.DebugOverlay.ClearBeforePass:
                    continue

                rdtest.log.success(f"Checking overlay {overlay!s} with no viewport/scissor")

                tex.overlay = overlay
                out.SetTextureDisplay(tex)

                out.Display()

                overlay_id = out.GetDebugOverlayTexID()

                rdtest.log.success(f"Overlay {overlay!s} rendered with no viewport/scissor")

            rdtest.log.success(f"extended tests worked on {base_event_name}")

        out.Shutdown()
