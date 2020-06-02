import renderdoc as rd
import rdtest
from typing import List

def value_selector(x): return x.floatValue
def passed(x): return x.Passed()
def event_id(x): return x.eventId
def culled(x): return x.backfaceCulled
def depth_test_failed(x): return x.depthTestFailed
def scissor_clipped(x): return x.scissorClipped
def stencil_test_failed(x): return x.stencilTestFailed
def shader_discarded(x): return x.shaderDiscarded
def shader_out_col(x): return value_selector(x.shaderOut.col)
def shader_out_depth(x): return x.shaderOut.depth
def pre_mod_col(x): return value_selector(x.preMod.col)
def post_mod_col(x): return value_selector(x.postMod.col)
def shader_out_depth(x): return x.shaderOut.depth
def pre_mod_depth(x): return x.preMod.depth
def post_mod_depth(x): return x.postMod.depth
def primitive_id(x): return x.primitiveID
def unboundPS(x): return x.unboundPS

class VK_Pixel_History(rdtest.TestCase):
    demos_test_name = 'VK_Pixel_History_Test'
    demos_frame_cap = 5

    def check_capture(self):
        apiprops: rd.APIProperties = self.controller.GetAPIProperties()

        if not apiprops.pixelHistory:
            rdtest.log.print("Vulkan pixel history not tested")
            return

        self.primary_test()
        self.multisampled_image_test()
        self.secondary_cmd_test()
        self.depth_target_test()

    def primary_test(self):
        test_marker: rd.DrawcallDescription = self.find_draw("Test")
        self.controller.SetFrameEvent(test_marker.next.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        rt: rd.BoundResource = pipe.GetOutputTargets()[0]

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
        unbound_fs_eid = self.find_draw("Unbound Fragment Shader").next.eventId
        background_eid = self.find_draw("Background").next.eventId
        cull_eid = self.find_draw("Cull Front").next.eventId
        test_eid = self.find_draw("Test").next.eventId
        fixed_scissor_fail_eid = self.find_draw("Fixed Scissor Fail").next.eventId
        fixed_scissor_pass_eid = self.find_draw("Fixed Scissor Pass").next.eventId
        dynamic_stencil_ref_eid = self.find_draw("Dynamic Stencil Ref").next.eventId
        dynamic_stencil_mask_eid = self.find_draw("Dynamic Stencil Mask").next.eventId
        depth_test_eid = self.find_draw("Depth Test").next.eventId

        # For pixel 190, 149 inside the red triangle
        x, y = 190, 149
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, unbound_fs_eid], [passed, True], [unboundPS, True], [primitive_id, 0]],
            [[event_id, stencil_write_eid], [passed, True]],
            [[event_id, background_eid], [depth_test_failed, True], [post_mod_col, (1.0, 0.0, 0.0, 1.0)]],
            [[event_id, test_eid], [stencil_test_failed, True]],
        ]
        self.check_events(events, modifs, False)
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
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

        x, y = 200, 50
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, background_eid], [passed, True]],
            [[event_id, test_eid], [passed, True], [primitive_id, 7]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

        x, y = 150, 250
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, background_eid], [shader_discarded, True]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

        rdtest.log.print("Testing dynamic state pipelines")
        self.controller.SetFrameEvent(dynamic_stencil_mask_eid, True)

        x, y = 100, 250
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, background_eid], [passed, True]],
            [[event_id, fixed_scissor_fail_eid], [scissor_clipped, True]],
            [[event_id, fixed_scissor_pass_eid], [passed, True], [shader_out_col, (0.0, 1.0, 0.0, 1.0)]],
            [[event_id, dynamic_stencil_ref_eid], [passed, True], [shader_out_col, (0.0, 0.0, 1.0, 1.0)]],
            [[event_id, dynamic_stencil_mask_eid], [passed, True], [shader_out_col, (0.0, 1.0, 1.0, 1.0)]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

        rdtest.log.print("Testing depth test for per fragment reporting")
        self.controller.SetFrameEvent(depth_test_eid, True)

        x, y = 275, 260
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, background_eid], [passed, True]],
            [[event_id, depth_test_eid], [primitive_id, 0], [shader_out_col, (1.0, 1.0, 1.0, 1.0)],
                [shader_out_depth, 0.97], [post_mod_col, (1.0, 0.0, 1.0, 1.0)], [post_mod_depth, 0.95]],
            [[event_id, depth_test_eid], [primitive_id, 1], [shader_out_col, (1.0, 1.0, 0.0, 1.0)],
                [shader_out_depth, 0.20], [post_mod_col, (1.0, 1.0, 0.0, 1.0)], [post_mod_depth, 0.20]],
            [[event_id, depth_test_eid], [primitive_id, 2], [shader_out_col, (1.0, 0.0, 0.0, 1.0)],
                [shader_out_depth, 0.30], [post_mod_col, (1.0, 1.0, 0.0, 1.0)], [post_mod_depth, 0.20]],
            [[event_id, depth_test_eid], [primitive_id, 3], [shader_out_col, (0.0, 0.0, 1.0, 1.0)],
                [shader_out_depth, 0.10], [post_mod_col, (0.0, 0.0, 1.0, 1.0)], [post_mod_depth, 0.10]],
            [[event_id, depth_test_eid], [primitive_id, 4], [shader_out_col, (1.0, 1.0, 1.0, 1.0)],
                [shader_out_depth, 0.05], [post_mod_col, (0.0, 0.0, 1.0, 1.0)], [post_mod_depth, 0.10]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

    def multisampled_image_test(self):
        test_marker: rd.DrawcallDescription = self.find_draw("Multisampled: test")
        draw_eid = test_marker.next.eventId
        self.controller.SetFrameEvent(draw_eid, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()
        rt: rd.BoundResource = pipe.GetOutputTargets()[0]
        sub = rd.Subresource()
        tex = rt.resourceId
        tex_details = self.get_texture(tex)
        if tex_details.arraysize > 1:
            sub.slice = rt.firstSlice

        beg_renderpass_eid = self.find_draw("Multisampled: begin renderpass").next.eventId

        x, y = 140, 130
        sub.sample = 1
        rdtest.log.print("Testing pixel {}, {} at sample {}".format(x, y, sub.sample))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, beg_renderpass_eid], [passed, True], [post_mod_col, (0.0, 1.0, 0.0, 1.0)], [post_mod_depth, 0.0]],
            [[event_id, draw_eid], [passed, True], [primitive_id, 0],
                [shader_out_col, (1.0, 0.0, 1.0, 1.0)], [post_mod_col, (1.0, 0.0, 1.0, 1.0)],
                [pre_mod_depth, 0.0], [shader_out_depth, 0.9], [post_mod_depth, 0.9]],
            [[event_id, draw_eid], [passed, True], [primitive_id, 1],
                [shader_out_col, (0.0, 0.0, 1.0, 1.0)], [post_mod_col, (0.0, 0.0, 1.0, 1.0)],
                [shader_out_depth, 0.95], [post_mod_depth, 0.95]],
        ]
        self.check_events(events, modifs, True)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

        sub.sample = 2
        rdtest.log.print("Testing pixel {}, {} at sample {}".format(x, y, sub.sample))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, beg_renderpass_eid], [passed, True], [post_mod_col, (0.0, 1.0, 0.0, 1.0)]],
            [[event_id, draw_eid], [passed, True], [primitive_id, 0],
                [shader_out_col, (1.0, 0.0, 1.0, 1.0)], [post_mod_col, (1.0, 0.0, 1.0, 1.0)]],
            [[event_id, draw_eid], [passed, True], [primitive_id, 1],
                [shader_out_col, (0.0, 1.0, 1.0, 1.0)], [post_mod_col, (0.0, 1.0, 1.0, 1.0)]],
        ]
        self.check_events(events, modifs, True)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

    def secondary_cmd_test(self):
        secondary_marker: rd.DrawcallDescription = self.find_draw("Secondary: red and blue")
        self.controller.SetFrameEvent(secondary_marker.next.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()
        rt: rd.BoundResource = pipe.GetOutputTargets()[0]
        sub = rd.Subresource()
        tex = rt.resourceId
        tex_details = self.get_texture(tex)
        if tex_details.arraysize > 1:
            sub.slice = rt.firstSlice
        if tex_details.mips > 1:
            sub.mip = rt.firstMip

        sec_beg_renderpass_eid = self.find_draw("Begin RenderPass Secondary").next.eventId
        background_eid = self.find_draw("Secondary: background").next.eventId
        culled_eid = self.find_draw("Secondary: culled").next.eventId
        sec_red_and_blue = self.find_draw("Secondary: red and blue").next.eventId

        # Test culling
        x, y = 70, 40
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, sec_beg_renderpass_eid], [passed, True], [post_mod_col, (0.0, 1.0, 0.0, 1.0)]],
            [[event_id, background_eid], [passed, True], [pre_mod_col, (0.0, 1.0, 0.0, 1.0)]],
            [[event_id, culled_eid], [passed, False], [culled, True]],
        ]
        self.check_events(events, modifs, True)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

        # Blue triangle
        x, y = 40, 40
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, sec_beg_renderpass_eid], [passed, True], [post_mod_col, (0.0, 1.0, 0.0, 1.0)]],
            # This is the first event in the command buffer, should have pre-mod
            [[event_id, background_eid], [passed, True], [pre_mod_col, (0.0, 1.0, 0.0, 1.0)]],
            # This is the last event in the command buffer, should have post-mod
            [[event_id, sec_red_and_blue], [passed, True], [post_mod_col, (0.0, 0.0, 1.0, 1.0)]],
        ]
        self.check_events(events, modifs, True)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

        # Didn't get post mod for background_eid
        self.controller.SetFrameEvent(background_eid, True)
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, sec_beg_renderpass_eid]],
            # The only event, should have both pre and post mod.
            [[event_id, background_eid], [passed, True], [pre_mod_col, (0.0, 1.0, 0.0, 1.0)], [post_mod_col, (1.0, 0.0, 1.0, 1.0)]],
        ]
        self.check_events(events, modifs, True)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.typeCast)

    def depth_target_test(self):
        test_marker: rd.DrawcallDescription = self.find_draw("Test")
        self.controller.SetFrameEvent(test_marker.next.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        rt: rd.BoundResource = pipe.GetDepthTarget()

        tex = rt.resourceId
        tex_details = self.get_texture(tex)

        sub = rd.Subresource()
        if tex_details.arraysize > 1:
            sub.slice = rt.firstSlice
        if tex_details.mips > 1:
            sub.mip = rt.firstMip

        begin_renderpass_eid = self.find_draw("Begin RenderPass").next.eventId
        background_eid = self.find_draw("Background").next.eventId
        test_eid = self.find_draw("Test").next.eventId

        x, y = 200, 190
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.typeCast)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True], [post_mod_depth, 1.0]],
            [[event_id, background_eid], [passed, True], [primitive_id, 0], [pre_mod_depth, 1.0], [post_mod_depth, 0.95]],
            [[event_id, test_eid], [passed, True], [primitive_id, 0], [shader_out_depth, 0.5], [post_mod_depth, 0.5]],
            [[event_id, test_eid], [passed, True], [primitive_id, 1], [shader_out_depth, 0.6], [post_mod_depth, 0.5]],
        ]
        self.check_events(events, modifs, False)


    def check_events(self, events, modifs, hasSecondary):
        self.check(len(modifs) == len(events), "Expected {} events, got {}".format(len(events), len(modifs)))
        # Check for consistency first. For secondary command buffers,
        # might not have all information, so don't check for consistency
        if not hasSecondary:
            self.check_modifs_consistent(modifs)
        for i in range(len(modifs)):
            for c in range(len(events[i])):
                expected = events[i][c][1]
                actual = events[i][c][0](modifs[i])
                if not rdtest.value_compare(actual, expected):
                    raise rdtest.TestFailureException(
                        "eventId {}, primitiveID {}: testing {} expected {}, got {}".format(modifs[i].eventId, modifs[i].primitiveID,
                                                                            events[i][c][0].__name__,
                                                                            expected,
                                                                            actual))

    def check_modifs_consistent(self, modifs):
        # postmod of each should match premod of the next
        for i in range(len(modifs) - 1):
            if value_selector(modifs[i].postMod.col) != value_selector(modifs[i + 1].preMod.col):
                raise rdtest.TestFailureException(
                    "postmod at {} primitive {}: {} doesn't match premod at {} primitive {}: {}".format(modifs[i].eventId,
                                                                              modifs[i].primitiveID,
                                                                              value_selector(modifs[i].postMod.col),
                                                                              modifs[i + 1].eventId,
                                                                              modifs[i + 1].primitiveID,
                                                                              value_selector(modifs[i].preMod.col)))

        # Check that if the test failed, its postmod is the same as premod
        for i in range(len(modifs)):
            if not modifs[i].Passed():
                if not rdtest.value_compare(value_selector(modifs[i].preMod.col), value_selector(modifs[i].postMod.col)):
                    raise rdtest.TestFailureException(
                        "postmod at {} primitive {}: {} doesn't match premod: {}".format(modifs[i].eventId,
                                                                            modifs[i].primitiveID,
                                                                            value_selector(modifs[i].postMod.col),
                                                                            value_selector(modifs[i].preMod.col)))
