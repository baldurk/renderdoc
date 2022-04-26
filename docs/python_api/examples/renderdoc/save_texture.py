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
	for d in controller.GetRootActions():
		draw = biggestDraw(draw, d)

	# Move to that draw
	controller.SetFrameEvent(draw.eventId, True)

	texsave = rd.TextureSave()

	# Select the first color output
	texsave.resourceId = draw.outputs[0]

	if texsave.resourceId == rd.ResourceId.Null():
		return
	
	filename = str(int(texsave.resourceId))

	print("Saving images of %s at %d: %s" % (filename, draw.eventId, draw.GetName(controller.GetStructuredFile())))

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
	result = cap.OpenFile(filename, '', None)

	# Make sure the file opened successfully
	if result != rd.ResultCode.Succeeded:
		raise RuntimeError("Couldn't open file: " + str(result))

	# Make sure we can replay
	if not cap.LocalReplaySupport():
		raise RuntimeError("Capture cannot be replayed")

	# Initialise the replay
	result,controller = cap.OpenCapture(rd.ReplayOptions(), None)

	if result != rd.ResultCode.Succeeded:
		raise RuntimeError("Couldn't initialise replay: " + str(result))

	return (cap, controller)

if 'pyrenderdoc' in globals():
	pyrenderdoc.Replay().BlockInvoke(sampleCode)
else:
	rd.InitialiseReplay(rd.GlobalEnvironment(), [])

	if len(sys.argv) <= 1:
		print('Usage: python3 {} filename.rdc'.format(sys.argv[0]))
		sys.exit(0)

	cap,controller = loadCapture(sys.argv[1])

	sampleCode(controller)

	controller.Shutdown()
	cap.Shutdown()

	rd.ShutdownReplay()

