import renderdoc as rd
import struct
import rdtest

class Groupshared(rdtest.TestCase):
    internal = True
    slow_test = True
    demos_test_name = None

    def check_compute_thread_result(self, test: int, action: rd.ActionDescription, x: int, y: int, z: int, expected: rdtest.VectorValue):
        try:
            with self.debug_thread((0, 0, 0), (x, y, z)) as debug:
                _, variables = self.process_trace(debug.trace)


                # Find the source variable 'outval' at the highest instruction index
                name = 'outval'
                debugged = None
                countInst = len(debug.trace.instInfo)
                for inst in range(countInst):
                    sourceVars = debug.trace.instInfo[countInst-1-inst].sourceVars
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
                    raise rdtest.TestFailureException(f"Couldn't find source variable {name} at {x},{y},{z}")

                debuggedValue = list(debugged.value.f32v[0:4])

                if not rdtest.value_compare(expected, debuggedValue, eps=5.0E-06):
                    raise rdtest.TestFailureException(f"EID:{action.eventId} TID:{x},{y},{z} debugged thread value {debuggedValue} does not match output {expected}")

        except rdtest.TestFailureException as ex:
            rdtest.log.error(f"Test {test} failed {ex}")
            return False
        except Exception as ex:
            rdtest.log.error(f"Test {test} exception {ex}")
            return False

        return True

    def check_compute_tests(self, action: rd.ActionDescription):
        overallFailed = False
        tests = [a for a in action.children if a.flags & rd.ActionFlags.Dispatch]

        for test, action in enumerate(tests):
            failed = False
            self.set_event(action.eventId, False)

            pipe = self.controller.GetPipelineState()
            csrefl = pipe.GetShaderReflection(rd.ShaderStage.Compute)

            dim = csrefl.dispatchThreadsDimension

            rw = pipe.GetReadWriteResources(rd.ShaderStage.Compute)

            if len(rw) != 2:
                rdtest.log.error(f"Unexpected number of RW resources {len(rw)}")
                return False

            outBuf = rw[1].descriptor.resource
            # each test writes up to one vec4 per thread * up to 64 threads
            maxThreads = 64
            dataPerThread = 4 * 4
            dataPerTest = dataPerThread * maxThreads
            bufdata = self.controller.GetBufferData(outBuf, 0, dataPerTest)

            for x in range(dim[0]):
                y = 0
                z = 0
                expected: rdtest.VectorValue = struct.unpack_from("4f", bufdata, 16*x)
                # Test 2 is a special case with hard coded results
                if test == 2:
                    expected = (x, 1.25, 1.25, 1.25)

                if not self.check_compute_thread_result(test, action, x, y, z, expected):
                    failed = True

            overallFailed |= failed

            if not failed:
                rdtest.log.success(f"Tests at EID {action.eventId} successful")
            else:
                rdtest.log.error(f"Tests at EID {action.eventId} failed")

        return overallFailed

    def check_compute_section_tests(self, sectionAction: rd.ActionDescription):
        sectionName = sectionAction.customName
        rdtest.log.begin_section(sectionName)
        failed = self.check_compute_tests(sectionAction)
        rdtest.log.end_section(sectionName)
        if failed:
            raise rdtest.TestFailureException("Some tests were not as expected")

    def check_capture(self):
        action = self.find_action("Compute Tests")
        assert action is not None
        self.check_compute_section_tests(action)

        rdtest.log.success("All tests matched")