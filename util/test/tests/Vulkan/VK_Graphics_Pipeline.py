import renderdoc as rd
import rdtest


class VK_Graphics_Pipeline(rdtest.TestCase):
    demos_test_name = 'VK_Graphics_Pipeline'

    def check_capture(self):
        last_action = self.get_last_action()

        self.set_event(last_action.eventId, True)

        tri_col = [0.408, 0.863, 0.182, 1.0]

        self.check_triangle(out=last_action.copyDestination, fore=tri_col)

        self.check_export(self.capture_filename)

        action = self.find_action("Draw")

        assert action is not None

        self.set_event(action.eventId, False)

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref: rdtest.MeshReference = {
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

        assert len(vsrefl.inputSignature) == 3
        assert vsrefl.inputSignature[0].varName == "Position"
        assert vsrefl.inputSignature[1].varName == "Color"
        assert vsrefl.inputSignature[2].varName == "UV"

        assert len(fsrefl.readOnlyResources) == 1
        assert fsrefl.readOnlyResources[0].name == "smiley"

        access = pipe.GetDescriptorAccess()

        # only expect 4 accesses, the texture we actually read, spec constant, push constants, and VS UBO
        if len(access) != 4:
            raise rdtest.TestFailureException(f"Only expected 4 descriptor accesses, but saw {len(access)}")

        if not (rd.DescriptorType.ImageSampler, 0, 13) in [(a.type, a.index, a.arrayElement) for a in access]:
            raise rdtest.TestFailureException(
                f"Graphics bind 0[15] isn't the accessed descriptor {rd.DumpObject(access)!s}")

        self.check_vertex_debug(0, 0, 0, postvs_data)

        history = self.controller.PixelHistory(pipe.GetOutputTargets()[0].resource, 200, 150, rd.Subresource(0, 0, 0),
                                               rd.CompType.Typeless)

        # should be a clear then a draw
        assert len(history) == 2

        assert self.find_action('', history[0].eventId).flags & rd.ActionFlags.BeginPass

        assert self.find_action('', history[1].eventId).eventId == action.eventId
        assert history[1].Passed()

        if not rdtest.value_compare(history[1].shaderOut.col.floatValue, tri_col, eps=1.0 / 256.0):
            raise rdtest.TestFailureException(f"History for drawcall output is wrong: {history[1].shaderOut.col.floatValue}")

        inputs = rd.DebugPixelInputs()
        inputs.sample = 0
        inputs.primitive = 0
        trace = self.controller.DebugPixel(200, 150, inputs)

        cycles, variables = self.process_trace(trace)

        output_sourcevar = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

        if output_sourcevar is None:
            raise rdtest.TestFailureException("Couldn't get colour output value")

        debugged = self.evaluate_source_var(output_sourcevar, variables)

        self.controller.FreeTrace(trace)

        debuggedValue = list(debugged.value.f32v[0:4])

        is_eq, diff_amt = rdtest.value_compare_diff(history[1].shaderOut.col.floatValue, debuggedValue, eps=5.0E-06)
        if not is_eq:
            raise rdtest.TestFailureException(
                f"Debugged pixel value {debugged.name}: {diff_amt} difference. {debuggedValue} doesn't exactly match history shader output {history[1].shaderOut.col.floatValue}")

        rdtest.log.success(f'Successfully debugged pixel in {cycles} cycles, result matches')

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

        source = vsrefl.debugInfo.files[0].contents.replace('#if 0', '#if 1')

        newShader = self.controller.BuildTargetShader(vsrefl.entryPoint, rd.ShaderEncoding.GLSL, bytes(source, 'UTF-8'),
                                                      rd.ShaderCompileFlags(), rd.ShaderStage.Vertex)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException(f"Failed to compile edited shader: {newShader[1]}")

        self.controller.ReplaceResource(vsrefl.resourceId, newShader[0])

        # Refresh the replay if it didn't happen already
        self.set_event(last_action.eventId, True)

        tri_col2 = [0.906, 0.361, 0.182, 1.0]
        self.check_triangle(out=last_action.copyDestination, fore=tri_col2)

        rdtest.log.success("Edited shader had the right triangle output")

        self.controller.RemoveReplacement(vsrefl.resourceId)
        self.controller.FreeTargetResource(newShader[0])
