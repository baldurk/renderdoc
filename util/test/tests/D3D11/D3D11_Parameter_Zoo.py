import renderdoc as rd
import rdtest


class D3D11_Parameter_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Parameter_Zoo'
    demos_frame_cap = 50
    demos_frame_count = 10

    def check_capture(self):
        rdtest.log.success("Got {} captures as expected".format(self.demos_frame_count))

        action = self.find_action("Draw")
        assert action is not None
        self.controller.SetFrameEvent(action.eventId, False)
     
        pipe: rd.PipeState = self.controller.GetPipelineState()

        v = pipe.GetViewport(0)

        stage = rd.ShaderStage.Pixel
        cbuf = pipe.GetConstantBlock(stage, 0, 0).descriptor

        self.check_eq(cbuf.byteSize, 0)

        self.check_triangle()

        self.check_debug_pixel(int(0.5 * v.width), int(0.5 * v.height))
  
        var_check = rdtest.ConstantBufferChecker(
            self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                       pipe.GetShader(stage), stage,
                                                       pipe.GetShaderEntryPoint(stage), 0,
                                                       cbuf.resource, cbuf.byteOffset, cbuf.byteSize))

        var_check.check('cbuf_zero').rows(1).cols(4).value([0.0, 0.0, 0.0, 0.0])
 
        tex = rd.TextureDisplay()
        tex.overlay = rd.DebugOverlay.Drawcall
        tex.resourceId = pipe.GetOutputTargets()[0].resource

        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100),
                                                            rd.ReplayOutputType.Texture)

        out.SetTextureDisplay(tex)

        out.Display()

        overlay_id = out.GetDebugOverlayTexID()

        self.check_pixel_value(overlay_id, int(0.5 * v.width), int(0.5 * v.height), [0.8, 0.1, 0.8, 1.0],
                               eps=1.0 / 256.0)

        expected_markers = [
            "Features1: D3D11_TILED_RESOURCES_NOT_SUPPORTED",
            "Features2: D3D11_TILED_RESOURCES_NOT_SUPPORTED",
            "CreateTiledBuffer: Failed",
            "CreateTile_PoolBuffer: Failed",
            "CreateTiledTexture2D: Failed",
            "CreateTiledTexture2D1: Failed",
        ]
        for marker in expected_markers:
            if self.find_action(marker) == None:
                raise rdtest.TestFailureException("Failed to find marker `{}`".format(marker))

        out.Shutdown()

        action = self.find_action("RastState")
        assert action is not None
        self.controller.SetFrameEvent(action.eventId, False)

        pipe11 = self.controller.GetD3D11PipelineState()

        assert pipe11.rasterizer.state.resourceId != rd.ResourceId()

        assert self.get_resource(pipe11.rasterizer.state.resourceId).name == "RastState"

        rdtest.log.success("Overlay color is as expected")
