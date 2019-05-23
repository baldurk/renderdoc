import rdtest
import renderdoc as rd


class GL_Entry_Points(rdtest.TestCase):
    demos_test_name = 'GL_Entry_Points'

    def check_capture(self):
        sdf = self.controller.GetStructuredFile()

        # The marker name, and the calls that we expect to follow it
        expected = {
            'First Test': ['glUniform1ui'],
            'Second Test': ['glVertexAttribBinding', 'glProgramUniform4f'],
            'Third Test': ['glVertexArrayAttribBinding', 'glUniform4f'],
        }

        for test in expected.keys():
            marker: rd.DrawcallDescription = self.find_draw(test)
            draw: rd.DrawcallDescription = marker.next

            calls = []

            ev: rd.APIEvent
            for ev in draw.events:
                # skip any events up to and including the marker itself
                if ev.eventId <= marker.eventId:
                    continue

                calls.append(sdf.chunks[ev.chunkIndex].name)

            for i in range(len(expected[test])):
                if expected[test][i] != calls[i]:
                    raise rdtest.TestFailureException('After marker {} got call {} but expected {}'
                                                      .format(test, calls[i], expected[test][i]))

        rdtest.log.success("API calls are as expected")

