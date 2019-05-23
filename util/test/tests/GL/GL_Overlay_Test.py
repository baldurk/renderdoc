import rdtest
import renderdoc as rd


class GL_Overlay_Test(rdtest.TestCase):
    demos_test_name = 'GL_Overlay_Test'

    def check_capture(self):
        self.check_final_backbuffer()

        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        self.check(out is not None)

        test_marker: rd.DrawcallDescription = self.find_draw("Test")

        self.controller.SetFrameEvent(test_marker.next.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        tex = rd.TextureDisplay()
        tex.resourceId = pipe.GetOutputTargets()[0].resourceId

        for overlay in rd.DebugOverlay:
            if overlay == rd.DebugOverlay.NoOverlay:
                continue

            # These overlays are just displaymodes really, not actually separate overlays
            if overlay == rd.DebugOverlay.NaN or overlay == rd.DebugOverlay.Clipping:
                continue

            # Unfortunately line-fill rendering seems to vary too much by IHV, so gives inconsistent results
            if overlay == rd.DebugOverlay.Wireframe:
                continue

            tex.overlay = overlay
            out.SetTextureDisplay(tex)

            overlay_path = rdtest.get_tmp_path(str(overlay) + '.png')
            ref_path = self.get_ref_path(str(overlay) + '.png')

            save_data = rd.TextureSave()
            save_data.resourceId = out.GetDebugOverlayTexID()
            save_data.destType = rd.FileType.PNG

            save_data.comp.blackPoint = 0.0
            save_data.comp.whitePoint = 1.0

            tolerance = 2

            # These overlays return grayscale above 1, so rescale to an expected range.
            if (overlay == rd.DebugOverlay.QuadOverdrawDraw or overlay == rd.DebugOverlay.QuadOverdrawPass or
                    overlay == rd.DebugOverlay.TriangleSizeDraw or overlay == rd.DebugOverlay.TriangleSizePass):
                save_data.comp.whitePoint = 10.0

            # These overlays modify the underlying texture, so we need to save it out instead of the overlay
            if overlay == rd.DebugOverlay.ClearBeforeDraw or overlay == rd.DebugOverlay.ClearBeforePass:
                save_data.resourceId = tex.resourceId

            self.controller.SaveTexture(save_data, overlay_path)

            if not rdtest.png_compare(overlay_path, ref_path, tolerance):
                raise rdtest.TestFailureException("Reference and output image differ for overlay {}".format(str(overlay)), overlay_path, ref_path)

            rdtest.log.success("Reference and output image are identical for {}".format(str(overlay)))

        save_data = rd.TextureSave()
        save_data.resourceId = pipe.GetDepthTarget().resourceId
        save_data.destType = rd.FileType.PNG
        save_data.channelExtract = 0

        tmp_path = rdtest.get_tmp_path('depth.png')
        ref_path = self.get_ref_path('depth.png')

        self.controller.SaveTexture(save_data, tmp_path)

        if not rdtest.png_compare(tmp_path, ref_path):
            raise rdtest.TestFailureException("Reference and output image differ for depth {}", tmp_path, ref_path)

        rdtest.log.success("Reference and output image are identical for depth")

        save_data.channelExtract = 1

        tmp_path = rdtest.get_tmp_path('stencil.png')
        ref_path = self.get_ref_path('stencil.png')

        self.controller.SaveTexture(save_data, tmp_path)

        if not rdtest.png_compare(tmp_path, ref_path):
            raise rdtest.TestFailureException("Reference and output image differ for stencil {}", tmp_path, ref_path)

        rdtest.log.success("Reference and output image are identical for stencil")

        out.Shutdown()
