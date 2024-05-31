import rdtest
import os
import random
import struct
from typing import List
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
        pipe: rd.PipeState = self.controller.GetPipelineState()

        texsave = rd.TextureSave()

        for res in pipe.GetOutputTargets():
            texsave.resourceId = res.resource
            texsave.mip = res.firstMip
            self.save_texture(texsave)

        depth = pipe.GetDepthTarget()
        texsave.resourceId = depth.resource
        texsave.mip = depth.firstMip
        self.save_texture(texsave)

        rdtest.log.success('Successfully saved images at {}'.format(action.eventId))

    def vert_debug(self, action: rd.ActionDescription):
        pipe: rd.PipeState = self.controller.GetPipelineState()

        refl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Vertex)

        if pipe.GetShader(rd.ShaderStage.Vertex) == rd.ResourceId.Null():
            rdtest.log.print("No vertex shader bound at {}".format(action.eventId))
            return

        if not (action.flags & rd.ActionFlags.Drawcall):
            rdtest.log.print("{} is not a debuggable action".format(action.eventId))
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

            striprestart_index = pipe.GetRestartIndex() & ((1 << (ib.byteStride*8)) - 1)

            if pipe.IsRestartEnabled() and idx == striprestart_index:
                return

        rdtest.log.print("Debugging vtx %d idx %d (inst %d)" % (vtx, idx, inst))

        postvs = self.get_postvs(action, rd.MeshDataStage.VSOut, first_index=vtx, num_indices=1, instance=inst)

        trace: rd.ShaderDebugTrace = self.controller.DebugVertex(vtx, inst, idx, 0)

        if trace.debugger is None:
            self.controller.FreeTrace(trace)

            rdtest.log.print("No debug result")
            return

        cycles, variables = self.process_trace(trace)

        outputs = 0

        for var in trace.sourceVars:
            var: rd.SourceVariableMapping
            if var.variables[0].type == rd.DebugVariableType.Variable and var.signatureIndex >= 0:
                name = var.name

                if name not in postvs[0].keys():
                    rdtest.log.error("Don't have expected output for {}".format(name))
                    continue

                expect = postvs[0][name]
                value = self.evaluate_source_var(var, variables)

                if len(expect) != value.columns:
                    rdtest.log.error(
                        "Output {} at EID {} has different size ({} values) to expectation ({} values)"
                            .format(name, action.eventId, value.columns, len(expect)))
                    continue

                compType = rd.VarTypeCompType(value.type)
                if compType == rd.CompType.UInt:
                    debugged = list(value.value.u32v[0:value.columns])
                elif compType == rd.CompType.SInt:
                    debugged = list(value.value.s32v[0:value.columns])
                else:
                    debugged = list(value.value.f32v[0:value.columns])

                # For now, ignore debugged values that are uninitialised. This is an application bug but it causes false
                # reports of problems
                for comp in range(4):
                    if value.value.u32v[comp] == 0xcccccccc:
                        debugged[comp] = expect[comp]

                # Unfortunately we can't ever trust that we should get back a matching results, because some shaders
                # rely on undefined/inaccurate maths that we don't emulate.
                # So the best we can do is log an error for manual verification
                is_eq, diff_amt = rdtest.value_compare_diff(expect, debugged, eps=5.0E-06)
                if not is_eq:
                    rdtest.log.error(
                        "Debugged value {} at EID {} vert {} (idx {}) instance {}: {} difference. {} doesn't exactly match postvs output {}".format(
                            name, action.eventId, vtx, idx, inst, diff_amt, debugged, expect))

                outputs = outputs + 1

        rdtest.log.success('Successfully debugged vertex in {} cycles, {}/{} outputs match'
                           .format(cycles, outputs, len(refl.outputSignature)))

        self.controller.FreeTrace(trace)

    def pixel_debug(self, action: rd.ActionDescription):
        pipe: rd.PipeState = self.controller.GetPipelineState()

        if pipe.GetShader(rd.ShaderStage.Pixel) == rd.ResourceId.Null():
            rdtest.log.print("No pixel shader bound at {}".format(action.eventId))
            return

        if len(pipe.GetOutputTargets()) == 0 and pipe.GetDepthTarget().resource == rd.ResourceId.Null():
            rdtest.log.print("No render targets bound at {}".format(action.eventId))
            return

        if not (action.flags & rd.ActionFlags.Drawcall):
            rdtest.log.print("{} is not a debuggable action".format(action.eventId))
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
            rdtest.log.print("Valid targets at {} are {}".format(action.eventId, valid_targets))
            if len(valid_targets) > 0:
                target = valid_targets[int(random.random()*len(valid_targets))]

        if target == rd.ResourceId.Null():
            target = pipe.GetDepthTarget().resource

        if target == rd.ResourceId.Null():
            rdtest.log.print("No targets bound! Can't fetch history at {}".format(action.eventId))
            return

        rdtest.log.print("Fetching history for %d,%d on target %s" % (x, y, str(target)))

        history = self.controller.PixelHistory(target, x, y, rd.Subresource(0, 0, 0), rd.CompType.Typeless)

        rdtest.log.success("Pixel %d,%d has %d history events" % (x, y, len(history)))

        lastmod: rd.PixelModification = None

        for i in reversed(range(len(history))):
            mod = history[i]
            action = self.find_action('', mod.eventId)

            if action is None or not (action.flags & rd.ActionFlags.Drawcall):
                continue

            rdtest.log.print("  hit %d at %d (%s)" % (i, mod.eventId, str(action.flags)))

            lastmod = history[i]

            rdtest.log.print("Got a hit on a action at event %d" % lastmod.eventId)

            if mod.sampleMasked or mod.backfaceCulled or mod.depthClipped or mod.viewClipped or mod.scissorClipped or mod.shaderDiscarded or mod.depthTestFailed or mod.stencilTestFailed:
                rdtest.log.print("This hit failed, looking for one that passed....")
                lastmod = None
                continue

            if not mod.shaderOut.IsValid():
                rdtest.log.print("This hit's shader out is not valid, looking for one that valid....")
                lastmod = None
                continue

            break

        if target == pipe.GetDepthTarget().resource:
            rdtest.log.print("Not doing pixel debug for depth output")
            return

        if lastmod is not None:
            rdtest.log.print("Debugging pixel {},{} @ {}, primitive {}".format(x, y, lastmod.eventId, lastmod.primitiveID))
            self.controller.SetFrameEvent(lastmod.eventId, True)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            inputs = rd.DebugPixelInputs()
            inputs.sample = 0
            inputs.primitive = lastmod.primitiveID;
            trace = self.controller.DebugPixel(x, y, inputs)

            if trace.debugger is None:
                self.controller.FreeTrace(trace)

                rdtest.log.print("No debug result")
                return

            cycles, variables = self.process_trace(trace)

            output_index = [o.resource for o in pipe.GetOutputTargets()].index(target)

            if action.outputs[0] == rd.ResourceId.Null():
                rdtest.log.success('Successfully debugged pixel in {} cycles, skipping result check due to no output'.format(cycles))
                self.controller.FreeTrace(trace)
            elif (action.flags & rd.ActionFlags.Instanced) and action.numInstances > 1:
                rdtest.log.success('Successfully debugged pixel in {} cycles, skipping result check due to instancing'.format(cycles))
                self.controller.FreeTrace(trace)
            elif pipe.GetColorBlends()[output_index].writeMask == 0:
                rdtest.log.success('Successfully debugged pixel in {} cycles, skipping result check due to write mask'.format(cycles))
                self.controller.FreeTrace(trace)
            else:
                rdtest.log.print("At event {} the target is index {}".format(lastmod.eventId, output_index))

                output_sourcevar = self.find_output_source_var(trace, rd.ShaderBuiltin.ColorOutput, output_index)

                if output_sourcevar is not None:
                    debugged = self.evaluate_source_var(output_sourcevar, variables)

                    self.controller.FreeTrace(trace)

                    debuggedValue = list(debugged.value.f32v[0:4])

                    # For now, ignore debugged values that are uninitialised. This is an application bug but it causes
                    # false reports of problems
                    for idx in range(4):
                        if debugged.value.u32v[idx] == 0xcccccccc:
                            debuggedValue[idx] = lastmod.shaderOut.col.floatValue[idx]

                    # Unfortunately we can't ever trust that we should get back a matching results, because some shaders
                    # rely on undefined/inaccurate maths that we don't emulate.
                    # So the best we can do is log an error for manual verification
                    is_eq, diff_amt = rdtest.value_compare_diff(lastmod.shaderOut.col.floatValue, debuggedValue, eps=5.0E-06)
                    if not is_eq:
                        rdtest.log.error(
                            "Debugged value {} at EID {} {},{}: {} difference. {} doesn't exactly match history shader output {}".format(
                                debugged.name, lastmod.eventId, x, y, diff_amt, debuggedValue, lastmod.shaderOut.col.floatValue))

                    rdtest.log.success('Successfully debugged pixel in {} cycles, result matches'.format(cycles))
                else:
                    # This could be an application error - undefined but seen in the wild
                    rdtest.log.error("At EID {} No output variable declared for index {}".format(lastmod.eventId, output_index))

            self.controller.SetFrameEvent(action.eventId, True)

    def mesh_output(self, action: rd.ActionDescription):
        self.controller.GetPostVSData(0, 0, rd.MeshDataStage.VSOut)
        self.controller.GetPostVSData(0, 0, rd.MeshDataStage.GSOut)

        rdtest.log.success('Successfully fetched mesh output')

    def drawcall_overlay(self, action: rd.ActionDescription):
        pipe = self.controller.GetPipelineState()

        if len(pipe.GetOutputTargets()) == 0 and pipe.GetDepthTarget().resource == rd.ResourceId.Null():
            rdtest.log.print("No render targets bound at {}".format(action.eventId))
            return

        if not (action.flags & rd.ActionFlags.Drawcall):
            rdtest.log.print("{} is not a drawcall".format(action.eventId))
            return

        tex = rd.TextureDisplay()
        tex.overlay = rd.DebugOverlay.Drawcall
        tex.resourceId = rd.ResourceId()

        col = pipe.GetOutputTargets()
        depth = pipe.GetDepthTarget()
        if len(col) > 1 and col[0].resourceId != rd.ResourceId():
            tex.resourceId = col[0].resourceId
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
        do_vert_debug = 1.0     # Chance of debugging a vertex (if valid)
        do_pixel_debug = 1.0    # Chance of doing pixel history at the current event and debugging a pixel (if valid)
        mesh_output = 1.0       # Chance of fetching mesh output data
        drawcall_overlay = 0.0  # Always show drawcall overlay when we run tests

        self.props: rd.APIProperties = self.controller.GetAPIProperties()

        event_tests = {
            'Image Save': {'chance': do_image_save, 'func': self.image_save},
            'Vertex Debug': {'chance': do_vert_debug, 'func': self.vert_debug},
            'Pixel History & Debug': {'chance': do_pixel_debug, 'func': self.pixel_debug},
            'Mesh Output': {'chance': mesh_output, 'func': self.mesh_output},
            'Drawcall overlay': {'chance': drawcall_overlay, 'func': self.drawcall_overlay},
        }

        # To choose an action, if we're going to do one, we take random in range(0, choice_max) then check each action
        # type in turn to see which part of the range we landed in
        choice_max = 0
        for event_test in event_tests:
            choice_max += event_tests[event_test]['chance']

        action = self.get_first_action()
        last_action = self.get_last_action()

        self.texout = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        while action:
            rdtest.log.print("{}/{}".format(action.eventId, last_action.eventId))

            self.controller.SetFrameEvent(action.eventId, False)

            rdtest.log.print("Set event")

            # If we should take an action at this event
            if random.random() < test_chance:
                c = random.random() * choice_max

                for event_test in event_tests:
                    chance = event_tests[event_test]['chance']
                    if c < chance or chance == 0.0:
                        rdtest.log.print("Performing test '{}' on event {}".format(event_test, action.eventId))
                        event_tests[event_test]['func'](action)
                        break
                    else:
                        c -= chance

            action = action.next

        self.texout.Shutdown()

    def run(self):
        dir_path = self.get_ref_path('', extra=True)

        for file in sorted(os.scandir(dir_path), key=lambda e: e.name.lower()):
            if '.rdc' not in file.name:
                continue

            # Ensure we are deterministic at least from run to run by seeding with the path
            random.seed(file.name)

            self.filename = file.name

            rdtest.log.print("Opening '{}'.".format(file.name))

            try:
                self.controller = rdtest.open_capture(file.path)
            except RuntimeError as err:
                rdtest.log.print("Skipping. Can't open {}: {}".format(file.path, err))
                continue

            section_name = 'Iterating {}'.format(file.name)

            rdtest.log.begin_section(section_name)
            self.iter_test()
            rdtest.log.end_section(section_name)

            self.controller.Shutdown()

        rdtest.log.success("Iterated all files")

    # Useful for calling from within the UI
    def run_external(self, controller: rd.ReplayController):
        self.controller = controller

        self.iter_test()


def run_locally(r):
    test = Iter_Test()
    test.run_external(r)
