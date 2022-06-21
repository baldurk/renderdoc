import renderdoc as rd
import rdtest
import struct


class VK_Compute_Only(rdtest.TestCase):
    demos_test_name = 'VK_Compute_Only'

    def check_capture(self):
        tex = self.get_resource_by_name("tex").resourceId
        bufin = self.get_resource_by_name("bufin").resourceId
        bufout = self.get_resource_by_name("bufout").resourceId

        self.check_pixel_value(tex, 0, 0, [0.25, 0.5, 0.75, 1.0])

        self.controller.SetFrameEvent(self.find_action("Pre-Dispatch").eventId, True)

        uints = struct.unpack_from('=4L', self.controller.GetBufferData(bufin, 0, 0), 0)

        if not rdtest.value_compare(uints, [111, 111, 111, 111]):
            raise rdtest.TestFailureException(
                'bufin data is incorrect before dispatch: {}'.format(uints))

        uints = struct.unpack_from('=4L', self.controller.GetBufferData(bufout, 0, 0), 0)

        if not rdtest.value_compare(uints, [222, 222, 222, 222]):
            raise rdtest.TestFailureException(
                'bufout data is incorrect before dispatch: {}'.format(uints))

        self.controller.SetFrameEvent(self.find_action("Post-Dispatch").eventId, True)

        uints = struct.unpack_from('=4L', self.controller.GetBufferData(bufout, 0, 0), 0)

        if not rdtest.value_compare(uints, [777, 888, 999, 1110]):
            raise rdtest.TestFailureException(
                'bufout data is incorrect after dispatch: {}'.format(uints))
