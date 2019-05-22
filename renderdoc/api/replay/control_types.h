/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "data_types.h"
#include "replay_enums.h"

DOCUMENT(R"(
Contains the details of a single element of data (such as position or texture
co-ordinates) within a mesh.)");
struct MeshFormat
{
  MeshFormat()
  {
    indexByteOffset = 0;
    indexByteStride = 0;
    baseVertex = 0;
    vertexByteOffset = 0;
    vertexByteStride = 0;
    instStepRate = 1;
    showAlpha = false;
    topology = Topology::Unknown;
    numIndices = 0;
    unproject = false;
    instanced = false;
    nearPlane = farPlane = 0.0f;
  }
  MeshFormat(const MeshFormat &o) = default;

  DOCUMENT("The :class:`ResourceId` of the index buffer that goes with this mesh element.");
  ResourceId indexResourceId;
  DOCUMENT("The offset in bytes where the indices start in idxbuf.");
  uint64_t indexByteOffset;
  DOCUMENT("The width in bytes of each index. Valid values are 1 (depending on API), 2 or 4.");
  uint32_t indexByteStride;
  DOCUMENT("For indexed meshes, a value added to each index before using it to read the vertex.");
  int32_t baseVertex;

  DOCUMENT("The :class:`ResourceId` of the vertex buffer containing this mesh element.");
  ResourceId vertexResourceId;
  DOCUMENT("The offset in bytes to the start of the vertex data.");
  uint64_t vertexByteOffset;
  DOCUMENT("The stride in bytes between the start of one vertex and the start of another.");
  uint32_t vertexByteStride;

  DOCUMENT("The :class:`ResourceFormat` describing this mesh component.");
  ResourceFormat format;

  DOCUMENT(
      "The color to use for rendering the wireframe of this mesh element, as a "
      ":class:`FloatVector`.");
  FloatVector meshColor;

  DOCUMENT("The :class:`Topology` that describes the primitives in this mesh.");
  Topology topology;
  DOCUMENT("The number of vertices in the mesh.");
  uint32_t numIndices;
  DOCUMENT("The number of instances to render with the same value. See :data:`instanced`.");
  uint32_t instStepRate;

  DOCUMENT("The near plane for the projection matrix.");
  float nearPlane;
  DOCUMENT("The far plane for the projection matrix.");
  float farPlane;
  DOCUMENT("``True`` if this mesh element contains post-projection positional data.");
  bool unproject;

  DOCUMENT("``True`` if this mesh element comes from instanced data. See :data:`instStepRate`.");
  bool instanced;

  DOCUMENT("``True`` if the alpha component of this element should be used.");
  bool showAlpha;
};

DECLARE_REFLECTION_STRUCT(MeshFormat);

struct ICamera;

DOCUMENT(R"(
Describes how to render a mesh preview of one or more meshes. Describes the camera configuration as
well as what options to use when rendering both the current mesh, and any other auxilliary meshes.

.. data:: NoHighlight

  Value for :data:`highlightVert` if no vertex should be highlighted.
)");
struct MeshDisplay
{
  DOCUMENT("");
  MeshDisplay() = default;
  MeshDisplay(const MeshDisplay &) = default;

  DOCUMENT("The :class:`MeshDataStage` where this mesh data comes from.");
  MeshDataStage type;

  DOCUMENT("The :class:`Camera` to use when rendering all of the meshes.");
  ICamera *cam;

  DOCUMENT(
      "``True`` if the projection matrix to use when unprojecting vertex positions is "
      "orthographic.");
  bool ortho;
  DOCUMENT("The field of view to use when calculating a perspective projection matrix.");
  float fov;
  DOCUMENT("The aspect ratio to use when calculating a perspective projection matrix.");
  float aspect;

  DOCUMENT(
      "``True`` if all previous instances in the drawcall should be drawn as secondary meshes.");
  bool showPrevInstances;
  DOCUMENT("``True`` if all instances in the drawcall should be drawn as secondary meshes.");
  bool showAllInstances;
  DOCUMENT(
      "``True`` if all draws in the current pass up to the current draw should be drawn as "
      "secondary meshes.");
  bool showWholePass;
  DOCUMENT("The index of the currently selected instance in the drawcall.");
  uint32_t curInstance;
  DOCUMENT("The index of the currently selected multiview view in the drawcall.");
  uint32_t curView;

  DOCUMENT("The index of the vertex to highlight, or :data:`NoHighlight` to select no vertex.");
  uint32_t highlightVert;
  DOCUMENT("The :class:`MeshFormat` of the position data for the mesh.");
  MeshFormat position;
  DOCUMENT(
      "The :class:`MeshFormat` of the secondary data for the mesh, if used for solid shading.");
  MeshFormat second;

  DOCUMENT("The minimum co-ordinates in each axis of the mesh bounding box.");
  FloatVector minBounds;
  DOCUMENT("The maximum co-ordinates in each axis of the mesh bounding box.");
  FloatVector maxBounds;
  DOCUMENT("``True`` if the bounding box around the mesh should be rendered.");
  bool showBBox;

  DOCUMENT("The :class:`solid shading mode <SolidShade>` to use when rendering the current mesh.");
  SolidShade solidShadeMode;
  DOCUMENT("``True`` if the wireframe of the mesh should be rendered as well as solid shading.");
  bool wireframeDraw;

  static const uint32_t NoHighlight = ~0U;
};

DECLARE_REFLECTION_STRUCT(MeshDisplay);

DOCUMENT(R"(
Describes how to render a texture preview of an image. Describes the zoom and pan settings for the
texture when rendering on a particular output, as well as the modification and selection of a
particular subresource (such as array slice, mip or multi-sampled sample).

.. note::
  X and Y co-ordinates are always considered to be top-left, even on GL, for consistency between
  APIs and preventing the need for API-specific code in most cases. This means if co-ordinates are
  fetched from e.g. viewport or scissor data or other GL pipeline state which is perhaps in
  bottom-left co-ordinates, care must be taken to translate them.

.. data:: ResolveSamples

  Value for :data:`sampleIdx` if the samples should be averaged.
)");
struct TextureDisplay
{
  DOCUMENT("");
  TextureDisplay() = default;
  TextureDisplay(const TextureDisplay &) = default;

  DOCUMENT("The :class:`ResourceId` of the texture to display.");
  ResourceId resourceId;

  DOCUMENT("An optional :class:`CompType` hint to use when displaying a typeless texture.");
  CompType typeHint = CompType::Typeless;

  DOCUMENT("The value in each channel to map to the black point.");
  float rangeMin = 0.0f;

  DOCUMENT("The value in each channel to map to the white point.");
  float rangeMax = 1.0f;

  DOCUMENT(R"(The scale to apply to the texture when rendering as a floating point value.

``1.0`` corresponds to ``100%``
)");
  float scale = 1.0f;

  DOCUMENT(R"(``True`` if the red channel should be visible.

If only one channel is selected, it will be rendered in grayscale
)");
  bool red = true;

  DOCUMENT(R"(``True`` if the green channel should be visible.

If only one channel is selected, it will be rendered in grayscale
)");
  bool green = true;

  DOCUMENT(R"(``True`` if the blue channel should be visible.

If only one channel is selected, it will be rendered in grayscale
)");
  bool blue = true;

  DOCUMENT(R"(``True`` if the alpha channel should be visible. If enabled with any of RGB, the
texture will be blended to the background color or checkerboard.

If only one channel is selected, it will be rendered in grayscale
)");
  bool alpha = false;

  DOCUMENT("``True`` if the texture should be flipped vertically when rendering.");
  bool flipY = false;

  DOCUMENT("If ``>= 0.0`` the RGBA values will be viewed as HDRM with this as the multiplier.");
  float hdrMultiplier = -1.0f;

  DOCUMENT("``True`` if the texture should be decoded as if it contains YUV data.");
  bool decodeYUV = false;

  DOCUMENT(R"(``True`` if the texture should be interpreted as gamma.

See :ref:`the FAQ entry <gamma-linear-display>`.
)");
  bool linearDisplayAsGamma = true;

  DOCUMENT(R"(The :class:`ResourceId` of a custom shader to use when rendering.

See :meth:`ReplayController.BuildCustomShader` for creating an appropriate custom shader.
)");
  ResourceId customShaderId;

  DOCUMENT("Select the mip of the texture to display.");
  uint32_t mip = 0;

  DOCUMENT("Select the slice or face of the texture to display if it's an array, 3D, or cube tex.");
  uint32_t sliceFace = 0;

  DOCUMENT(R"(Select the sample of the texture to display if it's a multi-sampled texture.

If this is set to :data:`ResolveSamples` then a default resolve will be performed that averages all
samples.
)");
  uint32_t sampleIdx = 0;

  DOCUMENT(R"(``True`` if the rendered image should be as close as possible in value to the input.

This is primarily useful when rendering to a floating point target for retrieving pixel data from
the input texture in cases where it isn't easy to directly fetch the input texture data.
)");
  bool rawOutput = false;

  DOCUMENT("The offset to pan in the X axis.");
  float xOffset = 0.0f;

  DOCUMENT("The offset to pan in the Y axis.");
  float yOffset = 0.0f;

  DOCUMENT(R"(The background color to use behind the texture display.

If set to (0, 0, 0, 0) the global checkerboard colors are used.
)");
  FloatVector backgroundColor;

  DOCUMENT("Selects a :class:`DebugOverlay` to draw over the top of the texture.");
  DebugOverlay overlay = DebugOverlay::NoOverlay;

  static const uint32_t ResolveSamples = ~0U;
};

DECLARE_REFLECTION_STRUCT(TextureDisplay);

// some dependent structs for TextureSave
DOCUMENT("How to map components to normalised ``[0, 255]`` for saving to 8-bit file formats.");
struct TextureComponentMapping
{
  DOCUMENT("");
  TextureComponentMapping() = default;
  TextureComponentMapping(const TextureComponentMapping &) = default;

  DOCUMENT("The value that should be mapped to ``0``");
  float blackPoint = 0.0f;
  DOCUMENT("The value that should be mapped to ``255``");
  float whitePoint = 1.0f;
};

DECLARE_REFLECTION_STRUCT(TextureComponentMapping);

DOCUMENT(R"(How to map multisampled textures for saving to non-multisampled file formats.

.. data:: ResolveSamples

  Value for :data:`sampleIndex` if the samples should be averaged.
)");
struct TextureSampleMapping
{
  DOCUMENT("");
  TextureSampleMapping() = default;
  TextureSampleMapping(const TextureSampleMapping &) = default;

  DOCUMENT(R"(
``True`` if the samples should be mapped to array slices. A multisampled array expands each slice
in-place, so it would be slice 0: sample 0, slice 0: sample 1, slice 1: sample 0, etc.

This then follows the mapping for array slices as with any other array texture. :data:`sampleIndex`
is ignored.
)");
  bool mapToArray = false;

  DOCUMENT(R"(
If :data:`mapToArray` is ``False`` this selects which sample should be extracted to treat as a
normal 2D image. If set to :data:`ResolveSamples` then instead there's a default average resolve.
)");
  uint32_t sampleIndex = ~0U;

  static const uint32_t ResolveSamples = ~0U;
};

DECLARE_REFLECTION_STRUCT(TextureSampleMapping);

DOCUMENT(R"(How to map array textures for saving to non-arrayed file formats.

If :data:`sliceIndex` is -1, :data:`cubeCruciform` == :data:`slicesAsGrid` == ``False`` and the file
format doesn't support saving all slices, only slice 0 is saved.
)");
struct TextureSliceMapping
{
  DOCUMENT("");
  TextureSliceMapping() = default;
  TextureSliceMapping(const TextureSliceMapping &) = default;

  DOCUMENT(R"(
Selects the (depth/array) slice to save.

If this is -1, then all slices are written out as detailed below. This is only supported in formats
that don't support slices natively, and will be done in RGBA8.
)");
  int32_t sliceIndex = -1;

  // write out the slices as a 2D grid, with the below
  // width. Any empty slices are writted as (0,0,0,0)
  DOCUMENT(R"(
If ``True``, write out the slices as a 2D grid with the width given in :data:`sliceGridWidth`. Any
empty slices in the grid are written as transparent black.
)");
  bool slicesAsGrid = false;

  DOCUMENT("The width of a grid if :data:`slicesAsGrid` is ``True``.");
  int32_t sliceGridWidth = 1;

  DOCUMENT(R"(Write out 6 slices in a cruciform pattern::

          +----+
          | +y |
          |    |
     +----+----+----+----+
     | -x | +z | +x | -z |
     |    |    |    |    |
     +----+----+----+----+
          | -y |
          |    |
          +----+

With the gaps filled in with transparent black.
)");
  bool cubeCruciform = false;
};

DECLARE_REFLECTION_STRUCT(TextureSliceMapping);

DOCUMENT("Describes a texture to save and how to map it to the destination file format.");
struct TextureSave
{
  DOCUMENT("");
  TextureSave() = default;
  TextureSave(const TextureSave &) = default;

  DOCUMENT("The :class:`ResourceId` of the texture to save.");
  ResourceId resourceId;

  DOCUMENT("An optional :class:`CompType` hint to use when saving a typeless texture.");
  CompType typeHint = CompType::Typeless;

  DOCUMENT("The :class:`FileType` to use when saving to the destination file.");
  FileType destType = FileType::DDS;

  // mip == -1 writes out all mips where allowed by file format
  // or writes mip 0 otherwise
  DOCUMENT(R"(Selects the mip to be written out.

If set to ``-1`` then all mips are written, where allowed by file format. If not allowed, mip 0 is
written
)");
  int32_t mip = -1;

  DOCUMENT(R"(Controls black/white point mapping for output formats that are normal
:attr:`8-bit SRGB <CompType.UNorm>`, values are
)");
  TextureComponentMapping comp;

  DOCUMENT("Controls mapping for multisampled textures (ignored if texture is not multisampled)");
  TextureSampleMapping sample;

  DOCUMENT("Controls mapping for arrayed textures (ignored if texture is not arrayed)");
  TextureSliceMapping slice;

  DOCUMENT("Selects a single component out of a texture to save as grayscale, or -1 to save all.");
  int channelExtract = -1;

  // for formats without an alpha channel, define how it should be
  // mapped. Only available for uncompressed simple formats, done
  // in RGBA8 space.
  DOCUMENT(R"(Controls handling of alpha channel, mostly relevant for file formats that without
alpha.

It is an :class:`AlphaMapping` that controls what behaviour to use.
)");
  AlphaMapping alpha = AlphaMapping::Preserve;

  DOCUMENT("The background color if :data:`alpha` is set to :attr:`AlphaMapping.BlendToColor`");
  FloatVector alphaCol;

  DOCUMENT("The quality to use when saving to a ``JPG`` file. Valid values are between 1 and 100.");
  int jpegQuality = 90;
};

DECLARE_REFLECTION_STRUCT(TextureSave);

// dependent structs for TargetControlMessage
DOCUMENT("Information about the a new capture created by the target.");
struct NewCaptureData
{
  DOCUMENT("");
  NewCaptureData() = default;
  NewCaptureData(const NewCaptureData &) = default;

  DOCUMENT("An identifier to use to refer to this capture.");
  uint32_t captureId = 0;
  DOCUMENT("The frame number that this capture came from.");
  uint32_t frameNumber = 0;
  DOCUMENT("The time the capture was created, as a unix timestamp in UTC.");
  uint64_t timestamp = 0;
  DOCUMENT("The raw bytes that contain the capture thumbnail, as RGB8 data.");
  bytebuf thumbnail;
  DOCUMENT("The width of the image contained in :data:`thumbnail`.");
  int32_t thumbWidth = 0;
  DOCUMENT("The height of the image contained in :data:`thumbnail`.");
  int32_t thumbHeight = 0;
  DOCUMENT("The local path on the target system where the capture is saved.");
  rdcstr path;
  DOCUMENT(R"(The API used for this capture, if available.

.. note::
  May be empty if running with an older version of RenderDoc
)");
  rdcstr api;
  DOCUMENT("``True`` if the target is running on the local system.");
  bool local = true;
};

DECLARE_REFLECTION_STRUCT(NewCaptureData);

DOCUMENT("Information about the API that the target is using.");
struct APIUseData
{
  DOCUMENT("");
  APIUseData() = default;
  APIUseData(const APIUseData &) = default;

  DOCUMENT("The name of the API.");
  rdcstr name;

  DOCUMENT("``True`` if the API is presenting to a swapchain");
  bool presenting = false;

  DOCUMENT("``True`` if the API can be captured.");
  bool supported = false;
};

DECLARE_REFLECTION_STRUCT(APIUseData);

DOCUMENT("Information about why the target is busy.");
struct BusyData
{
  DOCUMENT("");
  BusyData() = default;
  BusyData(const BusyData &) = default;

  DOCUMENT("The name of the client currently connected to the target.");
  rdcstr clientName;
};

DECLARE_REFLECTION_STRUCT(BusyData);

DOCUMENT("Information about a new child process spawned by the target.");
struct NewChildData
{
  DOCUMENT("");
  NewChildData() = default;
  NewChildData(const NewChildData &) = default;

  DOCUMENT("The PID (Process ID) of the new child.");
  uint32_t processId = 0;
  DOCUMENT("The ident where the new child's target control is active.");
  uint32_t ident = 0;
};

DECLARE_REFLECTION_STRUCT(NewChildData);

DOCUMENT("A message from a target control connection.");
struct TargetControlMessage
{
  DOCUMENT("");
  TargetControlMessage() = default;
  TargetControlMessage(const TargetControlMessage &) = default;

  DOCUMENT("The :class:`type <TargetControlMessageType>` of message received");
  TargetControlMessageType type = TargetControlMessageType::Unknown;

  DOCUMENT("The :class:`new capture data <NewCaptureData>`.");
  NewCaptureData newCapture;
  DOCUMENT("The :class:`API use data <APIUseData>`.");
  APIUseData apiUse;
  DOCUMENT("The :class:`busy signal data <BusyData>`.");
  BusyData busy;
  DOCUMENT("The :class:`new child process data <NewChildData>`.");
  NewChildData newChild;
  DOCUMENT(R"(The progress of an on-going capture.

When valid, will be in the range of 0.0 to 1.0 (0 - 100%). If not valid when a capture isn't going
or has finished, it will be -1.0
)");
  float capProgress = -1.0f;

  DOCUMENT("The number of the capturable windows");
  uint32_t capturableWindowCount = 0;
};

DECLARE_REFLECTION_STRUCT(TargetControlMessage);

DOCUMENT("A modification to a single environment variable.");
struct EnvironmentModification
{
  DOCUMENT("");
  EnvironmentModification() : mod(EnvMod::Set), sep(EnvSep::NoSep), name(""), value("") {}
  EnvironmentModification(const EnvironmentModification &) = default;
  EnvironmentModification(EnvMod m, EnvSep s, const char *n, const char *v)
      : mod(m), sep(s), name(n), value(v)
  {
  }

  bool operator==(const EnvironmentModification &o) const
  {
    return mod == o.mod && sep == o.sep && name == o.name && value == o.value;
  }
  bool operator<(const EnvironmentModification &o) const
  {
    if(!(mod == o.mod))
      return mod < o.mod;
    if(!(sep == o.sep))
      return sep < o.sep;
    if(!(name == o.name))
      return name < o.name;
    if(!(value == o.value))
      return value < o.value;
    return false;
  }
  DOCUMENT("The :class:`modification <EnvMod>` to use.");
  EnvMod mod;
  DOCUMENT("The :class:`separator <EnvSep>` to use if needed.");
  EnvSep sep;
  DOCUMENT("The name of the environment variable.");
  rdcstr name;
  DOCUMENT("The value to use with the modification specified in :data:`mod`.");
  rdcstr value;
};

DECLARE_REFLECTION_STRUCT(EnvironmentModification);

DOCUMENT("The format for a capture file either supported to read from, or export to");
struct CaptureFileFormat
{
  DOCUMENT("");
  CaptureFileFormat() = default;
  CaptureFileFormat(const CaptureFileFormat &) = default;

  bool operator==(const CaptureFileFormat &o) const
  {
    return extension == o.extension && name == o.name && description == o.description &&
           requiresBuffers == o.requiresBuffers && openSupported == o.openSupported &&
           convertSupported == o.convertSupported;
  }
  bool operator<(const CaptureFileFormat &o) const
  {
    if(!(extension == o.extension))
      return extension < o.extension;
    if(!(name == o.name))
      return name < o.name;
    if(!(description == o.description))
      return description < o.description;
    if(!(requiresBuffers == o.requiresBuffers))
      return requiresBuffers < o.requiresBuffers;
    if(!(openSupported == o.openSupported))
      return openSupported < o.openSupported;
    if(!(convertSupported == o.convertSupported))
      return convertSupported < o.convertSupported;
    return false;
  }
  DOCUMENT("The file of the format as a single minimal string, e.g. ``rdc``.");
  rdcstr extension;

  DOCUMENT("A human readable short phrase naming the file format.");
  rdcstr name;

  DOCUMENT("A human readable long-form description of the file format.");
  rdcstr description;

  DOCUMENT(R"(Indicates whether exporting to this format requires buffers or just structured data.
If it doesn't require buffers then it can be exported directly from an opened capture, which by
default has structured data but no buffers available.
)");
  bool requiresBuffers;

  DOCUMENT(R"(Indicates whether or not files in this format can be opened and processed as
structured data.
)");
  bool openSupported;

  DOCUMENT("Indicates whether captures or structured data can be saved out in this format.");
  bool convertSupported;
};

DECLARE_REFLECTION_STRUCT(CaptureFileFormat);
