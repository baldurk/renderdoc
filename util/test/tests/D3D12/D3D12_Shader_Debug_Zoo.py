import renderdoc as rd
from typing import List
import rdtest


class D3D12_Shader_Debug_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Shader_Debug_Zoo'

    def check_capture(self):
        if not self.controller.GetAPIProperties().shaderDebugging:
            rdtest.log.success("Shader debugging not enabled, skipping test")
            return

        undefined_tests = [int(test) for test in self.find_action("Undefined tests: ").customName.split(" ")[2:]]

        failed = False

        shaderModels = ["sm_5_0", "sm_5_1"]
        for sm in range(len(shaderModels)):
            rdtest.log.begin_section(shaderModels[sm] + " tests")

            # Jump to the action
            test_marker: rd.ActionDescription = self.find_action(shaderModels[sm])
            action = test_marker.next
            self.controller.SetFrameEvent(action.eventId, False)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                rdtest.log.print("Skipping undebuggable shader at {}.".format(action.eventId))
                return

            # Loop over every test
            for test in range(action.numInstances):
                # Debug the shader
                trace: rd.ShaderDebugTrace = self.controller.DebugPixel(4 * test, 0, rd.DebugPixelInputs())

                cycles, variables = self.process_trace(trace)

                output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

                debugged = self.evaluate_source_var(output, variables)

                try:
                    self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 4 * test, 0, debugged.value.f32v[0:4])
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
                
            rdtest.log.end_section(shaderModels[sm] + " tests")

        rdtest.log.begin_section("MSAA tests")
        test_marker: rd.ActionDescription = self.find_action("MSAA")
        action = test_marker.next
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
                self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 4, 4, debugged.value.f32v[0:4], sub=rd.Subresource(0, 0, test))
            except rdtest.TestFailureException as ex:
                failed = True
                rdtest.log.error("Test {} did not match. {}".format(test, str(ex)))
                continue

        rdtest.log.end_section("MSAA tests")

        test_marker: rd.ActionDescription = self.find_action("VertexSample")
        action = test_marker.next
        self.controller.SetFrameEvent(action.eventId, False)
        pipe: rd.PipeState = self.controller.GetPipelineState()

        # Debug the vertex shader
        trace: rd.ShaderDebugTrace = self.controller.DebugVertex(0, 0, 0, 0)

        cycles, variables = self.process_trace(trace)

        output = self.find_output_source_var(trace, rd.ShaderBuiltin.Undefined, 1)

        debugged = self.evaluate_source_var(output, variables)

        if not rdtest.value_compare(debugged.value.f32v[0:4], [0.3, 0.5, 0.8, 1.0]):
            failed = True
            rdtest.log.error(
                "Vertex shader color output did not match expectation ({}). {}".format(str(debugged.value.f32v[0:4]),
                                                                                       str([0.3, 0.5, 0.8, 1.0])))

        rdtest.log.success("VertexSample VS was debugged correctly")

        # Debug the pixel shader
        inputs = rd.DebugPixelInputs()
        inputs.sample = 0
        trace: rd.ShaderDebugTrace = self.controller.DebugPixel(51, 51, inputs)

        cycles, variables = self.process_trace(trace)

        output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

        debugged = self.evaluate_source_var(output, variables)

        # Validate the debug output result
        try:
            self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 51, 51, debugged.value.f32v[0:4])
        except rdtest.TestFailureException as ex:
            failed = True
            rdtest.log.error("Vertex sample pixel shader output did not match. {}".format(str(ex)))

        rdtest.log.success("VertexSample PS was debugged correctly")

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        test_marker: rd.ActionDescription = self.find_action("Banned")
        action = test_marker.next
        self.controller.SetFrameEvent(action.eventId, False)
        pipe: rd.PipeState = self.controller.GetPipelineState()

        # Debug the vertex shader
        trace: rd.ShaderDebugTrace = self.controller.DebugVertex(0, 0, 0, 0)

        cycles, variables = self.process_trace(trace)

        output = self.find_output_source_var(trace, rd.ShaderBuiltin.Position, 0)

        debugged = self.evaluate_source_var(output, variables)

        if not rdtest.value_compare(debugged.value.f32v[0:4], [-0.5, -0.5, 0.0, 1.0]):
            failed = True
            rdtest.log.error(
                "Banned signature vertex shader position did not match expectation ({}). {}".format(
                    str(debugged.value.f32v[0:4]),
                    str([-0.5, -0.5, 0.0, 1.0])))

        rdtest.log.success("Banned signature VS was debugged correctly")

        # Debug the pixel shader
        inputs = rd.DebugPixelInputs()
        inputs.sample = 0
        trace: rd.ShaderDebugTrace = self.controller.DebugPixel(64, 64, inputs)

        cycles, variables = self.process_trace(trace)

        output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

        debugged = self.evaluate_source_var(output, variables)

        # Validate the debug output result
        try:
            self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 64, 64, debugged.value.f32v[0:4])
        except rdtest.TestFailureException as ex:
            failed = True
            rdtest.log.error("Vertex sample pixel shader output did not match. {}".format(str(ex)))

        rdtest.log.success("Banned signature PS was debugged correctly")

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
