import renderdoc as rd
import rdtest


class VK_Graphics_Pipeline(rdtest.TestCase):
    demos_test_name = 'VK_Graphics_Pipeline'

    def check_capture(self):
        last_action = self.get_last_action()

        self.controller.SetFrameEvent(last_action.eventId, True)

        tri_col = [0.408, 0.863, 0.182, 1.0]

        self.check_triangle(out=last_action.copyDestination, fore=tri_col)

        self.check_export(self.capture_filename)

        action = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, False)

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [-0.5, 0.5, 0.0, 1.0],
                'vertOut.pos': [-0.5, 0.5, 0.0, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_Position': [0.0, -0.5, 0.0, 1.0],
                'vertOut.pos': [0.0, -0.5, 0.0, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'gl_Position': [0.5, 0.5, 0.0, 1.0],
                'vertOut.pos': [0.5, 0.5, 0.0, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [1.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        pipe = self.controller.GetPipelineState()

        # Do a minimal reflection test. This doesn't exhaustively test all reflection data, just to make sure that
        # the linked pipelines work
        vsrefl = pipe.GetShaderReflection(rd.ShaderStage.Vertex)
        fsrefl = pipe.GetShaderReflection(rd.ShaderStage.Fragment)

        self.check(len(vsrefl.inputSignature) == 3)
        self.check(vsrefl.inputSignature[0].varName == "Position")
        self.check(vsrefl.inputSignature[1].varName == "Color")
        self.check(vsrefl.inputSignature[2].varName == "UV")

        self.check(len(fsrefl.readOnlyResources) == 1)
        self.check(fsrefl.readOnlyResources[0].name == "smiley")

        vkpipe = self.controller.GetVulkanPipelineState()

        binding = vkpipe.graphics.descriptorSets[2].bindings[0]

        if binding.dynamicallyUsedCount != 1:
            raise rdtest.TestFailureException("Bind 0 doesn't have the right used count {}"
                                              .format(binding.dynamicallyUsedCount))

        if not binding.binds[13].dynamicallyUsed:
            raise rdtest.TestFailureException("Graphics bind 0[13] isn't dynamically used")

        trace = self.controller.DebugVertex(0, 0, 0, 0)

        if trace.debugger is None:
            raise rdtest.TestFailureException("No vertex debug result")

        cycles, variables = self.process_trace(trace)

        outputs = 0

        for var in trace.sourceVars:
            var: rd.SourceVariableMapping
            if var.variables[0].type == rd.DebugVariableType.Variable and var.signatureIndex >= 0:
                name = var.name

                if name not in postvs_data[0].keys():
                    raise rdtest.TestFailureException("Don't have expected output for {}".format(name))

                expect = postvs_data[0][name]
                value = self.evaluate_source_var(var, variables)

                if len(expect) != value.columns:
                    raise rdtest.TestFailureException(
                        "Vertex output {} has different size ({} values) to expectation ({} values)"
                            .format(name, action.eventId, value.columns, len(expect)))

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
                        "Debugged vertex output value {}: {} difference. {} doesn't exactly match postvs output {}".format(
                            name, action.eventId, diff_amt, debugged, expect))

                outputs = outputs + 1

        rdtest.log.success('Successfully debugged vertex in {} cycles, {}/{} outputs match'
                           .format(cycles, outputs, len(vsrefl.outputSignature)))

        self.controller.FreeTrace(trace)

        history = self.controller.PixelHistory(pipe.GetOutputTargets()[0].resourceId, 200, 150, rd.Subresource(0, 0, 0),
                                               rd.CompType.Typeless)

        # should be a clear then a draw
        self.check(len(history) == 2)

        self.check(self.find_action('', history[0].eventId).flags & rd.ActionFlags.BeginPass)

        self.check(self.find_action('', history[1].eventId).eventId == action.eventId)
        self.check(history[1].Passed())

        if not rdtest.value_compare(history[1].shaderOut.col.floatValue, tri_col, eps=1.0/256.0):
            raise rdtest.TestFailureException(
                "History for drawcall output is wrong: {}".format(history[1].shaderOut.col.floatValue))

        inputs = rd.DebugPixelInputs()
        inputs.sample = 0
        inputs.primitive = 0
        trace = self.controller.DebugPixel(200, 150, inputs)

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
        tex.resourceId = pipe.GetOutputTargets()[0].resourceId

        tex.overlay = rd.DebugOverlay.TriangleSizeDraw
        out.SetTextureDisplay(tex)

        out.Display()

        overlay_id = out.GetDebugOverlayTexID()

        self.check_pixel_value(overlay_id, 200, 150, [14992.0, 14992.0, 14992.0, 1.0])

        rdtest.log.success("Triangle size overlay gave correct output")

        out.Shutdown()

        source = vsrefl.debugInfo.files[0].contents.replace('#if 0', '#if 1')

        newShader = self.controller.BuildTargetShader(vsrefl.entryPoint,
                                                      rd.ShaderEncoding.GLSL,
                                                      bytes(source, 'UTF-8'),
                                                      rd.ShaderCompileFlags(),
                                                      rd.ShaderStage.Vertex)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited shader: {}".format(newShader[1]))

        self.controller.ReplaceResource(vsrefl.resourceId, newShader[0])

        # Refresh the replay if it didn't happen already
        self.controller.SetFrameEvent(last_action.eventId, True)

        tri_col2 = [0.906, 0.361, 0.182, 1.0]
        self.check_triangle(out=last_action.copyDestination, fore=tri_col2)

        rdtest.log.success("Edited shader had the right triangle output")

        self.controller.RemoveReplacement(vsrefl.resourceId)
        self.controller.FreeTargetResource(newShader[0])
