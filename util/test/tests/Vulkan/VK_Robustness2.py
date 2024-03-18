import renderdoc as rd
import rdtest


class VK_Robustness2(rdtest.TestCase):
    demos_test_name = 'VK_Robustness2'

    def check_capture(self):
        action: rd.ActionDescription = self.find_action('vkCmdDraw')

        self.controller.SetFrameEvent(action.eventId, True)

        self.check_triangle()

        rdtest.log.success('Triangle is rendered correctly')

        vsin_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'Position': [-0.5, -0.5, 0.0],
                'Color': None,
                'UV': None,
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'Position': [0.0, 0.5, 0.0],
                'Color': None,
                'UV': None,
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'Position': [0.5, -0.5, 0.0],
                'Color': None,
                'UV': None,
            },
        }

        self.check_mesh_data(vsin_ref, self.get_vsin(action))

        rdtest.log.success('Mesh input data is correct, including unbound VB')

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [-0.5, 0.5, 0.0, 1.0],
                'vertOut.pos': [-0.5, 0.5, 0.0, 1.0],
                'vertOut.col': [0.0, 0.0, 0.0, 0.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
            1: {
                'vtx': 1,
                'idx': 1,
                'gl_Position': [0.0, -0.5, 0.0, 1.0],
                'vertOut.pos': [0.0, -0.5, 0.0, 1.0],
                'vertOut.col': [0.0, 0.0, 0.0, 0.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
            2: {
                'vtx': 2,
                'idx': 2,
                'gl_Position': [0.5, 0.5, 0.0, 1.0],
                'vertOut.pos': [0.5, 0.5, 0.0, 1.0],
                'vertOut.col': [0.0, 0.0, 0.0, 0.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        rdtest.log.success('Mesh output data is correct, including unbound VB')

        pipe = self.controller.GetPipelineState()
        refl = pipe.GetShaderReflection(rd.ShaderStage.Fragment)

        for i, cb in enumerate(refl.constantBlocks):
            cbuf = pipe.GetConstantBlock(rd.ShaderStage.Fragment, i, 0).descriptor

            var_check = rdtest.ConstantBufferChecker(
                self.controller.GetCBufferVariableContents(pipe.GetGraphicsPipelineObject(),
                                                           pipe.GetShader(rd.ShaderStage.Fragment), rd.ShaderStage.Fragment, refl.entryPoint, i,
                                                           cbuf.resource, cbuf.byteOffset, cbuf.byteSize))

            if cb.bufferBacked:
                var_check.check('data').type(rd.VarType.Float).rows(1).cols(4).value([0.0, 0.0, 0.0, 0.0])
            else:
                val = [0, 0, 0, 0]
                if self.find_action('robustBufferAccess2') is not None:
                    val[2] = 1000000
                if self.find_action('robustImageAccess2') is not None:
                    val[0] = val[1] = 1000000
                var_check.check('coord').type(rd.VarType.SInt).rows(1).cols(4).value(val)

            rdtest.log.success('CBuffer {} at bindpoint {}.{}[0] contains the correct contents'
                               .format(cb.name, cb.fixedBindSetOrSpace, cb.fixedBindNumber))
