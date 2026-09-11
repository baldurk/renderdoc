import renderdoc as rd
import rdtest


class GL_Shader_Debug_Zoo(rdtest.TestCase):
    demos_test_name = 'GL_Shader_Debug_Zoo'
    slow_test = True

    def check_capture(self):
        assert self.controller is not None
        if not self.controller.GetAPIProperties().shaderDebugging:
            rdtest.log.success("Shader debugging not enabled, skipping test")
            return

        failed = False

        x = -3
        for child in ["GLSL tests", "SPIRV tests"]:
            rdtest.log.begin_section(child)
            section = self.find_action(child)
            for test in range(len(section.children)):
                action = section.children[test]

                if not (action.flags & rd.ActionFlags.Drawcall):
                    continue

                x += 4

                self.controller.SetFrameEvent(action.eventId, False)
                pipe = self.controller.GetPipelineState()

                if not pipe.GetShaderReflection(rd.ShaderStage.Vertex).debugInfo.debuggable:
                    rdtest.log.print(f"Skipping undebuggable shader at {test} in {child}.")
                    return

                if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                    rdtest.log.print(f"Skipping undebuggable shader at {test} in {child}.")
                    return

                y = 1

                inputs = rd.DebugPixelInputs()
                inputs.primitive = 0
                inputs.sample = 0

                if action.numIndices > 3:
                    rdtest.log.print("prim 1")
                    inputs.primitive = 1

                # Debug the shader
                trace = self.controller.DebugPixel(x, y, inputs)

                rdtest.log.print(f"debugging {x},{y}")

                if trace.debugger is None:
                    failed = True
                    rdtest.log.error(f"Test {test} in sub-section {child} did not debug pixel")
                    self.controller.FreeTrace(trace)
                    continue

                _, variables = self.process_trace(trace)

                output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)
                
                assert output is not None

                debugged = self.evaluate_source_var(output, variables)

                try:
                    self.check_pixel_value(pipe.GetOutputTargets()[0].resource, x, y, debugged.value.f32v[0:4])
                except rdtest.TestFailureException as ex:
                    failed = True
                    rdtest.log.error(f"Test {test} in sub-section {child} did not match pixel. {ex!s}")
                    continue
                finally:
                    self.controller.FreeTrace(trace)

                rdtest.log.success(f"Test {test} pixel in sub-section {child} matched as expected")
                
                vtx = 1
                inst = 0
                idx = vtx

                if action.flags & rd.ActionFlags.Instanced:
                    inst = 1

                if action.flags & rd.ActionFlags.Indexed:
                    ib = pipe.GetIBuffer()

                    mesh = rd.MeshFormat()
                    mesh.indexResourceId = ib.resourceId
                    mesh.indexByteStride = ib.byteStride
                    mesh.indexByteOffset = ib.byteOffset + action.indexOffset * ib.byteStride
                    mesh.indexByteSize = ib.byteSize
                    mesh.baseVertex = action.baseVertex

                    indices = rdtest.fetch_indices(self.controller, action, mesh, 0, vtx, 1)

                    idx = indices[1]

                    assert idx is not None

                postvs = self.get_postvs(action, rd.MeshDataStage.VSOut, first_index=vtx, num_indices=1, instance=inst)

                try:
                    self.check_vertex_debug(vtx, idx, inst, postvs, single_postvs=True, name_retry = lambda x: x.replace(".", "Block."))
                except rdtest.TestFailureException as err:
                    failed = True
                    rdtest.log.error(f"Error debugging vertex at test {test} in sub-section {child}: {err.message}")
                    continue

                rdtest.log.success(f"Test {test} vertex in sub-section {child} matched as expected")

            rdtest.log.end_section(child)

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
