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
        apiprops = self.controller.GetAPIProperties()

        if not apiprops.pixelHistory:
            rdtest.log.print("Vulkan pixel history not tested")
            return

        self.primary_test()

    def primary_test(self):
        test_marker = self.find_action("Test End")
        self.controller.SetFrameEvent(test_marker.eventId, True)

        pipe = self.controller.GetPipelineState()

        rt = pipe.GetOutputTargets()[0]

        tex = rt.resource
        tex_details = self.get_texture(tex)

        sub = rd.Subresource()
        if tex_details.arraysize > 1:
            sub.slice = rt.firstSlice
        if tex_details.mips > 1:
            sub.mip = rt.firstMip

        red_eid = self.find_action("Red: ").nextAction.eventId
        red_last_eid = self.find_action("End of red").nextAction.eventId
        green_eid = self.find_action("Green: ").nextAction.eventId
        blue_eid = self.find_action("Blue: ").nextAction.eventId
        all_eid = self.find_action("All of the above in a single drawcall").nextAction.eventId

        # Pixel inside of all of the triangles
        x, y = 200, 150
        rdtest.log.print(f"Testing pixel {x}, {y}")
        modifs = self.controller.PixelHistory(tex, x, y, sub, rt.format.compType)
        self.check_modifs_consistent(modifs)
        red_modifs = [m for m in modifs if m.eventId >= red_eid and m.eventId < red_last_eid]
        green_modifs = [m for m in modifs if m.eventId == green_eid]
        blue_modifs = [m for m in modifs if m.eventId == blue_eid]
        all_modifs = [m for m in modifs if m.eventId == all_eid]

        if len(red_modifs) != NUM_TRIANGLES_RED_REAL:
            raise rdtest.TestFailureException(f"Expected {NUM_TRIANGLES_RED_REAL} modifications for red triangles (EIDS {red_eid} until {red_last_eid}) but got {len(red_modifs)}")

        for i, modif in enumerate(red_modifs):
            if not rdtest.value_compare(modif.shaderOut.col.floatValue, (1.0/255.0, 0.0, 0.0, 1.0), eps=1.0/256.0):
                raise rdtest.TestFailureException(f"Wrong shader output for red triangle {i}; got {modif.shaderOut.col.floatValue}, wanted {(1.0 / 255.0, 0.0, 0.0, 1.0)}")
            if not rdtest.value_compare(modif.postMod.col.floatValue, ((i+1)/255.0, 0.0, 0.0, 1.0), eps=1.0/256.0):
                raise rdtest.TestFailureException(f"Wrong post mod for red triangle {i}; got {modif.postMod.col.floatValue}, wanted {((i + 1) / 255.0, 0.0, 0.0, 1.0)}")

        i = 1
        eid_counter = 0
        modif_counter = 0
        while i <= NUM_TRIANGLES_RED:
            for primitive_id in range(i):
                if red_modifs[modif_counter].eventId != red_eid + eid_counter:
                    raise rdtest.TestFailureException(f"Expected red triangle {modif_counter} to be part of EID {red_eid + eid_counter} but was {red_modifs[modif_counter].eventId}")
                if red_modifs[modif_counter].primitiveID != primitive_id:
                    raise rdtest.TestFailureException(f"Expected red triangle {modif_counter} to have primitive ID {primitive_id} but was {red_modifs[modif_counter].primitiveID}")
                modif_counter += 1
            eid_counter += 1
            i *= 2

        if len(green_modifs) != NUM_TRIANGLES_GREEN:
            raise rdtest.TestFailureException(f"Expected {NUM_TRIANGLES_GREEN} modifications for green triangles (EID {green_eid}) but got {len(green_modifs)}")

        for i, modif in enumerate(green_modifs):
            if modif.primitiveID != i:
                raise rdtest.TestFailureException(f"Expected green triangle {i} to have primitive ID {modif.primitiveID} but was {modif.primitiveID}")
            if not rdtest.value_compare(modif.shaderOut.col.floatValue, (0.0, 1.0/255.0, 0.0, 1.0), eps=1.0/256.0):
                raise rdtest.TestFailureException(f"Wrong shader output for green triangle {i}; got {modif.shaderOut.col.floatValue}, wanted {(0.0, 1.0 / 255.0, 0.0, 1.0)}")
            if not rdtest.value_compare(modif.postMod.col.floatValue, (NUM_TRIANGLES_RED_REAL/255.0, (i+1)/255.0, 0.0, 1.0), eps=1.0/256.0):
                raise rdtest.TestFailureException(f"Wrong post mod for green triangle {i}; got {modif.postMod.col.floatValue}, wanted {(NUM_TRIANGLES_RED_REAL / 255.0, (i + 1) / 255.0, 0.0, 1.0)}")

        # We can only record 255 modifications due to the stencil format
        if len(blue_modifs) != 255:
            raise rdtest.TestFailureException(f"Expected {255} modifications for blue triangles (EID {blue_eid}) but got {len(blue_modifs)}")

        for i, modif in enumerate(blue_modifs):
            if modif.primitiveID != i:
                raise rdtest.TestFailureException(f"Expected blue triangle {i} to have primitive ID {modif.primitiveID} but was {modif.primitiveID}")
            if not rdtest.value_compare(modif.shaderOut.col.floatValue, (0.0, 0.0, 1.0/255.0, 1.0), eps=1.0/256.0):
                raise rdtest.TestFailureException(f"Wrong shader output for blue triangle {i}; got {modif.shaderOut.col.floatValue}, wanted {(0.0, 0.0, 1.0 / 255.0, 1.0)}")
            if i == 254:
                if not rdtest.value_compare(modif.postMod.col.floatValue, (NUM_TRIANGLES_RED_REAL/255.0, 1.0, NUM_TRIANGLES_BLUE/255.0, 1.0), eps=1.0/256.0):
                    raise rdtest.TestFailureException(f"Wrong post mod for final blue triangle {i}; got {modif.postMod.col.floatValue}, wanted {(NUM_TRIANGLES_RED_REAL / 255.0, 1.0, NUM_TRIANGLES_BLUE / 255.0, 1.0)}")
            else:
                if not rdtest.value_compare(modif.postMod.col.floatValue, (NUM_TRIANGLES_RED_REAL/255.0, 1.0, (i+1)/255.0, 1.0), eps=1.0/256.0):
                    raise rdtest.TestFailureException(f"Wrong post mod for blue triangle {i}; got {modif.postMod.col.floatValue}, wanted {(NUM_TRIANGLES_RED_REAL / 255.0, 1.0, (i + 1) / 255.0, 1.0)}")

        # Once again, we can only record 255 modifications due to the stencil format
        if len(all_modifs) != 255:
            raise rdtest.TestFailureException(f"Expected {255} modifications for all triangles (EID {all_eid}) but got {len(all_modifs)}")

        for i, modif in enumerate(all_modifs):
            if modif.primitiveID != i:
                raise rdtest.TestFailureException(f"Expected triangle {i} in all to have primitive ID {modif.primitiveID} but was {modif.primitiveID}")

            if i < NUM_TRIANGLES_RED:
                if not rdtest.value_compare(modif.shaderOut.col.floatValue, (1.0/255.0, 0.0, 0.0, 1.0), eps=1.0/256.0):
                    raise rdtest.TestFailureException(f"Wrong shader output for red triangle in all {i}; got {modif.shaderOut.col.floatValue}, wanted {(1.0 / 255.0, 0.0, 0.0, 1.0)}")
                if not rdtest.value_compare(modif.postMod.col.floatValue, ((i+1)/255.0, 0.0, 0.0, 1.0), eps=1.0/256.0):
                    raise rdtest.TestFailureException(f"Wrong post mod for red triangle in all {i}; got {modif.postMod.col.floatValue}, wanted {((i + 1) / 255.0, 0.0, 0.0, 1.0)}")
            else:
                if not rdtest.value_compare(modif.shaderOut.col.floatValue, (0.0, 1.0/255.0, 0.0, 1.0), eps=1.0/256.0):
                    raise rdtest.TestFailureException(f"Wrong shader output for green triangle in all {i}; got {modif.shaderOut.col.floatValue}, wanted {(0.0, 1.0 / 255.0, 0.0, 1.0)}")
                if i != 254:
                    if not rdtest.value_compare(modif.postMod.col.floatValue, (NUM_TRIANGLES_RED/255.0, (i+1-NUM_TRIANGLES_RED)/255.0, 0.0, 1.0), eps=1.0/256.0):
                        raise rdtest.TestFailureException(f"Wrong post mod for green triangle in all {i}; got {modif.postMod.col.floatValue}, wanted {(NUM_TRIANGLES_RED / 255.0, (i + 1 - NUM_TRIANGLES_RED) / 255.0, 0.0, 1.0)}")
                else:
                    # For i = 254 (the last triangle), the post-mod value is always set to the final post-mod value, but everything else is correctly set to the 255th modification
                    if not rdtest.value_compare(modif.postMod.col.floatValue, (NUM_TRIANGLES_RED/255.0, 1.0, NUM_TRIANGLES_BLUE/255.0, 1.0), eps=1.0/256.0):
                        raise rdtest.TestFailureException(f"Wrong post mod for final (blue) triangle in all {i}; got {modif.postMod.col.floatValue}, wanted {(NUM_TRIANGLES_RED / 255.0, 1.0, NUM_TRIANGLES_BLUE / 255.0, 1.0)}")

    def check_modifs_consistent(self, modifs):
        # postmod of each should match premod of the next
        for i in range(len(modifs) - 1):
            a = value_selector(modifs[i].postMod.col)
            b = value_selector(modifs[i + 1].preMod.col)

            if a != b:
                raise rdtest.TestFailureException(
                    f"postmod at {modifs[i].eventId} primitive {modifs[i].primitiveID}: {a} doesn't match premod at {modifs[i + 1].eventId} primitive {modifs[i + 1].primitiveID}: {b}")
