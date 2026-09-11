import rdtest
import renderdoc as rd


class D3D11_Shader_Editing(rdtest.TestCase):
    demos_test_name = 'D3D11_Shader_Editing'

    def check_capture(self):
        eid = self.find_action("Draw 1").nextAction.eventId
        self.set_event(eid, False)

        pipe = self.controller.GetPipelineState()

        psrefl1 = pipe.GetShaderReflection(rd.ShaderStage.Pixel)

        eid = self.find_action("Draw 2").nextAction.eventId
        self.set_event(eid, False)

        pipe = self.controller.GetPipelineState()

        psrefl2 = pipe.GetShaderReflection(rd.ShaderStage.Pixel)
        vsrefl = pipe.GetShaderReflection(rd.ShaderStage.Vertex)

        tex = pipe.GetOutputTargets()[0].resource

        # Both triangles should be green
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Values are as expected initially")

        source = psrefl1.debugInfo.files[0].contents.replace('#if 1', '#if 0')

        newShader = self.controller.BuildTargetShader(
            psrefl1.entryPoint,
            rd.ShaderEncoding.HLSL,
            bytes(source, "UTF-8"),
            rd.ShaderCompileFlags(),
            rd.ShaderStage.Pixel,
        )

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException(
                f"Failed to compile edited shader: {newShader[1]}"
            )

        ps1 = newShader[0]

        source = psrefl2.debugInfo.files[0].contents.replace("#if 1", "#if 0")

        newShader = self.controller.BuildTargetShader(
            psrefl2.entryPoint,
            rd.ShaderEncoding.HLSL,
            bytes(source, "UTF-8"),
            rd.ShaderCompileFlags(),
            rd.ShaderStage.Pixel,
        )

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException(
                f"Failed to compile edited shader: {newShader[1]}"
            )

        ps2 = newShader[0]

        source = vsrefl.debugInfo.files[0].contents.replace(
            "INpos.xyz", "INpos.xyz+float3(1,1,1)"
        )

        newShader = self.controller.BuildTargetShader(
            vsrefl.entryPoint,
            rd.ShaderEncoding.HLSL,
            bytes(source, "UTF-8"),
            rd.ShaderCompileFlags(),
            rd.ShaderStage.Vertex,
        )

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException(
                f"Failed to compile edited shader: {newShader[1]}"
            )

        offsetVS = newShader[0]

        source_bytes = vsrefl.rawBytes

        newShader = self.controller.BuildTargetShader(
            vsrefl.entryPoint,
            vsrefl.encoding,
            source_bytes,
            rd.ShaderCompileFlags(),
            rd.ShaderStage.Vertex,
        )

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException(f"Failed to compile edited shader: {newShader[1]}")

        nochangeVS = newShader[0]

        # Edit both Pixel shaders
        self.replace_resource(psrefl1.resourceId, ps1)
        self.replace_resource(psrefl2.resourceId, ps2)

        # Refresh the replay if it didn't happen already
        self.set_event(eid, True)

        # Triangles have green and blue channel
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after Pixel editing")

        # Now "edit" the VS but don't change it. We should still get the same values
        self.replace_resource(vsrefl.resourceId, nochangeVS)
        self.set_event(eid, True)

        # Triangles have green and blue channel
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after no-op vertex editing")

        # Change the VS to one that has ofpset the triangles off-centre
        self.replace_resource(vsrefl.resourceId, offsetVS)
        self.set_event(eid, True)

        # Original sample positions are now the clear color
        self.check_pixel_value(tex, 0.25, 0.5, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.2, 0.2, 0.2, 1.0])

        # Triangles have green and blue channel
        self.check_pixel_value(tex, 0.45, 0.05, [0.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(tex, 0.95, 0.05, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after ofpset vertex editing")

        # Now undo the first ps edit
        self.remove_replacement(psrefl1.resourceId)
        self.set_event(eid, True)

        # Original sample positions are still the clear color
        self.check_pixel_value(tex, 0.25, 0.5, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.2, 0.2, 0.2, 1.0])

        # The right triangle is the edited colour, the other two have reverted to green channel only
        self.check_pixel_value(tex, 0.45, 0.05, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.95, 0.05, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after removing first Pixel edit")

        # Now undo the first VS edit
        self.remove_replacement(vsrefl.resourceId)
        self.set_event(eid, True)

        # The right triangle is the edited colour, but they are back in the original positions
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after removing vertex edit")

        # finally undo the second ps edit
        self.remove_replacement(psrefl2.resourceId)
        self.set_event(eid, True)

        # We should be back to where we started
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Values are as expected after removing all edits")

        self.controller.FreeTargetResource(nochangeVS)
        self.controller.FreeTargetResource(offsetVS)
        self.controller.FreeTargetResource(ps1)
        self.controller.FreeTargetResource(ps2)
