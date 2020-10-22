import rdtest
import renderdoc as rd


class D3D12_Overlay_Test(rdtest.Overlay_Test):
    demos_test_name = 'D3D12_Overlay_Test'
    internal = False

    def check_capture(self):
        super(D3D12_Overlay_Test, self).check_capture()

        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        # Don't check any pixel values, but ensure all overlays at least work with no viewport/scissor bound
        sub_marker: rd.DrawcallDescription = self.find_draw("NoView draw")
        self.controller.SetFrameEvent(sub_marker.next.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        tex = rd.TextureDisplay()
        tex.resourceId = pipe.GetOutputTargets()[0].resourceId

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

        out.Shutdown()
