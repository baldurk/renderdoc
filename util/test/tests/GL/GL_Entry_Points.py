from typing import List

import rdtest


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
            marker = self.find_action(test)
            if marker is None:
                raise rdtest.TestFailureException(f'Failed to find action {test}')
            action = marker.nextAction

            calls: List[str] = []

            for ev in action.events:
                # skip any events up to and including the marker itself
                if ev.eventId <= marker.eventId:
                    continue

                calls.append(sdf.chunks[ev.chunkIndex].name)

            for i in range(len(expected[test])):
                if expected[test][i] != calls[i]:
                    raise rdtest.TestFailureException(f'After marker {test} got call {calls[i]} but expected {expected[test][i]}')

        rdtest.log.success("API calls are as expected")

