from typing import List, Tuple

import renderdoc as rd
import struct
import rdtest

# Not a real test, re-used by API-specific tests
class Subgroup_Zoo(rdtest.TestCase):
    internal = True
    demos_test_name = None
    workgroup = (0, 0, 0)

    def check_compute_thread_result(self, test: int, action: rd.ActionDescription, x: int, y: int, z: int, dim: Tuple[int,int,int], bufdata: bytes):
        try:
            real = struct.unpack_from(
                "4f", bufdata, 16*y*dim[0] + 16*x)
        except Exception as ex:
            rdtest.log.error(f"Exception Test {test} failed {ex}")
            return False

        trace = self.controller.DebugThread(self.workgroup, (x, y, z))
        try:
            _, variables = self.process_trace(trace)

            if trace.debugger is None:
                raise rdtest.TestFailureException(f"Test {test} at {action.eventId} got no debug result at {x},{y},{z}")

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
                raise rdtest.TestFailureException(f"Couldn't find source variable {name} at {x},{y},{z}")

            debuggedValue = list(debugged.value.f32v[0:4])

            if not rdtest.value_compare(real, debuggedValue, eps=5.0E-06):
                raise rdtest.TestFailureException(f"EID:{action.eventId} TID:{x},{y},{z} debugged thread value {debuggedValue} does not match output {real}")

        except rdtest.TestFailureException as ex:
            rdtest.log.error(f"Test {test} failed {ex}")
            return False
        finally:
            self.controller.FreeTrace(trace)

        return True

    def check_compute_tests(self, compute_dims: List[rd.ActionDescription], thread_checks: List[int]):
        overallFailed = False
        for comp_dim in compute_dims:
            rdtest.log.begin_section(
                f"Compute tests with {comp_dim.customName} workgroup")

            compute_tests = [
                a for a in comp_dim.children if a.flags & rd.ActionFlags.Dispatch]

            for test, action in enumerate(compute_tests):
                failed = False
                self.controller.SetFrameEvent(action.eventId, False)

                pipe = self.controller.GetPipelineState()
                csrefl = pipe.GetShaderReflection(rd.ShaderStage.Compute)

                dim = csrefl.dispatchThreadsDimension

                rw = pipe.GetReadWriteResources(rd.ShaderStage.Compute)

                if len(rw) != 1:
                    rdtest.log.error("Unexpected number of RW resources")
                    continue

                # each test writes up to 16k data, one vec4 per thread * up to 1024 threads
                bufdata = self.controller.GetBufferData(
                    rw[0].descriptor.resource, test * 16 * 1024, 16 * 1024
                )

                for t in thread_checks:
                    xrange = 1
                    yrange = dim[1]
                    xbase = t
                    ybase = 0

                    # vertical orientation
                    if dim[1] > dim[0]:
                        xrange = dim[0]
                        yrange = 1
                        xbase = 0
                        ybase = t

                    for x in range(xbase, xbase+xrange):
                        for y in range(ybase, ybase+yrange):
                            z = 0

                            if x >= dim[0] or y >= dim[1]:
                                continue

                            if not self.check_compute_thread_result(test, action, x, y, z, dim, bufdata):
                                failed = True

                overallFailed |= failed
                if not failed:
                    rdtest.log.success(f"Test {test} successful")
                else:
                    rdtest.log.error(f"Test {test} failed")

            rdtest.log.end_section(
                f"Compute tests with {comp_dim.customName} workgroup")

        return overallFailed

    def check_capture(self):
        graphics_tests = [a for a in self.find_action(
            "Graphics Tests").children if a.flags & rd.ActionFlags.Drawcall]

        rdtest.log.begin_section("Graphics tests")

        # instances to check in instanced draws
        inst_checks = [0, 1, 5, 10]
        # pixels to check
        pixel_checks = [
            # top quad
            (0, 0), (1, 0), (0, 1), (1, 1),
            # middle quad (away from triangle border)
            (64, 56), (65, 56), (64, 57), (65, 57),
            # middle quad (on triangle border)
            (64, 64), (65, 64), (64, 65), (65, 65),
            # middle quad on other triangle
            (56, 64), (57, 64), (56, 65), (57, 65),
        ]
        clear_col = (123456.0, 789.0, 101112.0, 0.0)

        overallFailed = False
        for idx, action in enumerate(graphics_tests):
            failed = False
            self.controller.SetFrameEvent(action.eventId, False)

            pipe = self.controller.GetPipelineState()

            # check vertex output for every vertex
            for inst in [inst for inst in inst_checks if inst < action.numInstances]:
                for view in range(pipe.MultiviewBroadcastCount()):

                    postvs = self.get_postvs(
                        action, rd.MeshDataStage.VSOut, first_index=0, num_indices=action.numIndices, instance=inst)

                    for vtx in range(action.numIndices):
                        trace = self.controller.DebugVertex(vtx, inst, vtx, view)

                        if trace.debugger is None:
                            self.controller.FreeTrace(trace)

                            rdtest.log.error(
                                f"Test {idx} at {action.eventId} got no debug result at {vtx} inst {inst} view {view}")
                            failed = True
                            continue

                        _, variables = self.process_trace(trace)

                        for var in trace.sourceVars:
                            if var.name == 'vertdata':
                                name = var.name

                                if var.name not in postvs[vtx].keys():
                                    rdtest.log.error(
                                        f"Don't have expected output for {var.name}")
                                    failed = True
                                    continue

                                real = postvs[vtx][name]
                                assert rdtest.is_vector(real)
                                debugged = self.evaluate_source_var(
                                    var, variables)

                                if debugged.columns != 4 or len(real) != 4:
                                    rdtest.log.error(
                                        f"Vertex output is not the right size ({len(real)} vs {debugged.columns})")
                                    failed = True
                                    continue

                                if not rdtest.value_compare(real, debugged.value.f32v[0:4], eps=5.0E-06):
                                    rdtest.log.error(
                                        f"Test {idx} at {action.eventId} debugged vertex value {debugged.value.f32v[0:4]} at {vtx} instance {inst} view {view} does not match output {real}")
                                    failed = True

                        self.controller.FreeTrace(trace)

            # check some assorted pixel outputs
            target = pipe.GetOutputTargets()[0].resource

            for pixel in pixel_checks:
                for view in range(pipe.MultiviewBroadcastCount()):
                    x, y = pixel

                    picked = self.controller.PickPixel(
                        target, x, y, rd.Subresource(0, 0, 0), rd.CompType.Float
                    )

                    real = picked.floatValue

                    # silently skip pixels that weren't written to
                    if real == clear_col:
                        continue

                    inputs = rd.DebugPixelInputs()
                    inputs.sample = 0
                    inputs.primitive = rd.ReplayController.NoPreference
                    inputs.view = view
                    trace = self.controller.DebugPixel(x, y, inputs)

                    if trace.debugger is None:
                        self.controller.FreeTrace(trace)
                        rdtest.log.error(
                            f"Test {idx} at {action.eventId} got no debug result at {x},{y}")
                        failed = True
                        continue

                    _, variables = self.process_trace(trace)

                    output_sourcevar = self.find_output_source_var(
                        trace, rd.ShaderBuiltin.ColorOutput, 0)

                    if output_sourcevar is None:
                        rdtest.log.error("No output variable found")
                        failed = True
                        continue

                    debugged = self.evaluate_source_var(
                        output_sourcevar, variables)

                    self.controller.FreeTrace(trace)

                    debuggedValue = list(debugged.value.f32v[0:4])

                    if not rdtest.value_compare(real, debuggedValue, eps=5.0E-06):
                        rdtest.log.error(
                            f"Test {idx} at {action.eventId} debugged pixel value {debuggedValue} at {x},{y} in {view} does not match output {real}")
                        failed = True

            overallFailed |= failed
            if not failed:
                rdtest.log.success(f"Test {idx} successful")
            else:
                rdtest.log.error(f"Test {idx} failed")

        rdtest.log.end_section("Graphics tests")

        # threads to check. largest dimension only (all small dim checked)
        thread_checks = [
            # first few
            0, 1, 2,
            # near end of 32-subgroup and boundary
            30, 31, 32, 33, 34,
            # near end of 64-subgroup and boundary
            62, 63, 64, 64, 65,
            # large values spaced out with one near the end of our unaligned size
            100, 110, 120, 140, 149, 150, 160, 200, 250,
        ]
        compute_dims = [a for a in self.find_action(
            "Compute Tests").children if 'x' in a.customName]

        overallFailed |= self.check_compute_tests(compute_dims, thread_checks)

        if overallFailed:
            raise rdtest.TestFailureException("Some tests were not as expected")

        self.check_renderdoc_log_asserts()

        rdtest.log.success("All tests matched")
