import renderdoc as rd
import rdtest

prevProgress = -1.0


def resolve_progress(progress: float):
    global prevProgress
    if progress - prevProgress > 0.01 or progress == 1.0:
        prevProgress = progress
        rdtest.log.print(f"Resolve progress: {progress * 100.0:.2f}%")


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
        if not result:
            cap.Shutdown()
            raise rdtest.TestFailureException(f"Couldn't open capture {self.capture_filename} for access: {result!s}")

        if not cap.HasCallstacks():
            raise rdtest.TestFailureException("Capture does not report having callstacks")

        if not cap.InitResolver(False, resolve_progress):
            raise rdtest.TestFailureException("Failed to initialise callstack resolver")

        action = self.find_action("Draw")

        event = action.events[-1]

        expected_funcs = [
            "GL_Callstacks::testFunction",
            "GL_Callstacks::main",
        ]

        expected_lines = [
            7000,
            8000
        ]

        sdfile = self.controller.GetStructuredFile()

        if event.chunkIndex < 0 or event.chunkIndex > len(sdfile.chunks):
            raise rdtest.TestFailureException(f"Event {event.eventId} has invalid chunk index {event.chunkIndex}")

        chunk = sdfile.chunks[event.chunkIndex]

        callstack = cap.GetResolve(list(chunk.metadata.callstack))

        if len(callstack) < len(expected_funcs):
            raise rdtest.TestFailureException(f"Resolved callstack isn't long enough ({len(callstack)} stack frames), expected at least {len(expected_funcs)}")

        for i in range(len(expected_funcs)):
            stack = callstack[i]
            if expected_funcs[i] not in stack:
                raise rdtest.TestFailureException(f"Expected '{expected_funcs[i]}' in '{stack}'")
            idx = callstack[i].find("line")
            if idx < 0:
                raise rdtest.TestFailureException(f"Expected a line number in '{stack}'")

            # allow line numbers reported to be off by 1 or 2, to allow for compiler differences.
            line_diff = int(stack[idx+5:]) - expected_lines[i]
            if line_diff < 0 or line_diff > 2:
                raise rdtest.TestFailureException(f"Expected line number around {expected_lines[i]} in '{stack}'")

        rdtest.log.success("Callstacks are as expected")
