import renderdoc as rd
import rdtest
import pprint
import math
from typing import List


def float_value(x): return x.floatValue
def uint_value(x): return x.uintValue
def unknown_stencil(x): return x == -2

# Not a real test, re-used by API-specific tests


class Pixel_History(rdtest.TestCase):
    internal = True
    demos_test_name = None

    def check_capture(self):
        # cache some information since our python bindings deep copy actions and this can add up
        self.eventCache = {}
        for a in self.controller.GetRootActions():
            self.populate_eventcache(a, '', [])

        self.errored = False

        self.batch_base = self.find_action("Batch:", 0)
        while self.batch_base is not None:
            with rdtest.log.auto_section(self.batch_base.customName):
                self.is_secondary = 'Secondary' in self.batch_base.customName
                self.batch_test(self.batch_base.eventId)
                self.batch_base = self.find_action(
                    "Batch:", self.batch_base.eventId+1)
                
        if self.errored:
            raise rdtest.TestFailureException("Detected problems in pixel history")

    def error(self, message):
        rdtest.log.error(message)
        self.errored = True

    def relative_xy(self, x, y):
        self.x, self.y = (x >> self.sub.mip, y >> self.sub.mip)
        return (self.x, self.y)

    def populate_eventcache(self, action: rd.ActionDescription, name: str, parents: List[int]):
        hierarchy = parents + [action.eventId]

        self.eventCache[action.eventId] = {
            'name': name,
            'flags': action.flags,
            'hierarchy': hierarchy,
        }

        for child in action.children:
            if child.flags & rd.ActionFlags.SetMarker:
                name = child.customName

            self.populate_eventcache(child, name, hierarchy)

    def get_eventname(self, eventId: int) -> str:
        return self.eventCache[eventId]['name']

    def get_actionflags(self, eventId: int) -> rd.ActionFlags:
        return self.eventCache[eventId]['flags']

    def get_hierarchy(self, eventId: int) -> List[int]:
        return self.eventCache[eventId]['hierarchy']

    def batch_test(self, base_eid: int):
        test_marker = self.find_action("Simple Test", base_eid)
        self.controller.SetFrameEvent(test_marker.next.eventId, True)

        pipe = self.controller.GetPipelineState()

        if len(pipe.GetOutputTargets()) > 0:
            col = pipe.GetOutputTargets()[0]
        else:
            col = None
        depth = pipe.GetDepthTarget()

        self.has_colour = (col is not None and col.resource != rd.ResourceId())
        self.has_depth = (depth.resource != rd.ResourceId())

        if self.has_colour:
            rt = col
            tex = col.resource
            self.is_depth = False
        else:
            self.check(self.has_depth)
            self.is_depth = True
            rt = depth

        if self.has_depth:
            depth_details = self.get_texture(depth.resource)
            self.has_stencil = (depth_details.format.type in [
                                rd.ResourceFormatType.D16S8, rd.ResourceFormatType.D24S8, rd.ResourceFormatType.D32S8])
        else:
            self.has_stencil = False

        self.tex = tex = rt.resource
        tex_details = self.get_texture(tex)

        self.epsilon = 1.0/float(1 << tex_details.format.compByteWidth)

        sub = rd.Subresource()
        if tex_details.arraysize > 1 or tex_details.depth > 1:
            sub.slice = rt.firstSlice
        if tex_details.mips > 1:
            sub.mip = rt.firstMip

        self.sub = sub
        self.comp = comp = rt.format.compType

        def event_name(x: rd.PixelModification):
            return self.get_eventname(x.eventId)

        value_func = float_value
        alpha_value = 2.75  # 1 + ALPHA_ADD=1.75
        overflow_value = 100000.0
        clear_col = 0.2

        if self.comp == rd.CompType.UInt:
            alpha_value = 4  # 1 + ALPHA_ADD=3
            clear_col = 5  # uint values are shifted by 16, we clear to 80,80,80,160
            value_func = uint_value
            overflow_value = (1 << 28)-1

        def roundToInf(col):
            # for half-floats lower inf to the largest representable value since either is valid
            if tex_details.format.compType == rd.CompType.Float and tex_details.format.compByteWidth == 2:
                return tuple([65504.0 if x == math.inf else x for x in col])

            return col

        self.fetch_property = {
            '_': None,  # dynamically skipped test via with_col or with_depth below
            'event_name': event_name,
            'value': value_func,
            'passed': lambda x: x.Passed(),
            'culled': lambda x: x.backfaceCulled,
            'depth_test_failed': lambda x: x.depthTestFailed,
            'depth_clipped': lambda x: x.depthClipped,
            'sample_masked': lambda x: x.sampleMasked,
            'depth_bounds_failed': lambda x: x.depthBoundsFailed,
            'scissor_clipped': lambda x: x.scissorClipped,
            'stencil_test_failed': lambda x: x.stencilTestFailed,
            'shader_discarded': lambda x: x.shaderDiscarded,
            'shader_out_col': lambda x: value_func(x.shaderOut.col),
            'shader_out_depth': lambda x: x.shaderOut.depth,
            'pre_mod_col': lambda x: value_func(x.preMod.col),
            'post_mod_col': lambda x: roundToInf(value_func(x.postMod.col)),
            'pre_mod_depth': lambda x: x.preMod.depth,
            'post_mod_depth': lambda x: x.postMod.depth,
            'post_mod_stencil': lambda x: x.postMod.stencil,
            'primitive_id': lambda x: x.primitiveID,
            'unknown_post_mod_stencil': lambda x: unknown_stencil(x.postMod.stencil),
            'unboundPS': lambda x: x.unboundPS,
            'directWrite': lambda x: x.directShaderWrite,
        }

        def with_depth(x):
            return x if self.has_depth else '_'

        def with_stencil(x):
            return x if self.has_stencil else '_'

        def fmt_adjusted(r, g, b, a):
            if tex_details.format.compType == rd.CompType.UInt:
                r = int(r * 16)
                g = int(g * 16)
                b = int(b * 16)
                a = int(a * 16)

            return (r, g, b, a)

        def fmt_clamped(r, g, b, a):
            r, g, b, a = fmt_adjusted(r, g, b, a)

            if tex_details.format.compType == rd.CompType.UNorm:
                r = max(0, min(1, r))
                g = max(0, min(1, g))
                b = max(0, min(1, b))
                a = max(0, min(1, a))
            elif tex_details.format.compType == rd.CompType.Float and tex_details.format.compByteWidth == 2:
                r = max(-65504, min(65504, r))
                g = max(-65504, min(65504, g))
                b = max(-65504, min(65504, b))
                a = max(-65504, min(65504, a))
            elif tex_details.format.compType == rd.CompType.UInt:
                maxval = (1 << (8*tex_details.format.compByteWidth))-1
                r = max(0, min(maxval, r))
                b = max(0, min(maxval, b))
                a = max(0, min(maxval, a))
                g = max(0, min(maxval, g))

            return r, g, b, a

        # can't reliably get colours, depths, or primitive IDs on secondaries
        if self.is_secondary:
            self.fetch_property['pre_mod_col'] = None
            self.fetch_property['post_mod_col'] = None
            self.fetch_property['shader_out_col'] = None
            self.fetch_property['pre_mod_depth'] = None
            self.fetch_property['shader_out_depth'] = None
            self.fetch_property['post_mod_depth'] = None
            self.fetch_property['post_mod_stencil'] = None
            self.fetch_property['unknown_post_mod_stencil'] = None
            self.fetch_property['primitive_id'] = None

        if not self.has_colour:
            self.fetch_property['pre_mod_col'] = None
            self.fetch_property['post_mod_col'] = None
            self.fetch_property['shader_out_col'] = None

        if not self.has_depth:
            self.fetch_property['depth_test_failed'] = None
            self.fetch_property['depth_clipped'] = None
            self.fetch_property['depth_bounds_failed'] = None
            self.fetch_property['stencil_test_failed'] = None
            self.fetch_property['pre_mod_depth'] = None
            self.fetch_property['shader_out_depth'] = None
            self.fetch_property['post_mod_depth'] = None
            self.fetch_property['post_mod_stencil'] = None
            self.fetch_property['unknown_post_mod_stencil'] = None

        # don't check stencil if there's no stencil channel
        if not self.has_stencil:
            self.fetch_property['post_mod_stencil'] = None
            self.fetch_property['unknown_post_mod_stencil'] = None

        # check if depth bounds is supportted - D3D11 does not
        has_depth_bounds = self.find_action('Depth Bounds Prep') is not None

        x, y = self.relative_xy(110, 100)
        rdtest.log.print("Testing Unbound PS {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        events = [
            {
                'event_name': 'Begin RenderPass',
                'passed': True
            },
            {
                'event_name': 'Unbound Shader',
                'passed': True,
                'unboundPS': True,
                'primitive_id': 0,
                'post_mod_stencil': 0x33
            },
            {
                'event_name': 'Stencil Write',
                'passed': True,
                'primitive_id': 0,
                'post_mod_stencil': 0x55
            },
        ]
        self.check_events(events, modifs)

        x, y = self.relative_xy(170, 140)
        rdtest.log.print("Testing Stencil failure {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        events = [
            {
                'event_name': 'Begin RenderPass',
                'passed': True
            },
            {
                'event_name': 'Unbound Shader',
                'passed': True,
                'unboundPS': True,
                'primitive_id': 0
            },
            {
                'event_name': 'Stencil Write',
                'passed': True
            },
            {
                'event_name': 'Background',
                'depth_test_failed': True,
                with_depth('post_mod_col'): fmt_clamped(1, 0, 0, alpha_value)
            },
            {
                'event_name': 'Simple Test',
                with_stencil('stencil_test_failed'): True
            },
        ]
        self.check_events(events, modifs)

        x, y = self.relative_xy(170, 160)
        rdtest.log.print("Testing Depth failure {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        events = [
            {
                'event_name': 'Begin RenderPass',
                'passed': True
            },
            {
                'event_name': 'Depth Write',
                'passed': True
            },
            {
                'event_name': 'Background',
                'depth_test_failed': True
            },
            {
                'event_name': 'Cull Front',
                'culled': True
            },
            {
                'event_name': 'Simple Test',
                'depth_test_failed': True
            },
        ]
        self.check_events(events, modifs)

        x, y = self.relative_xy(150, 250)
        rdtest.log.print("Testing Discard {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        events = [
            {
                'event_name': 'Begin RenderPass',
                'passed': True
            },
            {
                'event_name': 'Background',
                'shader_discarded': True
            },
        ]
        self.check_events(events, modifs)

        x, y = self.relative_xy(330, 145)
        rdtest.log.print("Testing Primitive ID {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        events = [
            {
                'event_name': 'Begin RenderPass',
                'passed': True
            },
            {
                'event_name': 'Simple Test',
                'passed': True,
                'primitive_id': 3,
                'shader_out_col': fmt_adjusted(0, 0, 0, alpha_value)
            },
        ]
        self.check_events(events, modifs)

        x, y = self.relative_xy(340, 145)
        rdtest.log.print("Testing Depth Clip {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        events = [
            {
                'event_name': 'Begin RenderPass',
                'passed': True
            },
            {
                'event_name': 'Simple Test',
                'passed': False,
                'depth_clipped': True
            },
        ]
        self.check_events(events, modifs)

        # if depth bounds test isn't supported these draws won't be emitted
        if has_depth_bounds:
            x, y = self.relative_xy(330, 102)
            rdtest.log.print("Testing Depth Bounds Pass {}, {}".format(x, y))
            modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
            events = [
                {
                    'event_name': 'Begin RenderPass',
                    'passed': True
                },
                {
                    'event_name': 'Depth Bounds Prep',
                    'passed': True,
                    'primitive_id': 0,
                    'shader_out_col': fmt_adjusted(1, 0, 0, alpha_value)
                },
                {
                    'event_name': 'Depth Bounds Clip',
                    'passed': True,
                    'primitive_id': 0,
                    'shader_out_col': fmt_adjusted(0, 1, 0, alpha_value)
                },
            ]
            self.check_events(events, modifs)

            # slightly precise X to ensure it still picks the right edge of the triangle on mip version
            x, y = self.relative_xy(318, 102)
            rdtest.log.print("Testing Depth Bounds Fail {}, {}".format(x, y))
            modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
            events = [
                {
                    'event_name': 'Begin RenderPass',
                    'passed': True
                },
                {
                    'event_name': 'Depth Bounds Prep',
                    'passed': True,
                    'primitive_id': 0,
                    'shader_out_col': fmt_adjusted(1, 0, 0, alpha_value)
                },
                {
                    'event_name': 'Depth Bounds Clip',
                    with_depth('passed'): False,
                    'depth_bounds_failed': True
                },
            ]
            self.check_events(events, modifs)

            x, y = self.relative_xy(346, 102)
            rdtest.log.print("Testing Depth Bounds Fail {}, {}".format(x, y))
            modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
            events = [
                {
                    'event_name': 'Begin RenderPass',
                    'passed': True
                },
                {
                    'event_name': 'Depth Bounds Prep',
                    'passed': True,
                    'primitive_id': 0,
                    'shader_out_col': fmt_adjusted(1, 0, 0, alpha_value)
                },
                {
                    'event_name': 'Depth Bounds Clip',
                    with_depth('passed'): False,
                    'depth_bounds_failed': True
                },
            ]
            self.check_events(events, modifs)

        rdtest.log.print("Testing stencil/scissor checks")
        test_marker = self.find_action("Stencil Mask", base_eid)
        self.controller.SetFrameEvent(test_marker.next.eventId, True)

        x, y = self.relative_xy(106, 248)
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        events = [
            {
                'event_name': 'Begin RenderPass',
                'passed': True
            },
            {
                'event_name': 'Background',
                'passed': True
            },
            {
                'event_name': 'Scissor Fail',
                'scissor_clipped': True
            },
            {
                'event_name': 'Scissor Pass',
                'passed': True,
                'shader_out_col': fmt_adjusted(0, 1, 0, alpha_value),
                'post_mod_col': fmt_clamped(0, 1, 0, alpha_value)
            },
            {
                'event_name': 'Stencil Ref',
                'passed': True,
                'shader_out_col': fmt_adjusted(0, 0, 1, alpha_value),
                'post_mod_col': fmt_clamped(0, 0, 1, alpha_value)
            },
            {
                'event_name': 'Stencil Mask',
                'passed': True,
                'shader_out_col': fmt_adjusted(0, 1, 1, alpha_value),
                'post_mod_col': fmt_clamped(0, 1, 1, alpha_value)
            },
        ]
        self.check_events(events, modifs)

        rdtest.log.print("Testing depth test for per fragment reporting")
        test_marker = self.find_action("Depth Test", base_eid)
        self.controller.SetFrameEvent(test_marker.next.eventId, True)

        x, y = self.relative_xy(275, 258)
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        events = [
            {
                'event_name': 'Begin RenderPass',
                'passed': True
            },
            {
                'event_name': 'Background',
                'passed': True
            },
            {
                'event_name': 'Depth Test',
                'primitive_id': 0,
                'depth_test_failed': True,
                'shader_out_col': fmt_adjusted(0, 1, 0, alpha_value),
                'shader_out_depth': 0.97,
                with_depth('post_mod_col'): fmt_clamped(1, 0, 1, alpha_value),
                'post_mod_depth': 0.95,
                'unknown_post_mod_stencil': True
            },
            {
                'event_name': 'Depth Test',
                'passed': True,
                'primitive_id': 1,
                'depth_test_failed': False,
                'shader_out_col': fmt_adjusted(1, 1, 0, alpha_value),
                'shader_out_depth': 0.20,
                'post_mod_col': fmt_clamped(1, 1, 0, alpha_value),
                'post_mod_depth': 0.20,
                'unknown_post_mod_stencil': True
            },
            {
                'event_name': 'Depth Test',
                'primitive_id': 2,
                'depth_test_failed': True,
                'shader_out_col': fmt_adjusted(1, 0, 0, alpha_value),
                'shader_out_depth': 0.30,
                with_depth('post_mod_col'): fmt_clamped(1, 1, 0, alpha_value),
                'post_mod_depth': 0.20,
                'unknown_post_mod_stencil': True
            },
            {
                'event_name': 'Depth Test',
                'passed': True,
                'primitive_id': 3,
                'depth_test_failed': False,
                'shader_out_col': fmt_adjusted(0, 0, 1, alpha_value),
                'shader_out_depth': 0.10,
                'post_mod_col': fmt_clamped(0, 0, 1, alpha_value),
                'post_mod_depth': 0.10,
                'unknown_post_mod_stencil': True
            },
        ]

        # if we don't have depth bounds testing the final event will pass
        if has_depth_bounds:
            events.append(
                {
                    'event_name': 'Depth Test',
                    'primitive_id': 4,
                    'depth_test_failed': False,
                    'depth_bounds_failed': True,
                    'shader_out_col': fmt_adjusted(1, 1, 1, alpha_value),
                    'shader_out_depth': 0.05,
                    with_depth('post_mod_col'): fmt_clamped(0, 0, 1, alpha_value),
                    'post_mod_depth': 0.10,
                    'unknown_post_mod_stencil': False
                })
        else:
            events.append(
                {
                    'event_name': 'Depth Test',
                    'passed': True,
                    'primitive_id': 4,
                    'depth_test_failed': False,
                    'shader_out_col': fmt_adjusted(1, 1, 1, alpha_value),
                    'shader_out_depth': 0.05,
                    with_depth('post_mod_col'): fmt_clamped(1, 1, 1, alpha_value),
                    'post_mod_depth': 0.05,
                    'unknown_post_mod_stencil': False
                })

        # can't distinguish per-fragment results in secondaries
        if self.is_secondary:
            events = [
                {
                    'event_name': 'Begin RenderPass',
                    'passed': True
                },
                {
                    'event_name': 'Background',
                    'passed': True
                },
                {
                    'event_name': 'Depth Test',
                    'post_mod_col': fmt_clamped(0, 0, 1, alpha_value),
                },
            ]
        self.check_events(events, modifs)

        # For pixel 60, 130 inside the light green triangle which is 300 draws of 1 instance of 1 triangle
        rdtest.log.print("Testing Lots of Drawcalls")
        test_marker = self.find_action("300 Instances", base_eid)
        self.controller.SetFrameEvent(test_marker.next.eventId, True)
        x, y = self.relative_xy(60, 130)
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        self.check_final_colour(tex, x, y, modifs, sub, comp)
        countEvents = 1 + 300
        if len(modifs) != countEvents:
            self.error("Expected {} events, got {}".format(countEvents, len(modifs)))
        self.check_modifs_consistent(modifs)

        # For pixel 60, 50 inside the orange triangle which is 1 draws of 300 instances of 1 triangle
        rdtest.log.print("Testing Lots of Instances")
        x, y = self.relative_xy(60, 50)
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        self.check_final_colour(tex, x, y, modifs, sub, comp)

        countEvents = 1 + 255
        # secondaries can't count fragment events, so we only get 1 for the draw
        if self.is_secondary:
            countEvents = 1 + 1
        if len(modifs) != countEvents:
            self.error("Expected {} events, got {}".format(countEvents, len(modifs)))
        self.check_modifs_consistent(modifs)

        rdtest.log.print("Testing Sample colouring")
        test_marker = self.find_action("Sample Colouring", base_eid)
        self.controller.SetFrameEvent(test_marker.next.eventId, True)
        x, y = self.relative_xy(330, 200)
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        events = [
            {
                'event_name': 'Begin RenderPass',
                'passed': True
            },
            {
                'event_name': 'Sample Colouring',
                'passed': True,
                'primitive_id': 0,
                'shader_out_col': fmt_adjusted(1, 0, 0, alpha_value)
            },
        ]
        self.check_events(events, modifs)
        self.check_modifs_consistent(modifs)

        if tex_details.msSamp == 4:
            sub.sample = 1
            rdtest.log.print(f"Testing sample {sub.sample}")
            modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
            events = [
                {
                    'event_name': 'Begin RenderPass',
                    'passed': True
                },
                {
                    'event_name': 'Sample Colouring',
                    'passed': True,
                    'primitive_id': 0,
                    'shader_out_col': fmt_adjusted(0, 0, 1, alpha_value)
                },
            ]
            self.check_events(events, modifs)
            self.check_modifs_consistent(modifs)

            sub.sample = 2
            rdtest.log.print(f"Testing sample {sub.sample}")
            modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
            events = [
                {
                    'event_name': 'Begin RenderPass',
                    'passed': True
                },
                {
                    'event_name': 'Sample Colouring',
                    'passed': True,
                    'primitive_id': 0,
                    'shader_out_col': fmt_adjusted(0, 1, 1, alpha_value)
                },
            ]
            self.check_events(events, modifs)
            self.check_modifs_consistent(modifs)

            sub.sample = 3
            rdtest.log.print(f"Testing sample {sub.sample}")
            modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
            events = [
                {
                    'event_name': 'Begin RenderPass',
                    'passed': True,
                },
                {
                    'event_name': 'Sample Colouring',
                    'passed': False,
                    'sample_masked': True,
                },
            ]
            self.check_events(events, modifs)
            self.check_modifs_consistent(modifs)

            sub.sample = 0

        rdtest.log.print("Testing depth-equal testing")
        test_marker = self.find_action("Depth Equal Pass", base_eid)
        self.controller.SetFrameEvent(test_marker.next.eventId, True)

        x, y = self.relative_xy(200, 250)
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        events = [
            {
                'event_name': 'Begin RenderPass',
                'passed': True
            },
            {
                'event_name': 'Depth Equal Setup',
                'passed': True,
                'shader_out_col': fmt_adjusted(0.1, 0.1, 0.1, alpha_value),
                'shader_out_depth': 0.1,
                with_depth('post_mod_col'): fmt_clamped(0.1, 0.1, 0.1, alpha_value),
            },
            {
                'event_name': 'Background',
                with_depth('passed'): False,
                'depth_test_failed': True,
            },
            {
                'event_name': 'Depth Equal Fail',
                with_depth('passed'): False,
                'depth_test_failed': True,
                'shader_out_col': fmt_adjusted(0, 0, 0, alpha_value),
                'shader_out_depth': 0.1 + 5.0e-4,
                with_depth('post_mod_col'): fmt_clamped(0.1, 0.1, 0.1, alpha_value),
            },
            {
                'event_name': 'Depth Equal Pass',
                'passed': True,
                'shader_out_col': fmt_adjusted(1, 1, 1, alpha_value),
                'shader_out_depth': 0.1 + 1e-8,
                'post_mod_col': fmt_clamped(1, 1, 1, alpha_value),
            },
        ]
        self.check_events(events, modifs)

        rdtest.log.print("Testing colour masking")
        test_marker = self.find_action("Colour Masked", base_eid)
        self.controller.SetFrameEvent(test_marker.next.eventId, True)

        x, y = self.relative_xy(60, 80)
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        events = [
            {
                'event_name': 'Begin RenderPass',
                'passed': True
            },
            {
                'event_name': 'Colour Masked',
                'passed': True,
                'shader_out_col': fmt_adjusted(3, 3, 3, 2+alpha_value),
                'post_mod_col': fmt_clamped(3, 3, clear_col, 1),
            },
        ]
        self.check_events(events, modifs)

        if self.has_colour:
            rdtest.log.print("Testing direct writes")
            test_marker = self.find_action("Compute write", base_eid)

            # don't have compute writes in secondaries and some tests like D3D MSAA won't have compute writes
            if test_marker is not None and not self.is_secondary and base_eid in self.get_hierarchy(test_marker.eventId):
                self.controller.SetFrameEvent(test_marker.next.eventId, True)
                x, y = self.relative_xy(225, 85)
                rdtest.log.print("Testing pixel {}, {}".format(x, y))
                modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
                events = [
                    {
                        'event_name': 'Begin RenderPass',
                        'passed': True
                    },
                    {
                        'event_name': 'Background',
                        'passed': True
                    },
                    {
                        'event_name': 'Compute write',
                        'passed': True,
                        'directWrite': True,
                        'shader_out_depth': -1,
                        'post_mod_stencil': -1,
                        'post_mod_depth': -1,
                        'post_mod_col': fmt_clamped(3, 3, 3, 9),
                    },
                ]
                self.check_events(events, modifs)

            rdtest.log.print("Testing overflowed writes")
            test_marker = self.find_action("Overflowing", base_eid)
            self.controller.SetFrameEvent(test_marker.next.eventId, True)

            x, y = self.relative_xy(105, 50)
            rdtest.log.print("Testing pixel {}, {}".format(x, y))
            modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
            events = [
                {
                    'event_name': 'Begin RenderPass',
                    'passed': True
                },
                {
                    'event_name': 'Overflowing',
                    'passed': True,
                    'shader_out_col': fmt_adjusted(overflow_value, 0, 0, alpha_value),
                    'post_mod_col': fmt_clamped(overflow_value, 0, 0, alpha_value)
                },
                {
                    'event_name': 'Overflowing',
                    'passed': True,
                    'shader_out_col': fmt_adjusted(0, overflow_value, 0, alpha_value),
                    'post_mod_col': fmt_clamped(0, overflow_value, 0, alpha_value)
                },
                {
                    'event_name': 'Overflowing',
                    'passed': True,
                    'shader_out_col': fmt_adjusted(0, 0, overflow_value, alpha_value),
                    'post_mod_col': fmt_clamped(0, 0, overflow_value, alpha_value)
                },
            ]
            # can't distinguish per-fragment results in secondaries
            if self.is_secondary:
                events = [
                    {
                        'event_name': 'Begin RenderPass',
                        'passed': True
                    },
                    {
                        'event_name': 'Overflowing',
                        'passed': True,
                        'shader_out_col': fmt_adjusted(0, 0, overflow_value, alpha_value),
                        'post_mod_col': fmt_clamped(0, 0, overflow_value, alpha_value)
                    },
                ]
            self.check_events(events, modifs)

        rdtest.log.print("Testing per-fragment discards")
        test_marker = self.find_action("Per-Fragment discarding", base_eid)
        self.controller.SetFrameEvent(test_marker.next.eventId, True)

        x, y = self.relative_xy(60, 160)
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
        events = [
            {
                'event_name': 'Begin RenderPass',
                'passed': True
            },
            {
                'event_name': 'Per-Fragment discarding',
                'passed': False,
                'shader_discarded': True,
                'shader_out_col': (0, 0, 0, 0),
                'shader_out_depth': -1,
                'unknown_post_mod_stencil': True,
                'primitive_id': 0,
            },
            {
                'event_name': 'Per-Fragment discarding',
                'passed': True,
                'primitive_id': 1,
                'shader_out_col': fmt_adjusted(1, 1, 1, alpha_value),
                'shader_out_depth': 0.33,
                'post_mod_col': fmt_clamped(1, 1, 1, alpha_value),
            },
        ]
        # can't distinguish per-fragment results in secondaries
        if self.is_secondary:
            events = [
                {
                    'event_name': 'Begin RenderPass',
                    'passed': True
                },
                {
                    'event_name': 'Per-Fragment discarding',
                    'passed': True,
                    'primitive_id': 1,
                    'shader_out_col': fmt_adjusted(1, 1, 1, alpha_value),
                    'shader_out_depth': 0.33,
                    'post_mod_col': fmt_clamped(1, 1, 1, alpha_value),
                },
            ]
        self.check_events(events, modifs)

        if self.has_colour:
            x, y = self.relative_xy(120, 110)
            rdtest.log.print("Testing D3D no-output shader {}, {}".format(x, y))
            test_marker = self.find_action("No Output Shader", base_eid)
            self.controller.SetFrameEvent(test_marker.next.eventId, True)

            modifs = self.controller.PixelHistory(tex, x, y, sub, comp)
            events = [
                {
                    'event_name': 'Begin RenderPass',
                },
                {
                    'event_name': 'Unbound Shader',
                },
                {
                    'event_name': 'Stencil Write',
                },
                {
                    'event_name': 'No Output Shader',
                    'passed': True,
                    'unboundPS': True,
                    'primitive_id': 0
                },
            ]
            self.check_events(events, modifs)

    def check_final_colour(self, tex, x, y, modifs: List[rd.PixelModification], sub, comp):
        m = modifs[-1]
        if self.has_colour:
            expected = self.fetch_property['value'](m.postMod.col)
        else:
            expected = (m.postMod.depth, m.postMod.stencil/255.0, 0, 1)
        self.check_pixel_value(tex, x, y, expected, sub=sub, cast=comp)

    def check_events(self, events, modifs: List[rd.PixelModification]):
        # remove any modifs that didn't happen in the batch we're looking at -
        # targets can be reused between batches
        modifs = [
            m for m in modifs if self.batch_base.eventId in self.get_hierarchy(m.eventId)]

        if len(modifs) != len(events):
            rdtest.log.print(str([e['event_name'] for e in events]))
            rdtest.log.print(str([self.fetch_property['event_name'](m) for m in modifs]))
            self.error(f"Expected {len(events)} events got {len(modifs)}")
            return

        if not self.is_secondary:
            self.check_modifs_consistent(modifs)

        self.check_final_colour(self.tex, self.x, self.y,
                                modifs, self.sub, self.comp)

        for i, m in enumerate(modifs):
            m = modifs[i]
            for prop_name, expected in events[i].items():
                prop_getter = self.fetch_property[prop_name]

                # property disabled, e.g. because colour or depth is not present, or we're in a secondary
                if prop_getter is None:
                    # must have a reason - don't allow tests to skip in the main check
                    self.check(
                        not self.has_colour or not self.has_depth or not self.has_stencil or self.is_secondary)
                    continue

                actual = prop_getter(m)

                epsilon = self.epsilon

                if prop_name == 'shader_out_depth':
                    # reasonable F32 epsilon close to 1.0
                    epsilon = 6.0e-8

                if not rdtest.value_compare(actual, expected, eps=epsilon):
                    self.error(
                        f"eventId {m.eventId}, primitiveID {m.primitiveID}: " +
                        f"testing {prop_name} on modification {i} expected {expected}, got {actual}")

    def check_modifs_consistent(self, modifs: List[rd.PixelModification]):
        # postmod of each should match premod of the next
        for i in range(len(modifs) - 1):
            m = modifs[i]
            n = modifs[i + 1]

            if not m.postMod.IsValid() or not n.preMod.IsValid():
                continue

            a = self.fetch_property['value'](m.postMod.col)
            b = self.fetch_property['value'](n.preMod.col)

            # A fragment event. If we have depth postMod.stencil should be unknown and depth should be consistent
            if m.eventId == n.eventId and self.has_depth:
                if self.has_stencil and not unknown_stencil(m.postMod.stencil):
                    self.error(
                        f"postmod stencil at EID {m.eventId} primitive {m.primitiveID}: {m.postMod.stencil} is not unknown")

                if not rdtest.value_compare(m.postMod.depth, n.preMod.depth):
                    self.error(
                        f"postmod depth at EID {m.eventId} primitive {m.primitiveID}: {m.postMod.depth} " +
                        f"doesn't match premod at next primitive {n.primitiveID}: {m.preMod.depth}")

            epsilon = self.epsilon

            if self.is_depth:
                a = (m.postMod.depth, m.postMod.stencil)
                b = (n.preMod.depth, n.preMod.stencil)

                epsilon = 1.0e-5

            if not rdtest.value_compare(a, b, eps=epsilon):
                self.error(
                    f"postmod at EID {m.eventId} primitive {m.primitiveID}: {a} "
                    f"doesn't match premod at {n.eventId} primitive {n.primitiveID}: {b}")

        # if we're missing either colour or depth, or it's a clear (which never is affected by the other even if it's bound),
        # check that modifications list that properly
        for m in modifs:
            if not self.has_colour or (self.get_actionflags(m.eventId) & rd.ActionFlags.ClearDepthStencil):
                if not rdtest.value_compare(m.preMod.col.uintValue, (0, 0, 0, 0)):
                    self.error(f"preMod is not zeroed for missing color at EID {m.eventId}")
                if not rdtest.value_compare(m.postMod.col.uintValue, (0, 0, 0, 0)):
                    self.error(f"postMod is not zeroed for missing color at EID {m.eventId}")
                if not rdtest.value_compare(m.shaderOut.col.uintValue, (0, 0, 0, 0)):
                    self.error(f"shaderOut is not zeroed for missing color at EID {m.eventId}")
            if not self.has_depth or (self.get_actionflags(m.eventId) & rd.ActionFlags.ClearColor):
                if not rdtest.value_compare(m.preMod.depth, -1.0):
                    self.error(f"preMod depth is not -1 for missing depth at EID {m.eventId}")
                if not rdtest.value_compare(m.preMod.stencil, -1):
                    self.error(f"preMod stencil is not -1 for missing depth at EID {m.eventId}")
                if not rdtest.value_compare(m.postMod.depth, -1.0):
                    self.error(f"postMod depth is not -1 for missing depth at EID {m.eventId}")
                if not rdtest.value_compare(m.postMod.stencil, -1):
                    self.error(f"postMod stencil is not -1 for missing depth at EID {m.eventId}")
                if not rdtest.value_compare(m.shaderOut.depth, -1.0):
                    self.error(f"shaderOut depth is not -1 for missing depth at EID {m.eventId}")
                if not rdtest.value_compare(m.shaderOut.stencil, -1):
                    self.error(f"shaderOut stencil is not -1 for missing depth at EID {m.eventId}")

        # Check that if the test failed, its postmod is the same as premod
        for i in range(len(modifs)):
            if not m.Passed() and m.preMod.IsValid() and m.postMod.IsValid():
                a = self.fetch_property['value'](m.preMod.col)
                b = self.fetch_property['value'](m.postMod.col)

                epsilon = self.epsilon

                if self.is_depth:
                    a = (m.preMod.depth, m.preMod.stencil)
                    b = (m.postMod.depth, m.postMod.stencil)

                    epsilon = 1.0e-5

                    if a[1] == -2 or b[1] == -2:
                        a = (a[0], -2)
                        b = (b[0], -2)

                if not rdtest.value_compare(a, b, eps=epsilon):
                    self.error(
                        f"postmod at EID {m.eventId} primitive {m.primitiveID}: {b} doesn't match premod: {a}")
