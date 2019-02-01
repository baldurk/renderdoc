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

#include "common_pipestate.h"
#include "data_types.h"

namespace GLPipe
{
DOCUMENT(R"(Describes the configuration for a single vertex attribute.

.. note:: If old-style vertex attrib pointer setup was used for the vertex attributes then it will
  be decomposed into 1:1 attributes and buffers.
)");
struct VertexAttribute
{
  DOCUMENT("");
  VertexAttribute() = default;
  VertexAttribute(const VertexAttribute &) = default;

  bool operator==(const VertexAttribute &o) const
  {
    return enabled == o.enabled && format == o.format &&
           !memcmp(&genericValue, &o.genericValue, sizeof(genericValue)) &&
           vertexBufferSlot == o.vertexBufferSlot && byteOffset == o.byteOffset;
  }
  bool operator<(const VertexAttribute &o) const
  {
    if(!(enabled == o.enabled))
      return enabled < o.enabled;
    if(!(format == o.format))
      return format < o.format;
    if(memcmp(&genericValue, &o.genericValue, sizeof(genericValue)) < 0)
      return true;
    if(!(vertexBufferSlot == o.vertexBufferSlot))
      return vertexBufferSlot < o.vertexBufferSlot;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    return false;
  }
  DOCUMENT("``True`` if this vertex attribute is enabled.");
  bool enabled = false;
  DOCUMENT("The :class:`ResourceFormat` of the vertex attribute.");
  ResourceFormat format;

  DOCUMENT("A :class:`PixelValue` containing the generic value of a vertex attribute.");
  PixelValue genericValue;

  DOCUMENT("The vertex buffer input slot where the data is sourced from.");
  uint32_t vertexBufferSlot = 0;
  DOCUMENT(R"(The byte offset from the start of the vertex data in the vertex buffer from
:data:`vertexBufferSlot`.
)");
  uint32_t byteOffset = 0;
};

DOCUMENT("Describes a single OpenGL vertex buffer binding.")
struct VertexBuffer
{
  DOCUMENT("");
  VertexBuffer() = default;
  VertexBuffer(const VertexBuffer &) = default;

  bool operator==(const VertexBuffer &o) const
  {
    return resourceId == o.resourceId && byteStride == o.byteStride && byteOffset == o.byteOffset &&
           instanceDivisor == o.instanceDivisor;
  }
  bool operator<(const VertexBuffer &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(byteStride == o.byteStride))
      return byteStride < o.byteStride;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(instanceDivisor == o.instanceDivisor))
      return instanceDivisor < o.instanceDivisor;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer bound to this slot.");
  ResourceId resourceId;

  DOCUMENT("The byte stride between the start of one set of vertex data and the next.");
  uint32_t byteStride = 0;
  DOCUMENT("The byte offset from the start of the buffer to the beginning of the vertex data.");
  uint32_t byteOffset = 0;
  DOCUMENT(R"(The instance rate divisor.

If this is ``0`` then the vertex buffer is read at vertex rate.

If it's ``1`` then one element is read for each instance, and for ``N`` greater than ``1`` then
``N`` instances read the same element before advancing.
)");
  uint32_t instanceDivisor = 0;
};

DOCUMENT("Describes the setup for fixed-function vertex input fetch.");
struct VertexInput
{
  DOCUMENT("");
  VertexInput() = default;
  VertexInput(const VertexInput &) = default;

  DOCUMENT("The :class:`ResourceId` of the vertex array object that's bound.");
  ResourceId vertexArrayObject;

  DOCUMENT("A list of :class:`GLVertexAttribute` with the vertex attributes.");
  rdcarray<VertexAttribute> attributes;

  DOCUMENT("A list of :class:`GLVertexBuffer` with the vertex buffers.");
  rdcarray<VertexBuffer> vertexBuffers;

  DOCUMENT("The :class:`ResourceId` of the index buffer.");
  ResourceId indexBuffer;
  DOCUMENT("``True`` if primitive restart is enabled for strip primitives.");
  bool primitiveRestart = false;
  DOCUMENT("The index value to use to indicate a strip restart.");
  uint32_t restartIndex = 0;

  DOCUMENT(R"(``True`` if the provoking vertex is the last one in the primitive.

``False`` if the provoking vertex is the first one.
)");
  bool provokingVertexLast = false;
};

DOCUMENT("Describes an OpenGL shader stage.");
struct Shader
{
  DOCUMENT("");
  Shader() = default;
  Shader(const Shader &) = default;

  DOCUMENT("The :class:`ResourceId` of the shader object itself.");
  ResourceId shaderResourceId;

  DOCUMENT("The :class:`ResourceId` of the program bound to this stage.");
  ResourceId programResourceId;

  DOCUMENT("A :class:`ShaderReflection` describing the reflection data for this shader.");
  ShaderReflection *reflection = NULL;
  DOCUMENT(R"(A :class:`ShaderBindpointMapping` to match :data:`reflection` with the bindpoint
mapping data.
)");
  ShaderBindpointMapping bindpointMapping;

  DOCUMENT("A :class:`ShaderStage` identifying which stage this shader is bound to.");
  ShaderStage stage = ShaderStage::Vertex;

  DOCUMENT("A list of integers with the subroutine values.");
  rdcarray<uint32_t> subroutines;
};

DOCUMENT("Describes the setup for fixed vertex processing operations.");
struct FixedVertexProcessing
{
  DOCUMENT("");
  FixedVertexProcessing() = default;
  FixedVertexProcessing(const FixedVertexProcessing &) = default;

  DOCUMENT("A list of ``float`` giving the default inner level of tessellation.");
  float defaultInnerLevel[2] = {0.0f, 0.0f};
  DOCUMENT("A list of ``float`` giving the default outer level of tessellation.");
  float defaultOuterLevel[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  DOCUMENT("``True`` if primitives should be discarded during rasterization.");
  bool discard = false;

  DOCUMENT("A list of ``bool`` determining which user clipping planes are enabled.");
  bool clipPlanes[8] = {false, false, false, false, false, false, false, false};
  DOCUMENT(R"(``True`` if the clipping origin should be in the lower left.

``False`` if it's in the upper left.
)");
  bool clipOriginLowerLeft = false;
  DOCUMENT(R"(``True`` if the clip-space Z goes from ``-1`` to ``1``.

``False`` if the clip-space Z goes from ``0`` to ``1``.
)");
  bool clipNegativeOneToOne = false;
};

DOCUMENT("Describes the details of a texture.");
struct Texture
{
  DOCUMENT("");
  Texture() = default;
  Texture(const Texture &) = default;

  bool operator==(const Texture &o) const
  {
    return resourceId == o.resourceId && firstMip == o.firstMip && numMips == o.numMips &&
           type == o.type && swizzle[0] == o.swizzle[0] && swizzle[1] == o.swizzle[1] &&
           swizzle[2] == o.swizzle[2] && swizzle[3] == o.swizzle[3] &&
           depthReadChannel == o.depthReadChannel;
  }
  bool operator<(const Texture &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(firstMip == o.firstMip))
      return firstMip < o.firstMip;
    if(!(numMips == o.numMips))
      return numMips < o.numMips;
    if(!(type == o.type))
      return type < o.type;
    if(!(swizzle[0] == o.swizzle[0]))
      return swizzle[0] < o.swizzle[0];
    if(!(swizzle[1] == o.swizzle[1]))
      return swizzle[1] < o.swizzle[1];
    if(!(swizzle[2] == o.swizzle[2]))
      return swizzle[2] < o.swizzle[2];
    if(!(swizzle[3] == o.swizzle[3]))
      return swizzle[3] < o.swizzle[3];
    if(!(depthReadChannel == o.depthReadChannel))
      return depthReadChannel < o.depthReadChannel;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the underlying resource the view refers to.");
  ResourceId resourceId;
  DOCUMENT("Valid for textures - the first mip that is available.");
  uint32_t firstMip = 0;
  DOCUMENT("Valid for textures - the number of mips that are available.");
  uint32_t numMips = 0;
  DOCUMENT("The :class:`TextureType` of the texture.");
  TextureType type = TextureType::Unknown;

  DOCUMENT("Four :class:`TextureSwizzle` elements indicating the swizzle applied to this texture.");
  TextureSwizzle swizzle[4] = {TextureSwizzle::Red, TextureSwizzle::Green, TextureSwizzle::Blue,
                               TextureSwizzle::Alpha};
  DOCUMENT(R"(The channel to read from in a depth-stencil texture.

``-1`` for non depth-stencil textures.

``0`` if depth should be read.

``1`` if stencil should be read.
)");
  int32_t depthReadChannel = -1;
};

DOCUMENT("Describes the sampler properties of a texture.");
struct Sampler
{
  DOCUMENT("");
  Sampler() = default;
  Sampler(const Sampler &) = default;

  bool operator==(const Sampler &o) const
  {
    return resourceId == o.resourceId && addressS == o.addressS && addressT == o.addressT &&
           addressR == o.addressR && borderColor[0] == o.borderColor[0] &&
           borderColor[1] == o.borderColor[1] && borderColor[2] == o.borderColor[2] &&
           borderColor[3] == o.borderColor[3] && compareFunction == o.compareFunction &&
           filter == o.filter && seamlessCubeMap == o.seamlessCubeMap &&
           maxAnisotropy == o.maxAnisotropy && maxLOD == o.maxLOD && minLOD == o.minLOD &&
           mipLODBias == o.mipLODBias;
  }
  bool operator<(const Sampler &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(addressS == o.addressS))
      return addressS < o.addressS;
    if(!(addressT == o.addressT))
      return addressT < o.addressT;
    if(!(addressR == o.addressR))
      return addressR < o.addressR;
    if(!(borderColor[0] == o.borderColor[0]))
      return borderColor[0] < o.borderColor[0];
    if(!(borderColor[1] == o.borderColor[1]))
      return borderColor[1] < o.borderColor[1];
    if(!(borderColor[2] == o.borderColor[2]))
      return borderColor[2] < o.borderColor[2];
    if(!(borderColor[3] == o.borderColor[3]))
      return borderColor[3] < o.borderColor[3];
    if(!(compareFunction == o.compareFunction))
      return compareFunction < o.compareFunction;
    if(!(filter == o.filter))
      return filter < o.filter;
    if(!(seamlessCubeMap == o.seamlessCubeMap))
      return seamlessCubeMap < o.seamlessCubeMap;
    if(!(maxAnisotropy == o.maxAnisotropy))
      return maxAnisotropy < o.maxAnisotropy;
    if(!(maxLOD == o.maxLOD))
      return maxLOD < o.maxLOD;
    if(!(minLOD == o.minLOD))
      return minLOD < o.minLOD;
    if(!(mipLODBias == o.mipLODBias))
      return mipLODBias < o.mipLODBias;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the sampler object, if a separate one is set.");
  ResourceId resourceId;
  DOCUMENT("The :class:`AddressMode` in the S direction.");
  AddressMode addressS = AddressMode::Wrap;
  DOCUMENT("The :class:`AddressMode` in the T direction.");
  AddressMode addressT = AddressMode::Wrap;
  DOCUMENT("The :class:`AddressMode` in the R direction.");
  AddressMode addressR = AddressMode::Wrap;
  DOCUMENT("The RGBA border color.");
  float borderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  DOCUMENT("The :class:`CompareFunction` for comparison samplers.");
  CompareFunction compareFunction = CompareFunction::AlwaysTrue;
  DOCUMENT("The :class:`TextureFilter` describing the filtering mode.");
  TextureFilter filter;
  DOCUMENT("``True`` if seamless cubemap filtering is enabled for this texture.");
  bool seamlessCubeMap = false;
  DOCUMENT("The maximum anisotropic filtering level to use.");
  float maxAnisotropy = 0.0f;
  DOCUMENT("The maximum mip level that can be used.");
  float maxLOD = 0.0f;
  DOCUMENT("The minimum mip level that can be used.");
  float minLOD = 0.0f;
  DOCUMENT("A bias to apply to the calculated mip level before sampling.");
  float mipLODBias = 0.0f;

  DOCUMENT(R"(Check if the border color is used in this OpenGL sampler.

:return: ``True`` if the border color is used, ``False`` otherwise.
:rtype: ``bool``
)");
  bool UseBorder() const
  {
    return addressS == AddressMode::ClampBorder || addressT == AddressMode::ClampBorder ||
           addressR == AddressMode::ClampBorder;
  }
};

DOCUMENT("Describes the properties of a buffer.");
struct Buffer
{
  DOCUMENT("");
  Buffer() = default;
  Buffer(const Buffer &) = default;

  bool operator==(const Buffer &o) const
  {
    return resourceId == o.resourceId && byteOffset == o.byteOffset && byteSize == o.byteSize;
  }
  bool operator<(const Buffer &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(byteSize == o.byteSize))
      return byteSize < o.byteSize;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer object.");
  ResourceId resourceId;
  DOCUMENT("The byte offset from the start of the buffer.");
  uint64_t byteOffset = 0;
  DOCUMENT("The byte size of the buffer.");
  uint64_t byteSize = 0;
};

DOCUMENT("Describes the properties of a load/store image.");
struct ImageLoadStore
{
  DOCUMENT("");
  ImageLoadStore() = default;
  ImageLoadStore(const ImageLoadStore &) = default;

  bool operator==(const ImageLoadStore &o) const
  {
    return resourceId == o.resourceId && mipLevel == o.mipLevel && layered == o.layered &&
           slice == o.slice && type == o.type && readAllowed == o.readAllowed &&
           writeAllowed == o.writeAllowed && imageFormat == o.imageFormat;
  }
  bool operator<(const ImageLoadStore &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(mipLevel == o.mipLevel))
      return mipLevel < o.mipLevel;
    if(!(layered == o.layered))
      return layered < o.layered;
    if(!(slice == o.slice))
      return slice < o.slice;
    if(!(type == o.type))
      return type < o.type;
    if(!(readAllowed == o.readAllowed))
      return readAllowed < o.readAllowed;
    if(!(writeAllowed == o.writeAllowed))
      return writeAllowed < o.writeAllowed;
    if(!(imageFormat == o.imageFormat))
      return imageFormat < o.imageFormat;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the texture object.");
  ResourceId resourceId;
  DOCUMENT("The mip of the texture that's used in the attachment.");
  uint32_t mipLevel = 0;
  DOCUMENT(R"(``True`` if multiple layers are bound together to the image.
``False`` if only one layer is bound.
)");
  bool layered = false;
  DOCUMENT("The slice of the texture that's used in the attachment.");
  uint32_t slice = 0;
  DOCUMENT("The :class:`TextureType` of the texture.");
  TextureType type = TextureType::Unknown;
  DOCUMENT("``True`` if loading from the image is allowed.");
  bool readAllowed = false;
  DOCUMENT("``True`` if storing to the image is allowed.");
  bool writeAllowed = false;
  DOCUMENT("The :class:`ResourceFormat` that the image is bound as.");
  ResourceFormat imageFormat;
};

DOCUMENT("Describes the current feedback state.");
struct Feedback
{
  DOCUMENT("");
  Feedback() = default;
  Feedback(const Feedback &) = default;

  DOCUMENT("The :class:`ResourceId` of the transform feedback binding.");
  ResourceId feedbackResourceId;
  DOCUMENT("A list of :class:`ResourceId` with the buffer bindings.");
  ResourceId bufferResourceId[4];
  DOCUMENT("A list of ``int`` with the buffer byte offsets.");
  uint64_t byteOffset[4] = {0, 0, 0, 0};
  DOCUMENT("A list of ``int`` with the buffer byte sizes.");
  uint64_t byteSize[4] = {0, 0, 0, 0};
  DOCUMENT("``True`` if the transform feedback object is currently active.");
  bool active = false;
  DOCUMENT("``True`` if the transform feedback object is currently paused.");
  bool paused = false;
};

DOCUMENT("Describes the rasterizer state toggles.");
struct RasterizerState
{
  DOCUMENT("");
  RasterizerState() = default;
  RasterizerState(const RasterizerState &) = default;

  DOCUMENT("The polygon :class:`FillMode`.");
  FillMode fillMode = FillMode::Solid;
  DOCUMENT("The polygon :class:`CullMode`.");
  CullMode cullMode = CullMode::NoCull;
  DOCUMENT(R"(``True`` if counter-clockwise polygons are front-facing.
``False`` if clockwise polygons are front-facing.
)");
  bool frontCCW = false;
  DOCUMENT("The fixed depth bias value to apply to z-values.");
  float depthBias = 0.0f;
  DOCUMENT("The slope-scaled depth bias value to apply to z-values.");
  float slopeScaledDepthBias = 0.0f;
  DOCUMENT(R"(The clamp value for calculated depth bias from :data:`depthBias` and
:data:`slopeScaledDepthBias`
)");
  float offsetClamp = 0.0f;
  DOCUMENT(R"(``True`` if pixels outside of the near and far depth planes should be clamped and
to ``0.0`` to ``1.0`` and not clipped.
)");
  bool depthClamp = false;

  DOCUMENT("``True`` if multisampling should be used during rendering.");
  bool multisampleEnable = false;
  DOCUMENT("``True`` if rendering should happen at sample-rate frequency.");
  bool sampleShading = false;
  DOCUMENT(R"(``True`` if the generated samples should be bitwise ``AND`` masked with
:data:`sampleMaskValue`.
)");
  bool sampleMask = false;
  DOCUMENT("The sample mask value that should be masked against the generated coverage.");
  uint32_t sampleMaskValue = ~0U;
  DOCUMENT(R"(``True`` if a temporary mask using :data:`sampleCoverageValue` should be used to
resolve the final output color.
)");
  bool sampleCoverage = false;
  DOCUMENT("``True`` if the temporary sample coverage mask should be inverted.");
  bool sampleCoverageInvert = false;
  DOCUMENT("The sample coverage value used if :data:`sampleCoverage` is ``True``.");
  float sampleCoverageValue = 1.0f;
  DOCUMENT("``True`` if alpha-to-coverage should be used when blending to an MSAA target.");
  bool alphaToCoverage = false;
  DOCUMENT("``True`` if alpha-to-one should be used when blending to an MSAA target.");
  bool alphaToOne = false;
  DOCUMENT("The minimum sample shading rate.");
  float minSampleShadingRate = 0.0f;

  DOCUMENT("``True`` if the point size can be programmably exported from a shader.");
  bool programmablePointSize = false;
  DOCUMENT("The fixed point size in pixels.");
  float pointSize = 1.0f;
  DOCUMENT("The fixed line width in pixels.");
  float lineWidth = 1.0f;
  DOCUMENT("The threshold value at which points are clipped if they exceed this size.");
  float pointFadeThreshold = 0.0f;
  DOCUMENT("``True`` if the point sprite texture origin is upper-left. ``False`` if lower-left.");
  bool pointOriginUpperLeft = false;
};

DOCUMENT("Describes the rasterization state of the OpenGL pipeline.");
struct Rasterizer
{
  DOCUMENT("");
  Rasterizer() = default;
  Rasterizer(const Rasterizer &) = default;

  DOCUMENT("A list of :class:`Viewport` with the bound viewports.");
  rdcarray<Viewport> viewports;

  DOCUMENT("A list of :class:`Scissor` with the bound scissor regions.");
  rdcarray<Scissor> scissors;

  DOCUMENT("A :class:`GLRasterizerState` with the details of the rasterization state.");
  RasterizerState state;
};

DOCUMENT("Describes the depth state.");
struct DepthState
{
  DOCUMENT("");
  DepthState() = default;
  DepthState(const DepthState &) = default;

  DOCUMENT("``True`` if depth testing should be performed.");
  bool depthEnable = false;
  DOCUMENT("The :class:`CompareFunction` to use for testing depth values.");
  CompareFunction depthFunction = CompareFunction::AlwaysTrue;
  DOCUMENT("``True`` if depth values should be written to the depth target.");
  bool depthWrites = false;
  DOCUMENT("``True`` if depth bounds tests should be applied.");
  bool depthBounds = false;
  DOCUMENT("The near plane bounding value.");
  double nearBound = 0.0;
  DOCUMENT("The far plane bounding value.");
  double farBound = 0.0;
};

DOCUMENT("Describes the stencil state.");
struct StencilState
{
  DOCUMENT("");
  StencilState() = default;
  StencilState(const StencilState &) = default;

  DOCUMENT("``True`` if stencil operations should be performed.");
  bool stencilEnable = false;

  DOCUMENT("A :class:`StencilFace` describing what happens for front-facing polygons.");
  StencilFace frontFace;
  DOCUMENT("A :class:`StencilFace` describing what happens for back-facing polygons.");
  StencilFace backFace;
};

DOCUMENT("Describes the state of a framebuffer attachment.");
struct Attachment
{
  DOCUMENT("");
  Attachment() = default;
  Attachment(const Attachment &) = default;

  bool operator==(const Attachment &o) const
  {
    return resourceId == o.resourceId && slice == o.slice && mipLevel == o.mipLevel &&
           swizzle[0] == o.swizzle[0] && swizzle[1] == o.swizzle[1] && swizzle[2] == o.swizzle[2] &&
           swizzle[3] == o.swizzle[3];
  }
  bool operator<(const Attachment &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(slice == o.slice))
      return slice < o.slice;
    if(!(mipLevel == o.mipLevel))
      return mipLevel < o.mipLevel;
    if(!(swizzle[0] == o.swizzle[0]))
      return swizzle[0] < o.swizzle[0];
    if(!(swizzle[1] == o.swizzle[1]))
      return swizzle[1] < o.swizzle[1];
    if(!(swizzle[2] == o.swizzle[2]))
      return swizzle[2] < o.swizzle[2];
    if(!(swizzle[3] == o.swizzle[3]))
      return swizzle[3] < o.swizzle[3];
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the texture bound to this attachment.");
  ResourceId resourceId;
  DOCUMENT("The slice of the texture that's used in the attachment.");
  uint32_t slice = 0;
  DOCUMENT("The mip of the texture that's used in the attachment.");
  uint32_t mipLevel = 0;
  DOCUMENT("Four :class:`TextureSwizzle` elements indicating the swizzle applied to this texture.");
  TextureSwizzle swizzle[4] = {TextureSwizzle::Red, TextureSwizzle::Green, TextureSwizzle::Blue,
                               TextureSwizzle::Alpha};
};

DOCUMENT("Describes the contents of a framebuffer object.");
struct FBO
{
  DOCUMENT("");
  FBO() = default;
  FBO(const FBO &) = default;

  DOCUMENT("The :class:`ResourceId` of the framebuffer.");
  ResourceId resourceId;
  DOCUMENT("The list of :class:`GLAttachment` with the framebuffer color attachments.");
  rdcarray<Attachment> colorAttachments;
  DOCUMENT("The :class:`GLAttachment` with the framebuffer depth attachment.");
  Attachment depthAttachment;
  DOCUMENT("The :class:`GLAttachment` with the framebuffer stencil attachment.");
  Attachment stencilAttachment;

  DOCUMENT("The list of draw buffer indices into the :data:`colorAttachments` attachment list.");
  rdcarray<int32_t> drawBuffers;
  DOCUMENT("The read buffer index in the :data:`colorAttachments` attachment list.");
  int32_t readBuffer = 0;
};

DOCUMENT("Describes the blend pipeline state.");
struct BlendState
{
  DOCUMENT("");
  BlendState() = default;
  BlendState(const BlendState &) = default;

  DOCUMENT("A list of :class:`ColorBlend` describing the blend operations for each target.");
  rdcarray<ColorBlend> blends;

  DOCUMENT("The constant blend factor to use in blend equations.");
  float blendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

DOCUMENT("Describes the current state of the framebuffer stage of the pipeline.");
struct FrameBuffer
{
  DOCUMENT("");
  FrameBuffer() = default;
  FrameBuffer(const FrameBuffer &) = default;

  DOCUMENT(
      "``True`` if sRGB correction should be applied when writing to an sRGB-formatted texture.");
  bool framebufferSRGB = false;
  DOCUMENT("``True`` if dithering should be used when writing to color buffers.");
  bool dither = false;

  DOCUMENT("A :class:`GLFBO` with the information about a draw framebuffer.");
  FBO drawFBO;
  DOCUMENT("A :class:`GLFBO` with the information about a read framebuffer.");
  FBO readFBO;

  DOCUMENT("A :class:`GLBlendState` with the details of the blending state.");
  BlendState blendState;
};

DOCUMENT("Describes the current state of GL hints and smoothing.");
struct Hints
{
  DOCUMENT("");
  Hints() = default;
  Hints(const Hints &) = default;

  DOCUMENT("A :class:`QualityHint` with the derivatives hint.");
  QualityHint derivatives = QualityHint::DontCare;
  DOCUMENT("A :class:`QualityHint` with the line smoothing hint.");
  QualityHint lineSmoothing = QualityHint::DontCare;
  DOCUMENT("A :class:`QualityHint` with the polygon smoothing hint.");
  QualityHint polySmoothing = QualityHint::DontCare;
  DOCUMENT("A :class:`QualityHint` with the texture compression hint.");
  QualityHint textureCompression = QualityHint::DontCare;
  DOCUMENT("``True`` if line smoothing is enabled.");
  bool lineSmoothingEnabled = false;
  DOCUMENT("``True`` if polygon smoothing is enabled.");
  bool polySmoothingEnabled = false;
};

DOCUMENT("The full current OpenGL pipeline state.");
struct State
{
#if !defined(RENDERDOC_EXPORTS)
  // disallow creation/copy of this object externally
  State() = delete;
  State(const State &) = delete;
#endif

  DOCUMENT("A :class:`GLVertexInput` describing the vertex input stage.");
  VertexInput vertexInput;

  DOCUMENT("A :class:`GLShader` describing the vertex shader stage.");
  Shader vertexShader;
  DOCUMENT("A :class:`GLShader` describing the tessellation control shader stage.");
  Shader tessControlShader;
  DOCUMENT("A :class:`GLShader` describing the tessellation evaluation shader stage.");
  Shader tessEvalShader;
  DOCUMENT("A :class:`GLShader` describing the geometry shader stage.");
  Shader geometryShader;
  DOCUMENT("A :class:`GLShader` describing the fragment shader stage.");
  Shader fragmentShader;
  DOCUMENT("A :class:`GLShader` describing the compute shader stage.");
  Shader computeShader;

  DOCUMENT("The :class:`ResourceId` of the program pipeline (if active).");
  ResourceId pipelineResourceId;

  DOCUMENT(
      "A :class:`GLFixedVertexProcessing` describing the fixed-function vertex processing stage.");
  FixedVertexProcessing vertexProcessing;

  DOCUMENT("A list of :class:`GLTexture` with the currently bound textures.");
  rdcarray<Texture> textures;
  DOCUMENT("A list of :class:`GLSampler` with the currently bound samplers.");
  rdcarray<Sampler> samplers;

  DOCUMENT("A list of :class:`GLBuffer` with the currently bound atomic buffers.");
  rdcarray<Buffer> atomicBuffers;
  DOCUMENT("A list of :class:`GLBuffer` with the currently bound uniform buffers.");
  rdcarray<Buffer> uniformBuffers;
  DOCUMENT("A list of :class:`GLBuffer` with the currently bound shader storage buffers.");
  rdcarray<Buffer> shaderStorageBuffers;

  DOCUMENT("A list of :class:`GLImageLoadStore` with the currently bound load/store images.");
  rdcarray<ImageLoadStore> images;

  DOCUMENT("A :class:`GLFeedback` describing the transform feedback stage.");
  Feedback transformFeedback;

  DOCUMENT("A :class:`GLRasterizer` describing rasterization.");
  Rasterizer rasterizer;

  DOCUMENT("A :class:`GLDepthState` describing depth processing.");
  DepthState depthState;

  DOCUMENT("A :class:`GLStencilState` describing stencil processing.");
  StencilState stencilState;

  DOCUMENT("A :class:`GLFrameBuffer` describing the framebuffer.");
  FrameBuffer framebuffer;

  DOCUMENT("A :class:`GLHints` describing the hint state.");
  Hints hints;
};

};    // namespace GLPipe

DECLARE_REFLECTION_STRUCT(GLPipe::VertexAttribute);
DECLARE_REFLECTION_STRUCT(GLPipe::VertexBuffer);
DECLARE_REFLECTION_STRUCT(GLPipe::VertexInput);
DECLARE_REFLECTION_STRUCT(GLPipe::Shader);
DECLARE_REFLECTION_STRUCT(GLPipe::FixedVertexProcessing);
DECLARE_REFLECTION_STRUCT(GLPipe::Texture);
DECLARE_REFLECTION_STRUCT(GLPipe::Sampler);
DECLARE_REFLECTION_STRUCT(GLPipe::Buffer);
DECLARE_REFLECTION_STRUCT(GLPipe::ImageLoadStore);
DECLARE_REFLECTION_STRUCT(GLPipe::Feedback);
DECLARE_REFLECTION_STRUCT(GLPipe::RasterizerState);
DECLARE_REFLECTION_STRUCT(GLPipe::Rasterizer);
DECLARE_REFLECTION_STRUCT(GLPipe::DepthState);
DECLARE_REFLECTION_STRUCT(GLPipe::StencilState);
DECLARE_REFLECTION_STRUCT(GLPipe::Attachment);
DECLARE_REFLECTION_STRUCT(GLPipe::FBO);
DECLARE_REFLECTION_STRUCT(GLPipe::BlendState);
DECLARE_REFLECTION_STRUCT(GLPipe::FrameBuffer);
DECLARE_REFLECTION_STRUCT(GLPipe::Hints);
DECLARE_REFLECTION_STRUCT(GLPipe::State);
