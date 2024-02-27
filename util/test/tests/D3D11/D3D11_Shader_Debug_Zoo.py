import renderdoc as rd
from typing import List
import rdtest


class D3D11_Shader_Debug_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Shader_Debug_Zoo'

    def check_capture(self):
        # Jump to the action
        action = self.find_action("Main Test").next

        undefined_tests = [int(test) for test in self.find_action("Undefined tests: ").customName.split(" ")[2:]]

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        failed = False

        # Loop over every test
        rdtest.log.begin_section("General tests")
        for test in range(action.numInstances):
            # Debug the shader
            trace: rd.ShaderDebugTrace = self.controller.DebugPixel(4 * test, 0, rd.DebugPixelInputs())

            cycles, variables = self.process_trace(trace)

            output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

            debugged = self.evaluate_source_var(output, variables)

            try:
                self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 4 * test, 0, debugged.value.f32v[0:4])
            except rdtest.TestFailureException as ex:
                if test in undefined_tests:
                    rdtest.log.comment("Undefined test {} did not match. {}".format(test, str(ex)))
                else:
                    rdtest.log.error("Test {} did not match. {}".format(test, str(ex)))
                    failed = True
                continue
            finally:
                self.controller.FreeTrace(trace)

            rdtest.log.success("Test {} matched as expected".format(test))
        rdtest.log.end_section("General tests")

        rdtest.log.begin_section("Flow tests")
        action = self.find_action("Flow Test").next
        self.controller.SetFrameEvent(action.eventId, False)
        pipe: rd.PipeState = self.controller.GetPipelineState()

        # Debug the shader
        trace: rd.ShaderDebugTrace = self.controller.DebugPixel(0, 4, rd.DebugPixelInputs())

        cycles, variables = self.process_trace(trace)

        output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

        debugged = self.evaluate_source_var(output, variables)

        try:
            self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0, 4, debugged.value.f32v[0:4])
            self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0, 4, [9.0, 66.0, 4.0, 18.0])
        except rdtest.TestFailureException as ex:
            raise rdtest.TestFailureException("Flow test did not match. {}".format(str(ex)))
        finally:
            self.controller.FreeTrace(trace)

        rdtest.log.success("Flow test matched as expected")

        rdtest.log.end_section("Flow tests")

        rdtest.log.begin_section("MSAA tests")
        action = self.find_action("MSAA Test").next
        self.controller.SetFrameEvent(action.eventId, False)
        pipe: rd.PipeState = self.controller.GetPipelineState()
        for test in range(4):
            # Debug the shader
            inputs = rd.DebugPixelInputs()
            inputs.sample = test
            trace: rd.ShaderDebugTrace = self.controller.DebugPixel(4, 4, inputs)

            # Validate that the correct sample index was debugged
            sampRegister = self.find_input_source_var(trace, rd.ShaderBuiltin.MSAASampleIndex)
            sampInput = [var for var in trace.inputs if var.name == sampRegister.variables[0].name][0]
            if sampInput.value.u32v[0] != test:
                rdtest.log.error("Test {} did not pick the correct sample.".format(test))

            cycles, variables = self.process_trace(trace)

            output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

            debugged = self.evaluate_source_var(output, variables)

            # Validate the debug output result
            try:
                self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 4, 4, debugged.value.f32v[0:4], sub=rd.Subresource(0, 0, test))
            except rdtest.TestFailureException as ex:
                failed = True
                rdtest.log.error("Test {} did not match. {}".format(test, str(ex)))
                continue

        rdtest.log.end_section("MSAA tests")

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
