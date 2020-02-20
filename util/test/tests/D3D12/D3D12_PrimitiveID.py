import renderdoc as rd
from typing import List
import rdtest


class D3D12_PrimitiveID(rdtest.TestCase):
    demos_test_name = 'D3D12_PrimitiveID'
    
    def check_support(self):
        return False, 'shader debugging is not yet enabled for D3D12'

    def test_draw(self, draw: rd.DrawcallDescription, x, y, prim, expected_prim, expected_output):
        self.controller.SetFrameEvent(draw.eventId, True)
        pipe: rd.PipeState = self.controller.GetPipelineState()

        trace: rd.ShaderDebugTrace = self.controller.DebugPixel(x, y, rd.ReplayController.NoPreference, prim)

        sourceVars: List[rd.SourceVariableMapping] = list(trace.sourceVars)
        cycles, variables = self.process_trace(trace)

        # Find the SV_PrimitiveID variable
        primInput = [var for var in sourceVars if var.builtin == rd.ShaderBuiltin.PrimitiveIndex]
        if primInput == []:
            # If we didn't find it, then we should be expecting a 0
            if len(expected_prim) > 1 or expected_prim[0] is not 0:
                rdtest.log.error("Expected prim {} at {},{} did not match actual prim {}.".format(
                    str(expected_prim), x, y, prim))
                return False
        else:
            # Look up the matching register in the inputs, and see if the expected value matches
            inputs: List[rd.ShaderVariable] = list(trace.inputs)
            primValue = [var for var in inputs if var.name == primInput[0].variables[0].name][0]
            if primValue.value.uv[0] not in expected_prim:
                rdtest.log.error("Expected prim {} at {},{} did not match actual prim {}.".format(
                    str(expected_prim), x, y, primValue.value.uv[0]))
                return False

        # Compare shader debug output against an expected value instead of the RT's output,
        # since we're testing overlapping primitives in a single draw
        if expected_output is not None:
            output = [var for var in sourceVars if var.builtin == rd.ShaderBuiltin.ColorOutput and var.offset == 0][0]
            debugged = self.evalute_source_var(output, variables)
            if debugged.value.fv[0:4] != expected_output:
                rdtest.log.error("Expected value {} at {},{} did not match actual {}.".format(
                    expected_output, x, y, debugged.value.fv[0:4]))
                return False

        rdtest.log.success("Test at {},{} matched as expected".format(x, y))
        return True

    def check_capture(self):
        success = True

        # Jump to the draw
        test_marker: rd.DrawcallDescription = self.find_draw("Test")

        # Draw 1: No GS, PS without prim
        draw = test_marker.next
        success &= self.test_draw(draw, 100, 80, rd.ReplayController.NoPreference, [0], [0, 1, 0, 1])

        # Draw 2: No GS, PS with prim
        draw = draw.next
        success &= self.test_draw(draw, 300, 80, rd.ReplayController.NoPreference, [0], [0, 1, 0, 1])

        # Draw 3: GS, PS without prim
        draw = draw.next
        success &= self.test_draw(draw, 125, 250, rd.ReplayController.NoPreference, [0], [0, 1, 0, 1])

        # Draw 4: GS, PS with prim
        draw = draw.next
        success &= self.test_draw(draw, 325, 250, 2, [2], [0.5, 1, 0, 1])
        success &= self.test_draw(draw, 325, 250, 3, [3], [0.75, 1, 0, 1])
        # No expected output here, since it's nondeterministic which primitive gets selected
        success &= self.test_draw(draw, 325, 250, rd.ReplayController.NoPreference, [2, 3], None)

        if not success:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
