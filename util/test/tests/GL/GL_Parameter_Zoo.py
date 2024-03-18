import struct
import math
import renderdoc as rd
import rdtest


class GL_Parameter_Zoo(rdtest.TestCase):
    demos_test_name = 'GL_Parameter_Zoo'
    demos_frame_cap = 6

    def check_capture(self):
        id = self.get_last_action().copyDestination

        tex_details = self.get_texture(id)

        self.controller.SetFrameEvent(self.get_last_action().eventId, True)

        data = self.controller.GetTextureData(id, rd.Subresource(0, 0, 0))
        first_pixel = struct.unpack_from("BBBB", data, 0)

        val = [255, 0, 255, 255]
        if not rdtest.value_compare(first_pixel, val):
            raise rdtest.TestFailureException("First pixel should be clear color {}, not {}".format(val, first_pixel))

        magic_pixel = struct.unpack_from("BBBB", data, (50 * tex_details.width + 320) * 4)

        # allow 127 or 128 for alpha
        val = [0, 0, 255, magic_pixel[3]]
        if not rdtest.value_compare(magic_pixel, val) or magic_pixel[3] not in [127, 128]:
            raise rdtest.TestFailureException("Pixel @ 320,50 should be blue: {}, not {}".format(val, magic_pixel))

        rdtest.log.success("Decoded pixels from texture data are correct")

        img_path = rdtest.get_tmp_path('preserved_alpha.png')

        self.controller.SetFrameEvent(self.get_last_action().eventId, True)

        save_data = rd.TextureSave()
        save_data.resourceId = id
        save_data.destType = rd.FileType.PNG
        save_data.alpha = rd.AlphaMapping.Discard # this should not discard the alpha

        self.controller.SaveTexture(save_data, img_path)

        data = rdtest.png_load_data(img_path)

        magic_pixel = struct.unpack_from("BBBB", data[-1-50], 320 * 4)

        val = [0, 0, 255, magic_pixel[3]]
        if not rdtest.value_compare(magic_pixel, val) or magic_pixel[3] not in [127, 128]:
            raise rdtest.TestFailureException("Pixel @ 320,50 should be blue: {}, not {}".format(val, magic_pixel))

        action = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, False)

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [-0.5, -0.5, 0.0, 1.0],
                'v2fcol': [0.0, 1.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_Position': [0.0, 0.5, 0.0, 1.0],
                'v2fcol': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'gl_Position': [0.5, -0.5, 0.0, 1.0],
                'v2fcol': [0.0, 1.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        results = self.controller.FetchCounters([rd.GPUCounter.RasterizedPrimitives, rd.GPUCounter.VSInvocations, rd.GPUCounter.FSInvocations])

        results = [r for r in results if r.eventId == action.eventId]

        if len(results) != 3:
            raise rdtest.TestFailureException("Expected 3 results, got {} results".format(len(results)))
        
        for r in results:
            r: rd.CounterResult
            val = r.value.u32
            if r.counter == rd.GPUCounter.RasterizedPrimitives:
                if not rdtest.value_compare(val, 1):
                    raise rdtest.TestFailureException("RasterizedPrimitives result {} is not as expected".format(val))
                else:
                    rdtest.log.success("RasterizedPrimitives result is as expected")
            elif r.counter == rd.GPUCounter.VSInvocations:
                if not rdtest.value_compare(val, 3):
                    raise rdtest.TestFailureException("VSInvocations result {} is not as expected".format(val))
                else:
                    rdtest.log.success("VSInvocations result is as expected")
            elif r.counter == rd.GPUCounter.FSInvocations:
                if val < int(0.1 * tex_details.width * tex_details.height):
                    raise rdtest.TestFailureException("FSInvocations result {} is not as expected".format(val))
                else:
                    rdtest.log.success("FSInvocations result is as expected")
            else:
                raise rdtest.TestFailureException("Unexpected counter result {}".format(r.counter))

        rdtest.log.success("Counter data retrieved successfully")

        action = self.find_action("NoScissor")

        self.check(action is not None)
        action = action.next
        pipe: rd.PipeState = self.controller.GetPipelineState()

        tex = rd.TextureDisplay()
        tex.overlay = rd.DebugOverlay.Drawcall
        tex.resourceId = pipe.GetOutputTargets()[0].resource

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

