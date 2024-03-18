import renderdoc as rd
import rdtest


class GL_Separable_Geometry_Shaders(rdtest.TestCase):
    demos_test_name = 'GL_Separable_Geometry_Shaders'

    def check_capture(self):
        action = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, False)

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [-0.5, -0.5, 0.0, 1.0],
                'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                'v2f_block.uv': [0.0, 0.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_Position': [0.0, 0.5, 0.0, 1.0],
                'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                'v2f_block.uv': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'gl_Position': [0.5, -0.5, 0.0, 1.0],
                'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                'v2f_block.uv': [1.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        postgs_data = self.get_postvs(action, rd.MeshDataStage.GSOut, 0, action.numIndices*3)

        postgs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [0.2, -0.5, 0.0, 1.0],
                'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                'v2f_block.uv': [0.0, 0.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_Position': [0.7, 0.5, 0.0, 1.0],
                'v2f_block.col': [0.0, 1.0, 0.0, 1.0],
                'v2f_block.uv': [0.0, 1.0, 0.0, 1.0],
            },
            4: {
                'vtx': 4,
                'idx': 4,
                'gl_Position': [-0.7, 0.5, 0.0, 1.0],
                'v2f_block.col': [1.0, 0.0, 1.0, 0.0],
                'v2f_block.uv': [0.0, 1.0, 0.0, 1.0],
            },
            5: {
                'vtx': 5,
                'idx': 5,
                'gl_Position': [-0.2, -0.5, 0.0, 1.0],
                'v2f_block.col': [1.0, 0.0, 1.0, 0.0],
                'v2f_block.uv': [1.0, 0.0, 0.0, 1.0],
            },
            8: {
                'vtx': 8,
                'idx': 8,
                'gl_Position': [0.5, 0.2, 0.0, 1.0],
                'v2f_block.col': [1.0, 0.0, 0.0, 1.0],
                'v2f_block.uv': [1.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postgs_ref, postgs_data)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        rt = pipe.GetOutputTargets()[0].resource
        self.check_pixel_value(rt, 0.5, 0.1, [1.0, 0.0, 0.0, 1.0])
        self.check_pixel_value(rt, 0.75, 0.5, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(rt, 0.25, 0.5, [1.0, 0.0, 1.0, 0.0])

        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        tex = rd.TextureDisplay()
        tex.resourceId = rt
        tex.overlay = rd.DebugOverlay.Drawcall
        out.SetTextureDisplay(tex)
        out.Display()

        eps = 1.0 / 256.0

        overlay_id: rd.ResourceId = out.GetDebugOverlayTexID()

        self.check_pixel_value(overlay_id, 200, 100, [0.8, 0.1, 0.8, 1.0], eps=eps)
        self.check_pixel_value(overlay_id, 50, 150, [0.8, 0.1, 0.8, 1.0], eps=eps)
        self.check_pixel_value(overlay_id, 350, 150, [0.8, 0.1, 0.8, 1.0], eps=eps)

        self.check_pixel_value(overlay_id, 200, 150, [0.0, 0.0, 0.0, 0.5], eps=eps)
        self.check_pixel_value(overlay_id, 200, 225, [0.0, 0.0, 0.0, 0.5], eps=eps)
        self.check_pixel_value(overlay_id, 75, 50, [0.0, 0.0, 0.0, 0.5], eps=eps)
        self.check_pixel_value(overlay_id, 350, 50, [0.0, 0.0, 0.0, 0.5], eps=eps)

        out.Shutdown()
