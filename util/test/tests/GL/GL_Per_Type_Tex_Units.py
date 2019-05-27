import struct
from typing import List
import renderdoc as rd
import rdtest


class GL_Per_Type_Tex_Units(rdtest.TestCase):
    demos_test_name = 'GL_Per_Type_Tex_Units'

    def check_capture(self):
        # Make an output so we can pick pixels
        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        self.check(out is not None)

        draw = self.find_draw("Draw")

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        bind: rd.ShaderBindpointMapping = pipe.GetBindpointMapping(rd.ShaderStage.Fragment)
        texs: List[rd.BoundResourceArray] = pipe.GetReadOnlyResources(rd.ShaderStage.Fragment)

        if len(bind.readOnlyResources) != 2:
            raise rdtest.TestFailureException(
                "Expected 2 textures bound, not {}".format(len(bind.readOnlyResources)))

        if bind.readOnlyResources[0].bind != 2:
            raise rdtest.TestFailureException(
                "First texture should be on slot 2, not {}".format(bind.readOnlyResources[0].bind))

        id = texs[2].resources[0].resourceId

        tex_details = self.get_texture(id)
        res_details = self.get_resource(id)

        if res_details.name != "Red 2D":
            raise rdtest.TestFailureException("First texture should be Red 2D texture, not {}".format(res_details.name))

        if tex_details.dimension != 2:
            raise rdtest.TestFailureException(
                "First texture should be 2D texture, not {}".format(tex_details.dimension))

        if tex_details.width != 8 or tex_details.height != 8:
            raise rdtest.TestFailureException(
                "First texture should be 8x8, not {}x{}".format(tex_details.width, tex_details.height))

        data = self.controller.GetTextureData(id, 0, 0)
        first_pixel = struct.unpack_from("BBBB", data, 0)

        if not rdtest.value_compare(first_pixel, (255, 0, 0, 255)):
            raise rdtest.TestFailureException("Texture should contain red, not {}".format(first_pixel))

        rdtest.log.success("First texture is as expected")

        if bind.readOnlyResources[1].bind != 3:
            raise rdtest.TestFailureException(
                "First texture should be on slot 3, not {}".format(texs[0].bindPoint.bind))

        id = texs[3].resources[0].resourceId

        tex_details = self.get_texture(id)
        res_details = self.get_resource(id)

        if res_details.name != "Green 3D":
            raise rdtest.TestFailureException(
                "First texture should be Green 3D texture, not {}".format(res_details.name))

        if tex_details.dimension != 3:
            raise rdtest.TestFailureException(
                "First texture should be 3D texture, not {}".format(tex_details.dimension))

        if tex_details.width != 4 or tex_details.height != 4 or tex_details.depth != 4:
            raise rdtest.TestFailureException(
                "First texture should be 4x4x4, not {}x{}x{}".format(tex_details.width, tex_details.height,
                                                                     tex_details.depth))

        data = self.controller.GetTextureData(id, 0, 0)
        first_pixel = struct.unpack_from("BBBB", data, 0)

        if not rdtest.value_compare(first_pixel, (0, 255, 0, 255)):
            raise rdtest.TestFailureException("Texture should contain green, not {}".format(first_pixel))

        rdtest.log.success("Second texture is as expected")

        tex = rd.TextureDisplay()
        tex.resourceId = pipe.GetOutputTargets()[0].resourceId
        out.SetTextureDisplay(tex)

        tex_details = self.get_texture(tex.resourceId)

        picked: rd.PixelValue = out.PickPixel(tex.resourceId, False,
                                              int(tex_details.width / 2), int(tex_details.height / 2), 0, 0, 0)

        if not rdtest.value_compare(picked.floatValue, [1.0, 1.0, 0.0, 0.2]):
            raise rdtest.TestFailureException("Picked value {} doesn't match expectation".format(picked.floatValue))

        rdtest.log.success("Picked value is as expected")

        out.Shutdown()

