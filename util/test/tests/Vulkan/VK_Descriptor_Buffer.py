import renderdoc as rd
import struct
import rdtest


class VK_Descriptor_Buffer(rdtest.TestCase):
    demos_test_name = 'VK_Descriptor_Buffer'

    def check_capture(self):
        eid = 0

        stage = rd.ShaderStage.Pixel

        for prefix in ["Normal", "Maint6"]:
            for test in range(1000):
                draw = self.find_action(f"{prefix} Test {test}", eid)

                if draw is None:
                    break

                rdtest.log.begin_section(f"{prefix} Test {test}")

                eid = draw.eventId

                self.set_event(draw.nextAction.eventId, False)

                pipe = self.controller.GetPipelineState()

                v = pipe.GetViewport(0)

                x = int(v.x) + int(v.width / 2)
                y = int(v.y) + int(v.height // 2)
                offset = int(v.height // 5)

                refl = pipe.GetShaderReflection(stage)

                if refl.debugInfo.debuggable:
                    self.check_debug_pixel(x, y)
                    self.check_debug_pixel(x, y+offset)
                    self.check_debug_pixel(x, y-offset)
                else:
                    rdtest.log.comment(f'Shader is not debuggable, not verifying')

                cbs = pipe.GetConstantBlocks(stage, True)

                out = pipe.GetOutputTargets()[0].resource

                res_data = [
                    (pipe.GetReadOnlyResources(stage, True), refl.readOnlyResources),
                    (pipe.GetReadWriteResources(stage, True), refl.readWriteResources),
                ]

                for reslist,refllist in res_data:
                    for res in reslist:
                        expected_name = refllist[res.access.index].name

                        if refllist[res.access.index].bindArraySize > 1:
                            if (res.access.arrayElement % 10) == 1:
                                if res.descriptor.resource != rd.ResourceId():
                                    raise rdtest.TestFailureException(
                                        f"Expected no resource to be bound at {expected_name}[{res.access.arrayElement}")
                                continue

                            expected_name = f'{expected_name}_{res.access.arrayElement}'

                        if res.descriptor.type == rd.DescriptorType.ImageSampler:
                            sampname = self.get_resource(res.descriptor.secondary).name

                            resname = self.get_resource(res.descriptor.resource).name

                            expected_samp = f'{expected_name}_samp'
                            expected_name = f'{expected_name}_tex'

                            if sampname != expected_samp:
                                raise rdtest.TestFailureException(
                                    f"Expected resource {resname} to be named {expected_name}") 
                        else:
                            resname = self.get_resource(res.descriptor.resource).name

                        if resname != expected_name:
                            raise rdtest.TestFailureException(
                                f"Expected resource {resname} to be named {expected_name}") 

                        # we don't care about sampler/texture contents, it is enough that we get the right name
                        rdtest.log.success(f"Resource {resname} bound as expected")

                for cb in pipe.GetConstantBlocks(stage, True):
                    expected_name = refl.constantBlocks[cb.access.index].name

                    # ignore the push constants
                    if not refl.constantBlocks[cb.access.index].bufferBacked and expected_name == 'push':
                        continue

                    if refl.constantBlocks[cb.access.index].bindArraySize > 1:
                        if (cb.access.arrayElement % 10) == 1:
                            if cb.descriptor.resource != rd.ResourceId():
                                raise rdtest.TestFailureException(
                                    f"Expected no resource to be bound at {expected_name}[{cb.access.arrayElement}")
                            continue

                        expected_name = f'{expected_name}_{cb.access.arrayElement}'

                    resname = self.get_resource(cb.descriptor.resource).name

                    if resname != expected_name:
                        raise rdtest.TestFailureException(
                            f"Expected resource {resname} to be named {expected_name}")

                    data = self.controller.GetBufferData(
                        cb.descriptor.resource,
                        cb.descriptor.byteOffset,
                        cb.descriptor.byteSize,
                    )

                    floats = struct.unpack_from("8f", data, 0)

                    picked = self.pick_pixel(
                        out, x, y, rd.Subresource(), rd.CompType.Float
                    )

                    output_vec = picked.floatValue[0:4]
                    second_vec = floats[4:8]

                    if not rdtest.value_compare(second_vec, output_vec):
                        raise rdtest.TestFailureException(
                            f"Expected buffer data {output_vec}, but got {second_vec}")

                    data = self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                                    pipe.GetShader(
                                                                        stage), stage,
                                                                    pipe.GetShaderEntryPoint(
                                                                        stage),
                                                                    cb.access.index, cb.descriptor.resource,
                                                                    cb.descriptor.byteOffset, cb.descriptor.byteSize)

                    if data[0].name != 'data':
                        raise rdtest.TestFailureException(
                            f"Expected 'data' member in cbuffers, got {data[0].name}")

                    if len(data[0].members) < 2:
                        raise rdtest.TestFailureException(
                            f"Expected 'data' member in cbuffers to have at least 2 entries, it has {len(data[0].members)}")

                    second_vec = data[0].members[1].value.f32v[0:4]

                    if not rdtest.value_compare(second_vec, output_vec):
                        raise rdtest.TestFailureException(
                            f"Expected constant data {output_vec}, but got {second_vec}")

                    rdtest.log.success(f"CBuffer {resname} bound as expected with correct data")

                rdtest.log.end_section(f"{prefix} Test {test}")
