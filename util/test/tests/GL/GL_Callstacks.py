import renderdoc as rd
import rdtest

prevProgress = -1.0


def resolve_progress(progress: float):
    global prevProgress
    if progress - prevProgress > 0.01 or progress == 1.0:
        prevProgress = progress
        rdtest.log.print("Resolve progress: {:.2f}%".format(progress*100.0))


class GL_Callstacks(rdtest.TestCase):
    demos_test_name = 'GL_Callstacks'

    def get_capture_options(self):
        ret = rd.CaptureOptions()
        ret.captureCallstacks = True
        return ret

    def check_capture(self):
        # Need capture access. Rather than trying to keep the original around, we just open a new one
        cap = rd.OpenCaptureFile()

        # Open a particular file
        result = cap.OpenFile(self.capture_filename, '', None)

        # Make sure the file opened successfully
        if result != rd.ResultCode.Succeeded:
            cap.Shutdown()
            raise rdtest.TestFailureException("Couldn't open capture for access: {}".format(self.capture_filename, str(result)))

        if not cap.HasCallstacks():
            raise rdtest.TestFailureException("Capture does not report having callstacks")

        if not cap.InitResolver(False, resolve_progress):
            raise rdtest.TestFailureException("Failed to initialise callstack resolver")

        action = self.find_action("Draw")

        event: rd.APIEvent = action.events[-1]

        expected_funcs = [
            "GL_Callstacks::testFunction",
            "GL_Callstacks::main",
        ]

        expected_lines = [
            7001,
            8002
        ]

        sdfile = self.controller.GetStructuredFile()

        if event.chunkIndex < 0 or event.chunkIndex > len(sdfile.chunks):
            raise rdtest.TestFailureException("Event {} has invalid chunk index {}"
                                              .format(event.eventId, event.chunkIndex))

        chunk = sdfile.chunks[event.chunkIndex]

        callstack = cap.GetResolve(list(chunk.metadata.callstack))

        if len(callstack) < len(expected_funcs):
            raise rdtest.TestFailureException("Resolved callstack isn't long enough ({} stack frames), expected at least {}".format(len(event.callstack), len(expected_funcs)))

        for i in range(len(expected_funcs)):
            stack: str = callstack[i]
            if expected_funcs[i] not in stack:
                raise rdtest.TestFailureException("Expected '{}' in '{}'".format(expected_funcs[i], stack))
            idx = callstack[i].find("line")
            if idx < 0:
                raise rdtest.TestFailureException("Expected a line number in '{}'".format(stack))

            if int(stack[idx+5:]) != expected_lines[i]:
                raise rdtest.TestFailureException("Expected line number {} in '{}'".format(expected_lines[i], stack))

        rdtest.log.success("Callstacks are as expected")
