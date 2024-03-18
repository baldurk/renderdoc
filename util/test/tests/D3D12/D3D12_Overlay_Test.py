import rdtest
import renderdoc as rd


class D3D12_Overlay_Test(rdtest.Overlay_Test):
    demos_test_name = 'D3D12_Overlay_Test'
    internal = False

    def check_capture(self):
        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        for base_event_name in ["sm5.0", "sm5.1", "sm6.0"]:
            base = self.find_action(base_event_name)

            if base is None:
                continue

            base_event = base.eventId

            rdtest.log.print("Checking tests on {}".format(base_event_name))

            super(D3D12_Overlay_Test, self).check_capture(base_event)

            rdtest.log.success("Base tests worked on {}".format(base_event_name))

            # Don't check any pixel values, but ensure all overlays at least work with no viewport/scissor bound
            sub_marker: rd.ActionDescription = self.find_action("NoView draw", base_event)
            self.controller.SetFrameEvent(sub_marker.next.eventId, True)

            pipe: rd.PipeState = self.controller.GetPipelineState()

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

                rdtest.log.success("Checking overlay {} with no viewport/scissor".format(str(overlay)))

                tex.overlay = overlay
                out.SetTextureDisplay(tex)

                out.Display()

                overlay_id: rd.ResourceId = out.GetDebugOverlayTexID()

                rdtest.log.success("Overlay {} rendered with no viewport/scissor".format(str(overlay)))

            rdtest.log.success("extended tests worked on {}".format(base_event_name))

        out.Shutdown()
