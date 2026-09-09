from typing import Dict, List, Tuple

import renderdoc as rd
import rdtest

class VK_Resource_Usage(rdtest.TestCase):
    demos_test_name = 'VK_Resource_Usage'
    resourceUsages: Dict[rd.ResourceId, List[rd.EventUsage]] = {}
    eids: List[int] = []

    def add_action(self, action: rd.ActionDescription):
        self.eids.append(action.eventId)
        for c in action.children:
            self.add_action(c)
        for e in action.events:
            self.eids.append(e.eventId)

    def check_resource_usage(self, res: rd.ResourceDescription, expectedUsages: List[Tuple[int,rd.ResourceUsage]]):
        usages = self.resourceUsages[res.resourceId]
        if len(usages) != len(expectedUsages):
            for u in usages:
                rdtest.log.print(f"Resource '{res.name}' {res.resourceId} usage EID:{u.eventId} usage:{u.usage.name}")
            for u in expectedUsages:
                eid, usage = u
                rdtest.log.print(f"Resource '{res.name}' {res.resourceId} usage EID:{eid} usage:{usage.name}")
            raise rdtest.TestFailureException(f"'{res.name}' {res.resourceId} Incorrect resource usages count expected:{len(expectedUsages)} actual:{len(usages)}")
        for i, u in enumerate(usages):
            eid, usage = expectedUsages[i]
            if u.usage != usage:
                raise rdtest.TestFailureException(f"'{res.name}' {res.resourceId} EID:{u.eventId} Incorrect resource usage expected:{usage.name} actual:{u.usage.name}")
            if u.eventId != eid:
                raise rdtest.TestFailureException(f"'{res.name}' {res.resourceId} usage:{u.usage.name} Incorrect resource usage EID expected:{eid} actual:{u.eventId}")

    def check_capture(self):
        assert self.controller is not None
        # Cache the resource usage before running any replay i.e. without calling SetFrameEvent
        resources = self.controller.GetResources()
        for res in resources:
            self.resourceUsages[res.resourceId] = self.controller.GetUsage(res.resourceId)

        drawIndirectCount = self.find_action("Draw Indirect Count") is not None
        rdtest.log.print(f"Has Draw Indirect Count: {'Yes' if drawIndirectCount else 'No'}")

        nestedSecondaries = self.find_action("Nested Secondary Command Buffer") is not None
        rdtest.log.print(f"Has Nested Secondary Command Buffer: {'Yes' if nestedSecondaries else 'No'}")

        descBuffer = self.find_action("Descriptor Buffer") is not None
        rdtest.log.print(f"Has Descriptor Buffer: {'Yes' if descBuffer else 'No'}")

        meshShader = self.find_action("Mesh Shader") is not None
        rdtest.log.print(f"Has Mesh Shader: {'Yes' if meshShader else 'No'}")

        countDrawIndirectCount = 30 if drawIndirectCount else 0
        countNested = 39 if nestedSecondaries else 0
        countDescBufferCopy = 10 if descBuffer else 0
        countDescBuffer = 21 if descBuffer else 0
        countDescBuffer += countDescBufferCopy
        countMeshShader = 33 if meshShader else 0

        # Walk the actions finding all the Dispatch, Draw, DrawIndexed, DrawIndirect, DrawIndirectCount
        sdfile = self.controller.GetStructuredFile()
        actions = self.controller.GetRootActions().copy()

        drawEIDs: List[int] = []
        meshDispatchEIDs: List[int] = []
        indexedEIDs: List[int] = []
        indexedSpecialEIDs: List[int] = []
        dispatchEIDs: List[int] = []
        indirectEIDs: List[int] = []
        multiEIDs: List[int] = []
        indirectCountEIDs: List[int] = []
        submitEIDs: List[int] = []
        waitFencesEIDs: List[int] = []
        resetFencesEIDs: List[int] = []
        descSetDrawEIDs: List[int] = []
        descBufferDrawEIDs: List[int] = []

        markerGraphicsDescriptorSet = 0
        markerGraphicsSecondaryCommandBuffer = 0
        markerIndirect = 0
        markerIndirectEnd = 0
        markerNestedSecondaryCommandBuffer = 0
        markerDescriptorBuffer = 0
        markerBarrierCommandSubmitFence = 0
        markerCompute = 0
        markerComputeDescriptorSet = 0

        markerGraphicsSecondaryCommandBuffer = self.find_action("Secondary Command Buffer").eventId
        if nestedSecondaries:
            markerNestedSecondaryCommandBuffer = self.find_action("Nested Secondary Command Buffer").eventId
        if descBuffer:
            markerDescriptorBuffer = self.find_action("Descriptor Buffer").eventId
        markerBarrierCommandSubmitFence = self.find_action("Barrier Command Submit Fence").eventId

        while len(actions) > 0:
            action = actions.pop(0)
            actions += action.children
            flags = action.flags
            if flags & rd.ActionFlags.Drawcall:
                drawEIDs.append(action.eventId)
            if flags & rd.ActionFlags.Indexed:
                # Special case remove this from the indexed list
                # vkCmdDrawIndexedIndirect(1) => <3, 1>
                if action.customName.endswith("<3, 1>"):
                    indexedSpecialEIDs.append(action.eventId)
                else:
                    indexedEIDs.append(action.eventId)
            if flags & rd.ActionFlags.Dispatch:
                dispatchEIDs.append(action.eventId)
            if flags & rd.ActionFlags.Indirect:
                indirectEIDs.append(action.eventId)
                if "Count" in action.customName:
                    indirectCountEIDs.append(action.eventId)
            if flags & rd.ActionFlags.MeshDispatch:
                meshDispatchEIDs.append(action.eventId)
            if flags & rd.ActionFlags.MultiAction:
                multiEIDs.append(action.eventId)
            if flags & rd.ActionFlags.PushMarker:
                parentName = action.parent.customName if action.parent is not None else None
                markerPath = parentName + "." + action.customName if parentName is not None else action.customName
                if not markerGraphicsDescriptorSet and markerPath == "Graphics.Descriptor Set":
                    rdtest.log.print(f"{action.parent.customName} {action.parent.eventId} {action.customName} {action.eventId}")
                    markerGraphicsDescriptorSet = action.eventId
                if not markerIndirect and markerPath == "Indirect":
                    markerIndirect = action.eventId
                    markerIndirectEnd = action.children[-1].eventId
                if not markerCompute and markerPath == "Compute":
                    markerCompute = action.eventId
                if not markerComputeDescriptorSet and markerPath == "Compute.Descriptor Set":
                    markerComputeDescriptorSet = action.eventId
            for e in action.events:
                eventName = sdfile.chunks[e.chunkIndex].name
                if "vkQueueSubmit" in eventName:
                    submitEIDs.append(e.eventId)
                if "vkWaitForFences" in eventName:
                    waitFencesEIDs.append(e.eventId)
                if "vkResetFences" in eventName:
                    resetFencesEIDs.append(e.eventId)

        for eid in drawEIDs:
            if eid > markerGraphicsDescriptorSet and eid < markerGraphicsSecondaryCommandBuffer:
                descSetDrawEIDs.append(eid)
            elif eid > markerIndirect and (not nestedSecondaries or eid < markerNestedSecondaryCommandBuffer):
                descSetDrawEIDs.append(eid)
            elif descBuffer and eid > markerDescriptorBuffer:
                descBufferDrawEIDs.append(eid)

        drawEIDs.sort()
        indexedEIDs.sort()
        indexedSpecialEIDs.sort()
        dispatchEIDs.sort()
        indirectEIDs.sort()
        meshDispatchEIDs.sort()
        multiEIDs.sort()
        indirectCountEIDs.sort()
        submitEIDs.sort()
        waitFencesEIDs.sort()
        resetFencesEIDs.sort()
        descSetDrawEIDs.sort()
        descBufferDrawEIDs.sort()

        action = self.find_action("Draw")
        self.controller.SetFrameEvent(action.eventId, False)
        swapImage = self.controller.GetPipelineState().GetOutputTargets()[0].resource

        with rdtest.log.auto_section("Checking Resource Usage"):
            for res in resources:
                expectedUsage: List[Tuple[int,rd.ResourceUsage]] = []
                if res.type == rd.ResourceType.Device:
                    expectedUsage = [(0,rd.ResourceUsage.Unused)]
                elif res.type == rd.ResourceType.Queue:
                    expectedUsage = [(0,rd.ResourceUsage.Unused)]
                elif res.type == rd.ResourceType.Pool:
                    expectedUsage = [(0,rd.ResourceUsage.Unused)]
                elif res.type == rd.ResourceType.SwapchainImage:
                    # the swap chain image has usage, anything else does not
                    if res.resourceId == swapImage:
                        expectedUsage = [
                                        (6,rd.ResourceUsage.Barrier), 
                                        (6,rd.ResourceUsage.Discard), 
                                        (7,rd.ResourceUsage.Clear), 
                                        (8,rd.ResourceUsage.Barrier)] 
                        for eid in drawEIDs:
                            expectedUsage.append((eid,rd.ResourceUsage.ColorTarget))
                        for eid in meshDispatchEIDs:
                            expectedUsage.append((eid,rd.ResourceUsage.ColorTarget))
                        expectedUsage += [
                                    (235+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier)]
                    else:
                        expectedUsage = []
                elif res.type == rd.ResourceType.RenderPass:
                    expectedUsage = [(0,rd.ResourceUsage.Unused)]
                elif res.type == rd.ResourceType.Sync:
                    expectedUsage = [(0,rd.ResourceUsage.Unused)]
                elif res.type == rd.ResourceType.View:
                    expectedUsage = [(0,rd.ResourceUsage.Unused)]
                elif res.type == rd.ResourceType.Memory:
                    expectedUsage = [(0,rd.ResourceUsage.Unused)]
                elif res.type == rd.ResourceType.ShaderBinding:
                    expectedUsage = [(0,rd.ResourceUsage.Unused)]
                elif res.type == rd.ResourceType.Shader:
                    expectedUsage = [(0,rd.ResourceUsage.Unused)]
                elif res.type == rd.ResourceType.PipelineState:
                    expectedUsage = [(0,rd.ResourceUsage.Unused)]
                elif res.type == rd.ResourceType.Buffer:
                    if res.name == "Vertex Buffer":
                        for eid in drawEIDs:
                            expectedUsage.append((eid,rd.ResourceUsage.VertexBuffer))
                    if res.name == "Index Buffer":
                        for eid in indexedEIDs:
                            expectedUsage.append((eid,rd.ResourceUsage.IndexBuffer))
                    if res.name == "Compute Buffer In":
                        # All dispatches within "Compute" region
                        # All dispatches within "Nested Secondary Command Buffer" region
                        for eid in dispatchEIDs:
                            if eid > markerComputeDescriptorSet and eid < markerIndirect:
                                expectedUsage.append((eid,rd.ResourceUsage.CS_Constants))
                            elif nestedSecondaries and eid > markerNestedSecondaryCommandBuffer:
                                expectedUsage.append((eid,rd.ResourceUsage.CS_Constants))
                    if res.name == "Compute Buffer Out":
                        # All dispatches within "Compute" region
                        # All dispatches within "Nested Secondary Command Buffer" region
                        for eid in dispatchEIDs:
                            if eid > markerComputeDescriptorSet and eid < markerIndirect:
                                expectedUsage.append((eid,rd.ResourceUsage.CS_RWResource))
                            elif nestedSecondaries and eid > markerNestedSecondaryCommandBuffer:
                                expectedUsage.append((eid,rd.ResourceUsage.CS_RWResource))
                    if res.name == "Indirect Data":
                        expectedUsage += [(14,rd.ResourceUsage.Barrier),
                                        (15,rd.ResourceUsage.Clear),
                                        (16,rd.ResourceUsage.Barrier),
                                        (20,rd.ResourceUsage.CS_RWResource),
                                        (21,rd.ResourceUsage.Barrier),
                                        (97,rd.ResourceUsage.Barrier),
                                        (109,rd.ResourceUsage.Barrier),
                                        (120,rd.ResourceUsage.Indirect),
                                        (124,rd.ResourceUsage.Indirect),
                                        (142,rd.ResourceUsage.Indirect),
                                        (144,rd.ResourceUsage.Barrier),
                                        (149,rd.ResourceUsage.Barrier),
                                        (150,rd.ResourceUsage.Clear),
                                        (151,rd.ResourceUsage.Barrier),
                                        (155,rd.ResourceUsage.CS_RWResource),
                                        (157,rd.ResourceUsage.Barrier),
                                        (161,rd.ResourceUsage.Barrier),
                                        (163,rd.ResourceUsage.Barrier),
                                        (177,rd.ResourceUsage.Indirect),
                                        (208,rd.ResourceUsage.Indirect)]
                        if drawIndirectCount:
                            expectedUsage += [
                                        (247,rd.ResourceUsage.Indirect),
                                        (254,rd.ResourceUsage.Indirect)]
                        expectedUsage += [(231+countDrawIndirectCount,rd.ResourceUsage.Barrier)]
                        if nestedSecondaries:
                            expectedUsage += [
                                        (268+countDrawIndirectCount+countDescBufferCopy,rd.ResourceUsage.Barrier)]
                        if meshShader:
                            expectedUsage += [
                                        (249+countDrawIndirectCount+countNested+countDescBuffer,rd.ResourceUsage.Indirect),
                                        (252+countDrawIndirectCount+countNested+countDescBuffer,rd.ResourceUsage.Indirect),
                                        (255+countDrawIndirectCount+countNested+countDescBuffer,rd.ResourceUsage.Indirect)]
                        for eid in indirectEIDs:
                            # Dispatch + Indirect
                            if eid in dispatchEIDs:
                                expectedUsage.append((eid,rd.ResourceUsage.CS_RWResource))
                                expectedUsage.append((eid,rd.ResourceUsage.Indirect))
                        for eid in multiEIDs:
                            expectedUsage.append((eid,rd.ResourceUsage.Indirect))
                        for eid in indirectCountEIDs:
                            expectedUsage.append((eid,rd.ResourceUsage.Indirect))
                        for eid in indexedSpecialEIDs:
                            expectedUsage.append((eid,rd.ResourceUsage.IndexBuffer))
                            expectedUsage.append((eid,rd.ResourceUsage.Indirect))
                    if res.name == "Barrier Buffer":
                        expectedUsage = [(243+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier),
                                        (251+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier),
                                        (259+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier),
                                        (267+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier),
                                        (275+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier),
                                        (283+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier),
                                        (291+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier),
                                        (299+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier),
                                        (307+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier),
                                        (315+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier)]
                    if res.name == "Barrier2 Buffer":
                        expectedUsage = [(323+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier),
                                        (328+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier),
                                        (333+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier),
                                        (338+countDrawIndirectCount+countNested+countDescBuffer+countMeshShader,rd.ResourceUsage.Barrier)]
                    if res.name == "Descriptor Buffer":
                        if descBuffer:
                            expectedUsage = [(235+countDrawIndirectCount,rd.ResourceUsage.Barrier), 
                                        (236+countDrawIndirectCount,rd.ResourceUsage.CopySrc),
                                        (237+countDrawIndirectCount,rd.ResourceUsage.Barrier),
                                        (238+countDrawIndirectCount,rd.ResourceUsage.Clear),
                                        (241+countDrawIndirectCount,rd.ResourceUsage.Barrier),
                                        (242+countDrawIndirectCount,rd.ResourceUsage.CopyDst)]
                    if res.name == "Descriptor Backup Buffer":
                        if descBuffer:
                            expectedUsage = [(235+countDrawIndirectCount,rd.ResourceUsage.Barrier), 
                                        (236+countDrawIndirectCount,rd.ResourceUsage.CopyDst),
                                        (241+countDrawIndirectCount,rd.ResourceUsage.Barrier),
                                        (242+countDrawIndirectCount,rd.ResourceUsage.CopySrc)]
                elif res.type == rd.ResourceType.Texture:
                    if res.name == "Offscreen MSAA Image":
                        expectedUsage = [(11,rd.ResourceUsage.Barrier), 
                                        (11,rd.ResourceUsage.Discard), 
                                        (12,rd.ResourceUsage.Clear)]
                    if res.name == "Offscreen Image":
                        expectedUsage = [(9,rd.ResourceUsage.Barrier), 
                                        (9,rd.ResourceUsage.Discard), 
                                        (10,rd.ResourceUsage.Clear)] 
                        for eid in descSetDrawEIDs:
                            expectedUsage.append((eid,rd.ResourceUsage.PS_Resource))
                        for eid in descBufferDrawEIDs:
                            expectedUsage.append((eid,rd.ResourceUsage.PS_Resource))
                elif res.type == rd.ResourceType.CommandBuffer:
                    expectedUsage = [(0,rd.ResourceUsage.Unused)]
                elif res.type == rd.ResourceType.DescriptorStore:
                    expectedUsage = [(0,rd.ResourceUsage.Unused)]
                elif res.type == rd.ResourceType.Sampler:
                    expectedUsage = [(0,rd.ResourceUsage.Unused)]
                else:
                    raise rdtest.TestFailureException(f"'{res.name}' {res.resourceId} Unexpected resource type {res.type.name}")
                rdtest.log.print(f"Resource '{res.name}' type:{res.type.name} {res.resourceId} usages:{len(self.resourceUsages[res.resourceId])} expectedUsages:{len(expectedUsage)}")
                expectedUsage.sort(key=lambda x: x[0])
                self.check_resource_usage(res, expectedUsage)

        actions = self.controller.GetRootActions()
        for a in actions:
            self.add_action(a)

        # Select every event of the resource usage to ensure the EID is valid
        with rdtest.log.auto_section("Checking Resource Usage Events can be replayed"):
            for res in resources:
                rdtest.log.print(f"Resource '{res.name}' type:{res.type.name} {res.resourceId}")
                usages = self.resourceUsages[res.resourceId]
                for u in usages:
                    eid = u.eventId
                    if eid == 0:
                        continue
                    self.controller.SetFrameEvent(eid, True)
                    if eid not in self.eids:
                        raise rdtest.TestFailureException(f"'{res.name}' {res.resourceId} Missing EID:{eid}")
        
        with rdtest.log.auto_section("Checking Indirect Action Names"):
            if not self.check_indirect_action_name_consistency(self.controller):
                raise rdtest.TestFailureException("Indirect action parameters do not match its event parameters")
