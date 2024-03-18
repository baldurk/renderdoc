import copy
import rdtest
import renderdoc as rd
from typing import Tuple


class GL_Depth_Bounds(rdtest.TestCase):
    demos_test_name = 'GL_Depth_Bounds'

    def check_capture(self):
        eid = self.find_action("Test").next.eventId
        self.controller.SetFrameEvent(eid, False)

        glpipe = self.controller.GetGLPipelineState()

        self.check(glpipe.depthState.depthBounds)

        if (not rdtest.value_compare(glpipe.depthState.nearBound, 0.2) or
            not rdtest.value_compare(glpipe.depthState.farBound, 0.8)):
            raise rdtest.TestFailureException("Bounds {} - {} aren't as expected"
                                              .format(glpipe.depthState.nearBound, glpipe.depthState.farBound))

        pipe = self.controller.GetPipelineState()

        tex: rd.ResourceId = pipe.GetOutputTargets()[0].resource

        self.check_pixel_value(tex, 200, 200, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 200, 100, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 125, 100, [1.0, 0.0, 1.0, 1.0])
        self.check_pixel_value(tex, 125, 200, [1.0, 0.0, 1.0, 1.0])
        self.check_pixel_value(tex, 275, 100, [1.0, 0.0, 1.0, 1.0])
        self.check_pixel_value(tex, 275, 200, [1.0, 0.0, 1.0, 1.0])

        rdtest.log.success("Triangle is clipped as expected")
