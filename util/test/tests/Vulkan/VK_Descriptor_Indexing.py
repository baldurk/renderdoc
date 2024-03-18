import rdtest
import renderdoc as rd


class VK_Descriptor_Indexing(rdtest.TestCase):
    demos_test_name = 'VK_Descriptor_Indexing'

    def check_capture(self):

        action = self.find_action("Dispatch")
        self.check(action is not None)
        self.controller.SetFrameEvent(action.eventId, False)

        pipe = self.controller.GetPipelineState()
        vkpipe: rd.VKState = self.controller.GetVulkanPipelineState()

        if len(vkpipe.compute.descriptorSets) != 1:
            raise rdtest.TestFailureException("Wrong number of compute sets is bound: {}, not 1"
                                              .format(len(vkpipe.compute.descriptorSets)))

        rw = pipe.GetReadWriteResources(rd.ShaderStage.Compute)

        self.check_eq(len(rw), 1)
        self.check_eq(rw[0].access.index, 0)
        self.check_eq(rw[0].access.stage, rd.ShaderStage.Compute)
        self.check_eq(rw[0].access.type, rd.DescriptorType.ReadWriteBuffer)
        self.check_eq(rw[0].access.staticallyUnused, False)
        self.check_eq(rw[0].access.arrayElement, 15)
        self.check_eq(rw[0].access.descriptorStore,
                      vkpipe.compute.descriptorSets[0].descriptorSetResourceId)

        self.check_eq(
            len(pipe.GetReadOnlyResources(rd.ShaderStage.Compute)), 0)

        rw_used = pipe.GetReadWriteResources(rd.ShaderStage.Compute, True)

        # should get the same results for dynamic array indexing, the 'only used' is only for
        # statically unused or used bindings
        self.check(rw == rw_used)

        action = self.find_action("Draw")
        self.check(action is not None)
        self.controller.SetFrameEvent(action.eventId, False)

        pipe = self.controller.GetPipelineState()
        vkpipe = self.controller.GetVulkanPipelineState()

        # Check bindings:
        #   - buffer 15 in bind 0 (the SSBO) should be used
        #   - images 19, 20, 21 in bind 1 should be used for the non-uniform index
        #     images 49 & 59 in bind 1 should be used for the first fixed index
        #     image 4 in bind 1 should be used for the global access from a function with no dynamic/patched parameters
        #   - images 381 & 386 in bind 2 should be used for the second fixed index
        #   - image 1 in bind 3 should be used
        bind_info = {
            (rd.DescriptorType.ReadWriteBuffer, 0): {'loc': (0, 0), 'elems': [15]},
            (rd.DescriptorType.ImageSampler, 0): {'loc': (0, 1), 'elems': [4, 19, 20, 21, 49, 59]},
            (rd.DescriptorType.ImageSampler, 1): {'loc': (0, 2), 'elems': [381, 386]},
            (rd.DescriptorType.ImageSampler, 2): {'loc': (0, 3), 'elems': [1]},
        }

        if len(vkpipe.graphics.descriptorSets) != 1:
            raise rdtest.TestFailureException("Wrong number of sets is bound: {}, not 1"
                                              .format(len(vkpipe.graphics.descriptorSets)))

        desc_set = vkpipe.graphics.descriptorSets[0]

        rw = pipe.GetReadWriteResources(rd.ShaderStage.Fragment)
        ro = pipe.GetReadOnlyResources(rd.ShaderStage.Fragment)

        self.check_eq(len(ro), 1+6+2)
        self.check_eq(len(rw), 1)

        refl = pipe.GetShaderReflection(rd.ShaderStage.Fragment)

        for a in ro + rw:
            idx = (a.access.type, a.access.index)
            if idx not in bind_info.keys():
                raise rdtest.TestFailureException(
                    "Accessed bind {} of type {} doesn't exist in expected list".format(a.access.index, str(a.access.type)))

            if rd.IsReadOnlyDescriptor(a.access.type):
                res = refl.readOnlyResources[a.access.index]
            else:
                res = refl.readWriteResources[a.access.index]

            if a.access.arrayElement not in bind_info[idx]['elems']:
                raise rdtest.TestFailureException("Bind {} reports array element {} as used, which shouldn't be"
                                                  .format(res.name, a.access.arrayElement))

            if a.access.descriptorStore != desc_set.descriptorSetResourceId:
                raise rdtest.TestFailureException("Access is in descriptor store {} but expected set 0 {}"
                                                  .format(a.access.descriptorStore, desc_set))

            if (res.fixedBindSetOrSpace, res.fixedBindNumber) != bind_info[idx]['loc']:
                raise rdtest.TestFailureException("Bind {} expected to be {} but is {}, {}"
                                                  .format(res.name, bind_info[idx]['loc']), res.fixedBindSetOrSpace, res.fixedBindNumber)

            # On vulkan the logical bind name is set-relative bind[idx]. The fixed bind number is the bind only
            loc = self.controller.GetDescriptorLocations(
                a.access.descriptorStore, [rd.DescriptorRange(a.access)])[0]
            if loc.fixedBindNumber != bind_info[idx]['loc'][1]:
                raise rdtest.TestFailureException("Bind {} not expected for set,bind {}"
                                                  .format(loc.fixedBindNumber, bind_info[idx]['loc']))
            if loc.logicalBindName != "{}[{}]".format(bind_info[idx]['loc'][1], a.access.arrayElement):
                raise rdtest.TestFailureException("Bind {} not expected for set,bind {} array element {}"
                                                  .format(loc.logicalBindName, bind_info[idx]['loc'], a.access.arrayElement))

            bind_info[idx]['elems'].remove(a.access.arrayElement)

        rdtest.log.success("Dynamic usage is as expected")
