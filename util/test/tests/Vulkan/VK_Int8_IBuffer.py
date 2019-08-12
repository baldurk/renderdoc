import rdtest
import renderdoc as rd


class VK_Int8_IBuffer(rdtest.TestCase):
    demos_test_name = 'VK_Int8_IBuffer'

    def check_capture(self):
        self.check_final_backbuffer()

        draw = self.find_draw("Draw")

        self.check(draw is not None)

        self.controller.SetFrameEvent(draw.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        postvs_data = self.get_postvs(rd.MeshDataStage.VSOut, 0, draw.numIndices)

        # Calculate the strip restart index for this index width
        striprestart_index = pipe.GetStripRestartIndex() & ((1 << (draw.indexByteWidth*8)) - 1)

        # We don't check all of the output, we check a few key vertices to ensure they match up
        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_PerVertex.gl_Position': [-0.8, -0.2, 0.0, 1.0],
                'vertOut.col': [1.0, 0.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
            4: {
                'vtx': 4,
                'idx': 4,
                'gl_PerVertex.gl_Position': [0.0, -0.2, 0.0, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
            8: {
                'idx': striprestart_index
            },
            9: {
                'vtx': 9,
                'idx': 8,
                'gl_PerVertex.gl_Position': [-0.8, 0.7, 0.0, 1.0],
                'vertOut.col': [1.0, 0.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)
