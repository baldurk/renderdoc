import rdtest
import renderdoc as rd


class D3D12_Descriptor_Indexing(rdtest.TestCase):
    demos_test_name = 'D3D12_Descriptor_Indexing'

    def check_compute(self, eventId):
            action = self.find_action("Dispatch", eventId)
            self.check(action is not None)
            self.controller.SetFrameEvent(action.eventId, False)

            pipe = self.controller.GetD3D12PipelineState()

            if len(pipe.rootElements) != 2:
                raise rdtest.TestFailureException("Wrong number of root elements is bound: {}, not 2"
                                                  .format(len(pipe.rootElements)))

            root = pipe.rootElements[0]
            # second root element is the cbuffer const, we don't care

            if root.dynamicallyUsedCount != 1:
                raise rdtest.TestFailureException("Compute root range 0 has {} dynamically used, not 1"
                                                  .format(root.dynamicallyUsedCount))

            if not root.views[15].dynamicallyUsed:
                raise rdtest.TestFailureException("Compute root range 0[15] isn't dynamically used")

            for i in range(len(root.views)):
                if i == 15:
                    continue

                if root.views[i].dynamicallyUsed:
                    raise rdtest.TestFailureException("Compute root range 0[{}] i dynamically used".format(i))

    def check_capture(self):

        for sm in ["sm_5_1", "sm_6_0", "sm_6_6"]:
            base = self.find_action("Tests " + sm)
            if base == None:
                rdtest.log.print("Skipping test " + sm)
                continue
            self.check_compute(base.eventId)

            action = self.find_action("Draw", base.eventId)
            self.check(action is not None)
            self.controller.SetFrameEvent(action.eventId, False)

            pipe = self.controller.GetD3D12PipelineState()

            # Check bindings:
            #   - buffer 8 in root range 0 should be statically used (single fixed declaration) for the parameters
            #     image 12 in root range 0 should also be statically used
            #   - images 19, 20, 21 in root range 1 should be used for the non-uniform index
            #     images 49 & 59 in root range 1 should be used for a second array in the same range
            #     image 60 in root range 1 should be used for a fixed index in an array
            #     image 99 and 103 in root range 1 should be used
            bind_info = {
                0: [8, 12],
                1: [19, 20, 21, 49, 59, 60, 99, 103],
            }

            if len(pipe.rootElements) != 3:
                raise rdtest.TestFailureException("Wrong number of root signature ranges: {}, not 3"
                                                  .format(len(pipe.rootElements)))

            for rangeIdx, root in enumerate(pipe.rootElements):
                if rangeIdx == 2: # static sampler
                    continue

                if root.dynamicallyUsedCount != len(bind_info[rangeIdx]):
                    raise rdtest.TestFailureException(
                        "Root range {} doesn't have the right used count. {} is not the expected count of {}"
                        .format(rangeIdx, root.dynamicallyUsedCount, len(bind_info[rangeIdx])))

                for idx, el in enumerate(root.views):
                    expected_used = idx in bind_info[rangeIdx]
                    actually_used = el.dynamicallyUsed

                    if expected_used and not actually_used:
                        raise rdtest.TestFailureException(
                            "Range {} element {} expected to be used, but isn't.".format(rangeIdx, idx))

                    if not expected_used and actually_used:
                        raise rdtest.TestFailureException(
                            "Range {} element {} expected to be unused, but is.".format(rangeIdx, idx))

            rdtest.log.success("Dynamic usage is as expected for {}".format(sm))
            
        for sm in ["sm_6_6_heap"]:
            base = self.find_action("Tests " + sm)
            if base == None:
                rdtest.log.print("Skipping test " + sm)
                continue
            self.check_compute(base.eventId)

            action = self.find_action("Draw", base.eventId)
            self.check(action is not None)
            self.controller.SetFrameEvent(action.eventId, False)

            pipe = self.controller.GetD3D12PipelineState()

            # Check bindings:
            #   - CBV 
            #   - Samplers 
            #   - SRV resources
            #   - UAV resources 
            bind_info = {
                0: [9],
                1: [0, 1, 2, 4, 5, 6, 7],
                2: [8, 12, 19, 20, 21, 49, 59, 6, 99, 103],
                3: [10],
            }

            if len(pipe.rootElements) != 4:
                raise rdtest.TestFailureException("Wrong number of root signature ranges: {}, not 4"
                                                  .format(len(pipe.rootElements)))
            cbvRangeIdx = 0
            samplerRangeIdx = 1
            srvRangeIdx = 2
            uavRangeIdx = 3
            for rangeIdx, root in enumerate(pipe.rootElements):
                if root.dynamicallyUsedCount != len(bind_info[rangeIdx]):
                    raise rdtest.TestFailureException(
                        "{} : Root range {} doesn't have the right used count. {} is not the expected count of {}"
                        .format(sm, rangeIdx, root.dynamicallyUsedCount, len(bind_info[rangeIdx])))

                for el in root.views:
                    expected_used = ( el.tableIndex in bind_info[srvRangeIdx] ) or ( ( el.tableIndex in bind_info[uavRangeIdx] ) )
                    if not expected_used:
                        raise rdtest.TestFailureException(
                            "Descriptor {} expected to be unused, but is.".format(el.tableIndex))
                for el in root.constantBuffers:
                    expected_used = el.tableIndex in bind_info[cbvRangeIdx]
                    if not expected_used:
                        raise rdtest.TestFailureException(
                            "CBV {} expected to be unused, but is.".format(el.tableIndex))
                for el in root.samplers:
                    expected_used = el.tableIndex in bind_info[samplerRangeIdx]
                    if not expected_used:
                        raise rdtest.TestFailureException(
                            "Sampler {} expected to be unused, but is.".format(el.tableIndex))

            for elemId in bind_info[srvRangeIdx]:
                actually_used = any( srv.tableIndex == elemId for srv in pipe.rootElements[srvRangeIdx].views)
                if not actually_used:
                    raise rdtest.TestFailureException(
                        "SRV {} expected to be used, but isn't.".format(elemId))
            for elemId in bind_info[uavRangeIdx]:
                actually_used = any( uav.tableIndex == elemId for uav in pipe.rootElements[uavRangeIdx].views)
                if not actually_used:
                    raise rdtest.TestFailureException(
                        "UAV {} expected to be used, but isn't.".format(elemId))
            for elemId in bind_info[cbvRangeIdx]:
                actually_used = any( cbv.tableIndex == elemId for cbv in pipe.rootElements[cbvRangeIdx].constantBuffers)
                if not actually_used:
                    raise rdtest.TestFailureException(
                        "CBV {} expected to be used, but isn't.".format(elemId))
            for elemId in bind_info[samplerRangeIdx]:
                actually_used = any( samp.tableIndex == elemId for samp in pipe.rootElements[samplerRangeIdx].samplers)
                if not actually_used:
                    raise rdtest.TestFailureException(
                        "Sampler {} expected to be used, but isn't.".format(elemId))

            rdtest.log.success("Dynamic usage is as expected for {}".format(sm))
