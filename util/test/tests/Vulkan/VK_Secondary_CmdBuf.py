import rdtest
import renderdoc as rd


class VK_Secondary_CmdBuf(rdtest.TestCase):
    demos_test_name = 'VK_Secondary_CmdBuf'

    def check_capture(self):
        self.check_final_backbuffer()

        draw = self.find_draw("Primary")

        resources = self.controller.GetResources()

        self.check(draw is not None and draw.next is not None)

        self.controller.SetFrameEvent(draw.next.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check(pipe.GetVBuffers()[0].byteOffset == 0)
        rdtest.log.success("Primary draw has correct byte offset")

        pipeline: rd.ResourceId = self.controller.GetVulkanPipelineState().graphics.pipelineResourceId

        checked = False
        res: rd.ResourceDescription
        for res in resources:
            if res.resourceId == pipeline:
                self.check(res.name == "Pipeline 0")
                checked = True

        if not checked:
            raise rdtest.TestFailureException("Couldn't find resource description for pipeline {}".format(pipeline))

        rdtest.log.success("Primary draw has correct pipeline bound")

        draw = self.find_draw("Secondary")

        self.check(draw is not None and draw.next is not None)

        self.controller.SetFrameEvent(draw.next.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check(pipe.GetVBuffers()[0].byteOffset == 108)
        rdtest.log.success("Secondary draw has correct byte offset")

        pipeline: rd.ResourceId = self.controller.GetVulkanPipelineState().graphics.pipelineResourceId

        checked = False
        res: rd.ResourceDescription
        for res in resources:
            if res.resourceId == pipeline:
                self.check(res.name == "Pipeline 1")
                checked = True

        if not checked:
            raise rdtest.TestFailureException("Couldn't find resource description for pipeline {}".format(pipeline))

        rdtest.log.success("Secondary draw has correct pipeline bound")
