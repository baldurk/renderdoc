from __future__ import annotations
from typing import Any, Callable, Dict

import rdtest
import renderdoc as rd


class D3D12_Reflection_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Reflection_Zoo'

    def check_capture(self):
        action = self.find_action("DXBC")

        assert action is not None

        self.controller.SetFrameEvent(action.nextAction.eventId, False)

        pipe = self.controller.GetPipelineState()

        stage = rd.ShaderStage.Pixel

        # Verify that the DXBC action is first
        refl = pipe.GetShaderReflection(stage)
        assert refl is not None
        disasm = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), refl, '')

        assert 'ps_5_1' in disasm

        self.check_event()

        rdtest.log.success("DXBC action is as expected")

        # Move to the DXIL action
        action = self.find_action("SM6.0")

        if action is None:
            rdtest.log.print("No SM6.0 DXIL action to test")
            return

        self.controller.SetFrameEvent(action.nextAction.eventId, False)

        pipe = self.controller.GetPipelineState()

        refl = pipe.GetShaderReflection(stage)
        assert refl is not None
        disasm = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), refl, '')

        assert 'SM6.0' in disasm

        self.check_event()

        rdtest.log.success("SM6.0 DXIL action is as expected")

        # Move to the DXIL action
        action = self.find_action("SM6.7")

        if action is None:
            rdtest.log.print("No SM6.7 DXIL action to test")
            return

        self.controller.SetFrameEvent(action.nextAction.eventId, False)

        pipe = self.controller.GetPipelineState()

        refl = pipe.GetShaderReflection(stage)
        assert refl is not None
        disasm = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), refl, '')

        assert 'SM6.7' in disasm

        self.check_event()

        rdtest.log.success("SM6.7 DXIL action is as expected")

    def check_event(self):
        pipe = self.controller.GetPipelineState()

        stage = rd.ShaderStage.Pixel

        refl = pipe.GetShaderReflection(stage)

        # Check we have the source and it is unmangled
        debugInfo = refl.debugInfo

        assert len(debugInfo.files) == 1

        assert 'Iñtërnâtiônàližætiøn' in debugInfo.files[0].contents

        def checker(textureType: rd.TextureType,
                    varType: rd.VarType,
                    col: int,
                    register: int,
                    typeName: str,
                    *,
                    isTexture: bool = True,
                    regCount: int = 1,
                    structVarCheck: Callable[[rd.ShaderConstantType], None] | None=None) -> Dict[str, Any]:
            return {
                'textureType': textureType,
                'isTexture': isTexture,
                'register': register,
                'regCount': regCount,
                'varType': varType,
                'columns': col,
                'typeName': typeName,
                'structVarCheck': structVarCheck,
            }

        def buf_struct_check(type: rd.ShaderConstantType):
            assert type.name == 'buf_struct'
            assert len(type.members) == 3

            assert type.members[0].name == 'a'
            assert type.members[0].type.baseType == rd.VarType.Float
            assert type.members[0].type.rows == 1
            assert type.members[0].type.columns == 1
            assert type.members[0].type.elements == 1
            assert type.members[0].byteOffset == 0
            assert type.members[0].bitFieldOffset == 0
            assert type.members[0].bitFieldSize == 0

            assert type.members[1].name == 'b'
            assert type.members[1].type.baseType == rd.VarType.Float
            assert type.members[1].type.rows == 1
            assert type.members[1].type.columns == 1
            assert type.members[1].type.elements == 2
            assert type.members[1].byteOffset == 4
            assert type.members[1].bitFieldOffset == 0
            assert type.members[1].bitFieldSize == 0

            assert type.members[2].name == 'c'
            assert type.members[2].type.name == 'nested'
            assert type.members[2].byteOffset == 12
            assert type.members[2].bitFieldOffset == 0
            assert type.members[2].bitFieldSize == 0
            assert len(type.members[2].type.members) == 1
            assert type.members[2].type.members[0].name == 'x'
            assert type.members[2].type.members[0].type.baseType == rd.VarType.Float
            assert type.members[2].type.members[0].type.rows == 2
            assert type.members[2].type.members[0].type.columns == 3
            assert type.members[2].type.members[0].type.RowMajor()
            assert type.members[2].type.members[0].byteOffset == 0
            assert type.members[2].type.members[0].bitFieldOffset == 0
            assert type.members[2].type.members[0].bitFieldSize == 0

            return

        def sm67_struct_check(type: rd.ShaderConstantType):
            assert type.name == 'sm67_struct'
            assert len(type.members) == 17

            # to simplify checks we only look at offsets and bitfield properties,
            # assuming the base types are the same (except for deliberate int
            # check_eqs on some bitfields)
            self.check_eq(type.members[0].name, 'x')
            self.check_eq(type.members[0].byteOffset, 0)

            self.check_eq(type.members[1].name, 'a')
            self.check_eq(type.members[1].byteOffset, 4)
            self.check_eq(type.members[1].bitFieldOffset, 0)
            self.check_eq(type.members[1].bitFieldSize, 10)

            self.check_eq(type.members[2].name, 'b')
            self.check_eq(type.members[2].byteOffset, 4)
            self.check_eq(type.members[2].bitFieldOffset, 10)
            self.check_eq(type.members[2].bitFieldSize, 10)

            self.check_eq(type.members[3].name, 'c')
            self.check_eq(type.members[3].byteOffset, 4)
            self.check_eq(type.members[3].bitFieldOffset, 20)
            self.check_eq(type.members[3].bitFieldSize, 10)

            self.check_eq(type.members[4].name, 'd')
            self.check_eq(type.members[4].byteOffset, 4)
            self.check_eq(type.members[4].bitFieldOffset, 30)
            self.check_eq(type.members[4].bitFieldSize, 2)

            self.check_eq(type.members[5].name, 'e')
            self.check_eq(type.members[5].byteOffset, 8)

            self.check_eq(type.members[6].name, 'f')
            self.check_eq(type.members[6].byteOffset, 20)
            self.check_eq(type.members[6].bitFieldOffset, 0)
            self.check_eq(type.members[6].bitFieldSize, 14)

            self.check_eq(type.members[7].name, 'g')
            self.check_eq(type.members[7].byteOffset, 24)

            self.check_eq(type.members[8].name, 'h')
            self.check_eq(type.members[8].byteOffset, 36)
            self.check_eq(type.members[8].bitFieldOffset, 0)
            self.check_eq(type.members[8].bitFieldSize, 10)

            self.check_eq(type.members[9].name, 'i')
            self.check_eq(type.members[9].byteOffset, 36)
            self.check_eq(type.members[9].bitFieldOffset, 10)
            self.check_eq(type.members[9].bitFieldSize, 10)

            self.check_eq(type.members[10].name, 'j')
            self.check_eq(type.members[10].byteOffset, 36)
            self.check_eq(type.members[10].bitFieldOffset, 20)
            self.check_eq(type.members[10].bitFieldSize, 10)

            self.check_eq(type.members[11].name, 'k')
            self.check_eq(type.members[11].byteOffset, 40)
            self.check_eq(type.members[11].bitFieldOffset, 0)
            self.check_eq(type.members[11].bitFieldSize, 10)

            self.check_eq(type.members[12].name, 'l')
            self.check_eq(type.members[12].byteOffset, 40)
            self.check_eq(type.members[12].bitFieldOffset, 10)
            self.check_eq(type.members[12].bitFieldSize, 10)

            self.check_eq(type.members[13].name, 'm')
            self.check_eq(type.members[13].byteOffset, 44)

            self.check_eq(type.members[14].name, 'n')
            self.check_eq(type.members[14].type.baseType, rd.VarType.UInt)
            self.check_eq(type.members[14].byteOffset, 48)
            self.check_eq(type.members[14].bitFieldOffset, 0)
            self.check_eq(type.members[14].bitFieldSize, 5)

            self.check_eq(type.members[15].name, '')
            self.check_eq(type.members[15].type.baseType, rd.VarType.UInt)
            self.check_eq(type.members[15].byteOffset, 48)
            self.check_eq(type.members[15].bitFieldOffset, 5)
            self.check_eq(type.members[15].bitFieldSize, 5)

            self.check_eq(type.members[16].name, 'o')
            self.check_eq(type.members[16].type.baseType, rd.VarType.SInt)
            self.check_eq(type.members[16].byteOffset, 48)
            self.check_eq(type.members[16].bitFieldOffset, 10)
            self.check_eq(type.members[16].bitFieldSize, 5)

            return

        ro_db = {
            'tex1d':
                checker(rd.TextureType.Texture1D, rd.VarType.Float, 4, 0, 'float4'),
            'tex2d':
                checker(rd.TextureType.Texture2D, rd.VarType.Float, 4, 1, 'float4'),
            'tex3d':
                checker(rd.TextureType.Texture3D, rd.VarType.Float, 4, 2, 'float4'),
            'tex1darray':
                checker(rd.TextureType.Texture1DArray, rd.VarType.Float, 4, 3, 'float4'),
            'tex2darray':
                checker(rd.TextureType.Texture2DArray, rd.VarType.Float, 4, 4, 'float4'),
            'texcube':
                checker(rd.TextureType.TextureCube, rd.VarType.Float, 4, 5, 'float4'),
            'texcubearray':
                checker(rd.TextureType.TextureCubeArray, rd.VarType.Float, 4, 6, 'float4'),
            'tex2dms':
                checker(rd.TextureType.Texture2DMS, rd.VarType.Float, 4, 7, 'float4'),
            'tex2dmsarray':
                checker(rd.TextureType.Texture2DMSArray, rd.VarType.Float, 4, 8, 'float4'),
            'tex2d_f1':
                checker(rd.TextureType.Texture2D, rd.VarType.Float, 1, 10, 'float'),
            'tex2d_f2':
                checker(rd.TextureType.Texture2D, rd.VarType.Float, 2, 11, 'float2'),
            'tex2d_f3':
                checker(rd.TextureType.Texture2D, rd.VarType.Float, 3, 12, 'float3'),
            'tex2d_u2':
                checker(rd.TextureType.Texture2D, rd.VarType.UInt, 2, 13, 'uint2'),
            'tex2d_u3':
                checker(rd.TextureType.Texture2D, rd.VarType.UInt, 3, 14, 'uint3'),
            'tex2d_i2':
                checker(rd.TextureType.Texture2D, rd.VarType.SInt, 2, 15, 'int2'),
            'tex2d_i3':
                checker(rd.TextureType.Texture2D, rd.VarType.SInt, 3, 16, 'int3'),
            'msaa_flt2_4x':
                checker(rd.TextureType.Texture2DMS, rd.VarType.Float, 2, 17, 'float2'),
            'msaa_flt3_2x':
                checker(rd.TextureType.Texture2DMS, rd.VarType.Float, 3, 18, 'float3'),
            'msaa_flt4_8x':
                checker(rd.TextureType.Texture2DMS, rd.VarType.Float, 4, 19, 'float4'),
            'buf_f1':
                checker(rd.TextureType.Buffer, rd.VarType.Float, 1, 20, 'float', isTexture=False),
            'buf_f2':
                checker(rd.TextureType.Buffer, rd.VarType.Float, 2, 21, 'float2', isTexture=False),
            'buf_f3':
                checker(rd.TextureType.Buffer, rd.VarType.Float, 3, 22, 'float3', isTexture=False),
            'buf_f4':
                checker(rd.TextureType.Buffer, rd.VarType.Float, 4, 23, 'float4', isTexture=False),
            'buf_u2':
                checker(rd.TextureType.Buffer, rd.VarType.UInt, 2, 24, 'uint2', isTexture=False),
            'buf_i3':
                checker(rd.TextureType.Buffer, rd.VarType.SInt, 3, 25, 'int3', isTexture=False),
            'bytebuf':
                checker(rd.TextureType.Buffer, rd.VarType.UByte, 1, 30, 'byte', isTexture=False),
            'strbuf':
                checker(rd.TextureType.Buffer,
                        rd.VarType.Unknown,
                        0,
                        40,
                        'buf_struct',
                        isTexture=False,
                        structVarCheck=buf_struct_check),
            'strbuf_f2':
                checker(rd.TextureType.Buffer, rd.VarType.Float, 2, 41, 'float2', isTexture=False),
            'tex2dArray':
                checker(rd.TextureType.Texture2DArray, rd.VarType.Float, 1, 50, 'float', regCount=4),
        }

        rw_db = {
            'rwtex1d':
                checker(rd.TextureType.Texture1D, rd.VarType.Float, 4, 0, 'float4'),
            'rwtex2d':
                checker(rd.TextureType.Texture2D, rd.VarType.Float, 4, 1, 'float4'),
            'rwtex3d':
                checker(rd.TextureType.Texture3D, rd.VarType.Float, 4, 2, 'float4'),
            'rwtex1darray':
                checker(rd.TextureType.Texture1DArray, rd.VarType.Float, 4, 3, 'float4'),
            'rwtex2darray':
                checker(rd.TextureType.Texture2DArray, rd.VarType.Float, 4, 4, 'float4'),
            'rwtex2d_f1':
                checker(rd.TextureType.Texture2D, rd.VarType.Float, 1, 10, 'float'),
            'rwtex2d_f2':
                checker(rd.TextureType.Texture2D, rd.VarType.Float, 2, 11, 'float2'),
            'rwtex2d_f3':
                checker(rd.TextureType.Texture2D, rd.VarType.Float, 3, 12, 'float3'),
            'rwtex2d_u2':
                checker(rd.TextureType.Texture2D, rd.VarType.UInt, 2, 13, 'uint2'),
            'rwtex2d_u3':
                checker(rd.TextureType.Texture2D, rd.VarType.UInt, 3, 14, 'uint3'),
            'rwtex2d_i2':
                checker(rd.TextureType.Texture2D, rd.VarType.SInt, 2, 15, 'int2'),
            'rwtex2d_i3':
                checker(rd.TextureType.Texture2D, rd.VarType.SInt, 3, 16, 'int3'),
            'rwbuf_f1':
                checker(rd.TextureType.Buffer, rd.VarType.Float, 1, 20, 'float', isTexture=False),
            'rwbuf_f2':
                checker(rd.TextureType.Buffer, rd.VarType.Float, 2, 21, 'float2', isTexture=False),
            'rwbuf_f3':
                checker(rd.TextureType.Buffer, rd.VarType.Float, 3, 22, 'float3', isTexture=False),
            'rwbuf_f4':
                checker(rd.TextureType.Buffer, rd.VarType.Float, 4, 23, 'float4', isTexture=False),
            'rwbuf_u2':
                checker(rd.TextureType.Buffer, rd.VarType.UInt, 2, 24, 'uint2', isTexture=False),
            'rwbuf_i3':
                checker(rd.TextureType.Buffer, rd.VarType.SInt, 3, 25, 'int3', isTexture=False),
            'rov':
                checker(rd.TextureType.Texture2D, rd.VarType.Float, 4, 30, 'float4'),
            'rwbytebuf':
                checker(rd.TextureType.Buffer, rd.VarType.UByte, 1, 40, 'byte', isTexture=False),
            'rwstrbuf':
                checker(rd.TextureType.Buffer,
                        rd.VarType.Unknown,
                        0,
                        50,
                        'buf_struct',
                        isTexture=False,
                        structVarCheck=buf_struct_check),
            'rwcounter':
                checker(rd.TextureType.Buffer,
                        rd.VarType.Unknown,
                        0,
                        51,
                        'buf_struct',
                        isTexture=False,
                        structVarCheck=buf_struct_check),
            'rwappend':
                checker(rd.TextureType.Buffer,
                        rd.VarType.Unknown,
                        0,
                        52,
                        'buf_struct',
                        isTexture=False,
                        structVarCheck=buf_struct_check),
            'rwconsume':
                checker(rd.TextureType.Buffer,
                        rd.VarType.Unknown,
                        0,
                        53,
                        'buf_struct',
                        isTexture=False,
                        structVarCheck=buf_struct_check),
            'rwstrbuf_f2':
                checker(rd.TextureType.Buffer, rd.VarType.Float, 2, 54, 'float2', isTexture=False),
            'rwstrbuf67':
                checker(rd.TextureType.Buffer,
                        rd.VarType.Unknown,
                        0,
                        55,
                        'sm67_struct',
                        isTexture=False,
                        structVarCheck=sm67_struct_check),
        }

        # ROVs are optional, if it wasn't found then ignore that
        if '#define ROV 0' in debugInfo.files[0].contents:
            del rw_db['rov']

        # only check SM6.7 structured buffer with bitfields on SM6.7
        if '#define SM67 1' not in debugInfo.files[0].contents:
            del rw_db['rwstrbuf67']

        access = [(a.type, a.index) for a in self.controller.GetDescriptorAccess()]

        for idx, s in enumerate(refl.samplers):
            assert s.fixedBindSetOrSpace == 0
            assert (rd.DescriptorType.Sampler, idx) in access
            assert s.bindArraySize == 1

            if s.name == 's1':
                assert s.fixedBindNumber == 5
            elif s.name == 's2':
                assert s.fixedBindNumber == 8
            else:
                raise rdtest.TestFailureException(f'Unrecognised sampler {s.name}')

        for res_list, res_db, res_readonly in [(refl.readOnlyResources, ro_db, True),
                                               (refl.readWriteResources, rw_db, False)]:
            for idx, res in enumerate(res_list):
                assert res.isReadOnly == res_readonly
                assert res.fixedBindSetOrSpace == 0

                assert (res.descriptorType, idx) in access, f"{res.name} not found - ({res.descriptorType!s}, {idx})"

                if res.name in res_db:
                    check = res_db[res.name]

                    assert res.textureType == check['textureType']
                    assert res.isTexture == check['isTexture']

                    if check['structVarCheck']:
                        check['structVarCheck'](res.variableType)
                    else:
                        assert res.variableType.baseType == check['varType']
                        assert res.variableType.name == check['typeName']
                        assert res.variableType.columns == check['columns']

                    assert res.fixedBindNumber == check['register']
                    assert res.bindArraySize == check['regCount']
                else:
                    raise rdtest.TestFailureException(f"Unrecognised {'read-only' if res_readonly else 'read-write'} resource {res.name}")

                del res_db[res.name]

            if len(res_db) != 0:
                raise rdtest.TestFailureException(f"Expected resources weren't found: {res_db.keys()}")

        rdtest.log.success("Reflected shader source as expected")
