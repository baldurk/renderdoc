import rdtest
import os
import random
import struct
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
            mesh.baseVertex = draw.baseVertex

            indices = rdtest.fetch_indices(self.controller, mesh, 0, vtx, 1)

            if len(indices) < 1:
                rdtest.log.print("No index buffer, skipping")
                return

            idx = indices[0]

        rdtest.log.print("Debugging vtx %d idx %d (inst %d)" % (vtx, idx, inst))

        trace = self.controller.DebugVertex(vtx, inst, idx, draw.instanceOffset, draw.vertexOffset)

        rdtest.log.success('Successfully debugged vertex in {} cycles'.format(len(trace.states)))

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

        for i in range(len(history)-1, 0, -1):
            mod = history[i]
            draw = self.find_draw('', mod.eventId)

            rdtest.log.print("  hit %d at %d is %s (%s)" % (i, mod.eventId, draw.name, str(draw.flags)))

            if draw is None or not (draw.flags & rd.DrawFlags.Drawcall):
                continue

            lastmod = history[i]

            rdtest.log.print("Got a hit on a drawcall at event %d" % lastmod.eventId)

            if mod.sampleMasked or mod.backfaceCulled or mod.depthClipped or mod.viewClipped or mod.scissorClipped or mod.depthTestFailed or mod.stencilTestFailed:
                rdtest.log.print("This hit failed, looking for one that passed....")
                lastmod = None
                continue

            break

        if lastmod is not None:
            rdtest.log.print("Debugging pixel {},{} @ {}, primitive {}".format(x, y, lastmod.eventId, lastmod.primitiveID))
            self.controller.SetFrameEvent(lastmod.eventId, True)

            trace = self.controller.DebugPixel(x, y, 0, lastmod.primitiveID)

            if draw.outputs[0] == rd.ResourceId.Null():
                rdtest.log.success('Successfully debugged pixel in {} cycles, skipping result check due to no output'.format(len(trace.states)))
            elif draw.numInstances == 1:
                lastState: rd.ShaderDebugState = trace.states[-1]

                output_index = [o.resourceId for o in self.controller.GetPipelineState().GetOutputTargets()].index(target)
                rdtest.log.print("At event {} the target is index {}".format(lastmod.eventId, output_index))

                debugged: rd.ShaderVariable = lastState.outputs[output_index]

                debuggedValue = [debugged.value.f.x, debugged.value.f.y, debugged.value.f.z, debugged.value.f.w]

                if not rdtest.value_compare(lastmod.shaderOut.col.floatValue, [debugged.value.f.x, debugged.value.f.y, debugged.value.f.z, debugged.value.f.w]):
                    raise rdtest.TestFailureException("Debugged value {}: {} doesn't match history shader output {}".format(debugged.name, debuggedValue, lastmod.shaderOut.col.floatValue))

                rdtest.log.success('Successfully debugged pixel in {} cycles, result matches'.format(len(trace.states)))
            else:
                rdtest.log.success('Successfully debugged pixel in {} cycles, skipping result check due to instancing'.format(len(trace.states)))

            self.controller.SetFrameEvent(draw.eventId, True)

    def iter_test(self):
        # Handy tweaks when running locally to disable certain things

        action_chance = 0.1     # Chance of doing anything at all
        do_image_save = 0.25    # Chance of saving images of the outputs
        do_vert_debug = 1.0     # Chance of debugging a vertex (if valid)
        do_pixel_debug = 1.0    # Chance of doing pixel history at the current event and debugging a pixel (if valid)

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

        for file in os.scandir(dir_path):
            if '.rdc' not in file.name:
                continue

            # Ensure we are deterministic at least from run to run by seeding with the path
            random.seed(file.name)

            rdtest.log.print('Iterating {}'.format(file.name))

            try:
                self.controller = rdtest.open_capture(file.path)
            except RuntimeError as err:
                rdtest.log.print("Skipping. Can't open {}: {}".format(file.path, err))
                continue

            self.iter_test()

            self.controller.Shutdown()

            rdtest.log.success("Iterated {}".format(file.name))

        rdtest.log.success("Iterated all files")

    # Useful for calling from within the UI
    def run_external(self, controller: rd.ReplayController):
        self.controller = controller

        self.iter_test()


def run_locally(r):
    test = Iter_Test()
    test.run_external(r)