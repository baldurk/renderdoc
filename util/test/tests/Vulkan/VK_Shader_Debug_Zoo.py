from typing import List

import renderdoc as rd
import rdtest
import struct

class VK_Shader_Debug_Zoo(rdtest.TestCase):
    demos_test_name = 'VK_Shader_Debug_Zoo'
    slow_test = True

    def check_capture(self):
        if not self.controller.GetAPIProperties().shaderDebugging:
            rdtest.log.success("Shader debugging not enabled, skipping test")
            return

        failed = False

        for test_name in ["GLSL1 tests", "GLSL2 tests", "ASM tests"]:
            rdtest.log.begin_section(test_name)
            action = self.find_action(test_name)
            for child in range(len(action.children)):
                section = action.children[child]
                self.controller.SetFrameEvent(section.eventId, False)
                pipe = self.controller.GetPipelineState()

                if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                    rdtest.log.print(f"Skipping undebuggable shader at {child} in {test_name}.")
                    return

                for test in range(section.numInstances):
                    x = 4 * test + 1
                    y = 4 * child + 1

                    # Debug the shader
                    trace = self.controller.DebugPixel(x, y, rd.DebugPixelInputs())

                    if trace.debugger is None:
                        failed = True
                        rdtest.log.error(f"Test {test} in sub-section {child} did not debug at all")
                        self.controller.FreeTrace(trace)
                        continue

                    _, variables = self.process_trace(trace)

                    output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)
                    
                    assert output is not None

                    debugged = self.evaluate_source_var(output, variables)

                    try:
                            
                        valscale = min(debugged.value.f32v[0:4])
                        eps = rdtest.FLT_EPSILON
                        if valscale > 1.0:
                            eps = 5.0e-05

                        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, x, y, debugged.value.f32v[0:4], eps=eps)
                    except rdtest.TestFailureException as ex:
                        failed = True
                        rdtest.log.error(f"Test {test} in sub-section {child} did not match. {ex!s}")
                        continue
                    finally:
                        self.controller.FreeTrace(trace)

                    rdtest.log.success(f"Test {test} in sub-section {child} matched as expected")
            rdtest.log.end_section(test_name)

            test_name = "Disassembly Tests"
            rdtest.log.begin_section(test_name)

            action = self.find_action("ASM tests")
            self.controller.SetFrameEvent(action.children[0].eventId, False)
            pipe = self.controller.GetPipelineState()
            refl = pipe.GetShaderReflection(rd.ShaderStage.Pixel)
            assert refl is not None
            disasm = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), refl, "")
            # Test for some expected strings in the disassembly
            expectedStrings: List[str] = []
            # OpSwitch disassembly of 32-bit and 64-bit literals
            expectedStrings.append("case 305419896:")
            expectedStrings.append("case 4063516280:")
            expectedStrings.append("case 1311768465173141112:")
            expectedStrings.append("case 17452669529668998776:")
            for exp in expectedStrings:
                if exp not in disasm:
                    failed = True
                    rdtest.log.error(f"Failed to find `{exp}` in disassembly")

            rdtest.log.end_section(test_name)

        # CS is 8x4x1
        thread_checks = [
            (0,0,0),
            (1,1,0),
            (2,2,0),
            (3,3,0),
            (4,2,0),
            (5,1,0),
            (6,0,0),
            (7,1,0),
        ]
        compute_dims = [a for a in self.find_action(
            "Compute Tests").children if 'x' in a.customName]
        for comp_dim in compute_dims:
            section = f"Compute tests with {comp_dim.customName}"
            rdtest.log.begin_section(section)

            compute_tests = [
                a for a in comp_dim.children if a.flags & rd.ActionFlags.Dispatch]

            for test, action in enumerate(compute_tests):
                self.controller.SetFrameEvent(action.eventId, False)
                pipe = self.controller.GetPipelineState()
                csrefl = pipe.GetShaderReflection(rd.ShaderStage.Compute)
                if not csrefl.debugInfo.debuggable:
                    rdtest.log.print(f"Compute shader is undebuggable at {action.eventId} for {test}.")
                    failed = True
                    continue

                rw = pipe.GetReadWriteResources(rd.ShaderStage.Compute)
                if len(rw) != 1:
                    rdtest.log.error("Unexpected number of RW resources")
                    failed = True
                    continue

                groupid = (1, 0, 0)
                dim = csrefl.dispatchThreadsDimension

                # Debug the shader
                for threadid in thread_checks:
                    # each test writes up to 16k data, one vec4 per thread * up to 1024 threads
                    bufdata = self.controller.GetBufferData(
                        rw[0].descriptor.resource, test*16*1024, 16*1024)
                    try:
                        expectedValue = struct.unpack_from(
                            "4f", bufdata, 16*threadid[1]*dim[0] + 16*threadid[0])
                    except Exception as ex:
                        rdtest.log.error(f"Exception Test {test} failed {ex}")
                        failed = True
                        continue

                    # Debug the shader
                    trace = self.controller.DebugThread(groupid, threadid)
                    cycles, variables = self.process_trace(trace)
                    # Check for non-zero cycles
                    if cycles == 0:
                        rdtest.log.success(f"Test {test} Group:{groupid} Thread:{threadid} : Shader debug cycle count was zero")
                        self.controller.FreeTrace(trace)
                        failed = True
                        continue

                    # Find the source variable 'testResult' at the highest instruction index
                    name = 'testResult'
                    debugged = None
                    countInst = len(trace.instInfo)
                    for inst in range(countInst):
                        sourceVars = trace.instInfo[countInst-1-inst].sourceVars
                        try:
                            dataVars = [v for v in sourceVars if v.name == name]
                            if len(dataVars) == 0:
                                continue
                            debugged = self.evaluate_source_var(dataVars[0], variables)
                        except KeyError as ex:
                            continue
                        except rdtest.TestFailureException as ex:
                            continue
                        break
                    if debugged is None:
                        raise rdtest.TestFailureException(f"Couldn't find source variable {name} at {test}")
                    debuggedValue = list(debugged.value.f32v[0:4])

                    if not rdtest.value_compare(expectedValue, debuggedValue, eps=5.0E-06):
                        rdtest.log.error(f"Test {test} Group:{groupid} Thread:{threadid} EID:{action.eventId} failed {name} debugger {debuggedValue} doesn't match expected {expectedValue}")
                        self.controller.FreeTrace(trace)
                        failed = True
                        continue

                    self.controller.FreeTrace(trace)
                    rdtest.log.success(f"Test {test} Group:{groupid} Thread:{threadid} as expected")

            rdtest.log.end_section(section)

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
