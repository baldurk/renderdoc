/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#pragma once

#include "replay_enums.h"

DOCUMENT("A floating point four-component vector");
struct FloatVector
{
  FloatVector() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
  FloatVector(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}
  DOCUMENT("The x component.");
  float x;
  DOCUMENT("The y component.");
  float y;
  DOCUMENT("The z component.");
  float z;
  DOCUMENT("The w component.");
  float w;
};

DECLARE_REFLECTION_STRUCT(FloatVector);

DOCUMENT("Properties of a path on a remote filesystem.");
struct PathEntry
{
  PathEntry() : flags(PathProperty::NoFlags), lastmod(0), size(0) {}
  PathEntry(const char *fn, PathProperty f) : filename(fn), flags(f), lastmod(0), size(0) {}
  DOCUMENT("The filename of this path. This contains only the filename, not the full path.");
  rdctype::str filename;

  DOCUMENT("The :class:`PathProperty` flags for this path.");
  PathProperty flags;

  DOCUMENT("The last modified date of this path, as a unix timestamp in UTC.");
  uint32_t lastmod;

  DOCUMENT("The size of the path in bytes.");
  uint64_t size;
};

DECLARE_REFLECTION_STRUCT(PathEntry);

DOCUMENT("Description of the format of a resource or element.");
struct ResourceFormat
{
  ResourceFormat()
  {
    special = true;
    specialFormat = SpecialFormat::Unknown;

    compCount = compByteWidth = 0;
    compType = CompType::Float;

    bgraOrder = false;
    srgbCorrected = false;
  }

  DOCUMENT("Compares two ``ResourceFormat`` objects for equality.");
  bool operator==(const ResourceFormat &r) const
  {
    if(special || r.special)
      return special == r.special && specialFormat == r.specialFormat && compType == r.compType;

    return compCount == r.compCount && compByteWidth == r.compByteWidth && compType == r.compType &&
           bgraOrder == r.bgraOrder && srgbCorrected == r.srgbCorrected;
  }

  DOCUMENT("Compares two ``ResourceFormat`` objects for inequality.");
  bool operator!=(const ResourceFormat &r) const { return !(*this == r); }
  // indicates it's not a type represented with the members below
  // usually this means non-uniform across components or block compressed
  DOCUMENT("``True`` if :data:`specialFormat` is valid.");
  bool32 special;
  DOCUMENT("The :class:`SpecialFormat` if it's a non-uniform layout like block-compressed.");
  SpecialFormat specialFormat;

  DOCUMENT("The name of the format.");
  rdctype::str strname;

  DOCUMENT("The number of components in each vertex.");
  uint32_t compCount;
  DOCUMENT("The width in bytes of each component.");
  uint32_t compByteWidth;
  DOCUMENT("The :class:`type <CompType>` of each component.");
  CompType compType;

  DOCUMENT("``True`` if the components are to be read in ``BGRA`` order.");
  bool32 bgraOrder;
  DOCUMENT("``True`` if the components are SRGB corrected on read and write.");
  bool32 srgbCorrected;
};

DECLARE_REFLECTION_STRUCT(ResourceFormat);

DOCUMENT("The details of a texture filter in a sampler.");
struct TextureFilter
{
  DOCUMENT("The :class:`FilterMode` to use when minifying the texture.");
  FilterMode minify = FilterMode::NoFilter;
  DOCUMENT("The :class:`FilterMode` to use when magnifying the texture.");
  FilterMode magnify = FilterMode::NoFilter;
  DOCUMENT("The :class:`FilterMode` to use when interpolating between mips.");
  FilterMode mip = FilterMode::NoFilter;
  DOCUMENT("The :class:`FilterFunc` to apply after interpolating values.");
  FilterFunc func = FilterFunc::Normal;
};

DOCUMENT("A description of a buffer resource.");
struct BufferDescription
{
  DOCUMENT("The unique :class:`ResourceId` that identifies this buffer.");
  ResourceId ID;

  DOCUMENT("The name given to this buffer.");
  rdctype::str name;

  DOCUMENT(R"(``True`` if the name was assigned by the application, otherwise it's autogenerated
based on the ID.
)");
  bool32 customName;

  DOCUMENT("The way this buffer will be used in the pipeline.");
  BufferCategory creationFlags;

  DOCUMENT("The byte length of the buffer.");
  uint64_t length;
};

DECLARE_REFLECTION_STRUCT(BufferDescription);

DOCUMENT("A description of a texture resource.");
struct TextureDescription
{
  DOCUMENT("The name given to this buffer.");
  rdctype::str name;

  DOCUMENT(R"(``True`` if the name was assigned by the application, otherwise it's autogenerated
based on the ID.
)");
  bool32 customName;

  DOCUMENT("The :class:`ResourceFormat` that describes the format of each pixel in the texture.");
  ResourceFormat format;

  DOCUMENT("The base dimension of the texture - either 1, 2, or 3.");
  uint32_t dimension;

  DOCUMENT("The :class:`TextureDim` of the texture.");
  TextureDim resType;

  DOCUMENT("The width of the texture, or length for buffer textures.");
  uint32_t width;

  DOCUMENT("The height of the texture, or 1 if not applicable.");
  uint32_t height;

  DOCUMENT("The depth of the texture, or 1 if not applicable.");
  uint32_t depth;

  DOCUMENT("The unique :class:`ResourceId` that identifies this buffer.");
  ResourceId ID;

  DOCUMENT("``True`` if this texture is used as a cubemap or cubemap array.");
  bool32 cubemap;

  DOCUMENT("How many mips this texture has, will be at least 1.");
  uint32_t mips;

  DOCUMENT("How many array elements this texture has, will be at least 1.");
  uint32_t arraysize;

  DOCUMENT("The way this texture will be used in the pipeline.");
  TextureCategory creationFlags;

  DOCUMENT("The quality setting of this texture, or 0 if not applicable.");
  uint32_t msQual;

  DOCUMENT("How many multisampled samples this texture has, will be at least 1.");
  uint32_t msSamp;

  DOCUMENT("How many bytes would be used to store this texture and all its mips/slices.");
  uint64_t byteSize;
};

DECLARE_REFLECTION_STRUCT(TextureDescription);

DOCUMENT("An individual API-level event, generally corresponds one-to-one with an API call.");
struct APIEvent
{
  DOCUMENT(R"(The API event's Event ID (EID).

This is a 1-based count of API events in the capture. The EID is used as a reference point in
many places in the API to represent where in the capture the 'current state' is, and to perform
analysis in reference to the state at a particular point in the frame.

EIDs are always increasing and positive, but they may not be contiguous - in some circumstances
there may be gaps if some events are consumed entirely internally, such as debug marker pops which
only modify the internal drawcall tree structures.

Also EIDs may not correspond directly to an actual function call - sometimes a function such as a
multi draw indirect will be one function call that expands to multiple events to allow inspection of
results part way through the multi draw.
)");
  uint32_t eventID;

  DOCUMENT("A list of addresses in the CPU callstack where this function was called.");
  rdctype::array<uint64_t> callstack;

  DOCUMENT("A raw debug string with the serialised form of the function call parameters.");
  rdctype::str eventDesc;

  DOCUMENT(R"(A byte offset in the data stream where this event happens.

.. note:: This should only be used as a relative measure, it is not a literal number of bytes from
  the start of the file on disk.
)");
  uint64_t fileOffset;
};

DECLARE_REFLECTION_STRUCT(APIEvent);

DOCUMENT("A debugging message from the API validation or internal analysis and error detection.");
struct DebugMessage
{
  DOCUMENT("The :data:`EID <APIEvent.eventID>` where this debug message was found.");
  uint32_t eventID;

  DOCUMENT("The :class:`category <MessageCategory>` of this debug message.");
  MessageCategory category;

  DOCUMENT("The :class:`severity <MessageSeverity>` of this debug message.");
  MessageSeverity severity;

  DOCUMENT("The :class:`source <MessageSource>` of this debug message.");
  MessageSource source;

  DOCUMENT("An ID that identifies this particular debug message uniquely.");
  uint32_t messageID;

  DOCUMENT("The string contents of the message.");
  rdctype::str description;
};

DECLARE_REFLECTION_STRUCT(DebugMessage);

DOCUMENT(R"(The type of bucketing method for recording statistics.

.. data:: Linear

  Each bucket contains a fixed number of elements. The highest bucket also accumulates any values
  too high for any of the buckets.

.. data:: Pow2

  Each bucket holds twice as many elements as the previous one, with the first bucket containing
  just 1 (bucket index is ``log2(value)``).
)");
enum class BucketRecordType : int
{
  Linear,
  Pow2,
};

DOCUMENT(R"(Contains the statistics for constant binds in a frame.

.. data:: BucketType

  The type of buckets being used. See :class:`BucketRecordType`.

.. data:: BucketCount

  How many buckets there are in the arrays.
)");
struct ConstantBindStats
{
  static const BucketRecordType BucketType = BucketRecordType::Pow2;
  static const size_t BucketCount = 31;

  DOCUMENT("How many function calls were made.");
  uint32_t calls;

  DOCUMENT("How many objects were bound.");
  uint32_t sets;

  DOCUMENT("How many objects were unbound.");
  uint32_t nulls;

  DOCUMENT("A list where the Nth element contains the number of calls that bound N buffers.");
  rdctype::array<uint32_t> bindslots;

  DOCUMENT("A :class:`bucketed <BucketType>` list over the sizes of buffers bound.");
  rdctype::array<uint32_t> sizes;
};

DECLARE_REFLECTION_STRUCT(ConstantBindStats);

DOCUMENT("Contains the statistics for sampler binds in a frame.");
struct SamplerBindStats
{
  DOCUMENT("How many function calls were made.");
  uint32_t calls;

  DOCUMENT("How many objects were bound.");
  uint32_t sets;

  DOCUMENT("How many objects were unbound.");
  uint32_t nulls;

  DOCUMENT("A list where the Nth element contains the number of calls that bound N samplers.");
  rdctype::array<uint32_t> bindslots;
};

DECLARE_REFLECTION_STRUCT(SamplerBindStats);

DOCUMENT("Contains the statistics for resource binds in a frame.");
struct ResourceBindStats
{
  DOCUMENT("How many function calls were made.");
  uint32_t calls;

  DOCUMENT("How many objects were bound.");
  uint32_t sets;

  DOCUMENT("How many objects were unbound.");
  uint32_t nulls;

  DOCUMENT(R"(A list with one element for each type in :class:`TextureDim`.

The Nth element contains the number of times a resource of that type was bound.
)");
  rdctype::array<uint32_t> types;

  DOCUMENT("A list where the Nth element contains the number of calls that bound N resources.");
  rdctype::array<uint32_t> bindslots;
};

DECLARE_REFLECTION_STRUCT(ResourceBindStats);

DOCUMENT(R"(Contains the statistics for resource updates in a frame.

.. data:: BucketType

  The type of buckets being used. See :class:`BucketRecordType`.

.. data:: BucketCount

  How many buckets there are in the arrays.
)");
struct ResourceUpdateStats
{
  static const BucketRecordType BucketType = BucketRecordType::Pow2;
  static const size_t BucketCount = 31;

  DOCUMENT("How many function calls were made.");
  uint32_t calls;

  DOCUMENT("How many of :data:`calls` were mapped pointers written by the CPU.");
  uint32_t clients;

  DOCUMENT("How many of :data:`calls` were batched updates written in the command queue.");
  uint32_t servers;

  DOCUMENT(R"(A list with one element for each type in :class:`TextureDim`.

The Nth element contains the number of times a resource of that type was updated.
)");
  rdctype::array<uint32_t> types;

  DOCUMENT("A :class:`bucketed <BucketType>` list over the number of bytes in the update.");
  rdctype::array<uint32_t> sizes;
};

DECLARE_REFLECTION_STRUCT(ResourceUpdateStats);

DOCUMENT(R"(Contains the statistics for draws in a frame.

.. data:: BucketType

  The type of buckets being used. See :class:`BucketRecordType`.

.. data:: BucketSize

  How many elements each bucket contains.

.. data:: BucketCount

  How many buckets there are in the arrays.
)");
struct DrawcallStats
{
  static const BucketRecordType BucketType = BucketRecordType::Linear;
  static const size_t BucketSize = 1;
  static const size_t BucketCount = 16;

  DOCUMENT("How many draw calls were made.");
  uint32_t calls;
  DOCUMENT("How many of :data:`calls` were instanced.");
  uint32_t instanced;
  DOCUMENT("How many of :data:`calls` were indirect.");
  uint32_t indirect;

  DOCUMENT("A :class:`bucketed <BucketType>` list over the number of instances in the draw.");
  rdctype::array<uint32_t> counts;
};

DECLARE_REFLECTION_STRUCT(DrawcallStats);

DOCUMENT("Contains the statistics for compute dispatches in a frame.");
struct DispatchStats
{
  DOCUMENT("How many dispatch calls were made.");
  uint32_t calls;

  DOCUMENT("How many of :data:`calls` were indirect.");
  uint32_t indirect;
};

DECLARE_REFLECTION_STRUCT(DispatchStats);

DOCUMENT("Contains the statistics for index buffer binds in a frame.");
struct IndexBindStats
{
  DOCUMENT("How many function calls were made.");
  uint32_t calls;

  DOCUMENT("How many objects were bound.");
  uint32_t sets;

  DOCUMENT("How many objects were unbound.");
  uint32_t nulls;
};

DECLARE_REFLECTION_STRUCT(IndexBindStats);

DOCUMENT("Contains the statistics for vertex buffer binds in a frame.");
struct VertexBindStats
{
  DOCUMENT("How many function calls were made.");
  uint32_t calls;

  DOCUMENT("How many objects were bound.");
  uint32_t sets;

  DOCUMENT("How many objects were unbound.");
  uint32_t nulls;

  DOCUMENT(
      "A list where the Nth element contains the number of calls that bound N vertex buffers.");
  rdctype::array<uint32_t> bindslots;
};

DECLARE_REFLECTION_STRUCT(VertexBindStats);

DOCUMENT("Contains the statistics for vertex layout binds in a frame.");
struct LayoutBindStats
{
  DOCUMENT("How many function calls were made.");
  uint32_t calls;

  DOCUMENT("How many objects were bound.");
  uint32_t sets;

  DOCUMENT("How many objects were unbound.");
  uint32_t nulls;
};

DECLARE_REFLECTION_STRUCT(LayoutBindStats);

DOCUMENT("Contains the statistics for shader binds in a frame.");
struct ShaderChangeStats
{
  DOCUMENT("How many function calls were made.");
  uint32_t calls;

  DOCUMENT("How many objects were bound.");
  uint32_t sets;

  DOCUMENT("How many objects were unbound.");
  uint32_t nulls;

  DOCUMENT("How many calls made no change due to the existing bind being identical.");
  uint32_t redundants;
};

DECLARE_REFLECTION_STRUCT(ShaderChangeStats);

DOCUMENT("Contains the statistics for blend state binds in a frame.");
struct BlendStats
{
  DOCUMENT("How many function calls were made.");
  uint32_t calls;

  DOCUMENT("How many objects were bound.");
  uint32_t sets;

  DOCUMENT("How many objects were unbound.");
  uint32_t nulls;

  DOCUMENT("How many calls made no change due to the existing bind being identical.");
  uint32_t redundants;
};

DECLARE_REFLECTION_STRUCT(BlendStats);

DOCUMENT("Contains the statistics for depth stencil state binds in a frame.");
struct DepthStencilStats
{
  DOCUMENT("How many function calls were made.");
  uint32_t calls;

  DOCUMENT("How many objects were bound.");
  uint32_t sets;

  DOCUMENT("How many objects were unbound.");
  uint32_t nulls;

  DOCUMENT("How many calls made no change due to the existing bind being identical.");
  uint32_t redundants;
};

DECLARE_REFLECTION_STRUCT(DepthStencilStats);

DOCUMENT("Contains the statistics for rasterizer state binds in a frame.");
struct RasterizationStats
{
  DOCUMENT("How many function calls were made.");
  uint32_t calls;

  DOCUMENT("How many objects were bound.");
  uint32_t sets;

  DOCUMENT("How many objects were unbound.");
  uint32_t nulls;

  DOCUMENT("How many calls made no change due to the existing bind being identical.");
  uint32_t redundants;

  DOCUMENT("A list where the Nth element contains the number of calls that bound N viewports.");
  rdctype::array<uint32_t> viewports;

  DOCUMENT("A list where the Nth element contains the number of calls that bound N scissor rects.");
  rdctype::array<uint32_t> rects;
};

DECLARE_REFLECTION_STRUCT(RasterizationStats);

DOCUMENT("Contains the statistics for output merger or UAV binds in a frame.");
struct OutputTargetStats
{
  DOCUMENT("How many function calls were made.");
  uint32_t calls;

  DOCUMENT("How many objects were bound.");
  uint32_t sets;

  DOCUMENT("How many objects were unbound.");
  uint32_t nulls;

  DOCUMENT("A list where the Nth element contains the number of calls that bound N targets.");
  rdctype::array<uint32_t> bindslots;
};

DECLARE_REFLECTION_STRUCT(OutputTargetStats);

DOCUMENT(R"(Contains all the available statistics about the captured frame.

Currently this information is only available on D3D11 and is fairly API-centric.
)");
struct FrameStatistics
{
  DOCUMENT("``True`` if the statistics in this structure are valid.");
  bool32 recorded;

  DOCUMENT("A list of constant buffer bind statistics, one per each :class:`stage <ShaderStage>`.");
  ConstantBindStats constants[ENUM_ARRAY_SIZE(ShaderStage)];

  DOCUMENT("A list of sampler bind statistics, one per each :class:`stage <ShaderStage>`.");
  SamplerBindStats samplers[ENUM_ARRAY_SIZE(ShaderStage)];

  DOCUMENT("A list of resource bind statistics, one per each :class:`stage <ShaderStage>`.");
  ResourceBindStats resources[ENUM_ARRAY_SIZE(ShaderStage)];

  DOCUMENT("Information about resource contents updates.");
  ResourceUpdateStats updates;

  DOCUMENT("Information about drawcalls.");
  DrawcallStats draws;

  DOCUMENT("Information about compute dispatches.");
  DispatchStats dispatches;

  DOCUMENT("Information about index buffer binds.");
  IndexBindStats indices;

  DOCUMENT("Information about vertex buffer binds.");
  VertexBindStats vertices;

  DOCUMENT("Information about vertex layout binds.");
  LayoutBindStats layouts;

  DOCUMENT("A list of shader bind statistics, one per each :class:`stage <ShaderStage>`.");
  ShaderChangeStats shaders[ENUM_ARRAY_SIZE(ShaderStage)];

  DOCUMENT("Information about blend state binds.");
  BlendStats blends;

  DOCUMENT("Information about depth-stencil state binds.");
  DepthStencilStats depths;

  DOCUMENT("Information about rasterizer state binds.");
  RasterizationStats rasters;

  DOCUMENT("Information about output merger and UAV binds.");
  OutputTargetStats outputs;
};

DECLARE_REFLECTION_STRUCT(FrameStatistics);

DOCUMENT("Contains frame-level global information");
struct FrameDescription
{
  FrameDescription()
      : frameNumber(0),
        fileOffset(0),
        uncompressedFileSize(0),
        compressedFileSize(0),
        persistentSize(0),
        initDataSize(0),
        captureTime(0)
  {
  }

  DOCUMENT(R"(Starting from frame #1 defined as the time from application startup to first present,
this counts the frame number when the capture was made.

.. note:: This value is only accurate if the capture was triggered through the default mechanism, if
  it was triggered from the application API it doesn't correspond to anything.
)");
  uint32_t frameNumber;

  DOCUMENT(R"(The offset into the file of the start of the frame.

.. note:: Similarly to :data:`APIEvent.fileOffset` this should only be used as a relative measure,
  as it is not a literal number of bytes from the start of the file on disk.
)");
  uint64_t fileOffset;

  DOCUMENT("The total file size of the whole capture in bytes, after decompression.");
  uint64_t uncompressedFileSize;

  DOCUMENT("The total file size of the whole capture in bytes, before decompression.");
  uint64_t compressedFileSize;

  DOCUMENT("The byte size of the section of the file that must be kept in memory persistently.");
  uint64_t persistentSize;

  DOCUMENT("The byte size of the section of the file that contains frame-initial contents.");
  uint64_t initDataSize;

  DOCUMENT("The time when the capture was created, as a unix timestamp in UTC.");
  uint64_t captureTime;

  DOCUMENT("The :class:`frame statistics <FrameStatistics>`.");
  FrameStatistics stats;

  DOCUMENT("A list of debug messages that are not associated with any particular event.");
  rdctype::array<DebugMessage> debugMessages;
};

DECLARE_REFLECTION_STRUCT(FrameDescription);

DOCUMENT("Describes a particular use of a resource at a specific :data:`EID <APIEvent.eventID>`.");
struct EventUsage
{
  EventUsage() : eventID(0), usage(ResourceUsage::Unused) {}
  EventUsage(uint32_t e, ResourceUsage u) : eventID(e), usage(u) {}
  EventUsage(uint32_t e, ResourceUsage u, ResourceId v) : eventID(e), usage(u), view(v) {}
  DOCUMENT("Compares two ``EventUsage`` objects for less-than.");
  bool operator<(const EventUsage &o) const
  {
    if(eventID != o.eventID)
      return eventID < o.eventID;
    return usage < o.usage;
  }

  DOCUMENT("Compares two ``EventUsage`` objects for equality.");
  bool operator==(const EventUsage &o) const { return eventID == o.eventID && usage == o.usage; }
  DOCUMENT("The :data:`EID <APIEvent.eventID>` where this usage happened.");
  uint32_t eventID;

  DOCUMENT("The :class:`ResourceUsage` in question.");
  ResourceUsage usage;

  DOCUMENT("An optional :class:`ResourceId` identifying the view through which the use happened.");
  ResourceId view;
};

DECLARE_REFLECTION_STRUCT(EventUsage);

DOCUMENT("Describes the properties of a drawcall, dispatch, debug marker, or similar event.");
struct DrawcallDescription
{
  DrawcallDescription() { Reset(); }
  DOCUMENT("Resets the drawcall back to a default/empty state.");
  void Reset()
  {
    eventID = 0;
    drawcallID = 0;
    flags = DrawFlags::NoFlags;
    markerColor[0] = markerColor[1] = markerColor[2] = markerColor[3] = 0.0f;
    numIndices = 0;
    numInstances = 0;
    indexOffset = 0;
    baseVertex = 0;
    vertexOffset = 0;
    instanceOffset = 0;

    dispatchDimension[0] = dispatchDimension[1] = dispatchDimension[2] = 0;
    dispatchThreadsDimension[0] = dispatchThreadsDimension[1] = dispatchThreadsDimension[2] = 0;

    indexByteWidth = 0;
    topology = Topology::Unknown;

    copySource = ResourceId();
    copyDestination = ResourceId();

    parent = 0;
    previous = 0;
    next = 0;

    for(int i = 0; i < 8; i++)
      outputs[i] = ResourceId();
    depthOut = ResourceId();
  }

  DOCUMENT("The :data:`EID <APIEvent.eventID>` that actually produced the drawcall.");
  uint32_t eventID;
  DOCUMENT("A 1-based index of this drawcall relative to other drawcalls.");
  uint32_t drawcallID;

  DOCUMENT(R"(The name of this drawcall. Typically a summarised/concise list of parameters.

.. note:: For drawcalls, the convention is to list primary parameters (vertex/index count, instance
  count) and omit secondary parameters (vertex offset, instance offset).
)");
  rdctype::str name;

  DOCUMENT("A set of :class:`DrawFlags` properties describing what kind of drawcall this is.");
  DrawFlags flags;

  DOCUMENT("A RGBA colour specified by a debug marker call.");
  float markerColor[4];

  DOCUMENT("The number of indices or vertices as appropriate for the drawcall. 0 if not used.");
  uint32_t numIndices;

  DOCUMENT("The number of instances for the drawcall. 0 if not used.");
  uint32_t numInstances;

  DOCUMENT("For indexed drawcalls, the offset added to each index after fetching.");
  int32_t baseVertex;

  DOCUMENT("For indexed drawcalls, the first index to fetch from the index buffer.");
  uint32_t indexOffset;

  DOCUMENT("For non-indexed drawcalls, the offset applied before looking up each vertex input.");
  uint32_t vertexOffset;

  DOCUMENT(
      "For instanced drawcalls, the offset applied before looking up instanced vertex inputs.");
  uint32_t instanceOffset;

  DOCUMENT("The 3D number of workgroups to dispatch in a dispatch call.");
  uint32_t dispatchDimension[3];

  DOCUMENT("The 3D size of each workgroup in threads if the call allows an override, or 0 if not.");
  uint32_t dispatchThreadsDimension[3];

  DOCUMENT(R"(The width in bytes of each index.

Valid values are 1 (depending on API), 2 or 4, or 0 if the drawcall is not an indexed draw.
)");
  uint32_t indexByteWidth;

  DOCUMENT("The :class:`Topology` used in this drawcall.");
  Topology topology;

  DOCUMENT(R"(The :class:`ResourceId` identifying the source object in a copy, resolve or blit
operation.
)");
  ResourceId copySource;
  DOCUMENT(R"(The :class:`ResourceId` identifying the destination object in a copy, resolve or blit
operation.
)");
  ResourceId copyDestination;

  DOCUMENT(R"(The :data:`EID <APIEvent.eventID>` of the parent of this drawcall, or ``0`` if there
is no parent for this drawcall.
)");
  int64_t parent;

  DOCUMENT(R"(The :data:`EID <APIEvent.eventID>` of the previous drawcall in the frame, or ``0`` if
this is the first drawcall in the frame.
)");
  int64_t previous;
  DOCUMENT(R"(The :data:`EID <APIEvent.eventID>` of the next drawcall in the frame, or ``0`` if this
is the last drawcall in the frame.
)");
  int64_t next;

  DOCUMENT(R"(A simple list of the :class:`ResourceId` ids for the colour outputs, which can be used
for very coarse bucketing of drawcalls into similar passes by their outputs.
)");
  ResourceId outputs[8];
  DOCUMENT("The resource used for depth output - see :data:`outputs`.");
  ResourceId depthOut;

  DOCUMENT("A list of the :class:`APIEvent` events that happened since the previous drawcall.");
  rdctype::array<APIEvent> events;

  DOCUMENT("A list of :class:`DrawcallDescription` child drawcalls.");
  rdctype::array<DrawcallDescription> children;
};

DECLARE_REFLECTION_STRUCT(DrawcallDescription);

DOCUMENT("Gives some API-specific information about the capture.");
struct APIProperties
{
  DOCUMENT("The :class:`GraphicsAPI` of the actual log/capture.");
  GraphicsAPI pipelineType;

  DOCUMENT(R"(The :class:`GraphicsAPI` used to render the log. For remote replay this could be
different to the above, and lets the UI make decisions e.g. to flip rendering of images.
)");
  GraphicsAPI localRenderer;

  DOCUMENT(R"(``True`` if the capture was loaded successfully but running in a degraded mode - e.g.
with software rendering, or with some functionality disabled due to lack of support.
)");
  bool32 degraded;
};

DECLARE_REFLECTION_STRUCT(APIProperties);

DOCUMENT("Describes a GPU counter's purpose and result value.");
struct CounterDescription
{
  DOCUMENT(R"(The :class:`GPUCounter` this counter represents.

.. note:: The value may not correspond to any of the predefined values if it's a hardware-specific
  counter value.
)");
  GPUCounter counterID;

  DOCUMENT("A short human-readable name for the counter.");
  rdctype::str name;

  DOCUMENT("If available, a longer human-readable description of the value this counter measures.");
  rdctype::str description;

  DOCUMENT("The :class:`type of value <CompType>` returned by this counter.");
  CompType resultType;

  DOCUMENT("The number of bytes in the resulting value.");
  uint32_t resultByteWidth;

  DOCUMENT("The :class:`CounterUnit` for the result value.");
  CounterUnit unit;
};

DECLARE_REFLECTION_STRUCT(CounterDescription);

DOCUMENT(R"(A resulting value from a GPU counter. Only one member is valid, see
:class:`CounterDescription`.
)");
union CounterValue
{
  DOCUMENT("A ``float`` value.");
  float f;
  DOCUMENT("A ``double`` value.");
  double d;
  DOCUMENT("A 32-bit unsigned integer.");
  uint32_t u32;
  DOCUMENT("A 64-bit unsigned integer.");
  uint64_t u64;
};

DOCUMENT("The resulting value from a counter at an event.");
struct CounterResult
{
  CounterResult() : eventID(0), counterID(GPUCounter::EventGPUDuration) { value.u64 = 0; }
  CounterResult(uint32_t EID, GPUCounter c, float data) : eventID(EID), counterID(c)
  {
    value.f = data;
  }
  CounterResult(uint32_t EID, GPUCounter c, double data) : eventID(EID), counterID(c)
  {
    value.d = data;
  }
  CounterResult(uint32_t EID, GPUCounter c, uint32_t data) : eventID(EID), counterID(c)
  {
    value.u32 = data;
  }
  CounterResult(uint32_t EID, GPUCounter c, uint64_t data) : eventID(EID), counterID(c)
  {
    value.u64 = data;
  }

  DOCUMENT("Compares two ``CounterResult`` objects for less-than.");
  bool operator<(const CounterResult &o) const
  {
    if(eventID != o.eventID)
      return eventID < o.eventID;
    if(counterID != o.counterID)
      return counterID < o.counterID;

    // don't compare values, just consider equal
    return false;
  }

  DOCUMENT("Compares two ``CounterResult`` objects for equality.");
  bool operator==(const CounterResult &o) const
  {
    // don't compare values, just consider equal by EID/counterID
    return eventID == o.eventID && counterID == o.counterID;
  }

  DOCUMENT("The :data:`EID <APIEvent.eventID>` that produced this value.");
  uint32_t eventID;

  DOCUMENT("The :data:`counter <GPUCounter>` that produced this value.");
  GPUCounter counterID;

  DOCUMENT("The value itself.");
  CounterValue value;
};

DECLARE_REFLECTION_STRUCT(CounterResult);

DOCUMENT("The contents of an RGBA pixel.");
union PixelValue
{
  DOCUMENT("The RGBA value interpreted as ``float``.");
  float value_f[4];
  DOCUMENT("The RGBA value interpreted as 32-bit unsigned integer.");
  uint32_t value_u[4];
  DOCUMENT("The RGBA value interpreted as 32-bit signed integer.");
  int32_t value_i[4];
  DOCUMENT("The RGBA value interpreted as 16-bit unsigned integer.");
  uint16_t value_u16[4];
};

DOCUMENT("The value of pixel output at a particular event.");
struct ModificationValue
{
  DOCUMENT("The colour value.");
  PixelValue col;

  DOCUMENT("The depth output, as a ``float``.");
  float depth;

  DOCUMENT("The stencil output, or ``-1`` if not available.");
  int32_t stencil;
};

DECLARE_REFLECTION_STRUCT(ModificationValue);

DOCUMENT("An attempt to modify a pixel by a particular event.");
struct PixelModification
{
  DOCUMENT("The :data:`EID <APIEvent.eventID>` where the modification happened.");
  uint32_t eventID;

  DOCUMENT("``True`` if this event came as part of an arbitrary shader write.");
  bool32 directShaderWrite;

  DOCUMENT("``True`` if no pixel shader was bound at this event.");
  bool32 unboundPS;

  DOCUMENT(R"(A 0-based index of which fragment this modification corresponds to, in the case that
multiple fragments from a single draw wrote to a pixel.
)");
  uint32_t fragIndex;

  DOCUMENT("The primitive that generated this fragment.");
  uint32_t primitiveID;

  DOCUMENT(R"(The :class:`ModificationValue` of the texture before this fragment ran.

This is valid only for the first fragment if multiple fragments in the same event write to the same
pixel.
)");
  ModificationValue preMod;
  DOCUMENT("The :class:`ModificationValue` that this fragment wrote from the pixel shader.");
  ModificationValue shaderOut;
  DOCUMENT(R"(The :class:`ModificationValue` of the texture after this fragment ran.)");
  ModificationValue postMod;

  DOCUMENT("``True`` if the sample mask eliminated this fragment.");
  bool32 sampleMasked;
  DOCUMENT("``True`` if the backface culling test eliminated this fragment.");
  bool32 backfaceCulled;
  DOCUMENT("``True`` if depth near/far clipping eliminated this fragment.");
  bool32 depthClipped;
  DOCUMENT("``True`` if viewport clipping eliminated this fragment.");
  bool32 viewClipped;
  DOCUMENT("``True`` if scissor clipping eliminated this fragment.");
  bool32 scissorClipped;
  DOCUMENT("``True`` if the pixel shader executed a discard on this fragment.");
  bool32 shaderDiscarded;
  DOCUMENT("``True`` if depth testing eliminated this fragment.");
  bool32 depthTestFailed;
  DOCUMENT("``True`` if stencil testing eliminated this fragment.");
  bool32 stencilTestFailed;

  DOCUMENT(R"(Determine if this fragment passed all tests and wrote to the texture.

:return: ``True`` if it passed all tests, ``False`` if it failed any.
:rtype: bool
)");
  bool passed() const
  {
    return !sampleMasked && !backfaceCulled && !depthClipped && !viewClipped && !scissorClipped &&
           !shaderDiscarded && !depthTestFailed && !stencilTestFailed;
  }
};

DECLARE_REFLECTION_STRUCT(PixelModification);
