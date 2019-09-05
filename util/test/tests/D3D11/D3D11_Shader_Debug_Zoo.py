import renderdoc as rd
import rdtest


class D3D11_Shader_Debug_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Shader_Debug_Zoo'

    def check_capture(self):
        # Jump to the draw
        draw = self.find_draw("Draw")

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        # Loop over every test
        for test in range(draw.numInstances):
            # Debug the shader
            trace: rd.ShaderDebugTrace = self.controller.DebugPixel(4 * test, 0, rd.ReplayController.NoPreference,
                                                                    rd.ReplayController.NoPreference)

            last_state: rd.ShaderDebugState = trace.states[-1]

            self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 4 * test, 0, last_state.outputs[0].value.fv[0:4])

            rdtest.log.success("Test {} matched as expected".format(test))

        rdtest.log.success("All tests matched")
