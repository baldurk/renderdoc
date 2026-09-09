import renderdoc as rd
import rdtest

class VK_KHR_Buffer_Address(rdtest.TestCase):
    demos_test_name = 'VK_KHR_Buffer_Address'

    def check_capture(self):
        if not self.controller.GetAPIProperties().shaderDebugging:
            rdtest.log.success("Shader debugging not enabled, skipping test")
            return

        x = 100
        y = 150

        for test_name in ["Draw 1", "Draw 2", "Draw 3", "Draw 4"]:
            rdtest.log.print(f"Test {test_name}")
            action = self.find_action(test_name)
            action = action.nextAction
            self.controller.SetFrameEvent(action.eventId, True)
            pipe = self.controller.GetPipelineState()

            if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                raise rdtest.TestFailureException(f"Test {test_name} shader can not be debugged")

            # Debug the pixel shader
            trace = self.controller.DebugPixel(x, y, rd.DebugPixelInputs())
            if trace.debugger is None:
                self.controller.FreeTrace(trace)
                raise rdtest.TestFailureException(f"Test {test_name} did not debug at all")

            cycles, variables = self.process_trace(trace)
            output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)
            debugged = self.evaluate_source_var(output, variables)
            self.check_pixel_value(pipe.GetOutputTargets()[0].resource, x, y, debugged.value.f32v[0:4])
            self.controller.FreeTrace(trace)
            x = x + 100
            if x > 300:
                x = 100
                y += 100

            inst = 0
            postvs = self.get_postvs(action, rd.MeshDataStage.VSOut, instance=inst)
            for vtx in range(action.numIndices):
                idx = vtx
                self.check_vertex_debug(vtx, idx, inst, postvs)

        rdtest.log.success("All tests matched")
