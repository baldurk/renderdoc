import renderdoc as rd
import rdtest
from typing import List, Tuple
import time
import os


# Not a direct test, re-used by API-specific tests
class Overlay_Test(rdtest.TestCase):
    internal = True

    def check_capture(self):
        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        self.check(out is not None)

        test_marker: rd.DrawcallDescription = self.find_draw("Test")

        self.controller.SetFrameEvent(test_marker.next.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()
        
        col_tex: rd.ResourceId = pipe.GetOutputTargets()[0].resourceId

        tex = rd.TextureDisplay()
        tex.resourceId = col_tex

        # Check the actual output is as expected first.

        # Background around the outside
        self.check_pixel_value(col_tex, 0.1, 0.1, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(col_tex, 0.8, 0.1, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(col_tex, 0.5, 0.95, [0.2, 0.2, 0.2, 1.0])

        # Large dark grey triangle
        self.check_pixel_value(col_tex, 0.5, 0.1, [0.1, 0.1, 0.1, 1.0])
        self.check_pixel_value(col_tex, 0.5, 0.9, [0.1, 0.1, 0.1, 1.0])
        self.check_pixel_value(col_tex, 0.2, 0.9, [0.1, 0.1, 0.1, 1.0])
        self.check_pixel_value(col_tex, 0.8, 0.9, [0.1, 0.1, 0.1, 1.0])

        # Red upper half triangle
        self.check_pixel_value(col_tex, 0.3, 0.4, [1.0, 0.0, 0.0, 1.0])
        # Blue lower half triangle
        self.check_pixel_value(col_tex, 0.3, 0.6, [0.0, 0.0, 1.0, 1.0])

        # Floating clipped triangle
        self.check_pixel_value(col_tex, 335, 140, [0.0, 0.0, 0.0, 1.0])
        self.check_pixel_value(col_tex, 340, 140, [0.2, 0.2, 0.2, 1.0])

        # Triangle size triangles
        self.check_pixel_value(col_tex, 200, 51, [1.0, 0.5, 1.0, 1.0])
        self.check_pixel_value(col_tex, 200, 65, [1.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(col_tex, 200, 79, [0.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(col_tex, 200, 93, [0.0, 1.0, 0.0, 1.0])

        for overlay in rd.DebugOverlay:
            if overlay == rd.DebugOverlay.NoOverlay:
                continue

            # These overlays are just displaymodes really, not actually separate overlays
            if overlay == rd.DebugOverlay.NaN or overlay == rd.DebugOverlay.Clipping:
                continue

            # We'll test the clear-before-X overlays seperately, for both colour and depth
            if overlay == rd.DebugOverlay.ClearBeforeDraw or overlay == rd.DebugOverlay.ClearBeforePass:
                continue

            rdtest.log.print("Checking overlay {} in main draw".format(str(overlay)))

            tex.overlay = overlay
            out.SetTextureDisplay(tex)

            out.Display()

            eps = 1.0 / 256.0

            overlay_id: rd.ResourceId = out.GetDebugOverlayTexID()

            # We test a few key spots:
            #  4 points along the left side of the big triangle, above/in/below its intersection with the tri behind
            #  4 points outside all triangles
            #  The overlap between the big tri and the bottom tri, and between it and the right backface culled tri
            #  The bottom tri's part that sticks out
            #  The two parts of the backface culled tri that stick out
            #  The depth clipped tri, in and out of clipping
            #  The 4 triangle size test triangles

            if overlay == rd.DebugOverlay.Drawcall:
                self.check_pixel_value(overlay_id, 150, 90, [0.8, 0.1, 0.8, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 130, [0.8, 0.1, 0.8, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 160, [0.8, 0.1, 0.8, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 200, [0.8, 0.1, 0.8, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 125, 60, [0.0, 0.0, 0.0, 0.5], eps=eps)
                self.check_pixel_value(overlay_id, 125, 250, [0.0, 0.0, 0.0, 0.5], eps=eps)
                self.check_pixel_value(overlay_id, 250, 60, [0.0, 0.0, 0.0, 0.5], eps=eps)
                self.check_pixel_value(overlay_id, 250, 250, [0.0, 0.0, 0.0, 0.5], eps=eps)

                self.check_pixel_value(overlay_id, 220, 175, [0.8, 0.1, 0.8, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 150, [0.8, 0.1, 0.8, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 190, [0.8, 0.1, 0.8, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 285, 135, [0.8, 0.1, 0.8, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 285, 165, [0.8, 0.1, 0.8, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 330, 145, [0.8, 0.1, 0.8, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 340, 145, [0.8, 0.1, 0.8, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 200, 51, [0.8, 0.1, 0.8, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 65, [0.8, 0.1, 0.8, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 79, [0.8, 0.1, 0.8, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 93, [0.8, 0.1, 0.8, 1.0], eps=eps)
            elif overlay == rd.DebugOverlay.Wireframe:
                # Wireframe we only test a limited set to avoid hitting implementation variations of line raster
                # We also have to fudge a little because the lines might land on adjacent pixels

                x = 142
                picked: rd.PixelValue = self.controller.PickPixel(overlay_id, x, 150, rd.Subresource(), rd.CompType.Typeless)
                if picked.floatValue[3] == 0.0:
                    x = 141

                self.check_pixel_value(overlay_id, x, 90, [200.0/255.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, x, 130, [200.0/255.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, x, 160, [200.0/255.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, x, 200, [200.0/255.0, 1.0, 0.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 125, 60, [200.0/255.0, 1.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 125, 250, [200.0/255.0, 1.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 60, [200.0/255.0, 1.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 250, [200.0/255.0, 1.0, 0.0, 0.0], eps=eps)

                y = 149
                picked: rd.PixelValue = self.controller.PickPixel(overlay_id, 325, y, rd.Subresource(), rd.CompType.Typeless)
                if picked.floatValue[3] == 0.0:
                    y = 150

                self.check_pixel_value(overlay_id, 325, y, [200.0/255.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 340, y, [200.0/255.0, 1.0, 0.0, 1.0], eps=eps)
            elif overlay == rd.DebugOverlay.Depth:
                self.check_pixel_value(overlay_id, 150, 90, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 130, [0.0, 1.0, 0.0, 1.0], eps=eps)
                # Intersection with lesser depth - depth fail
                self.check_pixel_value(overlay_id, 150, 160, [1.0, 0.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 200, [0.0, 1.0, 0.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 125, 60, [0.0, 1.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 125, 250, [0.0, 1.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 60, [0.0, 1.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 250, [0.0, 1.0, 0.0, 0.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 175, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 150, [0.0, 1.0, 0.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 190, [0.0, 1.0, 0.0, 1.0], eps=eps)

                # Backface culled triangle
                self.check_pixel_value(overlay_id, 285, 135, [1.0, 0.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 285, 165, [1.0, 0.0, 0.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 330, 145, [0.0, 1.0, 0.0, 1.0], eps=eps)
                # Depth clipped part of tri
                self.check_pixel_value(overlay_id, 340, 145, [1.0, 0.0, 0.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 200, 51, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 65, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 79, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 93, [0.0, 1.0, 0.0, 1.0], eps=eps)
            elif overlay == rd.DebugOverlay.Stencil:
                self.check_pixel_value(overlay_id, 150, 90, [0.0, 1.0, 0.0, 1.0], eps=eps)
                # Intersection with different stencil - stencil fail
                self.check_pixel_value(overlay_id, 150, 130, [1.0, 0.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 160, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 200, [0.0, 1.0, 0.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 125, 60, [0.0, 1.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 125, 250, [0.0, 1.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 60, [0.0, 1.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 250, [0.0, 1.0, 0.0, 0.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 175, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 150, [0.0, 1.0, 0.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 190, [0.0, 1.0, 0.0, 1.0], eps=eps)

                # Backface culled triangle
                self.check_pixel_value(overlay_id, 285, 135, [1.0, 0.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 285, 165, [1.0, 0.0, 0.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 330, 145, [0.0, 1.0, 0.0, 1.0], eps=eps)
                # Depth clipped part of tri
                self.check_pixel_value(overlay_id, 340, 145, [1.0, 0.0, 0.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 200, 51, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 65, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 79, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 93, [0.0, 1.0, 0.0, 1.0], eps=eps)
            elif overlay == rd.DebugOverlay.BackfaceCull:
                self.check_pixel_value(overlay_id, 150, 90, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 130, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 160, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 200, [0.0, 1.0, 0.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 125, 60, [0.0, 1.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 125, 250, [0.0, 1.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 60, [0.0, 1.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 250, [0.0, 1.0, 0.0, 0.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 175, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 150, [0.0, 1.0, 0.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 190, [0.0, 1.0, 0.0, 1.0], eps=eps)

                # Backface culled triangle
                self.check_pixel_value(overlay_id, 285, 135, [1.0, 0.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 285, 165, [1.0, 0.0, 0.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 330, 145, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 340, 145, [0.0, 1.0, 0.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 200, 51, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 65, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 79, [0.0, 1.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 93, [0.0, 1.0, 0.0, 1.0], eps=eps)
            elif overlay == rd.DebugOverlay.ViewportScissor:
                # Inside viewport
                self.check_pixel_value(overlay_id, 50, 50, [0.2*0.7, 0.2*0.7, 0.9*0.7, 0.7*0.7], eps=eps)
                self.check_pixel_value(overlay_id, 350, 50, [0.2*0.7, 0.2*0.7, 0.9*0.7, 0.7*0.7], eps=eps)
                self.check_pixel_value(overlay_id, 50, 250, [0.2*0.7, 0.2*0.7, 0.9*0.7, 0.7*0.7], eps=eps)
                self.check_pixel_value(overlay_id, 350, 250, [0.2*0.7, 0.2*0.7, 0.9*0.7, 0.7*0.7], eps=eps)

                # Viewport border
                self.check_pixel_value(overlay_id, 12, 12, [0.1, 0.1, 0.1, 1.0], eps=eps)

                # Outside viewport (not on scissor border)
                self.check_pixel_value(overlay_id, 6, 6, [0.0, 0.0, 0.0, 0.0], eps=eps)

                # Scissor border
                self.check_pixel_value(overlay_id, 0, 0, [1.0, 1.0, 1.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 20, 0, [0.0, 0.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 40, 0, [1.0, 1.0, 1.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 60, 0, [0.0, 0.0, 0.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 60, 0, [0.0, 0.0, 0.0, 1.0], eps=eps)
            elif overlay == rd.DebugOverlay.QuadOverdrawDraw:
                self.check_pixel_value(overlay_id, 150, 90, [1.0, 1.0, 1.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 130, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 160, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 200, [1.0, 1.0, 1.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 125, 60, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 125, 250, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 60, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 250, [0.0, 0.0, 0.0, 0.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 175, [2.0, 2.0, 2.0, 2.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 150, [1.0, 1.0, 1.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 190, [1.0, 1.0, 1.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 285, 135, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 285, 165, [0.0, 0.0, 0.0, 0.0], eps=eps)

                self.check_pixel_value(overlay_id, 330, 145, [1.0, 1.0, 1.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 340, 145, [0.0, 0.0, 0.0, 0.0], eps=eps)

                self.check_pixel_value(overlay_id, 200, 51, [1.0, 1.0, 1.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 65, [1.0, 1.0, 1.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 79, [1.0, 1.0, 1.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 93, [1.0, 1.0, 1.0, 1.0], eps=eps)
            elif overlay == rd.DebugOverlay.QuadOverdrawPass:
                self.check_pixel_value(overlay_id, 150, 90, [1.0, 1.0, 1.0, 1.0], eps=eps)

                # Do an extra tap here where we overlap with the extreme-background largest triangle, to show that
                # overdraw
                self.check_pixel_value(overlay_id, 150, 100, [2.0, 2.0, 2.0, 2.0], eps=eps)

                self.check_pixel_value(overlay_id, 150, 130, [1.0, 1.0, 1.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 160, [1.0, 1.0, 1.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 200, [2.0, 2.0, 2.0, 2.0], eps=eps)

                # Two of these have overdraw from the pass due to the large background triangle
                self.check_pixel_value(overlay_id, 125, 60, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 125, 250, [1.0, 1.0, 1.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 60, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 250, [1.0, 1.0, 1.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 175, [3.0, 3.0, 3.0, 3.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 150, [2.0, 2.0, 2.0, 2.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 190, [2.0, 2.0, 2.0, 2.0], eps=eps)

                self.check_pixel_value(overlay_id, 285, 135, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 285, 165, [1.0, 1.0, 1.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 330, 145, [1.0, 1.0, 1.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 340, 145, [0.0, 0.0, 0.0, 0.0], eps=eps)

                self.check_pixel_value(overlay_id, 200, 51, [2.0, 2.0, 2.0, 2.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 65, [2.0, 2.0, 2.0, 2.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 79, [2.0, 2.0, 2.0, 2.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 93, [2.0, 2.0, 2.0, 2.0], eps=eps)
            elif overlay == rd.DebugOverlay.TriangleSizeDraw:
                eps = 1.0

                self.check_pixel_value(overlay_id, 150, 90, [10632.0, 10632.0, 10632.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 130, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 160, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 200, [10632.0, 10632.0, 10632.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 125, 60, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 125, 250, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 60, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 250, [0.0, 0.0, 0.0, 0.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 175, [2128.0, 2128.0, 2128.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 150, [10632.0, 10632.0, 10632.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 190, [2128.0, 2128.0, 2128.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 285, 135, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 285, 165, [0.0, 0.0, 0.0, 0.0], eps=eps)

                self.check_pixel_value(overlay_id, 330, 145, [531.0, 531.0, 531.0, 531.0], eps=eps)
                self.check_pixel_value(overlay_id, 340, 145, [0.0, 0.0, 0.0, 0.0], eps=eps)

                eps = 0.01

                self.check_pixel_value(overlay_id, 200, 51, [8.305, 8.305, 8.305, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 65, [5.316, 5.316, 5.316, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 79, [3.0, 3.0, 3.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 93, [1.33, 1.33, 1.33, 1.0], eps=eps)
            elif overlay == rd.DebugOverlay.TriangleSizePass:
                eps = 1.0

                self.check_pixel_value(overlay_id, 150, 90, [10632.0, 10632.0, 10632.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 130, [3324.0, 3324.0, 3324.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 160, [3324.0, 3324.0, 3324.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 150, 200, [10632.0, 10632.0, 10632.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 125, 60, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 125, 250, [43072.0, 43072.0, 43072.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 60, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 250, [43072.0, 43072.0, 43072.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 175, [2128.0, 2128.0, 2128.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 250, 150, [10632.0, 10632.0, 10632.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 220, 190, [2128.0, 2128.0, 2128.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 285, 135, [0.0, 0.0, 0.0, 0.0], eps=eps)
                self.check_pixel_value(overlay_id, 285, 165, [43072.0, 43072.0, 43072.0, 1.0], eps=eps)

                self.check_pixel_value(overlay_id, 330, 145, [531.0, 531.0, 531.0, 531.0], eps=eps)
                self.check_pixel_value(overlay_id, 340, 145, [0.0, 0.0, 0.0, 0.0], eps=eps)

                eps = 0.01

                self.check_pixel_value(overlay_id, 200, 51, [8.305, 8.305, 8.305, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 65, [5.316, 5.316, 5.316, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 79, [3.0, 3.0, 3.0, 1.0], eps=eps)
                self.check_pixel_value(overlay_id, 200, 93, [1.33, 1.33, 1.33, 1.0], eps=eps)

            rdtest.log.success("Picked pixels are as expected for {}".format(str(overlay)))

        # Now check clear-before-X by hand, for colour and for depth

        depth_tex: rd.ResourceId = pipe.GetDepthTarget().resourceId

        eps = 1.0/256.0

        # Check colour and depth before-hand
        self.check_pixel_value(col_tex, 250, 250, [0.1, 0.1, 0.1, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 125, 125, [1.0, 0.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 125, 175, [0.0, 0.0, 1.0, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 50, 50, [0.2, 0.2, 0.2, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 291, 150, [0.977, 0.977, 0.977, 1.0], eps=0.075)
        self.check_pixel_value(col_tex, 200, 51, [1.0, 0.5, 1.0, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 200, 65, [1.0, 1.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 200, 79, [0.0, 1.0, 1.0, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 200, 93, [0.0, 1.0, 0.0, 1.0], eps=eps)

        eps = 0.001

        self.check_pixel_value(depth_tex, 160, 135, [0.9, 85.0/255.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(depth_tex, 160, 165, [0.0, 0.0/255.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(depth_tex, 250, 150, [0.5, 85.0/255.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(depth_tex, 250, 250, [0.95, 0.0/255.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(depth_tex, 50, 50, [1.0, 0.0/255.0, 0.0, 1.0], eps=eps)

        # Check clear before pass
        tex.resourceId = col_tex
        tex.overlay = rd.DebugOverlay.ClearBeforePass
        out.SetTextureDisplay(tex)
        out.Display()

        eps = 1.0/256.0

        self.check_pixel_value(col_tex, 250, 250, [0.1, 0.1, 0.1, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 125, 125, [1.0, 0.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 125, 175, [0.0, 0.0, 1.0, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 50, 50, [0.0, 0.0, 0.0, 0.0], eps=eps)
        self.check_pixel_value(col_tex, 291, 150, [0.977, 0.977, 0.977, 1.0], eps=0.075)
        self.check_pixel_value(col_tex, 200, 51, [1.0, 0.5, 1.0, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 200, 65, [1.0, 1.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 200, 79, [0.0, 1.0, 1.0, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 200, 93, [0.0, 1.0, 0.0, 1.0], eps=eps)

        tex.resourceId = depth_tex
        tex.overlay = rd.DebugOverlay.ClearBeforePass
        out.SetTextureDisplay(tex)
        out.Display()

        eps = 0.001

        self.check_pixel_value(depth_tex, 160, 135, [0.9, 85.0/255.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(depth_tex, 160, 165, [0.0, 0.0/255.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(depth_tex, 250, 150, [0.5, 85.0/255.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(depth_tex, 250, 250, [0.95, 0.0/255.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(depth_tex, 50, 50, [1.0, 0.0/255.0, 0.0, 1.0], eps=eps)

        rdtest.log.success("Clear before pass colour and depth values as expected")

        # Check clear before draw
        tex.resourceId = col_tex
        tex.overlay = rd.DebugOverlay.ClearBeforeDraw
        out.SetTextureDisplay(tex)
        out.Display()

        eps = 1.0/256.0

        # These are all pass triangles, should be cleared
        self.check_pixel_value(col_tex, 250, 250, [0.0, 0.0, 0.0, 0.0], eps=eps)
        self.check_pixel_value(col_tex, 125, 125, [0.0, 0.0, 0.0, 0.0], eps=eps)
        self.check_pixel_value(col_tex, 125, 175, [0.0, 0.0, 0.0, 0.0], eps=eps)
        self.check_pixel_value(col_tex, 50, 50, [0.0, 0.0, 0.0, 0.0], eps=eps)

        # These should be identical
        self.check_pixel_value(col_tex, 291, 150, [0.977, 0.977, 0.977, 1.0], eps=0.075)
        self.check_pixel_value(col_tex, 200, 51, [1.0, 0.5, 1.0, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 200, 65, [1.0, 1.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 200, 79, [0.0, 1.0, 1.0, 1.0], eps=eps)
        self.check_pixel_value(col_tex, 200, 93, [0.0, 1.0, 0.0, 1.0], eps=eps)

        tex.resourceId = depth_tex
        tex.overlay = rd.DebugOverlay.ClearBeforeDraw
        out.SetTextureDisplay(tex)
        out.Display()

        eps = 0.001

        # Without the pass, depth/stencil results are different
        self.check_pixel_value(depth_tex, 160, 135, [0.5, 85.0/255.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(depth_tex, 160, 165, [0.5, 85.0/255.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(depth_tex, 250, 150, [0.5, 85.0/255.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(depth_tex, 250, 250, [1.0, 0.0/255.0, 0.0, 1.0], eps=eps)
        self.check_pixel_value(depth_tex, 50, 50, [1.0, 0.0/255.0, 0.0, 1.0], eps=eps)

        rdtest.log.success("Clear before draw colour and depth values as expected")

        rdtest.log.success("All overlays as expected for main draw")

        # Now test overlays on a render-to-slice/mip case
        for mip in [2, 3]:
            sub_marker: rd.DrawcallDescription = self.find_draw("Subresources mip {}".format(mip))

            self.controller.SetFrameEvent(sub_marker.next.eventId, True)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            col_tex = pipe.GetOutputTargets()[0].resourceId
            sub = rd.Subresource(pipe.GetOutputTargets()[0].firstMip, pipe.GetOutputTargets()[0].firstSlice, 0)

            for overlay in rd.DebugOverlay:
                if overlay == rd.DebugOverlay.NoOverlay:
                    continue

                # These overlays are just displaymodes really, not actually separate overlays
                if overlay == rd.DebugOverlay.NaN or overlay == rd.DebugOverlay.Clipping:
                    continue

                if overlay == rd.DebugOverlay.ClearBeforeDraw or overlay == rd.DebugOverlay.ClearBeforePass:
                    continue

                rdtest.log.success("Checking overlay {} with mip/slice rendering".format(str(overlay)))

                tex.resourceId = col_tex
                tex.overlay = overlay
                tex.subresource = sub
                out.SetTextureDisplay(tex)

                out.Display()

                overlay_id: rd.ResourceId = out.GetDebugOverlayTexID()

                shift = 0
                if mip == 3:
                    shift = 1

                # All values in mip 0 should be 0 for all overlays
                self.check_pixel_value(overlay_id, 200 >> shift, 150 >> shift, [0.0, 0.0, 0.0, 0.0], sub=rd.Subresource(0, 0, 0))
                self.check_pixel_value(overlay_id, 197 >> shift, 147 >> shift, [0.0, 0.0, 0.0, 0.0], sub=rd.Subresource(0, 0, 0))
                self.check_pixel_value(overlay_id, 203 >> shift, 153 >> shift, [0.0, 0.0, 0.0, 0.0], sub=rd.Subresource(0, 0, 0))

                # Also for array slice 0 on this mip
                self.check_pixel_value(overlay_id, 200 >> shift, 150 >> shift, [0.0, 0.0, 0.0, 0.0], sub=rd.Subresource(mip, 0, 0))
                self.check_pixel_value(overlay_id, 197 >> shift, 147 >> shift, [0.0, 0.0, 0.0, 0.0], sub=rd.Subresource(mip, 0, 0))
                self.check_pixel_value(overlay_id, 203 >> shift, 153 >> shift, [0.0, 0.0, 0.0, 0.0], sub=rd.Subresource(mip, 0, 0))

                rdtest.log.success("Other mips are empty as expected for overlay {}".format(str(overlay)))

                if overlay == rd.DebugOverlay.Drawcall:
                    self.check_pixel_value(overlay_id, 50 >> shift, 36 >> shift, [0.8, 0.1, 0.8, 1.0], sub=sub, eps=eps)
                    self.check_pixel_value(overlay_id, 30 >> shift, 36 >> shift, [0.0, 0.0, 0.0, 0.5], sub=sub, eps=eps)
                    self.check_pixel_value(overlay_id, 70 >> shift, 34 >> shift, [0.8, 0.1, 0.8, 1.0], sub=sub, eps=eps)
                    self.check_pixel_value(overlay_id, 70 >> shift, 20 >> shift, [0.0, 0.0, 0.0, 0.5], sub=sub, eps=eps)
                elif overlay == rd.DebugOverlay.Wireframe:
                    self.check_pixel_value(overlay_id, 36 >> shift, 36 >> shift, [200.0 / 255.0, 1.0, 0.0, 1.0],
                                           sub=sub, eps=eps)
                    self.check_pixel_value(overlay_id, 36 >> shift, 50 >> shift, [200.0 / 255.0, 1.0, 0.0, 1.0],
                                           sub=sub, eps=eps)
                    self.check_pixel_value(overlay_id, 50 >> shift, 36 >> shift, [200.0 / 255.0, 1.0, 0.0, 0.0],
                                           sub=sub, eps=eps)
                elif overlay == rd.DebugOverlay.Depth or overlay == rd.DebugOverlay.Stencil:
                    self.check_pixel_value(overlay_id, 50 >> shift, 36 >> shift, [0.0, 1.0, 0.0, 1.0], sub=sub)
                    self.check_pixel_value(overlay_id, 30 >> shift, 36 >> shift, [0.0, 1.0, 0.0, 0.0], sub=sub)
                    self.check_pixel_value(overlay_id, 70 >> shift, 34 >> shift, [1.0, 0.0, 0.0, 1.0], sub=sub)
                    self.check_pixel_value(overlay_id, 70 >> shift, 20 >> shift, [0.0, 1.0, 0.0, 0.0], sub=sub)
                elif overlay == rd.DebugOverlay.BackfaceCull:
                    self.check_pixel_value(overlay_id, 50 >> shift, 36 >> shift, [0.0, 1.0, 0.0, 1.0], sub=sub)
                    self.check_pixel_value(overlay_id, 30 >> shift, 36 >> shift, [0.0, 1.0, 0.0, 0.0], sub=sub)
                    self.check_pixel_value(overlay_id, 70 >> shift, 34 >> shift, [1.0, 0.0, 0.0, 1.0], sub=sub)
                    self.check_pixel_value(overlay_id, 70 >> shift, 20 >> shift, [0.0, 1.0, 0.0, 0.0], sub=sub)
                elif overlay == rd.DebugOverlay.ViewportScissor:
                    self.check_pixel_value(overlay_id, 50 >> shift, 36 >> shift,
                                           [0.2 * 0.7, 0.2 * 0.7, 0.9 * 0.7, 0.7 * 0.7], sub=sub, eps=eps)
                    self.check_pixel_value(overlay_id, 30 >> shift, 36 >> shift,
                                           [0.2 * 0.7, 0.2 * 0.7, 0.9 * 0.7, 0.7 * 0.7], sub=sub, eps=eps)
                    self.check_pixel_value(overlay_id, 70 >> shift, 34 >> shift,
                                           [0.2 * 0.7, 0.2 * 0.7, 0.9 * 0.7, 0.7 * 0.7], sub=sub, eps=eps)
                    self.check_pixel_value(overlay_id, 70 >> shift, 20 >> shift,
                                           [0.2 * 0.7, 0.2 * 0.7, 0.9 * 0.7, 0.7 * 0.7], sub=sub, eps=eps)

                    if mip == 2:
                        self.check_pixel_value(overlay_id, 6, 6, [0.1, 0.1, 0.1, 1.0], sub=sub, eps=eps)
                        self.check_pixel_value(overlay_id, 4, 4, [0.0, 0.0, 0.0, 0.0], sub=sub)
                        self.check_pixel_value(overlay_id, 0, 0, [1.0, 1.0, 1.0, 1.0], sub=sub)
                        self.check_pixel_value(overlay_id, 20, 0, [0.0, 0.0, 0.0, 1.0], sub=sub)
                        self.check_pixel_value(overlay_id, 40, 0, [1.0, 1.0, 1.0, 1.0], sub=sub)
                        self.check_pixel_value(overlay_id, 60, 0, [0.0, 0.0, 0.0, 1.0], sub=sub)
                    else:
                        self.check_pixel_value(overlay_id, 4, 4, [0.1, 0.1, 0.1, 1.0], sub=sub, eps=eps)
                        self.check_pixel_value(overlay_id, 0, 0, [1.0, 1.0, 1.0, 1.0], sub=sub)
                        self.check_pixel_value(overlay_id, 20, 0, [0.0, 0.0, 0.0, 1.0], sub=sub)
                        self.check_pixel_value(overlay_id, 40, 0, [1.0, 1.0, 1.0, 1.0], sub=sub)
                elif overlay == rd.DebugOverlay.QuadOverdrawDraw or overlay == rd.DebugOverlay.QuadOverdrawPass:
                    self.check_pixel_value(overlay_id, 50 >> shift, 36 >> shift, [1.0, 1.0, 1.0, 1.0], sub=sub)
                    self.check_pixel_value(overlay_id, 30 >> shift, 36 >> shift, [0.0, 0.0, 0.0, 0.0], sub=sub)
                    self.check_pixel_value(overlay_id, 70 >> shift, 20 >> shift, [0.0, 0.0, 0.0, 0.0], sub=sub)
                    self.check_pixel_value(overlay_id, 50 >> shift, 45 >> shift, [2.0, 2.0, 2.0, 2.0], sub=sub)
                elif overlay == rd.DebugOverlay.TriangleSizeDraw or overlay == rd.DebugOverlay.TriangleSizePass:
                    if mip == 2:
                        self.check_pixel_value(overlay_id, 50 >> shift, 36 >> shift, [585.0, 585.0, 585.0, 1.0], sub=sub)
                    else:
                        self.check_pixel_value(overlay_id, 50 >> shift, 36 >> shift, [151.75, 151.75, 151.75, 1.0], sub=sub)
                    self.check_pixel_value(overlay_id, 30 >> shift, 36 >> shift, [0.0, 0.0, 0.0, 0.0], sub=sub)
                    self.check_pixel_value(overlay_id, 70 >> shift, 34 >> shift, [0.0, 0.0, 0.0, 0.0], sub=sub)
                    self.check_pixel_value(overlay_id, 70 >> shift, 20 >> shift, [0.0, 0.0, 0.0, 0.0], sub=sub)
                    if mip == 2:
                        self.check_pixel_value(overlay_id, 50 >> shift, 45 >> shift, [117.0, 117.0, 117.0, 1.0], sub=sub)
                    else:
                        self.check_pixel_value(overlay_id, 50 >> shift, 45 >> shift, [30.359375, 30.359375, 30.359375, 1.0], sub=sub)

                rdtest.log.success("Picked values are correct for mip {} overlay {}".format(sub.mip, str(overlay)))

        out.Shutdown()
