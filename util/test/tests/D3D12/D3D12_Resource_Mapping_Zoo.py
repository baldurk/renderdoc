import renderdoc as rd
import rdtest


class D3D12_Resource_Mapping_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Resource_Mapping_Zoo'

    def test_debug_pixel(self, x: int, y: int, test_name: str):
        pipe = self.controller.GetPipelineState()

        if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
            rdtest.log.print(f"Skipping undebuggable shader at {test_name}.")
            return True

        # Debug the shader
        trace = self.controller.DebugPixel(x, y, rd.DebugPixelInputs())

        cycles, variables = self.process_trace(trace)

        output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)
        
        assert output is not None

        debugged = self.evaluate_source_var(output, variables)

        try:
            self.check_pixel_value(pipe.GetOutputTargets()[0].resource, x, y, debugged.value.f32v[0:4])
        except rdtest.TestFailureException as ex:
            rdtest.log.error(f"Test {test_name} did not match. {ex!s}")
            return False
        finally:
            self.controller.FreeTrace(trace)

        rdtest.log.success(f"Test {test_name} matched as expected")
        return True

    def check_capture(self):
        if not self.controller.GetAPIProperties().shaderDebugging:
            rdtest.log.success("Shader debugging not enabled, skipping test")
            return
        if not self.check_capture_internal():
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")

    def check_capture_internal(self) -> bool:
        failed = False

        rdtest.log.begin_section("SM5.x tests")
        test_marker = self.find_action("sm_5_0")
        action = test_marker.nextAction
        self.controller.SetFrameEvent(action.eventId, False)
        failed = not self.test_debug_pixel(200, 200, "sm_5_0") or failed

        test_marker = self.find_action("sm_5_1")
        action = test_marker.nextAction
        self.controller.SetFrameEvent(action.eventId, False)
        failed = not self.test_debug_pixel(200, 200, "sm_5_1") or failed

        rdtest.log.begin_section("Resource array tests")
        test_marker = self.find_action("ResArray")
        action = test_marker.nextAction
        self.controller.SetFrameEvent(action.eventId, False)

        for y in range(4):
            for x in range(4):
                failed = not self.test_debug_pixel(200 + x, 200 + y, f"ResArray({x},{y})") or failed

        rdtest.log.end_section("Resource array tests")

        rdtest.log.begin_section("Bindless tests")
        test_marker = self.find_action("Bindless")
        action = test_marker.nextAction
        self.controller.SetFrameEvent(action.eventId, False)

        for y in range(4):
            for x in range(4):
                failed = not self.test_debug_pixel(200 + x, 200 + y, f"Bindless({x},{y})") or failed

        rdtest.log.end_section("Bindless tests")
        rdtest.log.end_section("SM5.x tests")

        test_marker = self.find_action("SM6.0")
        if test_marker is None:
            rdtest.log.print("No SM6.0 action to test")
            return not failed
        
        rdtest.log.begin_section("SM6.0 tests")

        action = test_marker.nextAction
        self.controller.SetFrameEvent(action.eventId, False)
        failed = not self.test_debug_pixel(200, 200, "SM6.0") or failed

        test_marker = self.find_action("SM6.0 Table")
        action = test_marker.nextAction
        self.controller.SetFrameEvent(action.eventId, False)
        failed = not self.test_debug_pixel(200, 200, "SM6.0 Table") or failed

        rdtest.log.begin_section("Resource array tests")
        test_marker = self.find_action("SM6.0 ResArray")
        action = test_marker.nextAction
        self.controller.SetFrameEvent(action.eventId, False)

        for y in range(4):
            for x in range(4):
                failed = not self.test_debug_pixel(200 + x, 200 + y, f"SM6.0 ResArray({x},{y})") or failed

        rdtest.log.end_section("Resource array tests")

        rdtest.log.begin_section("Bindless tests")
        test_marker = self.find_action("SM6.0 Bindless")
        action = test_marker.nextAction
        self.controller.SetFrameEvent(action.eventId, False)

        for y in range(4):
            for x in range(4):
                failed = not self.test_debug_pixel(200 + x, 200 + y, f"SM6.0 Bindless({x},{y})") or failed

        rdtest.log.end_section("Bindless tests")
        rdtest.log.end_section("SM6.0 tests")

        test_marker = self.find_action("SM6.6")
        if test_marker is None:
            rdtest.log.print("No SM6.6 action to test")
            return not failed

        rdtest.log.begin_section("SM6.6 tests")

        action = test_marker.nextAction
        self.controller.SetFrameEvent(action.eventId, False)
        failed = not self.test_debug_pixel(200, 200, "SM6.6") or failed

        test_marker = self.find_action("SM6.6 Table")
        action = test_marker.nextAction
        self.controller.SetFrameEvent(action.eventId, False)
        failed = not self.test_debug_pixel(200, 200, "SM6.6 Table") or failed

        rdtest.log.begin_section("Resource array tests")
        test_marker = self.find_action("SM6.6 ResArray")
        action = test_marker.nextAction
        self.controller.SetFrameEvent(action.eventId, False)

        for y in range(4):
            for x in range(4):
                failed = not self.test_debug_pixel(200 + x, 200 + y, f"SM6.6 ResArray({x},{y})") or failed

        rdtest.log.end_section("Resource array tests")

        rdtest.log.begin_section("Bindless tests")
        test_marker = self.find_action("SM6.6 Bindless")
        action = test_marker.nextAction
        self.controller.SetFrameEvent(action.eventId, False)

        for y in range(4):
            for x in range(4):
                failed = not self.test_debug_pixel(200 + x, 200 + y, f"SM6.6 Bindless({x},{y})") or failed

        rdtest.log.end_section("Bindless tests")
        rdtest.log.end_section("SM6.6 tests")

        return not failed
