import renderdoc as rd
import rdtest
import struct

class D3D12_Shader_Debug_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Shader_Debug_Zoo'
    slow_test = True

    def check_compute_derivative_tests(self):
        failed = False
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
            "Compute Derivative Tests").children if 'x' in a.customName]
        for comp_dim in compute_dims:
            section = f"Compute tests with {comp_dim.customName}"
            rdtest.log.begin_section(section)

            compute_tests = [
                a for a in comp_dim.children if a.flags & rd.ActionFlags.Dispatch]

            for test, action in enumerate(compute_tests):
                self.set_event(action.eventId, False)
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
                    tid = ( min(threadid[0], dim[0]-1), min(threadid[1], dim[1]-1), min(threadid[2], dim[2]-1))
                    # each test writes up to 16k data, one vec4 per thread * up to 1024 threads
                    bufdata = self.controller.GetBufferData(
                        rw[0].descriptor.resource, test * 16 * 1024, 16 * 1024
                    )
                    try:
                        expectedValue = struct.unpack_from(
                            "4f", bufdata, 16*tid[1]*dim[0] + 16*tid[0])
                    except Exception as ex:
                        rdtest.log.error(f"Exception Test {test} failed {ex}")
                        failed = True
                        continue

                    # Debug the shader
                    trace = self.controller.DebugThread(groupid, tid)
                    cycles, variables = self.process_trace(trace)
                    # Check for non-zero cycles
                    if cycles == 0:
                        rdtest.log.success(f"Test {test} Group:{groupid} Thread:{tid} : Shader debug cycle count was zero")
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
                        rdtest.log.error(f"Test {test} Group:{groupid} Thread:{tid} EID:{action.eventId} failed {name} debugger {debuggedValue} doesn't match expected {expectedValue}")
                        self.controller.FreeTrace(trace)
                        failed = True
                        continue

                    self.controller.FreeTrace(trace)
                    rdtest.log.success(f"Test {test} Group:{groupid} Thread:{tid} as expected")

            rdtest.log.end_section(section)
        return failed

    def check_capture(self):
        if not self.controller.GetAPIProperties().shaderDebugging:
            rdtest.log.success("Shader debugging not enabled, skipping test")
            return

        undefined_tests = [int(test) for test in self.find_action("Undefined tests: ").customName.split(" ")[2:]]

        failed = False

        shaderModels = [
            "sm_5_0", "sm_5_1", "sm_6_0", "sm_6_2", "sm_6_6",
            "sm_5_0_opt", "sm_5_1_opt", "sm_6_0_opt", "sm_6_2_opt", "sm_6_6_opt",
        ]
        for j in range(2):
            for sm in range(len(shaderModels)):
                sectionName = shaderModels[sm] + " tests"

                # Jump to the action
                markerName = shaderModels[sm]
                instId = 10
                if j == 0:
                    markerName = "NoResources " + markerName
                    sectionName = "NoResources " + sectionName
                    instId = 2

                with rdtest.log.auto_section(sectionName):
                    test_marker = self.find_action(markerName)
                    if (test_marker is None):
                        rdtest.log.print(f"Skipping Graphics tests for {sectionName}")
                        continue
                    action = test_marker.nextAction
                    assert action is not None
                    self.set_event(action.eventId, False)

                    pipe = self.controller.GetPipelineState()

                    if pipe.GetShaderReflection(rd.ShaderStage.Vertex).debugInfo.debuggable:
                        postvs = self.get_postvs(action, rd.MeshDataStage.VSOut, instance=instId)

                        success, err = self.check_vertex_debug(0, 0, instId, postvs, fatal=False)
                        if not success:
                            failed = True
                            rdtest.log.error(f"Basic VS debugging didn't match: {err}")
                            continue
                        else:
                            rdtest.log.success("Basic VS debugging was successful")
                    else:
                        rdtest.log.print(f"Ignoring undebuggable Vertex shader at {action.eventId} for {shaderModels[sm]}.")

                    if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                        rdtest.log.print(f"Skipping undebuggable Pixel shader at {action.eventId} for {shaderModels[sm]}.")
                        continue

                    # Loop over every test
                    for test in range(action.numInstances):
                        # Debug the shader
                        trace = self.controller.DebugPixel(4 * test, 0, rd.DebugPixelInputs())

                        cycles, variables = self.process_trace(trace)

                        output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

                        assert output is not None

                        debugged = self.evaluate_source_var(output, variables)
                        self.controller.FreeTrace(trace)

                        try:
                            self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 4 * test, 0, debugged.value.f32v[0:4])
                        except rdtest.TestFailureException as ex:
                            if test in undefined_tests:
                                rdtest.log.comment(f"Undefined test {test} did not match. {ex!s}")
                            else:
                                rdtest.log.error(f"Test {test} did not match. {ex!s}")
                                failed = True
                            continue

                        rdtest.log.success(f"Test {test} matched as expected")

        rdtest.log.begin_section("MSAA tests")

        # Enable MSAA tests when DXIL shader debugger supports MSAA instructions and SV_Barycentrics
        msaaMarkers = [
            "MSAA sm_5_0",
            "MSAA sm_6_0",
            "MSAA sm_6_1",
        ]
        msaaMarkers = [
            "MSAA sm_5_0",
        ]
        for marker in msaaMarkers:
            rdtest.log.begin_section(marker)
            test_marker = self.find_action(marker)
            action = test_marker.nextAction
            self.set_event(action.eventId, False)
            pipe = self.controller.GetPipelineState()
            for (x,y) in [(4, 4), (4, 5), (3, 4), (3, 5)]:
                for test in range(4):
                    # Debug the shader
                    inputs = rd.DebugPixelInputs()
                    inputs.sample = test
                    trace = self.controller.DebugPixel(x, y, inputs)

                    # Validate that the correct sample index was debugged
                    sampRegister = self.find_input_source_var(trace, rd.ShaderBuiltin.MSAASampleIndex)
                    sampInput = [var for var in trace.inputs if var.name == sampRegister.variables[0].name][0]
                    if sampInput.value.u32v[0] != test:
                        rdtest.log.error(f"Test {test} did not pick the correct sample.")

                    cycles, variables = self.process_trace(trace)

                    output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)

                    assert output is not None

                    debugged = self.evaluate_source_var(output, variables)
                    self.controller.FreeTrace(trace)

                    # Validate the debug output result
                    try:
                        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, x, y, debugged.value.f32v[0:4], sub=rd.Subresource(0, 0, test))
                    except rdtest.TestFailureException as ex:
                        failed = True
                        rdtest.log.error(f"Test {test} did not match. {ex!s}")

            rdtest.log.end_section(marker)

        rdtest.log.end_section("MSAA tests")

        rdtest.log.begin_section("VertexSample tests")
        shaderModels = ["sm_5_0", "sm_6_0", "sm_6_6"]
        for sm in range(len(shaderModels)):
            test_marker = self.find_action("VertexSample " + shaderModels[sm])
            if test_marker is None:
                rdtest.log.print(f"Skipping Vertex Sample tests for {shaderModels[sm]}")
                continue
            action = test_marker.nextAction
            assert action is not None
            self.set_event(action.eventId, False)
            pipe = self.controller.GetPipelineState()

            if pipe.GetShaderReflection(rd.ShaderStage.Vertex).debugInfo.debuggable:
                # Debug the vertex shader
                success, err = self.check_vertex_debug(0, 0, 0, self.get_postvs(action, rd.MeshDataStage.VSOut), fatal=False)
                if not success:
                    failed = True
                    rdtest.log.error(
                        f"{shaderModels[sm]} Vertex shader failed: {err}")
                    continue
                else:
                    rdtest.log.success(shaderModels[sm] + " VertexSample VS was debugged correctly")
            else:
                rdtest.log.print(f"Skipping undebuggable Vertex shader at {action.eventId} for {shaderModels[sm]}.")

            if pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                # Debug the pixel shader
                inputs = rd.DebugPixelInputs()
                inputs.sample = 0
                trace = self.controller.DebugPixel(51, 51, inputs)
                cycles, variables = self.process_trace(trace)
                output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)
                assert output is not None
                debugged = self.evaluate_source_var(output, variables)
                self.controller.FreeTrace(trace)

                # Validate the debug output result
                try:
                    self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 51, 51, debugged.value.f32v[0:4])
                except rdtest.TestFailureException as ex:
                    failed = True
                    rdtest.log.error(f"Vertex sample pixel shader output did not match. {ex!s}")

                rdtest.log.success("VertexSample PS was debugged correctly")
            else:
                rdtest.log.print(f"Skipping undebuggable Pixel shader at {action.eventId} for {shaderModels[sm]}.")

        rdtest.log.end_section("VertexSample tests")

        test_marker = self.find_action("Banned")
        action = test_marker.nextAction
        assert action is not None
        self.set_event(action.eventId, False)
        pipe = self.controller.GetPipelineState()

        # Debug the banned vertex shader
        self.check_vertex_debug(0, 0, 0, self.get_postvs(action, rd.MeshDataStage.VSOut), fatal=False)

        # Debug the pixel shader
        inputs = rd.DebugPixelInputs()
        inputs.sample = 0
        trace = self.controller.DebugPixel(64, 64, inputs)

        cycles, variables = self.process_trace(trace)

        output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)
        assert output is not None

        debugged = self.evaluate_source_var(output, variables)
        self.controller.FreeTrace(trace)

        # Validate the debug output result
        try:
            self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 64, 64, debugged.value.f32v[0:4])
        except rdtest.TestFailureException as ex:
            failed = True
            rdtest.log.error(f"Vertex sample pixel shader output did not match. {ex!s}")

        rdtest.log.success("Banned signature PS was debugged correctly")

        csShaderModels = ["cs_5_0", "cs_6_0", "cs_6_6"]
        for sm in range(len(csShaderModels)):
            test = csShaderModels[sm]
            section = test + " tests"
            rdtest.log.begin_section(section)

            # Jump to the action
            test_marker = self.find_action(test)
            if test_marker is None:
                rdtest.log.print(f"Skipping Compute tests for {csShaderModels[sm]}")
                rdtest.log.end_section(section)
                continue
            action = test_marker.nextAction
            self.set_event(action.eventId, False)
            pipe = self.controller.GetPipelineState()
            if not pipe.GetShaderReflection(rd.ShaderStage.Compute).debugInfo.debuggable:
                rdtest.log.print(f"Skipping undebuggable Compute shader at {action.eventId} for {csShaderModels[sm]}.")
                rdtest.log.end_section(section)
                continue

            # Debug the shader
            for groupX in range(action.dispatchDimension[0]):
                groupid = (groupX, 1, 0)
                threadid = (0, 0, 0)
                testIndex = groupX
                trace = self.controller.DebugThread(groupid, threadid)
                cycles, variables = self.process_trace(trace)
                # Check for non-zero cycles
                if cycles == 0:
                    rdtest.log.error("Shader debug cycle count was zero")
                    self.controller.FreeTrace(trace)
                    failed = True
                    continue

                # Result is stored in RWStructuredBuffer<uint4> bufOut : register(u1);
                bufOut = pipe.GetReadWriteResources(rd.ShaderStage.Compute)[1].descriptor.resource
                bufdata = self.controller.GetBufferData(bufOut, testIndex*16, 16)
                expectedValue = struct.unpack_from("4i", bufdata, 0)
                # Test result is in variable called "int4 testResult"
                name = 'testResult'
                varType = 'int4'
                try:
                    debuggedValue = self.get_source_shader_var_value(trace.instInfo[-1].sourceVars, name, varType, variables)
                    if not rdtest.value_compare(expectedValue, debuggedValue):
                        raise rdtest.TestFailureException(f"'{name}' debugger {debuggedValue} doesn't match expected {expectedValue}")

                except rdtest.TestFailureException as ex:
                    rdtest.log.error(f"Test {test} Group:{groupid} Thread:{threadid} Index:{testIndex} failed {ex}")
                    failed = True
                    continue
                finally:
                    self.controller.FreeTrace(trace)

                rdtest.log.success(f"Test {test} Group:{groupid} Thread:{threadid} as expected")
            rdtest.log.end_section(section)

        if self.check_compute_derivative_tests():
            failed = True

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        rdtest.log.success("All tests matched")
