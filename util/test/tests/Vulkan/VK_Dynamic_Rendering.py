import copy
import rdtest
import renderdoc as rd


class VK_Dynamic_Rendering(rdtest.TestCase):
    demos_test_name = 'VK_Dynamic_Rendering'

    def check_capture(self):
        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100),
                                                            rd.ReplayOutputType.Texture)

        for cmdLevel in [0, 1]:
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

            pipe = self.controller.GetPipelineState()
            vkpipe = self.controller.GetVulkanPipelineState()

            access = pipe.GetDescriptorAccess()

            # only expect two accesses, the buffer we actually use and the push constants
            if len(access) != 2:
                raise rdtest.TestFailureException("Only expected two descriptor accesses, but saw {}".format(
                    len(access)))

            if not (rd.DescriptorType.ReadWriteBuffer, 0, 17) in [(a.type, a.index, a.arrayElement) for a in access]:
                raise rdtest.TestFailureException(
                    f"Graphics bind 0[17] isn't the accessed RW buffer descriptor {str(rd.DumpObject(access))}")

            if len(vkpipe.graphics.descriptorSets) != 1:
                raise rdtest.TestFailureException("Wrong number of sets is bound: {}, not 1".format(
                    len(vkpipe.graphics.descriptorSets)))

            rdtest.log.success("Dynamic usage is as expected")

            tex = rd.TextureDisplay()
            tex.overlay = rd.DebugOverlay.Drawcall
            tex.resourceId = vkpipe.currentPass.framebuffer.attachments[0].resource

            out.SetTextureDisplay(tex)

            out.Display()

            overlay_id = out.GetDebugOverlayTexID()

            x = int(vkpipe.viewportScissor.viewportScissors[0].vp.width / 2)
            y = int(vkpipe.viewportScissor.viewportScissors[0].vp.height / 2)

            self.check_pixel_value(overlay_id, x, y, [0.8, 0.1, 0.8, 1.0], eps=1.0 / 256.0)
            self.check_pixel_value(overlay_id, x // 10, y // 10, [0.0, 0.0, 0.0, 0.5], eps=1.0 / 256.0)

        out.Shutdown()
