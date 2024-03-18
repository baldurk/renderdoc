import renderdoc as rd
import rdtest


class VK_Multi_Entry(rdtest.TestCase):
    demos_test_name = 'VK_Multi_Entry'

    def check_capture(self):
        last_action: rd.ActionDescription = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        self.check_triangle(out=last_action.copyDestination)

        rdtest.log.success("Triangle output looks correct")

        action = self.find_action('CmdDraw')

        self.controller.SetFrameEvent(action.eventId, True)

        pipe = self.controller.GetPipelineState()

        access = pipe.GetDescriptorAccess()

        # only expect two accesses, the texture we actually read and the push constants
        if len(access) != 2:
            raise rdtest.TestFailureException("Only expected two descriptor accesses, but saw {}".format(len(access)))

        if not (rd.DescriptorType.ImageSampler, 0, 15) in [(a.type, a.index, a.arrayElement) for a in access]:
            raise rdtest.TestFailureException(
                f"Graphics bind 0[15] isn't the accessed descriptor {str(rd.DumpObject(access))}")

        refl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Vertex)

        self.check(len(refl.readOnlyResources) == 0)

        postvs = self.get_postvs(action, rd.MeshDataStage.VSOut, first_index=0, num_indices=1, instance=0)

        trace: rd.ShaderDebugTrace = self.controller.DebugVertex(0, 0, 0, 0)

        if trace.debugger is None:
            raise rdtest.TestFailureException("No vertex debug result")

        cycles, variables = self.process_trace(trace)

        outputs = 0

        for var in trace.sourceVars:
            var: rd.SourceVariableMapping
            if var.variables[0].type == rd.DebugVariableType.Variable and var.signatureIndex >= 0:
                name = var.name

                if name not in postvs[0].keys():
                    raise rdtest.TestFailureException("Don't have expected output for {}".format(name))

                expect = postvs[0][name]
                value = self.evaluate_source_var(var, variables)

                if len(expect) != value.columns:
                    raise rdtest.TestFailureException(
                        "Vertex output {} has different size ({} values) to expectation ({} values)".format(
                            name, action.eventId, value.columns, len(expect)))

                compType = rd.VarTypeCompType(value.type)
                if compType == rd.CompType.UInt:
                    debugged = list(value.value.u32v[0:value.columns])
                elif compType == rd.CompType.SInt:
                    debugged = list(value.value.s32v[0:value.columns])
                else:
                    debugged = list(value.value.f32v[0:value.columns])

                is_eq, diff_amt = rdtest.value_compare_diff(expect, debugged, eps=5.0E-06)
                if not is_eq:
                    rdtest.log.error(
                        "Debugged vertex output value {}: {} difference. {} doesn't exactly match postvs output {}".
                        format(name, action.eventId, diff_amt, debugged, expect))

                outputs = outputs + 1

        rdtest.log.success('Successfully debugged vertex in {} cycles, {}/{} outputs match'.format(
            cycles, outputs, len(refl.outputSignature)))

        self.controller.FreeTrace(trace)

        history = self.controller.PixelHistory(pipe.GetOutputTargets()[0].resource, 200, 150, rd.Subresource(0, 0, 0),
                                               rd.CompType.Typeless)

        # should be a clear then a draw
        self.check(len(history) == 2)

        self.check(self.find_action('', history[0].eventId).flags & rd.ActionFlags.Clear)

        self.check(self.find_action('', history[1].eventId).eventId == action.eventId)
        self.check(history[1].Passed())

        if not rdtest.value_compare(history[1].shaderOut.col.floatValue, (0.0, 1.0, 0.0, 1.0)):
            raise rdtest.TestFailureException("History for drawcall output is wrong: {}".format(
                history[1].shaderOut.col.floatValue))

        inputs = rd.DebugPixelInputs()
        inputs.sample = 0
        inputs.primitive = 0
        trace = self.controller.DebugPixel(200, 150, inputs)

        refl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Pixel)

        self.check(len(refl.readOnlyResources) == 1)

        if trace.debugger is None:
            raise rdtest.TestFailureException("No pixel debug result")

        cycles, variables = self.process_trace(trace)

        output_sourcevar = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

        if output_sourcevar is None:
            raise rdtest.TestFailureException("Couldn't get colour output value")

        debugged = self.evaluate_source_var(output_sourcevar, variables)

        self.controller.FreeTrace(trace)

        debuggedValue = list(debugged.value.f32v[0:4])

        is_eq, diff_amt = rdtest.value_compare_diff(history[1].shaderOut.col.floatValue, debuggedValue, eps=5.0E-06)
        if not is_eq:
            rdtest.log.error(
                "Debugged pixel value {}: {} difference. {} doesn't exactly match history shader output {}".format(
                    debugged.name, diff_amt, debuggedValue, history[1].shaderOut.col.floatValue))

        rdtest.log.success('Successfully debugged pixel in {} cycles, result matches'.format(cycles))

        out = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        tex = rd.TextureDisplay()
        tex.resourceId = pipe.GetOutputTargets()[0].resource

        tex.overlay = rd.DebugOverlay.TriangleSizeDraw
        out.SetTextureDisplay(tex)

        out.Display()

        overlay_id = out.GetDebugOverlayTexID()

        self.check_pixel_value(overlay_id, 200, 150, [14992.0, 14992.0, 14992.0, 1.0])

        rdtest.log.success("Triangle size overlay gave correct output")

        out.Shutdown()
