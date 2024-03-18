import rdtest
import renderdoc as rd


class VK_Int8_IBuffer(rdtest.TestCase):
    demos_test_name = 'VK_Int8_IBuffer'

    def check_capture(self):
        action = self.find_action("Draw")

        self.check(action is not None)

        self.controller.SetFrameEvent(action.eventId, False)

        pipe: rd.PipeState = self.controller.GetPipelineState()

        postvs_data = self.get_postvs(action, rd.MeshDataStage.VSOut, 0, action.numIndices)

        ib = pipe.GetIBuffer()

        # Calculate the strip restart index for this index width
        striprestart_index = pipe.GetRestartIndex() & ((1 << (ib.byteStride*8)) - 1)

        # We don't check all of the output, we check a few key vertices to ensure they match up
        postvs_ref = {
            0: {
                'vtx': 0,
                'idx': 0,
                'gl_Position': [-0.8, -0.2, 0.0, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
            4: {
                'vtx': 4,
                'idx': 4,
                'gl_Position': [0.0, -0.2, 0.0, 1.0],
                'vertOut.col': [0.0, 1.0, 0.0, 1.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
            8: {
                'idx': striprestart_index
            },
            9: {
                'vtx': 9,
                'idx': 8,
                'gl_Position': [-0.8, 0.7, 0.0, 1.0],
                'vertOut.col': [0.0, 0.0, 1.0, 1.0],
                'vertOut.uv': [0.0, 0.0, 0.0, 1.0],
            },
        }

        self.check_mesh_data(postvs_ref, postvs_data)

        # Check that the rendered mesh is as expected
        out = pipe.GetOutputTargets()[0].resource
        for x in [x*0.01 for x in range(1, 100)]:
            self.check_pixel_value(out, x, 0.1, [0.2, 0.2, 0.2, 1.0])
            self.check_pixel_value(out, x, 0.5, [0.2, 0.2, 0.2, 1.0])

        self.check_pixel_value(out, 0.3, 0.25, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(out, 0.5, 0.25, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(out, 0.7, 0.25, [0.0, 1.0, 0.0, 1.0])
        self.check_pixel_value(out, 0.3, 0.75, [0.0, 0.0, 1.0, 1.0])
        self.check_pixel_value(out, 0.5, 0.75, [0.0, 0.0, 1.0, 1.0])
        self.check_pixel_value(out, 0.7, 0.75, [0.0, 0.0, 1.0, 1.0])
