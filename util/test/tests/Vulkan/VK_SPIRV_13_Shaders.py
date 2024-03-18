import renderdoc as rd
import rdtest


class VK_SPIRV_13_Shaders(rdtest.TestCase):
    demos_test_name = 'VK_SPIRV_13_Shaders'

    def check_capture(self):
        action = self.find_action("Draw")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        refl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Vertex)

        disasm: str = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), refl, "")

        if (refl.inputSignature[0].varName != 'pos' or refl.inputSignature[0].compCount != 3):
            raise rdtest.TestFailureException("Vertex shader input 'pos' not reflected correctly")
        if (refl.inputSignature[1].varName != 'col' or refl.inputSignature[1].compCount != 4):
            raise rdtest.TestFailureException("Vertex shader input 'col' not reflected correctly")
        if (refl.inputSignature[2].varName != 'uv' or refl.inputSignature[2].compCount != 2):
            raise rdtest.TestFailureException("Vertex shader input 'uv' not reflected correctly")

        if (refl.outputSignature[0].varName != 'opos' or refl.outputSignature[0].compCount != 4 or refl.outputSignature[0].systemValue != rd.ShaderBuiltin.Position):
            raise rdtest.TestFailureException("Vertex shader output 'opos' not reflected correctly")
        if (refl.outputSignature[1].varName != 'outcol' or refl.outputSignature[1].compCount != 4):
            raise rdtest.TestFailureException("Vertex shader output 'outcol' not reflected correctly")

        if 'vertmain' not in disasm:
            raise rdtest.TestFailureException("Vertex shader disassembly failed, entry point not found")

        refl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Fragment)

        disasm: str = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), refl, "")

        if (refl.inputSignature[0].varName != 'incol' or refl.inputSignature[0].compCount != 4):
            raise rdtest.TestFailureException("Fragment shader input 'incol' not reflected correctly")

        if (refl.outputSignature[0].varName != 'ocol' or refl.outputSignature[0].compCount != 4 or refl.outputSignature[0].systemValue != rd.ShaderBuiltin.ColorOutput):
            raise rdtest.TestFailureException("Fragment shader output 'ocol' not reflected correctly")

        if 'fragmain' not in disasm:
            raise rdtest.TestFailureException("Fragment shader disassembly failed, entry point not found")

        rdtest.log.success("shader reflection and disassembly as expected")

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'opos': [-0.5, 0.5, 0.0, 1.0],
                'outcol': [0.0, 1.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'opos': [0.0, -0.5, 0.0, 1.0],
                'outcol': [0.0, 1.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'opos': [0.5, 0.5, 0.0, 1.0],
                'outcol': [0.0, 1.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        rdtest.log.success("vertex output is as expected")

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, [0.0, 1.0, 0.0, 1.0])

        rdtest.log.success("picked value is as expected")
