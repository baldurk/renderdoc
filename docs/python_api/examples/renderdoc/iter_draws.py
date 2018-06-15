import renderdoc as rd

# Set up a hash for storing draw information by event
draws = {}

# Define a recursive function for iterating over draws
def iterDraw(d, indent = ''):
	global draws

	# Print this drawcall
	print('%s%d: %s' % (indent, d.eventId, d.name))

	# Save the draw by eventId for use later
	draws[d.eventId] = d

	# Iterate over the draw's children
	for d in d.children:
		iterDraw(d, indent + '    ')

def sampleCode(controller):
	global draws

	# Iterate over all of the root drawcalls
	for d in controller.GetDrawcalls():
		iterDraw(d)

	# Start iterating from the first real draw as a child of markers
	draw = controller.GetDrawcalls()[0]

	while len(draw.children) > 0:
		draw = draw.children[0]

	# Counter for which pass we're in
	passnum = 0
	# Counter for how many draws are in the pass
	passcontents = 0
	# Whether we've started seeing draws in the pass - i.e. we're past any
	# starting clear calls that may be batched together
	inpass = False

	print("Pass #0 starts with %d: %s" % (draw.eventId, draw.name))

	while draw != None:
		# When we encounter a clear
		if draw.flags & rd.DrawFlags.Clear:
			if inpass:
				print("Pass #%d contained %d draws" % (passnum, passcontents))
				passnum += 1
				print("Pass #%d starts with %d: %s" % (passnum, draw.eventId, draw.name))
				passcontents = 0
				inpass = False
		else:
			passcontents += 1
			inpass = True

		# Advance to the next drawcall
		if not draw.next in draws:
			break
		draw = draws[draw.next]

	if inpass:
		print("Pass #%d contained %d draws" % (passnum, passcontents))

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

	return controller

if 'pyrenderdoc' in globals():
	pyrenderdoc.Replay().BlockInvoke(sampleCode)
else:
	cap,controller = loadCapture('test.rdc')

	sampleCode(controller)

	controller.Shutdown()
	cap.Shutdown()

