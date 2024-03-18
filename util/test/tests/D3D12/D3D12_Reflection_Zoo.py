import rdtest
import renderdoc as rd


class D3D12_Reflection_Zoo(rdtest.TestCase):
    demos_test_name = 'D3D12_Reflection_Zoo'

    def check_capture(self):
        action = self.find_action("DXBC")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.next.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        stage = rd.ShaderStage.Pixel

        # Verify that the DXBC action is first
        disasm = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), pipe.GetShaderReflection(stage),
                                                   '')

        self.check('ps_5_1' in disasm)

        self.check_event()

        rdtest.log.success("DXBC action is as expected")

        # Move to the DXIL action
        action = self.find_action("DXIL")

        if action is None:
            rdtest.log.print("No DXIL action to test")
            return

        self.controller.SetFrameEvent(action.next.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        disasm = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), pipe.GetShaderReflection(stage),
                                                   '')

        self.check('SM6.0' in disasm)

        self.check_event()

        rdtest.log.success("DXIL action is as expected")

    def check_event(self):
        pipe: rd.PipeState = self.controller.GetPipelineState()

        stage = rd.ShaderStage.Pixel

        refl: rd.ShaderReflection = pipe.GetShaderReflection(stage)

        # Check we have the source and it is unmangled
        debugInfo: rd.ShaderDebugInfo = refl.debugInfo

        self.check(len(debugInfo.files) == 1)

        self.check('Iñtërnâtiônàližætiøn' in debugInfo.files[0].contents)

        def checker(textureType: rd.TextureType,
                    varType: rd.VarType,
                    col: int,
                    register: int,
                    typeName: str,
                    *,
                    isTexture: bool = True,
                    regCount: int = 1,
                    structVarCheck=None):
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
            self.check(type.name == 'buf_struct')
            self.check(len(type.members) == 3)

            self.check(type.members[0].name == 'a')
            self.check(type.members[0].type.baseType == rd.VarType.Float)
            self.check(type.members[0].type.rows == 1)
            self.check(type.members[0].type.columns == 1)
            self.check(type.members[0].type.elements == 1)

            self.check(type.members[1].name == 'b')
            self.check(type.members[1].type.baseType == rd.VarType.Float)
            self.check(type.members[1].type.rows == 1)
            self.check(type.members[1].type.columns == 1)
            self.check(type.members[1].type.elements == 2)

            self.check(type.members[2].name == 'c')
            self.check(type.members[2].type.name == 'nested')
            self.check(len(type.members[2].type.members) == 1)
            self.check(type.members[2].type.members[0].name == 'x')
            self.check(type.members[2].type.members[0].type.baseType == rd.VarType.Float)
            self.check(type.members[2].type.members[0].type.rows == 2)
            self.check(type.members[2].type.members[0].type.columns == 3)
            self.check(type.members[2].type.members[0].type.RowMajor())

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
        }

        # ROVs are optional, if it wasn't found then ignore that
        if '#define ROV 0' in debugInfo.files[0].contents:
            del rw_db['rov']

        access = [(a.type, a.index) for a in self.controller.GetDescriptorAccess()]

        for idx, s in enumerate(refl.samplers):
            self.check(s.fixedBindSetOrSpace == 0)
            self.check((rd.DescriptorType.Sampler, idx) in access)
            self.check(s.bindArraySize == 1)

            if s.name == 's1':
                self.check(s.fixedBindNumber == 5)
            elif s.name == 's2':
                self.check(s.fixedBindNumber == 8)
            else:
                raise rdtest.TestFailureException('Unrecognised sampler {}'.format(s.name))

        for res_list, res_db, res_readonly in [(refl.readOnlyResources, ro_db, True),
                                               (refl.readWriteResources, rw_db, False)]:
            for idx, res in enumerate(res_list):
                res: rd.ShaderResource

                self.check(res.isReadOnly == res_readonly)
                self.check(res.fixedBindSetOrSpace == 0)

                self.check((res.descriptorType, idx) in access, f"{res.name} - ({str(res.descriptorType)}, {idx})")

                if res.name in res_db:
                    check = res_db[res.name]

                    self.check(res.textureType == check['textureType'])
                    self.check(res.isTexture == check['isTexture'])

                    if check['structVarCheck']:
                        check['structVarCheck'](res.variableType)
                    else:
                        self.check(res.variableType.baseType == check['varType'])
                        self.check(res.variableType.name == check['typeName'])
                        self.check(res.variableType.columns == check['columns'])

                    self.check(res.fixedBindNumber == check['register'])
                    self.check(res.bindArraySize == check['regCount'])
                else:
                    raise rdtest.TestFailureException('Unrecognised read-only resource {}'.format(res.name))

                del res_db[res.name]

            if len(res_db) != 0:
                raise rdtest.TestFailureException("Expected resources weren't found: {}".format(res_db.keys()))

        rdtest.log.success("Reflected shader source as expected")
