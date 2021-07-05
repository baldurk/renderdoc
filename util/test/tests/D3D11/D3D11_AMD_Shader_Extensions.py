import renderdoc as rd
import rdtest
import struct


class D3D11_AMD_Shader_Extensions(rdtest.TestCase):
    demos_test_name = 'D3D11_AMD_Shader_Extensions'

    def check_capture(self):
        action = self.get_last_action()

        self.controller.SetFrameEvent(action.eventId, False)

        # Should have barycentrics showing the closest vertex for each pixel in the triangle
        # Without relying on barycentric order, ensure that the three pixels are red, green, and blue
        pixels = []

        picked: rd.PixelValue = self.controller.PickPixel(action.copyDestination, 125, 215, rd.Subresource(),
                                                          rd.CompType.UNorm)
        pixels.append(picked.floatValue[0:4])
        picked: rd.PixelValue = self.controller.PickPixel(action.copyDestination, 200, 85, rd.Subresource(),
                                                          rd.CompType.UNorm)
        pixels.append(picked.floatValue[0:4])
        picked: rd.PixelValue = self.controller.PickPixel(action.copyDestination, 285, 215, rd.Subresource(),
                                                          rd.CompType.UNorm)
        pixels.append(picked.floatValue[0:4])

        if (not (1.0, 0.0, 0.0, 1.0) in pixels) or (not (1.0, 0.0, 0.0, 1.0) in pixels) or (
        not (1.0, 0.0, 0.0, 1.0) in pixels):
            raise rdtest.TestFailureException("Expected red, green and blue in picked pixels. Got {}".format(pixels))

        rdtest.log.success("Picked barycentric values are as expected")

        # find the cpuMax and gpuMax actions
        cpuMax = self.find_action("cpuMax")
        gpuMax = self.find_action("gpuMax")

        # The values should be identical
        cpuMax = int(cpuMax.customName[8:])
        gpuMax = int(gpuMax.customName[8:])

        if cpuMax != gpuMax or cpuMax == 0:
            raise rdtest.TestFailureException(
                "captured cpuMax and gpuMax are not equal and positive: {} vs {}".format(cpuMax, gpuMax))

        rdtest.log.success("recorded cpuMax and gpuMax are as expected")

        outBuf = self.get_resource_by_name("outBuf")

        data = self.controller.GetBufferData(outBuf.resourceId, 0, 8)

        replayedGpuMax = struct.unpack("Q", data)[0]

        if replayedGpuMax != gpuMax:
            raise rdtest.TestFailureException(
                "captured gpuMax and replayed gpuMax are not equal: {} vs {}".format(gpuMax, replayedGpuMax))

        rdtest.log.success("replayed gpuMax is as expected")

        cs = self.get_resource_by_name("cs")
        pipe = rd.ResourceId()

        refl: rd.ShaderReflection = self.controller.GetShader(pipe, cs.resourceId,
                                                              rd.ShaderEntryPoint("main", rd.ShaderStage.Compute))

        self.check(len(refl.readWriteResources) == 2)
        self.check([rw.name for rw in refl.readWriteResources] == ["inUAV", "outUAV"])

        disasm = self.controller.DisassembleShader(pipe, refl, "")

        if "amd_u64_atomic" not in disasm:
            raise rdtest.TestFailureException(
                "Didn't find expected AMD opcode in disassembly: {}".format(disasm))

        rdtest.log.success("compute shader disassembly is as expected")

        if refl.debugInfo.debuggable:
            self.controller.SetFrameEvent(self.find_action("Dispatch").eventId, False)

            trace: rd.ShaderDebugTrace = self.controller.DebugThread((0, 0, 0), (0, 0, 0))

            if trace.debugger is None:
                self.controller.FreeTrace(trace)

                raise rdtest.TestFailureException("Couldn't debug compute shader")

            cycles, variables = self.process_trace(trace)

            if cycles < 3:
                raise rdtest.TestFailureException("Compute shader has too few cycles {}".format(cycles))
        else:
            raise rdtest.TestFailureException(
                "Compute shader is listed as non-debuggable: {}".format(refl.debugInfo.debugStatus))

        rdtest.log.success("compute shader debugged successfully")