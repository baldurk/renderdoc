import renderdoc as rd
import rdtest


class VK_Extended_Dynamic_State(rdtest.TestCase):
    demos_test_name = 'VK_Extended_Dynamic_State'

    def check_capture(self):
        action: rd.ActionDescription = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, True)

        vsin_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'Position': [-0.75, -0.5, 0.4],
                'Color': [0.0, 1.0, 0.0, 1.0],
                'UV': [0.0, 0.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'Position': [-0.25, 0.5, 0.4],
                'Color': [0.0, 1.0, 0.0, 1.0],
                'UV': [0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'Position': [0.25, -0.5, 0.4],
                'Color': [0.0, 1.0, 0.0, 1.0],
                'UV': [1.0, 0.0],
            },
            5: {
                'vtx': 5,
                'idx': 5,
                'Position': [0.75, -0.5, 0.6],
                'Color': [0.0, 0.0, 1.0, 1.0],
                'UV': [1.0, 0.0],
            },
        }

        self.check_mesh_data(vsin_ref, self.get_vsin(action))

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [-0.75, 0.5, 0.4, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_Position': [-0.25, -0.5, 0.4, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'gl_Position': [0.25, 0.5, 0.4, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [1.0, 0.0, 0.0, 1.0],
            },
            5: {
                'vtx': 5,
                'idx': 5,
                'gl_Position': [0.75, 0.5, 0.6, 1.0],
                'vertOut.col': [0.0, 0.0, 1.0, 1.0],
                'vertOut.uv': [1.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 100, 200, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 300, 200, [0.0, 0.0, 1.0, 1.0])
        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 200, 100, [0.2, 0.2, 0.2, 1.0])

        self.check_pixel_value(pipe.GetDepthTarget().resource, 100, 200, [0.4, 205.0/255.0, 0.0, 1.0])
        self.check_pixel_value(pipe.GetDepthTarget().resource, 300, 200, [0.6, 205.0/255.0, 0.0, 1.0])
        self.check_pixel_value(pipe.GetDepthTarget().resource, 200, 100, [0.9, 204.0/255.0, 0.0, 1.0])
        self.check_pixel_value(pipe.GetDepthTarget().resource, 200, 200, [0.4, 206.0/255.0, 0.0, 1.0])

        rdtest.log.success("Triangles are as expected")

        # check that the state listed is the dynamic state, not the static state

        vkpipe: rd.VKState = self.controller.GetVulkanPipelineState()

        self.check(vkpipe.inputAssembly.topology == rd.Topology.TriangleList)

        self.check(vkpipe.depthStencil.depthTestEnable == True)
        self.check(vkpipe.depthStencil.depthWriteEnable == True)
        self.check(vkpipe.depthStencil.depthBoundsEnable == False)
        self.check(vkpipe.depthStencil.depthFunction == rd.CompareFunction.LessEqual)

        self.check(vkpipe.rasterizer.frontCCW == False)
        self.check(vkpipe.rasterizer.cullMode == rd.CullMode.Back)
        self.check(vkpipe.rasterizer.rasterizerDiscardEnable == False)

        self.check(vkpipe.depthStencil.stencilTestEnable == True)
        self.check(vkpipe.depthStencil.frontFace.passOperation == rd.StencilOperation.IncSat)
        self.check(vkpipe.depthStencil.frontFace.failOperation == rd.StencilOperation.IncSat)
        self.check(vkpipe.depthStencil.frontFace.depthFailOperation == rd.StencilOperation.IncSat)
        self.check(vkpipe.depthStencil.frontFace.function == rd.CompareFunction.AlwaysTrue)
        self.check(vkpipe.depthStencil.backFace.passOperation == rd.StencilOperation.Keep)
        self.check(vkpipe.depthStencil.backFace.failOperation == rd.StencilOperation.Keep)
        self.check(vkpipe.depthStencil.backFace.depthFailOperation == rd.StencilOperation.Keep)
        self.check(vkpipe.depthStencil.backFace.function == rd.CompareFunction.AlwaysTrue)

        self.check(len(vkpipe.viewportScissor.viewportScissors) == 1)
        self.check(vkpipe.viewportScissor.viewportScissors[0].vp.width == 400)
        self.check(vkpipe.viewportScissor.viewportScissors[0].scissor.width == 400)

        self.check(vkpipe.vertexInput.vertexBuffers[0].byteStride == 36)
