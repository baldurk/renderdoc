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

DOCUMENT(R"(The types of several pre-defined and known sections. This allows consumers of the API
to recognise and understand the contents of the section.

Note that sections above the highest value here may be encountered if they were written in a new
version of RenderDoc that addes a new section type. They should be considered equal to
:data:`Unknown` by any processing.

.. data:: Unknown

  An unknown section - any custom or non-predefined section will have this type.

.. data:: FrameCapture

  This section contains the actual captured frame, in RenderDoc's internal chunked representation.
  The contents can be fetched as structured data with or without replaying the frame.

  The name for this section will be "renderdoc/internal/framecapture".

.. data:: ResolveDatabase

  This section contains platform-specific data used to resolve callstacks.

  The name for this section will be "renderdoc/internal/resolvedb".

.. data:: Bookmarks

  This section contains a JSON document with bookmarks added to the capture to highlight important
  events.

  The name for this section will be "renderdoc/ui/bookmarks".

.. data:: Notes

  This section contains a JSON document with free-form information added for human consumption, e.g.
  details about how the capture was obtained with repro steps in the original program, or with
  driver and machine info.

  The name for this section will be "renderdoc/ui/notes".

.. data:: ResourceRenames

  This section contains a JSON document with custom names applied to resources in the UI, over and
  above any friendly names specified in the capture itself.

  The name for this section will be "renderdoc/ui/resrenames".

.. data:: AMDRGPProfile

  This section contains a .rgp profile from AMD's RGP tool, which can be extracted and loaded.

  The name for this section will be "amd/rgp/profile".

.. data:: ExtendedThumbnail

  This section contains a thumbnail in format other than JPEG. For example, when it needs to be
  lossless.

  The name for this section will be "renderdoc/internal/exthumb".
)");
enum class SectionType : uint32_t
{
  Unknown = 0,
  First = Unknown,
  FrameCapture,
  ResolveDatabase,
  Bookmarks,
  Notes,
  ResourceRenames,
  AMDRGPProfile,
  ExtendedThumbnail,
  Count,
};

ITERABLE_OPERATORS(SectionType);
DECLARE_REFLECTION_ENUM(SectionType);

// replay_shader.h

DOCUMENT(R"(Represents the type of register a local variable maps to.

.. data:: Undefined

  No defined register.

.. data:: Input

  An input register.

.. data:: Temporary

  A normal temporary register.

.. data:: IndexedTemporary

  An indexed temporary register.

.. data:: Output

  An output register.
)");
enum class RegisterType : uint32_t
{
  Undefined,
  Input,
  Temporary,
  IndexedTemporary,
  Output,
};

DECLARE_REFLECTION_ENUM(RegisterType);

DOCUMENT(R"(Represents the base type of a shader variable in debugging or constant blocks.

.. data:: Float

  A single-precision (32-bit) floating point value.

.. data:: Double

  A double-precision (64-bit) floating point value.

.. data:: Half

  A half-precision (16-bit) floating point value.

.. data:: SInt

  A signed 32-bit integer value.

.. data:: UInt

  An unsigned 32-bit integer value.

.. data:: SShort

  A signed 16-bit integer value.

.. data:: UShort

  An unsigned 16-bit integer value.

.. data:: SLong

  A signed 64-bit integer value.

.. data:: ULong

  An unsigned 64-bit integer value.

.. data:: SByte

  A signed 8-bit integer value.

.. data:: UByte

  An unsigned 8-bit integer value.

.. data:: Unknown

  An unknown type.
)");
enum class VarType : uint32_t
{
  Float = 0,
  Double,
  Half,
  SInt,
  UInt,
  SShort,
  UShort,
  SLong,
  ULong,
  SByte,
  UByte,
  Unknown = ~0U,
};

DECLARE_REFLECTION_ENUM(VarType);

DOCUMENT(R"(Represents the component type of a channel in a texture or element in a structure.

.. data:: Typeless

  A component that has no concrete type.

.. data:: Float

  A single-precision (32-bit) floating point value. This is an IEEE float with 1 sign bit,
  8 bits of exponent and 23 bits of mantissa.

.. data:: UNorm

  An unsigned normalised floating point value. This is converted by dividing the input value by
  the maximum representable unsigned integer value, to produce a value in the range ``[0, 1]``

.. data:: SNorm

  A signed normalised floating point value in range. This is converted by dividing the input value
  by the maximum representable *positive signed* integer value, to produce a value in the range
  ``[-1, 1]``. As a special case, the maximum negative signed integer is also mapped to ``-1`` so
  there are two representations of -1. This means there is only one ``0`` value and that there is
  the same range of available values for positive and negative values.

  For example, signed 16-bit integers range from ``-32768`` to ``+32767``. ``-32768`` is mapped to
  ``-1``, and then any other value is divided by ``32767`` giving an equal set of values in the
  range ``[-1, 0]`` as in the range ``[0, 1]``.

.. data:: UInt

  An unsigned integer value.

.. data:: SInt

  A signed integer value.

.. data:: UScaled

  An unsigned scaled floating point value. This is converted from the input unsigned integer without
  any normalisation as with :data:`UNorm`, so the resulting values range from ``0`` to the maximum
  unsigned integer value ``2^N - 1``.

.. data:: SScaled

  A signed scaled floating point value. This is converted from the input unsigned integer without
  any normalisation as with :data:`SNorm`, so the resulting values range from the minimum signed
  integer value ``-2^(N-1)`` to the maximum signed integer value ``2^(N-1) - 1``.

.. data:: Depth

  An opaque value storing depth information, either :data:`unsigned normalised <UNorm>` or
  :data:`floating point <float>`.

.. data:: Double

  A double-precision (64-bit) floating point value.

.. data:: UNormSRGB

  Similar to :data:`UNorm` normalised between the minimum and maximum unsigned values to ``0.0`` -
  ``1.0``, but with an sRGB gamma curve applied.
)");
enum class CompType : uint8_t
{
  Typeless = 0,
  Float,
  UNorm,
  SNorm,
  UInt,
  SInt,
  UScaled,
  SScaled,
  Depth,
  Double,
  UNormSRGB,
};

DECLARE_REFLECTION_ENUM(CompType);

DOCUMENT(R"(A single source component for a destination texture swizzle.

.. data:: Red

  The Red component.

.. data:: Green

  The Green component.

.. data:: Blue

  The Blue component.

.. data:: Alpha

  The Alpha component.

.. data:: Zero

  The fixed value ``0``.

.. data:: One

  The fixed value ``1``.
)");
enum class TextureSwizzle : uint32_t
{
  Red,
  Green,
  Blue,
  Alpha,
  Zero,
  One,
};

DECLARE_REFLECTION_ENUM(TextureSwizzle);

DOCUMENT(R"(A texture addressing mode in a single direction (U,V or W).

.. data:: Wrap

  The texture is tiled at every multiple of 1.0.

.. data:: Repeat

  Alias of :data:`Wrap`.

.. data:: Mirror

  The texture is tiled as with :data:`Wrap`, but with the absolute value of the texture co-ordinate.

.. data:: MirrorRepeat

  Alias of :data:`Mirror`.

.. data:: MirrorOnce

  The texture is mirrored with :data:`Mirror`, but the texture does not tile as with
  :data:`ClampEdge`.

.. data:: MirrorClamp

  Alias of :data:`MirrorOnce`.

.. data:: ClampEdge

  The texture is clamped to the range of ``[0.0, 1.0]`` and the texture value at each end used.

.. data:: ClampBorder

  The texture is clamped such that texture co-ordinates outside the range of ``[0.0, 1.0]`` are set
  to the border color specified in the sampler.
)");
enum class AddressMode : uint32_t
{
  Wrap,
  Repeat = Wrap,
  Mirror,
  MirrorRepeat = Mirror,
  MirrorOnce,
  MirrorClamp = MirrorOnce,
  ClampEdge,
  ClampBorder,
};

DECLARE_REFLECTION_ENUM(AddressMode);

enum YcbcrConversion
{
  Raw,
  RangeOnly,
  BT709,
  BT601,
  BT2020,
};

DECLARE_REFLECTION_ENUM(YcbcrConversion);

enum YcbcrRange
{
  ITUFull,
  ITUNarrow,
};

DECLARE_REFLECTION_ENUM(YcbcrRange);

enum ChromaSampleLocation
{
  CositedEven,
  Midpoint,
};

DECLARE_REFLECTION_ENUM(ChromaSampleLocation);

DOCUMENT(R"(The type of a resource referred to by binding or API usage.

In some cases there is a little overlap or fudging when mapping API concepts - this is primarily
just intended for e.g. fuzzy user filtering or rough categorisation. Precise mapping would require
API-specific concepts.

.. data:: Unknown

  An unknown type of resource.

.. data:: Device

  A system-level object, typically unique.

.. data:: Queue

  A queue representing the ability to execute commands in a single stream, possibly in parallel to
  other queues.

.. data:: CommandBuffer

  A recorded set of commands that can then be subsequently executed.

.. data:: Texture

  A texture - one- to three- dimensional, possibly with array layers and mip levels. See
  :class:`TextureDescription`.

.. data:: Buffer

  A linear (possibly typed) view of memory. See :class:`BufferDescription`.

.. data:: View

  A particular view into a texture or buffer, e.g. either accessing the underlying resource through
  a different type, or only a subset of the resource.

.. data:: Sampler

  The information regarding how a texture is accessed including wrapping, minification/magnification
  and other information. The precise details are API-specific and listed in the API state when
  bound.

.. data:: SwapchainImage

  A special class of :data:`Texture` that is owned by the swapchain and is used for presentation.

.. data:: Memory

  An object corresponding to an actual memory allocation, which other resources can then be bound
  to.

.. data:: Shader

  A single shader object for any shader stage. May be bound directly, or used to compose into a
  :data:`PipelineState` depending on the API.

.. data:: ShaderBinding

  An object that determines some manner of shader binding. Since this varies significantly by API,
  different concepts used for shader resource binding fall under this type.

.. data:: PipelineState

  A single object containing all information regarding the current GPU pipeline, containing both
  shader objects, potentially some shader binding information, and fixed-function state.

.. data:: StateObject

  A single object encapsulating some amount of related state that can be set together, instead of
  setting each individual state separately.

.. data:: RenderPass

  An object related to collecting render pass information together. This may not be an actual
  explicit render pass object if it doesn't exist in the API, it may also be a collection of
  textures in a framebuffer that are bound together to the API for rendering.

.. data:: Query

  A query for retrieving some kind of feedback from the GPU, either as a fixed number or a boolean
  value which can be used in predicated rendering.

.. data:: Sync

  A synchronisation object used for either synchronisation between GPU and CPU, or GPU-to-GPU work.

.. data:: Pool

  An object which pools together other objects in an opaque way, either for runtime allocation and
  deallocation, or for caching purposes.
)");
enum class ResourceType : uint32_t
{
  Unknown,

  Device,
  Queue,
  CommandBuffer,

  Texture,
  Buffer,
  View,
  Sampler,
  SwapchainImage,
  Memory,

  Shader,
  ShaderBinding,
  PipelineState,

  StateObject,
  RenderPass,

  Query,
  Sync,
  Pool,
};

DECLARE_REFLECTION_ENUM(ResourceType);

DOCUMENT(R"(The dimensionality of a texture binding.

.. data:: Unknown

  An unknown type of texture.

.. data:: Buffer

  A texel buffer.

.. data:: Texture1D

  A 1D texture.

.. data:: Texture1DArray

  A 1D texture array.

.. data:: Texture2D

  A 2D texture.

.. data:: TextureRect

  A rectangle texture, a legacy format for non-power of two textures.

.. data:: Texture2DArray

  A 2D texture array.

.. data:: Texture2DMS

  A multi-sampled 2D texture.

.. data:: Texture2DMSArray

  A multi-sampled 2D texture array.

.. data:: Texture3D

  A 3D texture.

.. data:: TextureCube

  A Cubemap texture.

.. data:: TextureCubeArray

  A Cubemap texture array.
)");
enum class TextureType : uint32_t
{
  Unknown,
  First = Unknown,
  Buffer,
  Texture1D,
  Texture1DArray,
  Texture2D,
  TextureRect,
  Texture2DArray,
  Texture2DMS,
  Texture2DMSArray,
  Texture3D,
  TextureCube,
  TextureCubeArray,
  Count,
};

ITERABLE_OPERATORS(TextureType);
DECLARE_REFLECTION_ENUM(TextureType);

DOCUMENT(R"(The type of a shader resource bind.

.. data:: Unknown

  An unknown type of binding.

.. data:: ConstantBuffer

  A constant or uniform buffer.

.. data:: Sampler

  A separate sampler object.

.. data:: ImageSampler

  A combined image and sampler object.

.. data:: ReadOnlyImage

  An image that can only be sampled from.

.. data:: ReadWriteImage

  An image that can be read from and written to arbitrarily.

.. data:: ReadOnlyTBuffer

  A texture buffer that can only be read from.

.. data:: ReadWriteTBuffer

  A texture buffer that can be read from and written to arbitrarily.

.. data:: ReadOnlyBuffer

  A buffer that can only be read from, distinct from :data:`ConstantBuffer`.

.. data:: ReadWriteBuffer

  A buffer that can be read from and written to arbitrarily.

.. data:: InputAttachment

  An input attachment for reading from the target currently being written.
)");
enum class BindType : uint32_t
{
  Unknown = 0,
  ConstantBuffer,
  Sampler,
  ImageSampler,
  ReadOnlyImage,
  ReadWriteImage,
  ReadOnlyTBuffer,
  ReadWriteTBuffer,
  ReadOnlyBuffer,
  ReadWriteBuffer,
  InputAttachment,
};

DECLARE_REFLECTION_ENUM(BindType);

DOCUMENT2(R"(Annotates a particular built-in input or output from a shader with a special meaning to
the hardware or API.

Some of the built-in inputs or outputs can be declared multiple times in arrays or otherwise indexed
to apply to multiple related things - see :data:`ClipDistance`, :data:`CullDistance` and
:data:`ColorOutput`.

.. data:: Undefined

  Undefined built-in or no built-in is attached to this shader variable.

.. data:: Position

  As an output from the final vertex processing shader stage, this feeds the vertex position to the
  rasterized. As an input to the pixel shader stage this receives the position from the rasterizer.

.. data:: PointSize

  An output that controls the size of point primitives.

.. data:: ClipDistance

  An output for the distance to a user-defined clipping plane. Any pixel with an interpolated value
  that is negative will not be rasterized. Typically there can be more than one such output.

.. data:: CullDistance

  An output for the distance to a user-defined culling plane. Any primitive with all vertices having
  negative values will not be rasterized. Typically there can be more than one such output.

.. data:: RTIndex

  An output for selecting the render target index in an array to render to. Available in geometry
  shaders and possibly earlier stages depending on hardware/API capability.

.. data:: ViewportIndex

  An output for selecting the viewport index to render to. Available in geometry shaders and
  possibly earlier stages depending on hardware/API capability.

.. data:: VertexIndex

  An input to the vertex shader listing the vertex index. The exact meaning of this index can vary
  by API but generally it refers to either a 0-based counter for non-indexed draws, or the index
  value for indexed draws. It may or may not be affected by offsets, depending on API semantics.

.. data:: PrimitiveIndex

  A built-in indicating which primitive is being processed. This can be read by all primitive stages
  after the vertex shader, and written by the geometry shader.

.. data:: InstanceIndex

  This built-in is defined similar to :data:`VertexIndex` but for instances within an instanced
  drawcall. It counts from 0 and as with :data:`VertexIndex` it may or may not be affected by
  drawcall offsets.

.. data:: DispatchSize

  An input in compute shaders that gives the number of workgroups executed by the dispatch call.

.. data:: DispatchThreadIndex

  An input in compute shaders giving a 3D shared index across all workgroups, such that the index
  varies across each thread in the workgroup up to its size, then the indices for workgroup
  ``(0,0,1)`` begin adjacent to where workgroup ``(0,0,0)`` ended.

  This is related to :data:`GroupThreadIndex` and :data:`GroupIndex`.

.. data:: GroupIndex

  An input in compute shaders giving a 3D index of this current workgroup amongst all workgroups,
  up to the dispatch size.

  The index is constant across all threads in the workgroup.

  This is related to :data:`GroupThreadIndex` and :data:`DispatchThreadIndex`.

.. data:: GroupFlatIndex

  An input in compute shaders giving a flat 1D index of the thread within the current workgroup.
  This index increments first in the ``X`` dimension, then in the ``Y`` dimension, then in the ``Z``
  dimension.

.. data:: GroupThreadIndex

  An input in compute shaders giving a 3D index of this thread within its workgroup, up to the
  workgroup size.

  The input does not vary between one thread in a workgroup and the same thread in another
  workgroup.

  This is related to :data:`GroupIndex` and :data:`DispatchThreadIndex`.

.. data:: GSInstanceIndex

  An input to the geometry shader giving the instance being run, if the geometry shader was setup to
  be invoked multiple times for each input primitive.

.. data:: OutputControlPointIndex

  An input to the tessellation control or hull shader giving the output control point index or patch
  vertex being operated on.

.. data:: DomainLocation

  An input to the tessellation evaluation or domain shader, giving the normalised location on the
  output patch where evaluation is occuring. E.g. for triangle output this is the barycentric
  co-ordinates of the output vertex.

.. data:: IsFrontFace

  An input to the pixel shader indicating whether or not the contributing triangle was considered
  front-facing or not according to the API setup for winding order and backface orientation.

.. data:: MSAACoverage

  An input or an output from the pixel shader. As an input, it specifies a bitmask of which samples
  in a pixel were covered by the rasterizer. As an output, it specifies which samples in the
  destination target should be updated.

.. data:: MSAASamplePosition

  An input to the pixel shader that contains the location of the current sample relative to the
  pixel, when running the pixel shader at sample frequency.

)",
          R"(
.. data:: MSAASampleIndex

  An input to the pixel shader that indicates which sample in the range ``0 .. N-1`` is currently
  being processed.

.. data:: PatchNumVertices

  An input to the tessellation stages, this gives the number of vertices in each patch.

.. data:: OuterTessFactor

  An output from the tessellation control or hull shader, this determines the level to which the
  outer edge of each primitive is tessellated by the fixed-function tessellator.

  It is also available for reading in the tessellation evaluation or domain shader.

.. data:: InsideTessFactor

  Related to :data:`OuterTessFactor` this functions in the same way to determine the tessellation
  level inside the primitive.

.. data:: ColorOutput

  An output from the pixel shader, this determines the color value written to the corresponding
  target. There will be as many color output built-ins as there are targets bound.

.. data:: DepthOutput

  An output from the pixel shader, writes the depth of this pixel with no restrictions.

  Related to :data:`DepthOutputGreaterEqual` and :data:`DepthOutputLessEqual`.

.. data:: DepthOutputGreaterEqual

  An output from the pixel shader, writes the depth of this pixel with the restriction that it will
  be greater than or equal to the original depth produced by the rasterizer.

  Related to :data:`DepthOutput` and :data:`DepthOutputLessEqual`.

.. data:: DepthOutputLessEqual

  An output from the pixel shader, writes the depth of this pixel with the restriction that it will
  be less than or equal to the original depth produced by the rasterizer.

  Related to :data:`DepthOutputGreaterEqual` and :data:`DepthOutput`.

.. data:: BaseVertex

  The first vertex processed in this draw, as specified by the ``firstVertex`` / ``baseVertex``
  parameter to the draw call.

.. data:: BaseInstance

  The first instance processed in this draw call, as specified by the ``firstInstance`` parameter.

.. data:: DrawIndex

  For indirect or multi-draw commands, the index of this draw call within the overall draw command.

.. data:: StencilReference

  The stencil reference to be used for stenciling operations on this fragment.

)");
enum class ShaderBuiltin : uint32_t
{
  Undefined = 0,
  First = Undefined,
  Position,
  PointSize,
  ClipDistance,
  CullDistance,
  RTIndex,
  ViewportIndex,
  VertexIndex,
  PrimitiveIndex,
  InstanceIndex,
  DispatchSize,
  DispatchThreadIndex,
  GroupIndex,
  GroupFlatIndex,
  GroupThreadIndex,
  GSInstanceIndex,
  OutputControlPointIndex,
  DomainLocation,
  IsFrontFace,
  MSAACoverage,
  MSAASamplePosition,
  MSAASampleIndex,
  PatchNumVertices,
  OuterTessFactor,
  InsideTessFactor,
  ColorOutput,
  DepthOutput,
  DepthOutputGreaterEqual,
  DepthOutputLessEqual,
  BaseVertex,
  BaseInstance,
  DrawIndex,
  StencilReference,
  Count,
};

ITERABLE_OPERATORS(ShaderBuiltin);
DECLARE_REFLECTION_ENUM(ShaderBuiltin);

// replay_render.h

DOCUMENT(R"(The type of :class:`ReplayOutput` to create

.. data:: Headless

  A headless output that does nothing to display to windows but can still be controlled and
  queried the same way

.. data:: Texture

  An output that is used for displaying textures, thumbnails and pixel context

.. data:: Mesh

  An output that will display mesh data previews
)");
enum class ReplayOutputType : uint32_t
{
  Headless = 0,
  Texture,
  Mesh,
};

DECLARE_REFLECTION_ENUM(ReplayOutputType);

DOCUMENT(R"(Describes a particular stage in the geometry transformation pipeline.

.. data:: Unknown

  Unknown or invalid stage.

.. data:: VSIn

  The inputs to the vertex shader described by the explicit API vertex input bindings.

.. data:: VSOut

  The outputs from the vertex shader corresponding one-to-one to the input elements.

.. data:: GSOut

  The final output from the last stage in the pipeline, be that tessellation or geometry shader.

  This has possibly been expanded/multiplied from the inputs
)");
enum class MeshDataStage : uint32_t
{
  Unknown = 0,
  VSIn,
  VSOut,
  GSOut,
};

DECLARE_REFLECTION_ENUM(MeshDataStage);

DOCUMENT(R"(The type of overlay image to render on top of an existing texture view, for debugging
purposes.

In overlays that refer to the 'current pass', for any API that does not have an explicit notion of a
render pass, it is defined as all previous drawcalls that render to the same set of render targets.
Note that this is defined independently from any marker regions.

See :ref:`the documentation for this feature <render-overlay>`.

.. data:: NoOverlay

  No overlay should be rendered.

.. data:: Drawcall

  An overlay highlighting the area rasterized by the drawcall on screen, no matter what tests or
  processes may be discarding the pixels actually rendered.

  The rest of the image should be dimmed slightly to make the draw on screen clearer.

.. data:: Wireframe

  Similar to the :data:`Drawcall` overlay, this should render over the top of the image, but showing
  the wireframe of the object instead of a solid render.

.. data:: Depth

  This overlay shows pixels from the object that passed all depth tests in green, and pixels that
  failed any depth test in red.

  If some pixel is overwritten more than once by the object, if any of the samples passed the result
  will be green (i.e. the failure overlay is conservative).

.. data:: Stencil

  This overlay shows pixels from the object that passed all stencil tests in green, and pixels that
  failed any stencil test in red.

  If some pixel is overwritten more than once by the object, if any of the samples passed the result
  will be green (i.e. the failure overlay is conservative).

.. data:: BackfaceCull

  This overlay shows pixels from the object that passed backface culling in green, and pixels that
  were backface culled in red.

  If some pixel is overwritten more than once by the object, if any of the samples passed the result
  will be green (i.e. the failure overlay is conservative).

.. data:: ViewportScissor

  This overlay shows a rectangle on screen corresponding to both the current viewport, and if
  enabled the current scissor as well.

.. data:: NaN

  This overlay renders the image in greyscale using a simple luminosity calculation, then highlights
  any pixels that are ``NaN`` in red, any that are positive or negative infinity in green, and any
  that are negative in blue.

.. data:: Clipping

  This overlay renders the image in greyscale using a simple luminosity calculation, then highlights
  any pixels that are currently above the white point in green and any pixels that are below the
  black point in red.

  This is relative to the current black and white points used to display the texture.

.. data:: ClearBeforePass

  This overlay clears the bound render targets before the current pass, allowing you to see only the
  contribution from the current pass.

  Note only color targets are cleared, depth-stencil targets are unchanged so any depth or stencil
  tests will still pass or fail in the same way.

.. data:: ClearBeforeDraw

  This is the same as the :data:`ClearBeforePass` overlay, except it clears before the current
  drawcall, not the current pass.

.. data:: QuadOverdrawPass

  This overlay shows pixel overdraw using 2x2 rasterized quad granularity instead of single-pixel
  overdraw. This represents the number of times the pixel shader was invoked along triangle edges
  even if each pixel is only overdrawn once.

  The overlay accounts for all draws in the current pass.

.. data:: QuadOverdrawDraw

  This is the same as the :data:`QuadOverdrawPass` overlay, except it only shows the overdraw for
  the current drawcall, not the current pass.

.. data:: TriangleSizePass

  This overlay shows the size of each triangle, starting from triangles with area ``16 (4x4)`` and above
  at the lower end to triangles with area ``0.125 (1/8th pixel)`` at the upper end.

  The overlay accounts for all draws in the current pass.

.. data:: TriangleSizeDraw

  This is similar to the :data:`TriangleSizePass` overlay, except it only shows the triangle size
  for the current drawcall, not the current pass.

)");
enum class DebugOverlay : uint32_t
{
  NoOverlay = 0,
  Drawcall,
  Wireframe,
  Depth,
  Stencil,
  BackfaceCull,
  ViewportScissor,
  NaN,
  Clipping,
  ClearBeforePass,
  ClearBeforeDraw,
  QuadOverdrawPass,
  QuadOverdrawDraw,
  TriangleSizePass,
  TriangleSizeDraw,
};

DECLARE_REFLECTION_ENUM(DebugOverlay);

DOCUMENT(R"(The format of an image file

.. data:: DDS

  A DDS file

.. data:: PNG

  A PNG file

.. data:: JPG

  A JPG file

.. data:: BMP

  A BMP file

.. data:: TGA

  A TGA file

.. data:: HDR

  An HDR file

.. data:: EXR

  An EXR file

.. data:: RAW

  Raw data, just the bytes of the image tightly packed with no metadata or compression/encoding
)");
enum class FileType : uint32_t
{
  DDS,
  First = DDS,
  PNG,
  JPG,
  BMP,
  TGA,
  HDR,
  EXR,
  Raw,
  Count,
};

ITERABLE_OPERATORS(FileType);
DECLARE_REFLECTION_ENUM(FileType);

DOCUMENT(R"(What to do with the alpha channel from a texture while saving out to a file.

.. data:: Discard

  Completely discard the alpha channel and only write RGB t othe file.

.. data:: BlendToColor

  Blend to the primary background color using alpha.

.. data:: BlendToCheckerboard

  Blend to a checkerboard pattern with the primary and secondary background colors.

.. data:: Preserve

  Preserve the alpha channel and save it to the file by itself.

  This is only valid for file formats that support alpha channels.
)");
enum class AlphaMapping : uint32_t
{
  Discard,
  First = Discard,
  BlendToColor,
  BlendToCheckerboard,
  Preserve,
  Count,
};

ITERABLE_OPERATORS(AlphaMapping);
DECLARE_REFLECTION_ENUM(AlphaMapping);

DOCUMENT(R"(A resource format's particular type. This accounts for either block-compressed textures
or formats that don't have equal byte-multiple sizes for each channel.

.. data:: Regular

  This format has no special layout, so its format is described by a number of components, a
  :class:`CompType` and a byte width per component.

.. data:: Undefined

  This format is undefined or unknown, or does not map to any known regular format.

.. data:: BC1

  A block-compressed texture in ``BC1`` format (RGB with 1-bit alpha, 0.5 bytes per pixel)

  Formerly known as ``DXT1``, commonly used for color maps.

.. data:: BC2

  A block-compressed texture in ``BC2`` format (RGB with 4-bit alpha, 1 byte per pixel)

  Formerly known as ``DXT3``, rarely used.

.. data:: BC3

  A block-compressed texture in ``BC3`` format (RGBA, 1 byte per pixel)

  Formerly known as ``DXT5``, commonly used for color + alpha maps, or color with attached
  single channel data.

.. data:: BC4

  A block-compressed texture in ``BC4`` format (Single channel, 0.5 bytes per pixel)

  Commonly used for single component data such as gloss or height data.

.. data:: BC5

  A block-compressed texture in ``BC5`` format (Two channels, 1 byte per pixel)

  Commonly used for normal maps.

.. data:: BC6

  A block-compressed texture in ``BC6`` format (RGB floating point, 1 byte per pixel)

  Commonly used for HDR data of all kinds.

.. data:: BC7

  A block-compressed texture in ``BC7`` format (RGB or RGBA, 1 byte per pixel)

  Commonly used for high quality color maps, with or without alpha.

.. data:: ETC2

  A block-compressed texture in ``ETC2`` format (RGB with 1-bit alpha, 0.5 bytes per pixel)

  Commonly used on mobile or embedded platforms.

.. data:: EAC

  A block-compressed texture in ``EAC`` format (Single channel 11-bit or RGBA, 0.5 bytes per pixel)

  Commonly used on mobile or embedded platforms.

.. data:: ASTC

  A block-compressed texture in ``ASTC`` format (Representation varies a lot)

  The ASTC format encodes each block as 16 bytes, but the block size can vary from 4x4 (so 1 byte
  per pixel) up to 12x12 (0.11 bytes per pixel).

  Each block can encode between one and three channels of data, either correlated or uncorrelated,
  in low or high dynamic range.

  Commonly used on mobile or embedded platforms.

.. data:: R10G10B10A2

  Each pixel is stored in 32 bits. Red, green and blue are stored in 10-bits each and alpha in 2
  bits. The data can either be :data:`unsigned normalised <CompType.UNorm>` or
  :data:`unsigned integer <CompType.UInt>`.

.. data:: R11G11B10

  Each pixel is stored in 32 bits. Red and green are stored as an 11-bit float with no sign bit,
  5-bit exponent and 6-bit mantissa. Blue is stored with 5-bit exponent and 5-bit mantissa.

.. data:: R5G6B5

  Each pixel is stored in 16 bits. Red and blue are stored as 5 bits, and green is stored as six.
  The data is :data:`unsigned normalised <CompType.UNorm>`.

.. data:: R5G5B5A1

  Each pixel is stored in 16 bits. Red, green, and blue are stored as 5 bits, with 1-bit alpha.
  The data is :data:`unsigned normalised <CompType.UNorm>`.

.. data:: R9G9B9E5

  Each pixel is stored in 32 bits. Red, green, and blue are stored with individual 9-bit mantissas
  and a shared 5-bit exponent. There are no sign bits.

.. data:: R4G4B4A4

  Each pixel is stored in 16 bits. Red, green, blue, and alpha are stored as 4-bit
  :data:`unsigned normalised <CompType.UNorm>` values.

.. data:: R4G4

  Each pixel is stored in 8 bits. Red and green are stored as 4-bit
  :data:`unsigned normalised <CompType.UNorm>` values.

.. data:: D16S8

  Each pixel is considered a packed depth-stencil value with 16 bit normalised depth and 8 bit
  stencil.

.. data:: D24S8

  Each pixel is considered a packed depth-stencil value with 24 bit normalised depth and 8 bit
  stencil.

.. data:: D32S8

  Each pixel is considered a packed depth-stencil value with 32 bit floating point depth and 8 bit
  stencil.

.. data:: S8

  Each pixel is an 8 bit stencil value.

.. data:: YUV8

  The pixel data is 8-bit in YUV subsampled format. More information about subsampling setup is
  stored separately

.. data:: YUV10

  The pixel data is 10-bit in YUV subsampled format. More information about subsampling setup is
  stored separately

.. data:: YUV12

  The pixel data is 12-bit in YUV subsampled format. More information about subsampling setup is
  stored separately

.. data:: YUV16

  The pixel data is 16-bit in YUV subsampled format. More information about subsampling setup is
  stored separately

.. data:: PVRTC

  PowerVR properitary texture compression format.
)");
enum class ResourceFormatType : uint8_t
{
  Regular = 0,
  Undefined,
  BC1,
  BC2,
  BC3,
  BC4,
  BC5,
  BC6,
  BC7,
  ETC2,
  EAC,
  ASTC,
  R10G10B10A2,
  R11G11B10,
  R5G6B5,
  R5G5B5A1,
  R9G9B9E5,
  R4G4B4A4,
  R4G4,
  D16S8,
  D24S8,
  D32S8,
  S8,
  YUV8,
  YUV10,
  YUV12,
  YUV16,
  PVRTC,
};

DECLARE_REFLECTION_ENUM(ResourceFormatType);

DOCUMENT(R"(An API specific hint for a certain behaviour. A legacy concept in OpenGL that controls
hints to the implementation where there is room for interpretation within the range of valid
behaviour.

.. data:: DontCare

  The hinted behaviour can follow any valid path as the implementation decides.

.. data:: Nicest

  The hinted behaviour should follow the most correct or highest quality path.

.. data:: Fastest

  The hinted behaviour should follow the most efficient path.
)");
enum class QualityHint : uint32_t
{
  DontCare,
  Nicest,
  Fastest,
};

DECLARE_REFLECTION_ENUM(QualityHint);

DOCUMENT(R"(Identifies a GPU vendor.

.. data:: Unknown

  A GPU from an unknown vendor

.. data:: ARM

  An ARM GPU

.. data:: AMD

  An AMD GPU

.. data:: Broadcom

  A Broadcom GPU

.. data:: Imagination

  An Imagination GPU

.. data:: Intel

  An Intel GPU

.. data:: nVidia

  An nVidia GPU

.. data:: Qualcomm

  A Qualcomm  GPU

.. data:: Verisilicon

  A Verisilicon or Vivante GPU

.. data:: Software

  A software-rendering emulated GPU
)");
enum class GPUVendor : uint32_t
{
  Unknown,
  ARM,
  AMD,
  Broadcom,
  Imagination,
  Intel,
  nVidia,
  Qualcomm,
  Verisilicon,
  Software,
};

DECLARE_REFLECTION_ENUM(GPUVendor);

DOCUMENT(R"(Get the GPUVendor for a given PCI Vendor ID.

:param int vendorID: The PCI Vendor ID
:return: The vendor identified
:rtype: GPUVendor
)");
constexpr GPUVendor GPUVendorFromPCIVendor(uint32_t vendorID)
{
  // temporarily disable clang-format to make this more readable.
  // Ideally we'd use a simple switch() but VS2015 doesn't support that :(.
  // clang-format off
  return vendorID == 0x13B5 ? GPUVendor::ARM
       : vendorID == 0x1002 ? GPUVendor::AMD
       : vendorID == 0x1010 ? GPUVendor::Imagination
       : vendorID == 0x8086 ? GPUVendor::Intel
       : vendorID == 0x10DE ? GPUVendor::nVidia
       : vendorID == 0x5143 ? GPUVendor::Qualcomm
       : GPUVendor::Unknown;
  // clang-format on
}

DOCUMENT(R"(Identifies a Graphics API.

.. data:: D3D11

  Direct3D 11.

.. data:: D3D12

  Direct3D 12.

.. data:: OpenGL

  OpenGL.

.. data:: Vulkan

  Vulkan.

)");
enum class GraphicsAPI : uint32_t
{
  D3D11,
  D3D12,
  OpenGL,
  Vulkan,
};

DECLARE_REFLECTION_ENUM(GraphicsAPI);

DOCUMENT(R"(Check if an API is D3D or not

:param GraphicsAPI api: The graphics API in question
:return: ``True`` if api is a D3D-based API, ``False`` otherwise
)");
constexpr inline bool IsD3D(GraphicsAPI api)
{
  return api == GraphicsAPI::D3D11 || api == GraphicsAPI::D3D12;
}

DOCUMENT(R"(Identifies a shader encoding used to pass shader code to an API.

.. data:: Unknown

  Unknown or unprocessable format.

.. data:: DXBC

  DXBC binary shader, used by D3D11 and D3D12.

.. data:: GLSL

  GLSL in string format, used by OpenGL.

.. data:: SPIRV

  SPIR-V binary shader, used by Vulkan and with an extension by OpenGL.

.. data:: SPIRVAsm

  Canonical SPIR-V assembly form, used (indirectly via :data:`SPIRV`) by Vulkan and with an
  extension by OpenGL.

.. data:: HLSL

  HLSL in string format, used by D3D11, D3D12, and Vulkan/GL via compilation to SPIR-V.
)");
enum class ShaderEncoding : uint32_t
{
  Unknown,
  First = Unknown,
  DXBC,
  GLSL,
  SPIRV,
  SPIRVAsm,
  HLSL,
  Count,
};

ITERABLE_OPERATORS(ShaderEncoding);
DECLARE_REFLECTION_ENUM(ShaderEncoding);

DOCUMENT(R"(Check whether or not this is a human readable text representation.

:param ShaderEncoding e: The encoding to check.
:return: ``True`` if it describes a text representation, ``False`` for a bytecode representation.
:rtype: ``bool``
)");
constexpr inline bool IsTextRepresentation(ShaderEncoding encoding)
{
  return encoding == ShaderEncoding::HLSL || encoding == ShaderEncoding::GLSL ||
         encoding == ShaderEncoding::SPIRVAsm;
}

DOCUMENT(R"(A primitive topology used for processing vertex data.

.. data:: Unknown

  An unknown or undefined topology.

.. data:: PointList

  A point list.

.. data:: LineList

  A line list.

.. data:: LineStrip

  A line strip.

.. data:: LineLoop

  A line loop.

.. data:: TriangleList

  A triangle list.

.. data:: TriangleStrip

  A triangle strip.

.. data:: TriangleFan

  A triangle fan.

.. data:: LineList_Adj

  A line list with adjacency information.

.. data:: LineStrip_Adj

  A line strip with adjacency information.

.. data:: TriangleList_Adj

  A triangle list with adjacency information.

.. data:: TriangleStrip_Adj

  A triangle strip with adjacency information.

.. data:: PatchList

  An alias for :data:`PatchList_1CPs`.

.. data:: PatchList_1CPs

  A patch list with 1 control points.

.. data:: PatchList_2CPs

  A patch list with 2 control points.

.. data:: PatchList_3CPs

  A patch list with 3 control points.

.. data:: PatchList_4CPs

  A patch list with 4 control points.

.. data:: PatchList_5CPs

  A patch list with 5 control points.

.. data:: PatchList_6CPs

  A patch list with 6 control points.

.. data:: PatchList_7CPs

  A patch list with 7 control points.

.. data:: PatchList_8CPs

  A patch list with 8 control points.

.. data:: PatchList_9CPs

  A patch list with 9 control points.

.. data:: PatchList_10CPs

  A patch list with 10 control points.

.. data:: PatchList_11CPs

  A patch list with 11 control points.

.. data:: PatchList_12CPs

  A patch list with 12 control points.

.. data:: PatchList_13CPs

  A patch list with 13 control points.

.. data:: PatchList_14CPs

  A patch list with 14 control points.

.. data:: PatchList_15CPs

  A patch list with 15 control points.

.. data:: PatchList_16CPs

  A patch list with 16 control points.

.. data:: PatchList_17CPs

  A patch list with 17 control points.

.. data:: PatchList_18CPs

  A patch list with 18 control points.

.. data:: PatchList_19CPs

  A patch list with 19 control points.

.. data:: PatchList_20CPs

  A patch list with 20 control points.

.. data:: PatchList_21CPs

  A patch list with 21 control points.

.. data:: PatchList_22CPs

  A patch list with 22 control points.

.. data:: PatchList_23CPs

  A patch list with 23 control points.

.. data:: PatchList_24CPs

  A patch list with 24 control points.

.. data:: PatchList_25CPs

  A patch list with 25 control points.

.. data:: PatchList_26CPs

  A patch list with 26 control points.

.. data:: PatchList_27CPs

  A patch list with 27 control points.

.. data:: PatchList_28CPs

  A patch list with 28 control points.

.. data:: PatchList_29CPs

  A patch list with 29 control points.

.. data:: PatchList_30CPs

  A patch list with 30 control points.

.. data:: PatchList_31CPs

  A patch list with 31 control points.

.. data:: PatchList_32CPs

  A patch list with 32 control points.

)");
enum class Topology : uint32_t
{
  Unknown,
  PointList,
  LineList,
  LineStrip,
  LineLoop,
  TriangleList,
  TriangleStrip,
  TriangleFan,
  LineList_Adj,
  LineStrip_Adj,
  TriangleList_Adj,
  TriangleStrip_Adj,
  PatchList,
  PatchList_1CPs = PatchList,
  PatchList_2CPs,
  PatchList_3CPs,
  PatchList_4CPs,
  PatchList_5CPs,
  PatchList_6CPs,
  PatchList_7CPs,
  PatchList_8CPs,
  PatchList_9CPs,
  PatchList_10CPs,
  PatchList_11CPs,
  PatchList_12CPs,
  PatchList_13CPs,
  PatchList_14CPs,
  PatchList_15CPs,
  PatchList_16CPs,
  PatchList_17CPs,
  PatchList_18CPs,
  PatchList_19CPs,
  PatchList_20CPs,
  PatchList_21CPs,
  PatchList_22CPs,
  PatchList_23CPs,
  PatchList_24CPs,
  PatchList_25CPs,
  PatchList_26CPs,
  PatchList_27CPs,
  PatchList_28CPs,
  PatchList_29CPs,
  PatchList_30CPs,
  PatchList_31CPs,
  PatchList_32CPs,
};

DECLARE_REFLECTION_ENUM(Topology);

DOCUMENT(R"(Return the patch list ``Topology`` with N control points

``N`` must be between 1 and 32 inclusive.

:param int N: The number of control points in the patch list
:return: The patchlist topology with that number of control points
:rtype: Topology
)");
constexpr inline Topology PatchList_Topology(uint32_t N)
{
  return (N < 1 || N > 32) ? Topology::PatchList_1CPs
                           : Topology(uint32_t(Topology::PatchList_1CPs) + N - 1);
}

DOCUMENT(R"(Return the number of control points in a patch list ``Topology``

``t`` must be a patch list topology, the return value will be between 1 and 32 inclusive

:param Topology t: The patch list topology
:return: The number of control points in the specified topology
:rtype: ``int``
)");
constexpr inline uint32_t PatchList_Count(Topology topology)
{
  return uint32_t(topology) < uint32_t(Topology::PatchList_1CPs)
             ? 0
             : uint32_t(topology) - uint32_t(Topology::PatchList_1CPs) + 1;
}

DOCUMENT(R"(Check whether or not this is a strip-type topology.

:param Topology t: The topology to check.
:return: ``True`` if it describes a strip topology, ``False`` for a list.
:rtype: ``bool``
)");
constexpr inline bool IsStrip(Topology topology)
{
  return topology == Topology::LineStrip || topology == Topology::TriangleStrip ||
         topology == Topology::LineStrip_Adj || topology == Topology::TriangleStrip_Adj;
}

DOCUMENT(R"(The stage in a pipeline where a shader runs

.. data:: Vertex

  The vertex shader.

.. data:: Hull

  The hull shader. See also :data:`Tess_Control`.

.. data:: Tess_Control

  The tessellation control shader. See also :data:`Hull`.

.. data:: Domain

  The domain shader. See also :data:`Tess_Eval`.

.. data:: Tess_Eval

  The tessellation evaluation shader. See also :data:`Domain`.

.. data:: Geometry

  The geometry shader.

.. data:: Pixel

  The pixel shader. See also :data:`Fragment`.

.. data:: Fragment

  The fragment shader. See also :data:`Pixel`.

.. data:: Compute

  The compute shader.
)");
enum class ShaderStage : uint32_t
{
  Vertex = 0,
  First = Vertex,

  Hull,
  Tess_Control = Hull,

  Domain,
  Tess_Eval = Domain,

  Geometry,

  Pixel,
  Fragment = Pixel,

  Compute,

  Count,
};

ITERABLE_OPERATORS(ShaderStage);
DECLARE_REFLECTION_ENUM(ShaderStage);

template <typename integer>
constexpr inline ShaderStage StageFromIndex(integer stage)
{
  return ShaderStage(stage);
}

DOCUMENT(R"(The type of issue that a debug message is about.

.. data:: Application_Defined

  This message was generated by the application.

.. data:: Miscellaneous

  This message doesn't fall into any other pre-defined category.

.. data:: Initialization

  This message is about initialisation or creation of objects.

.. data:: Cleanup

  This message is about cleanup, destruction or shutdown of objects.

.. data:: Compilation

  This message is about compilation of shaders.

.. data:: State_Creation

  This message is about creating unified state objects.

.. data:: State_Setting

  This message is about changing current pipeline state.

.. data:: State_Getting

  This message is about fetching or retrieving current pipeline state.

.. data:: Resource_Manipulation

  This message is about updating or changing a resource's properties or contents.

.. data:: Execution

  This message is about performing work.

.. data:: Shaders

  This message is about the use, syntax, binding or linkage of shaders.

.. data:: Deprecated

  This message is about the use of deprecated functionality.

.. data:: Undefined

  This message is about the use of undefined behaviour.

.. data:: Portability

  This message is about behaviour that could be or is not portable between different environments.

.. data:: Performance

  This message is about performance problems or pitfalls.
)");
enum class MessageCategory : uint32_t
{
  Application_Defined = 0,
  Miscellaneous,
  Initialization,
  Cleanup,
  Compilation,
  State_Creation,
  State_Setting,
  State_Getting,
  Resource_Manipulation,
  Execution,
  Shaders,
  Deprecated,
  Undefined,
  Portability,
  Performance,
};

DECLARE_REFLECTION_ENUM(MessageCategory);

DOCUMENT(R"(How serious a debug message is

.. data:: High

  This message is very serious, indicating a guaranteed problem or major flaw.

.. data:: Medium

  This message is somewhat serious, indicating a problem that should be addressed or investigated.

.. data:: Low

  This message is not very serious. This indicates something that might indicate a problem.

.. data:: Info

  This message is not about a problem but is purely informational.
)");
enum class MessageSeverity : uint32_t
{
  High = 0,
  Medium,
  Low,
  Info,
};

DECLARE_REFLECTION_ENUM(MessageSeverity);

DOCUMENT(R"(Where a debug message was reported from

.. data:: API

  This message comes from the API's debugging or validation layers.

.. data:: RedundantAPIUse

  This message comes from detecting redundant API calls - calls with no side-effect or purpose, e.g.
  setting state that is already set.

.. data:: IncorrectAPIUse

  This message comes from detecting incorrect use of the API.

.. data:: GeneralPerformance

  This message comes from detecting general performance problems that are not hardware or platform
  specific.

.. data:: GCNPerformance

  This message comes from detecting patterns that will cause performance problems on GCN-based
  hardware.

.. data:: RuntimeWarning

  This message comes not from inspecting the log but something detected at runtime while in use,
  for example exceptions generated during shader debugging.

.. data:: UnsupportedConfiguration

  This message comes from replaying a capture in an environment with insufficient capability to
  accurately reproduce the API work. Either this means the replay will be wrong, or it may be that
  depending on the exact API work some inaccuracies might happen.

)");
enum class MessageSource : uint32_t
{
  API = 0,
  RedundantAPIUse,
  IncorrectAPIUse,
  GeneralPerformance,
  GCNPerformance,
  RuntimeWarning,
  UnsupportedConfiguration,
};

DECLARE_REFLECTION_ENUM(MessageSource);

DOCUMENT(R"(How a resource is being used in the pipeline at a particular point.

Note that a resource may be used for more than one thing in one event, see :class:`EventUsage`.

.. data:: Unused

  The resource is not being used.

.. data:: VertexBuffer

  The resource is being used as a fixed-function vertex buffer input.

.. data:: IndexBuffer

  The resource is being used as an index buffer.

.. data:: VS_Constants

  The resource is being used for constants in the :data:`vertex shader <ShaderStage.Vertex>`.

.. data:: HS_Constants

  The resource is being used for constants in the tessellation control or
  :data:`hull shader <ShaderStage.Hull>`.

.. data:: DS_Constants

  The resource is being used for constants in the tessellation evaluation or
  :data:`domain shader <ShaderStage.Domain>`.

.. data:: GS_Constants

  The resource is being used for constants in the :data:`geometry shader <ShaderStage.Geometry>`.

.. data:: PS_Constants

  The resource is being used for constants in the :data:`pixel shader <ShaderStage.Pixel>`.

.. data:: CS_Constants

  The resource is being used for constants in the :data:`compute shader <ShaderStage.Compute>`.

.. data:: All_Constants

  The resource is being used for constants in all shader stages.

.. data:: StreamOut

  The resource is being used for stream out/transform feedback storage after geometry processing.

.. data:: VS_Resource

  The resource is being used as a read-only resource in the
  :data:`vertex shader <ShaderStage.Vertex>`.

.. data:: HS_Resource

  The resource is being used as a read-only resource in the tessellation control or
  :data:`hull shader <ShaderStage.Hull>`.

.. data:: DS_Resource

  The resource is being used as a read-only resource in the tessellation evaluation or
  :data:`domain shader <ShaderStage.Domain>`.

.. data:: GS_Resource

  The resource is being used as a read-only resource in the
  :data:`geometry shader <ShaderStage.Geometry>`.

.. data:: PS_Resource

  The resource is being used as a read-only resource in the
  :data:`pixel shader <ShaderStage.Pixel>`.

.. data:: CS_Resource

  The resource is being used as a read-only resource in the
  :data:`compute shader <ShaderStage.Compute>`.

.. data:: All_Resource

  The resource is being used as a read-only resource in all shader stages.

.. data:: VS_RWResource

  The resource is being used as a read-write resource in the
  :data:`vertex shader <ShaderStage.Vertex>`.

.. data:: HS_RWResource

  The resource is being used as a read-write resource in the tessellation control or
  :data:`hull shader <ShaderStage.Hull>`.

.. data:: DS_RWResource

  The resource is being used as a read-write resource in the tessellation evaluation or
  :data:`domain shader <ShaderStage.Domain>`.

.. data:: GS_RWResource

  The resource is being used as a read-write resource in the
  :data:`geometry shader <ShaderStage.Geometry>`.

.. data:: PS_RWResource

  The resource is being used as a read-write resource in the
  :data:`pixel shader <ShaderStage.Pixel>`.

.. data:: CS_RWResource

  The resource is being used as a read-write resource in the
  :data:`compute shader <ShaderStage.Compute>`.

.. data:: All_RWResource

  The resource is being used as a read-write resource in all shader stages.

.. data:: InputTarget

  The resource is being read as an input target for reading from the target currently being written.

.. data:: ColorTarget

  The resource is being written to as a color output.

.. data:: DepthStencilTarget

  The resource is being written to and tested against as a depth-stencil output.

.. data:: Indirect

  The resource is being used for indirect arguments.

.. data:: Clear

  The resource is being cleared

.. data:: GenMips

  The resource is having mips generated for it.

.. data:: Resolve

  The resource is being resolved or blitted, as both source and destination.

.. data:: ResolveSrc

  The resource is being resolved or blitted from.

.. data:: ResolveDst

  The resource is being resolved or blitted to.

.. data:: Copy

  The resource is being copied, as both source and destination.

.. data:: CopySrc

  The resource is being copied from.

.. data:: CopyDst

  The resource is being copied to.

.. data:: Barrier

  The resource is being specified in a barrier, as defined in Vulkan or Direct3D 12.
)");
enum class ResourceUsage : uint32_t
{
  Unused,

  VertexBuffer,
  IndexBuffer,

  VS_Constants,
  HS_Constants,
  DS_Constants,
  GS_Constants,
  PS_Constants,
  CS_Constants,

  All_Constants,

  StreamOut,

  VS_Resource,
  HS_Resource,
  DS_Resource,
  GS_Resource,
  PS_Resource,
  CS_Resource,

  All_Resource,

  VS_RWResource,
  HS_RWResource,
  DS_RWResource,
  GS_RWResource,
  PS_RWResource,
  CS_RWResource,

  All_RWResource,

  InputTarget,
  ColorTarget,
  DepthStencilTarget,

  Indirect,

  Clear,

  GenMips,
  Resolve,
  ResolveSrc,
  ResolveDst,
  Copy,
  CopySrc,
  CopyDst,

  Barrier,
};

DECLARE_REFLECTION_ENUM(ResourceUsage);

template <typename integer>
constexpr inline ResourceUsage CBUsage(integer stage)
{
  return ResourceUsage(uint32_t(ResourceUsage::VS_Constants) + stage);
}

DOCUMENT(R"(Calculate the ``ResourceUsage`` value for constant buffer use at a given shader stage.

:param ShaderStage stage: The shader stage.
:return: The value for constant buffer usage at a given shader stage.
:rtype: ResourceUsage
)");
constexpr inline ResourceUsage CBUsage(ShaderStage stage)
{
  return CBUsage(uint32_t(stage));
}

template <typename integer>
constexpr inline ResourceUsage ResUsage(integer stage)
{
  return ResourceUsage(uint32_t(ResourceUsage::VS_Resource) + stage);
}

DOCUMENT(R"(Calculate the ``ResourceUsage`` value for read-only resource use at a given shader
stage.

:param ShaderStage stage: The shader stage.
:return: The value for read-only resource usage at a given shader stage.
:rtype: ResourceUsage
)");
constexpr inline ResourceUsage ResUsage(ShaderStage stage)
{
  return ResUsage(uint32_t(stage));
}

template <typename integer>
constexpr inline ResourceUsage RWResUsage(integer stage)
{
  return ResourceUsage(uint32_t(ResourceUsage::VS_RWResource) + stage);
}

DOCUMENT(R"(Calculate the ``ResourceUsage`` value for read-write resource use at a given shader
stage.

:param ShaderStage stage: The shader stage.
:return: The value for read-write resource usage at a given shader stage.
:rtype: ResourceUsage
)");
constexpr inline ResourceUsage RWResUsage(ShaderStage stage)
{
  return RWResUsage(uint32_t(stage));
}

DOCUMENT(R"(What kind of solid shading to use when rendering a mesh.

.. data:: NoSolid

  No solid shading should be done.

.. data:: Solid

  The mesh should be rendered in a single flat unshaded color.

.. data:: Lit

  The mesh should be rendered with face normals generated on the primitives and used for lighting.

.. data:: Secondary

  The mesh should be rendered using the secondary element as color.

)");
enum class SolidShade : uint32_t
{
  NoSolid = 0,
  Solid,
  Lit,
  Secondary,
  Count,
};

DECLARE_REFLECTION_ENUM(SolidShade);

DOCUMENT(R"(The fill mode for polygons.

.. data:: Solid

  Polygons are filled in and rasterized solidly.

.. data:: Wireframe

  Polygons are rendered only with lines along their edges, forming a wireframe.

.. data:: Point

  Only the points at the polygons vertices are rendered.
)");
enum class FillMode : uint32_t
{
  Solid = 0,
  Wireframe,
  Point,
};

DECLARE_REFLECTION_ENUM(FillMode);

DOCUMENT(R"(The culling mode for polygons.

.. data:: NoCull

  No polygon culling is performed.

.. data:: Front

  Front-facing polygons are culled.

.. data:: Back

  Back-facing polygons are culled.

.. data:: FrontAndBack

  Both front-facing and back-facing polygons are culled.
)");
enum class CullMode : uint32_t
{
  NoCull = 0,
  Front,
  Back,
  FrontAndBack,
};

DECLARE_REFLECTION_ENUM(CullMode);

DOCUMENT(R"(The conservative rasterization mode.

.. data:: Disabled

  No conservative rasterization, the default rasterization coverage algorithm is used.

.. data:: Underestimate

  Fragments will only be generated if the primitive full covers all parts of the pixel, including
  edges and corners.

.. data:: Overestimate

  Fragments will be generated if the primitive covers any part of the pixel, including edges and
  corners.
)");
enum class ConservativeRaster : uint32_t
{
  Disabled = 0,
  Underestimate,
  Overestimate,
};

DECLARE_REFLECTION_ENUM(ConservativeRaster);

DOCUMENT(R"(The texture filtering mode for a given direction (minification, magnification, or
between mips).

.. data:: NoFilter

  No filtering - this direction is disabled or there is no sampler.

.. data:: Point

  Point or nearest filtering - the closest pixel or mip level to the sample location is used.

.. data:: Linear

  Linear filtering - a linear interpolation happens between the pixels or mips on either side of the
  sample location in each direction.

.. data:: Cubic

  Similar to linear filtering but with a cubic curve used for interpolation instead of linear.

.. data:: Anisotropic

  This sampler is using anisotropic filtering.
)");
enum class FilterMode : uint32_t
{
  NoFilter,
  Point,
  Linear,
  Cubic,
  Anisotropic,
};

DECLARE_REFLECTION_ENUM(FilterMode);

DOCUMENT(R"(The function used to process the returned value after interpolation.

.. data:: Normal

  No special processing is used, the value is returned directly to the shader.

.. data:: Comparison

  The value from interpolation is compared to a reference value and the comparison result is
  returned to the shader.

.. data:: Minimum

  Instead of interpolating between sample points to retrieve an interpolated value, a min filter is
  used instead to find the minimum sample value.

  Texels that were weight to 0 during interpolation are not included in the min function.

.. data:: Maximum

  Instead of interpolating between sample points to retrieve an interpolated value, a max filter is
  used instead to find the maximum sample value.

  Texels that were weight to 0 during interpolation are not included in the max function.
)");
enum class FilterFunction : uint32_t
{
  Normal,
  Comparison,
  Minimum,
  Maximum,
};

DECLARE_REFLECTION_ENUM(FilterFunction);

DOCUMENT(R"(A comparison function to return a ``bool`` result from two inputs ``A`` and ``B``.

.. data:: Never

  ``False``

.. data:: AlwaysTrue

  ``True``

.. data:: Less

  ``A < B``

.. data:: LessEqual

  ``A <= B``

.. data:: Greater

  ``A > B``

.. data:: GreaterEqual

  ``A >= B``

.. data:: Equal

  ``A == B``

.. data:: NotEqual

  ``A != B``

)");
enum class CompareFunction : uint32_t
{
  Never,
  AlwaysTrue,
  Less,
  LessEqual,
  Greater,
  GreaterEqual,
  Equal,
  NotEqual,
};

DECLARE_REFLECTION_ENUM(CompareFunction);

DOCUMENT(R"(A stencil operation to apply in stencil processing.

.. data:: Keep

  Keep the existing value unmodified.

.. data:: Zero

  Set the value to ``0``.

.. data:: Replace

  Replace the value with the stencil reference value.

.. data:: IncSat

  Increment the value but saturate at the maximum representable value (typically ``255``).

.. data:: DecSat

  Decrement the value but saturate at ``0``.

.. data:: IncWrap

  Increment the value and wrap at the maximum representable value (typically ``255``) to ``0``.

.. data:: DecWrap

  Decrement the value and wrap at ``0`` to the maximum representable value (typically ``255``).

.. data:: Invert

  Invert the bits in the stencil value (bitwise ``NOT``).
)");
enum class StencilOperation : uint32_t
{
  Keep,
  Zero,
  Replace,
  IncSat,
  DecSat,
  IncWrap,
  DecWrap,
  Invert,
};

DECLARE_REFLECTION_ENUM(StencilOperation);

DOCUMENT(R"(A multiplier on one component in the blend equation.

.. note:: The "source" value is the value written out by the shader.

  The "second source" value is provided when dual source blending is used.

  The "destination" value is the value in the target being blended to.

  These values are combined using a given blend operation, see :class:`BlendOperation`.

  Where a color is referenced, the value depends on where the multiplier appears in the blend
  equation. If it is a multiplier on the color component then it refers to the color component. If
  it is a multiplier on the alpha component then it refers to the alpha component.

  If alpha is referenced explicitly it always refers to alpha, in both color and alpha equations.

.. data:: Zero

  The literal value ``0.0``.

.. data:: One

  The literal value ``1.0``.

.. data:: SrcCol

  The source value's color.

.. data:: InvSrcCol

  ``1.0`` minus the source value's color.

.. data:: DstCol

  The destination value's color.

.. data:: InvDstCol

  ``1.0`` minus the destination value's color.

.. data:: SrcAlpha

  The source value's alpha.

.. data:: InvSrcAlpha

  ``1.0`` minus the source value's alpha.

.. data:: DstAlpha

  The destination value's alpha.

.. data:: InvDstAlpha

  ``1.0`` minus the destination value's alpha.

.. data:: SrcAlphaSat

  The lowest value of :data:`SrcAlpha` and :data:`InvDstAlpha`. If used in the alpha equation, it takes the value :data:`One`.

.. data:: FactorRGB

  The color components of the fixed blend factor constant.

.. data:: InvFactorRGB

  ``1.0`` minus the color components of the fixed blend factor constant.

.. data:: FactorAlpha

  The alpha component of the fixed blend factor constant.

.. data:: InvFactorAlpha

  ``1.0`` minus the alpha components of the fixed blend factor constant.

.. data:: Src1Col

  The second source value's color.

.. data:: InvSrc1Col

  ``1.0`` minus the second source value's color.

.. data:: Src1Alpha

  The second source value's alpha.

.. data:: InvSrc1Alpha

  ``1.0`` minus the second source value's alpha.
)");
enum class BlendMultiplier : uint32_t
{
  Zero,
  One,
  SrcCol,
  InvSrcCol,
  DstCol,
  InvDstCol,
  SrcAlpha,
  InvSrcAlpha,
  DstAlpha,
  InvDstAlpha,
  SrcAlphaSat,
  FactorRGB,
  InvFactorRGB,
  FactorAlpha,
  InvFactorAlpha,
  Src1Col,
  InvSrc1Col,
  Src1Alpha,
  InvSrc1Alpha,
};

DECLARE_REFLECTION_ENUM(BlendMultiplier);

DOCUMENT(R"(A blending operation to apply in color blending.

.. note:: The "source" value is the value written out by the shader.

  The "destination" value is the value in the target being blended to.

  These values are multiplied by a given blend factor, see :class:`BlendMultiplier`.

.. data:: Add

  Add the two values being processed together.

.. data:: Subtract

  Subtract the destination value from the source value.

.. data:: ReversedSubtract

  Subtract the source value from the destination value.

.. data:: Minimum

  The minimum of the source and destination value.

.. data:: Maximum

  The maximum of the source and destination value.
)");
enum class BlendOperation : uint32_t
{
  Add,
  Subtract,
  ReversedSubtract,
  Minimum,
  Maximum,
};

DECLARE_REFLECTION_ENUM(BlendOperation);

DOCUMENT(R"(A logical operation to apply when writing texture values to an output.

.. note:: The "source" value is the value written out by the shader.

  The "destination" value is the value in the target being written to.

.. data:: NoOp

  No operation is performed, the destination is unmodified.

.. data:: Clear

  A ``0`` in every bit.

.. data:: Set

  A ``1`` in every bit.

.. data:: Copy

  The contents of the source value.

.. data:: CopyInverted

  The contents of the source value are bitwise inverted.

.. data:: Invert

  The contents of the destination value are bitwise inverted, then written.

.. data:: And

  The source and destination values are combined with the bitwise ``AND`` operator.

.. data:: Nand

  The source and destination values are combined with the bitwise ``NAND`` operator.

.. data:: Or

  The source and destination values are combined with the bitwise ``OR`` operator.

.. data:: Xor

  The source and destination values are combined with the bitwise ``XOR`` operator.

.. data:: Nor

  The source and destination values are combined with the bitwise ``NOR`` operator.

.. data:: Equivalent

  The source and destination values are combined with the logical equivalence operator, defined as
  ``NOT (s XOR d)``.

.. data:: AndReverse

  The source and inverted destination values are combined with the bitwise ``AND`` operator - i.e.
  ``s AND (NOT d)``.

.. data:: AndInverted

  The inverted source and destination values are combined with the bitwise ``AND`` operator - i.e.
  ``(NOT s) AND d``.

.. data:: OrReverse

  The source and inverted destination values are combined with the bitwise ``OR`` operator - i.e.
  ``s OR (NOT d)``.

.. data:: OrInverted

  The inverted source and destination values are combined with the bitwise ``OR`` operator - i.e.
  ``(NOT s) OR d``.
)");
enum class LogicOperation : uint32_t
{
  NoOp,
  Clear,
  Set,
  Copy,
  CopyInverted,
  Invert,
  And,
  Nand,
  Or,
  Xor,
  Nor,
  Equivalent,
  AndReverse,
  AndInverted,
  OrReverse,
  OrInverted,
};

DECLARE_REFLECTION_ENUM(LogicOperation);

DOCUMENT(R"(Pre-defined GPU counters that can be supported by a given implementation.

GPU counters actually available can be queried by :meth:`ReplayController.EnumerateCounters`. If any
in this list are supported they will be returned with these counter IDs. More counters may be
enumerated with IDs in the appropriate ranges.

.. data:: EventGPUDuration

  Time taken for this event on the GPU, as measured by delta between two GPU timestamps.

.. data:: InputVerticesRead

  Number of vertices read by input assembler.

.. data:: IAPrimitives

  Number of primitives read by the input assembler.

.. data:: GSPrimitives

  Number of primitives output by a geometry shader.

.. data:: RasterizerInvocations

  Number of primitives that were sent to the rasterizer.

.. data:: RasterizedPrimitives

  Number of primitives that were rendered.

.. data:: SamplesPassed

  Number of samples that passed depth/stencil test.

.. data:: VSInvocations

  Number of times a :data:`vertex shader <ShaderStage.Vertex>` was invoked.

.. data:: HSInvocations

  Number of times a :data:`hull shader <ShaderStage.Hull>` was invoked.

.. data:: TCSInvocations

  Number of times a :data:`tessellation control shader <ShaderStage.Tess_Control>` was invoked.

.. data:: DSInvocations

  Number of times a :data:`domain shader <ShaderStage.Domain>` was invoked.

.. data:: TESInvocations

  Number of times a :data:`tessellation evaluation shader <ShaderStage.Tess_Eval>` was invoked.

.. data:: GSInvocations

  Number of times a :data:`domain shader <ShaderStage.Domain>` was invoked.

.. data:: PSInvocations

  Number of times a :data:`pixel shader <ShaderStage.Pixel>` was invoked.

.. data:: FSInvocations

  Number of times a :data:`fragment shader <ShaderStage.Fragment>` was invoked.

.. data:: CSInvocations

  Number of times a :data:`compute shader <ShaderStage.Compute>` was invoked.

.. data:: FirstAMD

  The AMD-specific counter IDs start from this value.

.. data:: LastAMD

  The AMD-specific counter IDs end with this value.

.. data:: FirstIntel

  The Intel-specific counter IDs start from this value.

.. data:: LastIntel

  The Intel-specific counter IDs end with this value.

.. data:: FirstNvidia

  The nVidia-specific counter IDs start from this value.

.. data:: LastNvidia

  The nVidia-specific counter IDs end with this value.
)");
enum class GPUCounter : uint32_t
{
  EventGPUDuration = 1,
  First = EventGPUDuration,
  InputVerticesRead,
  IAPrimitives,
  GSPrimitives,
  RasterizerInvocations,
  RasterizedPrimitives,
  SamplesPassed,
  VSInvocations,
  HSInvocations,
  TCSInvocations = HSInvocations,
  DSInvocations,
  TESInvocations = DSInvocations,
  GSInvocations,
  PSInvocations,
  FSInvocations = PSInvocations,
  CSInvocations,
  Count,

  // IHV specific counters can be set above this point
  // with ranges reserved for each IHV
  FirstAMD = 1000000,

  FirstIntel = 2000000,
  LastAMD = FirstIntel - 1,

  FirstNvidia = 3000000,
  LastIntel = FirstNvidia - 1,

  LastNvidia = 4000000,
};

ITERABLE_OPERATORS(GPUCounter);
DECLARE_REFLECTION_ENUM(GPUCounter);

DOCUMENT(R"(Check whether or not this is a Generic counter.

:param GPUCounter c: The counter.
:return: ``True`` if it is a generic counter, ``False`` if it's not.
:rtype: ``bool``
)");
inline constexpr bool IsGenericCounter(GPUCounter c)
{
  return c < GPUCounter::Count;
}

DOCUMENT(R"(Check whether or not this is an AMD private counter.

:param GPUCounter c: The counter.
:return: ``True`` if it is an AMD private counter, ``False`` if it's not.
:rtype: ``bool``
)");
inline constexpr bool IsAMDCounter(GPUCounter c)
{
  return c >= GPUCounter::FirstAMD && c <= GPUCounter::LastAMD;
}

DOCUMENT(R"(Check whether or not this is an Intel private counter.

:param GPUCounter c: The counter.
:return: ``True`` if it is an Intel private counter, ``False`` if it's not.
:rtype: ``bool``
)");
inline constexpr bool IsIntelCounter(GPUCounter c)
{
  return c >= GPUCounter::FirstIntel && c <= GPUCounter::LastIntel;
}

DOCUMENT(R"(Check whether or not this is an Nvidia private counter.

:param GPUCounter c: The counter.
:return: ``True`` if it is an Nvidia private counter, ``False`` if it's not.
:rtype: ``bool``
)");
inline constexpr bool IsNvidiaCounter(GPUCounter c)
{
  return c >= GPUCounter::FirstNvidia && c <= GPUCounter::LastNvidia;
}

DOCUMENT(R"(The unit that GPU counter data is returned in.

.. data:: Absolute

  The value is an absolute value and should be interpreted as unitless.

.. data:: Seconds

  The value is a duration in seconds.

.. data:: Percentage

  The value is a floating point percentage value between 0.0 and 1.0.

.. data:: Ratio

  The value describes a ratio between two separate GPU units or counters.

.. data:: Bytes

  The value is in bytes.

.. data:: Cycles

  The value is a duration in clock cycles.
)");
enum class CounterUnit : uint32_t
{
  Absolute,
  Seconds,
  Percentage,
  Ratio,
  Bytes,
  Cycles,
};

DECLARE_REFLECTION_ENUM(CounterUnit);

DOCUMENT(R"(The type of camera controls for an :class:`Camera`.

.. data:: Arcball

  Arcball controls that rotate and zoom around the origin point.

.. data:: FPSLook

  Traditional FPS style controls with movement in each axis relative to the current look direction.
)");
enum class CameraType : uint32_t
{
  Arcball = 0,
  FPSLook,
};

DECLARE_REFLECTION_ENUM(CameraType);

DOCUMENT(R"(How supported a given API is on a particular replay instance.

.. data:: Unsupported

  The API is not supported.

.. data:: Supported

  The API is fully supported.

.. data:: SuggestRemote

  The API is supported locally but the capture indicates it was made on a different type of machine
  so remote replay might be desired.
)");
enum class ReplaySupport : uint32_t
{
  Unsupported,
  Supported,
  SuggestRemote,
};

DECLARE_REFLECTION_ENUM(ReplaySupport);

DOCUMENT(R"(The status of a high-level replay operation such as opening a capture or connecting to
a remote server.

.. data:: Succeeded

  The operation succeeded.

.. data:: UnknownError

  An unknown error occurred.

.. data:: InternalError

  An internal error occurred indicating a bug or unexpected condition.

.. data:: FileNotFound

  The specified file was not found.

.. data:: InjectionFailed

  Injection or hooking into the target process failed.

.. data:: IncompatibleProcess

  An incompatible process was found, e.g. a 32-bit process with 32-bit support not available.

.. data:: NetworkIOFailed

  A network I/O operation failed.

.. data:: NetworkRemoteBusy

  The remote side of the network connection was busy.

.. data:: NetworkVersionMismatch

  The other side of the network connection was not at a compatible version.

.. data:: FileIOFailed

  A filesystem I/O operation failed.

.. data:: FileIncompatibleVersion

  The capture file had an incompatible version.

.. data:: FileCorrupted

  The capture file is corrupted or otherwise unrecognisable.

.. data:: ImageUnsupported

  The image file is recognised but the format is unsupported.

.. data:: APIUnsupported

  The API used in the capture is not supported.

.. data:: APIInitFailed

  The API used in the capture failed to initialise.

.. data:: APIIncompatibleVersion

  The API data in the capture had an incompatible version.

.. data:: APIHardwareUnsupported

  The API is not supported on the currently available hardware.

.. data:: APIDataCorrupted

  While loading the capture for replay, the driver encountered corrupted or invalid serialised data.

.. data:: APIReplayFailed

  The API failed to replay the capture, with some runtime error that couldn't be determined until
  the replay began.

.. data:: AndroidGrantPermissionsFailed

  Failed to grant runtime permissions when installing Android remote server.

.. data:: AndroidABINotFound

  Couldn't determine supported ABIs when installing Android remote server.

.. data:: AndroidAPKFolderNotFound

  Couldn't find the build-android folder which contains the Android remote server APK.

.. data:: AndroidAPKInstallFailed

  Failed to install Android remote server.
)");
enum class ReplayStatus : uint32_t
{
  Succeeded = 0,
  UnknownError,
  InternalError,
  FileNotFound,
  InjectionFailed,
  IncompatibleProcess,
  NetworkIOFailed,
  NetworkRemoteBusy,
  NetworkVersionMismatch,
  FileIOFailed,
  FileIncompatibleVersion,
  FileCorrupted,
  ImageUnsupported,
  APIUnsupported,
  APIInitFailed,
  APIIncompatibleVersion,
  APIHardwareUnsupported,
  APIDataCorrupted,
  APIReplayFailed,
  JDWPFailure,
  AndroidGrantPermissionsFailed,
  AndroidABINotFound,
  AndroidAPKFolderNotFound,
  AndroidAPKInstallFailed
};

DECLARE_REFLECTION_ENUM(ReplayStatus);

DOCUMENT(R"(The type of message received from or sent to an application target control connection.

.. data:: Unknown

  No message or an unknown message type.

.. data:: Disconnected

  The other end of the connection disconnected.

.. data:: Busy

  The other end of the connection was busy.

.. data:: Noop

  Nothing happened, the connection is being kept alive.

.. data:: NewCapture

  A new capture was made.

.. data:: CaptureCopied

  A capture was successfully copied across the connection.

.. data:: RegisterAPI

  The target has initialised a graphics API.

.. data:: NewChild

  The target has created a child process.

.. data:: CaptureProgress

  Progress update on an on-going frame capture.
)");
enum class TargetControlMessageType : uint32_t
{
  Unknown = 0,
  Disconnected,
  Busy,
  Noop,
  NewCapture,
  CaptureCopied,
  RegisterAPI,
  NewChild,
  CaptureProgress,
  CapturableWindowCount
};

DECLARE_REFLECTION_ENUM(TargetControlMessageType);

DOCUMENT(R"(How to modify an environment variable.

.. data:: Set

  Set the variable to the given value.

.. data:: Append

  Add the given value to the end of the variable, using the separator.

.. data:: Prepend

  Add the given value to the start of the variable, using the separator.
)");
enum class EnvMod : uint32_t
{
  Set,
  Append,
  Prepend,
};

DECLARE_REFLECTION_ENUM(EnvMod);

DOCUMENT(R"(The separator to use if needed when modifying an environment variable.

.. data:: Platform

  Use the character appropriate for separating items on the platform.

  On Windows this means the semi-colon ``;`` character will be used, on posix systems the colon
  ``:`` character will be used.

.. data:: SemiColon

  Use a semi-colon ``;`` character.

.. data:: Colon

  Use a colon ``:`` character.

.. data:: NoSep

  No separator will be used.
)");
enum class EnvSep : uint32_t
{
  Platform,
  SemiColon,
  Colon,
  NoSep,
};

DECLARE_REFLECTION_ENUM(EnvSep);

DOCUMENT(R"(The type of a log message

.. data:: Debug

  The log message is a verbose debug-only message that can be discarded in release builds.

.. data:: Comment

  The log message is informational.

.. data:: Warning

  The log message describes a warning that could indicate a problem or be useful in diagnostics.

.. data:: Error

  The log message indicates an error was encountered.

.. data:: Fatal

  The log message indicates a fatal error occurred which is impossible to recover from.
)");
enum class LogType : int32_t
{
  Debug,
  Comment,
  Warning,
  Error,
  Fatal,
  Count,
};

DECLARE_REFLECTION_ENUM(LogType);

#if defined(ENABLE_PYTHON_FLAG_ENUMS)

ENABLE_PYTHON_FLAG_ENUMS;

#endif

DOCUMENT(R"(A set of flags describing the properties of a path on a remote filesystem.

.. data:: NoFlags

  No special file properties.

.. data:: Directory

  This file is a directory or folder.

.. data:: Hidden

  This file is considered hidden by the filesystem.

.. data:: Executable

  This file has been identified as an executable program or script.

.. data:: ErrorUnknown

  A special flag indicating that a query for this file failed, but for unknown reasons.

.. data:: ErrorAccessDenied

  A special flag indicating that a query for this file failed because access to the path was
  denied.

.. data:: ErrorInvalidPath

  A special flag indicating that a query for this file failed because the path was invalid.
)");
enum class PathProperty : uint32_t
{
  NoFlags = 0x0,
  Directory = 0x1,
  Hidden = 0x2,
  Executable = 0x4,

  ErrorUnknown = 0x2000,
  ErrorAccessDenied = 0x4000,
  ErrorInvalidPath = 0x8000,
};

BITMASK_OPERATORS(PathProperty);
DECLARE_REFLECTION_ENUM(PathProperty);

DOCUMENT(R"(A set of flags describing the properties of a section in a renderdoc capture.

.. data:: NoFlags

  No special section properties.

.. data:: ASCIIStored

  This section was stored as pure ASCII. This can be useful since it is possible to generate
  an ASCII section in a text editor by hand or with any simple printf style script, and then
  concatenate it to a .rdc and have a valid section.

.. data:: LZ4Compressed

  This section is compressed with LZ4 on disk.

.. data:: ZstdCompressed

  This section is compressed with Zstd on disk.
)");
enum class SectionFlags : uint32_t
{
  NoFlags = 0x0,
  ASCIIStored = 0x1,
  LZ4Compressed = 0x2,
  ZstdCompressed = 0x4,
};

BITMASK_OPERATORS(SectionFlags);
DECLARE_REFLECTION_ENUM(SectionFlags);

DOCUMENT(R"(A set of flags describing how this buffer may be used

.. data:: NoFlags

  The buffer will not be used for any of the uses below.

.. data:: Vertex

  The buffer will be used for sourcing vertex input data.

.. data:: Index

  The buffer will be used for sourcing primitive index data.

.. data:: Constants

  The buffer will be used for sourcing shader constant data.

.. data:: ReadWrite

  The buffer will be used for read and write access from shaders.

.. data:: Indirect

  The buffer will be used to provide indirect parameters for launching GPU-based drawcalls.
)");
enum class BufferCategory : uint32_t
{
  NoFlags = 0x0,
  Vertex = 0x1,
  Index = 0x2,
  Constants = 0x4,
  ReadWrite = 0x8,
  Indirect = 0x10,
};

BITMASK_OPERATORS(BufferCategory);
DECLARE_REFLECTION_ENUM(BufferCategory);

DOCUMENT(R"(A set of flags for D3D buffer view properties.

.. data:: NoFlags

  The buffer will not be used for any of the uses below.

.. data:: Raw

  The buffer is used as a raw (byte-addressed) buffer.

.. data:: Append

  The buffer is used as a append/consume view.

.. data:: Counter

  The buffer is used with a structured buffer with associated hidden counter.
)");
enum class D3DBufferViewFlags : uint32_t
{
  NoFlags = 0x0,
  Raw = 0x1,
  Append = 0x2,
  Counter = 0x4,
};

BITMASK_OPERATORS(D3DBufferViewFlags);
DECLARE_REFLECTION_ENUM(D3DBufferViewFlags);

DOCUMENT(R"(A set of flags describing how this texture may be used

.. data:: NoFlags

  The texture will not be used for any of the uses below.

.. data:: ShaderRead

  The texture will be read by a shader.

.. data:: ColorTarget

  The texture will be written to as a color target.

.. data:: DepthTarget

  The texture will be written to and tested against as a depth target.

.. data:: ShaderReadWrite

  The texture will be read and written to by a shader.

.. data:: SwapBuffer

  The texture is part of a window swapchain.
)");
enum class TextureCategory : uint32_t
{
  NoFlags = 0x0,
  ShaderRead = 0x1,
  ColorTarget = 0x2,
  DepthTarget = 0x4,
  ShaderReadWrite = 0x8,
  SwapBuffer = 0x10,
};

BITMASK_OPERATORS(TextureCategory);
DECLARE_REFLECTION_ENUM(TextureCategory);

DOCUMENT(R"(A set of flags for ``ShaderStage`` stages

.. data:: Unknown

  No flags set for any shader stages.

.. data:: Vertex

  The flag for :data:`ShaderStage.Vertex`.

.. data:: Hull

  The flag for :data:`ShaderStage.Hull`.

.. data:: Tess_Control

  The flag for :data:`ShaderStage.Tess_Control`.

.. data:: Domain

  The flag for :data:`ShaderStage.Domain`.

.. data:: Tess_Eval

  The flag for :data:`ShaderStage.Tess_Eval`.

.. data:: Geometry

  The flag for :data:`ShaderStage.Geometry`.

.. data:: Pixel

  The flag for :data:`ShaderStage.Pixel`.

.. data:: Fragment

  The flag for :data:`ShaderStage.Fragment`.

.. data:: Compute

  The flag for :data:`ShaderStage.Compute`.

.. data:: All

  A shorthand version with flags set for all stages together.
)");
enum class ShaderStageMask : uint32_t
{
  Unknown = 0,
  Vertex = 1 << uint32_t(ShaderStage::Vertex),
  Hull = 1 << uint32_t(ShaderStage::Hull),
  Tess_Control = Hull,
  Domain = 1 << uint32_t(ShaderStage::Domain),
  Tess_Eval = Domain,
  Geometry = 1 << uint32_t(ShaderStage::Geometry),
  Pixel = 1 << uint32_t(ShaderStage::Pixel),
  Fragment = Pixel,
  Compute = 1 << uint32_t(ShaderStage::Compute),
  All = Vertex | Hull | Domain | Geometry | Pixel | Compute,
};

BITMASK_OPERATORS(ShaderStageMask);
DECLARE_REFLECTION_ENUM(ShaderStageMask);

DOCUMENT(R"(Calculate the corresponding flag for a shader stage

:param ShaderStage stage: The shader stage
:return: The flag that corresponds to the input shader stage
:rtype: ShaderStageMask
)");
constexpr inline ShaderStageMask MaskForStage(ShaderStage stage)
{
  return ShaderStageMask(1 << uint32_t(stage));
}

DOCUMENT(R"(A set of flags for events that may occur while debugging a shader

.. data:: NoEvent

  No event has occurred.

.. data:: SampleLoadGather

  A texture was sampled, loaded or gathered.

.. data:: GeneratedNanOrInf

  A floating point operation generated a ``NaN`` or ``infinity`` result.
)");
enum class ShaderEvents : uint32_t
{
  NoEvent = 0,
  SampleLoadGather = 0x1,
  GeneratedNanOrInf = 0x2,
};

BITMASK_OPERATORS(ShaderEvents);
DECLARE_REFLECTION_ENUM(ShaderEvents);

DOCUMENT(R"(A set of flags describing the properties of a particular drawcall.

.. data:: NoFlags

  The drawcall has no special properties.

.. data:: Clear

  The drawcall is a clear call. See :data:`ClearColor` and :data:`ClearDepthStencil`.

.. data:: Drawcall

  The drawcall renders primitives using the graphics pipeline.

.. data:: Dispatch

  The drawcall issues a number of compute workgroups.

.. data:: CmdList

  The drawcall calls into a previously recorded child command list.

.. data:: SetMarker

  The drawcall inserts a single debugging marker.

.. data:: PushMarker

  The drawcall begins a debugging marker region that has children.

.. data:: PopMarker

  The drawcall ends a debugging marker region.

  .. note::

    Drawcalls with this flag will not be exposed and it is only used internally for tracking
    markers.

.. data:: Present

  The drawcall is a presentation call that hands a swapchain image to the presentation engine.

.. data:: MultiDraw

  The drawcall is a multi-draw that contains several specified child draws.

.. data:: Copy

  The drawcall performs a resource copy operation.

.. data:: Resolve

  The drawcall performs a resource resolve or blit operation.

.. data:: GenMips

  The drawcall performs a resource mip-generation operation.

.. data:: PassBoundary

  The drawcall marks the beginning or end of a render pass. See :data:`BeginPass` and
  :data:`EndPass`.

.. data:: UseIBuffer

  The drawcall uses an index buffer.

.. data:: Instanced

  The drawcall uses instancing. This does not mean it renders more than one instanced, simply that
  it uses the instancing feature.

.. data:: Auto

  The drawcall interacts with stream-out to render all vertices previously written. This is a
  Direct3D 11 specific feature.

.. data:: Indirect

  The drawcall uses a buffer on the GPU to source some or all of its parameters in an indirect way.

.. data:: ClearColor

  The drawcall clears a color target.

.. data:: ClearDepthStencil

  The drawcall clears a depth-stencil target.

.. data:: BeginPass

  The drawcall marks the beginning of a render pass.

.. data:: EndPass

  The drawcall marks the end of a render pass.

.. data:: APICalls

  The drawcall does not contain any work directly, but is a 'virtual' draw inserted to encompass
  non-draw API calls that happened within a region, so they are included within the region where
  they occurred and not grouped into the next drawcall outside that region.
)");
enum class DrawFlags : uint32_t
{
  NoFlags = 0x0000,

  // types
  Clear = 0x0001,
  Drawcall = 0x0002,
  Dispatch = 0x0004,
  CmdList = 0x0008,
  SetMarker = 0x0010,
  PushMarker = 0x0020,
  PopMarker = 0x0040,    // this is only for internal tracking use
  Present = 0x0080,
  MultiDraw = 0x0100,
  Copy = 0x0200,
  Resolve = 0x0400,
  GenMips = 0x0800,
  PassBoundary = 0x1000,

  // flags
  Indexed = 0x010000,
  Instanced = 0x020000,
  Auto = 0x040000,
  Indirect = 0x080000,
  ClearColor = 0x100000,
  ClearDepthStencil = 0x200000,
  BeginPass = 0x400000,
  EndPass = 0x800000,
  APICalls = 0x1000000,
};

BITMASK_OPERATORS(DrawFlags);
DECLARE_REFLECTION_ENUM(DrawFlags);

DOCUMENT(R"(A set of flags giving details of the current status of vulkan layer registration.

.. data:: NoFlags

  There are no problems with the vulkan layer registration.

.. data:: OtherInstallsRegistered

  Other conflicting installs of the same layer in other locations are registered.

.. data:: ThisInstallRegistered

  This current install of the layer is registered.

.. data:: NeedElevation

  Fixing any issues will require elevation to system administrator privileges.

.. data:: CouldElevate

  Fixing issues could be done purely as a user, but can optionally be done at system level with
  system administrator privileges.

.. data:: RegisterAll

  All listed locations for the current layer must be registered for correct functioning.

  If this flag is not set, then the listed locations are an 'or' list of alternatives.

.. data:: UpdateAllowed

  If the current registrations can be updated or re-pointed to fix the issues.

.. data:: Unfixable

  The current situation is not fixable automatically and requires user intervention/disambiguation.
)");
enum class VulkanLayerFlags : uint32_t
{
  NoFlags = 0x0,
  OtherInstallsRegistered = 0x1,
  ThisInstallRegistered = 0x2,
  NeedElevation = 0x4,
  CouldElevate = 0x8,
  RegisterAll = 0x10,
  UpdateAllowed = 0x20,
  Unfixable = 0x40,
};

BITMASK_OPERATORS(VulkanLayerFlags);
DECLARE_REFLECTION_ENUM(VulkanLayerFlags);

DOCUMENT(R"(A set of flags giving details of the current status of Android tracability.

.. data:: NoFlags

  There are no problems with the Android application setup.

.. data:: Debuggable

  The application is debuggable.

.. data:: RootAccess

   The device being targeted has root access.

.. data:: MissingTools

   When patching, some necessary tools were not found.

.. data:: ManifestPatchFailure

   When patching, modifying the manifest file to include the debuggable flag failed.

.. data:: RepackagingAPKFailure

   When patching, repackaging, signing and installing the new package failed.
)");
enum class AndroidFlags : uint32_t
{
  NoFlags = 0x0,
  Debuggable = 0x1,
  RootAccess = 0x2,
  MissingTools = 0x1000,
  ManifestPatchFailure = 0x2000,
  RepackagingAPKFailure = 0x4000,
};

BITMASK_OPERATORS(AndroidFlags);
DECLARE_REFLECTION_ENUM(AndroidFlags);

#if defined(DISABLE_PYTHON_FLAG_ENUMS)
DISABLE_PYTHON_FLAG_ENUMS;
#endif