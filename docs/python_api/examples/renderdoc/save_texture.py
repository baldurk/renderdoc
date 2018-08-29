import sys

# Import renderdoc if not already imported (e.g. in the UI)
if 'renderdoc' not in sys.modules and '_renderdoc' not in sys.modules:
	import renderdoc

# Alias renderdoc for legibility
rd = renderdoc

# Recursively search for the drawcall with the most vertices
def biggestDraw(prevBiggest, d):
	ret = prevBiggest
	if ret == None or d.numIndices > ret.numIndices:
		ret = d

	for c in d.children:
		biggest = biggestDraw(ret, c)

		if biggest.numIndices > ret.numIndices:
			ret = biggest

	return ret

def sampleCode(controller):
	# Find the biggest drawcall in the whole capture
	draw = None
	for d in controller.GetDrawcalls():
		draw = biggestDraw(draw, d)

	# Move to that draw
	controller.SetFrameEvent(draw.eventId, True)

	texsave = rd.TextureSave()

	# Select the first color output
	texsave.resourceId = draw.outputs[0]

	if texsave.resourceId == rd.ResourceId.Null():
		return
	
	filename = str(int(texsave.resourceId))

	print("Saving images of %s at %d: %s" % (filename, draw.eventId, draw.name))

	# Save different types of texture

	# Blend alpha to a checkerboard pattern for formats without alpha support
	texsave.alpha = rd.AlphaMapping.BlendToCheckerboard

	# Most formats can only display a single image per file, so we select the
	# first mip and first slice
	texsave.mip = 0
	texsave.slice.sliceIndex = 0

	texsave.destType = rd.FileType.JPG
	controller.SaveTexture(texsave, filename + ".jpg")

	texsave.destType = rd.FileType.HDR
	controller.SaveTexture(texsave, filename + ".hdr")

	# For formats with an alpha channel, preserve it
	texsave.alpha = rd.AlphaMapping.Preserve

	texsave.destType = rd.FileType.PNG
	controller.SaveTexture(texsave, filename + ".png")

	# DDS textures can save multiple mips and array slices, so instead
	# of the default behaviour of saving mip 0 and slice 0, we set -1
	# which saves *all* mips and slices
	texsave.mip = -1
	texsave.slice.sliceIndex = -1

	texsave.destType = rd.FileType.DDS
	controller.SaveTexture(texsave, filename + ".dds")

def loadCapture(filename):
	# Open a capture file handle
	cap = rd.OpenCaptureFile()

	# Open a particular file - see also OpenBuffer to load from memory
	status = cap.OpenFile(filename, '', None)

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

	return (cap, controller)

if 'pyrenderdoc' in globals():
	pyrenderdoc.Replay().BlockInvoke(sampleCode)
else:
	cap,controller = loadCapture('test.rdc')

	sampleCode(controller)

	controller.Shutdown()
	cap.Shutdown()

