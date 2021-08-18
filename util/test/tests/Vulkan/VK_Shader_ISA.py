import renderdoc as rd
from typing import List
import rdtest


class VK_Shader_ISA(rdtest.TestCase):
    demos_test_name = 'VK_Shader_ISA'

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

        # For AMD we also expect live driver disassembly. Check that we get it
        if not is_amd:
            rdtest.log.print("Not testing live driver disassembly outside AMD")
        else:
            if 'AMD_shader_info' not in isas:
                raise rdtest.TestFailureException(
                    "AMD_shader_info expected but not found. Check driver version and update to latest.")

            disasm: str = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), refl, 'AMD_shader_info')

            expected = [
                'buffer_load',
                's_endpgm',
            ]

            for fragment in expected:
                if not fragment in disasm:
                    raise rdtest.TestFailureException(
                        "AMD_shader_info ISA doesn't contain '{}' as expected: {}".format(fragment, disasm))

            if 'KHR_pipeline_executable_properties' not in isas:
                raise rdtest.TestFailureException(
                    "KHR_pipeline_executable_properties expected but not found. Check driver version and update to "
                    "latest.")

            disasm: str = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), refl,
                                                            'KHR_pipeline_executable_properties')

            expected = [
                'buffer_load',
                's_endpgm',
            ]

            for fragment in expected:
                if not fragment in disasm:
                    raise rdtest.TestFailureException(
                        "KHR_pipeline_executable_properties ISA doesn't contain '{}' as expected: {}".format(fragment,
                                                                                                             disasm))

            rdtest.log.success("Live driver disassembly is as expected")
