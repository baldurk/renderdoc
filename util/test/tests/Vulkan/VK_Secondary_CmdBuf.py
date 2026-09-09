import rdtest
import renderdoc as rd


class VK_Secondary_CmdBuf(rdtest.TestCase):
    demos_test_name = 'VK_Secondary_CmdBuf'

    def check_capture(self):
        last_action = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        tex = self.get_texture(last_action.copyDestination)

        # Green triangle on the left, blue on the right
        self.check_triangle(out=last_action.copyDestination, fore=[0.0, 1.0, 0.0, 1.0],
                            vp=(0, 0, tex.width / 2, tex.height))
        self.check_triangle(out=last_action.copyDestination, fore=[0.0, 0.0, 1.0, 1.0],
                            vp=(tex.width / 2, 0, tex.width/2, tex.height))

        action = self.find_action("Primary")

        resources = self.controller.GetResources()

        assert action is not None and action.nextAction is not None

        self.controller.SetFrameEvent(action.nextAction.eventId, False)

        pipe = self.controller.GetPipelineState()

        assert pipe.GetVBuffers()[0].byteOffset == 0
        rdtest.log.success("Primary action has correct byte offset")

        pipeline = self.controller.GetVulkanPipelineState().graphics.pipelineResourceId

        checked = False
        for res in resources:
            if res.resourceId == pipeline:
                assert res.name == "Pipeline 0"
                checked = True

        if not checked:
            raise rdtest.TestFailureException("Couldn't find resource description for pipeline {}".format(pipeline))

        rdtest.log.success("Primary action has correct pipeline bound")

        action = self.find_action("Secondary")

        assert action is not None and action.nextAction is not None

        self.controller.SetFrameEvent(action.nextAction.eventId, False)

        pipe = self.controller.GetPipelineState()

        assert pipe.GetVBuffers()[0].byteOffset == 108
        rdtest.log.success("Secondary action has correct byte offset")

        pipeline = self.controller.GetVulkanPipelineState().graphics.pipelineResourceId

        checked = False
        for res in resources:
            if res.resourceId == pipeline:
                assert res.name == "Pipeline 1"
                checked = True

        if not checked:
            raise rdtest.TestFailureException("Couldn't find resource description for pipeline {}".format(pipeline))

        rdtest.log.success("Secondary action has correct pipeline bound")
