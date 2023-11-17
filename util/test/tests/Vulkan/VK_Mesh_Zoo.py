import rdtest
import struct
import renderdoc as rd


class VK_Mesh_Zoo(rdtest.TestCase):
    demos_test_name = 'VK_Mesh_Zoo'

    def __init__(self):
        rdtest.TestCase.__init__(self)
        self.zoo_helper = rdtest.Mesh_Zoo()

    def check_capture(self):
        self.zoo_helper.check_capture(self.capture_filename, self.controller)

        xfbDraw = self.find_action("XFB")

        if xfbDraw is not None:
            self.controller.SetFrameEvent(xfbDraw.next.eventId, False)

            postgs_data = self.get_postvs(xfbDraw.next, rd.MeshDataStage.GSOut, 0, 4)

            postgs_ref = {
                0: {
                    'vtx': 0,
                    'idx': 0,
                    'gl_Position': [0.8, 0.8, 0.0, 1.0],
                    'uv1': [0.5, 0.5],
                },
                1: {
                    'vtx': 1,
                    'idx': 1,
                    'gl_Position': [0.8, 0.9, 0.0, 1.0],
                    'uv1': [0.6, 0.6],
                },
                2: {
                    'vtx': 2,
                    'idx': 2,
                    'gl_Position': [0.9, 0.8, 0.0, 1.0],
                    'uv1': [0.7, 0.7],
                },
                3: {
                    'vtx': 3,
                    'idx': 3,
                    'gl_Position': [0.9, 0.9, 0.0, 1.0],
                    'uv1': [0.8, 0.8],
                },
            }

            self.check_mesh_data(postgs_ref, postgs_data)

            self.check(self.controller.GetPipelineState().GetRasterizedStream() == 2)

            xfbDraw = self.find_action("XFB After")

            self.controller.SetFrameEvent(xfbDraw.eventId, False)

            xfb = self.controller.GetVulkanPipelineState().transformFeedback

            bufs = []
            for i, fmt in enumerate(['8f', '4f', '24f']):
                xfbBuf = xfb.buffers[i]
                bufs.append(struct.unpack_from(fmt,
                                               self.controller.GetBufferData(xfbBuf.bufferResourceId, xfbBuf.byteOffset,
                                                                             0), 0))

            if bufs[0] != (1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0):
                raise rdtest.TestFailureException(
                    "XFB buffer 0 is not as expected: {}".format(bufs[0]))

            if bufs[1] != (9.0, 10.0, 11.0, 12.0):
                raise rdtest.TestFailureException(
                    "XFB buffer 1 is not as expected: {}".format(bufs[0]))

            vert_ref = [
                (0.8, 0.8),
                (0.8, 0.9),
                (0.9, 0.8),
                (0.9, 0.9),
            ]

            for i in range(4):
                vert = bufs[2][(i * 6):(i * 6 + 6)]

                ref = (vert_ref[i][0], vert_ref[i][1], 0.0, 1.0, 0.5 + 0.1 * i, 0.5 + 0.1 * i)

                if not rdtest.value_compare(vert, ref):
                    raise rdtest.TestFailureException(
                        "XFB buffer 2 vertex {} is not as expected: {}".format(i, vert))
