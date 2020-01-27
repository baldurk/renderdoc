import rdtest
import renderdoc as rd


class VK_Truncated_CBuffer(rdtest.TestCase):
    demos_test_name = 'VK_Truncated_CBuffer'

    def check_capture(self):
        draw = self.find_draw("Draw")

        self.check(draw is not None)

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        stage = rd.ShaderStage.Pixel

        cbuf: rd.BoundCBuffer = pipe.GetConstantBuffer(stage, 0, 0)

        variables = self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                               pipe.GetShader(stage),
                                                               pipe.GetShaderEntryPoint(stage), 0,
                                                               cbuf.resourceId, cbuf.byteOffset, cbuf.byteSize)

        outcol: rd.ShaderVariable = variables[1]

        self.check(outcol.name == "outcol")
        if not rdtest.value_compare(outcol.value.fv[0:4], [0.0, 0.0, 0.0, 0.0]):
            raise rdtest.TestFailureException("expected outcol to be 0s, but got {}".format(outcol.value.fv[0:4]))

        rdtest.log.success("CBuffer value was truncated as expected")
