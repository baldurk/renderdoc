import renderdoc as rd
from typing import List
import rdtest


class D3D12_Shader_Debug_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Shader_Debug_Zoo'

    def check_capture(self):
        if not self.controller.GetAPIProperties().shaderDebugging:
            rdtest.log.success("Shader debugging not enabled, skipping test")
            return

        failed = False

        shaderModels = ["sm_5_0", "sm_5_1"]
        for sm in range(len(shaderModels)):
            rdtest.log.begin_section(shaderModels[sm] + " tests")

            # Jump to the draw
            test_marker: rd.DrawcallDescription = self.find_draw(shaderModels[sm])
            draw = test_marker.next
            self.controller.SetFrameEvent(draw.eventId, False)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                rdtest.log.print("Skipping undebuggable shader at {}.".format(draw.name))
                return

            # Loop over every test
            for test in range(draw.numInstances):
                # Debug the shader
                trace: rd.ShaderDebugTrace = self.controller.DebugPixel(4 * test, 0, rd.ReplayController.NoPreference,
                                                                        rd.ReplayController.NoPreference)

                cycles, variables = self.process_trace(trace)

                output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

                debugged = self.evaluate_source_var(output, variables)

                try:
                    self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 4 * test, 0, debugged.value.fv[0:4])
                except rdtest.TestFailureException as ex:
                    failed = True
                    rdtest.log.error("Test {} did not match. {}".format(test, str(ex)))
                    continue
                finally:
                    self.controller.FreeTrace(trace)

                rdtest.log.success("Test {} matched as expected".format(test))
                
            rdtest.log.end_section(shaderModels[sm] + " tests")

        rdtest.log.begin_section("MSAA tests")
        test_marker: rd.DrawcallDescription = self.find_draw("MSAA")
        draw = test_marker.next
        self.controller.SetFrameEvent(draw.eventId, False)
        pipe: rd.PipeState = self.controller.GetPipelineState()
        for test in range(4):
            # Debug the shader
            trace: rd.ShaderDebugTrace = self.controller.DebugPixel(4, 4, test,
                                                                    rd.ReplayController.NoPreference)

            # Validate that the correct sample index was debugged
            sampRegister = self.find_input_source_var(trace, rd.ShaderBuiltin.MSAASampleIndex)
            sampInput = [var for var in trace.inputs if var.name == sampRegister.variables[0].name][0]
            if sampInput.value.uv[0] != test:
                rdtest.log.error("Test {} did not pick the correct sample.".format(test))

            cycles, variables = self.process_trace(trace)

            output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

            debugged = self.evaluate_source_var(output, variables)

            # Validate the debug output result
            try:
                self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 4, 4, debugged.value.fv[0:4], sub=rd.Subresource(0, 0, test))
            except rdtest.TestFailureException as ex:
                failed = True
                rdtest.log.error("Test {} did not match. {}".format(test, str(ex)))
                continue

        rdtest.log.end_section("MSAA tests")

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
