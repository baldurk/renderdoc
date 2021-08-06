import rdtest
import renderdoc as rd


class VK_Secondary_CmdBuf(rdtest.TestCase):
    demos_test_name = 'VK_Secondary_CmdBuf'

    def check_capture(self):
        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        tex = self.get_texture(last_action.copyDestination)

        # Green triangle on the left, blue on the right
        self.check_triangle(out=last_action.copyDestination, fore=[0.0, 1.0, 0.0, 1.0],
                            vp=(0, 0, tex.width / 2, tex.height))
        self.check_triangle(out=last_action.copyDestination, fore=[0.0, 0.0, 1.0, 1.0],
                            vp=(tex.width / 2, 0, tex.width/2, tex.height))

        action = self.find_action("Primary")

        resources = self.controller.GetResources()

        self.check(action is not None and action.next is not None)

        self.controller.SetFrameEvent(action.next.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check(pipe.GetVBuffers()[0].byteOffset == 0)
        rdtest.log.success("Primary action has correct byte offset")

        pipeline: rd.ResourceId = self.controller.GetVulkanPipelineState().graphics.pipelineResourceId

        checked = False
        res: rd.ResourceDescription
        for res in resources:
            if res.resourceId == pipeline:
                self.check(res.name == "Pipeline 0")
                checked = True

        if not checked:
            raise rdtest.TestFailureException("Couldn't find resource description for pipeline {}".format(pipeline))

        rdtest.log.success("Primary action has correct pipeline bound")

        action = self.find_action("Secondary")

        self.check(action is not None and action.next is not None)

        self.controller.SetFrameEvent(action.next.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check(pipe.GetVBuffers()[0].byteOffset == 108)
        rdtest.log.success("Secondary action has correct byte offset")

        pipeline: rd.ResourceId = self.controller.GetVulkanPipelineState().graphics.pipelineResourceId

        checked = False
        res: rd.ResourceDescription
        for res in resources:
            if res.resourceId == pipeline:
                self.check(res.name == "Pipeline 1")
                checked = True

        if not checked:
            raise rdtest.TestFailureException("Couldn't find resource description for pipeline {}".format(pipeline))

        rdtest.log.success("Secondary action has correct pipeline bound")
