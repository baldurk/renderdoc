import rdtest
import renderdoc as rd


class D3D12_Descriptor_Indexing(rdtest.TestCase):
    demos_test_name = 'D3D12_Descriptor_Indexing'

    def check_support(self):
        cfg = rd.GetConfigSetting("D3D12_Experimental_BindlessFeedback")
        if cfg is not None and cfg.AsBool() is False:
            return False, 'Bindless feedback is not enabled'

        return super().check_support()

    def check_capture(self):

        for sm in ["sm_5_1", "sm_6_0"]:
            base = self.find_action("Tests " + sm)
            action = self.find_action("Dispatch", base.eventId)
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

            action = self.find_action("Draw", base.eventId)
            self.check(action is not None)
            self.controller.SetFrameEvent(action.eventId, False)

            pipe = self.controller.GetD3D12PipelineState()

            # Check bindings:
            #   - buffer 8 in root range 0 should be statically used (single fixed declaration) for the parameters
            #     image 15 in root range 0 should also be statically used
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
