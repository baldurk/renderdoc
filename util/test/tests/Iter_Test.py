from __future__ import annotations
from typing import Callable, Dict, Tuple
import rdtest
import os
import random
import renderdoc as rd


class Iter_Test(rdtest.TestCase):
    slow_test = True

    def save_texture(self, texsave: rd.TextureSave):
        if texsave.resourceId == rd.ResourceId.Null():
            return

        rdtest.log.print("Saving image of " + str(texsave.resourceId))

        texsave.comp.blackPoint = 0.0
        texsave.comp.whitePoint = 1.0
        texsave.alpha = rd.AlphaMapping.BlendToCheckerboard

        filename = rdtest.get_tmp_path('texsave')

        texsave.destType = rd.FileType.HDR
        self.controller.SaveTexture(texsave, filename + ".hdr")

        texsave.destType = rd.FileType.JPG
        self.controller.SaveTexture(texsave, filename + ".jpg")

        texsave.mip = -1
        texsave.slice.sliceIndex = -1

        texsave.destType = rd.FileType.DDS
        self.controller.SaveTexture(texsave, filename + ".dds")

    def image_save(self, action: rd.ActionDescription):
        pipe = self.controller.GetPipelineState()

        texsave = rd.TextureSave()

        for res in pipe.GetOutputTargets():
            texsave.resourceId = res.resource
            texsave.mip = res.firstMip
            self.save_texture(texsave)

        depth = pipe.GetDepthTarget()
        texsave.resourceId = depth.resource
        texsave.mip = depth.firstMip
        self.save_texture(texsave)

        rdtest.log.success(f'Successfully saved images at {action.eventId}')

    def compute_debug(self, action: rd.ActionDescription):
        pipe = self.controller.GetPipelineState()

        if pipe.GetShader(rd.ShaderStage.Compute) == rd.ResourceId.Null():
            rdtest.log.print(f"No compute shader bound at {action.eventId}")
            return

        refl = pipe.GetShaderReflection(rd.ShaderStage.Compute)
        assert refl is not None

        if not (action.flags & rd.ActionFlags.Dispatch) and action.drawIndex == 0:
            rdtest.log.print(f"{action.eventId} is not a debuggable action")
            return

        if not refl.debugInfo.debuggable:
            rdtest.log.print(f"Compute shader is not debuggable at {action.eventId}")
            return

        wgSize = action.dispatchDimension
        if any(dim == 0 for dim in wgSize):
            rdtest.log.print(f"Empty dispatch ({wgSize[0]}x{wgSize[1]}x{wgSize[2]}), skipping")
            return

        groupid = [0,0,0]
        for i in range(3):
            groupid[i] = random.randint(0, wgSize[i]-1)

        threadid = [0,0,0]
        for i in range(3):
            threadid[i] = random.randint(0, refl.dispatchThreadsDimension[i]-1)

        groupid = (groupid[0], groupid[1], groupid[2])
        threadid = (threadid[0], threadid[1], threadid[2])

        rdtest.log.print(f"Debug Thread Workgroup:{wgSize} groupid:{groupid} threadid:{threadid}")
        with self.debug_thread(groupid, threadid) as debug:
            try:
                cycles, variables = self.process_trace(debug.trace)
            except rdtest.TestFailureException as err:
                rdtest.log.error(f"Error debugging: {err.message}")
                return

            rdtest.log.success(f'Successfully debugged compute shader in {cycles} cycles {len(refl.outputSignature)}')

    def vert_debug(self, action: rd.ActionDescription):
        assert self.controller is not None
        pipe = self.controller.GetPipelineState()

        refl = pipe.GetShaderReflection(rd.ShaderStage.Vertex)

        if pipe.GetShader(rd.ShaderStage.Vertex) == rd.ResourceId.Null():
            rdtest.log.print(f"No vertex shader bound at {action.eventId}")
            return

        if not refl.debugInfo.debuggable:
            rdtest.log.print(f"Vertex shader is not debuggable at {action.eventId}")
            return

        if not (action.flags & rd.ActionFlags.Drawcall) and action.drawIndex == 0:
            rdtest.log.print(f"{action.eventId} is not a debuggable action")
            return

        vtx = int(random.random()*action.numIndices)
        inst = 0
        idx = vtx

        if action.numIndices == 0:
            rdtest.log.print("Empty action (0 vertices), skipping")
            return

        if action.flags & rd.ActionFlags.Instanced:
            inst = int(random.random()*action.numInstances)
            if action.numInstances == 0:
                rdtest.log.print("Empty action (0 instances), skipping")
                return

        if action.flags & rd.ActionFlags.Indexed:
            ib = pipe.GetIBuffer()

            mesh = rd.MeshFormat()
            mesh.indexResourceId = ib.resourceId
            mesh.indexByteStride = ib.byteStride
            mesh.indexByteOffset = ib.byteOffset + action.indexOffset * ib.byteStride
            mesh.indexByteSize = ib.byteSize
            mesh.baseVertex = action.baseVertex

            indices = rdtest.fetch_indices(self.controller, action, mesh, 0, vtx, 1)

            if len(indices) < 1:
                rdtest.log.print("No index buffer, skipping")
                return

            idx = indices[0]

            if idx is None:
                rdtest.log.print("Index buffer out of bounds for idx 0, skipping")
                return

            striprestart_index = pipe.GetRestartIndex() & ((1 << (ib.byteStride*8)) - 1)

            if pipe.IsRestartEnabled() and idx == striprestart_index:
                return

        rdtest.log.print("Debugging vtx %d idx %d (inst %d)" % (vtx, idx, inst))

        postvs = self.get_postvs(action, rd.MeshDataStage.VSOut, first_index=vtx, num_indices=1, instance=inst)

        success, err = self.check_vertex_debug(vtx, idx, inst, postvs, fatal=False, eps=5.0E-06, single_postvs=True, ignore_uninit=True)
        if not success:
            rdtest.log.error(f"Error debugging at EID {action.eventId}: {err}")
            return

    def pixel_debug(self, action: rd.ActionDescription):
        pipe = self.controller.GetPipelineState()

        if pipe.GetShader(rd.ShaderStage.Pixel) == rd.ResourceId.Null():
            rdtest.log.print(f"No pixel shader bound at {action.eventId}")
            return

        if len(pipe.GetOutputTargets()) == 0 and pipe.GetDepthTarget().resource == rd.ResourceId.Null():
            rdtest.log.print(f"No render targets bound at {action.eventId}")
            return

        if not (action.flags & rd.ActionFlags.Drawcall):
            rdtest.log.print(f"{action.eventId} is not a debuggable action")
            return

        viewport = pipe.GetViewport(0)

        # TODO, query for some pixel this action actually touched.
        x = int(random.random()*viewport.width + viewport.x)
        y = int(random.random()*viewport.height + viewport.y)
        x = abs(x)
        y = abs(y)

        target = rd.ResourceId.Null()

        if len(pipe.GetOutputTargets()) > 0:
            valid_targets = [o.resource for o in pipe.GetOutputTargets() if o.resource != rd.ResourceId.Null()]
            rdtest.log.print(f"Valid targets at {action.eventId} are {valid_targets}")
            if len(valid_targets) > 0:
                target = valid_targets[int(random.random()*len(valid_targets))]

        if target == rd.ResourceId.Null():
            target = pipe.GetDepthTarget().resource

        if target == rd.ResourceId.Null():
            rdtest.log.print(f"No targets bound! Can't fetch history at {action.eventId}")
            return

        rdtest.log.print("Fetching history for %d,%d on target %s" % (x, y, str(target)))

        with self.pixel_history(target, x, y, rd.Subresource(0, 0, 0), rd.CompType.Typeless) as history:
            modifs = history.modifs

            rdtest.log.success("Pixel %d,%d has %d history events" % (x, y, len(modifs)))

            lastmod = None

            for i in reversed(range(len(modifs))):
                mod = modifs[i]
                next_action = self.find_action('', mod.eventId)

                if next_action is None:
                    continue

                action = next_action

                if not(action.flags & rd.ActionFlags.Drawcall):
                    if action.drawIndex == 0:
                        continue
                    if not(action.flags & rd.ActionFlags.Clear):
                        continue
                    if not(action.flags & rd.ActionFlags.Copy):
                        continue
                    if not(action.flags & rd.ActionFlags.Resolve):
                        continue

                rdtest.log.print("  hit %d at %d (%s)" % (i, mod.eventId, str(action.flags)))

                lastmod = modifs[i]

                rdtest.log.print("Got a hit on a action at event %d" % lastmod.eventId)

                if mod.sampleMasked or mod.backfaceCulled or mod.depthClipped or mod.viewClipped or mod.scissorClipped or mod.shaderDiscarded or mod.depthTestFailed or mod.stencilTestFailed:
                    rdtest.log.print("This hit failed, looking for one that passed....")
                    lastmod = None
                    continue

                if not mod.shaderOut.IsValid():
                    rdtest.log.print("This hit's shader out is not valid, looking for one that valid....")
                    lastmod = None
                    continue

                if mod.primitiveID == 0xffffffff:
                    rdtest.log.print("This hit's primitive ID is invalid, looking for one that is valid....")
                    lastmod = None
                    continue

                break

            if target == pipe.GetDepthTarget().resource:
                rdtest.log.print("Not doing pixel debug for depth output")
                return

            if lastmod is not None:
                rdtest.log.print(f"Debugging pixel {x},{y} @ {lastmod.eventId}, primitive {lastmod.primitiveID}")
                self.set_event(lastmod.eventId, True)

                pipe = self.controller.GetPipelineState()

                refl = pipe.GetShaderReflection(rd.ShaderStage.Pixel)
                if refl is None:
                    rdtest.log.print(f"Nothing to debug. No pixel shader bound at {action.eventId}")
                    return

                if not refl.debugInfo.debuggable:
                    rdtest.log.print(f"Pixel shader is not debuggable at {action.eventId}")
                    return

                inputs = rd.DebugPixelInputs()
                inputs.sample = 0
                inputs.primitive = lastmod.primitiveID;
                with self.debug_pixel(x, y, inputs) as debug:
                    try:
                        cycles, variables = self.process_trace(debug.trace)
                    except rdtest.TestFailureException as err:
                        rdtest.log.error(f"Error debugging: {err.message}")
                        return

                    output_index = [o.resource for o in pipe.GetOutputTargets()].index(target)

                    if action.outputs[0] == rd.ResourceId.Null():
                        rdtest.log.success(f'Successfully debugged pixel in {cycles} cycles, skipping result check due to no output')
                    elif (action.flags & rd.ActionFlags.Instanced) and action.numInstances > 1:
                        rdtest.log.success(f'Successfully debugged pixel in {cycles} cycles, skipping result check due to instancing')
                    elif pipe.GetColorBlends()[output_index].writeMask == 0:
                        rdtest.log.success(f'Successfully debugged pixel in {cycles} cycles, skipping result check due to write mask')
                    else:
                        rdtest.log.print(f"At event {lastmod.eventId} the target is index {output_index}")

                        try:
                            output_sourcevar = self.find_output_source_var(debug.trace, rd.ShaderBuiltin.ColorOutput, output_index)

                            debugged = self.evaluate_source_var(output_sourcevar, variables)

                            debuggedValue = list(debugged.value.f32v[0:4])

                            # For now, ignore debugged values that are uninitialised. This is an application bug but it causes
                            # false reports of problems
                            for idx in range(4):
                                if debugged.value.u32v[idx] == 0xcccccccc:
                                    debuggedValue[idx] = lastmod.shaderOut.col.floatValue[idx]

                            historyValue = list(lastmod.shaderOut.col.floatValue)

                            tex = self.get_texture(target)

                            historyValue = historyValue[0:tex.format.compCount]
                            debuggedValue = debuggedValue[0:tex.format.compCount]

                            # Unfortunately we can't ever trust that we should get back a matching results, because some shaders
                            # rely on undefined/inaccurate maths that we don't emulate.
                            # So the best we can do is log an error for manual verification
                            is_eq, diff_amt = rdtest.value_compare_diff(historyValue, debuggedValue, eps=5.0E-06)
                            if not is_eq:
                                rdtest.log.error(
                                    f"Debugged value {debugged.name} at EID {lastmod.eventId} {x},{y}: {diff_amt} difference. {debuggedValue} doesn't exactly match history shader output {historyValue}")

                            rdtest.log.success(f'Successfully debugged pixel in {cycles} cycles, result matches')
                        except rdtest.TestFailureException:
                            # This could be an application error - undefined but seen in the wild
                            rdtest.log.error(f"At EID {lastmod.eventId} No output variable declared for index {output_index}")

                self.set_event(action.eventId, True)

    def mesh_output(self, action: rd.ActionDescription):
        self.controller.GetPostVSData(0, 0, rd.MeshDataStage.VSOut)
        self.controller.GetPostVSData(0, 0, rd.MeshDataStage.GSOut)

        rdtest.log.success('Successfully fetched mesh output')

    def drawcall_overlay(self, action: rd.ActionDescription):
        pipe = self.controller.GetPipelineState()

        if len(pipe.GetOutputTargets()) == 0 and pipe.GetDepthTarget().resource == rd.ResourceId.Null():
            rdtest.log.print(f"No render targets bound at {action.eventId}")
            return

        if not (action.flags & rd.ActionFlags.Drawcall):
            rdtest.log.print(f"{action.eventId} is not a drawcall")
            return

        tex = rd.TextureDisplay()
        tex.overlay = rd.DebugOverlay.Drawcall
        tex.resourceId = rd.ResourceId()

        col = pipe.GetOutputTargets()
        depth = pipe.GetDepthTarget()
        if len(col) > 1 and col[0].resource != rd.ResourceId():
            tex.resourceId = col[0].resource
        elif depth.resource != rd.ResourceId():
            tex.resourceId = depth.resource

        if tex.resourceId != rd.ResourceId():
            self.texout.SetTextureDisplay(tex)
            self.texout.Display()
            rdtest.log.success('Successfully did drawcall overlay')

    def iter_test(self):
        # Handy tweaks when running locally to disable certain things

        test_chance = 0.1       # Chance of doing anything at all
        do_image_save = 0.25    # Chance of saving images of the outputs
        do_compute_debug = 1.0  # Chance of debugging a compute thread
        do_vert_debug = 1.0     # Chance of debugging a vertex (if valid)
        do_pixel_debug = 1.0    # Chance of doing pixel history at the current event and debugging a pixel (if valid)
        mesh_output = 1.0       # Chance of fetching mesh output data
        drawcall_overlay = 0.0  # Always show drawcall overlay when we run tests

        self.props = self.controller.GetAPIProperties()

        event_tests: Dict[str, Tuple[float, Callable[[rd.ActionDescription], None]]] = {
            'Image Save': (do_image_save, self.image_save),
            'Compute Debug': (do_compute_debug, self.compute_debug),
            'Vertex Debug': (do_vert_debug, self.vert_debug),
            'Pixel History & Debug': (do_pixel_debug, self.pixel_debug),
            'Mesh Output': (mesh_output, self.mesh_output),
            'Drawcall overlay': (drawcall_overlay, self.drawcall_overlay),
        }

        # To choose an action, if we're going to do one, we take random in range(0, choice_max) then check each action
        # type in turn to see which part of the range we landed in
        choice_max = 0.0
        for event_test in event_tests:
            choice_max += event_tests[event_test][0]

        action = self.get_first_action()
        last_action = self.get_last_action()

        self.texout = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        while action:
            rdtest.log.print(f"{action.eventId}/{last_action.eventId}")

            self.set_event(action.eventId, False)

            rdtest.log.print("Set event")

            # If we should take an action at this event
            if random.random() < test_chance:
                c = random.random() * choice_max

                for event_test in event_tests:
                    chance = event_tests[event_test][0]
                    if c < chance or chance == 0.0:
                        rdtest.log.print(f"Performing test '{event_test}' on event {action.eventId}")
                        event_tests[event_test][1](action)
                        break
                    else:
                        c -= chance

                fatal = self.controller.GetFatalErrorStatus()
                if fatal.code != rd.ResultCode.Succeeded:
                    rdtest.log.error(f"Fatal error detected: {fatal.Message()}")
                    break

            action = action.nextAction

        self.texout.Shutdown()

    def run(self):
        dir_path = self.get_ref_path('', extra=True)

        for file in sorted(os.scandir(dir_path), key=lambda e: e.name.lower()):
            if '.rdc' not in file.name:
                continue

            # Ensure we are deterministic at least from run to run by seeding with the path
            random.seed(file.name)

            self.filename = file.name

            rdtest.log.print(f"Opening '{file.name}'.")

            try:
                self.controller = rdtest.open_capture(file.path)
            except RuntimeError as err:
                rdtest.log.print(f"Skipping. Can't open {file.path}: {err}")
                continue

            section_name = f'Iterating {file.name}'
            if not self.validate_eventids(self.controller):
                raise rdtest.TestFailureException("ERROR: capture doesn't have valid event IDs.")

            rdtest.log.begin_section(section_name)
            self.iter_test()
            rdtest.log.end_section(section_name)

            self.controller.Shutdown()

        rdtest.log.success("Iterated all files")

    # Useful for calling from within the UI
    def run_external(self, controller: rd.ReplayController):
        self.controller = controller

        self.iter_test()


def run_locally(r: rd.ReplayController):
    test = Iter_Test()
    test.run_external(r)
