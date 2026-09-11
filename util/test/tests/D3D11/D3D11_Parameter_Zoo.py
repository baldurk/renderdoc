import renderdoc as rd
import rdtest


class D3D11_Parameter_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D11_Parameter_Zoo'
    demos_frame_cap = 50
    demos_frame_count = 10

    def check_capture(self):
        rdtest.log.success(f"Got {self.demos_frame_count} captures as expected")

        action = self.find_action("Draw")
        assert action is not None
        self.set_event(action.eventId, False)

        pipe = self.controller.GetPipelineState()

        stage = rd.ShaderStage.Pixel
        cbuf = pipe.GetConstantBlock(stage, 0, 0).descriptor

        self.check_eq(cbuf.byteSize, 0)

        self.check_triangle()

        self.check_debug_pixel()

        var_check = rdtest.ConstantBufferChecker(
            self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                       pipe.GetShader(stage), stage,
                                                       pipe.GetShaderEntryPoint(stage), 0,
                                                       cbuf.resource, cbuf.byteOffset, cbuf.byteSize))

        var_check.check('cbuf_zero').rows(1).cols(4).value([0.0, 0.0, 0.0, 0.0])

        tex = rd.TextureDisplay()
        tex.overlay = rd.DebugOverlay.Drawcall
        tex.resourceId = pipe.GetOutputTargets()[0].resource

        out = self.controller.CreateOutput(
            rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture
        )

        out.SetTextureDisplay(tex)

        out.Display()

        overlay_id = out.GetDebugOverlayTexID()

        x, y = self.get_view_centre()

        self.check_pixel_value(overlay_id, x, y, [0.8, 0.1, 0.8, 1.0], eps=1.0 / 256.0)

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
                raise rdtest.TestFailureException(f"Failed to find marker `{marker}`")

        out.Shutdown()

        action = self.find_action("RastState")
        assert action is not None
        self.set_event(action.eventId, False)

        pipe11 = self.controller.GetD3D11PipelineState()

        assert pipe11.rasterizer.state.resourceId != rd.ResourceId()

        assert self.get_resource(pipe11.rasterizer.state.resourceId).name == "RastState"

        rdtest.log.success("Overlay color is as expected")
