import rdtest
import renderdoc as rd


class VK_Overlay_Test(rdtest.Overlay_Test):
    demos_test_name = 'VK_Overlay_Test'
    internal = False

    def check_capture(self):
        # Check clear-before-action when first selecting a action, to ensure that bindless feedback doesn't interfere
        out = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        setup_marker = self.find_action("Setup")
        self.controller.SetFrameEvent(setup_marker.next.eventId, True)

        pipe = self.controller.GetPipelineState()

        tex = rd.TextureDisplay()
        tex.resourceId = pipe.GetOutputTargets()[0].resource

        tex.overlay = rd.DebugOverlay.ClearBeforeDraw
        out.SetTextureDisplay(tex)

        out.Display()

        # Select the next setup action
        self.controller.SetFrameEvent(setup_marker.next.eventId, True)

        # Select the real action for the first time
        self.controller.SetFrameEvent(self.find_action("Normal Test").next.eventId, True)

        self.check_pixel_value(tex.resourceId, 180, 150, [0.0, 0.0, 0.0, 0.0])
        self.check_pixel_value(tex.resourceId, 50, 50, [0.0, 0.0, 0.0, 0.0])
        self.check_pixel_value(tex.resourceId, 200, 64, [1.0, 1.0, 0.0, 1.0])

        # Clear the overlay to reset to a sensible state
        tex.overlay = rd.DebugOverlay.NoOverlay
        out.SetTextureDisplay(tex)

        out.Display()

        super(VK_Overlay_Test, self).check_capture()

        # Don't check any pixel values, but ensure all overlays at least work with rasterizer discard and no
        # viewport/scissor bound
        sub_marker = self.find_action("Discard Test")
        self.controller.SetFrameEvent(sub_marker.next.eventId, True)

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

            rdtest.log.success("Checking overlay {} with rasterizer discard".format(str(overlay)))

            tex.overlay = overlay
            out.SetTextureDisplay(tex)

            out.Display()

            overlay_id: rd.ResourceId = out.GetDebugOverlayTexID()

            rdtest.log.success("Overlay {} rendered with rasterizer discard".format(str(overlay)))

        out.Shutdown()