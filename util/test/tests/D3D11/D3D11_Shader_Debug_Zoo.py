import renderdoc as rd
import rdtest


class D3D11_Shader_Debug_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Shader_Debug_Zoo'

    def check_capture(self):
        # Jump to the draw
        draw = self.find_draw("Draw")

        self.controller.SetFrameEvent(draw.eventId, False)

        # Make an output so we can pick pixels
        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        self.check(out is not None)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        tex = rd.TextureDisplay()
        tex.resourceId = pipe.GetOutputTargets()[0].resourceId
        out.SetTextureDisplay(tex)

        # Loop over every test
        for test in range(draw.numInstances):
            # Pick the pixel
            picked: rd.PixelValue = out.PickPixel(tex.resourceId, False, 4 * test, 0, 0, 0, 0)

            # Debug the shader
            trace: rd.ShaderDebugTrace = self.controller.DebugPixel(4 * test, 0, rd.ReplayController.NoPreference,
                                                                    rd.ReplayController.NoPreference)

            last_state: rd.ShaderDebugState = trace.states[-1]

            if not rdtest.value_compare(picked.floatValue, last_state.outputs[0].value.fv[0:4]):
                raise rdtest.TestFailureException("Test {}: debugged output {} doesn't match actual output {}".format(test, last_state.outputs[0].value.fv[0:4], picked.floatValue))

            rdtest.log.success("Test {} matched as expected".format(test))

        rdtest.log.success("All tests matched")
