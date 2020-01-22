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
        self.controller.SetFrameEvent(self.zoo_helper.find_draw("Geom Only").next.eventId, False)

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

        self.check_mesh_data(gsout_ref, self.get_postvs(rd.MeshDataStage.GSOut))

        rdtest.log.success("Geometry-only pass is as expected")
