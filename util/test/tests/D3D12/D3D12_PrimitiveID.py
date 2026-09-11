from __future__ import annotations
import renderdoc as rd
import rdtest


class D3D12_PrimitiveID(rdtest.TestCase):
    demos_test_name = 'D3D12_PrimitiveID'

    def test_action(
        self,
        action: rd.ActionDescription,
        x: int,
        y: int,
        prim: int,
        expected_prim: rdtest.VectorValue,
        expected_output: rdtest.VectorValue | None,
    ):
        self.set_event(action.eventId, True)
        pipe = self.controller.GetPipelineState()

        pixel_inputs = rd.DebugPixelInputs()
        pixel_inputs.primitive = prim
        trace = self.controller.DebugPixel(x, y, pixel_inputs)

        cycles, variables = self.process_trace(trace)

        # Find the SV_PrimitiveID variable
        primInput = self.find_input_source_var(trace, rd.ShaderBuiltin.PrimitiveIndex)
        if primInput is None:
            # If we didn't find it, then we should be expecting a 0
            if len(expected_prim) != 1 or expected_prim[0] != 0:
                rdtest.log.error(f"Expected prim {expected_prim!s} at {x},{y} did not match actual prim {prim}.")
                return False
        else:
            # Look up the matching register in the inputs, and see if the expected value matches
            inputs = list(trace.inputs)
            primInputName = primInput.variables[0].name
            if inputs[0].name.startswith('_IN') and primInputName.startswith('_IN.'):
                # Walk the DXIL input structure
                inputVars = inputs[0].members
                # Remove the input name prefix
                primInputName = primInputName[4:]
            else:
                inputVars = inputs

            primVars = [var for var in inputVars if var.name == primInputName]

            primValue = primVars[0]
            if primValue.value.u32v[0] not in expected_prim:
                rdtest.log.error(f"Expected prim {expected_prim!s} at {x},{y} did not match actual prim {primValue.value.u32v[0]}.")
                return False

        # Compare shader debug output against an expected value instead of the RT's output,
        # since we're testing overlapping primitives in a single action
        if expected_output is not None:
            output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)
            assert output is not None
            debugged = self.evaluate_source_var(output, variables)
            if list(debugged.value.f32v[0:4]) != expected_output:
                rdtest.log.error(f"Expected value {expected_output} at {x},{y} did not match actual {debugged.value.f32v[0:4]}.")
                return False

        self.controller.FreeTrace(trace)

        rdtest.log.success(f"Test at {x},{y} matched as expected")
        return True

    def check_capture(self):
        success = True

        markers = ["SM5.0", "SM6.0"]
        for i in range(2):
            rdtest.log.begin_section(markers[i])
            # Jump to the action
            test_marker = self.find_action(markers[i])
            if test_marker is None:
                rdtest.log.print(f"No {markers[i]} actions to test")
                return

            y = 40 + i * 150
            # Draw 1: No GS, PS without prim
            action = test_marker.nextAction
            assert action is not None
            success &= self.test_action(action, 100, y, rd.ReplayController.NoPreference, [0], [0, 1, 0, 1])

            # Draw 2: No GS, PS with prim
            action = action.nextAction
            assert action is not None
            success &= self.test_action(action, 300, y, rd.ReplayController.NoPreference, [0], [0, 1, 0, 1])

            # Draw 3: GS, PS without prim
            y = 125 + i * 150
            action = action.nextAction
            assert action is not None
            success &= self.test_action(action, 125, y, rd.ReplayController.NoPreference, [0], [0, 1, 0, 1])

            # Draw 4: GS, PS with prim
            action = action.nextAction
            assert action is not None
            success &= self.test_action(action, 325, y, 2, [2], [0.5, 1, 0, 1])
            success &= self.test_action(action, 325, y, 3, [3], [0.75, 1, 0, 1])
            # No expected output here, since it's nondeterministic which primitive gets selected
            success &= self.test_action(action, 325, y, rd.ReplayController.NoPreference, [2, 3], None)
            rdtest.log.end_section(markers[i])

        if not success:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
