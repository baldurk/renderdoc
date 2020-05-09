import renderdoc as rd
from typing import List
import rdtest


class D3D11_Shader_Linkage_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Shader_Linkage_Zoo'

    def check_capture(self):
        failed = False

        test_marker: rd.DrawcallDescription = self.find_draw("draw")
        while test_marker is not None:
            drawcall = test_marker.next
            event_name = test_marker.name
            test_marker: rd.DrawcallDescription = self.find_draw("draw", drawcall.eventId)

            self.controller.SetFrameEvent(drawcall.eventId, False)
            pipe: rd.PipeState = self.controller.GetPipelineState()

            # Debug the shader
            trace: rd.ShaderDebugTrace = self.controller.DebugPixel(200, 150, rd.ReplayController.NoPreference,
                                                                    rd.ReplayController.NoPreference)
            if trace.debugger is None:
                failed = True
                rdtest.log.error("Test {} could not be debugged.".format(event_name))
                continue

            cycles, variables = self.process_trace(trace)

            output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

            debugged = self.evaluate_source_var(output, variables)

            try:
                self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 200, 150, debugged.value.fv[0:4])
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
