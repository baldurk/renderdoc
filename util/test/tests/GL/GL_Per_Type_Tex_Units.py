import struct
from typing import List
import renderdoc as rd
import rdtest


class GL_Per_Type_Tex_Units(rdtest.TestCase):
    demos_test_name = 'GL_Per_Type_Tex_Units'

    def check_capture(self):
        action = self.find_action("Draw")

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        refl = pipe.GetShaderReflection(rd.ShaderStage.Fragment)

        texs = pipe.GetReadOnlyResources(rd.ShaderStage.Fragment)

        if len(texs) != 2:
            raise rdtest.TestFailureException(
                "Expected 2 textures bound, not {}".format(len(texs)))
        
        # find tex2 in the reflection
        idx = [index for index, res in enumerate(refl.readOnlyResources) if res.name == "tex2"][0]

        # find the descriptor access for tex2
        tex2 = [t for t in texs if t.access.index == idx][0]

        # look up the location of the descriptor it accessed
        location = self.controller.GetDescriptorLocations(tex2.access.descriptorStore, [rd.DescriptorRange(tex2.access)])[0]

        # it should be bind 2
        if location.fixedBindNumber != 2:
            raise rdtest.TestFailureException(
                "First texture should be on slot 2, not {}".format(location.fixedBindNumber))

        id = tex2.descriptor.resource

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

        data = self.controller.GetTextureData(id, rd.Subresource(0, 0, 0))
        first_pixel = struct.unpack_from("BBBB", data, 0)

        if not rdtest.value_compare(first_pixel, (255, 0, 0, 255)):
            raise rdtest.TestFailureException("Texture should contain red, not {}".format(first_pixel))

        rdtest.log.success("First texture is as expected")

        # find tex2 in the reflection
        idx = [index for index, res in enumerate(refl.readOnlyResources) if res.name == "tex3"][0]

        # find the descriptor access for tex2
        tex3 = [t for t in texs if t.access.index == idx][0]

        # look up the location of the descriptor it accessed
        location = self.controller.GetDescriptorLocations(tex3.access.descriptorStore, [rd.DescriptorRange(tex3.access)])[0]

        # it should be bind 2
        if location.fixedBindNumber != 3:
            raise rdtest.TestFailureException(
                "Second texture should be on slot 3, not {}".format(location.fixedBindNumber))

        id = tex3.descriptor.resource

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

        data = self.controller.GetTextureData(id, rd.Subresource(0, 0, 0))
        first_pixel = struct.unpack_from("BBBB", data, 0)

        if not rdtest.value_compare(first_pixel, (0, 255, 0, 255)):
            raise rdtest.TestFailureException("Texture should contain green, not {}".format(first_pixel))

        rdtest.log.success("Second texture is as expected")

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, [1.0, 1.0, 0.0, 0.2])

        rdtest.log.success("Picked value is as expected")
