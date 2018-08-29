import sys

# Import renderdoc if not already imported (e.g. in the UI)
if 'renderdoc' not in sys.modules and '_renderdoc' not in sys.modules:
	import renderdoc

# Alias renderdoc for legibility
rd = renderdoc

draws = {}

# Define a recursive function for iterating over draws
def iterDraw(d, indent = ''):
	global draws

	# save the drawcall by eventId
	draws[d.eventId] = d

	# Iterate over the draw's children
	for d in d.children:
		iterDraw(d, indent + '    ')

def sampleCode(controller):
	# Iterate over all of the root drawcalls, so we have names for each
	# eventId
	for d in controller.GetDrawcalls():
		iterDraw(d)

	# Enumerate the available counters
	counters = controller.EnumerateCounters()

	if not (rd.GPUCounter.SamplesPassed in counters):
		raise RuntimeError("Implementation doesn't support Samples Passed counter")

	# Now we fetch the counter data, this is a good time to batch requests of as many
	# counters as possible, the implementation handles any book keeping.
	results = controller.FetchCounters([rd.GPUCounter.SamplesPassed])

	# Get the description for the counter we want
	samplesPassedDesc = controller.DescribeCounter(rd.GPUCounter.SamplesPassed)

	# Describe each counter
	for c in counters:
		desc = controller.DescribeCounter(c)

		print("Counter %d (%s):" % (c, desc.name))
		print("    %s" % desc.description)
		print("    Returns %d byte %s, representing %s" % (desc.resultByteWidth, desc.resultType, desc.unit))

	# Look in the results for any draws with 0 samples written - this is an indication
	# that if a lot of draws appear then culling could be better.
	for r in results:
		draw = draws[r.eventId]

		# Only care about draws, not about clears and other misc events
		if not (draw.flags & rd.DrawFlags.Drawcall):
			continue

		if samplesPassedDesc.resultByteWidth == 4:
			val = r.value.u32
		else:
			val = r.value.u64

		if val == 0:
			print("EID %d '%s' had no samples pass depth/stencil test!" % (r.eventId, draw.name))

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

	return cap,controller

if 'pyrenderdoc' in globals():
	pyrenderdoc.Replay().BlockInvoke(sampleCode)
else:
	cap,controller = loadCapture('test.rdc')

	sampleCode(controller)

	controller.Shutdown()
	cap.Shutdown()

