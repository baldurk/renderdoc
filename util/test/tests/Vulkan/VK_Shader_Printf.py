import renderdoc as rd
import rdtest
import struct


class VK_Shader_Printf(rdtest.TestCase):
    demos_test_name = 'VK_Shader_Printf'

    def check_capture(self):
        action: rd.ActionDescription = self.find_action('CmdDraw')

        self.controller.SetFrameEvent(action.eventId, True)

        self.check_triangle()

        ssbo = self.get_resource_by_name('SSBO').resourceId

        buf_data = self.controller.GetBufferData(ssbo, 0, 0)

        count = struct.unpack_from("L", buf_data)[0]

        if count != 3*64:
            raise rdtest.TestFailureException(
                "With draw selected, buffer count is wrong: {} vs {}".format(count, 3*64))

        vkpipe = self.controller.GetVulkanPipelineState()

        self.check(len(vkpipe.shaderMessages) == 8, "Expected 8 messages for draw, got {}"
                   .format(len(vkpipe.shaderMessages)))

        for msg in vkpipe.shaderMessages:
            if 'Invalid' in msg.message:
                self.check(msg.message == "Unrecognised % formatter in \"Invalid printf string %y\"",
                           "Invalid message is wrong: {}".format(msg.message))
            else:
                expected = "pixel:{0},{1},{0}.50, {1}.50,{2}".format(msg.location.pixel.x, msg.location.pixel.y,
                                                                     int(msg.location.pixel.x == 201))
                self.check(msg.message == expected,
                           "Message is wrong. Got '{}' expected '{}'".format(msg.message, expected))

                self.check(msg.location.pixel.x in [200, 201, 202])
                self.check(msg.location.pixel.y in [150, 151, 152])

        action = self.find_action("CmdDispatch")

        self.controller.SetFrameEvent(action.eventId, False)

        vkpipe = self.controller.GetVulkanPipelineState()

        buf_data = self.controller.GetBufferData(ssbo, 0, 0)

        count = struct.unpack_from("L", buf_data)[0]

        if count != 3*64:
            raise rdtest.TestFailureException(
                "With dispatch selected, buffer count is wrong: {} vs {}".format(count, 3*64))

        self.check(len(vkpipe.shaderMessages) == 5, "Expected 5 messages for dispatch, got {}"
                   .format(len(vkpipe.shaderMessages)))

        for msg in vkpipe.shaderMessages:
            c = msg.location.compute
            expected = "compute:{}, {}, {}".format(c.workgroup[0] * 64 + c.thread[0],
                                                   c.workgroup[1] * 64 + c.thread[1],
                                                   c.workgroup[2] * 64 + c.thread[2])
            self.check(msg.message == expected,
                       "Message is wrong. Got '{}' expected '{}'".format(msg.message, expected))

            self.check(c.workgroup == (1, 0, 0))
            self.check(c.thread[1] == 0)
            self.check(c.thread[2] == 0)
            self.check(c.thread[0] in [36, 37, 38, 39, 40])

        rdtest.log.success("All messages are as expected")
