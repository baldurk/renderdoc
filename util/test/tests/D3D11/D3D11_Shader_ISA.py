import renderdoc as rd
from typing import List
import rdtest


class D3D11_Shader_ISA(rdtest.TestCase):
    demos_test_name = 'D3D11_Shader_ISA'

    def check_capture(self):
        action = self.find_action("GPU=")

        self.check(action is not None)

        is_amd = 'AMD' in action.customName

        self.controller.SetFrameEvent(action.next.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        refl: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Vertex)

        isas: List[str] = self.controller.GetDisassemblyTargets(True)

        if isas == []:
            raise rdtest.TestFailureException("Expected some disassembly targets, got none!")

        # Generic testing can't do much, we just ensure that we can successfully get a non-empty disassembly string
        for isa in isas:
            disasm: str = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), refl, isa)

            if len(disasm) < 32:
                raise rdtest.TestFailureException("Disassembly for target '{}' is degenerate: {}".format(isa, disasm))

        rdtest.log.success("All disassembly targets successfully fetched and seem reasonable")

        # We make this a hard failure. Users can fix this by installing the plugins, and we don't want automated
        # overnight tests to suddenly stop checking
        if 'AMDIL' not in isas:
            raise rdtest.TestFailureException(
                "AMDIL is not an available disassembly target. Are you missing plugins?")

        disasm: str = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), refl, 'AMDIL')

        expected = [
            'il_vs',
            'dcl_output_position',
            'end',
        ]

        for fragment in expected:
            if not fragment in disasm:
                raise rdtest.TestFailureException(
                    "AMDIL ISA doesn't contain '{}' as expected: {}".format(fragment, disasm))

        if 'RDNA (gfx1010)' not in isas:
            raise rdtest.TestFailureException(
                "RDNA (gfx1010) is not an available disassembly target. Are you missing plugins?")

        disasm: str = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), refl, 'RDNA (gfx1010)')

        expected = [
            'asic(GFX10)',
            'vgpr_count',
            'wave_size',
            's_endpgm',
        ]

        for fragment in expected:
            if not fragment in disasm:
                raise rdtest.TestFailureException(
                    "RDNA ISA doesn't contain '{}' as expected: {}".format(fragment, disasm))

        rdtest.log.success("AMD disassembly is as expected")

        # D3D11 doesn't have live driver disassembly, can't test it
        if not is_amd:
            rdtest.log.print("Not testing live driver disassembly outside AMD")
        else:
            rdtest.log.print("No live driver disassembly to test on D3D11")