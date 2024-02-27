import renderdoc as rd
from typing import List
import rdtest


class D3D12_Resource_Mapping_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Resource_Mapping_Zoo'

    def test_debug_pixel(self, x, y, test_name):
        pipe: rd.PipeState = self.controller.GetPipelineState()

        if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
            rdtest.log.print("Skipping undebuggable shader at {}.".format(test_name))
            return

        # Debug the shader
        trace: rd.ShaderDebugTrace = self.controller.DebugPixel(x, y, rd.DebugPixelInputs())

        cycles, variables = self.process_trace(trace)

        output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

        debugged = self.evaluate_source_var(output, variables)

        try:
            self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, x, y, debugged.value.f32v[0:4])
        except rdtest.TestFailureException as ex:
            rdtest.log.error("Test {} did not match. {}".format(test_name, str(ex)))
            return False
        finally:
            self.controller.FreeTrace(trace)

        rdtest.log.success("Test {} matched as expected".format(test_name))
        return True

    def check_capture(self):
        if not self.controller.GetAPIProperties().shaderDebugging:
            rdtest.log.success("Shader debugging not enabled, skipping test")
            return

        failed = False

        test_marker: rd.ActionDescription = self.find_action("sm_5_0")
        action = test_marker.next
        self.controller.SetFrameEvent(action.eventId, False)
        failed = not self.test_debug_pixel(200, 200, "sm_5_0") or failed

        test_marker: rd.ActionDescription = self.find_action("sm_5_1")
        action = test_marker.next
        self.controller.SetFrameEvent(action.eventId, False)
        failed = not self.test_debug_pixel(200, 200, "sm_5_1") or failed

        rdtest.log.begin_section("Resource array tests")
        test_marker: rd.ActionDescription = self.find_action("ResArray")
        action = test_marker.next
        self.controller.SetFrameEvent(action.eventId, False)

        for y in range(4):
            for x in range(4):
                failed = not self.test_debug_pixel(200 + x, 200 + y, "ResArray({},{})".format(x, y)) or failed

        rdtest.log.end_section("Resource array tests")

        rdtest.log.begin_section("Bindless tests")
        test_marker: rd.ActionDescription = self.find_action("Bindless")
        action = test_marker.next
        self.controller.SetFrameEvent(action.eventId, False)

        for y in range(4):
            for x in range(4):
                failed = not self.test_debug_pixel(200 + x, 200 + y, "Bindless({},{})".format(x, y)) or failed

        rdtest.log.end_section("Bindless tests")

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
