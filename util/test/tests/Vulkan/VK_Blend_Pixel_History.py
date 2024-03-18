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

class VK_Blend_Pixel_History(rdtest.TestCase):
    demos_test_name = 'VK_Blend'

    def check_capture(self):
        apiprops: rd.APIProperties = self.controller.GetAPIProperties()

        if not apiprops.pixelHistory:
            rdtest.log.print("Vulkan pixel history not tested")
            return

        self.primary_test()

    def primary_test(self):
        test_marker: rd.ActionDescription = self.find_action("Test End")
        self.controller.SetFrameEvent(test_marker.eventId, True)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        rt = pipe.GetOutputTargets()[0]

        tex = rt.resource
        tex_details = self.get_texture(tex)

        sub = rd.Subresource()
        if tex_details.arraysize > 1:
            sub.slice = rt.firstSlice
        if tex_details.mips > 1:
            sub.mip = rt.firstMip

        red_eid = self.find_action("Red: ").next.eventId
        red_last_eid = self.find_action("End of red").next.eventId
        green_eid = self.find_action("Green: ").next.eventId
        blue_eid = self.find_action("Blue: ").next.eventId
        all_eid = self.find_action("All of the above in a single drawcall").next.eventId

        # Pixel inside of all of the triangles
        x, y = 200, 150
        rdtest.log.print("Testing pixel {}, {}".format(x, y))
        modifs: List[rd.PixelModification] = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        self.check_modifs_consistent(modifs)
        red_modifs = [m for m in modifs if m.eventId >= red_eid and m.eventId < red_last_eid]
        green_modifs = [m for m in modifs if m.eventId == green_eid]
        blue_modifs = [m for m in modifs if m.eventId == blue_eid]
        all_modifs = [m for m in modifs if m.eventId == all_eid]

        if len(red_modifs) != NUM_TRIANGLES_RED_REAL:
            raise rdtest.TestFailureException("Expected {} modifications for red triangles (EIDS {} until {}) but got {}".format(NUM_TRIANGLES_RED_REAL, red_eid, red_last_eid, len(red_modifs)))

        for i, modif in enumerate(red_modifs):
            if not rdtest.value_compare(modif.shaderOut.col.floatValue, (1.0/255.0, 0.0, 0.0, 1.0), eps=1.0/256.0):
                raise rdtest.TestFailureException("Wrong shader output for red triangle {}; got {}, wanted {}".format(i, modif.shaderOut.col.floatValue, (1.0/255.0, 0.0, 0.0, 1.0)))
            if not rdtest.value_compare(modif.postMod.col.floatValue, ((i+1)/255.0, 0.0, 0.0, 1.0), eps=1.0/256.0):
                raise rdtest.TestFailureException("Wrong post mod for red triangle {}; got {}, wanted {}".format(i, modif.postMod.col.floatValue, ((i+1)/255.0, 0.0, 0.0, 1.0)))

        i = 1
        eid_counter = 0
        modif_counter = 0
        while i <= NUM_TRIANGLES_RED:
            for primitive_id in range(i):
                if red_modifs[modif_counter].eventId != red_eid + eid_counter:
                    raise rdtest.TestFailureException("Expected red triangle {} to be part of EID {} but was {}".format(modif_counter, red_eid + eid_counter, red_modifs[modif_counter].eventId))
                if red_modifs[modif_counter].primitiveID != primitive_id:
                    raise rdtest.TestFailureException("Expected red triangle {} to have primitive ID {} but was {}".format(modif_counter, primitive_id, red_modifs[modif_counter].primitiveID))
                modif_counter += 1
            eid_counter += 1
            i *= 2

        if len(green_modifs) != NUM_TRIANGLES_GREEN:
            raise rdtest.TestFailureException("Expected {} modifications for green triangles (EID {}) but got {}".format(NUM_TRIANGLES_GREEN, green_eid, len(gren_modifs)))

        for i, modif in enumerate(green_modifs):
            if modif.primitiveID != i:
                raise rdtest.TestFailureException("Expected green triangle {} to have primitive ID {} but was {}".format(i, primitive_id, modif.primitiveID))
            if not rdtest.value_compare(modif.shaderOut.col.floatValue, (0.0, 1.0/255.0, 0.0, 1.0), eps=1.0/256.0):
                raise rdtest.TestFailureException("Wrong shader output for green triangle {}; got {}, wanted {}".format(i, modif.shaderOut.col.floatValue, (0.0, 1.0/255.0, 0.0, 1.0)))
            if not rdtest.value_compare(modif.postMod.col.floatValue, (NUM_TRIANGLES_RED_REAL/255.0, (i+1)/255.0, 0.0, 1.0), eps=1.0/256.0):
                raise rdtest.TestFailureException("Wrong post mod for green triangle {}; got {}, wanted {}".format(i, modif.postMod.col.floatValue, (NUM_TRIANGLES_RED_REAL/255.0, (i+1)/255.0, 0.0, 1.0)))

        # We can only record 255 modifications due to the stencil format
        if len(blue_modifs) != 255:
            raise rdtest.TestFailureException("Expected {} modifications for blue triangles (EID {}) but got {}".format(255, blue_eid, len(blue_modifs)))

        for i, modif in enumerate(blue_modifs):
            if modif.primitiveID != i:
                raise rdtest.TestFailureException("Expected blue triangle {} to have primitive ID {} but was {}".format(i, primitive_id, modif.primitiveID))
            if not rdtest.value_compare(modif.shaderOut.col.floatValue, (0.0, 0.0, 1.0/255.0, 1.0), eps=1.0/256.0):
                raise rdtest.TestFailureException("Wrong shader output for blue triangle {}; got {}, wanted {}".format(i, modif.shaderOut.col.floatValue, (0.0, 0.0, 1.0/255.0, 1.0)))
            if i == 254:
                if not rdtest.value_compare(modif.postMod.col.floatValue, (NUM_TRIANGLES_RED_REAL/255.0, 1.0, NUM_TRIANGLES_BLUE/255.0, 1.0), eps=1.0/256.0):
                    raise rdtest.TestFailureException("Wrong post mod for final blue triangle {}; got {}, wanted {}".format(i, modif.postMod.col.floatValue, (NUM_TRIANGLES_RED_REAL/255.0, 1.0, NUM_TRIANGLES_BLUE/255.0, 1.0)))
            else:
                if not rdtest.value_compare(modif.postMod.col.floatValue, (NUM_TRIANGLES_RED_REAL/255.0, 1.0, (i+1)/255.0, 1.0), eps=1.0/256.0):
                    raise rdtest.TestFailureException("Wrong post mod for blue triangle {}; got {}, wanted {}".format(i, modif.postMod.col.floatValue, (NUM_TRIANGLES_RED_REAL/255.0, 1.0, (i+1)/255.0, 1.0)))

        # Once again, we can only record 255 modifications due to the stencil format
        if len(all_modifs) != 255:
            raise rdtest.TestFailureException("Expected {} modifications for all triangles (EID {}) but got {}".format(255, all_eid, len(all_modifs)))

        for i, modif in enumerate(all_modifs):
            if modif.primitiveID != i:
                raise rdtest.TestFailureException("Expected triangle {} in all to have primitive ID {} but was {}".format(i, primitive_id, modif.primitiveID))

            if i < NUM_TRIANGLES_RED:
                if not rdtest.value_compare(modif.shaderOut.col.floatValue, (1.0/255.0, 0.0, 0.0, 1.0), eps=1.0/256.0):
                    raise rdtest.TestFailureException("Wrong shader output for red triangle in all {}; got {}, wanted {}".format(i, modif.shaderOut.col.floatValue, (1.0/255.0, 0.0, 0.0, 1.0)))
                if not rdtest.value_compare(modif.postMod.col.floatValue, ((i+1)/255.0, 0.0, 0.0, 1.0), eps=1.0/256.0):
                    raise rdtest.TestFailureException("Wrong post mod for red triangle in all {}; got {}, wanted {}".format(i, modif.postMod.col.floatValue, ((i+1)/255.0, 0.0, 0.0, 1.0)))
            else:
                if not rdtest.value_compare(modif.shaderOut.col.floatValue, (0.0, 1.0/255.0, 0.0, 1.0), eps=1.0/256.0):
                    raise rdtest.TestFailureException("Wrong shader output for green triangle in all {}; got {}, wanted {}".format(i, modif.shaderOut.col.floatValue, (0.0, 1.0/255.0, 0.0, 1.0)))
                if i != 254:
                    if not rdtest.value_compare(modif.postMod.col.floatValue, (NUM_TRIANGLES_RED/255.0, (i+1-NUM_TRIANGLES_RED)/255.0, 0.0, 1.0), eps=1.0/256.0):
                        raise rdtest.TestFailureException("Wrong post mod for green triangle in all {}; got {}, wanted {}".format(i, modif.postMod.col.floatValue, (NUM_TRIANGLES_RED/255.0, (i+1-NUM_TRIANGLES_RED)/255.0, 0.0, 1.0)))
                else:
                    # For i = 254 (the last triangle), the post-mod value is always set to the final post-mod value, but everything else is correctly set to the 255th modification
                    if not rdtest.value_compare(modif.postMod.col.floatValue, (NUM_TRIANGLES_RED/255.0, 1.0, NUM_TRIANGLES_BLUE/255.0, 1.0), eps=1.0/256.0):
                        raise rdtest.TestFailureException("Wrong post mod for final (blue) triangle in all {}; got {}, wanted {}".format(i, modif.postMod.col.floatValue, (NUM_TRIANGLES_RED/255.0, 1.0, NUM_TRIANGLES_BLUE/255.0, 1.0)))

    def check_modifs_consistent(self, modifs):
        # postmod of each should match premod of the next
        for i in range(len(modifs) - 1):
            a = value_selector(modifs[i].postMod.col)
            b = value_selector(modifs[i + 1].preMod.col)

            if a != b:
                raise rdtest.TestFailureException(
                    "postmod at {} primitive {}: {} doesn't match premod at {} primitive {}: {}".format(modifs[i].eventId,
                                                                              modifs[i].primitiveID,
                                                                              a,
                                                                              modifs[i + 1].eventId,
                                                                              modifs[i + 1].primitiveID,
                                                                              b))
