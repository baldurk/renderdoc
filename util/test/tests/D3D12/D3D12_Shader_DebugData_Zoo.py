import renderdoc as rd
import rdtest
import struct

class D3D12_Shader_DebugData_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Shader_DebugData_Zoo'

    def parse_shader_source(self, shaderSrcRaw, realTestResult, test):
        '''
        // TEST_DEBUG_VAR_START
        TEST_DEBUG_VAR_DECLARE(int, testIndex, TEST_INDEX)
        TEST_DEBUG_VAR_DECLARE(int1, intVal, TEST_INDEX)
        TEST_DEBUG_VAR_DECLARE(float4, testResult, TEST_RESULT)
        TEST_DEBUG_VAR_DECLARE(int2, jake, 5)
        TEST_DEBUG_VAR_DECLARE(float1, bob, 3.0)
        TEST_DEBUG_VAR_USE(int4, testStruct.anon.a, 1)
        TEST_DEBUG_VAR_DECLARE_MATRIX23(float, fish, 7.2)
        // TEST_DEBUG_VAR_END
        '''
        varsToCheck = []
        foundStart = False
        foundEnd = False
        shaderSrc = shaderSrcRaw.splitlines()
        for line in shaderSrc:
            line = line.strip()
            if line.endswith('TEST_DEBUG_VAR_START'):
                foundStart = True
                continue
            if line.endswith('TEST_DEBUG_VAR_END'):
                foundEnd = True
                break
            if not foundStart:
                continue
            if not line.startswith('TEST_DEBUG_VAR_'):
                continue
            toks = line.split(',')
            type = toks[0].split('(')[1].strip()
            name = toks[1].strip()
            valString = toks[2].split(')')[0].strip()
            scalarType, countElems = self.parse_shader_var_type(type)
            isMatrix23 = line.startswith('TEST_DEBUG_VAR_DECLARE_MATRIX23')
            if isMatrix23:
                rows = 2
                columns = 3
                countElems = columns # each row is its own variable

            if valString == 'TEST_INDEX':
                value = test
            elif valString == 'TEST_RESULT':
                value = realTestResult
            else:
                if scalarType == 'float':
                    scalarVal = float(valString)
                elif scalarType == 'int':
                    scalarVal = int(valString)
                else:
                    raise rdtest.TestFailureException(f"Unhandled scalarType {scalarType} {type}")
                value = [scalarVal] * countElems

            if not isMatrix23:
                var = (name, type, value)
                varsToCheck.append(var)
            else:
                for row in range(rows):
                    var = (f"{name}.row{row}", f"{type}3", value)
                    varsToCheck.append(var)
        
        if not foundStart or not foundEnd:
            raise rdtest.TestFailureException("Couldn't find TEST_DEBUG_VAR_START and TEST_DEBUG_VAR_END")

        return varsToCheck

    def check_capture(self):
        if not self.controller.GetAPIProperties().shaderDebugging:
            rdtest.log.success("Shader debugging not enabled, skipping test")
            return

        failed = False

        shaderModels = [
            "sm_6_0", 
        ]
        for sm in range(len(shaderModels)):
            rdtest.log.begin_section(shaderModels[sm] + " tests")

            # Jump to the action
            test_marker: rd.ActionDescription = self.find_action(shaderModels[sm])
            if (test_marker is None):
                rdtest.log.print(f"Skipping Graphics tests for {shaderModels[sm]}")
                rdtest.log.end_section(shaderModels[sm] + " tests")
                continue
            action = test_marker.next
            self.controller.SetFrameEvent(action.eventId, False)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            if pipe.GetShaderReflection(rd.ShaderStage.Vertex).debugInfo.debuggable:
                # Debug the vertex shader
                instId = 1
                trace: rd.ShaderDebugTrace = self.controller.DebugVertex(0, instId, 0, 0)
                cycles, variables = self.process_trace(trace)
                output = self.find_output_source_var(trace, rd.ShaderBuiltin.Undefined, 1)
                debugged = self.evaluate_source_var(output, variables)
                actual = debugged.value.u32v[0]
                expected = instId
                if not rdtest.value_compare(actual, expected):
                    failed = True
                    rdtest.log.error(
                        f"Vertex shader TRIANGLE output did not match expectation {actual} != {expected}")

                if not failed:
                    rdtest.log.success("Basic VS debugging was successful")

                # Look for MAT0 variable in the trace initial source variables
                matched = True
                varsToCheck = []
                varsToCheck.append((f"MAT0[0]", "float4", [1.0, 2.0, 3.0, 4.0]))
                varsToCheck.append((f"MAT0[1]", "float4", [5.0, 6.0, 7.0, 8.0]))
                varsToCheck.append((f"MAT0[2]", "float4", [9.0, 10.0, 11.0, 12.0]))
                for name, varType, expectedValue in varsToCheck:
                    debuggedValue = None
                    try:
                        debuggedValue = self.get_source_shader_var_value(trace.sourceVars, name, varType, variables)
                    except KeyError as ex:
                        matched = False
                        failed = True
                    except rdtest.TestFailureException as ex:
                        matched = False
                        failed = True

                    if debuggedValue is None:
                        raise rdtest.TestFailureException(f"Couldn't find source variable {name} type:{varType}")
                    if not rdtest.value_compare(expectedValue, debuggedValue):
                        matched = False
                        failed = True
                        rdtest.log.error(f"'{name}' {varType} debugger {debuggedValue} doesn't match expected {expectedValue}")

                self.controller.FreeTrace(trace)
                if matched:
                    rdtest.log.success("VS MAT0 output source variable matched as expected")

            else:
                rdtest.log.print(f"Ignoring undebuggable Vertex shader at {action.eventId} for {shaderModels[sm]}.")

            if not pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo.debuggable:
                rdtest.log.print(f"Skipping undebuggable Pixel shader at {action.eventId} for {shaderModels[sm]}.")
                rdtest.log.end_section(shaderModels[sm] + " tests")
                continue

            # Loop over every test
            for test in range(action.numInstances):
                # Debug the shader
                trace: rd.ShaderDebugTrace = self.controller.DebugPixel(4 * test, 0, rd.DebugPixelInputs())
                cycles, variables = self.process_trace(trace)
                output = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, 0)
                debugged = self.evaluate_source_var(output, variables)

                try:
                    tex = pipe.GetOutputTargets()[0].resource
                    x = 4 * test
                    y = 0
                    self.check_pixel_value(tex, x, y, debugged.value.f32v[0:4])
                    picked = rd.PixelValue = self.controller.PickPixel(tex, x, y, rd.Subresource(0,0,0), rd.CompType.Typeless)
                    realTestResult = picked.floatValue
                    debugInfo = pipe.GetShaderReflection(rd.ShaderStage.Pixel).debugInfo
                    shaderSrcRaw = debugInfo.files[0].contents
                    varsToCheck = self.parse_shader_source(shaderSrcRaw, realTestResult, test)
                    for name, varType, expectedValue in varsToCheck:
                        debuggedValue = None
                        countInst = len(trace.instInfo)
                        for inst in range(countInst):
                            sourceVars = trace.instInfo[countInst-1-inst].sourceVars
                            try:
                                debuggedValue = self.get_source_shader_var_value(sourceVars, name, varType, variables)
                            except KeyError as ex:
                                continue
                            except rdtest.TestFailureException as ex:
                                continue
                            break
                        if debuggedValue is None:
                            raise rdtest.TestFailureException(f"Couldn't find source variable {name} type:{varType}")
                        if not rdtest.value_compare(expectedValue, debuggedValue):
                            raise rdtest.TestFailureException(f"'{name}' {varType} debugger {debuggedValue} doesn't match expected {expectedValue}")

                    rdtest.log.success(f"{len(varsToCheck)} source variables matched as expected")

                    # Look for _IN.MAT0 variable in the trace input variables
                    name = "_IN"
                    inVar = [v for v in trace.inputs if v.name == name]
                    if len(inVar) != 1:
                        raise rdtest.TestFailureException(f"Couldn't find source variable {name} type:{varType}")
                    name = "MAT0"
                    inVar = [v for v in inVar[0].members if v.name == name]
                    if len(inVar) != 1:
                        raise rdtest.TestFailureException(f"Couldn't find source variable {name} type:{varType}")

                    matched = True
                    varsToCheck = []
                    varsToCheck.append((f"[0]", "float4", [1.0, 2.0, 3.0, 4.0]))
                    varsToCheck.append((f"[1]", "float4", [5.0, 6.0, 7.0, 8.0]))
                    varsToCheck.append((f"[2]", "float4", [9.0, 10.0, 11.0, 12.0]))
                    for name, varType, expectedValue in varsToCheck:
                        debuggedValue = None
                        for v in inVar[0].members:
                            if v.name == name:
                                try:
                                    debuggedValue = v.value.f32v[0:len(expectedValue)]
                                except rdtest.TestFailureException as ex:
                                    matched = False
                                    failed = True
                                break
                        
                        if debuggedValue is None:
                            raise rdtest.TestFailureException(f"Couldn't find source variable {name} type:{varType}")
                        if not rdtest.value_compare(expectedValue, debuggedValue):
                            matched = False
                            failed = True
                            rdtest.log.error(f"'{name}' {varType} debugger {debuggedValue} doesn't match expected {expectedValue}")

                    if matched:
                        rdtest.log.success("PS MAT0 input source variable matched as expected")

                except rdtest.TestFailureException as ex:
                    rdtest.log.error(f"Test {test} failed {ex}")
                    failed = True
                    continue
                finally:
                    self.controller.FreeTrace(trace)

                rdtest.log.success("Test {} matched as expected".format(test))
                
            rdtest.log.end_section(shaderModels[sm] + " tests")

        csShaderModels = ["cs_6_0"]
        for sm in range(len(csShaderModels)):
            test = csShaderModels[sm]
            section = test + " tests"
            rdtest.log.begin_section(section)

            # Jump to the action
            test_marker: rd.ActionDescription = self.find_action(test)
            if test_marker is None:
                rdtest.log.print(f"Skipping Compute tests for {csShaderModels[sm]}")
                rdtest.log.end_section(section)
                continue
            action = test_marker.next
            self.controller.SetFrameEvent(action.eventId, False)
            pipe: rd.PipeState = self.controller.GetPipelineState()
            if not pipe.GetShaderReflection(rd.ShaderStage.Compute).debugInfo.debuggable:
                rdtest.log.print(f"Skipping undebuggable Compute shader at {action.eventId} for {csShaderModels[sm]}.")
                rdtest.log.end_section(section)
                continue

            # Loop over every test
            for test in range(action.dispatchDimension[0]):
                # Debug the shader
                groupid = (test,0,0)
                threadid = (0,0,0)
                trace: rd.ShaderDebugTrace = self.controller.DebugThread(groupid, threadid)
                cycles, variables = self.process_trace(trace)
                # Check for non-zero cycles
                if cycles == 0:
                    rdtest.log.error("Shader debug cycle count was zero")
                    failed = True
                    self.controller.FreeTrace(trace)
                    continue

                # Result is stored in RWStructuredBuffer<float4> bufOut : register(u0);
                bufOut = pipe.GetReadWriteResources(rd.ShaderStage.Compute)[0].descriptor.resource
                bufdata = self.controller.GetBufferData(bufOut, test*16, 16)
                realTestResult = struct.unpack_from("4f", bufdata, 0)
                debugInfo = pipe.GetShaderReflection(rd.ShaderStage.Compute).debugInfo
                shaderSrcRaw = debugInfo.files[0].contents
                varsToCheck = self.parse_shader_source(shaderSrcRaw, realTestResult, test)
                try:
                    for name, varType, expectedValue in varsToCheck:
                        debuggedValue = None
                        countInst = len(trace.instInfo)
                        for inst in range(countInst):
                            sourceVars = trace.instInfo[countInst-1-inst].sourceVars
                            try:
                                debuggedValue = self.get_source_shader_var_value(sourceVars, name, varType, variables)
                            except KeyError as ex:
                                continue
                            except rdtest.TestFailureException as ex:
                                continue
                            break
                        if debuggedValue is None:
                            raise rdtest.TestFailureException(f"Couldn't find source variable {name} type:{varType}")
                        if not rdtest.value_compare(expectedValue, debuggedValue):
                            raise rdtest.TestFailureException(f"'{name}' {varType} debugger {debuggedValue} doesn't match expected {expectedValue}")
                    rdtest.log.success(f"{len(varsToCheck)} source variables matched as expected")

                except rdtest.TestFailureException as ex:
                    rdtest.log.error(f"Test {test} failed {ex}")
                    failed = True
                    continue
                finally:
                    self.controller.FreeTrace(trace)

                rdtest.log.success("Test {} matched as expected".format(test))

            rdtest.log.end_section(section)

        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        self.check_renderdoc_log_asserts()

        rdtest.log.success("All tests matched")
