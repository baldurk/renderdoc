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
            rdtest.log.print("Test {}".format(test_name))
            action: rd.ActionDescription = self.find_action(test_name)
            action = action.next
            self.controller.SetFrameEvent(action.eventId, True)
            pipe: rd.PipeState = self.controller.GetPipelineState()

            if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                raise rdtest.TestFailureException("Test {} shader can not be debugged".format(test_name))

            # Debug the pixel shader
            trace: rd.ShaderDebugTrace = self.controller.DebugPixel(x, y, rd.DebugPixelInputs())
            if trace.debugger is None:
                self.controller.FreeTrace(trace)
                raise rdtest.TestFailureException("Test {} did not debug at all".format(test_name))

            cycles, variables = self.process_trace(trace)
            output: rd.SourceVariableMapping = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)
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
                self.check_debug(vtx, idx, inst, postvs)

        rdtest.log.success("All tests matched")


    def check_debug(self, vtx, idx, inst, postvs):
        trace: rd.ShaderDebugTrace = self.controller.DebugVertex(vtx, inst, idx, 0)

        if trace.debugger is None:
            self.controller.FreeTrace(trace)

            raise rdtest.TestFailureException("Couldn't debug vertex {} in instance {}".format(vtx, inst))

        cycles, variables = self.process_trace(trace)

        for var in trace.sourceVars:
            var: rd.SourceVariableMapping
            if var.variables[0].type == rd.DebugVariableType.Variable and var.signatureIndex >= 0:
                name = var.name

                if name not in postvs[vtx].keys():
                    raise rdtest.TestFailureException("Don't have expected output for {}".format(name))

                expect = postvs[vtx][name]
                value = self.evaluate_source_var(var, variables)

                if len(expect) != value.columns:
                    raise rdtest.TestFailureException(
                        "Output {} at vert {} (idx {}) instance {} has different size ({} values) to expectation ({} values)"
                            .format(name, vtx, idx, inst, value.columns, len(expect)))

                debugged = value.value.f32v[0:value.columns]

                if not rdtest.value_compare(expect, debugged):
                    raise rdtest.TestFailureException(
                        "Debugged value {} at vert {} (idx {}) instance {}: {} doesn't exactly match postvs output {}".format(
                            name, vtx, idx, inst, debugged, expect))
        rdtest.log.success('Successfully debugged vertex {} in instance {}'
                           .format(vtx, inst))

