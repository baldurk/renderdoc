import rdtest
import renderdoc as rd


class VK_Descriptor_Indexing(rdtest.TestCase):
    demos_test_name = 'VK_Descriptor_Indexing'

    def check_capture(self):

        action = self.find_action("Dispatch")
        self.check(action is not None)
        self.controller.SetFrameEvent(action.eventId, False)

        vkpipe: rd.VKState = self.controller.GetVulkanPipelineState()

        if len(vkpipe.compute.descriptorSets) != 1:
            raise rdtest.TestFailureException("Wrong number of compute sets is bound: {}, not 1"
                                              .format(len(vkpipe.compute.descriptorSets)))

        binding = vkpipe.compute.descriptorSets[0].bindings[0]

        if binding.dynamicallyUsedCount != 1:
            raise rdtest.TestFailureException("Compute bind 0 doesn't have the right used count {}"
                                              .format(binding.dynamicallyUsedCount))

        if not binding.binds[15].dynamicallyUsed:
            raise rdtest.TestFailureException("Compute bind 0[15] isn't dynamically used")

        # these lists are only expected to be empty because the descriptors aren't written to, and so with mutable
        # consideration these aren't considered read-only with unknown contents
        pipe = self.controller.GetPipelineState()
        ro = pipe.GetReadOnlyResources(rd.ShaderStage.Compute, False)
        self.check_eq(ro, [])
        ro = pipe.GetReadOnlyResources(rd.ShaderStage.Compute, True)
        self.check_eq(ro, [])

        rw = pipe.GetReadWriteResources(rd.ShaderStage.Compute, False)
        self.check_eq(rw[0].dynamicallyUsedCount, 1)
        self.check_eq(rw[0].firstIndex, 0)
        self.check_eq(len(rw[0].resources), 128)
        self.check(rw[0].resources[15].dynamicallyUsed)
        self.check_eq(rw[0].resources[15].resourceId, binding.binds[15].resourceResourceId)

        rw = pipe.GetReadWriteResources(rd.ShaderStage.Compute, True)
        self.check_eq(rw[0].dynamicallyUsedCount, 1)
        self.check_eq(rw[0].firstIndex, 15)
        self.check_eq(len(rw[0].resources), 1)
        self.check_eq(rw[0].resources[0].resourceId, binding.binds[15].resourceResourceId)
        self.check(rw[0].resources[0].dynamicallyUsed)

        action = self.find_action("Draw")
        self.check(action is not None)
        self.controller.SetFrameEvent(action.eventId, False)

        vkpipe: rd.VKState = self.controller.GetVulkanPipelineState()

        # Check bindings:
        #   - buffer 15 in bind 0 should be used
        #   - images 19, 20, 21 in bind 1 should be used for the non-uniform index
        #     images 49 & 59 in bind 1 should be used for the first fixed index
        #     image 4 in bind 1 should be used for the global access from a function with no dynamic/patched parameters
        #   - images 381 & 386 in bind 2 should be used for the second fixed index
        #   - image 1 in bind 3 should be used
        bind_info = {
            0: { 'dynamicallyUsedCount': 1, 'used': [15] },
            1: { 'dynamicallyUsedCount': 6, 'used': [4, 19, 20, 21, 49, 59] },
            2: { 'dynamicallyUsedCount': 2, 'used': [381, 386] },
            3: { 'dynamicallyUsedCount': 1, 'used': [1] },
        }

        if len(vkpipe.graphics.descriptorSets) != 1:
            raise rdtest.TestFailureException("Wrong number of sets is bound: {}, not 1"
                                              .format(len(vkpipe.graphics.descriptorSets)))

        desc_set: rd.VKDescriptorSet = vkpipe.graphics.descriptorSets[0]

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

        pipe = self.controller.GetPipelineState()
        ro = pipe.GetReadOnlyResources(rd.ShaderStage.Pixel, False)
        self.check_eq(len(ro), 3)
        self.check_eq(ro[0].dynamicallyUsedCount, 6)
        self.check_eq(ro[0].firstIndex, 0)
        self.check_eq(len(ro[0].resources), 128)
        self.check_eq(ro[1].dynamicallyUsedCount, 2)
        self.check_eq(ro[1].firstIndex, 0)
        self.check_eq(len(ro[1].resources), 512)
        self.check(not ro[0].resources[18].dynamicallyUsed)
        self.check(ro[0].resources[19].dynamicallyUsed)
        ro = pipe.GetReadOnlyResources(rd.ShaderStage.Pixel, True)
        self.check_eq(len(ro), 3)
        self.check_eq(ro[0].dynamicallyUsedCount, 6)
        self.check_eq(ro[0].firstIndex, 4)
        self.check_eq(len(ro[0].resources), 56)
        self.check_eq(ro[1].dynamicallyUsedCount, 2)
        self.check_eq(ro[1].firstIndex, 381)
        self.check_eq(len(ro[1].resources), 6)
        self.check(not ro[0].resources[14].dynamicallyUsed)
        self.check(ro[0].resources[15].dynamicallyUsed)

        rw = pipe.GetReadWriteResources(rd.ShaderStage.Pixel, False)
        self.check_eq(rw[0].dynamicallyUsedCount, 1)
        self.check_eq(rw[0].firstIndex, 0)
        self.check_eq(len(rw[0].resources), 128)
        self.check(rw[0].resources[15].dynamicallyUsed)

        rw = pipe.GetReadWriteResources(rd.ShaderStage.Pixel, True)
        self.check_eq(rw[0].dynamicallyUsedCount, 1)
        self.check_eq(rw[0].firstIndex, 15)
        self.check_eq(len(rw[0].resources), 1)
        self.check(rw[0].resources[0].dynamicallyUsed)

        rdtest.log.success("Dynamic usage is as expected")
