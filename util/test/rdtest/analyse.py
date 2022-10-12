import struct
from typing import List
import renderdoc

# Alias for convenience - we need to import as-is so types don't get confused
rd = renderdoc


def open_capture(filename="", cap: rd.CaptureFile=None, opts: rd.ReplayOptions=None):
    """
    Opens a capture file and begins a replay.

    :param filename: The filename to open, or empty if cap is used.
    :param cap: The capture file to use, or ``None`` if a filename is given.
    :param opts: The replay options to use, or ``None`` to use the default options.
    :return: A replay controller for the capture
    :rtype: renderdoc.ReplayController
    """

    if opts is None:
        opts = rd.ReplayOptions()

    # Open a capture file handle
    own_cap = False
    api = "Unknown"

    if cap is None:
        own_cap = True

        cap = rd.OpenCaptureFile()

        # Open a particular file
        result = cap.OpenFile(filename, '', None)

        # Make sure the file opened successfully
        if result != rd.ResultCode.Succeeded:
            cap.Shutdown()
            raise RuntimeError("Couldn't open '{}': {}".format(filename, str(result)))

        api = cap.DriverName()

        # Make sure we can replay
        if not cap.LocalReplaySupport():
            cap.Shutdown()
            raise RuntimeError("{} capture cannot be replayed".format(api))

    result, controller = cap.OpenCapture(opts, None)

    if own_cap:
        cap.Shutdown()

    if result != rd.ResultCode.Succeeded:
        raise RuntimeError("Couldn't initialise replay for {}: {}".format(api, str(result)))

    return controller


def fetch_indices(controller: rd.ReplayController, action: rd.ActionDescription, mesh: rd.MeshFormat, index_offset: int, first_index: int, num_indices: int):

    pipe = controller.GetPipelineState()
    restart_idx = pipe.GetRestartIndex() & ((1 << (mesh.indexByteStride*8)) - 1)
    restart_enabled = pipe.IsRestartEnabled()

    # If we have an index buffer
    if mesh.indexResourceId != rd.ResourceId.Null():
        offset = mesh.indexByteStride*(first_index + index_offset)

        avail_bytes = mesh.indexByteSize
        if avail_bytes > offset:
            avail_bytes = avail_bytes - offset
        else:
            avail_bytes = 0

        read_bytes = min([avail_bytes, mesh.indexByteStride*num_indices])

        # Fetch the data
        if read_bytes > 0:
            ibdata = controller.GetBufferData(mesh.indexResourceId,
                                              mesh.indexByteOffset + offset,
                                              read_bytes)
        else:
            ibdata = bytes()

        # Get the character for the width of index
        index_fmt = 'B'
        if mesh.indexByteStride == 2:
            index_fmt = 'H'
        elif mesh.indexByteStride == 4:
            index_fmt = 'I'

        avail_indices = int(len(ibdata) / mesh.indexByteStride)

        # Duplicate the format by the number of indices
        index_fmt = '=' + str(min([avail_indices, num_indices])) + index_fmt

        # Unpack all the indices
        indices = struct.unpack_from(index_fmt, ibdata)

        extra = []
        if avail_indices < num_indices:
            extra = [None] * (num_indices - avail_indices)

        # Apply the baseVertex offset
        return [i if restart_enabled and i == restart_idx else i + mesh.baseVertex for i in indices] + extra
    else:
        # With no index buffer, just generate a range
        return tuple(range(first_index, first_index + num_indices))


class MeshAttribute:
    mesh: rd.MeshFormat
    name: str


def get_vsin_attrs(controller: rd.ReplayController, vertexOffset: int, index_mesh: rd.MeshFormat):
    pipe: rd.PipeState = controller.GetPipelineState()
    inputs: List[rd.VertexInputAttribute] = pipe.GetVertexInputs()

    attrs: List[MeshAttribute] = []
    vbs: List[rd.BoundVBuffer] = pipe.GetVBuffers()

    for a in inputs:
        if not a.used:
            continue

        attr = MeshAttribute()
        attr.name = a.name
        attr.mesh = rd.MeshFormat(index_mesh)

        attr.mesh.vertexByteStride = vbs[a.vertexBuffer].byteStride
        attr.mesh.instStepRate = a.instanceRate
        attr.mesh.instanced = a.perInstance
        attr.mesh.vertexResourceId = vbs[a.vertexBuffer].resourceId

        offs = a.byteOffset + vertexOffset * attr.mesh.vertexByteStride

        attr.mesh.vertexByteOffset = vbs[a.vertexBuffer].byteOffset + offs
        attr.mesh.vertexByteSize = max([0, vbs[a.vertexBuffer].byteSize - offs])

        attr.mesh.format = a.format

        attrs.append(attr)

    return attrs


def get_postvs_attrs(controller: rd.ReplayController, mesh: rd.MeshFormat, data_stage: rd.MeshDataStage):
    pipe: rd.PipeState = controller.GetPipelineState()

    if data_stage == rd.MeshDataStage.VSOut:
        shader = pipe.GetShaderReflection(rd.ShaderStage.Vertex)
    else:
        shader = pipe.GetShaderReflection(rd.ShaderStage.Geometry)
        if shader is None:
            shader = pipe.GetShaderReflection(rd.ShaderStage.Domain)

    attrs: List[MeshAttribute] = []
    posidx = 0

    for sig in shader.outputSignature:
        attr = MeshAttribute()
        attr.mesh = rd.MeshFormat(mesh)

        if pipe.GetRasterizedStream() >= 0:
            if sig.stream != pipe.GetRasterizedStream():
                continue
        else:
            if sig.stream != 0:
                continue

        # Construct a resource format for this element
        attr.mesh.format = rd.ResourceFormat()
        attr.mesh.format.compByteWidth = rd.VarTypeByteSize(sig.varType)
        attr.mesh.format.compCount = sig.compCount
        attr.mesh.format.compType = rd.VarTypeCompType(sig.varType)
        attr.mesh.format.type = rd.ResourceFormatType.Regular

        attr.name = sig.semanticIdxName if sig.varName == '' else sig.varName

        if sig.systemValue == rd.ShaderBuiltin.Position:
            posidx = len(attrs)

        attrs.append(attr)

    # Shuffle the position element to the front
    if posidx > 0:
        pos = attrs[posidx]
        del attrs[posidx]
        attrs.insert(0, pos)

    accum_offset = 0

    for i in range(0, len(attrs)):
        # Note that some APIs such as Vulkan will pad the size of the attribute here
        # while others will tightly pack
        fmt = attrs[i].mesh.format

        elem_size = (8 if fmt.compByteWidth > 4 else 4)

        alignment = elem_size
        if fmt.compCount == 2:
            alignment = elem_size * 2
        elif fmt.compCount > 2:
            alignment = elem_size * 4

        if pipe.HasAlignedPostVSData(data_stage) and (accum_offset % alignment) != 0:
            accum_offset += alignment - (accum_offset % alignment)

        attrs[i].mesh.vertexByteOffset += accum_offset

        accum_offset += elem_size * fmt.compCount

    return attrs


# Unpack a tuple of the given format, from the data
def unpack_data(fmt: rd.ResourceFormat, data: bytes, data_offset: int):
    # We don't handle 'special' formats - typically bit-packed such as 10:10:10:2
    if fmt.Special():
        raise RuntimeError("Packed formats are not supported!")

    format_chars = {
        #                   012345678
        rd.CompType.UInt:  "xBHxIxxxQ",
        rd.CompType.SInt:  "xbhxixxxq",
        rd.CompType.Float: "xxexfxxxd",  # only 2, 4 and 8 are valid
    }

    # These types have identical decodes, but we might post-process them
    format_chars[rd.CompType.UNorm] = format_chars[rd.CompType.UInt]
    format_chars[rd.CompType.UScaled] = format_chars[rd.CompType.UInt]
    format_chars[rd.CompType.SNorm] = format_chars[rd.CompType.SInt]
    format_chars[rd.CompType.SScaled] = format_chars[rd.CompType.SInt]

    # We need to fetch compCount components
    vertex_format = '=' + str(fmt.compCount) + format_chars[fmt.compType][fmt.compByteWidth]

    if data_offset >= len(data):
        return None

    # Unpack the data
    try:
        value = struct.unpack_from(vertex_format, data, data_offset)
    except struct.error as ex:
        raise

    # If the format needs post-processing such as normalisation, do that now
    if fmt.compType == rd.CompType.UNorm:
        divisor = float((1 << (fmt.compByteWidth*8)) - 1)
        value = tuple(float(i) / divisor for i in value)
    elif fmt.compType == rd.CompType.SNorm:
        max_neg = -(1 << (fmt.compByteWidth*8 - 1))
        divisor = -float(max_neg+1)
        value = tuple(-1.0 if (i == max_neg) else float(i / divisor) for i in value)
    elif fmt.compType == rd.CompType.UScaled or fmt.compType == rd.CompType.SScaled:
        value = tuple(float(i) for i in value)

    # If the format is BGRA, swap the two components
    if fmt.BGRAOrder():
        value = tuple(value[i] for i in [2, 1, 0, 3])

    return value


def decode_mesh_data(controller: rd.ReplayController, indices: List[int], display_indices: List[int],
                     attrs: List[MeshAttribute], instance: int = 0, indexOffset: int = 0):
    ret = []

    buffer_ranges = {}
    for attr in attrs:
        begin = attr.mesh.vertexByteOffset
        end = min(begin + attr.mesh.vertexByteSize, 0xffffffffffffffff)

        # This could be more optimal if we figure out the lower/upper bounds of any attribute and only fetch the
        # data we need. For each referenced buffer, pick the attribute that references the largest range and fetch that
        if attr.mesh.vertexResourceId in buffer_ranges:
            buf_range = buffer_ranges[attr.mesh.vertexResourceId]

            if buf_range[0] < begin:
                begin = buf_range[0]
            if buf_range[1] > end:
                end = buf_range[1]

        buffer_ranges[attr.mesh.vertexResourceId] = (begin, end)

    buffer_data = {}
    for buf, buf_range in buffer_ranges.items():
        buffer_data[buf] = controller.GetBufferData(buf, buf_range[0], buf_range[1] - buf_range[0])

    # Calculate the strip restart index for this index width
    striprestart_index = None
    if controller.GetPipelineState().IsRestartEnabled() and attrs[0].mesh.indexResourceId != rd.ResourceId.Null():
        striprestart_index = (controller.GetPipelineState().GetRestartIndex() &
                              ((1 << (attrs[0].mesh.indexByteStride*8)) - 1))

    for i,idx in enumerate(indices):
        vertex = {'vtx': i, 'idx': display_indices[i]}

        if striprestart_index is None or idx != striprestart_index:
            for attr in attrs:
                if idx is None:
                    vertex[attr.name] = None
                    continue

                offset = attr.mesh.vertexByteStride * idx

                if attr.mesh.instanced:
                    offset = (attr.mesh.vertexByteStride +
                              attr.mesh.vertexByteStride * int(instance / max(attr.mesh.instStepRate, 1)))

                vertex[attr.name] = unpack_data(attr.mesh.format, buffer_data[attr.mesh.vertexResourceId],
                                                attr.mesh.vertexByteOffset + offset -
                                                buffer_ranges[attr.mesh.vertexResourceId][0])

        ret.append(vertex)

    return ret
