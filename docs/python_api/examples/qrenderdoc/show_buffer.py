filename = "test.rdc"
formatter = "float3 pos; half norms[16]; uint flags;"

pyrenderdoc.LoadCapture(filename, filename, False, True)

mybuf = renderdoc.ResourceId.Null()

for buf in pyrenderdoc.GetBuffers():
    print("buf %s is %s" % (buf.resourceId, pyrenderdoc.GetResourceName(buf.resourceId)))

    # here put your actual selection criteria - i.e. look for a particular name
    if pyrenderdoc.GetResourceName(buf.resourceId) == "dataBuffer":
        mybuf = buf.resourceId
        break

print("selected %s" % pyrenderdoc.GetResourceName(mybuf))

# Open a new buffer viewer for this buffer, with the given format
bufview = pyrenderdoc.ViewBuffer(0, 0, mybuf, formatter)

# Show the buffer viewer on the main tool area
pyrenderdoc.AddDockWindow(bufview.Widget(), qrenderdoc.DockReference.MainToolArea, None)
