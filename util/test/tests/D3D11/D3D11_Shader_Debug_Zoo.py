import renderdoc as rd
import rdtest


class D3D11_Shader_Debug_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Shader_Debug_Zoo'
    slow_test = True

    def check_capture(self):
        undefined_tests = [int(test) for test in self.find_action("Undefined tests: ").customName.split(" ")[2:]]

        failed = False

        # Jump to the action
        for idx, action in enumerate([self.find_action("Main Test"), self.find_action("Optimised Test")]):
            name = action.customName

            action = action.nextAction

            self.set_event(action.eventId, False)

            pipe = self.controller.GetPipelineState()

            # Loop over every test
            rdtest.log.begin_section(name)
            for test in range(action.numInstances):
                # Debug the shader
                trace = self.controller.DebugPixel(4 * test, 4 * idx, rd.DebugPixelInputs())

                if trace.debugger is None:
                    rdtest.log.error(f"Test {test} failed to debug.")
                    self.controller.FreeTrace(trace)
                    continue

                cycles, variables = self.process_trace(trace)

                output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

                assert output is not None

                debugged = self.evaluate_source_var(output, variables)

                try:
                    self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 4 * test, 4 * idx, debugged.value.f32v[0:4])
                except rdtest.TestFailureException as ex:
                    if test in undefined_tests:
                        rdtest.log.comment(f"Undefined test {test} did not match. {ex!s}")
                    else:
                        rdtest.log.error(f"Test {test} did not match. {ex!s}")
                        failed = True
                    continue
                finally:
                    self.controller.FreeTrace(trace)

                rdtest.log.success(f"Test {test} matched as expected")
            rdtest.log.end_section(name)

        rdtest.log.begin_section("Flow tests")
        action = self.find_action("Flow Test").nextAction
        self.set_event(action.eventId, False)
        pipe = self.controller.GetPipelineState()

        # Debug the shader
        trace = self.controller.DebugPixel(0, 8, rd.DebugPixelInputs())

        cycles, variables = self.process_trace(trace)

        output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

        assert output is not None

        debugged = self.evaluate_source_var(output, variables)

        try:
            self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0, 8, debugged.value.f32v[0:4])
            self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0, 8, [9.0, 66.0, 4.0, 18.0])
        except rdtest.TestFailureException as ex:
            raise rdtest.TestFailureException(f"Flow test did not match. {ex!s}")
        finally:
            self.controller.FreeTrace(trace)

        rdtest.log.success("Flow test matched as expected")

        rdtest.log.end_section("Flow tests")

        rdtest.log.begin_section("MSAA tests")
        action = self.find_action("MSAA Test").nextAction
        self.set_event(action.eventId, False)
        pipe = self.controller.GetPipelineState()
        for (x,y) in [(4, 4), (4, 5), (3, 4), (3, 5)]:
            for test in range(4):
                # Debug the shader
                inputs = rd.DebugPixelInputs()
                inputs.sample = test
                trace = self.controller.DebugPixel(x, y, inputs)

                # Validate that the correct sample index was debugged
                sampRegister = self.find_input_source_var(trace, rd.ShaderBuiltin.MSAASampleIndex)
                sampInput = [var for var in trace.inputs if var.name == sampRegister.variables[0].name][0]
                if sampInput.value.u32v[0] != test:
                    rdtest.log.error(f"Test {test} did not pick the correct sample.")

                cycles, variables = self.process_trace(trace)

                output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

                assert output is not None

                debugged = self.evaluate_source_var(output, variables)

                # Validate the debug output result
                try:
                    self.check_pixel_value(pipe.GetOutputTargets()[0].resource, x, y, debugged.value.f32v[0:4], sub=rd.Subresource(0, 0, test))
                except rdtest.TestFailureException as ex:
                    failed = True
                    rdtest.log.error(f"Test {test} did not match. {ex!s}")
                    continue

        rdtest.log.end_section("MSAA tests")

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
