from typing import Callable, Tuple

import renderdoc as rd
import rdtest


class D3D12_Predication(rdtest.TestCase):
    demos_test_name = 'D3D12_Predication'

    def check_capture(self):
        a = self.find_action("Draw")
        b = self.find_action("Draw", a.eventId+1)
        c = self.find_action("Draw", b.eventId+1)
        d = self.find_action("Draw", c.eventId+1)
        e = self.find_action("Draw", d.eventId+1)

        viewport_array: Callable[[rd.Viewport], Tuple[float,float,float,float]] = lambda view: (view.x, view.y, view.width, view.height)

        self.set_event(a.eventId, False)
        pipe = self.controller.GetPipelineState()
        self.check_triangle(vp=viewport_array(pipe.GetViewport(0)))

        rdtest.log.success("Non-predicated triangle is correct")

        assert self.controller.GetD3D12PipelineState().predication.resourceId == rd.ResourceId()

        self.set_event(b.eventId, False)
        pipe = self.controller.GetPipelineState()
        self.check_triangle(vp=viewport_array(pipe.GetViewport(0)))

        assert self.controller.GetD3D12PipelineState().predication.resourceId != rd.ResourceId()
        assert self.controller.GetD3D12PipelineState().predication.offset == 0

        rdtest.log.success("Fixed data predicated triangle is correct")

        self.set_event(c.eventId, False)
        pipe = self.controller.GetPipelineState()
        self.check_triangle(vp=viewport_array(pipe.GetViewport(0)))

        assert self.controller.GetD3D12PipelineState().predication.resourceId != rd.ResourceId()
        assert self.controller.GetD3D12PipelineState().predication.offset > 0

        rdtest.log.success("Current frame query-predicated triangle is correct")

        self.set_event(d.eventId, False)
        pipe = self.controller.GetPipelineState()
        self.check_triangle(vp=viewport_array(pipe.GetViewport(0)))

        rdtest.log.success("Previous frame query-predicated triangle is correct")

        self.set_event(e.eventId, False)
        pipe = self.controller.GetPipelineState()
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 200, 150, [0.2, 0.2, 0.2, 1.0])

        assert self.controller.GetD3D12PipelineState().predication.resourceId != rd.ResourceId()
        assert self.controller.GetD3D12PipelineState().predication.offset > 0

        rdtest.log.success("Failing predicated triangle is correct")

        for eid in range(self.get_last_action().eventId):
            self.set_event(eid, False)

        if not self.controller.GetFatalErrorStatus().OK():
            raise rdtest.TestFailureException(self.controller.GetFatalErrorStatus().Message())

        rdtest.log.success("All events replay correctly")
