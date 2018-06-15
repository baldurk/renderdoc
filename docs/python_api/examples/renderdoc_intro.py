import renderdoc as rd

# Open a capture file handle
cap = rd.OpenCaptureFile()

# Open a particular file - see also OpenBuffer to load from memory
status = cap.OpenFile('test.rdc', '', None)

# Make sure the file opened successfully
if status != rd.ReplayStatus.Succeeded:
    raise RuntimeError("Couldn't open file: " + str(status))

# Make sure we can replay
if not cap.LocalReplaySupport():
    raise RuntimeError("Capture cannot be replayed")

# Initialise the replay
status,controller = cap.OpenCapture(None)

if status != rd.ReplayStatus.Succeeded:
    raise RuntimeError("Couldn't initialise replay: " + str(status))

# Now we can use the controller!
print("%d top-level drawcalls" % len(controller.GetDrawcalls()))

controller.Shutdown()

cap.Shutdown()
