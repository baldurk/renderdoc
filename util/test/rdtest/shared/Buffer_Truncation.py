import renderdoc as rd
import rdtest


# Not a direct test, re-used by API-specific tests
class Buffer_Truncation(rdtest.TestCase):
    internal = True

    def check_capture(self):
        action = self.find_action("Draw")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, False)

        vsin_ref = {
            0: {
                'vtx': 0,
                'idx': 1,
                'POSITION': [-0.5, -0.5, 0.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 2,
                'POSITION': [0.0, 0.5, 0.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 3,
                'POSITION': [0.5, -0.5, 0.0],
                'COLOR': [0.0, 1.0, 0.0, 1.0],
            },
            3: {
                'vtx': 3,
                'idx': 4,
                'POSITION': [8.8, 0.0, 0.0],
                'COLOR': [0.0, 0.0, 0.0, 1.0],
            },
            4: {
                'vtx': 4,
                'idx': 5,
                'POSITION': None,
                'COLOR': None,
            },
            5: {
                'vtx': 5,
                'idx': None,
                'POSITION': None,
                'COLOR': None,
            },
        }

        self.check_mesh_data(vsin_ref, self.get_vsin(action))

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 1,
                'OUTPOSITION': [-0.5, -0.5, 0.0, 1.0],
                'OUTCOLOR': [0.0, 1.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 2,
                'OUTPOSITION': [0.0, 0.5, 0.0, 1.0],
                'OUTCOLOR': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 3,
                'OUTPOSITION': [0.5, -0.5, 0.0, 1.0],
                'OUTCOLOR': [0.0, 1.0, 0.0, 1.0],
            },
            3: {
                'vtx': 3,
                'idx': 4,
                'OUTPOSITION': [8.8, 0.0, 0.0, 1.0],
                'OUTCOLOR': [0.0, 0.0, 0.0, 1.0],
            },
            4: {
                'vtx': 4,
                'idx': 5,
                # don't rely on any particular OOB behaviour for postvs, as this may come from the driver/API
            },
            5: {
                'vtx': 5,
                'idx': None,
                # don't rely on any particular OOB behaviour for postvs, as this may come from the driver/API
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        rdtest.log.success("vertex/index buffers were truncated as expected")

        pipe: rd.PipeState = self.controller.GetPipelineState()

        stage = rd.ShaderStage.Pixel

        cbuf = pipe.GetConstantBlock(stage, 0, 0).descriptor

        if self.find_action('NoCBufferRange') == None:
            self.check(cbuf.byteSize == 256)

        variables = self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                               pipe.GetShader(stage), stage,
                                                               pipe.GetShaderEntryPoint(stage), 0,
                                                               cbuf.resource, cbuf.byteOffset, cbuf.byteSize)

        outcol: rd.ShaderVariable = variables[1]

        self.check(outcol.name == "outcol")
        if not rdtest.value_compare(outcol.value.f32v[0:4], [0.0, 0.0, 0.0, 0.0]):
            raise rdtest.TestFailureException("expected outcol to be 0s, but got {}".format(outcol.value.f32v[0:4]))

        if self.controller.GetAPIProperties().shaderDebugging and pipe.GetShaderReflection(
                rd.ShaderStage.Pixel).debugInfo.debuggable:
            # Debug the shader
            trace: rd.ShaderDebugTrace = self.controller.DebugPixel(int(pipe.GetViewport(0).width/2),
                                                                    int(pipe.GetViewport(0).height/2),
                                                                    rd.DebugPixelInputs())

            if trace.debugger is None:
                self.controller.FreeTrace(trace)
                raise rdtest.TestFailureException("Shader did not debug at all")
            else:
                cycles, variables = self.process_trace(trace)

                cbuf_sourceVars = [s for s in trace.sourceVars if s.variables[0].type == rd.DebugVariableType.Constant and s.rows > 0]

                # Vulkan style, one source var for the cbuffer
                if len(cbuf_sourceVars) == 1:
                    debugged_cb = trace.constantBlocks[0]

                    self.check(debugged_cb.members[0].name == 'padding')
                    self.check(debugged_cb.members[1].name == 'outcol')

                    if not rdtest.value_compare(debugged_cb.members[1].value.f32v[0:4], [0.0, 0.0, 0.0, 0.0]):
                        raise rdtest.TestFailureException("expected outcol to be 0s, but got {}".format(debugged_cb.members[1].value.f32v[0:4]))
                # D3D style, one source var for each member mapping to a register
                elif len(cbuf_sourceVars) == 17:
                    debugged_cb = trace.constantBlocks[0].members[16]

                    self.check(all(['consts.padding[' in c.name for c in cbuf_sourceVars[0:16]]))
                    self.check(cbuf_sourceVars[16].name == 'consts.outcol')

                    self.check(cbuf_sourceVars[16].variables[0].name == 'cb0[16]')

                    if not rdtest.value_compare(debugged_cb.value.f32v[0:4], [0.0, 0.0, 0.0, 0.0]):
                        raise rdtest.TestFailureException("expected outcol to be 0s, but got {}".format(debugged_cb.members[1].value.f32v[0:4]))
                else:
                    raise rdtest.TestFailureException("Unexpected number of constant buffer source vars {}".format(len(cbuf_sourceVars)))

        rdtest.log.success("CBuffer value was truncated as expected")
