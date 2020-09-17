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

    def image_save(self, draw: rd.DrawcallDescription):
        pipe: rd.PipeState = self.controller.GetPipelineState()

        texsave = rd.TextureSave()

        for res in pipe.GetOutputTargets():
            texsave.resourceId = res.resourceId
            texsave.mip = res.firstMip
            self.save_texture(texsave)

        depth = pipe.GetDepthTarget()
        texsave.resourceId = depth.resourceId
        texsave.mip = depth.firstMip
        self.save_texture(texsave)

        rdtest.log.success('Successfully saved images at {}: {}'.format(draw.eventId, draw.name))

    def vert_debug(self, draw: rd.DrawcallDescription):
        pipe: rd.PipeState = self.controller.GetPipelineState()

        refl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Vertex)

        if pipe.GetShader(rd.ShaderStage.Vertex) == rd.ResourceId.Null():
            rdtest.log.print("No vertex shader bound at {}: {}".format(draw.eventId, draw.name))
            return

        if not (draw.flags & rd.DrawFlags.Drawcall):
            rdtest.log.print("{}: {} is not a debuggable drawcall".format(draw.eventId, draw.name))
            return

        vtx = int(random.random()*draw.numIndices)
        inst = 0
        idx = vtx

        if draw.numIndices == 0:
            rdtest.log.print("Empty drawcall (0 vertices), skipping")
            return

        if draw.flags & rd.DrawFlags.Instanced:
            inst = int(random.random()*draw.numInstances)
            if draw.numInstances == 0:
                rdtest.log.print("Empty drawcall (0 instances), skipping")
                return

        if draw.flags & rd.DrawFlags.Indexed:
            ib = pipe.GetIBuffer()

            mesh = rd.MeshFormat()
            mesh.indexResourceId = ib.resourceId
            mesh.indexByteStride = draw.indexByteWidth
            mesh.indexByteOffset = ib.byteOffset + draw.indexOffset * draw.indexByteWidth
            mesh.indexByteSize = ib.byteSize
            mesh.baseVertex = draw.baseVertex

            indices = rdtest.fetch_indices(self.controller, draw, mesh, 0, vtx, 1)

            if len(indices) < 1:
                rdtest.log.print("No index buffer, skipping")
                return

            idx = indices[0]

            striprestart_index = pipe.GetStripRestartIndex() & ((1 << (draw.indexByteWidth*8)) - 1)

            if pipe.IsStripRestartEnabled() and idx == striprestart_index:
                return

        rdtest.log.print("Debugging vtx %d idx %d (inst %d)" % (vtx, idx, inst))

        postvs = self.get_postvs(draw, rd.MeshDataStage.VSOut, first_index=vtx, num_indices=1, instance=inst)

        trace: rd.ShaderDebugTrace = self.controller.DebugVertex(vtx, inst, idx)

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
                    raise rdtest.TestFailureException("Don't have expected output for {}".format(name))

                expect = postvs[0][name]
                value = self.evaluate_source_var(var, variables)

                if len(expect) != value.columns:
                    raise rdtest.TestFailureException(
                        "Output {} at EID {} has different size ({} values) to expectation ({} values)"
                            .format(name, draw.eventId, value.columns, len(expect)))

                compType = rd.VarTypeCompType(value.type)
                if compType == rd.CompType.UInt:
                    debugged = value.value.uv[0:value.columns]
                elif compType == rd.CompType.SInt:
                    debugged = value.value.iv[0:value.columns]
                else:
                    debugged = value.value.fv[0:value.columns]

                # For now, ignore debugged values that are uninitialised. This is an application bug but it causes false reports of problems
                for comp in range(4):
                    if value.value.uv[comp] == 0xcccccccc:
                        debugged[comp] = expect[comp]

                # Unfortunately we can't ever trust that we should get back a matching results, because some shaders
                # rely on undefined/inaccurate maths that we don't emulate.
                # So the best we can do is log an error for manual verification
                is_eq, diff_amt = rdtest.value_compare_diff(expect, debugged, eps=5.0E-06)
                if not is_eq:
                    rdtest.log.error(
                        "Debugged value {} at EID {} vert {} (idx {}) instance {}: {} difference. {} doesn't exactly match postvs output {}".format(
                            name, draw.eventId, vtx, idx, inst, diff_amt, debugged, expect))

                outputs = outputs + 1

        rdtest.log.success('Successfully debugged vertex in {} cycles, {}/{} outputs match'
                           .format(cycles, outputs, len(refl.outputSignature)))

        self.controller.FreeTrace(trace)

    def pixel_debug(self, draw: rd.DrawcallDescription):
        pipe: rd.PipeState = self.controller.GetPipelineState()

        if pipe.GetShader(rd.ShaderStage.Pixel) == rd.ResourceId.Null():
            rdtest.log.print("No pixel shader bound at {}: {}".format(draw.eventId, draw.name))
            return

        if len(pipe.GetOutputTargets()) == 0 and pipe.GetDepthTarget().resourceId == rd.ResourceId.Null():
            rdtest.log.print("No render targets bound at {}: {}".format(draw.eventId, draw.name))
            return

        if not (draw.flags & rd.DrawFlags.Drawcall):
            rdtest.log.print("{}: {} is not a debuggable drawcall".format(draw.eventId, draw.name))
            return

        viewport = pipe.GetViewport(0)

        # TODO, query for some pixel this drawcall actually touched.
        x = int(random.random()*viewport.width + viewport.x)
        y = int(random.random()*viewport.height + viewport.y)

        target = rd.ResourceId.Null()

        if len(pipe.GetOutputTargets()) > 0:
            valid_targets = [o.resourceId for o in pipe.GetOutputTargets() if o.resourceId != rd.ResourceId.Null()]
            rdtest.log.print("Valid targets at {} are {}".format(draw.eventId, valid_targets))
            if len(valid_targets) > 0:
                target = valid_targets[int(random.random()*len(valid_targets))]

        if target == rd.ResourceId.Null():
            target = pipe.GetDepthTarget().resourceId

        if target == rd.ResourceId.Null():
            rdtest.log.print("No targets bound! Can't fetch history at {}".format(draw.eventId))
            return

        rdtest.log.print("Fetching history for %d,%d on target %s" % (x, y, str(target)))

        history = self.controller.PixelHistory(target, x, y, rd.Subresource(0, 0, 0), rd.CompType.Typeless)

        rdtest.log.success("Pixel %d,%d has %d history events" % (x, y, len(history)))

        lastmod: rd.PixelModification = None

        for i in reversed(range(len(history))):
            mod = history[i]
            draw = self.find_draw('', mod.eventId)

            if draw is None or not (draw.flags & rd.DrawFlags.Drawcall):
                continue

            rdtest.log.print("  hit %d at %d is %s (%s)" % (i, mod.eventId, draw.name, str(draw.flags)))

            lastmod = history[i]

            rdtest.log.print("Got a hit on a drawcall at event %d" % lastmod.eventId)

            if mod.sampleMasked or mod.backfaceCulled or mod.depthClipped or mod.viewClipped or mod.scissorClipped or mod.shaderDiscarded or mod.depthTestFailed or mod.stencilTestFailed:
                rdtest.log.print("This hit failed, looking for one that passed....")
                lastmod = None
                continue

            if not mod.shaderOut.IsValid():
                rdtest.log.print("This hit's shader out is not valid, looking for one that valid....")
                lastmod = None
                continue

            break

        if target == pipe.GetDepthTarget().resourceId:
            rdtest.log.print("Not doing pixel debug for depth output")
            return

        if lastmod is not None:
            rdtest.log.print("Debugging pixel {},{} @ {}, primitive {}".format(x, y, lastmod.eventId, lastmod.primitiveID))
            self.controller.SetFrameEvent(lastmod.eventId, True)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            trace = self.controller.DebugPixel(x, y, 0, lastmod.primitiveID)

            if trace.debugger is None:
                self.controller.FreeTrace(trace)

                rdtest.log.print("No debug result")
                return

            cycles, variables = self.process_trace(trace)

            output_index = [o.resourceId for o in pipe.GetOutputTargets()].index(target)

            if draw.outputs[0] == rd.ResourceId.Null():
                rdtest.log.success('Successfully debugged pixel in {} cycles, skipping result check due to no output'.format(cycles))
                self.controller.FreeTrace(trace)
            elif (draw.flags & rd.DrawFlags.Instanced) and draw.numInstances > 1:
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

                    debuggedValue = [debugged.value.f.x, debugged.value.f.y, debugged.value.f.z, debugged.value.f.w]

                    # For now, ignore debugged values that are uninitialised. This is an application bug but it causes false reports of problems
                    for idx in range(4):
                        if debugged.value.uv[idx] == 0xcccccccc:
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

            self.controller.SetFrameEvent(draw.eventId, True)

    def iter_test(self):
        # Handy tweaks when running locally to disable certain things

        action_chance = 0.1     # Chance of doing anything at all
        do_image_save = 0.25    # Chance of saving images of the outputs
        do_vert_debug = 1.0     # Chance of debugging a vertex (if valid)
        do_pixel_debug = 1.0    # Chance of doing pixel history at the current event and debugging a pixel (if valid)

        self.props: rd.APIProperties = self.controller.GetAPIProperties()

        actions = {
            'Image Save': {'chance': do_image_save, 'func': self.image_save},
            'Vertex Debug': {'chance': do_vert_debug, 'func': self.vert_debug},
            'Pixel History & Debug': {'chance': do_pixel_debug, 'func': self.pixel_debug},
        }

        # To choose an action, if we're going to do one, we take random in range(0, choice_max) then check each action
        # type in turn to see which part of the range we landed in
        choice_max = 0
        for action in actions:
            choice_max += actions[action]['chance']

        draw = self.get_first_draw()
        last_draw = self.get_last_draw()

        while draw:
            rdtest.log.print("{}/{} - {}".format(draw.eventId, last_draw.eventId, draw.name))

            self.controller.SetFrameEvent(draw.eventId, False)

            rdtest.log.print("Set event")

            # If we should take an action at this event
            if random.random() < action_chance:
                c = random.random() * choice_max

                for action in actions:
                    chance = actions[action]['chance']
                    if c < chance:
                        rdtest.log.print("Performing action '{}'".format(action))
                        actions[action]['func'](draw)
                        break
                    else:
                        c -= chance

            draw = draw.next

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
