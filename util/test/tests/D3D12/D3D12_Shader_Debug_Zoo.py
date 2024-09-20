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

        shaderModels = ["sm_5_0", "sm_5_1", "sm_6_0", "sm_6_6"]
        for sm in range(len(shaderModels)):
            rdtest.log.begin_section(shaderModels[sm] + " tests")

            # Jump to the action
            test_marker: rd.ActionDescription = self.find_action(shaderModels[sm])
            if (test_marker is None):
                rdtest.log.print(f"Skipping Graphics tests for {shaderModels[sm]}")
                rdtest.log.end_section(shaderModels[sm] + " tests")
                continue
            action = test_marker.next
            self.controller.SetFrameEvent(action.eventId, False)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            if pipe.GetShaderReflection(rd.ShaderStage.Vertex).debugInfo.debuggable:
                # Debug the vertex shader
                instId = 10
                trace: rd.ShaderDebugTrace = self.controller.DebugVertex(0, instId, 0, 0)
                cycles, variables = self.process_trace(trace)
                output = self.find_output_source_var(trace, rd.ShaderBuiltin.Undefined, 3)
                debugged = self.evaluate_source_var(output, variables)
                actual = debugged.value.u32v[0]
                expected = instId
                if not rdtest.value_compare(actual, expected):
                    failed = True
                    rdtest.log.error(
                        f"Vertex shader TRIANGLE output did not match expectation {actual} != {expected}")
                if not failed:
                    rdtest.log.success("Basic VS debugging was successful")
            else:
                rdtest.log.print(f"Ignoring undebuggable Vertex shader at {action.eventId} for {shaderModels[sm]}.")

            if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                rdtest.log.print(f"Skipping undebuggable Pixel shader at {action.eventId} for {shaderModels[sm]}.")
                rdtest.log.end_section(shaderModels[sm] + " tests")
                continue

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

        rdtest.log.begin_section("VertexSample tests")
        shaderModels = ["sm_5_0", "sm_6_0", "sm_6_6"]
        for sm in range(len(shaderModels)):
            test_marker: rd.ActionDescription = self.find_action("VertexSample " + shaderModels[sm])
            if test_marker is None:
                rdtest.log.print(f"Skipping Vertex Sample tests for {shaderModels[sm]}")
                continue
            action = test_marker.next
            self.controller.SetFrameEvent(action.eventId, False)
            pipe: rd.PipeState = self.controller.GetPipelineState()

            if pipe.GetShaderReflection(rd.ShaderStage.Vertex).debugInfo.debuggable:
                # Debug the vertex shader
                trace: rd.ShaderDebugTrace = self.controller.DebugVertex(0, 0, 0, 0)
                cycles, variables = self.process_trace(trace)
                output = self.find_output_source_var(trace, rd.ShaderBuiltin.Undefined, 1)
                debugged = self.evaluate_source_var(output, variables)

                actual = debugged.value.f32v[0:4]
                expected = [0.3, 0.5, 0.8, 1.0]
                if not rdtest.value_compare(actual, expected):
                    failed = True
                    rdtest.log.error(
                        f"{shaderModels[sm]} Vertex shader color output did not match expectation {actual} != {expected}")

                if not failed:
                    rdtest.log.success(shaderModels[sm] + " VertexSample VS was debugged correctly")
            else:
                rdtest.log.print(f"Skipping undebuggable Vertex shader at {action.eventId} for {shaderModels[sm]}.")

            if pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
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
            else:
                rdtest.log.print(f"Skipping undebuggable Pixel shader at {action.eventId} for {shaderModels[sm]}.")

            if failed:
                raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.end_section("VertexSample tests")

        test_marker: rd.ActionDescription = self.find_action("Banned")
        action = test_marker.next
        self.controller.SetFrameEvent(action.eventId, False)
        pipe: rd.PipeState = self.controller.GetPipelineState()

        # Debug the vertex shader
        trace: rd.ShaderDebugTrace = self.controller.DebugVertex(0, 0, 0, 0)

        cycles, variables = self.process_trace(trace)

        output = self.find_output_source_var(trace, rd.ShaderBuiltin.Position, 0)

        debugged = self.evaluate_source_var(output, variables)

        actual = debugged.value.f32v[0:4]
        expected = [-0.5, -0.5, 0.0, 1.0]
        if not rdtest.value_compare(actual, expected):
            failed = True
            rdtest.log.error(f"Banned signature vertex shader position did not match expectation {actual} != {expected}")

        if not failed:
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

        csShaderModels = ["cs_5_0", "cs_6_0", "cs_6_6"]
        for sm in range(len(csShaderModels)):
            test = csShaderModels[sm]
            section = test + " tests"
            rdtest.log.begin_section(section)

            # Jump to the action
            test_marker: rd.ActionDescription = self.find_action(test)
            if test_marker is None:
                rdtest.log.print(f"Skipping Compute tests for {csShaderModels[sm]}")
                rdtest.log.end_section(section)
                continue
            action = test_marker.next
            self.controller.SetFrameEvent(action.eventId, False)
            pipe: rd.PipeState = self.controller.GetPipelineState()
            if not pipe.GetShaderReflection(rd.ShaderStage.Compute).debugInfo.debuggable:
                rdtest.log.print(f"Skipping undebuggable Compute shader at {action.eventId} for {csShaderModels[sm]}.")
                rdtest.log.end_section(section)
                continue

            # Debug the shader
            trace: rd.ShaderDebugTrace = self.controller.DebugThread([0,0,0], [0,0,0])
            cycles, variables = self.process_trace(trace)
            # Check for non-zero cycles
            # TODO: Check source variables have expected values (bit like output variables in Vertex and Pixel Shaders)
            self.controller.FreeTrace(trace)
            if cycles == 0:
                rdtest.log.error("Shader debug cycle count was zero")
                failed = True
                continue

            rdtest.log.success("Test {} matched as expected".format(test))
            rdtest.log.end_section(section)

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
