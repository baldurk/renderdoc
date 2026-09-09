import renderdoc as rd
import rdtest
import struct


class D3D12_AMD_Shader_Extensions(rdtest.TestCase):
    demos_test_name = 'D3D12_AMD_Shader_Extensions'

    def check_capture(self):
        for pass_type in ["SM51", "SM60"]:
            action = self.find_action(pass_type + " Draw")

            if action is not None:
                self.controller.SetFrameEvent(action.nextAction.eventId, False)

                pipe = self.controller.GetPipelineState()
                tex = pipe.GetOutputTargets()[0].resource
                vp = pipe.GetViewport(0)

                # Should have barycentrics showing the closest vertex for each pixel in the triangle
                # Without relying on barycentric order, ensure that the three pixels are red, green, and blue
                pixels = []

                x = int(vp.x + vp.width * 0.5)
                y = int(vp.y + vp.height * 0.5)

                picked = self.controller.PickPixel(tex, x+ 0, y+ 0, rd.Subresource(), rd.CompType.UNorm)
                pixels.append(picked.floatValue[0:4])
                picked = self.controller.PickPixel(tex, x-20, y+20, rd.Subresource(), rd.CompType.UNorm)
                pixels.append(picked.floatValue[0:4])
                picked = self.controller.PickPixel(tex, x+20, y+20, rd.Subresource(), rd.CompType.UNorm)
                pixels.append(picked.floatValue[0:4])

                if (not (1.0, 0.0, 0.0, 1.0) in pixels) or (not (1.0, 0.0, 0.0, 1.0) in pixels) or (
                not (1.0, 0.0, 0.0, 1.0) in pixels):
                    raise rdtest.TestFailureException(f"Expected red, green and blue in picked pixels. Got {pixels}")

                rdtest.log.success("Picked barycentric values are as expected")

                action = self.find_action(pass_type + " Dispatch")

                self.controller.SetFrameEvent(action.nextAction.eventId, False)

                # find the cpuMax and gpuMax actions
                cpuMax = self.find_action(pass_type + " cpuMax")
                gpuMax = self.find_action(pass_type + " gpuMax")

                # The values should be identical
                cpuMax = int(cpuMax.customName.split(': ')[1])
                gpuMax = int(gpuMax.customName.split(': ')[1])

                if cpuMax != gpuMax or cpuMax == 0:
                    raise rdtest.TestFailureException(
                        f"captured cpuMax and gpuMax are not equal and positive: {cpuMax} vs {gpuMax}")

                rdtest.log.success("recorded cpuMax and gpuMax are as expected")

                outBuf = self.get_resource_by_name("outBuf")

                data = self.controller.GetBufferData(outBuf.resourceId, 0, 8)

                replayedGpuMax = struct.unpack("Q", data)[0]

                if replayedGpuMax != gpuMax:
                    raise rdtest.TestFailureException(
                        f"captured gpuMax and replayed gpuMax are not equal: {gpuMax} vs {replayedGpuMax}")

                rdtest.log.success("replayed gpuMax is as expected")
            # We should get everything except maybe DXIL
            elif pass_type != "SM60":
                raise rdtest.TestFailureException(f"Didn't find test action for {pass_type}")

            # We always check the CS pipe to ensure the reflection is OK
            cs_pipe = self.get_resource_by_name("cspipe" + pass_type)

            if cs_pipe is None:
                # everything but DXIL we must get, DXIL we may not be able to compile
                if pass_type != "SM60":
                    raise rdtest.TestFailureException(f"Didn't find compute pipeline for {pass_type}")
                continue

            pipe = cs_pipe.resourceId
            cs = rd.ResourceId()

            for d in cs_pipe.derivedResources + cs_pipe.parentResources:
                res = self.get_resource(d)
                if res.type == rd.ResourceType.Shader:
                    cs = res.resourceId
                    break

            refl = self.controller.GetShader(
                pipe, cs, rd.ShaderEntryPoint("main", rd.ShaderStage.Compute)
            )

            assert len(refl.readWriteResources) == 2
            assert [rw.name for rw in refl.readWriteResources] == ["inUAV", "outUAV"]

            # Don't test disassembly or debugging with DXIL, we don't do any of that
            if pass_type == "SM60":
                continue

            disasm = self.controller.DisassembleShader(pipe, refl, "")

            if "amd_u64_atomic" not in disasm:
                raise rdtest.TestFailureException(
                    f"Didn't find expected AMD opcode in disassembly: {disasm}")

            rdtest.log.success("compute shader disassembly is as expected")

            if refl.debugInfo.debuggable:
                self.controller.SetFrameEvent(self.find_action("Dispatch").eventId, False)

                trace = self.controller.DebugThread((0, 0, 0), (0, 0, 0))

                if trace.debugger is None:
                    self.controller.FreeTrace(trace)

                    raise rdtest.TestFailureException("Couldn't debug compute shader")

                cycles, variables = self.process_trace(trace)

                if cycles < 3:
                    raise rdtest.TestFailureException(f"Compute shader has too few cycles {cycles}")
            else:
                raise rdtest.TestFailureException(
                    f"Compute shader is listed as non-debuggable: {refl.debugInfo.debugStatus}")

            rdtest.log.success("compute shader debugged successfully")
