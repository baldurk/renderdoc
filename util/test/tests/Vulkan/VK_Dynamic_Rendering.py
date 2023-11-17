import copy
import rdtest
import renderdoc as rd


class VK_Dynamic_Rendering(rdtest.TestCase):
    demos_test_name = 'VK_Dynamic_Rendering'

    def check_capture(self):
        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100),
                                                            rd.ReplayOutputType.Texture)

        for cmdLevel in [0,1]:
            action = self.find_action("Draw {}".format(cmdLevel)).next

            self.check(action is not None)

            self.controller.SetFrameEvent(action.eventId, False)

            postgs_data = self.get_postvs(action, rd.MeshDataStage.GSOut, 0, action.numIndices)

            postgs_ref = {
                0: {
                    'vtx': 0,
                    'idx': 0,
                    'gl_Position': [-0.5, 0.5, 0.0, 1.0],
                    'gout.pos': [-0.5, 0.5, 0.0, 1.0],
                    'gout.col': [0.0, 1.0, 0.0, 1.0],
                    'gout.uv': [0.0, 0.0, 0.0, 1.0],
                },
                1: {
                    'vtx': 1,
                    'idx': 1,
                    'gl_Position': [0.0, -0.5, 0.0, 1.0],
                    'gout.pos': [0.0, -0.5, 0.0, 1.0],
                    'gout.col': [0.0, 1.0, 0.0, 1.0],
                    'gout.uv': [0.0, 1.0, 0.0, 1.0],
                },
                2: {
                    'vtx': 2,
                    'idx': 2,
                    'gl_Position': [0.5, 0.5, 0.0, 1.0],
                    'gout.pos': [0.5, 0.5, 0.0, 1.0],
                    'gout.col': [0.0, 1.0, 0.0, 1.0],
                    'gout.uv': [1.0, 0.0, 0.0, 1.0],
                },
            }

            self.check_mesh_data(postgs_ref, postgs_data)

            rdtest.log.success('Mesh data is as expected')

            self.check_triangle()

            rdtest.log.success('Triangle is as expected')

            pipe: rd.VKState = self.controller.GetVulkanPipelineState()

            if len(pipe.graphics.descriptorSets) != 1:
                raise rdtest.TestFailureException("Wrong number of sets is bound: {}, not 1"
                                                  .format(len(pipe.graphics.descriptorSets)))

            desc_set: rd.VKDescriptorSet = pipe.graphics.descriptorSets[0]

            if len(desc_set.bindings) != 1:
                raise rdtest.TestFailureException("Wrong number of bindings: {}, not 1"
                                                  .format(len(desc_set.bindings)))

            binding = desc_set.bindings[0]
            if binding.dynamicallyUsedCount != 1:
                raise rdtest.TestFailureException("Bind doesn't have the right used count. {} is not the expected count of {}"
                                                  .format(binding.dynamicallyUsedCount, 1))

            for idx, el in enumerate(binding.binds):
                expected_used = idx == 17
                actually_used = el.dynamicallyUsed

                if expected_used and not actually_used:
                    raise rdtest.TestFailureException("Bind {} element {} expected to be used, but isn't.".format(bind, idx))

                if not expected_used and actually_used:
                    raise rdtest.TestFailureException("Bind {} element {} expected to be unused, but is.".format(bind, idx))

            rdtest.log.success("Dynamic usage is as expected")

            tex = rd.TextureDisplay()
            tex.overlay = rd.DebugOverlay.Drawcall
            tex.resourceId = pipe.currentPass.framebuffer.attachments[0].imageResourceId

            out.SetTextureDisplay(tex)

            out.Display()

            overlay_id = out.GetDebugOverlayTexID()

            x = int(pipe.viewportScissor.viewportScissors[0].vp.width/2)
            y = int(pipe.viewportScissor.viewportScissors[0].vp.height/2)

            self.check_pixel_value(overlay_id, x, y, [0.8, 0.1, 0.8, 1.0], eps=1.0 / 256.0)
            self.check_pixel_value(overlay_id, x//10, y//10, [0.0, 0.0, 0.0, 0.5], eps=1.0 / 256.0)

        out.Shutdown()
