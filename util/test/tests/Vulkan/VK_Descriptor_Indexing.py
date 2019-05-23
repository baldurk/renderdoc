import rdtest
import renderdoc as rd


class VK_Descriptor_Indexing(rdtest.TestCase):
    demos_test_name = 'VK_Descriptor_Indexing'

    def check_capture(self):

        draw = self.find_draw("Draw")

        self.check(draw is not None)

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.VKState = self.controller.GetVulkanPipelineState()

        # Check bindings:
        #   - buffer 15 in bind 0 should be used
        #   - images 19, 20, 21 in bind 1 should be used for the non-uniform index
        #     images 49 & 59 in bind 1 should be used for the first fixed index
        #   - images 381 & 386 in bind 2 should be used for the second fixed index
        bind_info = {
            0: { 'dynamicallyUsedCount': 1, 'used': [15] },
            1: { 'dynamicallyUsedCount': 5, 'used': [19, 20, 21, 49, 59] },
            2: { 'dynamicallyUsedCount': 2, 'used': [381, 386] },
        }

        if len(pipe.graphics.descriptorSets) != 1:
            raise rdtest.TestFailureException("Wrong number of sets is bound: {}, not 1".format(len(pipe.graphics.descriptorSets)))

        desc_set: rd.VKDescriptorSet = pipe.graphics.descriptorSets[0]

        binding: rd.VKDescriptorBinding
        for bind, binding in enumerate(desc_set.bindings):
            if binding.dynamicallyUsedCount != bind_info[bind]['dynamicallyUsedCount']:
                raise rdtest.TestFailureException("Bind {} doesn't have the right used count. {} is not the expected count of {}"
                                                  .format(bind, binding.dynamicallyUsedCount, bind_info[bind]['dynamicallyUsedCount']))

            el: rd.VKBindingElement
            for idx, el in enumerate(binding.binds):
                expected_used = idx in bind_info[bind]['used']
                actually_used = el.dynamicallyUsed

                if expected_used and not actually_used:
                    raise rdtest.TestFailureException("Bind {} element {} expected to be used, but isn't.".format(bind, idx))

                if not expected_used and actually_used:
                    raise rdtest.TestFailureException("Bind {} element {} expected to be unused, but is.".format(bind, idx))

        rdtest.log.success("Dynamic usage is as expected")
