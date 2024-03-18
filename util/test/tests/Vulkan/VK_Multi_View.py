import renderdoc as rd
import rdtest

class VK_Multi_View(rdtest.TestCase):
    demos_test_name = 'VK_Multi_View'

    def check_capture(self):
        if not self.controller.GetAPIProperties().shaderDebugging:
            rdtest.log.success("Shader debugging not enabled, skipping test")
            return

        x = 200
        y = 150

        for test_name in ["Vertex: viewIndex", "Geometry: viewIndex", "Fragment: viewIndex", "No viewIndex"]:
            rdtest.log.print("Test {}".format(test_name))
            action: rd.ActionDescription = self.find_action(test_name).next
            self.controller.SetFrameEvent(action.eventId, True)

            pipe: rd.PipeState = self.controller.GetPipelineState()
            if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                raise rdtest.TestFailureException("Test {} shader can not be debugged".format(test_name))

            for view in range(2):
                # Debug the pixel shader
                inputs = rd.DebugPixelInputs()
                inputs.view = view
                trace: rd.ShaderDebugTrace = self.controller.DebugPixel(x, y, inputs)
                if trace.debugger is None:
                    self.controller.FreeTrace(trace)
                    raise rdtest.TestFailureException("Test {} view {} did not debug at all".format(test_name, view))

                cycles, variables = self.process_trace(trace)
                output: rd.SourceVariableMapping = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)
                debugged = self.evaluate_source_var(output, variables)
                slice = view + 1
                sub = rd.Subresource(0, slice, 0)
                self.check_pixel_value(pipe.GetOutputTargets()[0].resource, x, y, debugged.value.f32v[0:4], sub=sub)
                self.controller.FreeTrace(trace)

                inst = 0
                postvs = self.get_postvs(action, rd.MeshDataStage.VSOut, instance=inst, view=view)
                for vtx in range(action.numIndices):
                    idx = vtx
                    self.check_debug(vtx, idx, inst, view, postvs)
                rdtest.log.print(f"View {view} Slice {slice} passed")

        rdtest.log.success("All tests matched")


    def check_debug(self, vtx, idx, inst, view, postvs):
        trace: rd.ShaderDebugTrace = self.controller.DebugVertex(vtx, inst, idx, view)

        if trace.debugger is None:
            self.controller.FreeTrace(trace)

            raise rdtest.TestFailureException("Couldn't debug vertex {} in instance {} for view {}".format(vtx, inst, view))

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
                        "Output {} at vert {} (idx {}) instance {} view {} has different size ({} values) to expectation ({} values)"
                            .format(name, vtx, idx, inst, view, value.columns, len(expect)))

                debugged = value.value.f32v[0:value.columns]

                if not rdtest.value_compare(expect, debugged):
                    raise rdtest.TestFailureException(
                        "Debugged value {} at vert {} (idx {}) instance {} view {}: {} doesn't exactly match postvs output {}".format(
                            name, vtx, idx, inst, view, debugged, expect))
        rdtest.log.success('Successfully debugged vertex {} in instance {} for view {}'
                           .format(vtx, inst, view))

