import renderdoc as rd
from typing import List
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

                cycles, variables = self.process_trace(trace)

                output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

                debugged = self.evalute_source_var(output, variables)

                try:
                    self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 4 * test, 0, debugged.value.fv[0:4], 0.0)
                except rdtest.TestFailureException as ex:
                    failed = True
                    rdtest.log.error("Test {} did not match. {}".format(test, str(ex)))
                    continue

                rdtest.log.success("Test {} matched as expected".format(test))

            rdtest.log.dedent()

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
