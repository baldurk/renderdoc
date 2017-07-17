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

#include "data_types.h"
#include "replay_enums.h"

DOCUMENT(R"(
Contains the details of a single element of data (such as position or texture
co-ordinates) within a mesh.)");
struct MeshFormat
{
  MeshFormat()
  {
    idxoffs = 0;
    idxByteWidth = 0;
    baseVertex = 0;
    offset = 0;
    stride = 0;
    compCount = 0;
    compByteWidth = 0;
    compType = CompType::Typeless;
    bgraOrder = false;
    specialFormat = SpecialFormat::Unknown;
    showAlpha = false;
    topo = Topology::Unknown;
    numVerts = 0;
    unproject = false;
    nearPlane = farPlane = 0.0f;
  }

  DOCUMENT("The :class:`ResourceId` of the index buffer that goes with this mesh element.");
  ResourceId idxbuf;
  DOCUMENT("The offset in bytes where the indices start in idxbuf.");
  uint64_t idxoffs;
  DOCUMENT("The width in bytes of each index. Valid values are 1 (depending on API), 2 or 4.");
  uint32_t idxByteWidth;
  DOCUMENT("For indexed meshes, a value added to each index before using it to read the vertex.");
  int32_t baseVertex;

  DOCUMENT("The :class:`ResourceId` of the vertex buffer containing this mesh element.");
  ResourceId buf;
  DOCUMENT("The offset in bytes to the start of the vertex data.");
  uint64_t offset;
  DOCUMENT("The stride in bytes between the start of one vertex and the start of another.");
  uint32_t stride;

  DOCUMENT("The number of components in each vertex.");
  uint32_t compCount;
  DOCUMENT("The width in bytes of each component.");
  uint32_t compByteWidth;
  DOCUMENT("The :class:`type <CompType>` of each component.");
  CompType compType;
  DOCUMENT("``True`` if the components are to be read in ``BGRA`` order.");
  bool32 bgraOrder;
  DOCUMENT(
      "If the component is in a non-uniform format, contains the :class:`SpecialFormat` that "
      "describes it.");
  SpecialFormat specialFormat;

  DOCUMENT(
      "The color to use for rendering the wireframe of this mesh element, as a "
      ":class:`FloatVector`.");
  FloatVector meshColor;

  DOCUMENT("``True`` if the alpha component of this element should be used.");
  bool showAlpha;

  DOCUMENT("The :class:`Topology` that describes the primitives in this mesh.");
  Topology topo;
  DOCUMENT("The number of vertices in the mesh.");
  uint32_t numVerts;

  DOCUMENT("``True`` if this mesh element contains post-projection positional data.");
  bool32 unproject;
  DOCUMENT("The near plane for the projection matrix.");
  float nearPlane;
  DOCUMENT("The far plane for the projection matrix.");
  float farPlane;
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
  DOCUMENT("The :class:`MeshDataStage` where this mesh data comes from.");
  MeshDataStage type;

  DOCUMENT("The :class:`Camera` to use when rendering all of the meshes.");
  ICamera *cam;

  DOCUMENT(
      "``True`` if the projection matrix to use when unprojecting vertex positions is "
      "orthographic.");
  bool32 ortho;
  DOCUMENT("The field of view to use when calculating a perspective projection matrix.");
  float fov;
  DOCUMENT("The aspect ratio to use when calculating a perspective projection matrix.");
  float aspect;

  DOCUMENT(
      "``True`` if all previous instances in the drawcall should be drawn as secondary meshes.");
  bool32 showPrevInstances;
  DOCUMENT("``True`` if all instances in the drawcall should be drawn as secondary meshes.");
  bool32 showAllInstances;
  DOCUMENT(
      "``True`` if all draws in the current pass up to the current draw should be drawn as "
      "secondary meshes.");
  bool32 showWholePass;
  DOCUMENT("The index of the currently selected instance in the drawcall.");
  uint32_t curInstance;

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
  bool32 showBBox;

  DOCUMENT("The :class:`solid shading mode <SolidShade>` to use when rendering the current mesh.");
  SolidShade solidShadeMode;
  DOCUMENT("``True`` if the wireframe of the mesh should be rendered as well as solid shading.");
  bool32 wireframeDraw;

  static const uint32_t NoHighlight = ~0U;
};

DECLARE_REFLECTION_STRUCT(MeshDisplay);

DOCUMENT(R"(
Describes how to render a texture preview of an image. Describes the zoom and pan settings for the
texture when rendering on a particular output, as well as the modification and selection of a
particular subresource (such as array slice, mip or multi-sampled sample).

.. data:: ResolveSamples

  Value for :data:`sampleIdx` if the samples should be averaged.
)");
struct TextureDisplay
{
  DOCUMENT("The :class:`ResourceId` of the texture to display.");
  ResourceId texid;

  DOCUMENT("An optional :class:`CompType` hint to use when displaying a typeless texture.");
  CompType typeHint;

  DOCUMENT("The value in each channel to map to the black point.");
  float rangemin;

  DOCUMENT("The value in each channel to map to the white point.");
  float rangemax;

  DOCUMENT(R"(The scale to apply to the texture when rendering as a floating point value.

``1.0`` corresponds to ``100%``
)");
  float scale;

  DOCUMENT(R"(``True`` if the red channel should be visible.

If only one channel is selected, it will be rendered in grayscale
)");
  bool32 Red;

  DOCUMENT(R"(``True`` if the green channel should be visible.

If only one channel is selected, it will be rendered in grayscale
)");
  bool32 Green;

  DOCUMENT(R"(``True`` if the blue channel should be visible.

If only one channel is selected, it will be rendered in grayscale
)");
  bool32 Blue;

  DOCUMENT(R"(``True`` if the alpha channel should be visible. If enabled with any of RGB, the
texture will be blended to a checkerboard of :data:`lightBackgroundColor` and
:data:`darkBackgroundColor`.

If only one channel is selected, it will be rendered in grayscale
)");
  bool32 Alpha;

  DOCUMENT("``True`` if the texture should be flipped vertically when rendering.");
  bool32 FlipY;

  DOCUMENT("If ``>= 0.0`` the RGBA values will be viewed as HDRM with this as the multiplier.");
  float HDRMul;

  DOCUMENT(R"(``True`` if the texture should be interpreted as gamma.

See :ref:`the FAQ entry <gamma-linear-display>`.
)");
  bool32 linearDisplayAsGamma;

  DOCUMENT(R"(The :class:`ResourceId` of a custom shader to use when rendering.

See :meth:`ReplayController.BuildCustomShader` for creating an appropriate custom shader.
)");
  ResourceId CustomShader;

  DOCUMENT("Select the mip of the texture to display.");
  uint32_t mip;

  DOCUMENT("Select the slice or face of the texture to display if it's an array, 3D, or cube tex.");
  uint32_t sliceFace;

  DOCUMENT(R"(Select the sample of the texture to display if it's a multi-sampled texture.

If this is set to :data:`ResolveSamples` then a default resolve will be performed that averages all
samples.
)");
  uint32_t sampleIdx;

  DOCUMENT(R"(``True`` if the rendered image should be as close as possible in value to the input.

This is primarily useful when rendering to a floating point target for retrieving pixel data from
the input texture in cases where it isn't easy to directly fetch the input texture data.
)");
  bool32 rawoutput;

  DOCUMENT("The offset to pan in the X axis.");
  float offx;

  DOCUMENT("The offset to pan in the Y axis.");
  float offy;

  DOCUMENT("The light background colour to use in the checkerboard behind the texture display.");
  FloatVector lightBackgroundColor;
  DOCUMENT("The dark background colour to use in the checkerboard behind the texture display.");
  FloatVector darkBackgroundColor;

  DOCUMENT("Selects a :class:`DebugOverlay` to draw over the top of the texture.");
  DebugOverlay overlay;

  static const uint32_t ResolveSamples = ~0U;
};

DECLARE_REFLECTION_STRUCT(TextureDisplay);

// some dependent structs for TextureSave
DOCUMENT("How to map components to normalised ``[0, 255]`` for saving to 8-bit file formats.");
struct TextureComponentMapping
{
  DOCUMENT("The value that should be mapped to ``0``");
  float blackPoint;
  DOCUMENT("The value that should be mapped to ``255``");
  float whitePoint;
};

DECLARE_REFLECTION_STRUCT(TextureComponentMapping);

DOCUMENT(R"(How to map multisampled textures for saving to non-multisampled file formats.

.. data:: ResolveSamples

  Value for :data:`sampleIndex` if the samples should be averaged.
)");
struct TextureSampleMapping
{
  DOCUMENT(R"(
``True`` if the samples should be mapped to array slices. A multisampled array expands each slice
in-place, so it would be slice 0: sample 0, slice 0: sample 1, slice 1: sample 0, etc.

This then follows the mapping for array slices as with any other array texture. :data:`sampleIndex`
is ignored.
)");
  bool32 mapToArray;

  DOCUMENT(R"(
If :data:`mapToArray` is ``False`` this selects which sample should be extracted to treat as a
normal 2D image. If set to :data:`ResolveSamples` then instead there's a default average resolve.
)");
  uint32_t sampleIndex;

  static const uint32_t ResolveSamples = ~0U;
};

DECLARE_REFLECTION_STRUCT(TextureSampleMapping);

DOCUMENT(R"(How to map array textures for saving to non-arrayed file formats.

If :data:`sliceIndex` is -1, :data:`cubeCruciform` == :data:`slicesAsGrid` == ``False`` and the file
format doesn't support saving all slices, only slice 0 is saved.
)");
struct TextureSliceMapping
{
  DOCUMENT(R"(
Selects the (depth/array) slice to save.

If this is -1, then all slices are written out as detailed below. This is only supported in formats
that don't support slices natively, and will be done in RGBA8.
)");
  int32_t sliceIndex;

  // write out the slices as a 2D grid, with the below
  // width. Any empty slices are writted as (0,0,0,0)
  DOCUMENT(R"(
If ``True``, write out the slices as a 2D grid with the width given in :data:`sliceGridWidth`. Any
empty slices in the grid are written as transparent black.
)");
  bool32 slicesAsGrid;

  DOCUMENT("The width of a grid if :data:`slicesAsGrid` is ``True``.");
  int32_t sliceGridWidth;

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
  bool32 cubeCruciform;
};

DECLARE_REFLECTION_STRUCT(TextureSliceMapping);

DOCUMENT("Describes a texture to save and how to map it to the destination file format.");
struct TextureSave
{
  DOCUMENT("The :class:`ResourceId` of the texture to save.");
  ResourceId id;

  DOCUMENT("An optional :class:`CompType` hint to use when saving a typeless texture.");
  CompType typeHint;

  DOCUMENT("The :class:`FileType` to use when saving to the destination file.");
  FileType destType;

  // mip == -1 writes out all mips where allowed by file format
  // or writes mip 0 otherwise
  DOCUMENT(R"(Selects the mip to be written out.

If set to ``-1`` then all mips are written, where allowed by file format. If not allowed, mip 0 is
written
)");
  int32_t mip;

  DOCUMENT(R"(Controls black/white point mapping for output formats that are normal
:data:`8-bit SRGB <CompType.UNorm>`, values are
)");
  TextureComponentMapping comp;

  DOCUMENT("Controls mapping for multisampled textures (ignored if texture is not multisampled)");
  TextureSampleMapping sample;

  DOCUMENT("Controls mapping for arrayed textures (ignored if texture is not arrayed)");
  TextureSliceMapping slice;

  DOCUMENT("Selects a single component out of a texture to save as grayscale, or -1 to save all.");
  int channelExtract;

  // for formats without an alpha channel, define how it should be
  // mapped. Only available for uncompressed simple formats, done
  // in RGBA8 space.
  DOCUMENT(R"(Controls handling of alpha channel, mostly relevant for file formats that without
alpha.

It is an :class:`AlphaMapping` that controls what behaviour to use. :data:`alphaCol` and
:data:`alphaColSecondary` may be used depending on the behaviour that it selects.
)");
  AlphaMapping alpha;
  DOCUMENT("The primary color to use in conjunction with :data:`alpha`.");
  FloatVector alphaCol;
  DOCUMENT("The secondary color to use in conjunction with :data:`alpha`.");
  FloatVector alphaColSecondary;

  DOCUMENT("The quality to use when saving to a ``JPG`` file. Valid values are between 1 and 100.");
  int jpegQuality;
};

DECLARE_REFLECTION_STRUCT(TextureSave);

// dependent structs for TargetControlMessage
DOCUMENT("Information about the a new capture created by the target.");
struct NewCaptureData
{
  DOCUMENT("An identifier to use to refer to this capture.");
  uint32_t ID;
  DOCUMENT("The time the capture was created, as a unix timestamp in UTC.");
  uint64_t timestamp;
  DOCUMENT("The raw bytes that contain the capture thumbnail, as RGB8 data.");
  rdctype::array<byte> thumbnail;
  DOCUMENT("The width of the image contained in :data:`thumbnail`.");
  int32_t thumbWidth;
  DOCUMENT("The height of the image contained in :data:`thumbnail`.");
  int32_t thumbHeight;
  DOCUMENT("The local path on the target system where the capture is saved.");
  rdctype::str path;
  DOCUMENT("``True`` if the target is running on the local system.");
  bool32 local;
};

DECLARE_REFLECTION_STRUCT(NewCaptureData);

DOCUMENT("Information about the API that the target has begun using.");
struct RegisterAPIData
{
  DOCUMENT("The name of the new API.");
  rdctype::str APIName;
};

DECLARE_REFLECTION_STRUCT(RegisterAPIData);

DOCUMENT("Information about why the target is busy.");
struct BusyData
{
  DOCUMENT("The name of the client currently connected to the target.");
  rdctype::str ClientName;
};

DECLARE_REFLECTION_STRUCT(BusyData);

DOCUMENT("Information about a new child process spawned by the target.");
struct NewChildData
{
  DOCUMENT("The PID (Process ID) of the new child.");
  uint32_t PID;
  DOCUMENT("The ident where the new child's target control is active.");
  uint32_t ident;
};

DECLARE_REFLECTION_STRUCT(NewChildData);

DOCUMENT("A message from a target control connection.");
struct TargetControlMessage
{
  TargetControlMessage() {}
  DOCUMENT("The :class:`type <TargetControlMessageType>` of message received");
  TargetControlMessageType Type;

  DOCUMENT("The :class:`new capture data <NewCaptureData>`.");
  NewCaptureData NewCapture;
  DOCUMENT("The :class:`API registration data <RegisterAPIData>`.");
  RegisterAPIData RegisterAPI;
  DOCUMENT("The :class:`busy signal data <BusyData>`.");
  BusyData Busy;
  DOCUMENT("The :class:`new child process data <NewChild>`.");
  NewChildData NewChild;
};

DECLARE_REFLECTION_STRUCT(TargetControlMessage);

DOCUMENT("A modification to a single environment variable.");
struct EnvironmentModification
{
  EnvironmentModification() : mod(EnvMod::Set), sep(EnvSep::NoSep), name(""), value("") {}
  EnvironmentModification(EnvMod m, EnvSep s, const char *n, const char *v)
      : mod(m), sep(s), name(n), value(v)
  {
  }
  DOCUMENT("The :class:`modification <EnvMod>` to use.");
  EnvMod mod;
  DOCUMENT("The :class:`separator <EnvSep>` to use if needed.");
  EnvSep sep;
  DOCUMENT("The name of the environment variable.");
  rdctype::str name;
  DOCUMENT("The value to use with the modification specified in :data:`mod`.");
  rdctype::str value;
};

DECLARE_REFLECTION_STRUCT(EnvironmentModification);
