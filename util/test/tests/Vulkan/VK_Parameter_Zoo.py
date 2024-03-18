import renderdoc as rd
import rdtest


class VK_Parameter_Zoo(rdtest.TestCase):
    demos_test_name = 'VK_Parameter_Zoo'

    def check_capture(self):
        action = self.find_action("Color Draw")

        self.check(action is not None)

        action = action.next

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])

        # Readback reported extension support
        descriptor_update_template = self.find_action("KHR_descriptor_update_template") is not None
        push_descriptor = self.find_action("KHR_push_descriptor") is not None

        # Find the action that contains resource references
        action = self.find_action("References")
        self.check(action is not None)
        action = action.next
        self.controller.SetFrameEvent(action.eventId, False)

        vkpipe: rd.VKState = self.controller.GetVulkanPipelineState()

        res_names = {}
        for res in self.controller.GetResources():
            res: rd.ResourceDescription
            res_names[res.resourceId] = res.name

        expected_binds = [
            {
                'sampname': 'refsamp'
            },
            {
                'resname': 'refcombinedimg',
                'sampname': 'refcombinedsamp'
            },
            {
                'resname': 'refsampled'
            },
            {
                'resname': 'refstorage'
            },
            {
                'resname': 'refunitexel'
            },
            {
                'resname': 'refstoretexel'
            },
            {
                'resname': 'refunibuf'
            },
            {
                'resname': 'refstorebuf'
            },
            {
                'resname': 'refunibufdyn'
            },
            {
                'resname': 'refstorebufdyn'
            },
        ]

        setidx = 0
        for descset in vkpipe.graphics.descriptorSets:
            descset: rd.VKDescriptorSet

            if setidx == 2 and not descset.pushDescriptor:
                raise rdtest.TestFailureException("Expected set {} to be a push set", setidx)

            if setidx != 2 and descset.pushDescriptor:
                raise rdtest.TestFailureException("Expected set {} to be a non-push set", setidx)

            range: rd.DescriptorRange = rd.DescriptorRange()
            range.offset = 0
            # push descriptors don't include dynamic descriptors so don't fetch those
            range.count = len([
                e for e in expected_binds
                if ('sampname' in e or not descset.pushDescriptor or 'dyn' not in e['resname'])
            ])
            range.descriptorSize = 1

            descs = self.controller.GetDescriptors(descset.descriptorSetResourceId, [range])
            samps = self.controller.GetSamplerDescriptors(descset.descriptorSetResourceId, [range])

            for bindidx, (desc, samp) in enumerate(zip(descs, samps)):
                sampname = res_names.get(samp.object, '!' + str(setidx))
                resname = res_names.get(desc.resource, '!' + str(setidx))

                expected_samp = expected_binds[bindidx].get('sampname', '!') + str(setidx)
                expected_res = expected_binds[bindidx].get('resname', '!') + str(setidx)

                if not sampname == expected_samp:
                    raise rdtest.TestFailureException(
                        "Expected binding {} in set {} to have sampler {} but got {}".format(
                            bindidx, setidx, expected_samp, sampname))

                if not resname == expected_res:
                    raise rdtest.TestFailureException(
                        "Expected binding {} in set {} to have resource {} but got {}".format(
                            bindidx, setidx, expected_res, resname))

            rdtest.log.success("Resources in set {} were found as expected".format(setidx))

            setidx = setidx + 1

        # Since we can only have one push descriptor set we have a second action for push AND template updates
        if descriptor_update_template and push_descriptor:
            action = self.find_action("PushTemplReferences")
            self.check(action is not None)
            action = action.next
            self.controller.SetFrameEvent(action.eventId, False)

            vkpipe: rd.VKState = self.controller.GetVulkanPipelineState()

            descset: rd.VKDescriptorSet = vkpipe.graphics.descriptorSets[2]

            if not descset.pushDescriptor:
                raise rdtest.TestFailureException("Expected set 2 to be a push set")

            range: rd.DescriptorRange = rd.DescriptorRange()
            range.offset = 0
            # push descriptors don't include dynamic descriptors so don't fetch those
            range.count = len([
                e for e in expected_binds
                if ('sampname' in e or not descset.pushDescriptor or 'dyn' not in e['resname'])
            ])
            range.descriptorSize = 1

            descs = self.controller.GetDescriptors(descset.descriptorSetResourceId, [range])
            samps = self.controller.GetSamplerDescriptors(descset.descriptorSetResourceId, [range])

            for bindidx, (desc, samp) in enumerate(zip(descs, samps)):
                sampname = res_names.get(samp.object, '!' + str(setidx))
                resname = res_names.get(desc.resource, '!' + str(setidx))

                expected_samp = expected_binds[bindidx].get('sampname', '!') + str(setidx)
                expected_res = expected_binds[bindidx].get('resname', '!') + str(setidx)

                if not sampname == expected_samp:
                    raise rdtest.TestFailureException(
                        "Expected binding {} in set {} to have sampler {} but got {}".format(
                            bindidx, setidx, expected_samp, sampname))

                if not resname == expected_res:
                    raise rdtest.TestFailureException(
                        "Expected binding {} in set {} to have resource {} but got {}".format(
                            bindidx, setidx, expected_res, resname))

            rdtest.log.success("Resources in push template set were found as expected")

        rdtest.log.success("All resources were found as expected")

        action = self.find_action("Tools available")

        self.check(len(action.children) > 1)
        self.check(any([d.customName == 'RenderDoc' for d in action.children]))

        rdtest.log.success("RenderDoc tool was listed as available")

        action = self.find_action("ASM Draw")

        self.check(action is not None)

        action = action.next

        self.controller.SetFrameEvent(action.eventId, False)

        pipe = self.controller.GetPipelineState()

        ro = pipe.GetReadOnlyResources(rd.ShaderStage.Vertex)
        access = pipe.GetDescriptorAccess()

        self.check_eq(len(ro), 1)

        if not (rd.DescriptorType.Image, 0, 1) in [(a.type, a.index, a.arrayElement) for a in access]:
            raise rdtest.TestFailureException(
                f"Graphics bind 0[1] isn't the accessed descriptor {str(rd.DumpObject(access))}")

        vkpipe: rd.VKState = self.controller.GetVulkanPipelineState()
        self.check(len(vkpipe.viewportScissor.viewportScissors) == 0)

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                '_Position': [-1.0, 1.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                '_Position': [1.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                '_Position': [-1.0, -1.0, 0.0, 1.0],
            },
            3: {
                'vtx': 3,
                'idx': 3,
                '_Position': [1.0, -1.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        rdtest.log.success("ASM Draw is as expected")

        action = self.find_action("Immutable Draw")

        self.check(action is not None)

        action = action.next

        self.controller.SetFrameEvent(action.eventId, False)

        pipe = self.controller.GetPipelineState()

        ro = pipe.GetReadOnlyResources(rd.ShaderStage.Fragment)

        if len(ro) != 1:
            raise rdtest.TestFailureException("Expected only one resource to be used")

        if ro[0].sampler.filter.minify != rd.FilterMode.Linear:
            raise rdtest.TestFailureException("Expected linear sampler at binding slot 0 in immutable action")

        if self.get_resource(ro[0].descriptor.secondary).name != "validSampler":
            raise rdtest.TestFailureException("Expected validSampler to be at binding slot 0 in immutable action")

        rdtest.log.success("Immutable Draw is as expected")

        sdfile = self.controller.GetStructuredFile()

        # Check for resource leaks
        if len(sdfile.chunks) > 500:
            raise rdtest.TestFailureException("Too many chunks found: {}".format(len(sdfile.chunks)))

        action = self.find_action("before_empty")
        action = self.get_action(action.eventId + 1)
        self.check("vkQueueSubmit" in action.GetName(sdfile))
        a = action.GetName(sdfile)
        action = self.get_action(action.eventId + 1)
        self.check("vkQueueSubmit" in action.GetName(sdfile))
        self.check("No Command Buffers" in action.GetName(sdfile))
        self.check(a != action.GetName(sdfile))

        action = self.get_action(action.eventId + 1)
        if "after_empty" not in action.GetName(sdfile):
            self.check("vkQueueSubmit2" in action.GetName(sdfile))
            a = action.GetName(sdfile)
            action = self.get_action(action.eventId + 1)
            self.check("vkQueueSubmit2" in action.GetName(sdfile))
            self.check("No Command Buffers" in action.GetName(sdfile))
            self.check(a != action.GetName(sdfile))

        rdtest.log.success("Empty queue submits are as expected")

        action = self.find_action("Dynamic Array Draw")

        action = action.next

        self.controller.SetFrameEvent(action.eventId, False)

        self.check_triangle()

        rdtest.log.success("Dynamic Array Draw is as expected")
