import rdtest
import renderdoc as rd


class GL_Mesh_Zoo(rdtest.TestCase):
    demos_test_name = 'GL_Mesh_Zoo'

    def __init__(self):
        rdtest.TestCase.__init__(self)
        self.zoo_helper = rdtest.Mesh_Zoo()

    def check_capture(self):
        self.zoo_helper.check_capture(self.capture_filename, self.controller)

        # Test GL-only thing with geometry shader only and completely no-op vertex shader
        draw = self.zoo_helper.find_draw("Geom Only").next
        self.controller.SetFrameEvent(draw.eventId, False)

        pos: rd.MeshFormat = self.controller.GetPostVSData(0, 0, rd.MeshDataStage.VSOut)

        # vertex output should be completely empty
        self.check(pos.vertexByteStride == 0)
        self.check(pos.numIndices == 0)
        self.check(self.controller.GetBufferData(pos.vertexResourceId, 0, 0) == bytes())

        gsout_ref = {
            0: {
                'gl_Position': [-0.4, -0.4, 0.5, 1.0],
                'col': [1.0, 0.0, 0.0, 1.0],
            },
            1: {
                'gl_Position': [0.6, -0.6, 0.5, 1.0],
                'col': [1.0, 0.0, 0.0, 1.0],
            },
            2: {
                'gl_Position': [-0.5, 0.5, 0.5, 1.0],
                'col': [1.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(gsout_ref, self.get_postvs(draw, rd.MeshDataStage.GSOut))

        # Test GL-only thing with geometry shader only and completely no-op vertex shader
        multibase = self.zoo_helper.find_draw("Multi Draw").next.parent
        self.controller.SetFrameEvent(multibase.children[-1].eventId, False)

        baseVertex = [10, 11]
        baseInstance = [20, 22]

        for d, draw in enumerate(multibase.children):
            draw: rd.DrawcallDescription

            self.controller.SetFrameEvent(draw.eventId, False)

            pipe: rd.PipeState = self.controller.GetPipelineState()

            shad: rd.ShaderReflection = pipe.GetShaderReflection(rd.ShaderStage.Vertex)

            builtins = [sig.systemValue for sig in shad.inputSignature if sig.systemValue != rd.ShaderBuiltin.Undefined]

            self.check(rd.ShaderBuiltin.BaseInstance in builtins)
            self.check(rd.ShaderBuiltin.BaseVertex in builtins)
            self.check(rd.ShaderBuiltin.DrawIndex in builtins)

            bv = baseVertex[d]
            bi = baseInstance[d]

            for inst in range(draw.numInstances):
                multi_ref = {
                    0: {
                        'basevtx': bv,
                        'baseinst': bi,
                        'inst': inst,
                        'draw': d,
                        'vert': bv + 0,
                    },
                    1: {
                        'basevtx': bv,
                        'baseinst': bi,
                        'inst': inst,
                        'draw': d,
                        'vert': bv + 1,
                    },
                    2: {
                        'basevtx': bv,
                        'baseinst': bi,
                        'inst': inst,
                        'draw': d,
                        'vert': bv + 2,
                    },
                }

                self.check_mesh_data(multi_ref, self.get_postvs(draw, rd.MeshDataStage.VSOut, instance=inst))

        rdtest.log.success("Multi-draw pass is as expected")
