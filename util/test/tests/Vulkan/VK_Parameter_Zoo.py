from typing import Dict

import renderdoc as rd
import rdtest


class VK_Parameter_Zoo(rdtest.TestCase):
    demos_test_name = 'VK_Parameter_Zoo'

    def check_capture(self):
        assert self.controller is not None
        if not self.validate_eventids(self.controller):
            raise rdtest.TestFailureException("Event IDs are not valid")

        action = self.find_action("Color Draw")

        assert action is not None

        action = action.nextAction

        self.set_event(action.eventId, False)

        pipe = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])

        # Readback reported extension support
        descriptor_update_template = self.find_action("KHR_descriptor_update_template") is not None
        push_descriptor = self.find_action("KHR_push_descriptor") is not None

        # Find the action that contains resource references
        action = self.find_action("References")
        assert action is not None
        action = action.nextAction
        self.set_event(action.eventId, False)

        vkpipe = self.controller.GetVulkanPipelineState()

        res_names: Dict[rd.ResourceId, str] = {}
        for res in self.controller.GetResources():
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
            if setidx == 2 and not descset.pushDescriptor:
                raise rdtest.TestFailureException(f"Expected set {setidx} to be a push set")

            if setidx != 2 and descset.pushDescriptor:
                raise rdtest.TestFailureException(f"Expected set {setidx} to be a non-push set")

            range = rd.DescriptorRange()
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
                        f"Expected binding {bindidx} in set {setidx} to have sampler {expected_samp} but got {sampname}")

                if not resname == expected_res:
                    raise rdtest.TestFailureException(
                        f"Expected binding {bindidx} in set {setidx} to have resource {expected_res} but got {resname}")

            rdtest.log.success(f"Resources in set {setidx} were found as expected")

            setidx = setidx + 1

        # Since we can only have one push descriptor set we have a second action for push AND template updates
        if descriptor_update_template and push_descriptor:
            action = self.find_action("PushTemplReferences")
            assert action is not None
            action = action.nextAction
            self.set_event(action.eventId, False)

            vkpipe = self.controller.GetVulkanPipelineState()

            descset = vkpipe.graphics.descriptorSets[2]

            if not descset.pushDescriptor:
                raise rdtest.TestFailureException("Expected set 2 to be a push set")

            range = rd.DescriptorRange()
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
                        f"Expected binding {bindidx} in set {setidx} to have sampler {expected_samp} but got {sampname}")

                if not resname == expected_res:
                    raise rdtest.TestFailureException(
                        f"Expected binding {bindidx} in set {setidx} to have resource {expected_res} but got {resname}")

            rdtest.log.success("Resources in push template set were found as expected")

        rdtest.log.success("All resources were found as expected")

        action = self.find_action("Tools available")

        assert len(action.children) > 1
        assert any([d.customName == 'RenderDoc' for d in action.children])

        rdtest.log.success("RenderDoc tool was listed as available")

        for variant in [1, 2]:
            action = self.find_action(f"ASM Draw {variant}")
            assert action is not None

            action = action.nextAction
            assert action is not None

            self.set_event(action.eventId, False)

            pipe = self.controller.GetPipelineState()

            ro = pipe.GetReadOnlyResources(rd.ShaderStage.Vertex)
            access = pipe.GetDescriptorAccess()

            self.check_eq(len(ro), 1)

            if not (rd.DescriptorType.Image, 0, 1) in [(a.type, a.index, a.arrayElement) for a in access]:
                raise rdtest.TestFailureException(
                    f"Graphics bind 0[1] isn't the accessed descriptor {rd.DumpObject(access)!s}")

            vkpipe = self.controller.GetVulkanPipelineState()
            assert len(vkpipe.viewportScissor.viewportScissors) == 0

            postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

            postvs_ref: rdtest.MeshReference = {
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

            rdtest.log.success(f"ASM Draw {variant} is as expected")

        inline_ubo_actions = [
            self.find_action("Inline UBO Draw"),
            self.find_action("Inline UBO + Templ Draw"),
            self.find_action("Inline UBO + Templ Dyn Draw"),
        ]
        action = self.find_action("Inline UBO Draw")

        for action in inline_ubo_actions:
            if action is None:
                continue

            rdtest.log.print(f"Checking {action.customName}")

            self.set_event(action.nextAction.eventId, False)

            self.check_triangle(fore=[1.0, 0.0, 1.0, 1.0])

            stage = rd.ShaderStage.Pixel

            pipe = self.controller.GetPipelineState()
            ubo1 = pipe.GetConstantBlock(stage, 0, 0).descriptor
            ubo2 = pipe.GetConstantBlock(stage, 1, 0).descriptor

            ubo1_vars = self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                                   pipe.GetShader(
                                                                       stage), stage,
                                                                   pipe.GetShaderEntryPoint(
                                                                       stage), 0,
                                                                   ubo1.resource, ubo1.byteOffset, ubo1.byteSize)
            ubo2_vars = self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                                   pipe.GetShader(
                                                                       stage), stage,
                                                                   pipe.GetShaderEntryPoint(
                                                                       stage), 1,
                                                                   ubo2.resource, ubo2.byteOffset, ubo2.byteSize)

            if len(ubo1_vars) != 1 or ubo1_vars[0].name != "col":
                raise rdtest.TestFailureException("Didn't find ubo1 col variable")

            if len(ubo2_vars) != 1 or ubo2_vars[0].name != "col":
                raise rdtest.TestFailureException("Didn't find ubo2 col variable")

            if ubo1_vars[0].value.f32v[0:4] != (1.0, 0.0, 0.0, 0.0):
                raise rdtest.TestFailureException(f"ubo1 col value incorrect: {ubo1_vars[0].value.f32v[0:4]}")

            if ubo2_vars[0].value.f32v[0:4] != (0.0, 0.0, 1.0, 1.0):
                raise rdtest.TestFailureException(f"ubo2 col value incorrect: {ubo2_vars[0].value.f32v[0:4]}")

            rdtest.log.success(f"{action.customName} is as expected")

        action = self.find_action("Immutable Draw")

        assert action is not None

        action = action.nextAction

        self.set_event(action.eventId, False)

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
            raise rdtest.TestFailureException(f"Too many chunks found: {len(sdfile.chunks)}")

        action = self.find_action("before_empty")
        action = self.get_action(action.eventId + 1)
        a = action.GetName(sdfile)
        # vkQueueSubmit with two submits each with zero command buffers
        assert "vkQueueSubmit(" in action.GetName(sdfile)
        assert "No Command Buffers" in action.GetName(sdfile)
        action = self.get_action(action.eventId + 1)
        assert "vkQueueSubmit(" in action.GetName(sdfile)
        assert "No Command Buffers" in action.GetName(sdfile)
        # vkQueueSubmit with zero submits 
        action = self.get_action(action.eventId + 1)
        assert "vkQueueSubmit()" in action.GetName(sdfile)
        assert "No Submit" in action.GetName(sdfile)

        action = self.get_action(action.eventId + 1)
        if "after_empty" not in action.GetName(sdfile):
            # vkQueueSubmit2 with one submit with zero command buffers
            assert "vkQueueSubmit2(" in action.GetName(sdfile)
            assert "No Command Buffers" in action.GetName(sdfile)
            assert a != action.GetName(sdfile)
            # vkQueueSubmit with zero submits 
            action = self.get_action(action.eventId + 1)
            assert "vkQueueSubmit2()" in action.GetName(sdfile)
            assert "No Submit" in action.GetName(sdfile)

        rdtest.log.success("Empty queue submits are as expected")

        action = self.find_action("Dynamic Array Draw")

        action = action.nextAction

        self.set_event(action.eventId, False)

        self.check_triangle()

        rdtest.log.success("Dynamic Array Draw is as expected")
