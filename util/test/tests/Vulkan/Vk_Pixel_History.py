import renderdoc as rd
import rdtest

def value_selector(x): return x.floatValue
def passed(x): return x.Passed()
def event_id(x): return x.eventId
def culled(x): return x.backfaceCulled
def depth_test_failed(x): return x.depthTestFailed
def stencil_test_failed(x): return x.stencilTestFailed
def shader_discarded(x): return x.shaderDiscarded
def shader_out_col(x): return value_selector(x.shaderOut.col)
def post_mod_col(x): return value_selector(x.postMod.col)
def primitive_id(x): return x.primitiveID

class VK_Pixel_History(rdtest.TestCase):
    demos_test_name = 'VK_Pixel_History_Test'
    demos_frame_cap = 5

    def check_capture(self):
        apiprops: rd.APIProperties = self.controller.GetAPIProperties()

        if not apiprops.pixelHistory:
            rdtest.log.print("Vulkan pixel history not tested")
            return

        test_marker: rd.DrawcallDescription = self.find_draw("Test")
        self.controller.SetFrameEvent(test_marker.next.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        rt: rd.BoundResource = pipe.GetOutputTargets()[0]

        vp: rd.Viewport = pipe.GetViewport(0)

        tex = rt.resourceId
        tex_details = self.get_texture(tex)

        sub = rd.Subresource()
        if tex_details.arraysize > 1:
            sub.slice = rt.firstSlice
        if tex_details.mips > 1:
            sub.mip = rt.firstMip

        begin_renderpass_eid = self.find_draw("Begin RenderPass").next.eventId
        depth_write_eid = self.find_draw("Depth Write").next.eventId
        stencil_write_eid = self.find_draw("Stencil Write").next.eventId
        background_eid = self.find_draw("Background").next.eventId
        cull_eid = self.find_draw("Cull Front").next.eventId
        test_eid = self.find_draw("Test").next.eventId

        # For pixel 190, 149 inside the red triangle
        x, y = 190, 149
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, stencil_write_eid], [passed, True]],
            [[event_id, background_eid], [depth_test_failed, True], [post_mod_col, (1.0, 0.0, 0.0, 1.0)]],
            [[event_id, test_eid], [stencil_test_failed, True]],
        ]
        self.check_events(events, modifs)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

        x, y = 190, 150
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, depth_write_eid], [passed, True]],
            [[event_id, background_eid], [depth_test_failed, True]],
            [[event_id, cull_eid], [culled, True]],
            [[event_id, test_eid], [depth_test_failed, True]],
        ]
        self.check_events(events, modifs)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

        x, y = 200, 50
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, background_eid], [passed, True]],
            [[event_id, test_eid], [passed, True], [primitive_id, 7]],
        ]
        self.check_events(events, modifs)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

        x, y = 150, 250
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, background_eid], [shader_discarded, True]],
        ]
        self.check_events(events, modifs)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

    def check_events(self, events, modifs):
        self.check(len(modifs) == len(events))
        # Check for consistency first
        self.check_modifs_consistent(modifs)
        for i in range(len(modifs)):
            for c in range(len(events[i])):
                expected = events[i][c][1]
                actual = events[i][c][0](modifs[i])
                if not rdtest.value_compare(actual, expected):
                    raise rdtest.TestFailureException(
                        "eventId {}, testing {} expected {}, got {}".format(modifs[i].eventId,
                                                                            events[i][c][0].__name__,
                                                                            expected,
                                                                            actual))

    def check_modifs_consistent(self, modifs):
        # postmod of each should match premod of the next
        for i in range(len(modifs) - 1):
            if value_selector(modifs[i].postMod.col) != value_selector(modifs[i + 1].preMod.col):
                raise rdtest.TestFailureException(
                    "postmod at {}: {} doesn't match premod at {}: {}".format(modifs[i].eventId,
                                                                              value_selector(modifs[i].postMod.col),
                                                                              modifs[i + 1].eventId,
                                                                              value_selector(modifs[i].preMod.col)))

        # Check that if the test failed, its postmod is the same as premod
        for i in range(len(modifs)):
            if not modifs[i].Passed():
                if not rdtest.value_compare(value_selector(modifs[i].preMod.col), value_selector(modifs[i].postMod.col)):
                    raise rdtest.TestFailureException(
                        "postmod at {}: {} doesn't match premod: {}".format(modifs[i].eventId,
                                                                            value_selector(modifs[i].postMod.col),
                                                                            value_selector(modifs[i].preMod.col)))
