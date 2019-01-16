Decoding Mesh Data
==================

In this example we will fetch the geometry inputs to and outputs from a vertex shader. While this sample does not handle all possible edge cases, it is more complex than most others.

First we gather the API state that describes the vertex input data. In this example we will use the API abstraction :py:class:`~renderdoc.PipeState` so that this code works on a capture from any API:

.. highlight:: python
.. code:: python

	state = controller.GetPipelineState()

	# Get the index & vertex buffers, and fixed vertex inputs
	ib = state.GetIBuffer()
	vbs = state.GetVBuffers()
	attrs = state.GetVertexInputs()

We iterate over every attribute defined, and create an object that describes where to source it from, based on :py:class:`~renderdoc.MeshFormat` - since that is the format returned by :py:meth:`~renderdoc.ReplayController.GetPostVSData` this allows us to re-use code.

In the object we pass both the indices (which does not vary per attribute in our case) as well as the data for the vertex buffer the attribute comes from.

.. highlight:: python
.. code:: python

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

Next we fetch the index data using :py:meth:`~renderdoc.ReplayController.GetBufferData`, applying any offsets that might be present, and decode it using python's ``struct`` module. If we're not using index buffers, then we just generate a range of indices from the first vertex up to the number of indices.

.. highlight:: python
.. code:: python

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

To begin with, we define a helper that will read a given variable out of a ``bytes`` object, using a :py:class:`~renderdoc.ResourceFormat` do define the size and format of the data.

We only handle simple regular formatted types, rather than bit-packed types, to simplify the code. As a shortcut, we use a hash of strings, where the hash key is the component type, and then the character index in the string is the byte width. This gives us the ``struct.unpack`` character to decode one component of the variable, then we prepend the number of components to fetch.

For normalised formats - :py:attr:`~renderdoc.CompType.UNorm` and :py:attr:`~renderdoc.CompType.SNorm` - we also divide the resulting integer value to get the final floating point value used.

.. highlight:: python
.. code:: python

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

Finally with that helper defined we can iterate over each attribute for the first three indices:

.. highlight:: python
.. code:: python

	indices = getIndices(controller, meshData[0])

	# We'll decode the first three indices making up a triangle
	for i in range(0, 3):
		idx = indices[i]

		print("Vertex %d is index %d:" % (i, idx))

        for attr in meshData:

Using the index, we can fetch the right vertex data for each vertex's attribute using :py:meth:`~renderdoc.ReplayController.GetBufferData` again. This simplified approach is very wasteful since we re-fetch the same vertex data for each vertex buffer over and over. A more realistic sample would cache the vertex data:

.. highlight:: python
.. code:: python

    # This is the data we're reading from. This would be good to cache instead of
    # re-fetching for every attribute for every index
    offset = attr.vertexByteOffset + attr.vertexByteStride * idx
    data = controller.GetBufferData(attr.vertexResourceId, offset, 0)

    # Get the value from the data
    value = unpackData(attr.format, data)

    # We don't go into the details of semantic matching here, just print both
    print("\tAttribute '%s': %s" % (attr.name, str(value)))

For the vertex outputs, we do something very similar but instead of fetching the attributes from state bindings, we look at the shader reflection data of the vertex. Similarly instead of fetching the vertex byte data from bound vertex buffers, we call :py:meth:`~renderdoc.ReplayController.GetPostVSData` to fetch it from the analysis.

In the case of vertex outputs there is no explicit offset available, so we calculate our own offsets. Note that for some APIs like Vulkan the outputs are not necessarily tightly packed, so padding calculations may be necessary.

The position output is also treated specially - it always appears first, regardless of the actual order of the outputs. We solve this by noting which output is the builtin position output, and shuffling it to the start of the array.

.. highlight:: python
.. code:: python

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

Example Source
--------------

.. only:: html and not htmlhelp

    :download:`Download the example script <decode_mesh.py>`.

.. literalinclude:: decode_mesh.py

Sample output:

.. sourcecode:: text

    Decoding mesh inputs at 69: DrawIndexed(5580)


    Mesh configuration:
        POSITION0:
            - vertex: <ResourceId 142> / 44 stride
            - format: CompType.Float x 3 @ 0
        TANGENT0:
            - vertex: <ResourceId 142> / 44 stride
            - format: CompType.Float x 3 @ 12
        NORMAL0:
            - vertex: <ResourceId 142> / 44 stride
            - format: CompType.Float x 3 @ 24
        TEXCOORD0:
            - vertex: <ResourceId 142> / 44 stride
            - format: CompType.Float x 2 @ 36
    Vertex 0 is index 0:
        Attribute 'POSITION0': (1.0, -1.5, 0.0)
        Attribute 'TANGENT0': (-0.0, 0.0, 1.0)
        Attribute 'NORMAL0': (0.9701425433158875, 0.24253533780574799, 0.0)
        Attribute 'TEXCOORD0': (0.0, 1.0)
    Vertex 1 is index 31:
        Attribute 'POSITION0': (0.9750000238418579, -1.399999976158142, 0.0)
        Attribute 'TANGENT0': (-0.0, 0.0, 1.0)
        Attribute 'NORMAL0': (0.9701424241065979, 0.24253588914871216, 0.0)
        Attribute 'TEXCOORD0': (0.0, 0.9666666388511658)
    Vertex 2 is index 32:
        Attribute 'POSITION0': (0.9536939859390259, -1.399999976158142, 0.20271390676498413)
        Attribute 'TANGENT0': (-0.20791170001029968, 0.0, 0.9781476259231567)
        Attribute 'NORMAL0': (0.9489423036575317, 0.24253617227077484, 0.20170393586158752)
        Attribute 'TEXCOORD0': (0.03333333507180214, 0.9666666388511658)
    Decoding mesh outputs


    Mesh configuration:
        SV_POSITION:
            - vertex: <ResourceId 1000000000000000237> / 68 stride
            - format: CompType.Float x 4 @ 0
        POSITION:
            - vertex: <ResourceId 1000000000000000237> / 68 stride
            - format: CompType.Float x 4 @ 16
        TEXCOORD:
            - vertex: <ResourceId 1000000000000000237> / 68 stride
            - format: CompType.Float x 2 @ 32
        TANGENT:
            - vertex: <ResourceId 1000000000000000237> / 68 stride
            - format: CompType.Float x 3 @ 40
        NORMAL:
            - vertex: <ResourceId 1000000000000000237> / 68 stride
            - format: CompType.Float x 4 @ 52
    Vertex 0 is index 0:
        Attribute 'SV_POSITION': (-6.269223690032959, -4.345583915710449, -2.4250497817993164, -2.0)
        Attribute 'POSITION': (-4.0, 0.0, -12.0, 1.0)
        Attribute 'TEXCOORD': (0.0, 1.0)
        Attribute 'TANGENT': (-5.0, 1.5, -11.0)
        Attribute 'NORMAL': (0.9701425433158875, 0.24253533780574799, 0.0, 0.0)
    Vertex 1 is index 31:
        Attribute 'SV_POSITION': (-6.308406352996826, -4.104162216186523, -2.4250497817993164, -2.0)
        Attribute 'POSITION': (-4.025000095367432, 0.10000002384185791, -12.0, 1.0)
        Attribute 'TEXCOORD': (0.0, 0.9666666388511658)
        Attribute 'TANGENT': (-5.0, 1.5, -11.0)
        Attribute 'NORMAL': (0.9701424241065979, 0.24253588914871216, 0.0, 0.0)
    Vertex 2 is index 32:
        Attribute 'SV_POSITION': (-6.341799736022949, -4.104162216186523, -2.221970558166504, -1.797286033630371)
        Attribute 'POSITION': (-4.046306133270264, 0.10000002384185791, -11.797286033630371, 1.0)
        Attribute 'TEXCOORD': (0.03333333507180214, 0.9666666388511658)
        Attribute 'TANGENT': (-5.207911491394043, 1.5, -11.021852493286133)
        Attribute 'NORMAL': (0.9489423036575317, 0.24253617227077484, 0.20170393586158752, 0.0)
