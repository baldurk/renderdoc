import struct
import math
import renderdoc as rd
import rdtest


class D3D12_Parameter_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Parameter_Zoo'

    def check_capture(self):
        draw = self.find_draw("Color Draw")

        self.check(draw is not None)

        draw = draw.next

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        self.check_pixel_value(pipe.GetOutputTargets()[0].resourceId, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Captured loaded with color as expected")

        postvs_data = self.get_postvs(draw, rd.MeshDataStage.VSOut, 0, draw.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'SV_POSITION': [-0.5, -0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [0.0, 0.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'SV_POSITION': [0.0, 0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'SV_POSITION': [0.5, -0.5, 0.0, 1.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
                'TEXCOORD': [1.0, 0.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        rdtest.log.success("Mesh data is correct")

        tex = rd.TextureDisplay()
        tex.overlay = rd.DebugOverlay.Drawcall
        tex.resourceId = pipe.GetOutputTargets()[0].resourceId

        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100),
                                                            rd.ReplayOutputType.Texture)

        out.SetTextureDisplay(tex)

        out.Display()

        overlay_id = out.GetDebugOverlayTexID()

        v = pipe.GetViewport(0)

        self.check_pixel_value(overlay_id, int(0.5 * v.width), int(0.5 * v.height), [0.8, 0.1, 0.8, 1.0],
                               eps=1.0 / 256.0)

        out.Shutdown()

        rdtest.log.success("Overlay color is as expected")