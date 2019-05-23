import rdtest
import renderdoc as rd


class GL_CBuffer_Zoo(rdtest.TestCase):
    demos_test_name = 'GL_CBuffer_Zoo'

    def check_capture(self):
        draw = self.find_draw("Draw")

        self.check(draw is not None)

        self.controller.SetFrameEvent(draw.eventId, False)

        # Make an output so we can pick pixels
        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        self.check(out is not None)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        stage = rd.ShaderStage.Pixel
        cbuf: rd.BoundCBuffer = pipe.GetConstantBuffer(stage, 0, 0)

        var_check = rdtest.ConstantBufferChecker(
            self.controller.GetCBufferVariableContents(pipe.GetShader(stage),
                                                       pipe.GetShaderEntryPoint(stage), 0,
                                                       cbuf.resourceId, cbuf.byteOffset))

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

        # vec2 dummy12;
        var_check.check('dummy11')

        # column_major float2x2 ah;
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

        # vec4 test;
        var_check.check('test').rows(1).cols(4).value([440.0, 441.0, 442.0, 443.0])

        # to save duplicating if this array changes, we calculate out from the start, as the array is tightly packed
        base = 444.0

        exp_vals = lambda wi,yi,xi: [base + wi * 24.0 + yi * 8.0 + xi * 4.0 + c * 1.0 for c in range(0,4)]

        # vec4 multiarray2[4][3][2];
        var_check.check('multiarray2').cols(0).rows(0).arraySize(4).members({
            0: lambda w: w.cols(0).rows(0).arraySize(3).members({
                0: lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda y: y.cols(4).rows(1).value(exp_vals(0, 0, 0)),
                    1: lambda y: y.cols(4).rows(1).value(exp_vals(0, 0, 1)),
                }),
                1: lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda y: y.cols(4).rows(1).value(exp_vals(0, 1, 0)),
                    1: lambda y: y.cols(4).rows(1).value(exp_vals(0, 1, 1)),
                }),
                2: lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda y: y.cols(4).rows(1).value(exp_vals(0, 2, 0)),
                    1: lambda y: y.cols(4).rows(1).value(exp_vals(0, 2, 1)),
                }),
            }),
            1: lambda w: w.cols(0).rows(0).arraySize(3).members({
                0: lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda y: y.cols(4).rows(1).value(exp_vals(1, 0, 0)),
                    1: lambda y: y.cols(4).rows(1).value(exp_vals(1, 0, 1)),
                }),
                1: lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda y: y.cols(4).rows(1).value(exp_vals(1, 1, 0)),
                    1: lambda y: y.cols(4).rows(1).value(exp_vals(1, 1, 1)),
                }),
                2: lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda y: y.cols(4).rows(1).value(exp_vals(1, 2, 0)),
                    1: lambda y: y.cols(4).rows(1).value(exp_vals(1, 2, 1)),
                }),
            }),
            2: lambda w: w.cols(0).rows(0).arraySize(3).members({
                0: lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda y: y.cols(4).rows(1).value(exp_vals(2, 0, 0)),
                    1: lambda y: y.cols(4).rows(1).value(exp_vals(2, 0, 1)),
                }),
                1: lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda y: y.cols(4).rows(1).value(exp_vals(2, 1, 0)),
                    1: lambda y: y.cols(4).rows(1).value(exp_vals(2, 1, 1)),
                }),
                2: lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda y: y.cols(4).rows(1).value(exp_vals(2, 2, 0)),
                    1: lambda y: y.cols(4).rows(1).value(exp_vals(2, 2, 1)),
                }),
            }),
            3: lambda w: w.cols(0).rows(0).arraySize(3).members({
                0: lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda y: y.cols(4).rows(1).value(exp_vals(3, 0, 0)),
                    1: lambda y: y.cols(4).rows(1).value(exp_vals(3, 0, 1)),
                }),
                1: lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda y: y.cols(4).rows(1).value(exp_vals(3, 1, 0)),
                    1: lambda y: y.cols(4).rows(1).value(exp_vals(3, 1, 1)),
                }),
                2: lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda y: y.cols(4).rows(1).value(exp_vals(3, 2, 0)),
                    1: lambda y: y.cols(4).rows(1).value(exp_vals(3, 2, 1)),
                }),
            }),
        })

        var_check.done()

        rdtest.log.success("CBuffer variables are as expected")

        tex = rd.TextureDisplay()
        tex.resourceId = pipe.GetOutputTargets()[0].resourceId
        out.SetTextureDisplay(tex)

        texdetails = self.get_texture(tex.resourceId)

        picked: rd.PixelValue = out.PickPixel(tex.resourceId, False,
                                              int(texdetails.width / 2), int(texdetails.height / 2), 0, 0, 0)

        if not rdtest.value_compare(picked.floatValue, [440.1, 441.0, 442.0, 443.0]):
            raise rdtest.TestFailureException("Picked value {} doesn't match expectation".format(picked.floatValue))

        rdtest.log.success("Picked value is as expected")

        cbuf: rd.BoundCBuffer = pipe.GetConstantBuffer(stage, 1, 0)

        var_check = rdtest.ConstantBufferChecker(
            self.controller.GetCBufferVariableContents(pipe.GetShader(stage),
                                                       pipe.GetShaderEntryPoint(stage), 1,
                                                       cbuf.resourceId, cbuf.byteOffset))

        # For bare uniforms we have partial data - only values used in the shader need to get assigned locations and
        # some drivers are aggressive about stripping any others. Only uniforms with locations get upload values.
        # Hence some of these checks don't check for the value - that means they might not be present

        # vec4 A;
        var_check.check('A').cols(4).rows(1).value([10.0, 20.0, 30.0, 40.0])

        # vec2 B;
        var_check.check('B').cols(2).rows(1).value([50.0, 60.0])

        # vec3 C;
        var_check.check('C').cols(3).rows(1).value([70.0, 80.0, 90.0])

        # mat2x3 D;
        var_check.check('D').cols(2).rows(3).value([100.0, 130.0,
                                                    110.0, 140.0,
                                                    120.0, 150.0])

        # float E[3];
        var_check.check('E').cols(0).rows(0).arraySize(3).members({
            0: lambda x: x.cols(1).rows(1).value([160.0]),
            1: lambda x: x.cols(1).rows(1).value([170.0]),
            2: lambda x: x.cols(1).rows(1).value([180.0]),
        })

        # vec4 F[3][2][2];
        # Multidimensional arrays are represented as structs with N members
        # Due to lacking reflection, we skip structSize() checks, but check everything else if it's present (if it's
        # not, it will be skipped)
        var_check.check('F').cols(0).rows(0).members({
            '[0]': lambda x: x.cols(0).rows(0).members({
                '[0]': lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda x: x.cols(4).rows(1).value([190.0, 200.0, 210.0, 220.0]),
                    1: lambda x: x.cols(4).rows(1).value([230.0, 240.0, 250.0, 260.0]),
                }),
                '[1]': lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda x: x.cols(4).rows(1).value([270.0, 280.0, 290.0, 300.0]),
                    1: lambda x: x.cols(4).rows(1).value([310.0, 320.0, 330.0, 340.0]),
                }),
            }),
            '[1]': lambda x: x.cols(0).rows(0).members({
                '[0]': lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda x: x.cols(4).rows(1).value([350.0, 360.0, 370.0, 380.0]),
                    1: lambda x: x.cols(4).rows(1).value([390.0, 400.0, 410.0, 420.0]),
                }),
                '[1]': lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda x: x.cols(4).rows(1).value([430.0, 440.0, 450.0, 460.0]),
                    1: lambda x: x.cols(4).rows(1).value([470.0, 480.0, 490.0, 500.0]),
                }),
            }),
            '[2]': lambda x: x.cols(0).rows(0).members({
                '[0]': lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda x: x.cols(4).rows(1).value([510.0, 520.0, 530.0, 540.0]),
                    1: lambda x: x.cols(4).rows(1).value([550.0, 560.0, 570.0, 580.0]),
                }),
                '[1]': lambda x: x.cols(0).rows(0).arraySize(2).members({
                    0: lambda x: x.cols(4).rows(1).value([590.0, 600.0, 610.0, 620.0]),
                    1: lambda x: x.cols(4).rows(1).value([630.0, 640.0, 650.0, 660.0]),
                }),
            }),
        })

        # struct vec3_1 { vec3 a; float b; };
        # struct nested { vec3_1 a; vec4 b[4]; vec3_1 c[4]; };
        # nested G[2];
        # Due to lacking reflection, we don't know that G[x].a.a exists, as we only reference G[n].a.b
        # Similarly we don't know that G[x].c[y].b exists
        var_check.check('G').cols(0).rows(0).arraySize(2).members({
            # G[0]
            0: lambda s: s.cols(0).rows(0).structSize(3).members({
                'a': lambda x: x.cols(0).rows(0).members({
                    'a': lambda y: y.cols(3).rows(1).value([680.0, 690.0, 700.0]),
                    'b': lambda y: y.cols(1).rows(1).value([710.0]),
                }),
                'b': lambda x: x.cols(0).rows(0).arraySize(4).members({
                    0: lambda y: y.cols(4).rows(1),
                    1: lambda y: y.cols(4).rows(1),
                    2: lambda y: y.cols(4).rows(1),
                    3: lambda y: y.cols(4).rows(1),
                }),
                'c': lambda x: x.cols(0).rows(0).arraySize(4).members({
                    0: lambda y: y.cols(0).rows(0).members({
                        'a': lambda z: z.cols(3).rows(1),
                        'b': lambda z: z.cols(1).rows(1),
                    }),
                    1: lambda y: y.cols(0).rows(0).members({
                        'a': lambda z: z.cols(3).rows(1),
                        'b': lambda z: z.cols(1).rows(1),
                    }),
                    2: lambda y: y.cols(0).rows(0).members({
                        'a': lambda z: z.cols(3).rows(1),
                        'b': lambda z: z.cols(1).rows(1),
                    }),
                    3: lambda y: y.cols(0).rows(0).members({
                        'a': lambda z: z.cols(3).rows(1),
                        'b': lambda z: z.cols(1).rows(1),
                    }),
                }),
            }),
            # G[1]
            1: lambda s: s.cols(0).rows(0).structSize(3).members({
                'a': lambda x: x.cols(0).rows(0).members({
                    'a': lambda y: y.cols(3).rows(1).value([1040.0, 1050.0, 1060.0]),
                    'b': lambda y: y.cols(1).rows(1).value([1070.0]),
                }),
                'b': lambda x: x.cols(0).rows(0).arraySize(4).members({
                    0: lambda y: y.cols(4).rows(1).value([1080.0, 1090.0, 1100.0, 1110.0]),
                    1: lambda y: y.cols(4).rows(1).value([1120.0, 1130.0, 1140.0, 1150.0]),
                    2: lambda y: y.cols(4).rows(1).value([1160.0, 1170.0, 1180.0, 1190.0]),
                    3: lambda y: y.cols(4).rows(1).value([1200.0, 1210.0, 1220.0, 1230.0]),
                }),
                'c': lambda x: x.cols(0).rows(0).arraySize(4).members({
                    0: lambda y: y.cols(0).rows(0).members({
                        'a': lambda z: z.cols(3).rows(1),
                        'b': lambda z: z.cols(1).rows(1),
                    }),
                    1: lambda y: y.cols(0).rows(0).members({
                        'a': lambda z: z.cols(3).rows(1),
                        'b': lambda z: z.cols(1).rows(1),
                    }),
                    2: lambda y: y.cols(0).rows(0).members({
                        'a': lambda z: z.cols(3).rows(1),
                        'b': lambda z: z.cols(1).rows(1),
                    }),
                    3: lambda y: y.cols(0).rows(0).members({
                        'a': lambda z: z.cols(3).rows(1).value([1360.0, 1370.0, 1380.0]),
                        'b': lambda z: z.cols(1).rows(1),
                    }),
                }),
            }),
        })

        var_check.done()

        rdtest.log.success("Bare uniform variables are as expected")

        out.Shutdown()
