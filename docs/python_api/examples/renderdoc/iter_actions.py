import sys

# Import renderdoc if not already imported (e.g. in the UI)
if 'renderdoc' not in sys.modules and '_renderdoc' not in sys.modules:
	import renderdoc

# Alias renderdoc for legibility
rd = renderdoc

# Define a recursive function for iterating over actions
def iterAction(d, indent = ''):
	# Print this action
	print('%s%d: %s' % (indent, d.eventId, d.GetName(controller.GetStructuredFile())))

	# Iterate over the action's children
	for d in d.children:
		iterAction(d, indent + '    ')

def sampleCode(controller):
	# Iterate over all of the root actions
	for d in controller.GetRootActions():
		iterAction(d)

	# Start iterating from the first real action as a child of markers
	action = controller.GetRootActions()[0]

	while len(action.children) > 0:
		action = action.children[0]

	# Counter for which pass we're in
	passnum = 0
	# Counter for how many actions are in the pass
	passcontents = 0
	# Whether we've started seeing actions in the pass - i.e. we're past any
	# starting clear calls that may be batched together
	inpass = False

	print("Pass #0 starts with %d: %s" % (action.eventId, action.GetName(controller.GetStructuredFile())))

	while action != None:
		# When we encounter a clear
		if action.flags & rd.ActionFlags.Clear:
			if inpass:
				print("Pass #%d contained %d actions" % (passnum, passcontents))
				passnum += 1
				print("Pass #%d starts with %d: %s" % (passnum, action.eventId, action.GetName(controller.GetStructuredFile())))
				passcontents = 0
				inpass = False
		else:
			passcontents += 1
			inpass = True

		# Advance to the next action
		action = action.next
		if action is None:
			break

	if inpass:
		print("Pass #%d contained %d actions" % (passnum, passcontents))

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

	return cap,controller

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

