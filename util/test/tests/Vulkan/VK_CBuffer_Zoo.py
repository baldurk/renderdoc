import rdtest
import renderdoc as rd


class VK_CBuffer_Zoo(rdtest.TestCase):
    demos_test_name = 'VK_CBuffer_Zoo'

    def check_capture(self):
        action = self.find_action("Draw")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, False)

        props: rd.APIProperties = self.controller.GetAPIProperties()

        pipe: rd.PipeState = self.controller.GetPipelineState()

        stage = rd.ShaderStage.Pixel

        # Verify that the GLSL action is first
        disasm = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), pipe.GetShaderReflection(stage),
                                                   '')

        self.check('GLSL' in disasm)

        cbuf = pipe.GetConstantBlock(stage, 0, 0).descriptor

        var_check = rdtest.ConstantBufferChecker(
            self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                       pipe.GetShader(stage), stage,
                                                       pipe.GetShaderEntryPoint(stage), 0,
                                                       cbuf.resource, cbuf.byteOffset, cbuf.byteSize))

        self.check_glsl_cbuffer(var_check)

        rdtest.log.success("GLSL CBuffer variables are as expected")

        cbuf = pipe.GetConstantBlock(stage, 1, 0).descriptor

        inline_check = rdtest.ConstantBufferChecker(
            self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                       pipe.GetShader(stage), stage,
                                                       pipe.GetShaderEntryPoint(stage), 1,
                                                       cbuf.resource, cbuf.byteOffset, cbuf.byteSize))

        self.check_inline_cbuffer(inline_check)

        rdtest.log.success("GLSL Inline uniform variables are as expected")

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, [536.1, 537.0, 538.0, 539.0])

        rdtest.log.success("GLSL picked value is as expected")

        # Check the specialization constants
        cbuf = pipe.GetConstantBlock(stage, 2, 0).descriptor

        var_check = rdtest.ConstantBufferChecker(
            self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                       pipe.GetShader(stage), stage,
                                                       pipe.GetShaderEntryPoint(stage), 2,
                                                       cbuf.resource, cbuf.byteOffset, cbuf.byteSize))

        # int A;
        # Default value 10, untouched
        var_check.check('A').type(rd.VarType.SInt).rows(1).cols(1).value([10])

        # float B;
        # Value 20 from spec constants
        var_check.check('B').type(rd.VarType.Float).rows(1).cols(1).value([20.0])

        # bool C;
        # Value True from spec constants
        var_check.check('C').type(rd.VarType.Bool).rows(1).cols(1).value([1])

        var_check.done()

        rdtest.log.success("Specialization constants are as expected")

        # Move to the HLSL action
        action = action.next

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        # Verify that this is the HLSL action
        disasm = self.controller.DisassembleShader(pipe.GetGraphicsPipelineObject(), pipe.GetShaderReflection(stage),
                                                   '')

        self.check('HLSL' in disasm)

        cbuf = pipe.GetConstantBlock(stage, 0, 0).descriptor

        var_check = rdtest.ConstantBufferChecker(
            self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                       pipe.GetShader(stage), stage,
                                                       pipe.GetShaderEntryPoint(stage), 0,
                                                       cbuf.resource, cbuf.byteOffset, cbuf.byteSize))

        self.check_hlsl_cbuffer(var_check)

        rdtest.log.success("HLSL CBuffer variables are as expected")

        cbuf = pipe.GetConstantBlock(stage, 1, 0).descriptor

        inline_check = rdtest.ConstantBufferChecker(
            self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                       pipe.GetShader(stage), stage,
                                                       pipe.GetShaderEntryPoint(stage), 1,
                                                       cbuf.resource, cbuf.byteOffset, cbuf.byteSize))

        self.check_inline_cbuffer(inline_check)

        rdtest.log.success("HLSL Inline uniform variables are as expected")

        self.check_pixel_value(pipe.GetOutputTargets()[0].resource, 0.5, 0.5, [536.1, 537.0, 538.0, 539.0])

        rdtest.log.success("HLSL picked value is as expected")

    def check_inline_cbuffer(self, inline_check):
        # float4 zero;
        inline_check.check('inline_zero').rows(1).cols(4).value([0.0, 0.0, 0.0, 0.0])

        # float4 a;
        inline_check.check('inline_a').rows(1).cols(4).value([10.0, 20.0, 30.0, 40.0])

        # float2 b;
        inline_check.check('inline_b').rows(1).cols(2).value([50.0, 60.0])

        # float2 c;
        inline_check.check('inline_c').rows(1).cols(2).value([70.0, 80.0])

        # float3_1 d;
        inline_check.check('inline_d').rows(0).cols(0).structSize(2).members({
            'a': lambda y: y.rows(1).cols(3).value([90.0, 100.0, 110.0]),
            'b': lambda y: y.rows(1).cols(1).value([120.0]),
        })

        inline_check.done()

    def check_glsl_cbuffer(self, var_check):
        # For more detailed reference for the below checks, see the commented definition of the cbuffer
        # in the shader source code in the demo itself

        # vec4 a;
        var_check.check('a').cols(4).rows(1).value([0.0, 1.0, 2.0, 3.0])

        # vec3 b;
        var_check.check('b').cols(3).rows(1).value([4.0, 5.0, 6.0])

        # vec2 c; vec2 d;
        var_check.check('c').cols(2).rows(1).value([8.0, 9.0])
        var_check.check('d').cols(2).rows(1).value([10.0, 11.0])

        # float e; vec3 f;
        var_check.check('e').cols(1).rows(1).value([12.0])
        var_check.check('f').cols(3).rows(1).value([16.0, 17.0, 18.0])

        # vec4 dummy0;
        var_check.check('dummy0')

        # float j; vec2 k;
        var_check.check('j').cols(1).rows(1).value([24.0])
        var_check.check('k').cols(2).rows(1).value([26.0, 27.0])

        # vec2 l; float m;
        var_check.check('l').cols(2).rows(1).value([28.0, 29.0])
        var_check.check('m').cols(1).rows(1).value([30.0])

        # float n[4];
        var_check.check('n').cols(0).rows(0).arraySize(4).members({
            0: lambda x: x.cols(1).rows(1).value([32.0]),
            1: lambda x: x.cols(1).rows(1).value([36.0]),
            2: lambda x: x.cols(1).rows(1).value([40.0]),
            3: lambda x: x.cols(1).rows(1).value([44.0]),
        })

        # vec4 dummy1;
        var_check.check('dummy1')

        # float o[4];
        var_check.check('o').cols(0).rows(0).arraySize(4).members({
            0: lambda x: x.cols(1).rows(1).value([52.0]),
            1: lambda x: x.cols(1).rows(1).value([56.0]),
            2: lambda x: x.cols(1).rows(1).value([60.0]),
            3: lambda x: x.cols(1).rows(1).value([64.0]),
        })

        # float p;
        var_check.check('p').cols(1).rows(1).value([68.0])

        # vec4 dummy2;
        var_check.check('dummy2')

        # column_major vec4x4 q;
        var_check.check('q').cols(4).rows(4).column_major().value([76.0, 80.0, 84.0, 88.0,
                                                                   77.0, 81.0, 85.0, 89.0,
                                                                   78.0, 82.0, 86.0, 90.0,
                                                                   79.0, 83.0, 87.0, 91.0])

        # row_major vec4x4 r;
        var_check.check('r').cols(4).rows(4).row_major().value([92.0, 93.0, 94.0, 95.0,
                                                                96.0, 97.0, 98.0, 99.0,
                                                                100.0, 101.0, 102.0, 103.0])

        # column_major vec4x3 s;
        var_check.check('s').cols(4).rows(3).column_major().value([108.0, 112.0, 116.0, 120.0,
                                                                   109.0, 113.0, 117.0, 121.0,
                                                                   110.0, 114.0, 118.0, 122.0])

        # vec4 dummy3;
        var_check.check('dummy3')

        # row_major vec4x3 t;
        var_check.check('t').cols(4).rows(3).row_major().value([128.0, 129.0, 130.0, 131.0,
                                                                132.0, 133.0, 134.0, 135.0,
                                                                136.0, 137.0, 138.0, 139.0])

        # vec4 dummy4;
        var_check.check('dummy4')

        # column_major vec2x3 u;
        var_check.check('u').cols(3).rows(2).column_major().value([144.0, 148.0, 152.0,
                                                                   145.0, 149.0, 153.0])

        # vec4 dummy5;
        var_check.check('dummy5')

        # row_major vec3x2 v;
        var_check.check('v').cols(3).rows(2).row_major().value([160.0, 161.0, 162.0,
                                                                164.0, 165.0, 166.0])

        # vec4 dummy6;
        var_check.check('dummy6')

        # column_major vec3x2 w;
        var_check.check('w').cols(2).rows(2).column_major().value([172.0, 176.0,
                                                                   173.0, 177.0])

        # vec4 dummy7;
        var_check.check('dummy7')

        # row_major vec3x2 x;
        var_check.check('x').cols(2).rows(2).row_major().value([184.0, 185.0,
                                                                188.0, 189.0])

        # vec4 dummy8;
        var_check.check('dummy8')

        # row_major vec2x2 y;
        var_check.check('y').cols(2).rows(2).row_major().value([196.0, 197.0,
                                                                200.0, 201.0])

        # float z;
        var_check.check('z').cols(1).rows(1).value([204.0])

        # vec4 dummy9;
        var_check.check('dummy9')

        # vec4 multiarray[3][2];
        var_check.check('multiarray').cols(0).rows(0).arraySize(3).members({
            0: lambda x: x.cols(0).rows(0).arraySize(2).members({
                0: lambda y: y.cols(4).rows(1).value([228.0, 229.0, 230.0, 231.0]),
                1: lambda y: y.cols(4).rows(1).value([232.0, 233.0, 234.0, 235.0]),
            }),
            1: lambda x: x.cols(0).rows(0).arraySize(2).members({
                0: lambda y: y.cols(4).rows(1).value([236.0, 237.0, 238.0, 239.0]),
                1: lambda y: y.cols(4).rows(1).value([240.0, 241.0, 242.0, 243.0]),
            }),
            2: lambda x: x.cols(0).rows(0).arraySize(2).members({
                0: lambda y: y.cols(4).rows(1).value([244.0, 245.0, 246.0, 247.0]),
                1: lambda y: y.cols(4).rows(1).value([248.0, 249.0, 250.0, 251.0]),
            }),
        })

        # struct vec3_1 { vec3 a; float b; };
        # struct nested { vec3_1 a; vec4 b[4]; vec3_1 c[4]; };
        # nested structa[2];
        var_check.check('structa').cols(0).rows(0).arraySize(2).members({
            # structa[0]
            0: lambda s: s.cols(0).rows(0).structSize(3).members({
                'a': lambda x: x.cols(0).rows(0).structSize(2).members({
                    'a': lambda y: y.cols(3).rows(1).value([252.0, 253.0, 254.0]),
                    'b': lambda y: y.cols(1).rows(1).value([255.0]),
                }),
                'b': lambda x: x.cols(0).rows(0).arraySize(4).members({
                    0: lambda y: y.cols(4).rows(1).value([256.0, 257.0, 258.0, 259.0]),
                    1: lambda y: y.cols(4).rows(1).value([260.0, 261.0, 262.0, 263.0]),
                    2: lambda y: y.cols(4).rows(1).value([264.0, 265.0, 266.0, 267.0]),
                    3: lambda y: y.cols(4).rows(1).value([268.0, 269.0, 270.0, 271.0]),
                }),
                'c': lambda x: x.cols(0).rows(0).arraySize(4).members({
                    0: lambda y: y.cols(0).rows(0).structSize(2).members({
                        'a': lambda z: z.cols(3).rows(1).value([272.0, 273.0, 274.0]),
                        'b': lambda z: z.cols(1).rows(1).value([275.0]),
                    }),
                    1: lambda y: y.cols(0).rows(0).structSize(2).members({
                        'a': lambda z: z.cols(3).rows(1).value([276.0, 277.0, 278.0]),
                        'b': lambda z: z.cols(1).rows(1).value([279.0]),
                    }),
                    2: lambda y: y.cols(0).rows(0).structSize(2).members({
                        'a': lambda z: z.cols(3).rows(1).value([280.0, 281.0, 282.0]),
                        'b': lambda z: z.cols(1).rows(1).value([283.0]),
                    }),
                    3: lambda y: y.cols(0).rows(0).structSize(2).members({
                        'a': lambda z: z.cols(3).rows(1).value([284.0, 285.0, 286.0]),
                        'b': lambda z: z.cols(1).rows(1).value([287.0]),
                    }),
                }),
            }),
            # structa[1]
            1: lambda s: s.cols(0).rows(0).structSize(3).members({
                'a': lambda x: x.cols(0).rows(0).structSize(2).members({
                    'a': lambda y: y.cols(3).rows(1).value([288.0, 289.0, 290.0]),
                    'b': lambda y: y.cols(1).rows(1).value([291.0]),
                }),
                'b': lambda x: x.cols(0).rows(0).arraySize(4).members({
                    0: lambda y: y.cols(4).rows(1).value([292.0, 293.0, 294.0, 295.0]),
                    1: lambda y: y.cols(4).rows(1).value([296.0, 297.0, 298.0, 299.0]),
                    2: lambda y: y.cols(4).rows(1).value([300.0, 301.0, 302.0, 303.0]),
                    3: lambda y: y.cols(4).rows(1).value([304.0, 305.0, 306.0, 307.0]),
                }),
                'c': lambda x: x.cols(0).rows(0).arraySize(4).members({
                    0: lambda y: y.cols(0).rows(0).structSize(2).members({
                        'a': lambda z: z.cols(3).rows(1).value([308.0, 309.0, 310.0]),
                        'b': lambda z: z.cols(1).rows(1).value([311.0]),
                    }),
                    1: lambda y: y.cols(0).rows(0).structSize(2).members({
                        'a': lambda z: z.cols(3).rows(1).value([312.0, 313.0, 314.0]),
                        'b': lambda z: z.cols(1).rows(1).value([315.0]),
                    }),
                    2: lambda y: y.cols(0).rows(0).structSize(2).members({
                        'a': lambda z: z.cols(3).rows(1).value([316.0, 317.0, 318.0]),
                        'b': lambda z: z.cols(1).rows(1).value([319.0]),
                    }),
                    3: lambda y: y.cols(0).rows(0).structSize(2).members({
                        'a': lambda z: z.cols(3).rows(1).value([320.0, 321.0, 322.0]),
                        'b': lambda z: z.cols(1).rows(1).value([323.0]),
                    }),
                }),
            }),
        })

        # column_major mat2x3 ac;
        var_check.check('ac').cols(2).rows(3).column_major().value([324.0, 328.0,
                                                                    325.0, 329.0,
                                                                    326.0, 330.0])

        # row_major mat2x3 ad;
        var_check.check('ad').cols(2).rows(3).row_major().value([332.0, 333.0,
                                                                 336.0, 337.0,
                                                                 340.0, 341.0])

        # column_major mat2x3 ae[2];
        var_check.check('ae').cols(0).rows(0).arraySize(2).members({
            0: lambda x: x.cols(2).rows(3).column_major().value([344.0, 348.0,
                                                                 345.0, 349.0,
                                                                 346.0, 350.0]),
            1: lambda x: x.cols(2).rows(3).column_major().value([352.0, 356.0,
                                                                 353.0, 357.0,
                                                                 354.0, 358.0]),
        })

        # row_major mat2x3 af[2];
        var_check.check('af').cols(0).rows(0).arraySize(2).members({
            0: lambda x: x.cols(2).rows(3).row_major().value([360.0, 361.0,
                                                              364.0, 365.0,
                                                              368.0, 369.0]),
            1: lambda x: x.cols(2).rows(3).row_major().value([372.0, 373.0,
                                                              376.0, 377.0,
                                                              380.0, 381.0]),
        })

        # vec2 dummy10;
        var_check.check('dummy10')

        # row_major mat2x2 ag;
        var_check.check('ag').cols(2).rows(2).row_major().value([388.0, 389.0,
                                                                 392.0, 393.0])

        # vec2 dummy11;
        var_check.check('dummy11')

        # column_major mat2x2 ah;
        var_check.check('ah').cols(2).rows(2).column_major().value([400.0, 404.0,
                                                                    401.0, 405.0])

        # row_major mat2x2 ai[2];
        var_check.check('ai').rows(0).cols(0).arraySize(2).members({
            0: lambda x: x.cols(2).rows(2).row_major().value([408.0, 409.0,
                                                              412.0, 413.0]),
            1: lambda x: x.cols(2).rows(2).row_major().value([416.0, 417.0,
                                                              420.0, 421.0]),
        })

        # column_major mat2x2 aj[2];
        var_check.check('aj').rows(0).cols(0).arraySize(2).members({
            0: lambda x: x.cols(2).rows(2).column_major().value([424.0, 428.0,
                                                                 425.0, 429.0]),
            1: lambda x: x.cols(2).rows(2).column_major().value([432.0, 436.0,
                                                                 433.0, 437.0]),
        })

        # struct nested_with_padding
        # {
        #   float a; // vec3 padding
        #   vec4 b;
        #   float c; // vec3 padding
        #   vec3 d[4]; // float padding after each one
        # };
        # nested_with_padding ak[2];
        var_check.check('ak').rows(0).cols(0).arraySize(2).members({
            # ak[0]
            0: lambda s: s.rows(0).cols(0).structSize(4).members({
                'a': lambda y: y.rows(1).cols(1).value([440.0]),
                'b': lambda y: y.rows(1).cols(4).value([444.0, 445.0, 446.0, 447.0]),
                'c': lambda y: y.rows(1).cols(1).value([448.0]),
                'd': lambda x: x.rows(0).cols(0).arraySize(4).members({
                    0: lambda z: z.rows(1).cols(3).value([452.0, 453.0, 454.0]),
                    1: lambda z: z.rows(1).cols(3).value([456.0, 457.0, 458.0]),
                    2: lambda z: z.rows(1).cols(3).value([460.0, 461.0, 462.0]),
                    3: lambda z: z.rows(1).cols(3).value([464.0, 465.0, 466.0]),
                }),
            }),
            # ak[1]
            1: lambda s: s.rows(0).cols(0).structSize(4).members({
                'a': lambda y: y.rows(1).cols(1).value([468.0]),
                'b': lambda y: y.rows(1).cols(4).value([472.0, 473.0, 474.0, 475.0]),
                'c': lambda y: y.rows(1).cols(1).value([476.0]),
                'd': lambda x: x.rows(0).cols(0).arraySize(4).members({
                    0: lambda z: z.rows(1).cols(3).value([480.0, 481.0, 482.0]),
                    1: lambda z: z.rows(1).cols(3).value([484.0, 485.0, 486.0]),
                    2: lambda z: z.rows(1).cols(3).value([488.0, 489.0, 490.0]),
                    3: lambda z: z.rows(1).cols(3).value([492.0, 493.0, 494.0]),
                }),
            }),
        })

        # vec4 dummy12;
        var_check.check('dummy12')

        # float al;
        var_check.check('al').rows(1).cols(1).value([500.0])

        # struct float2_struct
        # {
        #   float x, y;
        # };
        # float2_struct am;
        var_check.check('am').rows(0).cols(0).members({
            'x': lambda y: y.rows(1).cols(1).value([504.0]),
            'y': lambda y: y.rows(1).cols(1).value([505.0]),
        })

        # float an;
        var_check.check('an').rows(1).cols(1).value([508.0])

        # float4 dummy13[2];
        var_check.check('dummy13')

        # misaligned_struct ao[2];
        var_check.check('ao').rows(0).cols(0).arraySize(2).members({
            # ao[0]
            0: lambda s: s.rows(0).cols(0).structSize(2).members({
                'a': lambda y: y.rows(1).cols(4).value([520.0, 521.0, 522.0, 523.0]),
                'b': lambda y: y.rows(1).cols(2).value([524.0, 525.0]),
            }),
            1: lambda s: s.rows(0).cols(0).structSize(2).members({
                'a': lambda y: y.rows(1).cols(4).value([528.0, 529.0, 530.0, 531.0]),
                'b': lambda y: y.rows(1).cols(2).value([532.0, 533.0]),
            }),
        })

        # float4 test;
        var_check.check('test').rows(1).cols(4).value([536.0, 537.0, 538.0, 539.0])

        var_check.done()

    def check_hlsl_cbuffer(self, var_check):
        # For more detailed reference for the below checks, see the commented definition of the cbuffer
        # in the shader source code in the demo itself

        # float4 a;
        var_check.check('a').rows(1).cols(4).value([0.0, 1.0, 2.0, 3.0])

        # float3 b;
        var_check.check('b').rows(1).cols(3).value([4.0, 5.0, 6.0])

        # float2 c; float2 d;
        var_check.check('c').rows(1).cols(2).value([8.0, 9.0])
        var_check.check('d').rows(1).cols(2).value([10.0, 11.0])

        # float e; float3 f;
        var_check.check('e').rows(1).cols(1).value([12.0])
        var_check.check('f').rows(1).cols(3).value([13.0, 14.0, 15.0])

        # float g; float2 h; float i;
        var_check.check('g').rows(1).cols(1).value([16.0])
        var_check.check('h').rows(1).cols(2).value([17.0, 18.0])
        var_check.check('i').rows(1).cols(1).value([19.0])

        # float j; float2 k;
        var_check.check('j').rows(1).cols(1).value([20.0])
        var_check.check('k').rows(1).cols(2).value([21.0, 22.0])

        # float2 l; float m;
        var_check.check('l').rows(1).cols(2).value([24.0, 25.0])
        var_check.check('m').rows(1).cols(1).value([26.0])

        # float n[4];
        var_check.check('n').rows(0).cols(0).arraySize(4).members({
            0: lambda x: x.rows(1).cols(1).value([28.0]),
            1: lambda x: x.rows(1).cols(1).value([32.0]),
            2: lambda x: x.rows(1).cols(1).value([36.0]),
            3: lambda x: x.rows(1).cols(1).value([40.0]),
        })

        # float4 dummy1;
        var_check.check('dummy1')

        # float o[4];
        var_check.check('o').rows(0).cols(0).arraySize(4).members({
            0: lambda x: x.rows(1).cols(1).value([48.0]),
            1: lambda x: x.rows(1).cols(1).value([52.0]),
            2: lambda x: x.rows(1).cols(1).value([56.0]),
            3: lambda x: x.rows(1).cols(1).value([60.0]),
        })

        # float p; with std140 vulkan packing
        var_check.check('p').rows(1).cols(1).value([64.0])

        # float4 dummy2;
        var_check.check('dummy2')

        # float4 gldummy;
        var_check.check('gldummy')

        # HLSL majorness is flipped to match column-major SPIR-V with row-major HLSL.
        # This means column major declared matrices will show up as row major in any reflection and SPIR-V
        # it also means that dimensions are flipped, so a float3x4 is declared as a float4x3, and a 'row'
        # is really a column, and vice-versa a 'column' is really a row.

        # column_major float4x4 q;
        var_check.check('q').rows(4).cols(4).row_major().value([76.0, 77.0, 78.0, 79.0,
                                                                80.0, 81.0, 82.0, 83.0,
                                                                84.0, 85.0, 86.0, 87.0,
                                                                88.0, 89.0, 90.0, 91.0])

        # row_major float4x4 r;
        var_check.check('r').rows(4).cols(4).column_major().value([92.0, 96.0, 100.0, 104.0,
                                                                   93.0, 97.0, 101.0, 105.0,
                                                                   94.0, 98.0, 102.0, 106.0,
                                                                   95.0, 99.0, 103.0, 107.0])

        # column_major float3x4 s;
        var_check.check('s').rows(4).cols(3).row_major().value([108.0, 109.0, 110.0,
                                                                112.0, 113.0, 114.0,
                                                                116.0, 117.0, 118.0,
                                                                120.0, 121.0, 122.0])

        # float4 dummy3;
        var_check.check('dummy3')

        # row_major float3x4 t;
        var_check.check('t').rows(4).cols(3).column_major().value([128.0, 132.0, 136.0,
                                                                   129.0, 133.0, 137.0,
                                                                   130.0, 134.0, 138.0,
                                                                   131.0, 135.0, 139.0])

        # float4 dummy4;
        var_check.check('dummy4')

        # column_major float2x3 u;
        var_check.check('u').rows(3).cols(2).row_major().value([144.0, 145.0,
                                                                148.0, 149.0,
                                                                152.0, 153.0])

        # float4 dummy5;
        var_check.check('dummy5')

        # row_major float2x3 v;
        var_check.check('v').rows(3).cols(2).column_major().value([160.0, 164.0,
                                                                   161.0, 165.0,
                                                                   162.0, 166.0])

        # float4 dummy6;
        var_check.check('dummy6')

        # column_major float2x2 w;
        var_check.check('w').rows(2).cols(2).row_major().value([172.0, 173.0,
                                                                176.0, 177.0])

        # float4 dummy7;
        var_check.check('dummy7')

        # row_major float2x2 x;
        var_check.check('x').rows(2).cols(2).column_major().value([184.0, 188.0,
                                                                   185.0, 189.0])

        # float4 dummy8;
        var_check.check('dummy8')

        # row_major float2x2 y;
        var_check.check('y').rows(2).cols(2).column_major().value([196.0, 200.0,
                                                                   197.0, 201.0])

        # float z;
        var_check.check('z').rows(1).cols(1).value([204.0])

        # Temporarily until SPIR-V support for degenerate HLSL matrices is determined
        var_check.check('dummy9')

        # row_major float4x1 aa;
        #var_check.check('aa').rows(1).cols(4).value([208.0, 212.0, 216.0, 220.0])

        # column_major float4x1 ab;
        #var_check.check('ab').rows(1).cols(4).value([224.0, 225.0, 226.0, 227.0])

        # float4 multiarray[3][2];
        var_check.check('multiarray').cols(0).rows(0).arraySize(3).members({
            0: lambda x: x.cols(0).rows(0).arraySize(2).members({
                0: lambda y: y.cols(4).rows(1).value([228.0, 229.0, 230.0, 231.0]),
                1: lambda y: y.cols(4).rows(1).value([232.0, 233.0, 234.0, 235.0]),
            }),
            1: lambda x: x.cols(0).rows(0).arraySize(2).members({
                0: lambda y: y.cols(4).rows(1).value([236.0, 237.0, 238.0, 239.0]),
                1: lambda y: y.cols(4).rows(1).value([240.0, 241.0, 242.0, 243.0]),
            }),
            2: lambda x: x.cols(0).rows(0).arraySize(2).members({
                0: lambda y: y.cols(4).rows(1).value([244.0, 245.0, 246.0, 247.0]),
                1: lambda y: y.cols(4).rows(1).value([248.0, 249.0, 250.0, 251.0]),
            }),
        })

        # struct float3_1 { float3 a; float b; };
        # struct nested { float3_1 a; float4 b[4]; float3_1 c[4]; };
        # nested structa[2];
        var_check.check('structa').rows(0).cols(0).arraySize(2).members({
            # structa[0]
            0: lambda s: s.rows(0).cols(0).structSize(3).members({
                'a': lambda x: x.rows(0).cols(0).structSize(2).members({
                    'a': lambda y: y.rows(1).cols(3).value([252.0, 253.0, 254.0]),
                    'b': lambda y: y.rows(1).cols(1).value([255.0]),
                }),
                'b': lambda x: x.rows(0).cols(0).arraySize(4).members({
                    0: lambda y: y.rows(1).cols(4).value([256.0, 257.0, 258.0, 259.0]),
                    1: lambda y: y.rows(1).cols(4).value([260.0, 261.0, 262.0, 263.0]),
                    2: lambda y: y.rows(1).cols(4).value([264.0, 265.0, 266.0, 267.0]),
                    3: lambda y: y.rows(1).cols(4).value([268.0, 269.0, 270.0, 271.0]),
                }),
                'c': lambda x: x.rows(0).cols(0).arraySize(4).members({
                    0: lambda y: y.rows(0).cols(0).structSize(2).members({
                        'a': lambda z: z.rows(1).cols(3).value([272.0, 273.0, 274.0]),
                        'b': lambda z: z.rows(1).cols(1).value([275.0]),
                    }),
                    1: lambda y: y.rows(0).cols(0).structSize(2).members({
                        'a': lambda z: z.rows(1).cols(3).value([276.0, 277.0, 278.0]),
                        'b': lambda z: z.rows(1).cols(1).value([279.0]),
                    }),
                    2: lambda y: y.rows(0).cols(0).structSize(2).members({
                        'a': lambda z: z.rows(1).cols(3).value([280.0, 281.0, 282.0]),
                        'b': lambda z: z.rows(1).cols(1).value([283.0]),
                    }),
                    3: lambda y: y.rows(0).cols(0).structSize(2).members({
                        'a': lambda z: z.rows(1).cols(3).value([284.0, 285.0, 286.0]),
                        'b': lambda z: z.rows(1).cols(1).value([287.0]),
                    }),
                }),
            }),
            # structa[1]
            1: lambda s: s.rows(0).cols(0).structSize(3).members({
                'a': lambda x: x.rows(0).cols(0).structSize(2).members({
                    'a': lambda y: y.rows(1).cols(3).value([288.0, 289.0, 290.0]),
                    'b': lambda y: y.rows(1).cols(1).value([291.0]),
                }),
                'b': lambda x: x.rows(0).cols(0).arraySize(4).members({
                    0: lambda y: y.rows(1).cols(4).value([292.0, 293.0, 294.0, 295.0]),
                    1: lambda y: y.rows(1).cols(4).value([296.0, 297.0, 298.0, 299.0]),
                    2: lambda y: y.rows(1).cols(4).value([300.0, 301.0, 302.0, 303.0]),
                    3: lambda y: y.rows(1).cols(4).value([304.0, 305.0, 306.0, 307.0]),
                }),
                'c': lambda x: x.rows(0).cols(0).arraySize(4).members({
                    0: lambda y: y.rows(0).cols(0).structSize(2).members({
                        'a': lambda z: z.rows(1).cols(3).value([308.0, 309.0, 310.0]),
                        'b': lambda z: z.rows(1).cols(1).value([311.0]),
                    }),
                    1: lambda y: y.rows(0).cols(0).structSize(2).members({
                        'a': lambda z: z.rows(1).cols(3).value([312.0, 313.0, 314.0]),
                        'b': lambda z: z.rows(1).cols(1).value([315.0]),
                    }),
                    2: lambda y: y.rows(0).cols(0).structSize(2).members({
                        'a': lambda z: z.rows(1).cols(3).value([316.0, 317.0, 318.0]),
                        'b': lambda z: z.rows(1).cols(1).value([319.0]),
                    }),
                    3: lambda y: y.rows(0).cols(0).structSize(2).members({
                        'a': lambda z: z.rows(1).cols(3).value([320.0, 321.0, 322.0]),
                        'b': lambda z: z.rows(1).cols(1).value([323.0]),
                    }),
                }),
            }),
        })

        # column_major float3x2 ac;
        var_check.check('ac').rows(2).cols(3).row_major().value([324.0, 325.0, 326.0,
                                                                 328.0, 329.0, 330.0])

        # row_major float3x2 ad;
        var_check.check('ad').rows(2).cols(3).column_major().value([332.0, 336.0, 340.0,
                                                                    333.0, 337.0, 341.0])

        # column_major float3x2 ae[2];
        var_check.check('ae').rows(0).cols(0).arraySize(2).members({
            0: lambda x: x.rows(2).cols(3).row_major().value([344.0, 345.0, 346.0,
                                                              348.0, 349.0, 350.0]),
            1: lambda x: x.rows(2).cols(3).row_major().value([352.0, 353.0, 354.0,
                                                              356.0, 357.0, 358.0]),
        })

        # row_major float3x2 af[2];
        var_check.check('af').rows(0).cols(0).arraySize(2).members({
            0: lambda x: x.rows(2).cols(3).column_major().value([360.0, 364.0, 368.0,
                                                                 361.0, 365.0, 369.0]),
            1: lambda x: x.rows(2).cols(3).column_major().value([372.0, 376.0, 380.0,
                                                                 373.0, 377.0, 381.0]),
        })

        # float2 dummy10;
        var_check.check('dummy10')

        # float2 dummy11;
        var_check.check('dummy11')

        # row_major float2x2 ag;
        var_check.check('ag').rows(2).cols(2).column_major().value([388.0, 392.0,
                                                                    389.0, 393.0])

        # float2 dummy12;
        var_check.check('dummy12')

        # float2 dummy13;
        var_check.check('dummy13')

        # column_major float2x2 ah;
        var_check.check('ah').rows(2).cols(2).row_major().value([400.0, 401.0,
                                                                 404.0, 405.0])

        # row_major float2x2 ai[2];
        var_check.check('ai').rows(0).cols(0).arraySize(2).members({
            0: lambda x: x.rows(2).cols(2).column_major().value([408.0, 412.0,
                                                                 409.0, 413.0]),
            1: lambda x: x.rows(2).cols(2).column_major().value([416.0, 420.0,
                                                                 417.0, 421.0]),
        })

        # column_major float2x2 aj[2];
        var_check.check('aj').rows(0).cols(0).arraySize(2).members({
            0: lambda x: x.rows(2).cols(2).row_major().value([424.0, 425.0,
                                                              428.0, 429.0]),
            1: lambda x: x.rows(2).cols(2).row_major().value([432.0, 433.0,
                                                              436.0, 437.0]),
        })

        # struct nested_with_padding
        # {
        #   float a; // float3 padding
        #   float4 b;
        #   float c; // float3 padding
        #   float3 d[4]; // float padding after each one
        # };
        # nested_with_padding ak[2];
        var_check.check('ak').rows(0).cols(0).arraySize(2).members({
            # ak[0]
            0: lambda s: s.rows(0).cols(0).structSize(4).members({
                'a': lambda y: y.rows(1).cols(1).value([440.0]),
                'b': lambda y: y.rows(1).cols(4).value([444.0, 445.0, 446.0, 447.0]),
                'c': lambda y: y.rows(1).cols(1).value([448.0]),
                'd': lambda x: x.rows(0).cols(0).arraySize(4).members({
                    0: lambda z: z.rows(1).cols(3).value([452.0, 453.0, 454.0]),
                    1: lambda z: z.rows(1).cols(3).value([456.0, 457.0, 458.0]),
                    2: lambda z: z.rows(1).cols(3).value([460.0, 461.0, 462.0]),
                    3: lambda z: z.rows(1).cols(3).value([464.0, 465.0, 466.0]),
                }),
            }),
            # ak[1]
            1: lambda s: s.rows(0).cols(0).structSize(4).members({
                'a': lambda y: y.rows(1).cols(1).value([468.0]),
                'b': lambda y: y.rows(1).cols(4).value([472.0, 473.0, 474.0, 475.0]),
                'c': lambda y: y.rows(1).cols(1).value([476.0]),
                'd': lambda x: x.rows(0).cols(0).arraySize(4).members({
                    0: lambda z: z.rows(1).cols(3).value([480.0, 481.0, 482.0]),
                    1: lambda z: z.rows(1).cols(3).value([484.0, 485.0, 486.0]),
                    2: lambda z: z.rows(1).cols(3).value([488.0, 489.0, 490.0]),
                    3: lambda z: z.rows(1).cols(3).value([492.0, 493.0, 494.0]),
                }),
            }),
        })

        # float4 dummy14;
        var_check.check('dummy14')

        # float al;
        var_check.check('al').rows(1).cols(1).value([500.0])

        # struct float2_struct
        # {
        #   float x, y;
        # };
        # float2_struct am;
        var_check.check('am').rows(0).cols(0).members({
            'x': lambda y: y.rows(1).cols(1).value([504.0]),
            'y': lambda y: y.rows(1).cols(1).value([505.0]),
        })

        # float an;
        var_check.check('an').rows(1).cols(1).value([508.0])

        # float4 dummy15[2];
        var_check.check('dummy15')

        # misaligned_struct ao[2];
        var_check.check('ao').rows(0).cols(0).arraySize(2).members({
            # ao[0]
            0: lambda s: s.rows(0).cols(0).structSize(2).members({
                'a': lambda y: y.rows(1).cols(4).value([520.0, 521.0, 522.0, 523.0]),
                'b': lambda y: y.rows(1).cols(2).value([524.0, 525.0]),
            }),
            1: lambda s: s.rows(0).cols(0).structSize(2).members({
                'a': lambda y: y.rows(1).cols(4).value([528.0, 529.0, 530.0, 531.0]),
                'b': lambda y: y.rows(1).cols(2).value([532.0, 533.0]),
            }),
        })

        # float4 test;
        var_check.check('test').rows(1).cols(4).value([536.0, 537.0, 538.0, 539.0])

        var_check.done()

