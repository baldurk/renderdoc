from typing import List

import rdtest
import renderdoc as rd


class VK_Line_Raster(rdtest.TestCase):
    demos_test_name = 'VK_Line_Raster'

    # Line segments are relative to 100x75 framebuffers. We sample in each corner (adjusted to be sure even with
    # slightly varying rasterization we should sample in the line endpoint) and in the middle
    points = [
           [ 6,  69 ],
           [ 50, 36 ],
           [ 93, 5  ],
        ]

    view = [ 100, 75 ]

    def sample(self, row: int, col: int):
        ret: List[bool] = []
        for p in self.points:
            x = self.view[0] * col + p[0]
            y = self.view[1] * row + p[1]

            picked = self.controller.PickPixel(self.tex, x, y, rd.Subresource(0, 0, 0), rd.CompType.Typeless)
            ret.append(rdtest.value_compare(picked.floatValue, [0.0, 1.0, 1.0, 1.0]))
        return ret

    def check_capture(self):
        action = self.find_action("vkCmdEndRenderPass")

        assert action is not None

        action = action.previousAction

        self.set_event(action.eventId, False)

        pipe = self.controller.GetPipelineState()

        self.tex = pipe.GetOutputTargets()[0].resource

        texdetails = self.get_texture(self.tex)

        # Top left we expect a regular line segment.
        s = self.sample(0, 0)

        # All points should be the line color
        if not all(s):
            raise rdtest.TestFailureException(f"Normal line picked values {s} doesn't match expectation")

        # Next row is unstippled. The lines should either be all present, or not present
        names = ["Rectangle", "Bresenham", "Rectangle Round"]
        for col in [0, 1, 2]:
            s = self.sample(1, col)

            n = f"Unstippled {names[col]}"

            if s[0]:
                if not all(s):
                    raise rdtest.TestFailureException(f"{n} picked values {s} doesn't match expectation")
                rdtest.log.success(f"{n} line looks as expected")
            else:
                if any(s):
                    raise rdtest.TestFailureException(f"{n} picked values {s} doesn't match expectation")
                rdtest.log.success(f"{n} line not supported")

        # Final row is stippled. The lines should be present on each end, and not present in the middle
        # (or not present at all)
        for col in [0, 1, 2]:
            s = self.sample(2, col)

            n = f"Stippled {names[col]}"

            if s[0]:
                if s != [True, False, True]:
                    raise rdtest.TestFailureException(f"{n} picked values {s} doesn't match expectation")
                rdtest.log.success(f"{n} line looks as expected")
            else:
                if s != [False, False, False]:
                    raise rdtest.TestFailureException(f"{n} picked values {s} doesn't match expectation")
                rdtest.log.success(f"{n} line not supported")

        rdtest.log.success("All lines look as expected")
