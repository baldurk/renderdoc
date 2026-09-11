import rdtest
import struct


class VK_Shader_Printf(rdtest.TestCase):
    demos_test_name = 'VK_Shader_Printf'

    def check_capture(self):
        action = self.find_action('CmdDraw')

        self.set_event(action.eventId, True)

        self.check_triangle()

        ssbo = self.get_resource_by_name('SSBO').resourceId

        buf_data = self.controller.GetBufferData(ssbo, 0, 0)

        count = struct.unpack_from("L", buf_data)[0]

        if count != 3*64:
            raise rdtest.TestFailureException(
                f"With draw selected, buffer count is wrong: {count} vs {3 * 64}")

        vkpipe = self.controller.GetVulkanPipelineState()

        num_msgs = len(vkpipe.shaderMessages)
        assert num_msgs == 8, f"Expected 8 messages for draw, got {num_msgs}"

        for msg in vkpipe.shaderMessages:
            if 'Invalid' in msg.message:
                assert msg.message == "Unrecognised % formatter in \"Invalid printf string %y\"", f"Invalid message is wrong: {msg.message}"
            else:
                x,y = msg.location.pixel.x, msg.location.pixel.y
                val = int(msg.location.pixel.x == 201)
                expected = f"pixel:{x},{y},{x}.50, {y}.50,{val}"
                assert msg.message == expected, f"Message is wrong. Got '{msg.message}' expected '{expected}'"

                assert msg.location.pixel.x in [200, 201, 202]
                assert msg.location.pixel.y in [150, 151, 152]

        action = self.find_action("CmdDispatch")

        self.set_event(action.eventId, False)

        vkpipe = self.controller.GetVulkanPipelineState()

        buf_data = self.controller.GetBufferData(ssbo, 0, 0)

        count = struct.unpack_from("L", buf_data)[0]

        if count != 3*64:
            raise rdtest.TestFailureException(
                f"With dispatch selected, buffer count is wrong: {count} vs {3 * 64}")

        num_msgs = len(vkpipe.shaderMessages)
        assert num_msgs == 5, f"Expected 5 messages for dispatch, got {num_msgs}"

        for msg in vkpipe.shaderMessages:
            c = msg.location.compute
            expected = f"compute:{c.workgroup[0] * 64 + c.thread[0]}, {c.workgroup[1] * 64 + c.thread[1]}, {c.workgroup[2] * 64 + c.thread[2]}"
            assert msg.message == expected, f"Message is wrong. Got '{msg.message}' expected '{expected}'"

            assert c.workgroup == (1, 0, 0)
            assert c.thread[1] == 0
            assert c.thread[2] == 0
            assert c.thread[0] in [36, 37, 38, 39, 40]

        rdtest.log.success("All messages are as expected")
