import copy
import rdtest
import renderdoc as rd
from typing import Tuple


class D3D11_Shader_Editing(rdtest.TestCase):
    demos_test_name = 'D3D11_Shader_Editing'

    def check_capture(self):
        eid = self.find_action("Draw 1").next.eventId
        self.controller.SetFrameEvent(eid, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        psrefl1: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Pixel)

        eid = self.find_action("Draw 2").next.eventId
        self.controller.SetFrameEvent(eid, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        psrefl2: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Pixel)
        vsrefl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Vertex)

        tex: rd.ResourceId = pipe.GetOutputTargets()[0].resource

        # Both triangles should be green
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Values are as expected initially")

        source: str = psrefl1.debugInfo.files[0].contents.replace('#if 1', '#if 0')

        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader(psrefl1.entryPoint,
                                                                                 rd.ShaderEncoding.HLSL,
                                                                                 bytes(source, 'UTF-8'),
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Pixel)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited shader: {}".format(newShader[1]))

        ps1 = newShader[0]

        source: str = psrefl2.debugInfo.files[0].contents.replace('#if 1', '#if 0')

        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader(psrefl2.entryPoint,
                                                                                 rd.ShaderEncoding.HLSL,
                                                                                 bytes(source, 'UTF-8'),
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Pixel)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited shader: {}".format(newShader[1]))

        ps2 = newShader[0]

        source: str = vsrefl.debugInfo.files[0].contents.replace('INpos.xyz', 'INpos.xyz+float3(1,1,1)')

        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader(vsrefl.entryPoint,
                                                                                 rd.ShaderEncoding.HLSL,
                                                                                 bytes(source, 'UTF-8'),
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Vertex)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited shader: {}".format(newShader[1]))

        offsetVS = newShader[0]

        source: bytes = vsrefl.rawBytes

        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader(vsrefl.entryPoint,
                                                                                 vsrefl.encoding, source,
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Vertex)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited shader: {}".format(newShader[1]))

        nochangeVS = newShader[0]

        # Edit both Pixel shaders
        self.controller.ReplaceResource(psrefl1.resourceId, ps1)
        self.controller.ReplaceResource(psrefl2.resourceId, ps2)

        # Refresh the replay if it didn't happen already
        self.controller.SetFrameEvent(eid, True)

        # Triangles have green and blue channel
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after Pixel editing")

        # Now "edit" the VS but don't change it. We should still get the same values
        self.controller.ReplaceResource(vsrefl.resourceId, nochangeVS)
        self.controller.SetFrameEvent(eid, True)

        # Triangles have green and blue channel
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after no-op vertex editing")

        # Change the VS to one that has ofpset the triangles off-centre
        self.controller.ReplaceResource(vsrefl.resourceId, offsetVS)
        self.controller.SetFrameEvent(eid, True)

        # Original sample positions are now the clear color
        self.check_pixel_value(tex, 0.25, 0.5, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.2, 0.2, 0.2, 1.0])

        # Triangles have green and blue channel
        self.check_pixel_value(tex, 0.45, 0.05, [0.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(tex, 0.95, 0.05, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after ofpset vertex editing")

        # Now undo the first ps edit
        self.controller.RemoveReplacement(psrefl1.resourceId)
        self.controller.SetFrameEvent(eid, True)

        # Original sample positions are still the clear color
        self.check_pixel_value(tex, 0.25, 0.5, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.2, 0.2, 0.2, 1.0])

        # The right triangle is the edited colour, the other two have reverted to green channel only
        self.check_pixel_value(tex, 0.45, 0.05, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.95, 0.05, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after removing first Pixel edit")

        # Now undo the first VS edit
        self.controller.RemoveReplacement(vsrefl.resourceId)
        self.controller.SetFrameEvent(eid, True)

        # The right triangle is the edited colour, but they are back in the original positions
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after removing vertex edit")

        # finally undo the second ps edit
        self.controller.RemoveReplacement(psrefl2.resourceId)
        self.controller.SetFrameEvent(eid, True)

        # We should be back to where we started
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Values are as expected after removing all edits")

        self.controller.FreeTargetResource(nochangeVS)
        self.controller.FreeTargetResource(offsetVS)
        self.controller.FreeTargetResource(ps1)
        self.controller.FreeTargetResource(ps2)
