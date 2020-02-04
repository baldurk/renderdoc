import renderdoc as rd
import rdtest


class D3D12_Shader_Debug_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Shader_Debug_Zoo'

    def check_support(self):
        return False, 'shader debugging is not yet enabled for D3D12'

    def check_capture(self):

        failed = False

        shaderModels = ["sm_5_0", "sm_5_1"]
        for sm in range(len(shaderModels)):
            rdtest.log.print("Beginning " + shaderModels[sm] + " tests...")
            rdtest.log.indent()

            # Jump to the draw
            test_marker: rd.DrawcallDescription = self.find_draw(shaderModels[sm])
            draw = test_marker.next
            self.controller.SetFrameEvent(draw.eventId, True)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            # Loop over every test
            for test in range(draw.numInstances):
                # Debug the shader
                trace: rd.ShaderDebugTrace = self.controller.DebugPixel(4 * test, 0, rd.ReplayController.NoPreference,
                                                                        rd.ReplayController.NoPreference)

                last_state: rd.ShaderDebugState = trace.states[-1]

                try:
                    self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 4 * test, 0, last_state.outputs[0].value.fv[0:4], 0.0)
                except rdtest.TestFailureException as ex:
                    failed = True
                    rdtest.log.error("Test {} did not match. {}".format(test, str(ex)))
                    continue

                rdtest.log.success("Test {} matched as expected".format(test))

            rdtest.log.dedent()

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
