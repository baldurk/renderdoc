import renderdoc as rd
import rdtest


class D3D11_Shader_Linkage_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Shader_Linkage_Zoo'

    def check_capture(self):
        failed = False

        test_marker = self.find_action("draw")
        while test_marker is not None:
            action = test_marker.nextAction
            event_name = test_marker.customName
            test_marker = self.find_action("draw", action.eventId)

            self.set_event(action.eventId, False)
            pipe = self.controller.GetPipelineState()

            # Debug the shader
            trace = self.controller.DebugPixel(200, 150, rd.DebugPixelInputs())
            if trace.debugger is None:
                failed = True
                rdtest.log.error(f"Test {event_name} could not be debugged.")
                continue

            cycles, variables = self.process_trace(trace)

            output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

            assert output is not None

            debugged = self.evaluate_source_var(output, variables)

            try:
                self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 200, 150, debugged.value.f32v[0:4])
            except rdtest.TestFailureException as ex:
                failed = True
                rdtest.log.error(f"Test {event_name} did not match. {ex!s}")
                continue
            finally:
                self.controller.FreeTrace(trace)

            rdtest.log.success(f"Test {event_name} matched as expected")

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
