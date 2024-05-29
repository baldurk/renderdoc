import sys

# Import renderdoc if not already imported (e.g. in the UI)
if 'renderdoc' not in sys.modules and '_renderdoc' not in sys.modules:
	import renderdoc

# Alias renderdoc for legibility
rd = renderdoc

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

	targets = controller.GetDisassemblyTargets(True)

	for disasm in targets:
		print("  - " + disasm)

	target = targets[0]

	state = controller.GetPipelineState()

	# For some APIs, it might be relevant to set the PSO id or entry point name
	pipe = state.GetGraphicsPipelineObject()
	entry = state.GetShaderEntryPoint(rd.ShaderStage.Pixel)

	# Get the pixel shader's reflection object
	ps = state.GetShaderReflection(rd.ShaderStage.Pixel)

	cb = state.GetConstantBlock(rd.ShaderStage.Pixel, 0, 0)

	print("Pixel shader:")
	print(controller.DisassembleShader(pipe, ps, target))

	cbufferVars = controller.GetCBufferVariableContents(pipe, ps.resourceId, rd.ShaderStage.Pixel, entry, 0, cb.descriptor.resource, 0, 0)

	for v in cbufferVars:
		printVar(v)

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

