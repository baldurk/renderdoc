import renderdoc as rd
import rdtest
from typing import List


def value_selector(x): return x.floatValue
def passed(x): return x.Passed()
def event_id(x): return x.eventId
def culled(x): return x.backfaceCulled
def depth_test_failed(x): return x.depthTestFailed
def depth_clipped(x): return x.depthClipped
def depth_bounds_failed(x): return x.depthBoundsFailed
def scissor_clipped(x): return x.scissorClipped
def stencil_test_failed(x): return x.stencilTestFailed
def shader_discarded(x): return x.shaderDiscarded

def get_shader_out_color(x): return value_selector(x.shaderOut.col)
def get_shader_out_depth(x): return x.shaderOut.depth

def get_pre_mod_color(x): return value_selector(x.preMod.col)
def get_pre_mod_depth(x): return x.preMod.depth

def get_post_mod_color(x): return value_selector(x.postMod.col)
def get_post_mod_depth(x): return x.postMod.depth
def get_post_mod_stencil(x): return x.postMod.stencil
def unknown_post_mod_stencil(x): return x.postMod.stencil == -1 or x.postMod.stencil == -2

def unknown_stencil(x): return x == -1 or x == -2
def primitive_id(x): return x.primitiveID
def unboundPS(x): return x.unboundPS

class D3D12_Pixel_History(rdtest.TestCase):
    demos_test_name = 'D3D12_Pixel_History'
    demos_frame_cap = 5

    def check_capture(self):
        apiprops: rd.APIProperties = self.controller.GetAPIProperties()

        if not apiprops.pixelHistory:
            rdtest.log.print("D3D12 pixel history not tested")
            return

        # Disabling SM6.6 test as the test setup is not doing anything special for it yet
        tests = ["SM5.1", "SM6.0"] #, "SM6.6"]
        for t in tests:
            rdtest.log.print("Testing " + t)

            self.is_depth = False
            rdtest.log.print("Testing primary")
            self.primary_test("Begin " + t)
            rdtest.log.print("Testing secondary")
            self.secondary_cmd_test("Begin " + t)

            self.is_depth = True
            rdtest.log.print("Testing depth")
            self.depth_target_test("Begin " + t)

            self.is_depth = False
            rdtest.log.print("Testing MSAA")
            self.multisampled_image_test("Begin " + t)

    def primary_test(self, begin_name: str):
        begin_renderpass_action = self.find_action(begin_name)
        if begin_renderpass_action is None:
            rdtest.log.print("Test not found")
            return

        begin_renderpass_eid = begin_renderpass_action.next.eventId
        test_marker: rd.ActionDescription = self.find_action("Test Begin", begin_renderpass_eid)
        self.controller.SetFrameEvent(test_marker.next.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        rt = pipe.GetOutputTargets()[0]

        tex = rt.resource
        tex_details = self.get_texture(tex)

        sub = rd.Subresource()
        if tex_details.arraysize > 1:
            sub.slice = rt.firstSlice
        if tex_details.mips > 1:
            sub.mip = rt.firstMip

        depth_write_eid = self.find_action("Depth Write", begin_renderpass_eid).next.eventId
        stencil_write_eid = self.find_action("Stencil Write", begin_renderpass_eid).next.eventId
        unbound_fs_eid = self.find_action("Unbound Fragment Shader", begin_renderpass_eid).next.eventId
        background_eid = self.find_action("Background", begin_renderpass_eid).next.eventId
        cull_eid = self.find_action("Cull Front", begin_renderpass_eid).next.eventId
        test_eid = self.find_action("Test Begin", begin_renderpass_eid).next.eventId
        fixed_scissor_fail_eid = self.find_action("Fixed Scissor Fail", begin_renderpass_eid).next.eventId
        fixed_scissor_pass_eid = self.find_action("Fixed Scissor Pass", begin_renderpass_eid).next.eventId
        dynamic_stencil_ref_eid = self.find_action("Dynamic Stencil Ref", begin_renderpass_eid).next.eventId
        dynamic_stencil_mask_eid = self.find_action("Dynamic Stencil Mask", begin_renderpass_eid).next.eventId
        depth_test_eid = self.find_action("Depth Test", begin_renderpass_eid).next.eventId
        depth_16bit_test_eid = self.find_action("Depth 16-bit Test", begin_renderpass_eid).next.eventId
        depth_bounds_prep_eid = self.find_action("Depth Bounds Prep", begin_renderpass_eid).next.eventId
        depth_bounds_clip_eid = self.find_action("Depth Bounds Clip", begin_renderpass_eid).next.eventId
        one_thousand_instances_action = self.find_action("1000 Instances", begin_renderpass_eid)

        # For pixel 110, 100, inside the red triangle with stencil value 0x55
        x, y = 110, 100
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, unbound_fs_eid], [passed, True], [unboundPS, True], [primitive_id, 0], [get_post_mod_stencil, 0x33]],
            [[event_id, stencil_write_eid], [passed, True], [primitive_id, 0], [get_post_mod_stencil, 0x55]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, value_selector(modifs[-1].postMod.col), sub=sub, cast=rt.format.compType)

        # For pixel 190, 149 inside the red triangle
        x, y = 190, 149
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, unbound_fs_eid], [passed, True], [unboundPS, True], [primitive_id, 0]],
            [[event_id, stencil_write_eid], [passed, True]],
            [[event_id, background_eid], [depth_test_failed, True], [get_post_mod_color, (1.0, 0.0, 0.0, 1.0)]],
            [[event_id, test_eid], [stencil_test_failed, True]],
        ]

        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        x, y = 190, 150
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, depth_write_eid], [passed, True]],
            [[event_id, background_eid], [depth_test_failed, True]],
            [[event_id, cull_eid], [culled, True]],
            [[event_id, test_eid], [depth_test_failed, True]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        x, y = 200, 50
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, background_eid], [passed, True]],
            [[event_id, test_eid], [passed, True], [primitive_id, 7]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        x, y = 150, 250
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, background_eid], [shader_discarded, True]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        x, y = 330, 145
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, test_eid], [passed, True], [primitive_id, 3], [get_shader_out_color, (0.0, 0.0, 0.0, 2.75)]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        x, y = 340, 145
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, test_eid], [passed, False], [depth_clipped, True]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        x, y = 330, 105
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, depth_bounds_prep_eid], [passed, True], [primitive_id, 0], [get_shader_out_color, (1.0, 0.0, 0.0, 2.75)]],
            [[event_id, depth_bounds_clip_eid], [passed, True], [primitive_id, 0], [get_shader_out_color, (0.0, 1.0, 0.0, 2.75)]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        x, y = 320, 105
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, depth_bounds_prep_eid], [passed, True], [primitive_id, 0], [get_shader_out_color, (1.0, 0.0, 0.0, 2.75)]],
            [[event_id, depth_bounds_clip_eid], [passed, False], [depth_bounds_failed, True]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        x, y = 345, 105
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, depth_bounds_prep_eid], [passed, True], [primitive_id, 0], [get_shader_out_color, (1.0, 0.0, 0.0, 2.75)]],
            [[event_id, depth_bounds_clip_eid], [passed, False], [depth_bounds_failed, True]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        rdtest.log.print("Testing dynamic state pipelines")
        self.controller.SetFrameEvent(dynamic_stencil_mask_eid, True)

        x, y = 100, 250
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, background_eid], [passed, True]],
            [[event_id, fixed_scissor_fail_eid], [scissor_clipped, True]],
            [[event_id, fixed_scissor_pass_eid], [passed, True], [get_shader_out_color, (0.0, 1.0, 0.0, 2.75)],
             [get_post_mod_color, (0.0, 1.0, 0.0, 1.0)]],
            [[event_id, dynamic_stencil_ref_eid], [passed, True], [get_shader_out_color, (0.0, 0.0, 1.0, 2.75)],
             [get_post_mod_color, (0.0, 0.0, 1.0, 1.0)]],
            [[event_id, dynamic_stencil_mask_eid], [passed, True], [get_shader_out_color, (0.0, 1.0, 1.0, 2.75)],
             [get_post_mod_color, (0.0, 1.0, 1.0, 1.0)]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        rdtest.log.print("Testing depth test for per fragment reporting")
        self.controller.SetFrameEvent(depth_test_eid, True)

        x, y = 275, 260
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, background_eid], [passed, True]],
            [[event_id, depth_test_eid], [primitive_id, 0], [depth_test_failed, True],
             [get_shader_out_color, (1.0, 1.0, 1.0, 2.75)], [get_shader_out_depth, 0.97], [get_post_mod_color, (1.0, 0.0, 1.0, 1.0)],
             [get_post_mod_depth, 0.95], [unknown_post_mod_stencil, True]],
            [[event_id, depth_test_eid], [primitive_id, 1], [depth_test_failed, False],
             [get_shader_out_color, (1.0, 1.0, 0.0, 2.75)], [get_shader_out_depth, 0.20], [get_post_mod_color, (1.0, 1.0, 0.0, 1.0)],
             [get_post_mod_depth, 0.20]],
            [[event_id, depth_test_eid], [primitive_id, 2], [depth_test_failed, True],
             [get_shader_out_color, (1.0, 0.0, 0.0, 2.75)], [get_shader_out_depth, 0.30], [get_post_mod_color, (1.0, 1.0, 0.0, 1.0)],
             [get_post_mod_depth, 0.20]],
            [[event_id, depth_test_eid], [primitive_id, 3], [depth_test_failed, False],
             [get_shader_out_color, (0.0, 0.0, 1.0, 2.75)], [get_shader_out_depth, 0.10], [get_post_mod_color, (0.0, 0.0, 1.0, 1.0)],
             [get_post_mod_depth, 0.10]],
            [[event_id, depth_test_eid], [primitive_id, 4], [depth_test_failed, False], [depth_bounds_failed, True],
             [get_shader_out_color, (1.0, 1.0, 1.0, 2.75)], [get_shader_out_depth, 0.05], [get_post_mod_color, (0.0, 0.0, 1.0, 1.0)],
             [get_post_mod_depth, 0.10]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        # For pixel 70, 100 inside the yellow triangle rendered with 16-bit depth
        rdtest.log.print("Testing 16-bit depth format")
        self.controller.SetFrameEvent(depth_16bit_test_eid, True)
        x, y = 70, 100
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, depth_16bit_test_eid], [primitive_id, 0], [depth_test_failed, False],
             [get_shader_out_color, (1.0, 1.0, 0.0, 2.75)], [get_shader_out_depth, 0.33], [get_post_mod_color, (1.0, 1.0, 0.0, 1.0)],
             [get_post_mod_depth, 0.33]],
        ]
        self.check_events(events, modifs, False)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        # For pixel 60, 130 inside the light green triangle which is 1000 draws of 1 instance of 1 triangle 
        rdtest.log.print("Testing Lots of Drawcalls")
        self.controller.SetFrameEvent(one_thousand_instances_action.eventId, True)
        x, y = 60, 130
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)
        countEvents = 1 + 1000
        self.check(len(modifs) == countEvents, "Expected {} events, got {}".format(countEvents, len(modifs)))
        self.check_modifs_consistent(modifs)

        # For pixel 60, 50 inside the orange triangle which is 1 draws of 1000 instances of 1 triangle 
        rdtest.log.print("Testing Lots of Instances")
        self.controller.SetFrameEvent(one_thousand_instances_action.next.eventId, True)
        x, y = 60, 50
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)
        countEvents = 1 + 255
        self.check(len(modifs) == countEvents, "Expected {} events, got {}".format(countEvents, len(modifs)))
        self.check_modifs_consistent(modifs)

    def multisampled_image_test(self, pass_name: str):
        begin_pass_action = self.find_action(pass_name)
        if begin_pass_action is None:
            rdtest.log.print("Test not found")
            return

        pass_start_eid = begin_pass_action.next.eventId

        test_marker: rd.ActionDescription = self.find_action("Multisampled: test", pass_start_eid)
        action_eid = test_marker.next.eventId
        self.controller.SetFrameEvent(action_eid, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()
        rt = pipe.GetOutputTargets()[0]

        if self.is_depth:
            rt = pipe.GetDepthTarget()

        sub = rd.Subresource()
        tex = rt.resource
        tex_details = self.get_texture(tex)
        if tex_details.arraysize > 1:
            sub.slice = rt.firstSlice

        beg_renderpass_eid = self.find_action("Begin MSAA", pass_start_eid).next.eventId

        x, y = 140, 130
        sub.sample = 1
        rdtest.log.print("Testing pixel {}, {} at sample {}".format(x, y, sub.sample))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)

        events = [
            [[event_id, beg_renderpass_eid], [passed, True]],
            [[event_id, action_eid], [passed, True], [primitive_id, 0], [get_pre_mod_depth, 0.0], [get_shader_out_depth, 0.9],
             [get_post_mod_depth, 0.9]],
            [[event_id, action_eid], [passed, True], [primitive_id, 1], [get_shader_out_depth, 0.95], [get_post_mod_depth, 0.95]],
        ]

        if not self.is_depth:
            events[0] += [[get_post_mod_color, (0.0, 1.0, 0.0, 1.0)]]
            events[1] += [[get_shader_out_color, (1.0, 0.0, 1.0, 2.75)], [get_post_mod_color, (1.0, 0.0, 1.0, 1.0)]]
            events[2] += [[get_shader_out_color, (0.0, 0.0, 1.0, 2.75)], [get_post_mod_color, (0.0, 0.0, 1.0, 1.0)]]

        self.check_events(events, modifs, True)

        if self.is_depth:
            self.check_pixel_value(tex, x, y, [modifs[-1].postMod.depth, float(modifs[-1].postMod.stencil)/255.0, 0.0, 1.0], sub=sub, cast=rt.format.compType)
        else:
            self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        sub.sample = 2
        rdtest.log.print("Testing pixel {}, {} at sample {}".format(x, y, sub.sample))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, beg_renderpass_eid], [passed, True]],
            [[event_id, action_eid], [passed, True], [primitive_id, 0], [get_pre_mod_depth, 0.0], [get_shader_out_depth, 0.9],
             [get_post_mod_depth, 0.9]],
            [[event_id, action_eid], [passed, True], [primitive_id, 1], [get_shader_out_depth, 0.95], [get_post_mod_depth, 0.95]],
        ]

        if not self.is_depth:
            events[0] += [[get_post_mod_color, (0.0, 1.0, 0.0, 1.0)]]
            events[1] += [[get_shader_out_color, (1.0, 0.0, 1.0, 2.75)], [get_post_mod_color, (1.0, 0.0, 1.0, 1.0)]]
            events[2] += [[get_shader_out_color, (0.0, 1.0, 1.0, 2.75)], [get_post_mod_color, (0.0, 1.0, 1.0, 1.0)]]

        self.check_events(events, modifs, True)

        if self.is_depth:
            self.check_pixel_value(tex, x, y,
                                   [modifs[-1].postMod.depth, float(modifs[-1].postMod.stencil) / 255.0, 0.0, 1.0],
                                   sub=sub, cast=rt.format.compType)
        else:
            self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

    def secondary_cmd_test(self, pass_name: str):
        begin_pass_action = self.find_action(pass_name)
        if begin_pass_action is None:
            rdtest.log.print("Test not found")
            return

        pass_start_eid = begin_pass_action.next.eventId

        secondary_marker: rd.ActionDescription = self.find_action("Secondary: red and blue", pass_start_eid)
        self.controller.SetFrameEvent(secondary_marker.next.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()
        rt = pipe.GetOutputTargets()[0]
        sub = rd.Subresource()
        tex = rt.resource
        tex_details = self.get_texture(tex)
        if tex_details.arraysize > 1:
            sub.slice = rt.firstSlice
        if tex_details.mips > 1:
            sub.mip = rt.firstMip

        sec_beg_renderpass_eid = self.find_action("Begin RenderPass Secondary", pass_start_eid).next.eventId
        background_eid = self.find_action("Secondary: background", pass_start_eid).next.eventId
        culled_eid = self.find_action("Secondary: culled", pass_start_eid).next.eventId
        sec_red_and_blue = self.find_action("Secondary: red and blue", pass_start_eid).next.eventId

        # Test culling
        x, y = 70, 40
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, sec_beg_renderpass_eid], [passed, True], [get_post_mod_color, (0.0, 1.0, 0.0, 1.0)]],
            [[event_id, background_eid], [passed, True], [get_pre_mod_color, (0.0, 1.0, 0.0, 1.0)]],
            [[event_id, culled_eid], [passed, False], [culled, True]],
        ]
        self.check_events(events, modifs, True)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        # Blue triangle
        x, y = 40, 40
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, sec_beg_renderpass_eid], [passed, True], [get_post_mod_color, (0.0, 1.0, 0.0, 1.0)]],
            # This is the first event in the command buffer, should have pre-mod
            [[event_id, background_eid], [passed, True], [get_pre_mod_color, (0.0, 1.0, 0.0, 1.0)]],
            # This is the last event in the command buffer, should have post-mod
            [[event_id, sec_red_and_blue], [passed, True], [get_post_mod_color, (0.0, 0.0, 1.0, 1.0)]],
        ]
        self.check_events(events, modifs, True)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

        # Didn't get post mod for background_eid
        self.controller.SetFrameEvent(background_eid, True)
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, sec_beg_renderpass_eid]],
            # The only event, should have both pre and post mod.
            [[event_id, background_eid], [passed, True], [get_pre_mod_color, (0.0, 1.0, 0.0, 1.0)], [get_post_mod_color, (1.0, 0.0, 1.0, 1.0)]],
        ]
        self.check_events(events, modifs, True)
        self.check_pixel_value(tex, x, y, get_post_mod_color(modifs[-1]), sub=sub, cast=rt.format.compType)

    def depth_target_test(self, begin_name: str):
        begin_pass_action = self.find_action(begin_name)
        if begin_pass_action is None:
            rdtest.log.print("Test not found")
            return

        # Advance one more than the color test since DSV clear is a separate event than RT clear
        begin_renderpass_eid = begin_pass_action.next.next.eventId

        test_marker: rd.ActionDescription = self.find_action("Test Begin", begin_renderpass_eid)

        self.controller.SetFrameEvent(test_marker.next.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        rt = pipe.GetDepthTarget()

        tex = rt.resource
        tex_details = self.get_texture(tex)

        sub = rd.Subresource()
        if tex_details.arraysize > 1:
            sub.slice = rt.firstSlice
        if tex_details.mips > 1:
            sub.mip = rt.firstMip

        stencil_write_eid = self.find_action("Stencil Write", begin_renderpass_eid).next.eventId
        unbound_fs_eid = self.find_action("Unbound Fragment Shader", begin_renderpass_eid).next.eventId
        background_eid = self.find_action("Background", begin_renderpass_eid).next.eventId
        clear_depth_16bit_eid = self.find_action("Clear Depth 16-bit", begin_renderpass_eid).next.eventId
        depth_16bit_test_eid = self.find_action("Depth 16-bit Test", begin_renderpass_eid).next.eventId
        test_eid = self.find_action("Test Begin", begin_renderpass_eid).next.eventId

        x, y = 200, 190
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True], [get_post_mod_depth, 1.0]],
            [[event_id, background_eid], [passed, True], [primitive_id, 0], [get_pre_mod_depth, 1.0], [get_post_mod_depth, 0.95]],
            [[event_id, test_eid], [passed, True], [depth_test_failed, False], [primitive_id, 0], [get_shader_out_depth, 0.5], [get_post_mod_depth, 0.5]],
            [[event_id, test_eid], [passed, False], [depth_test_failed, True], [primitive_id, 1], [get_shader_out_depth, 0.6], [get_post_mod_depth, 0.5]],
        ]
        self.check_events(events, modifs, False)

        # For pixel 110, 100, inside the red triangle with stencil value 0x55
        x, y = 110, 100
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, begin_renderpass_eid], [passed, True]],
            [[event_id, unbound_fs_eid], [passed, True], [unboundPS, True], [primitive_id, 0], [get_post_mod_stencil, 0x33]],
            [[event_id, stencil_write_eid], [passed, True], [primitive_id, 0], [get_post_mod_stencil, 0x55]],
        ]
        self.check_events(events, modifs, False)

        # For pixel 70, 100 inside the yellow triangle rendered with 16-bit depth
        rdtest.log.print("Testing 16-bit depth format")
        self.controller.SetFrameEvent(depth_16bit_test_eid, True)
        pipe: rd.PipeState = self.controller.GetPipelineState()

        rt = pipe.GetDepthTarget()

        tex = rt.resource
        tex_details = self.get_texture(tex)

        sub = rd.Subresource()
        if tex_details.arraysize > 1:
            sub.slice = rt.firstSlice
        if tex_details.mips > 1:
            sub.mip = rt.firstMip

        x, y = 70, 100
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        events = [
            [[event_id, clear_depth_16bit_eid], [passed, True]],
            [[event_id, depth_16bit_test_eid], [passed, True], [primitive_id, 0], [depth_test_failed, False],
             [get_shader_out_depth, 0.33], [get_post_mod_depth, 0.33]],
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
                if not rdtest.value_compare(actual, expected, eps=1.0/256.0):
                    raise rdtest.TestFailureException(
                        "eventId {}, primitiveID {}: testing {} expected {}, got {}".format(modifs[i].eventId, modifs[i].primitiveID,
                                                                            events[i][c][0].__name__,
                                                                            expected,
                                                                            actual))

    def check_modifs_consistent(self, modifs):
        # postmod of each should match premod of the next
        for i in range(len(modifs) - 1):
            a = get_post_mod_color(modifs[i])
            b = get_pre_mod_color(modifs[i + 1])

            # A fragment event : postMod.stencil should be unknown
            if modifs[i].eventId == modifs[i+1].eventId:
                if not unknown_stencil(modifs[i].postMod.stencil):
                    raise rdtest.TestFailureException(
                    "postmod stencil at {} primitive {}: {} is not unknown".format(modifs[i].eventId,
                                                                              modifs[i].primitiveID,
                                                                              modifs[i].postMod.stencil))

            if self.is_depth:
                a = (modifs[i].postMod.depth, modifs[i].postMod.stencil)
                b = (modifs[i + 1].preMod.depth, modifs[i + 1].preMod.stencil)

            if a != b:
                raise rdtest.TestFailureException(
                    "postmod at {} primitive {}: {} doesn't match premod at {} primitive {}: {}".format(modifs[i].eventId,
                                                                              modifs[i].primitiveID,
                                                                              a,
                                                                              modifs[i + 1].eventId,
                                                                              modifs[i + 1].primitiveID,
                                                                              b))

        # Check that if the test failed, its postmod is the same as premod
        for i in range(len(modifs)):
            if not modifs[i].Passed():
                a = get_pre_mod_color(modifs[i])
                b = get_post_mod_color(modifs[i])

                if self.is_depth:
                    a = (modifs[i].preMod.depth, modifs[i].preMod.stencil)
                    b = (modifs[i].postMod.depth, modifs[i].postMod.stencil)

                if a[1] == -2 or b[2] == -2:
                    a = (a[0], -2)
                    b = (b[0], -2)

                if not rdtest.value_compare(a, b):
                    raise rdtest.TestFailureException(
                        "postmod at {} primitive {}: {} doesn't match premod: {}".format(modifs[i].eventId,
                                                                                         modifs[i].primitiveID, b, a))
