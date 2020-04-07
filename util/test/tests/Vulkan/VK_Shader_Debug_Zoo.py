import renderdoc as rd
from typing import List
import rdtest


class VK_Shader_Debug_Zoo(rdtest.TestCase):
    demos_test_name = 'VK_Shader_Debug_Zoo'

    def check_capture(self):
        failed = False

        rdtest.log.begin_section("GLSL tests")
        draw = self.find_draw("GLSL tests")
        for child in range(len(draw.children)):
            section = draw.children[child]
            self.controller.SetFrameEvent(section.eventId, False)
            pipe: rd.PipeState = self.controller.GetPipelineState()

            for test in range(section.numInstances):
                x = 4 * test + 1
                y = 4 * child + 1

                # Debug the shader
                trace: rd.ShaderDebugTrace = self.controller.DebugPixel(x, y, rd.ReplayController.NoPreference,
                                                                        rd.ReplayController.NoPreference)

                if trace.debugger is None:
                    failed = True
                    rdtest.log.error("Test {} in sub-section {} did not debug at all".format(test, child))
                    self.controller.FreeTrace(trace)
                    continue

                cycles, variables = self.process_trace(trace)

                output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

                debugged = self.evaluate_source_var(output, variables)

                try:
                    self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, x, y, debugged.value.fv[0:4])
                except rdtest.TestFailureException as ex:
                    failed = True
                    rdtest.log.error("Test {} in sub-section {} did not match. {}".format(test, child, str(ex)))
                    continue
                finally:
                    self.controller.FreeTrace(trace)

                rdtest.log.success("Test {} in sub-section {} matched as expected".format(test, child))
        rdtest.log.end_section("GLSL tests")

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        rdtest.log.begin_section("ASM tests")
        draw = self.find_draw("ASM tests")
        for child in range(len(draw.children)):
            section = draw.children[child]
            self.controller.SetFrameEvent(section.eventId, False)
            pipe: rd.PipeState = self.controller.GetPipelineState()

            for test in range(section.numInstances):
                x = 4 * test + 1
                y = 4 * child + 1

                # Debug the shader
                trace: rd.ShaderDebugTrace = self.controller.DebugPixel(x, y, rd.ReplayController.NoPreference,
                                                                        rd.ReplayController.NoPreference)

                if trace.debugger is None:
                    failed = True
                    rdtest.log.error("Test {} in sub-section {} did not debug at all".format(test, child))
                    self.controller.FreeTrace(trace)
                    continue

                cycles, variables = self.process_trace(trace)

                output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

                debugged = self.evaluate_source_var(output, variables)

                try:
                    self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, x, y, debugged.value.fv[0:4])
                except rdtest.TestFailureException as ex:
                    failed = True
                    rdtest.log.error("Test {} in sub-section {} did not match. {}".format(test, child, str(ex)))
                    continue
                finally:
                    self.controller.FreeTrace(trace)

                rdtest.log.success("Test {} in sub-section {} matched as expected".format(test, child))
        rdtest.log.end_section("ASM tests")

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
