/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2026 Baldur Karlsson
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

namespace GLPipe
{
DOCUMENT(R"(
GLVertexAttribute()
GLVertexAttribute(other: GLVertexAttribute)

Describes the configuration for a single vertex attribute.

.. note:: If old-style vertex attrib pointer setup was used for the vertex attributes then it will
  be decomposed into 1:1 attributes and buffers.
)");
struct VertexAttribute
{
  DOCUMENT("");
  VertexAttribute() = default;
  VertexAttribute(const VertexAttribute &) = default;
  VertexAttribute &operator=(const VertexAttribute &) = default;

  bool operator==(const VertexAttribute &o) const
  {
    return enabled == o.enabled && floatCast == o.floatCast &&
           boundShaderInput == o.boundShaderInput && format == o.format &&
           !memcmp(&genericValue, &o.genericValue, sizeof(genericValue)) &&
           vertexBufferSlot == o.vertexBufferSlot && byteOffset == o.byteOffset;
  }
  bool operator<(const VertexAttribute &o) const
  {
    if(!(enabled == o.enabled))
      return enabled < o.enabled;
    if(!(floatCast == o.floatCast))
      return floatCast < o.floatCast;
    if(!(boundShaderInput == o.boundShaderInput))
      return boundShaderInput < o.boundShaderInput;
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
  DOCUMENT(R"(``True`` if this vertex attribute is enabled.

:type: bool
)");
  bool enabled = false;

  DOCUMENT(R"(Only valid for integer formatted attributes, ``True`` if they are cast to float.

This is because they were specified with an integer format but ``glVertexAttribFormat`` (not
``glVertexAttribIFormat``) so they will be cast.

:type: bool
)");
  bool floatCast = false;

  DOCUMENT(R"(This lists which shader input is bound to this attribute, as an index in the
:data:`ShaderReflection.inputSignature` list.

If any value is set to ``-1`` then the attribute is unbound.

:type: int
)");
  int32_t boundShaderInput = -1;

  DOCUMENT(R"(The format describing how the vertex attribute is interpreted.

:type: ResourceFormat
)");
  ResourceFormat format;

  DOCUMENT(R"(The generic value of the vertex attribute if no buffer is bound.

:type: PixelValue
)");
  PixelValue genericValue;

  DOCUMENT(R"(The vertex buffer input slot where the data is sourced from.

:type: int
)");
  uint32_t vertexBufferSlot = 0;
  DOCUMENT(R"(The byte offset from the start of the vertex data in the vertex buffer from
:data:`vertexBufferSlot`.

:type: int
)");
  uint32_t byteOffset = 0;
};

DOCUMENT(R"(
GLVertexBuffer()
GLVertexBuffer(other: GLVertexBuffer)

Describes a single OpenGL vertex buffer binding.
)")
struct VertexBuffer
{
  DOCUMENT("");
  VertexBuffer() = default;
  VertexBuffer(const VertexBuffer &) = default;
  VertexBuffer &operator=(const VertexBuffer &) = default;

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
  DOCUMENT(R"(The :class:`ResourceId` of the buffer bound to this slot.

:type: ResourceId
)");
  ResourceId resourceId;

  DOCUMENT(R"(The byte stride between the start of one set of vertex data and the next.

:type: int
)");
  uint32_t byteStride = 0;
  DOCUMENT(R"(The byte offset from the start of the buffer to the beginning of the vertex data.

:type: int
)");
  uint32_t byteOffset = 0;
  DOCUMENT(R"(The instance rate divisor.

If this is ``0`` then the vertex buffer is read at vertex rate.

If it's ``1`` then one element is read for each instance, and for ``N`` greater than ``1`` then
``N`` instances read the same element before advancing.

:type: int
)");
  uint32_t instanceDivisor = 0;
};

DOCUMENT(R"(
GLVertexInput()
GLVertexInput(other: GLVertexInput)

Describes the setup for fixed-function vertex input fetch.
)");
struct VertexInput
{
  DOCUMENT("");
  VertexInput() = default;
  VertexInput(const VertexInput &) = default;
  VertexInput &operator=(const VertexInput &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the vertex array object that's bound.

:type: ResourceId
)");
  ResourceId vertexArrayObject;

  DOCUMENT(R"(The vertex attributes.

:type: List[GLVertexAttribute]
)");
  rdcarray<VertexAttribute> attributes;

  DOCUMENT(R"(The vertex buffers.

:type: List[GLVertexBuffer]
)");
  rdcarray<VertexBuffer> vertexBuffers;

  DOCUMENT(R"(The :class:`ResourceId` of the index buffer.

:type: ResourceId
)");
  ResourceId indexBuffer;
  DOCUMENT(R"(The byte width of the index buffer - typically 1, 2 or 4 bytes. It can be 0 for
non-indexed draws.

.. note::
  This does not correspond to a real GL state since the index type is specified per-action in the call
  itself. This is an implicit state derived from the last (or current) action at any given event.

:type: int
)");
  uint32_t indexByteStride = 0;
  DOCUMENT(R"(The byte width of the index buffer - typically 1, 2 or 4 bytes.

.. note::
  This does not correspond to a real GL state since the topology is specified per-action in the call
  itself. This is an implicit state derived from the last (or current) action at any given event.

:type: Topology
)");
  Topology topology = Topology::Unknown;
  DOCUMENT(R"(``True`` if primitive restart is enabled for strip primitives.

:type: bool
)");
  bool primitiveRestart = false;
  DOCUMENT(R"(The index value to use to indicate a strip restart.

:type: int
)");
  uint32_t restartIndex = 0;

  DOCUMENT(R"(``True`` if the provoking vertex is the last one in the primitive.

``False`` if the provoking vertex is the first one.

:type: bool
)");
  bool provokingVertexLast = false;
};

DOCUMENT(R"(
GLShader()
GLShader(other: GLShader)

Describes an OpenGL shader stage.
)");
struct Shader
{
  DOCUMENT("");
  Shader() = default;
  Shader(const Shader &) = default;
  Shader &operator=(const Shader &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the shader object itself.

:type: ResourceId
)");
  ResourceId shaderResourceId;

  DOCUMENT(R"(The :class:`ResourceId` of the program bound to this stage.

:type: ResourceId
)");
  ResourceId programResourceId;

  DOCUMENT(R"(The reflection data for this shader.

:type: ShaderReflection
)");
  const ShaderReflection *reflection = NULL;

  DOCUMENT(R"(A :class:`ShaderStage` identifying which stage this shader is bound to.

:type: ShaderStage
)");
  ShaderStage stage = ShaderStage::Vertex;

  DOCUMENT(R"(A list of integers with the subroutine values.

:type: List[int]
)");
  rdcarray<uint32_t> subroutines;
};

DOCUMENT(R"(
GLFixedVertexProcessing()
GLFixedVertexProcessing(other: GLFixedVertexProcessing)

Describes the setup for fixed vertex processing operations.
)");
struct FixedVertexProcessing
{
  DOCUMENT("");
  FixedVertexProcessing() = default;
  FixedVertexProcessing(const FixedVertexProcessing &) = default;
  FixedVertexProcessing &operator=(const FixedVertexProcessing &) = default;

  DOCUMENT(R"(A tuple of ``float`` giving the default inner level of tessellation.

:type: Tuple[float,float]
)");
  rdcfixedarray<float, 2> defaultInnerLevel = {0.0f, 0.0f};
  DOCUMENT(R"(A tuple of ``float`` giving the default outer level of tessellation.

:type: Tuple[float,float,float,float]
)");
  rdcfixedarray<float, 4> defaultOuterLevel = {0.0f, 0.0f, 0.0f, 0.0f};
  DOCUMENT(R"(``True`` if primitives should be discarded during rasterization.

:type: bool
)");
  bool discard = false;

  DOCUMENT(R"(An 8-tuple of ``bool`` determining which user clipping planes are enabled.

:type: Tuple[bool,...]
)");
  rdcfixedarray<bool, 8> clipPlanes = {false, false, false, false, false, false, false, false};
  DOCUMENT(R"(``True`` if the clipping origin should be in the lower left.

``False`` if it's in the upper left.

:type: bool
)");
  bool clipOriginLowerLeft = false;
  DOCUMENT(R"(``True`` if the clip-space Z goes from ``-1`` to ``1``.

``False`` if the clip-space Z goes from ``0`` to ``1``.

:type: bool
)");
  bool clipNegativeOneToOne = false;
};

DOCUMENT(R"(
GLTextureCompleteness()
GLTextureCompleteness(other: GLTextureCompleteness)

Describes the a texture completeness issue of a descriptor.
)");
struct TextureCompleteness
{
  DOCUMENT("");
  TextureCompleteness() = default;
  TextureCompleteness(const TextureCompleteness &) = default;
  TextureCompleteness &operator=(const TextureCompleteness &) = default;

  bool operator==(const TextureCompleteness &o) const
  {
    return descriptorByteOffset == o.descriptorByteOffset && completeStatus == o.completeStatus;
  }
  bool operator<(const TextureCompleteness &o) const
  {
    return descriptorByteOffset < o.descriptorByteOffset;
  }

  DOCUMENT(R"(The byte offset in the GL descriptor storage of the problematic descriptor

:type: int
)");
  uint64_t descriptorByteOffset = 0;

  DOCUMENT(R"(The details of the texture's (in)completeness. If this string is empty, the texture is
complete. Otherwise it contains an explanation of why the texture is believed to be incomplete.

:type: str
)");
  rdcstr completeStatus;

  DOCUMENT(R"(The details of any type conflict on this binding. This can happen if
multiple uniforms are pointing to the same binding but with different types. In this case it is
impossible to disambiguate which binding was used.

If this string is empty, no conflict is present. Otherwise it contains the bindings which are
in conflict and their types.

:type: str
)");
  rdcstr typeConflict;
};

DOCUMENT(R"(
GLFeedback()
GLFeedback(other: GLFeedback)

Describes the current feedback state.
)");
struct Feedback
{
  DOCUMENT("");
  Feedback() = default;
  Feedback(const Feedback &) = default;
  Feedback &operator=(const Feedback &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the transform feedback binding.

:type: ResourceId
)");
  ResourceId feedbackResourceId;
  DOCUMENT(R"(The buffer bindings.
  
:type: Tuple[ResourceId,ResourceId,ResourceId,ResourceId]
)");
  rdcfixedarray<ResourceId, 4> bufferResourceId;
  DOCUMENT(R"(The buffer byte offsets.
  
:type: Tuple[int,int,int,int]
)");
  rdcfixedarray<uint64_t, 4> byteOffset = {0, 0, 0, 0};
  DOCUMENT(R"(The buffer byte sizes.
  
:type: Tuple[int,int,int,int]
)");
  rdcfixedarray<uint64_t, 4> byteSize = {0, 0, 0, 0};
  DOCUMENT(R"(``True`` if the transform feedback object is currently active.

:type: bool
)");
  bool active = false;
  DOCUMENT(R"(``True`` if the transform feedback object is currently paused.

:type: bool
)");
  bool paused = false;
};

DOCUMENT(R"(
GLRasterizerState()
GLRasterizerState(other: GLRasterizerState)

Describes the rasterizer state toggles.
)");
struct RasterizerState
{
  DOCUMENT("");
  RasterizerState() = default;
  RasterizerState(const RasterizerState &) = default;
  RasterizerState &operator=(const RasterizerState &) = default;

  DOCUMENT(R"(The polygon :class:`FillMode`.

:type: FillMode
)");
  FillMode fillMode = FillMode::Solid;
  DOCUMENT(R"(The polygon :class:`CullMode`.

:type: CullMode
)");
  CullMode cullMode = CullMode::NoCull;
  DOCUMENT(R"(``True`` if counter-clockwise polygons are front-facing.
``False`` if clockwise polygons are front-facing.

:type: bool
)");
  bool frontCCW = false;
  DOCUMENT(R"(The fixed depth bias value to apply to z-values.

:type: float
)");
  float depthBias = 0.0f;
  DOCUMENT(R"(The slope-scaled depth bias value to apply to z-values.

:type: float
)");
  float slopeScaledDepthBias = 0.0f;
  DOCUMENT(R"(The clamp value for calculated depth bias from :data:`depthBias` and
:data:`slopeScaledDepthBias`

:type: float
)");
  float offsetClamp = 0.0f;
  DOCUMENT(R"(``True`` if pixels outside of the near and far depth planes should be clamped and
to ``0.0`` to ``1.0`` and not clipped.

:type: bool
)");
  bool depthClamp = false;

  DOCUMENT(R"(``True`` if multisampling should be used during rendering.

:type: bool
)");
  bool multisampleEnable = false;
  DOCUMENT(R"(``True`` if rendering should happen at sample-rate frequency.

:type: bool
)");
  bool sampleShading = false;
  DOCUMENT(R"(``True`` if the generated samples should be bitwise ``AND`` masked with
:data:`sampleMaskValue`.

:type: bool
)");
  bool sampleMask = false;
  DOCUMENT(R"(The sample mask value that should be masked against the generated coverage.

:type: int
)");
  uint32_t sampleMaskValue = ~0U;
  DOCUMENT(R"(``True`` if a temporary mask using :data:`sampleCoverageValue` should be used to
resolve the final output color.

:type: bool
)");
  bool sampleCoverage = false;
  DOCUMENT(R"(``True`` if the temporary sample coverage mask should be inverted.

:type: bool
)");
  bool sampleCoverageInvert = false;
  DOCUMENT(R"(The sample coverage value used if :data:`sampleCoverage` is ``True``.

:type: float
)");
  float sampleCoverageValue = 1.0f;
  DOCUMENT(R"(``True`` if alpha-to-coverage should be used when blending to an MSAA target.

:type: bool
)");
  bool alphaToCoverage = false;
  DOCUMENT(R"(``True`` if alpha-to-one should be used when blending to an MSAA target.

:type: bool
)");
  bool alphaToOne = false;
  DOCUMENT(R"(The minimum sample shading rate.

:type: float
)");
  float minSampleShadingRate = 0.0f;

  DOCUMENT(R"(``True`` if the point size can be programmably exported from a shader.

:type: bool
)");
  bool programmablePointSize = false;
  DOCUMENT(R"(The fixed point size in pixels.

:type: float
)");
  float pointSize = 1.0f;
  DOCUMENT(R"(The fixed line width in pixels.

:type: float
)");
  float lineWidth = 1.0f;
  DOCUMENT(R"(The threshold value at which points are clipped if they exceed this size.

:type: float
)");
  float pointFadeThreshold = 0.0f;
  DOCUMENT(R"(``True`` if the point sprite texture origin is upper-left. ``False`` if lower-left.

:type: bool
)");
  bool pointOriginUpperLeft = false;
};

DOCUMENT(R"(
GLRasterizer()
GLRasterizer(other: GLRasterizer)

Describes the rasterization state of the OpenGL pipeline.
)");
struct Rasterizer
{
  DOCUMENT("");
  Rasterizer() = default;
  Rasterizer(const Rasterizer &) = default;
  Rasterizer &operator=(const Rasterizer &) = default;

  DOCUMENT(R"(The bound viewports.

:type: List[Viewport]
)");
  rdcarray<Viewport> viewports;

  DOCUMENT(R"(The bound scissor regions.

:type: List[Scissor]
)");
  rdcarray<Scissor> scissors;

  DOCUMENT(R"(The details of the rasterization state.

:type: GLRasterizerState
)");
  RasterizerState state;
};

DOCUMENT(R"(
GLDepthState()
GLDepthState(other: GLDepthState)

Describes the depth state.
)");
struct DepthState
{
  DOCUMENT("");
  DepthState() = default;
  DepthState(const DepthState &) = default;
  DepthState &operator=(const DepthState &) = default;

  DOCUMENT(R"(``True`` if depth testing should be performed.

:type: bool
)");
  bool depthEnable = false;
  DOCUMENT(R"(The :class:`CompareFunction` to use for testing depth values.

:type: CompareFunction
)");
  CompareFunction depthFunction = CompareFunction::AlwaysTrue;
  DOCUMENT(R"(``True`` if depth values should be written to the depth target.

:type: bool
)");
  bool depthWrites = false;
  DOCUMENT(R"(``True`` if depth bounds tests should be applied.

:type: bool
)");
  bool depthBounds = false;
  DOCUMENT(R"(The near plane bounding value.

:type: float
)");
  double nearBound = 0.0;
  DOCUMENT(R"(The far plane bounding value.

:type: float
)");
  double farBound = 0.0;
};

DOCUMENT(R"(
GLStencilState()
GLStencilState(other: GLStencilState)

Describes the stencil state.
)");
struct StencilState
{
  DOCUMENT("");
  StencilState() = default;
  StencilState(const StencilState &) = default;
  StencilState &operator=(const StencilState &) = default;

  DOCUMENT(R"(``True`` if stencil operations should be performed.

:type: bool
)");
  bool stencilEnable = false;

  DOCUMENT(R"(The stencil state for front-facing polygons.

:type: StencilFace
)");
  StencilFace frontFace;

  DOCUMENT(R"(The stencil state for back-facing polygons.

:type: StencilFace
)");
  StencilFace backFace;
};

DOCUMENT(R"(
GLFBO()
GLFBO(other: GLFBO)

Describes the contents of a framebuffer object.
)");
struct FBO
{
  DOCUMENT("");
  FBO() = default;
  FBO(const FBO &) = default;
  FBO &operator=(const FBO &) = default;

  DOCUMENT(R"(The :class:`ResourceId` of the framebuffer.

:type: ResourceId
)");
  ResourceId resourceId;
  DOCUMENT(R"(The framebuffer color attachments.

:type: List[Descriptor]
)");
  rdcarray<Descriptor> colorAttachments;
  DOCUMENT(R"(The framebuffer depth attachment.

:type: Descriptor
)");
  Descriptor depthAttachment;
  DOCUMENT(R"(The framebuffer stencil attachment.

:type: Descriptor
)");
  Descriptor stencilAttachment;

  DOCUMENT(R"(The draw buffer indices into the :data:`colorAttachments` attachment list.

:type: List[int]
)");
  rdcarray<int32_t> drawBuffers;
  DOCUMENT(R"(The read buffer index in the :data:`colorAttachments` attachment list.

:type: int
)");
  int32_t readBuffer = 0;

  DOCUMENT(R"(The default sample count to use if the framebuffer has no attachments

:type: int
)");
  uint32_t defaultSampleCount = 0;
};

DOCUMENT(R"(
GLBlendState()
GLBlendState(other: GLBlendState)

Describes the blend pipeline state.
)");
struct BlendState
{
  DOCUMENT("");
  BlendState() = default;
  BlendState(const BlendState &) = default;
  BlendState &operator=(const BlendState &) = default;

  DOCUMENT(R"(The blend operations for each target.

:type: List[ColorBlend]
)");
  rdcarray<ColorBlend> blends;

  DOCUMENT(R"(The constant blend factor to use in blend equations.
  
:type: Tuple[float,float,float,float]
)");
  rdcfixedarray<float, 4> blendFactor = {1.0f, 1.0f, 1.0f, 1.0f};
};

DOCUMENT(R"(
GLFrameBuffer()
GLFrameBuffer(other: GLFrameBuffer)

Describes the current state of the framebuffer stage of the pipeline.
)");
struct FrameBuffer
{
  DOCUMENT("");
  FrameBuffer() = default;
  FrameBuffer(const FrameBuffer &) = default;
  FrameBuffer &operator=(const FrameBuffer &) = default;

  DOCUMENT(R"(``True`` if sRGB correction should be applied when writing to an sRGB-formatted texture.

:type: bool
)");
  bool framebufferSRGB = false;
  DOCUMENT(R"(``True`` if dithering should be used when writing to color buffers.

:type: bool
)");
  bool dither = false;

  DOCUMENT(R"(The draw framebuffer.

:type: GLFBO
)");
  FBO drawFBO;
  DOCUMENT(R"(The read framebuffer.

:type: GLFBO
)");
  FBO readFBO;

  DOCUMENT(R"(The details of the blending state.

:type: GLBlendState
)");
  BlendState blendState;
};

DOCUMENT(R"(
GLHints()
GLHints(other: GLHints)

Describes the current state of GL hints and smoothing.
)");
struct Hints
{
  DOCUMENT("");
  Hints() = default;
  Hints(const Hints &) = default;
  Hints &operator=(const Hints &) = default;

  DOCUMENT(R"(A :class:`QualityHint` with the derivatives hint.

:type: QualityHint
)");
  QualityHint derivatives = QualityHint::DontCare;
  DOCUMENT(R"(A :class:`QualityHint` with the line smoothing hint.

:type: QualityHint
)");
  QualityHint lineSmoothing = QualityHint::DontCare;
  DOCUMENT(R"(A :class:`QualityHint` with the polygon smoothing hint.

:type: QualityHint
)");
  QualityHint polySmoothing = QualityHint::DontCare;
  DOCUMENT(R"(A :class:`QualityHint` with the texture compression hint.

:type: QualityHint
)");
  QualityHint textureCompression = QualityHint::DontCare;
  DOCUMENT(R"(``True`` if line smoothing is enabled.

:type: bool
)");
  bool lineSmoothingEnabled = false;
  DOCUMENT(R"(``True`` if polygon smoothing is enabled.

:type: bool
)");
  bool polySmoothingEnabled = false;
};

DOCUMENT(R"(
The full current OpenGL pipeline state.
)");
struct State
{
#if !defined(RENDERDOC_EXPORTS)
  // disallow creation/copy of this object externally
  State() = delete;
  State(const State &) = delete;
#endif

  DOCUMENT(R"(The vertex input stage.

:type: GLVertexInput
)");
  VertexInput vertexInput;

  DOCUMENT(R"(The vertex shader stage.

:type: GLShader
)")
  Shader vertexShader;

  DOCUMENT(R"(The tessellation control shader stage.

:type: GLShader
)");
  Shader tessControlShader;
  DOCUMENT(R"(The tessellation evaluation shader stage.

:type: GLShader
)");
  Shader tessEvalShader;
  DOCUMENT(R"(The geometry shader stage.
   
:type: GLShader
)");
  Shader geometryShader;
  DOCUMENT(R"(The fragment shader stage.
   
:type: GLShader
)");
  Shader fragmentShader;
  DOCUMENT(R"(The compute shader stage.
   
:type: GLShader
)");
  Shader computeShader;

  DOCUMENT(R"(The :class:`ResourceId` of the program pipeline (if active).

:type: ResourceId
)");
  ResourceId pipelineResourceId;

  DOCUMENT(R"(The fixed-function vertex processing stage.

:type: GLFixedVertexProcessing
)");
  FixedVertexProcessing vertexProcessing;

  DOCUMENT(R"(The virtual descriptor storage.

:type: ResourceId
)");
  ResourceId descriptorStore;

  DOCUMENT(R"(The number of descriptors in the virtual descriptor storage.

:type: int
)");
  uint32_t descriptorCount = 0;

  DOCUMENT(R"(The byte size of a descriptor in the virtual descriptor storage.

:type: int
)");
  uint32_t descriptorByteSize = 0;

  DOCUMENT(R"(Texture completeness issues of descriptors in the descriptor store.

:type: GLTextureCompleteness
)");
  rdcarray<TextureCompleteness> textureCompleteness;

  DOCUMENT(R"(The transform feedback stage.

:type: GLFeedback
)");
  Feedback transformFeedback;

  DOCUMENT(R"(The rasterization configuration.

:type: GLRasterizer
)");
  Rasterizer rasterizer;

  DOCUMENT(R"(The depth state.

:type: GLDepthState
)");
  DepthState depthState;

  DOCUMENT(R"(The stencil state.

:type: GLStencilState
)");
  StencilState stencilState;

  DOCUMENT(R"(The bound framebuffer.

:type: GLFrameBuffer
)");
  FrameBuffer framebuffer;

  DOCUMENT(R"(The hint state.

:type: GLHints
)");
  Hints hints;
};

};    // namespace GLPipe

DECLARE_REFLECTION_STRUCT(GLPipe::VertexAttribute);
DECLARE_REFLECTION_STRUCT(GLPipe::VertexBuffer);
DECLARE_REFLECTION_STRUCT(GLPipe::VertexInput);
DECLARE_REFLECTION_STRUCT(GLPipe::Shader);
DECLARE_REFLECTION_STRUCT(GLPipe::FixedVertexProcessing);
DECLARE_REFLECTION_STRUCT(GLPipe::TextureCompleteness);
DECLARE_REFLECTION_STRUCT(GLPipe::Feedback);
DECLARE_REFLECTION_STRUCT(GLPipe::RasterizerState);
DECLARE_REFLECTION_STRUCT(GLPipe::Rasterizer);
DECLARE_REFLECTION_STRUCT(GLPipe::DepthState);
DECLARE_REFLECTION_STRUCT(GLPipe::StencilState);
DECLARE_REFLECTION_STRUCT(GLPipe::FBO);
DECLARE_REFLECTION_STRUCT(GLPipe::BlendState);
DECLARE_REFLECTION_STRUCT(GLPipe::FrameBuffer);
DECLARE_REFLECTION_STRUCT(GLPipe::Hints);
DECLARE_REFLECTION_STRUCT(GLPipe::State);
