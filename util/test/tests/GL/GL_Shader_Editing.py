import copy
import rdtest
import renderdoc as rd
from typing import Tuple


class GL_Shader_Editing(rdtest.TestCase):
    demos_test_name = 'GL_Shader_Editing'

    def check_capture(self):
        eid = self.find_action("fixedprog").eventId
        self.controller.SetFrameEvent(eid, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        fixedrefl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Fragment)

        eid = self.find_action("dynamicprog").eventId
        self.controller.SetFrameEvent(eid, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        dynamicrefl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Fragment)
        vsrefl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Vertex)

        eid = self.find_action("sepprog").eventId
        self.controller.SetFrameEvent(eid, False)

        vsseprefl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Vertex)
        fsseprefl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Fragment)

        # Work at the last action, where the uniforms have been trashed
        self.controller.SetFrameEvent(self.get_last_action().eventId, False)

        tex: rd.ResourceId = pipe.GetOutputTargets()[0].resource

        # On upper row: Left triangle is fully green, right triangle is half-green
        # On lower row: Left triangle is fully green
        self.check_pixel_value(tex, 0.25, 0.25, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.25, [0.0, 0.5, 0.0, 1.0])
        self.check_pixel_value(tex, 0.25, 0.75, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Values are as expected initially")

        source: bytes = fixedrefl.rawBytes.replace(b'.rgba', b'.rgga').replace(b'location = 9', b'location = 10')

        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader(fixedrefl.entryPoint,
                                                                                 fixedrefl.encoding, source,
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Fragment)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited shader: {}".format(newShader[1]))

        fixedFS = newShader[0]

        source: bytes = dynamicrefl.rawBytes.replace(b'.rgba', b'.rgga').replace(b'#if 1', b'#if 0')

        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader(dynamicrefl.entryPoint,
                                                                                 dynamicrefl.encoding, source,
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Fragment)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited shader: {}".format(newShader[1]))

        dynamicFS = newShader[0]

        source: bytes = vsrefl.rawBytes.replace(b'Position.xyz', b'Position.xyz+vec3(1.0)')

        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader(vsrefl.entryPoint,
                                                                                 vsrefl.encoding, source,
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

        source: bytes = vsseprefl.rawBytes.replace(b'Position.xyz', b'Position.xyz+vec3(1.0)')

        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader(vsseprefl.entryPoint,
                                                                                 vsseprefl.encoding, source,
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Vertex)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited shader: {}".format(newShader[1]))

        sepVS = newShader[0]

        source: bytes = fsseprefl.rawBytes.replace(b'.rgba', b'.rgga')

        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader(fsseprefl.entryPoint,
                                                                                 fsseprefl.encoding, source,
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Fragment)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited shader: {}".format(newShader[1]))

        sepFS = newShader[0]

        # Edit both fragment shaders
        self.controller.ReplaceResource(fixedrefl.resourceId, fixedFS)
        self.controller.ReplaceResource(dynamicrefl.resourceId, dynamicFS)

        # Refresh the replay if it didn't happen already
        self.controller.SetFrameEvent(self.get_last_action().eventId, True)

        # Triangles have green propagated across to the blue channel
        self.check_pixel_value(tex, 0.25, 0.25, [0.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.25, [0.0, 0.5, 0.5, 1.0])
        self.check_pixel_value(tex, 0.25, 0.75, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after fragment editing")

        # Now "edit" the VS but don't change it. We should still get the same values
        self.controller.ReplaceResource(vsrefl.resourceId, nochangeVS)
        self.controller.SetFrameEvent(self.get_last_action().eventId, True)

        # Triangles have green propagated across to the blue channel
        self.check_pixel_value(tex, 0.25, 0.25, [0.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.25, [0.0, 0.5, 0.5, 1.0])
        self.check_pixel_value(tex, 0.25, 0.75, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after no-op vertex editing")

        # Change the VS to one that has offset the triangles off-centre
        self.controller.ReplaceResource(vsrefl.resourceId, offsetVS)
        self.controller.SetFrameEvent(self.get_last_action().eventId, True)

        # Original sample positions are now the clear color
        self.check_pixel_value(tex, 0.25, 0.25, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(tex, 0.75, 0.25, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(tex, 0.25, 0.75, [0.2, 0.2, 0.2, 1.0])

        # The triangles are still the same colour but up and to the right
        self.check_pixel_value(tex, 0.45, 0.05, [0.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(tex, 0.95, 0.05, [0.0, 0.5, 0.5, 1.0])
        self.check_pixel_value(tex, 0.45, 0.55, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after offset vertex editing")

        # Now undo the first FS edit
        self.controller.RemoveReplacement(fixedrefl.resourceId)
        self.controller.SetFrameEvent(self.get_last_action().eventId, True)

        # Original sample positions are still the clear color
        self.check_pixel_value(tex, 0.25, 0.25, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(tex, 0.75, 0.25, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(tex, 0.25, 0.75, [0.2, 0.2, 0.2, 1.0])

        # The lower triangle is the edited colour, the other two have reverted to green channel only
        self.check_pixel_value(tex, 0.45, 0.05, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.95, 0.05, [0.0, 0.5, 0.0, 1.0])
        self.check_pixel_value(tex, 0.45, 0.55, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after removing first fragment edit")

        # Now undo the first VS edit
        self.controller.RemoveReplacement(vsrefl.resourceId)
        self.controller.SetFrameEvent(self.get_last_action().eventId, True)

        # Only the lower triangle is the edited colour, but they are back in the original positions
        self.check_pixel_value(tex, 0.25, 0.25, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.25, [0.0, 0.5, 0.0, 1.0])
        self.check_pixel_value(tex, 0.25, 0.75, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after removing vertex edit")

        # finally undo the second FS edit
        self.controller.RemoveReplacement(dynamicrefl.resourceId)
        self.controller.SetFrameEvent(self.get_last_action().eventId, True)

        # We should be back to where we started
        self.check_pixel_value(tex, 0.25, 0.25, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.25, [0.0, 0.5, 0.0, 1.0])
        self.check_pixel_value(tex, 0.25, 0.75, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Values are as expected after removing all edits")

        rdtest.log.success("Linked program editing succeeded")

        # Check that we can edit separable shaders

        # Only looking at bottom left triangle, it should be green
        self.check_pixel_value(tex, 0.75, 0.75, [0.0, 1.0, 0.0, 1.0])

        self.controller.ReplaceResource(fsseprefl.resourceId, sepFS)
        self.controller.SetFrameEvent(self.get_last_action().eventId, True)

        # Now it should be green-blue
        self.check_pixel_value(tex, 0.75, 0.75, [0.0, 1.0, 1.0, 1.0])

        self.controller.ReplaceResource(vsseprefl.resourceId, sepVS)
        self.controller.SetFrameEvent(self.get_last_action().eventId, True)

        # Now it should be green-blue and offset
        self.check_pixel_value(tex, 0.75, 0.75, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(tex, 0.95, 0.55, [0.0, 1.0, 1.0, 1.0])

        self.controller.RemoveReplacement(fsseprefl.resourceId)
        self.controller.SetFrameEvent(self.get_last_action().eventId, True)

        # Now it should be back to green and offset
        self.check_pixel_value(tex, 0.75, 0.75, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(tex, 0.95, 0.55, [0.0, 1.0, 0.0, 1.0])

        self.controller.RemoveReplacement(vsseprefl.resourceId)
        self.controller.SetFrameEvent(self.get_last_action().eventId, True)

        # We should be back to where we started
        self.check_pixel_value(tex, 0.75, 0.75, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Separable program editing succeeded")

        self.controller.FreeTargetResource(nochangeVS)
        self.controller.FreeTargetResource(offsetVS)
        self.controller.FreeTargetResource(fixedFS)
        self.controller.FreeTargetResource(dynamicFS)
        self.controller.FreeTargetResource(sepVS)
        self.controller.FreeTargetResource(sepFS)
