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
def shader_out_col(x): return value_selector(x.shaderOut.col)
def shader_out_depth(x): return x.shaderOut.depth
def pre_mod_col(x): return value_selector(x.preMod.col)
def post_mod_col(x): return value_selector(x.postMod.col)
def shader_out_depth(x): return x.shaderOut.depth
def pre_mod_depth(x): return x.preMod.depth
def post_mod_depth(x): return x.postMod.depth
def primitive_id(x): return x.primitiveID
def unboundPS(x): return x.unboundPS

NUM_TRIANGLES_RED = 16
NUM_TRIANGLES_RED_REAL = NUM_TRIANGLES_RED * 2 - 1
NUM_TRIANGLES_GREEN = 255
NUM_TRIANGLES_BLUE = 512

class VK_Dual_Source(rdtest.TestCase):
    demos_test_name = 'VK_Dual_Source'
    demos_frame_cap = 5

    def check_capture(self):
        apiprops: rd.APIProperties = self.controller.GetAPIProperties()

        if not apiprops.pixelHistory:
            rdtest.log.print("Vulkan pixel history not tested")
            return

        self.primary_test()

    def primary_test(self):
        test_marker: rd.ActionDescription = self.find_action("End test B2")
        self.controller.SetFrameEvent(test_marker.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        rt: rd.BoundResource = pipe.GetOutputTargets()[0]

        tex = rt.resourceId
        tex_details = self.get_texture(tex)

        sub = rd.Subresource()
        if tex_details.arraysize > 1:
            sub.slice = rt.firstSlice
        if tex_details.mips > 1:
            sub.mip = rt.firstMip

        # Pixel inside first triangle
        modifs_1: List[rd.PixelModification] = self.controller.PixelHistory(tex, 175, 125, sub, rt.typeCast)
        # Pixel inside second triangle
        modifs_2: List[rd.PixelModification] = self.controller.PixelHistory(tex, 275, 175, sub, rt.typeCast)
        # Pixel inside both triangles
        modifs_both: List[rd.PixelModification] = self.controller.PixelHistory(tex, 225, 150, sub, rt.typeCast)

        self.check_range(modifs_1, modifs_2, modifs_both, "A1")
        self.check_range(modifs_1, modifs_2, modifs_both, "A2")
        self.check_range(modifs_1, modifs_2, modifs_both, "B1")
        self.check_range(modifs_1, modifs_2, modifs_both, "B2")

    def check_range(self, orig_modifs_1, orig_modifs_2, orig_modifs_both, group):
        start_eid = self.find_action("Begin test " + group).eventId
        end_eid = self.find_action("End test " + group).eventId
        modifs_1 = [m for m in orig_modifs_1 if m.eventId > start_eid and m.eventId < end_eid]
        modifs_2 = [m for m in orig_modifs_2 if m.eventId > start_eid and m.eventId < end_eid]
        modifs_both = [m for m in orig_modifs_both if m.eventId > start_eid and m.eventId < end_eid]
        if not len(modifs_1) == 1:
            raise rdtest.TestFailureException(
                "group {}: expected 1 modification but found {}".format(group, len(modifs_1)))
        if not len(modifs_2) == 1:
            raise rdtest.TestFailureException(
                "group {}: expected 1 modification but found {}".format(group, len(modifs_2)))
        if not len(modifs_both) == 2:
            raise rdtest.TestFailureException(
                "group {}: expected 2 modifications but found {}".format(group, len(modifs_both)))
        modif_1 = modifs_1[0]
        modif_2 = modifs_2[0]
        modif_both_1 = modifs_both[0]
        modif_both_2 = modifs_both[1]
        # modif_both_1 should match modif_1 as both are the same triangle on the same background
        assert(value_selector(modif_1.preMod.col) == value_selector(modif_both_1.preMod.col))
        assert(value_selector(modif_1.shaderOut.col) == value_selector(modif_both_1.shaderOut.col))
        assert(value_selector(modif_1.shaderOutDualSrc.col) == value_selector(modif_both_1.shaderOutDualSrc.col))
        assert(value_selector(modif_1.blendSrc.col) == value_selector(modif_both_1.blendSrc.col))
        assert(value_selector(modif_1.blendDst.col) == value_selector(modif_both_1.blendDst.col))
        assert(value_selector(modif_1.postMod.col) == value_selector(modif_both_1.postMod.col))

        # Triangle 1 outputs (0.5, 0.0, 0.0, 1.0) to source 0, (0.0, 0.5, 0.0, 1.0) to source 1
        # Background is (0.0, 1.0, 1.0, 1.0)
        # Blend is source0*1 + dest*source1, so the final output should be (0.5, 0.5, 0.0, 1.0)
        # eps=.01 because different GPUs handle blending differently
        epsilon = .01
        assert(rdtest.value_compare(value_selector(modif_1.preMod.col), (0.0, 1.0, 1.0, 1.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_1.shaderOut.col), (0.5, 0.0, 0.0, 1.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_1.shaderOutDualSrc.col), (0.0, 0.5, 0.0, 1.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_1.blendSrc.col), (0.5, 0.0, 0.0, 1.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_1.blendDst.col), (0.0, 0.5, 0.0, 0.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_1.postMod.col), (0.5, 0.5, 0.0, 1.0), eps=epsilon))

        # Triangle 2 outputs (0.5, 0.0, 0.5, 1.0) to source 0, (0.5, 0.5, 0.0, 1.0) to source 1
        # On the (0.0, 1.0, 1.0, 1.0) background the final output is (0.5, 0.0, 0.5, 1.0) added
        # to (0.0, 0.5, 0.0, 0.0) = (0.5, 0.5, 0.5, 0.0)
        assert(rdtest.value_compare(value_selector(modif_2.preMod.col), (0.0, 1.0, 1.0, 1.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_2.shaderOut.col), (0.5, 0.0, 0.5, 1.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_2.shaderOutDualSrc.col), (0.5, 0.5, 0.0, 1.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_2.blendSrc.col), (0.5, 0.0, 0.5, 1.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_2.blendDst.col), (0.0, 0.5, 0.0, 0.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_2.postMod.col), (0.5, 0.5, 0.5, 1.0), eps=epsilon))

        # Triangle 2 on top of triangle 1's output of (0.5, 0.5, 0.0, 1.0) is (0.5, 0.0, 0.5, 1.0)
        # added to (0.25, 0.25, 0.0, 0.0) = (0.75, 0.25, 0.5, 1.0)
        assert(rdtest.value_compare(value_selector(modif_both_2.preMod.col), (0.5, 0.5, 0.0, 1.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_both_2.shaderOut.col), (0.5, 0.0, 0.5, 1.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_both_2.shaderOutDualSrc.col), (0.5, 0.5, 0.0, 1.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_both_2.blendSrc.col), (0.5, 0.0, 0.5, 1.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_both_2.blendDst.col), (0.25, 0.25, 0.0, 0.0), eps=epsilon))
        assert(rdtest.value_compare(value_selector(modif_both_2.postMod.col), (0.75, 0.25, 0.5, 1.0), eps=epsilon))
