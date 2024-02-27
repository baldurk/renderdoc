import renderdoc as rd
from typing import List
import rdtest


class VK_Shader_Debug_Zoo(rdtest.TestCase):
    demos_test_name = 'VK_Shader_Debug_Zoo'

    def check_capture(self):
        if not self.controller.GetAPIProperties().shaderDebugging:
            rdtest.log.success("Shader debugging not enabled, skipping test")
            return

        failed = False

        for test_name in ["GLSL1 tests", "GLSL2 tests", "ASM tests"]:
            rdtest.log.begin_section(test_name)
            action = self.find_action(test_name)
            for child in range(len(action.children)):
                section = action.children[child]
                self.controller.SetFrameEvent(section.eventId, False)
                pipe: rd.PipeState = self.controller.GetPipelineState()

                if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                    rdtest.log.print("Skipping undebuggable shader at {} in {}.".format(child, test_name))
                    return

                for test in range(section.numInstances):
                    x = 4 * test + 1
                    y = 4 * child + 1

                    # Debug the shader
                    trace: rd.ShaderDebugTrace = self.controller.DebugPixel(x, y, rd.DebugPixelInputs())

                    if trace.debugger is None:
                        failed = True
                        rdtest.log.error("Test {} in sub-section {} did not debug at all".format(test, child))
                        self.controller.FreeTrace(trace)
                        continue

                    cycles, variables = self.process_trace(trace)

                    output: rd.SourceVariableMapping = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

                    debugged = self.evaluate_source_var(output, variables)

                    try:
                        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, x, y, debugged.value.f32v[0:4])
                    except rdtest.TestFailureException as ex:
                        failed = True
                        rdtest.log.error("Test {} in sub-section {} did not match. {}".format(test, child, str(ex)))
                        continue
                    finally:
                        self.controller.FreeTrace(trace)

                    rdtest.log.success("Test {} in sub-section {} matched as expected".format(test, child))
            rdtest.log.end_section(test_name)

            test_name = "Disassembly Tests"
            rdtest.log.begin_section(test_name)

            action = self.find_action("ASM tests")
            self.controller.SetFrameEvent(action.children[0].eventId, False)
            pipe: rd.PipeState = self.controller.GetPipelineState()
            refl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Pixel)
            disasm = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), refl, "")
            # Test for some expected strings in the disassembly
            expectedStrings = []
            # OpSwitch disassembly of 32-bit and 64-bit literals
            expectedStrings.append("case 305419896:")
            expectedStrings.append("case 4063516280:")
            expectedStrings.append("case 1311768465173141112:")
            expectedStrings.append("case 17452669529668998776:")
            for exp in expectedStrings:
                if exp not in disasm:
                    failed = True
                    rdtest.log.error("Failed to find `{}` in disassembly".format(exp))

            rdtest.log.end_section(test_name)

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
