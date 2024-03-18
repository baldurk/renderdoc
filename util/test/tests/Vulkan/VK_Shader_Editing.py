import copy
import rdtest
import struct
import renderdoc as rd
from typing import Tuple


class VK_Shader_Editing(rdtest.TestCase):
    demos_test_name = 'VK_Shader_Editing'

    def check_capture(self):
        eid = self.find_action("Draw 1").next.eventId
        self.controller.SetFrameEvent(eid, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        fsrefl1: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Fragment)

        eid = self.find_action("Draw 2").next.eventId
        self.controller.SetFrameEvent(eid, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        fsrefl2: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Fragment)
        vsrefl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Vertex)

        tex: rd.ResourceId = pipe.GetOutputTargets()[0].resource

        # Both triangles should be green
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Values are as expected initially")

        source: str = fsrefl1.debugInfo.files[0].contents.replace('#if 1', '#if 0')

        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader(fsrefl1.entryPoint,
                                                                                 rd.ShaderEncoding.GLSL,
                                                                                 bytes(source, 'UTF-8'),
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Fragment)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited shader: {}".format(newShader[1]))

        FS1 = newShader[0]

        source: str = fsrefl2.debugInfo.files[0].contents.replace('#if 1', '#if 0')

        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader(fsrefl2.entryPoint,
                                                                                 rd.ShaderEncoding.GLSL,
                                                                                 bytes(source, 'UTF-8'),
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Fragment)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited shader: {}".format(newShader[1]))

        FS2 = newShader[0]

        source: str = vsrefl.debugInfo.files[0].contents.replace('Position.xyz', 'Position.xyz+vec3(1.0)')

        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader(vsrefl.entryPoint,
                                                                                 rd.ShaderEncoding.GLSL,
                                                                                 bytes(source, 'UTF-8'),
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Vertex)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited shader: {}".format(newShader[1]))

        offsetVS = newShader[0]

        source: bytes = vsrefl.rawBytes

        self.check(vsrefl.entryPoint == "main")

        # we search-replace in the SPIR-V expecting that 'main' won't appear anywhere other than in the OpEntryPoint
        patched_entry_source = source.replace(b'main', b't_st')

        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader('t_st',
                                                                                 vsrefl.encoding, patched_entry_source,
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Vertex)

        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited shader: {}".format(newShader[1]))

        nochangeVS = newShader[0]

        # Edit both fragment shaders
        self.controller.ReplaceResource(fsrefl1.resourceId, FS1)
        self.controller.ReplaceResource(fsrefl2.resourceId, FS2)

        # Refresh the replay if it didn't happen already
        self.controller.SetFrameEvent(eid, True)

        # Triangles have green and blue channel
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after fragment editing")

        # Now "edit" the VS but don't change it. We should still get the same values
        self.controller.ReplaceResource(vsrefl.resourceId, nochangeVS)
        self.controller.SetFrameEvent(eid, True)

        # Triangles have green and blue channel
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after no-op vertex editing")

        # Change the VS to one that has offset the triangles off-centre
        self.controller.ReplaceResource(vsrefl.resourceId, offsetVS)
        self.controller.SetFrameEvent(eid, True)

        # Original sample positions are now the clear color
        self.check_pixel_value(tex, 0.25, 0.5, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.2, 0.2, 0.2, 1.0])

        # Triangles have green and blue channel
        self.check_pixel_value(tex, 0.45, 0.95, [0.0, 1.0, 1.0, 1.0])
        self.check_pixel_value(tex, 0.95, 0.95, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after offset vertex editing")

        # Now undo the first FS edit
        self.controller.RemoveReplacement(fsrefl1.resourceId)
        self.controller.SetFrameEvent(eid, True)

        # Original sample positions are still the clear color
        self.check_pixel_value(tex, 0.25, 0.5, [0.2, 0.2, 0.2, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.2, 0.2, 0.2, 1.0])

        # The right triangle is the edited colour, the other two have reverted to green channel only
        self.check_pixel_value(tex, 0.45, 0.95, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.95, 0.95, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after removing first fragment edit")

        # Now undo the first VS edit
        self.controller.RemoveReplacement(vsrefl.resourceId)
        self.controller.SetFrameEvent(eid, True)

        # The right triangle is the edited colour, but they are back in the original positions
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 1.0, 1.0])

        rdtest.log.success("Values are as expected after removing vertex edit")

        # finally undo the second FS edit
        self.controller.RemoveReplacement(fsrefl2.resourceId)
        self.controller.SetFrameEvent(eid, True)

        # We should be back to where we started
        self.check_pixel_value(tex, 0.25, 0.5, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(tex, 0.75, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("Values are as expected after removing all edits")

        self.controller.FreeTargetResource(nochangeVS)
        self.controller.FreeTargetResource(offsetVS)
        self.controller.FreeTargetResource(FS1)
        self.controller.FreeTargetResource(FS2)

        bufout = self.get_resource_by_name("bufout").resourceId

        self.controller.SetFrameEvent(self.find_action("Pre-Dispatch").eventId, False)
        pipe: rd.PipeState = self.controller.GetPipelineState()
        csrefl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Compute)

        uints = struct.unpack_from('=4L', self.controller.GetBufferData(bufout, 0, 0), 0)
        if not rdtest.value_compare(uints, [222, 222, 222, 222]):
            raise rdtest.TestFailureException(
                'bufout data is incorrect before dispatch: {}'.format(uints))

        eid = self.find_action("Post-Dispatch").eventId
        self.controller.SetFrameEvent(eid, False)

        uints = struct.unpack_from('=4L', self.controller.GetBufferData(bufout, 0, 0), 0)
        if not rdtest.value_compare(uints, [777, 888, 999, 1110]):
            raise rdtest.TestFailureException(
                'bufout data is incorrect after dispatch: {}'.format(uints))

        self.check(csrefl.debugInfo.encoding == rd.ShaderEncoding.HLSL)
        self.check(csrefl.entryPoint == "hlsl_main")
        rdtest.log.success("Values are as expected before compute shader edits")

        raw_source: bytes = csrefl.rawBytes
        # search-replace in the SPIR-V for 'hlsl_main' replace with a string of the same length
        patched_entry_source = raw_source.replace(b'hlsl_main', b'main_hlsl')
        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader('main_hlsl',
                                                                                 csrefl.encoding, patched_entry_source,
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Compute)
        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited compute shader: {}".format(newShader[1]))
        nochangeCS = newShader[0]
        self.controller.ReplaceResource(csrefl.resourceId, nochangeCS)
        self.controller.SetFrameEvent(eid, False)
        uints = struct.unpack_from('=4L', self.controller.GetBufferData(bufout, 0, 0), 0)
        if not rdtest.value_compare(uints, [777, 888, 999, 1110]):
            raise rdtest.TestFailureException(
                'bufout data is incorrect after dispatch: {}'.format(uints))

        rdtest.log.success("Values are as expected after compute shader entry point change")

        glsl_source = b"""
#version 450 core
layout(push_constant) uniform PushData
{
  uvec4 data;
} push;

layout(binding = 0, std430) buffer inbuftype {
  uvec4 data[];
} inbuf;

layout(binding = 1, std430) buffer outbuftype {
  uvec4 data[];
} outbuf;

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

void main()
{
  outbuf.data[0].x += inbuf.data[0].x * push.data.w;
  outbuf.data[0].y += inbuf.data[0].y * push.data.z;
  outbuf.data[0].z += inbuf.data[0].z * push.data.y;
  outbuf.data[0].w += inbuf.data[0].w * push.data.x;
}
"""
        newShader: Tuple[rd.ResourceId, str] = self.controller.BuildTargetShader('main',
                                                                                 rd.ShaderEncoding.GLSL, 
                                                                                 glsl_source,
                                                                                 rd.ShaderCompileFlags(),
                                                                                 rd.ShaderStage.Compute)
        if len(newShader[1]) != 0:
            raise rdtest.TestFailureException("Failed to compile edited compute shader: {}".format(newShader[1]))
        CS1 = newShader[0]
        self.controller.ReplaceResource(csrefl.resourceId, CS1)
        self.controller.SetFrameEvent(eid, False)
        uints = struct.unpack_from('=4L', self.controller.GetBufferData(bufout, 0, 0), 0)
        if not rdtest.value_compare(uints, [1110, 999, 888, 777]):
            raise rdtest.TestFailureException(
                'bufout data is incorrect after dispatch: {}'.format(uints))

        rdtest.log.success("Values are as expected after compute shader edit")

        self.controller.FreeTargetResource(CS1)
        self.controller.FreeTargetResource(nochangeCS)
