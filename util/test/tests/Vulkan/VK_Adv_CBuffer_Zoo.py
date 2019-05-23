import rdtest
import renderdoc as rd


class VK_Adv_CBuffer_Zoo(rdtest.TestCase):
    demos_test_name = 'VK_Adv_CBuffer_Zoo'

    def check_capture(self):
        draw = self.find_draw("Draw")

        self.check(draw is not None)

        self.controller.SetFrameEvent(draw.eventId, False)

        # Make an output so we can pick pixels
        out: rd.ReplayOutput = self.controller.CreateOutput(rd.CreateHeadlessWindowingData(100, 100), rd.ReplayOutputType.Texture)

        self.check(out is not None)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        stage = rd.ShaderStage.Vertex

        cbuf: rd.BoundCBuffer = pipe.GetConstantBuffer(stage, 0, 0)

        var_check = rdtest.ConstantBufferChecker(
            self.controller.GetCBufferVariableContents(pipe.GetShader(stage),
                                                       pipe.GetShaderEntryPoint(stage), 0,
                                                       cbuf.resourceId, cbuf.byteOffset))

        # For more detailed reference for the below checks, see the commented definition of the cbuffer
        # in the shader source code in the demo itself

        # float a;
        var_check.check('a').cols(1).rows(1).type(rd.VarType.Float).value([1.0])

        # vec2 b;
        var_check.check('b').cols(2).rows(1).type(rd.VarType.Float).value([2.0, 0.0])

        # vec3 c;
        var_check.check('c').cols(3).rows(1).type(rd.VarType.Float).value([0.0, 3.0])

        # float d[2];
        var_check.check('d').cols(0).rows(0).arraySize(2).members({
            0: lambda x: x.cols(1).rows(1).type(rd.VarType.Float).value([4.0]),
            1: lambda x: x.cols(1).rows(1).type(rd.VarType.Float).value([5.0]),
        })

        # mat2x3 e;
        var_check.check('e').cols(2).rows(3).column_major().type(rd.VarType.Float).value([6.0, 999.0,
                                                                                          7.0, 0.0,
                                                                                          0.0, 0.0])

        # mat2x3 f[2];
        var_check.check('f').cols(0).rows(0).arraySize(2).members({
            0: lambda x: x.cols(2).rows(3).column_major().type(rd.VarType.Float).value([8.0, 999.0,
                                                                                        9.0, 0.0,
                                                                                        0.0, 0.0]),
            1: lambda x: x.cols(2).rows(3).column_major().type(rd.VarType.Float).value([10.0, 999.0,
                                                                                        11.0, 0.0,
                                                                                        0.0, 0.0]),
        })

        # float g;
        var_check.check('g').cols(1).rows(1).type(rd.VarType.Float).value([12.0])

        # struct S
        # {
        #   float a;
        #   vec2 b;
        #   double c;
        #   float d;
        #   vec3 e;
        #   float f;
        # };
        # S h;

        var_check.check('h').cols(0).rows(0).structSize(6).members({
            'a': lambda x: x.cols(1).rows(1).type(rd.VarType.Float ).value([0.0]),
            'b': lambda x: x.cols(2).rows(1).type(rd.VarType.Float ).value([0.0]),
            'c': lambda x: x.cols(1).rows(1).type(rd.VarType.Double).longvalue([13.0]),
            'd': lambda x: x.cols(1).rows(1).type(rd.VarType.Float ).value([14.0]),
            'e': lambda x: x.cols(3).rows(1).type(rd.VarType.Float ).value([0.0]),
            'f': lambda x: x.cols(1).rows(1).type(rd.VarType.Float ).value([0.0]),
        })

        # S i[2];
        var_check.check('i').cols(0).rows(0).arraySize(2).members({
            0: lambda x: x.cols(0).rows(0).structSize(6).members({
                'a': lambda x: x.cols(1).rows(1).type(rd.VarType.Float ).value([0.0]),
                'b': lambda x: x.cols(2).rows(1).type(rd.VarType.Float ).value([0.0]),
                'c': lambda x: x.cols(1).rows(1).type(rd.VarType.Double).longvalue([15.0]),
                'd': lambda x: x.cols(1).rows(1).type(rd.VarType.Float ).value([0.0]),
                'e': lambda x: x.cols(3).rows(1).type(rd.VarType.Float ).value([0.0]),
                'f': lambda x: x.cols(1).rows(1).type(rd.VarType.Float ).value([0.0]),
            }),
            1: lambda x: x.cols(0).rows(0).structSize(6).members({
                'a': lambda x: x.cols(1).rows(1).type(rd.VarType.Float ).value([0.0]),
                'b': lambda x: x.cols(2).rows(1).type(rd.VarType.Float ).value([0.0]),
                'c': lambda x: x.cols(1).rows(1).type(rd.VarType.Double).longvalue([0.0]),
                'd': lambda x: x.cols(1).rows(1).type(rd.VarType.Float ).value([16.0]),
                'e': lambda x: x.cols(3).rows(1).type(rd.VarType.Float ).value([0.0]),
                'f': lambda x: x.cols(1).rows(1).type(rd.VarType.Float ).value([0.0]),
            }),
        })

        # i8vec4 pad1;
        var_check.check('pad1')

        # int8_t j;
        var_check.check('j').cols(1).rows(1).type(rd.VarType.SByte).value([17])

        # struct S8
        # {
        #   int8_t a;
        #   i8vec4 b;
        #   i8vec2 c[4];
        # };
        # S8 k;
        var_check.check('k').cols(0).rows(0).structSize(3).members({
            'a': lambda x: x.cols(1).rows(1).type(rd.VarType.SByte).value([0]),
            'b': lambda x: x.cols(4).rows(1).type(rd.VarType.SByte).value([0, 0, 0, 0]),
            'c': lambda x: x.cols(0).rows(0).arraySize(4).members({
                0: lambda x: x.cols(2).rows(1).type(rd.VarType.SByte).value([0, 0]),
                1: lambda x: x.cols(2).rows(1).type(rd.VarType.SByte).value([0, 18]),
                2: lambda x: x.cols(2).rows(1).type(rd.VarType.SByte).value([0, 0]),
                3: lambda x: x.cols(2).rows(1).type(rd.VarType.SByte).value([0, 0]),
            }),
        })

        # S8 l[2];
        var_check.check('l').cols(0).rows(0).arraySize(2).members({
            0: lambda x: x.cols(0).rows(0).structSize(3).members({
                'a': lambda x: x.cols(1).rows(1).type(rd.VarType.SByte).value([19]),
                'b': lambda x: x.cols(4).rows(1).type(rd.VarType.SByte).value([0, 0, 0, 0]),
                'c': lambda x: x.cols(0).rows(0).arraySize(4).members({
                    0: lambda x: x.cols(2).rows(1).type(rd.VarType.SByte).value([0, 0]),
                    1: lambda x: x.cols(2).rows(1).type(rd.VarType.SByte).value([0, 20]),
                    2: lambda x: x.cols(2).rows(1).type(rd.VarType.SByte).value([0, 0]),
                    3: lambda x: x.cols(2).rows(1).type(rd.VarType.SByte).value([0, 0]),
                }),
            }),
            1: lambda x: x.cols(0).rows(0).structSize(3).members({
                'a': lambda x: x.cols(1).rows(1).type(rd.VarType.SByte).value([21]),
                'b': lambda x: x.cols(4).rows(1).type(rd.VarType.SByte).value([0, 0, 0, 0]),
                'c': lambda x: x.cols(0).rows(0).arraySize(4).members({
                    0: lambda x: x.cols(2).rows(1).type(rd.VarType.SByte).value([0, 22]),
                    1: lambda x: x.cols(2).rows(1).type(rd.VarType.SByte).value([0, 0]),
                    2: lambda x: x.cols(2).rows(1).type(rd.VarType.SByte).value([0, 0]),
                    3: lambda x: x.cols(2).rows(1).type(rd.VarType.SByte).value([0, 0]),
                }),
            })
        })

        # int8_t m;
        var_check.check('m').cols(1).rows(1).type(rd.VarType.SByte).value([-23])

        # struct S16
        # {
        #   uint16_t a;
        #   i16vec4 b;
        #   i16vec2 c[4];
        #   int8_t d;
        # };
        # S16 n;
        var_check.check('n').cols(0).rows(0).structSize(4).members({
            'a': lambda x: x.cols(1).rows(1).type(rd.VarType.UShort).value([65524]),
            'b': lambda x: x.cols(4).rows(1).type(rd.VarType.SShort).value([0, 0, 0, -2424]),
            'c': lambda x: x.cols(0).rows(0).arraySize(4).members({
                0: lambda x: x.cols(2).rows(1).type(rd.VarType.SShort).value([0, 0]),
                1: lambda x: x.cols(2).rows(1).type(rd.VarType.SShort).value([0, 0]),
                2: lambda x: x.cols(2).rows(1).type(rd.VarType.SShort).value([0, 0]),
                3: lambda x: x.cols(2).rows(1).type(rd.VarType.SShort).value([0, 0]),
            }),
            'd': lambda x: x.cols(1).rows(1).type(rd.VarType.SByte).value([25]),
        })

        # i8vec4 pad2;
        var_check.check('pad2')

        # uint8_t o;
        var_check.check('o').cols(1).rows(1).type(rd.VarType.UByte).value([226])

        # S16 p[2];
        var_check.check('p').cols(0).rows(0).arraySize(2).members({
            0: lambda x: x.cols(0).rows(0).structSize(4).members({
                'a': lambda x: x.cols(1).rows(1).type(rd.VarType.UShort).value([0]),
                'b': lambda x: x.cols(4).rows(1).type(rd.VarType.SShort).value([0, 0, 2727, 0]),
                'c': lambda x: x.cols(0).rows(0).arraySize(4).members({
                    0: lambda x: x.cols(2).rows(1).type(rd.VarType.SShort).value([0, 0]),
                    1: lambda x: x.cols(2).rows(1).type(rd.VarType.SShort).value([0, 0]),
                    2: lambda x: x.cols(2).rows(1).type(rd.VarType.SShort).value([0, 0]),
                    3: lambda x: x.cols(2).rows(1).type(rd.VarType.SShort).value([0, 0]),
                }),
                'd': lambda x: x.cols(1).rows(1).type(rd.VarType.SByte).value([28]),
            }),
            1: lambda x: x.cols(0).rows(0).structSize(4).members({
                'a': lambda x: x.cols(1).rows(1).type(rd.VarType.UShort).value([0]),
                'b': lambda x: x.cols(4).rows(1).type(rd.VarType.SShort).value([0, 0, 0, 2929]),
                'c': lambda x: x.cols(0).rows(0).arraySize(4).members({
                    0: lambda x: x.cols(2).rows(1).type(rd.VarType.SShort).value([0, 0]),
                    1: lambda x: x.cols(2).rows(1).type(rd.VarType.SShort).value([0, 0]),
                    2: lambda x: x.cols(2).rows(1).type(rd.VarType.SShort).value([0, 0]),
                    3: lambda x: x.cols(2).rows(1).type(rd.VarType.SShort).value([0, 0]),
                }),
                'd': lambda x: x.cols(1).rows(1).type(rd.VarType.SByte).value([0]),
            })
        })

        # i8vec4 pad3;
        var_check.check('pad3')

        # uint64_t q;
        var_check.check('q').cols(1).rows(1).type(rd.VarType.ULong).longvalue([30303030303030])

        # int64_t r;
        var_check.check('r').cols(1).rows(1).type(rd.VarType.SLong).longvalue([-31313131313131])

        # half s;
        var_check.check('s').cols(1).rows(1).type(rd.VarType.Half).value([16.25])

        # int8_t test;
        var_check.check('test').cols(1).rows(1).type(rd.VarType.SByte).value([42])

        var_check.done()

        rdtest.log.success("CBuffer variables are as expected")

        tex = rd.TextureDisplay()
        tex.resourceId = pipe.GetOutputTargets()[0].resourceId
        out.SetTextureDisplay(tex)

        texdetails = self.get_texture(tex.resourceId)

        picked: rd.PixelValue = out.PickPixel(tex.resourceId, False,
                                              int(texdetails.width / 2), int(texdetails.height / 2), 0, 0, 0)

        # We just output green from the shader when the value is as expected
        if not rdtest.value_compare(picked.floatValue, [0.0, 1.0, 0.0, 0.0]):
            raise rdtest.TestFailureException("Picked value {} doesn't match expectation".format(picked.floatValue))

        rdtest.log.success("Picked value is as expected")

        out.Shutdown()
