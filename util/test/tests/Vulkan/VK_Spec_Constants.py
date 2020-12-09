import renderdoc as rd
import rdtest


class VK_Spec_Constants(rdtest.TestCase):
    demos_test_name = 'VK_Spec_Constants'

    def check_capture(self):
        # find the first draw
        draw = self.find_draw("Draw")

        # We should have 4 draws, with spec constant values 0, 1, 2, 3
        for num_colors in range(4):
            self.check(draw is not None)

            self.controller.SetFrameEvent(draw.eventId, False)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            shader: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Pixel)

            # uniform buffer and spec constants
            self.check(len(shader.constantBlocks) == 2)
            self.check(shader.constantBlocks[0].bufferBacked)
            self.check(not shader.constantBlocks[1].bufferBacked)
            self.check(len(shader.constantBlocks[1].variables) == 1)

            # should be an array of num_colors+1 elements
            array_len = shader.constantBlocks[0].variables[0].type.descriptor.elements
            if not rdtest.value_compare(array_len, num_colors+1):
                raise rdtest.TestFailureException("CBuffer variable is array of {}, not {}".format(array_len, num_colors+1))

            if num_colors > 0:
                cbuf: rd.BoundCBuffer = pipe.GetConstantBuffer(rd.ShaderStage.Pixel, 0, 0)

                cb_vars = self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                                     pipe.GetShader(rd.ShaderStage.Pixel),
                                                                     pipe.GetShaderEntryPoint(rd.ShaderStage.Pixel), 0,
                                                                     cbuf.resourceId, cbuf.byteOffset, cbuf.byteSize)

                self.check(len(cb_vars) == 1)

                if not rdtest.value_compare(len(cb_vars[0].members), num_colors+1):
                    raise rdtest.TestFailureException("CBuffer variable is array of {}, not {}".format(len(cb_vars[0].members), num_colors+1))

                for col in range(num_colors):
                    expected = [0.0, 0.0, 0.0, 0.0]
                    expected[col] = 1.0

                    val = [i for i in cb_vars[0].members[col].value.f32v[0:4]]

                    if not rdtest.value_compare(val, expected):
                        raise rdtest.TestFailureException("Cbuffer[{}] value {} doesn't match expectation {}".format(col, val, expected))

                rdtest.log.success("Draw with {} colors uniform buffer is as expected".format(num_colors))

            cbuf: rd.BoundCBuffer = pipe.GetConstantBuffer(rd.ShaderStage.Pixel, 1, 0)

            cb_vars = self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                                 pipe.GetShader(rd.ShaderStage.Pixel),
                                                                 pipe.GetShaderEntryPoint(rd.ShaderStage.Pixel), 1,
                                                                 cbuf.resourceId, cbuf.byteOffset, cbuf.byteSize)

            self.check(len(cb_vars) == 1)

            if not rdtest.value_compare(cb_vars[0].value.s32v[0], num_colors):
                raise rdtest.TestFailureException("Spec constant is {}, not {}".format(cb_vars[0].value.s32v[0], num_colors))

            rdtest.log.success("Draw with {} colors specialisation constant is as expected".format(num_colors))

            view = pipe.GetViewport(0)

            # the first num_colors components should be 0.6, the rest should be 0.1 (alpha is always 1.0)
            expected = [0.0, 0.0, 0.0, 1.0]
            for col in range(num_colors):
                expected[col] += 1.0

            # Sample the centre of the viewport
            self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, int(view.x) + int(view.width / 2), int(view.height / 2), expected)

            rdtest.log.success("Draw with {} colors picked value is as expected".format(num_colors))

            draw = draw.next
