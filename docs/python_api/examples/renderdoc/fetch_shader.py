import renderdoc as rd

def printVar(v, indent = ''):
	print(indent + v.name + ":")

	if len(v.members) == 0:
		valstr = ""
		for r in range(0, v.rows):
			valstr += indent + '  '

			for c in range(0, v.columns):
				valstr += '%.3f ' % v.value.fv[r*v.columns + c]

			if r < v.rows-1:
				valstr += "\n"

		print(valstr)

	for v in v.members:
		printVar(v, indent + '    ')

def sampleCode(controller):
	print("Available disassembly formats:")

	targets = controller.GetDisassemblyTargets()

	for disasm in targets:
		print("  - " + disasm)

	target = targets[0]

	# For some APIs, it might be relevant to set the PSO id or entry point name
	pipe = rd.ResourceId.Null()
	entry = "main"

	# Get the pixel shader's reflection object
	ps = controller.GetD3D11PipelineState().pixelShader

	print("Pixel shader:")
	print(controller.DisassembleShader(pipe, ps.reflection, target))

	cbufferVars = controller.GetCBufferVariableContents(ps.resourceId, entry, 0, ps.constantBuffers[0].resourceId, 0)

	for v in cbufferVars:
		printVar(v)

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

