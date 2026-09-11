import renderdoc as rd
import rdtest
import struct


class VK_Spec_Constants(rdtest.TestCase):
    demos_test_name = 'VK_Spec_Constants'

    def check_capture(self):
        # find the first action
        action = self.find_action("Draw")

        # We should have 4 actions, with spec constant values 0, 1, 2, 3
        for num_colors in range(4):
            assert action is not None

            self.set_event(action.eventId, False)

            pipe = self.controller.GetPipelineState()
            vkpipe = self.controller.GetVulkanPipelineState()

            shader = pipe.GetShaderReflection(rd.ShaderStage.Pixel)

            # uniform buffer and spec constants
            assert len(shader.constantBlocks) == 2
            assert shader.constantBlocks[0].bufferBacked
            assert not shader.constantBlocks[1].bufferBacked
            assert len(shader.constantBlocks[1].variables) == 3

            # should be an array of num_colors+1 elements
            array_len = shader.constantBlocks[0].variables[0].type.elements
            if not rdtest.value_compare(array_len, num_colors+1):
                raise rdtest.TestFailureException(f"CBuffer variable is array of {array_len}, not {num_colors + 1}")

            if num_colors > 0:
                cbuf = pipe.GetConstantBlock(rd.ShaderStage.Pixel, 0, 0).descriptor

                cb_vars = self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                                     pipe.GetShader(rd.ShaderStage.Pixel), rd.ShaderStage.Pixel,
                                                                     pipe.GetShaderEntryPoint(rd.ShaderStage.Pixel), 0,
                                                                     cbuf.resource, cbuf.byteOffset, cbuf.byteSize)

                assert len(cb_vars) == 1

                if not rdtest.value_compare(len(cb_vars[0].members), num_colors+1):
                    raise rdtest.TestFailureException(f"CBuffer variable is array of {len(cb_vars[0].members)}, not {num_colors + 1}")

                for col in range(num_colors):
                    expected = [0.0, 0.0, 0.0, 0.0]
                    expected[col] = 1.0

                    val = [i for i in cb_vars[0].members[col].value.f32v[0:4]]

                    if not rdtest.value_compare(val, expected):
                        raise rdtest.TestFailureException(f"Cbuffer[{col}] value {val} doesn't match expectation {expected}")

                rdtest.log.success(f"Draw with {num_colors} colors uniform buffer is as expected")

            cbuf = pipe.GetConstantBlock(rd.ShaderStage.Pixel, 1, 0).descriptor

            cb_vars = self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                                 pipe.GetShader(rd.ShaderStage.Pixel), rd.ShaderStage.Pixel,
                                                                 pipe.GetShaderEntryPoint(rd.ShaderStage.Pixel), 1,
                                                                 cbuf.resource, cbuf.byteOffset, cbuf.byteSize)

            assert len(cb_vars) == 3

            refl = pipe.GetShaderReflection(rd.ShaderStage.Pixel)

            for cb_var in cb_vars:
                refl_var = None

                for cb in refl.constantBlocks:
                    if not cb.bufferBacked:
                        for cbv in cb.variables:
                            if cbv.name == cb_var.name:
                                refl_var = cbv
                                break
                        break

                if cb_var.name == "numcols":
                    if not rdtest.value_compare(cb_var.value.s32v[0], num_colors):
                        raise rdtest.TestFailureException(
                            f"{cb_var.name} spec constant is {cb_var.value.s32v[0]}, not {num_colors}")

                    read = struct.unpack_from("L", vkpipe.fragmentShader.specializationData, refl_var.byteOffset)[0]

                    if not rdtest.value_compare(read, num_colors):
                        raise rdtest.TestFailureException(
                            f"{cb_var.name} spec constant read manually is {read}, not {num_colors}")

                    rdtest.log.success(f"Draw with {num_colors} colors constant {cb_var.name} is as expected")
                elif cb_var.name == "NOT_numcols":
                    expected = 999
                    if num_colors == 2:
                        expected = 9999

                    if not rdtest.value_compare(cb_var.value.s32v[0], expected):
                        raise rdtest.TestFailureException(
                            f"{cb_var.name} spec constant is {cb_var.value.s32v[0]}, not {expected}")

                    read = struct.unpack_from("L", vkpipe.fragmentShader.specializationData, refl_var.byteOffset)[0]

                    if not rdtest.value_compare(read, expected):
                        raise rdtest.TestFailureException(
                            f"{cb_var.name} spec constant read manually is {read}, not {expected}")

                    rdtest.log.success(f"Draw with {num_colors} colors constant {cb_var.name} is as expected")
                elif cb_var.name == "some_float":
                    expected = 1.5
                    if num_colors == 1:
                        expected = 2.5
                    if num_colors == 2:
                        expected = 16.5

                    if not rdtest.value_compare(cb_var.value.f32v[0], expected):
                        raise rdtest.TestFailureException(
                            f"{cb_var.name} spec constant is {cb_var.value.f32v[0]}, not {expected}")

                    read = struct.unpack_from("f", vkpipe.fragmentShader.specializationData, refl_var.byteOffset)[0]

                    if not rdtest.value_compare(read, expected):
                        raise rdtest.TestFailureException(
                            f"{cb_var.name} spec constant read manually is {read}, not {expected}")

                    rdtest.log.success(f"Draw with {num_colors} colors constant {cb_var.name} is as expected")
                else:
                    raise rdtest.TestFailureException(f"Spec constant {cb_var.name} is unexpected")

            rdtest.log.success(f"Draw with {num_colors} colors specialisation constant is as expected")

            view = pipe.GetViewport(0)

            # the first num_colors components should be 0.6, the rest should be 0.1 (alpha is always 1.0)
            expected = [0.0, 0.0, 0.0, 1.0]
            for col in range(num_colors):
                expected[col] += 1.0

            # Sample the centre of the viewport
            self.check_pixel_value(pipe.GetOutputTargets()[0].resource, int(view.x) + int(view.width / 2), int(view.height / 2), expected)

            rdtest.log.success(f"Draw with {num_colors} colors picked value is as expected")

            action = action.nextAction
