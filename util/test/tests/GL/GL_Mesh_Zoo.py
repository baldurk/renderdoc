import rdtest
import renderdoc as rd


class GL_Mesh_Zoo(rdtest.TestCase):
    demos_test_name = 'GL_Mesh_Zoo'

    def __init__(self):
        rdtest.TestCase.__init__(self)
        self.zoo_helper = rdtest.Mesh_Zoo()

    def check_capture(self):
        assert self.controller is not None
        self.zoo_helper.check_capture(self.capture_filename, self)

        # Test GL-only thing with geometry shader only and completely no-op vertex shader
        action = self.zoo_helper.find_action("Geom Only").nextAction
        assert action is not None
        self.set_event(action.eventId, False)

        pos = self.controller.GetPostVSData(0, 0, rd.MeshDataStage.VSOut)

        # vertex output should be completely empty
        assert pos.vertexByteStride == 0
        assert pos.numIndices == 0
        assert self.controller.GetBufferData(pos.vertexResourceId, 0, 0) == bytes()

        gsout_ref: rdtest.MeshReference = {
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

        self.check_mesh_data(gsout_ref, self.get_postvs(action, rd.MeshDataStage.GSOut))

        # Test GL-only thing with geometry shader only and completely no-op vertex shader
        multibase = self.zoo_helper.find_action("Multi Draw").nextAction.parent
        self.set_event(multibase.children[-1].eventId, False)

        baseVertex = [10, 11]
        baseInstance = [20, 22]

        for d, action in enumerate(multibase.children):
            self.set_event(action.eventId, False)

            pipe = self.controller.GetPipelineState()

            shad = pipe.GetShaderReflection(rd.ShaderStage.Vertex)

            builtins = [sig.systemValue for sig in shad.inputSignature if sig.systemValue != rd.ShaderBuiltin.Undefined]

            assert rd.ShaderBuiltin.BaseInstance in builtins
            assert rd.ShaderBuiltin.BaseVertex in builtins
            assert rd.ShaderBuiltin.DrawIndex in builtins

            bv = baseVertex[d]
            bi = baseInstance[d]

            for inst in range(action.numInstances):
                multi_ref: rdtest.MeshReference = {
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

                self.check_mesh_data(multi_ref, self.get_postvs(action, rd.MeshDataStage.VSOut, instance=inst))

        rdtest.log.success("Multi-action pass is as expected")
