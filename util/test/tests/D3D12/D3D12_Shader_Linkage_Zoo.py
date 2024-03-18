import renderdoc as rd
from typing import List
import rdtest


class D3D12_Shader_Linkage_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Shader_Linkage_Zoo'

    def check_capture(self):
        if not self.controller.GetAPIProperties().shaderDebugging:
            rdtest.log.success("Shader debugging not enabled, skipping test")
            return

        failed = False

        test_marker: rd.ActionDescription = self.find_action("draw")
        while test_marker is not None:
            action = test_marker.next
            event_name = test_marker.customName
            test_marker: rd.ActionDescription = self.find_action("draw", action.eventId)

            self.controller.SetFrameEvent(action.eventId, False)
            pipe: rd.PipeState = self.controller.GetPipelineState()

            if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                rdtest.log.print("Skipping undebuggable shader at {}.".format(event_name))
                continue

            # Debug the shader
            trace: rd.ShaderDebugTrace = self.controller.DebugPixel(200, 150, rd.DebugPixelInputs())
            if trace.debugger is None:
                failed = True
                rdtest.log.error("Test {} could not be debugged.".format(event_name))
                continue

            cycles, variables = self.process_trace(trace)

            output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

            debugged = self.evaluate_source_var(output, variables)

            try:
                self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 200, 150, debugged.value.f32v[0:4])
            except rdtest.TestFailureException as ex:
                failed = True
                rdtest.log.error("Test {} did not match. {}".format(event_name, str(ex)))
                continue
            finally:
                self.controller.FreeTrace(trace)

            rdtest.log.success("Test {} matched as expected".format(event_name))

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
