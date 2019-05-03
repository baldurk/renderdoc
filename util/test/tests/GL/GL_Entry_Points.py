import rdtest
import renderdoc as rd


class GL_Entry_Points(rdtest.TestCase):
    def get_capture(self):
        return rdtest.run_and_capture("demos_x64", "GL_Entry_Points", 5)

    def check_capture(self):
        sdf = self.controller.GetStructuredFile()

        # The marker name, and the calls that we expect to follow it
        expected = {
            'First Test': ['glUniform1ui'],
            'Second Test': ['glUniform1uiEXT', 'glProgramUniform4f'],
            'Third Test': ['glMemoryBarrierEXT', 'glUniform4f'],
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

