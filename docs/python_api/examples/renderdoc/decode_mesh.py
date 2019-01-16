import sys

# Import renderdoc if not already imported (e.g. in the UI)
if 'renderdoc' not in sys.modules and '_renderdoc' not in sys.modules:
	import renderdoc

# Alias renderdoc for legibility
rd = renderdoc

# We'll need the struct data to read out of bytes objects
import struct

# We base our data on a MeshFormat, but we add some properties
class MeshData(rd.MeshFormat):
	indexOffset = 0
	name = ''

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

# Unpack a tuple of the given format, from the data
def unpackData(fmt, data):
	# We don't handle 'special' formats - typically bit-packed such as 10:10:10:2
	if fmt.Special():
		raise RuntimeError("Packed formats are not supported!")

	formatChars = {}
	#                                 012345678
	formatChars[rd.CompType.UInt]  = "xBHxIxxxL"
	formatChars[rd.CompType.SInt]  = "xbhxixxxl"
	formatChars[rd.CompType.Float] = "xxexfxxxd" # only 2, 4 and 8 are valid

	# These types have identical decodes, but we might post-process them
	formatChars[rd.CompType.UNorm] = formatChars[rd.CompType.UInt]
	formatChars[rd.CompType.UScaled] = formatChars[rd.CompType.UInt]
	formatChars[rd.CompType.SNorm] = formatChars[rd.CompType.SInt]
	formatChars[rd.CompType.SScaled] = formatChars[rd.CompType.SInt]
	formatChars[rd.CompType.Double] = formatChars[rd.CompType.Float]

	# We need to fetch compCount components
	vertexFormat = str(fmt.compCount) + formatChars[fmt.compType][fmt.compByteWidth]

	# Unpack the data
	value = struct.unpack_from(vertexFormat, data, 0)

	# If the format needs post-processing such as normalisation, do that now
	if fmt.compType == rd.CompType.UNorm:
		divisor = float((1 << fmt.compByteWidth) - 1)
		value = tuple(float(value[i]) / divisor for i in value)
	elif fmt.compType == rd.CompType.SNorm:
		maxNeg = -(1 << (fmt.compByteWidth - 1))
		divisor = float(-(maxNeg-1))
		value = tuple((float(value[i]) if (value[i] == maxNeg) else (float(value[i]) / divisor)) for i in value)

	# If the format is BGRA, swap the two components
	if fmt.BGRAOrder():
		value = tuple(value[i] for i in [2, 1, 0, 3])

	return value

# Get a list of MeshData objects describing the vertex inputs at this draw
def getMeshInputs(controller, draw):
	state = controller.GetPipelineState()

	# Get the index & vertex buffers, and fixed vertex inputs
	ib = state.GetIBuffer()
	vbs = state.GetVBuffers()
	attrs = state.GetVertexInputs()

	meshInputs = []

	for attr in attrs:

		# We don't handle instance attributes
		if attr.perInstance:
			raise RuntimeError("Instanced properties are not supported!")
		
		meshInput = MeshData()
		meshInput.indexResourceId = ib.resourceId
		meshInput.indexByteOffset = ib.byteOffset
		meshInput.indexByteStride = draw.indexByteWidth
		meshInput.baseVertex = draw.baseVertex
		meshInput.indexOffset = draw.indexOffset
		meshInput.numIndices = draw.numIndices

		# If the draw doesn't use an index buffer, don't use it even if bound
		if not (draw.flags & rd.DrawFlags.Indexed):
			meshInput.indexResourceId = rd.ResourceId.Null()

		# The total offset is the attribute offset from the base of the vertex
		meshInput.vertexByteOffset = attr.byteOffset + vbs[attr.vertexBuffer].byteOffset
		meshInput.format = attr.format
		meshInput.vertexResourceId = vbs[attr.vertexBuffer].resourceId
		meshInput.vertexByteStride = vbs[attr.vertexBuffer].byteStride
		meshInput.name = attr.name

		meshInputs.append(meshInput)

	return meshInputs

# Get a list of MeshData objects describing the vertex outputs at this draw
def getMeshOutputs(controller, postvs):
	meshOutputs = []
	posidx = 0

	vs = controller.GetPipelineState().GetShaderReflection(rd.ShaderStage.Vertex)

	# Repeat the process, but this time sourcing the data from postvs.
	# Since these are outputs, we iterate over the list of outputs from the
	# vertex shader's reflection data
	for attr in vs.outputSignature:
		# Copy most properties from the postvs struct
		meshOutput = MeshData()
		meshOutput.indexResourceId = postvs.indexResourceId
		meshOutput.indexByteOffset = postvs.indexByteOffset
		meshOutput.indexByteStride = postvs.indexByteStride
		meshOutput.baseVertex = postvs.baseVertex
		meshOutput.indexOffset = 0
		meshOutput.numIndices = postvs.numIndices

		# The total offset is the attribute offset from the base of the vertex,
		# as calculated by the stride per index
		meshOutput.vertexByteOffset = postvs.vertexByteOffset
		meshOutput.vertexResourceId = postvs.vertexResourceId
		meshOutput.vertexByteStride = postvs.vertexByteStride

		# Construct a resource format for this element
		meshOutput.format = rd.ResourceFormat()
		meshOutput.format.compByteWidth = 8 if attr.compType == rd.CompType.Double else 4
		meshOutput.format.compCount = attr.compCount
		meshOutput.format.compType = attr.compType
		meshOutput.format.type = rd.ResourceFormatType.Regular

		meshOutput.name = attr.semanticIdxName if attr.varName == '' else attr.varName

		if attr.systemValue == rd.ShaderBuiltin.Position:
			posidx = len(meshOutputs)

		meshOutputs.append(meshOutput)
	
	# Shuffle the position element to the front
	if posidx > 0:
		pos = meshOutputs[posidx]
		del meshOutputs[posidx]
		meshOutputs.insert(0, pos)

	accumOffset = 0

	for i in range(0, len(meshOutputs)):
		meshOutputs[i].vertexByteOffset = accumOffset

		# Note that some APIs such as Vulkan will pad the size of the attribute here
		# while others will tightly pack
		fmt = meshOutputs[i].format

		accumOffset += (8 if fmt.compType == rd.CompType.Double else 4) * fmt.compCount

	return meshOutputs

def getIndices(controller, mesh):
	# Get the character for the width of index
	indexFormat = 'B'
	if mesh.indexByteStride == 2:
		indexFormat = 'H'
	elif mesh.indexByteStride == 4:
		indexFormat = 'I'

	# Duplicate the format by the number of indices
	indexFormat = str(mesh.numIndices) + indexFormat

	# If we have an index buffer
	if mesh.indexResourceId != rd.ResourceId.Null():
		# Fetch the data
		ibdata = controller.GetBufferData(mesh.indexResourceId, mesh.indexByteOffset, 0)

		# Unpack all the indices, starting from the first index to fetch
		offset = mesh.indexOffset * mesh.indexByteStride
		indices = struct.unpack_from(indexFormat, ibdata, offset)

		# Apply the baseVertex offset
		return [i + mesh.baseVertex for i in indices]
	else:
		# With no index buffer, just generate a range
		return tuple(range(vertexOffset, vertexOffset+mesh.numIndices))

def printMeshData(controller, meshData):
	indices = getIndices(controller, meshData[0])

	print("Mesh configuration:")
	for attr in meshData:
		print("\t%s:" % attr.name)
		print("\t\t- vertex: %s / %d stride" % (attr.vertexResourceId,  attr.vertexByteStride))
		print("\t\t- format: %s x %s @ %d" % (attr.format.compType, attr.format.compCount, attr.vertexByteOffset))

	# We'll decode the first three indices making up a triangle
	for i in range(0, 3):
		idx = indices[i]

		print("Vertex %d is index %d:" % (i, idx))

		for attr in meshData:
			# This is the data we're reading from. This would be good to cache instead of
			# re-fetching for every attribute for every index
			offset = attr.vertexByteOffset + attr.vertexByteStride * idx
			data = controller.GetBufferData(attr.vertexResourceId, offset, 0)

			# Get the value from the data
			value = unpackData(attr.format, data)

			# We don't go into the details of semantic matching here, just print both
			print("\tAttribute '%s': %s" % (attr.name, value))

def sampleCode(controller):
	# Find the biggest drawcall in the whole capture
	draw = None
	for d in controller.GetDrawcalls():
		draw = biggestDraw(draw, d)

	# Move to that draw
	controller.SetFrameEvent(draw.eventId, True)

	print("Decoding mesh inputs at %d: %s\n\n" % (draw.eventId, draw.name))

	# Calculate the mesh input configuration
	meshInputs = getMeshInputs(controller, draw)
	
	# Fetch and print the data from the mesh inputs
	printMeshData(controller, meshInputs)

	print("Decoding mesh outputs\n\n")

	# Fetch the postvs data
	postvs = controller.GetPostVSData(0, 0, rd.MeshDataStage.VSOut)

	# Calcualte the mesh configuration from that
	meshOutputs = getMeshOutputs(controller, postvs)
	
	# Print it
	printMeshData(controller, meshOutputs)

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

