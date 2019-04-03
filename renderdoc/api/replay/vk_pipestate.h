/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

namespace VKPipe
{
DOCUMENT("The contents of a single binding element within a descriptor set, possibly in an array.");
struct BindingElement
{
  DOCUMENT("");
  BindingElement() = default;
  BindingElement(const BindingElement &) = default;

  bool operator==(const BindingElement &o) const
  {
    return dynamicallyUsed == o.dynamicallyUsed && viewResourceId == o.viewResourceId &&
           resourceResourceId == o.resourceResourceId && samplerResourceId == o.samplerResourceId &&
           immutableSampler == o.immutableSampler && viewFormat == o.viewFormat &&
           swizzle[0] == o.swizzle[0] && swizzle[1] == o.swizzle[1] && swizzle[2] == o.swizzle[2] &&
           swizzle[3] == o.swizzle[3] && firstMip == o.firstMip && firstSlice == o.firstSlice &&
           numMips == o.numMips && numSlices == o.numSlices && byteOffset == o.byteOffset &&
           byteSize == o.byteSize && filter == o.filter && addressU == o.addressU &&
           addressV == o.addressV && addressW == o.addressW && mipBias == o.mipBias &&
           maxAnisotropy == o.maxAnisotropy && compareFunction == o.compareFunction &&
           minLOD == o.minLOD && maxLOD == o.maxLOD && borderColor[0] == o.borderColor[0] &&
           borderColor[1] == o.borderColor[1] && borderColor[2] == o.borderColor[2] &&
           borderColor[3] == o.borderColor[3] && unnormalized == o.unnormalized;
  }
  bool operator<(const BindingElement &o) const
  {
    if(!(dynamicallyUsed == o.dynamicallyUsed))
      return dynamicallyUsed < o.dynamicallyUsed;
    if(!(viewResourceId == o.viewResourceId))
      return viewResourceId < o.viewResourceId;
    if(!(resourceResourceId == o.resourceResourceId))
      return resourceResourceId < o.resourceResourceId;
    if(!(samplerResourceId == o.samplerResourceId))
      return samplerResourceId < o.samplerResourceId;
    if(!(immutableSampler == o.immutableSampler))
      return immutableSampler < o.immutableSampler;
    if(!(viewFormat == o.viewFormat))
      return viewFormat < o.viewFormat;
    if(!(swizzle[0] == o.swizzle[0]))
      return swizzle[0] < o.swizzle[0];
    if(!(swizzle[1] == o.swizzle[1]))
      return swizzle[1] < o.swizzle[1];
    if(!(swizzle[2] == o.swizzle[2]))
      return swizzle[2] < o.swizzle[2];
    if(!(swizzle[3] == o.swizzle[3]))
      return swizzle[3] < o.swizzle[3];
    if(!(firstMip == o.firstMip))
      return firstMip < o.firstMip;
    if(!(firstSlice == o.firstSlice))
      return firstSlice < o.firstSlice;
    if(!(numMips == o.numMips))
      return numMips < o.numMips;
    if(!(numSlices == o.numSlices))
      return numSlices < o.numSlices;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(byteSize == o.byteSize))
      return byteSize < o.byteSize;
    if(!(filter == o.filter))
      return filter < o.filter;
    if(!(addressU == o.addressU))
      return addressU < o.addressU;
    if(!(addressV == o.addressV))
      return addressV < o.addressV;
    if(!(addressW == o.addressW))
      return addressW < o.addressW;
    if(!(mipBias == o.mipBias))
      return mipBias < o.mipBias;
    if(!(maxAnisotropy == o.maxAnisotropy))
      return maxAnisotropy < o.maxAnisotropy;
    if(!(compareFunction == o.compareFunction))
      return compareFunction < o.compareFunction;
    if(!(minLOD == o.minLOD))
      return minLOD < o.minLOD;
    if(!(maxLOD == o.maxLOD))
      return maxLOD < o.maxLOD;
    if(!(borderColor[0] == o.borderColor[0]))
      return borderColor[0] < o.borderColor[0];
    if(!(borderColor[1] == o.borderColor[1]))
      return borderColor[1] < o.borderColor[1];
    if(!(borderColor[2] == o.borderColor[2]))
      return borderColor[2] < o.borderColor[2];
    if(!(borderColor[3] == o.borderColor[3]))
      return borderColor[3] < o.borderColor[3];
    if(!(unnormalized == o.unnormalized))
      return unnormalized < o.unnormalized;
    return false;
  }

  DOCUMENT("The :class:`ResourceId` of the current view object, if one is in use.");
  ResourceId viewResourceId;    // bufferview, imageview, attachmentview
  DOCUMENT("The :class:`ResourceId` of the current underlying buffer or image object.");
  ResourceId resourceResourceId;    // buffer, image, attachment
  DOCUMENT("The :class:`ResourceId` of the current sampler object.");
  ResourceId samplerResourceId;
  DOCUMENT("``True`` if this is an immutable sampler binding.");
  bool immutableSampler = false;
  DOCUMENT(R"(``True`` if this binding element is dynamically used.

If set to ``False`` this means that the binding was available to the shader but during execution it
was not referenced. The data gathered for setting this variable is conservative, meaning that only
accesses through arrays will have this calculated to reduce the required feedback bandwidth - single
non-arrayed descriptors may have this value set to ``True`` even if the shader did not use them,
since single descriptors may only be dynamically skipped by control flow.
)");
  bool dynamicallyUsed = true;

  DOCUMENT("The :class:`ResourceFormat` that the view uses.");
  ResourceFormat viewFormat;
  DOCUMENT("Four :class:`TextureSwizzle` elements indicating the swizzle applied to this texture.");
  TextureSwizzle swizzle[4] = {TextureSwizzle::Red, TextureSwizzle::Green, TextureSwizzle::Blue,
                               TextureSwizzle::Alpha};

  DOCUMENT("For textures - the first mip level used in the view.");
  uint32_t firstMip = 0;
  DOCUMENT("For textures - the number of mip levels in the view.");
  uint32_t numMips = 0;

  DOCUMENT("For 3D textures and texture arrays - the first slice used in the view.");
  uint32_t firstSlice = 0;
  DOCUMENT("For 3D textures and texture arrays - the number of array slices in the view.");
  uint32_t numSlices = 0;

  DOCUMENT("For buffers - the byte offset where the buffer view starts in the underlying buffer.");
  uint64_t byteOffset = 0;
  DOCUMENT("For buffers - how many bytes are in this buffer view.");
  uint64_t byteSize = 0;

  DOCUMENT("For samplers - the :class:`TextureFilter` describing the filtering mode.");
  TextureFilter filter;
  DOCUMENT("For samplers - the :class:`AddressMode` in the U direction.");
  AddressMode addressU = AddressMode::Wrap;
  DOCUMENT("For samplers - the :class:`AddressMode` in the V direction.");
  AddressMode addressV = AddressMode::Wrap;
  DOCUMENT("For samplers - the :class:`AddressMode` in the W direction.");
  AddressMode addressW = AddressMode::Wrap;
  DOCUMENT("For samplers - a bias to apply to the calculated mip level before sampling.");
  float mipBias = 0.0f;
  DOCUMENT("For samplers - the maximum anisotropic filtering level to use.");
  float maxAnisotropy = 0.0f;
  DOCUMENT("For samplers - the :class:`CompareFunction` for comparison samplers.");
  CompareFunction compareFunction = CompareFunction::AlwaysTrue;
  DOCUMENT("For samplers - the minimum mip level that can be used.");
  float minLOD = 0.0f;
  DOCUMENT("For samplers - the maximum mip level that can be used.");
  float maxLOD = 0.0f;
  DOCUMENT("For samplers - the RGBA border color.");
  float borderColor[4];
  DOCUMENT("For samplers - ``True`` if unnormalized co-ordinates are used in this sampler.");
  bool unnormalized = false;

  DOCUMENT(R"(For samplers - the :class:`ResourceId` of the ycbcr conversion object associated with
this sampler.
)");
  ResourceId ycbcrSampler;

  DOCUMENT("For ycbcr samplers - the :class:`YcbcrConversion` used for conversion.");
  YcbcrConversion ycbcrModel;
  DOCUMENT("For ycbcr samplers - the :class:`YcbcrRange` used for conversion.");
  YcbcrRange ycbcrRange;
  DOCUMENT(R"(For ycbcr samplers - Four :class:`TextureSwizzle` elements indicating the swizzle
applied before conversion.
)");
  TextureSwizzle ycbcrSwizzle[4];
  DOCUMENT("For ycbcr samplers - the :class:`ChromaSampleLocation` X-axis chroma offset.");
  ChromaSampleLocation xChromaOffset;
  DOCUMENT("For ycbcr samplers - the :class:`ChromaSampleLocation` Y-axis chroma offset.");
  ChromaSampleLocation yChromaOffset;
  DOCUMENT("For ycbcr samplers - the :class:`FilterMode` describing the chroma filtering mode.");
  FilterMode chromaFilter;
  DOCUMENT("For ycbcr samplers - ``True`` if explicit reconstruction is force enabled.");
  bool forceExplicitReconstruction;

  DOCUMENT(R"(For samplers - check if the border color is used in this Vulkan sampler.

:return: ``True`` if the border color is used, ``False`` otherwise.
:rtype: ``bool``
)");
  bool UseBorder() const
  {
    return addressU == AddressMode::ClampBorder || addressV == AddressMode::ClampBorder ||
           addressW == AddressMode::ClampBorder;
  }
};

DOCUMENT("The contents of a single binding within a descriptor set, either arrayed or not.");
struct DescriptorBinding
{
  DOCUMENT("");
  DescriptorBinding() = default;
  DescriptorBinding(const DescriptorBinding &) = default;

  bool operator==(const DescriptorBinding &o) const
  {
    return descriptorCount == o.descriptorCount && dynamicallyUsedCount == o.dynamicallyUsedCount &&
           type == o.type && stageFlags == o.stageFlags && binds == o.binds;
  }
  bool operator<(const DescriptorBinding &o) const
  {
    if(!(descriptorCount == o.descriptorCount))
      return descriptorCount < o.descriptorCount;
    if(!(dynamicallyUsedCount == o.dynamicallyUsedCount))
      return dynamicallyUsedCount < o.dynamicallyUsedCount;
    if(!(type == o.type))
      return type < o.type;
    if(!(stageFlags == o.stageFlags))
      return stageFlags < o.stageFlags;
    if(!(binds == o.binds))
      return binds < o.binds;
    return false;
  }
  DOCUMENT(R"(How many descriptors are in this binding array.
If this binding is empty/non-existant this value will be ``0``.
)");
  uint32_t descriptorCount = 0;
  DOCUMENT(R"(Lists how many bindings in :data:`binds` are dynamically used. Useful to avoid
redundant iteration to determine whether any bindings are present.

For more information see :data:`VKBindingElement.dynamicallyUsed`.
)");
  uint32_t dynamicallyUsedCount = 0;
  DOCUMENT("The :class:`BindType` of this binding.");
  BindType type = BindType::Unknown;
  DOCUMENT("The :class:`ShaderStageMask` where this binding is visible.");
  ShaderStageMask stageFlags = ShaderStageMask::Unknown;

  DOCUMENT(R"(A list of :class:`VKBindingElement` with the binding elements.
If :data:`descriptorCount` is 1 then this isn't an array, and this list has only one element.
)");
  rdcarray<BindingElement> binds;
};

DOCUMENT("The contents of a descriptor set.");
struct DescriptorSet
{
  DOCUMENT("");
  DescriptorSet() = default;
  DescriptorSet(const DescriptorSet &) = default;

  bool operator==(const DescriptorSet &o) const
  {
    return layoutResourceId == o.layoutResourceId &&
           descriptorSetResourceId == o.descriptorSetResourceId &&
           pushDescriptor == o.pushDescriptor && bindings == o.bindings;
  }
  bool operator<(const DescriptorSet &o) const
  {
    if(!(layoutResourceId == o.layoutResourceId))
      return layoutResourceId < o.layoutResourceId;
    if(!(descriptorSetResourceId == o.descriptorSetResourceId))
      return descriptorSetResourceId < o.descriptorSetResourceId;
    if(!(pushDescriptor == o.pushDescriptor))
      return pushDescriptor < o.pushDescriptor;
    if(!(bindings == o.bindings))
      return bindings < o.bindings;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the descriptor set layout that matches this set.");
  ResourceId layoutResourceId;
  DOCUMENT("The :class:`ResourceId` of the descriptor set object.");
  ResourceId descriptorSetResourceId;
  DOCUMENT("Indicates if this is a virtual 'push' descriptor set.");
  bool pushDescriptor = false;

  DOCUMENT(R"(A list of :class:`VKDescriptorBinding` with the bindings within this set.
This list is indexed by the binding, so it may be sparse (some entries do not contain any elements).
)");
  rdcarray<DescriptorBinding> bindings;
};

DOCUMENT("Describes the object and descriptor set bindings of a Vulkan pipeline object.");
struct Pipeline
{
  DOCUMENT("");
  Pipeline() = default;
  Pipeline(const Pipeline &) = default;

  DOCUMENT("The :class:`ResourceId` of the pipeline object.");
  ResourceId pipelineResourceId;
  DOCUMENT("The :class:`ResourceId` of the pipeline layout object.");
  ResourceId pipelineLayoutResourceId;
  DOCUMENT("The flags used to create the pipeline object.");
  uint32_t flags = 0;

  DOCUMENT("A list of :class:`VKDescriptorSet` with the bound descriptor sets.");
  rdcarray<DescriptorSet> descriptorSets;
};

DOCUMENT("Describes the Vulkan index buffer binding.")
struct IndexBuffer
{
  DOCUMENT("");
  IndexBuffer() = default;
  IndexBuffer(const IndexBuffer &) = default;

  DOCUMENT("The :class:`ResourceId` of the index buffer.");
  ResourceId resourceId;

  DOCUMENT("The byte offset from the start of the buffer to the beginning of the index data.");
  uint64_t byteOffset = 0;
};

DOCUMENT("Describes the vulkan input assembly configuration.");
struct InputAssembly
{
  DOCUMENT("");
  InputAssembly() = default;
  InputAssembly(const InputAssembly &) = default;

  DOCUMENT("``True`` if primitive restart is enabled for strip primitives.");
  bool primitiveRestartEnable = false;

  DOCUMENT("The :class:`VKIndexBuffer` with the index buffer binding.");
  IndexBuffer indexBuffer;
};

DOCUMENT("Describes the configuration of a single vertex attribute.");
struct VertexAttribute
{
  DOCUMENT("");
  VertexAttribute() = default;
  VertexAttribute(const VertexAttribute &) = default;

  bool operator==(const VertexAttribute &o) const
  {
    return location == o.location && binding == o.binding && format == o.format &&
           byteOffset == o.byteOffset;
  }
  bool operator<(const VertexAttribute &o) const
  {
    if(!(location == o.location))
      return location < o.location;
    if(!(binding == o.binding))
      return binding < o.binding;
    if(!(format == o.format))
      return format < o.format;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    return false;
  }
  DOCUMENT("The location in the shader that is bound to this attribute.");
  uint32_t location = 0;
  DOCUMENT("The vertex binding where data will be sourced from.");
  uint32_t binding = 0;
  DOCUMENT("The :class:`ResourceFormat` describing how each input element will be interpreted.");
  ResourceFormat format;
  DOCUMENT(
      "The byte offset from the start of each vertex data in the :data:`binding` to this "
      "attribute.");
  uint32_t byteOffset = 0;
};

DOCUMENT("Describes a vertex binding.");
struct VertexBinding
{
  DOCUMENT("");
  VertexBinding() = default;
  VertexBinding(const VertexBinding &) = default;

  bool operator==(const VertexBinding &o) const
  {
    return vertexBufferBinding == o.vertexBufferBinding && byteStride == o.byteStride &&
           perInstance == o.perInstance && instanceDivisor == o.instanceDivisor;
  }
  bool operator<(const VertexBinding &o) const
  {
    if(!(vertexBufferBinding == o.vertexBufferBinding))
      return vertexBufferBinding < o.vertexBufferBinding;
    if(!(byteStride == o.byteStride))
      return byteStride < o.byteStride;
    if(!(perInstance == o.perInstance))
      return perInstance < o.perInstance;
    if(!(instanceDivisor == o.instanceDivisor))
      return instanceDivisor < o.instanceDivisor;
    return false;
  }
  DOCUMENT("The vertex binding where data will be sourced from.");
  uint32_t vertexBufferBinding = 0;
  DOCUMENT("The byte stride between the start of one set of vertex data and the next.");
  uint32_t byteStride = 0;
  DOCUMENT("``True`` if the vertex data is instance-rate.");
  bool perInstance = false;
  DOCUMENT(R"(The instance rate divisor.

If this is ``0`` then every vertex gets the same value.

If it's ``1`` then one element is read for each instance, and for ``N`` greater than ``1`` then
``N`` instances read the same element before advancing.
)");
  uint32_t instanceDivisor = 1;
};

DOCUMENT("Describes a single Vulkan vertex buffer binding.")
struct VertexBuffer
{
  DOCUMENT("");
  VertexBuffer() = default;
  VertexBuffer(const VertexBuffer &) = default;

  bool operator==(const VertexBuffer &o) const
  {
    return resourceId == o.resourceId && byteOffset == o.byteOffset;
  }
  bool operator<(const VertexBuffer &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer bound to this slot.");
  ResourceId resourceId;
  DOCUMENT("The byte offset from the start of the buffer to the beginning of the vertex data.");
  uint64_t byteOffset = 0;
};

DOCUMENT("Describes the fixed-function vertex input fetch setup.");
struct VertexInput
{
  DOCUMENT("");
  VertexInput() = default;
  VertexInput(const VertexInput &) = default;

  DOCUMENT("A list of :class:`VKVertexAttribute` with the vertex attributes.");
  rdcarray<VertexAttribute> attributes;
  DOCUMENT("A list of :class:`VKVertexBinding` with the vertex bindings.");
  rdcarray<VertexBinding> bindings;
  DOCUMENT("A list of :class:`VKVertexBuffer` with the vertex buffers.");
  rdcarray<VertexBuffer> vertexBuffers;
};

DOCUMENT("The provided value for a specialization constant.");
struct SpecializationConstant
{
  DOCUMENT("");
  SpecializationConstant() = default;
  SpecializationConstant(const SpecializationConstant &) = default;

  bool operator==(const SpecializationConstant &o) const
  {
    return specializationId == o.specializationId && data == o.data;
  }
  bool operator<(const SpecializationConstant &o) const
  {
    if(!(specializationId == o.specializationId))
      return specializationId < o.specializationId;
    if(!(data == o.data))
      return data < o.data;
    return false;
  }
  DOCUMENT("The specialization ID");
  uint32_t specializationId = 0;
  DOCUMENT("A ``bytes`` with the contents of the constant.");
  bytebuf data;
};

DOCUMENT("Describes a Vulkan shader stage.");
struct Shader
{
  DOCUMENT("");
  Shader() = default;
  Shader(const Shader &) = default;

  DOCUMENT("The :class:`ResourceId` of the shader module object.");
  ResourceId resourceId;
  DOCUMENT("The name of the entry point in the shader module that is used.");
  rdcstr entryPoint;

  DOCUMENT("A :class:`ShaderReflection` describing the reflection data for this shader.");
  ShaderReflection *reflection = NULL;
  DOCUMENT(R"(A :class:`ShaderBindpointMapping` to match :data:`reflection` with the bindpoint
mapping data.
)");
  ShaderBindpointMapping bindpointMapping;

  DOCUMENT("A :class:`ShaderStage` identifying which stage this shader is bound to.");
  ShaderStage stage = ShaderStage::Vertex;

  DOCUMENT(
      "A list of :class:`VKSpecializationConstant` with the provided specialization constants.");
  rdcarray<SpecializationConstant> specialization;
};

DOCUMENT("Describes the state of the fixed-function tessellator.");
struct Tessellation
{
  DOCUMENT("");
  Tessellation() = default;
  Tessellation(const Tessellation &) = default;

  DOCUMENT("The number of control points in each input patch.");
  uint32_t numControlPoints = 0;

  DOCUMENT("``True`` if the tessellation domain origin is upper-left, ``False`` if lower-left.");
  bool domainOriginUpperLeft = true;
};

DOCUMENT("Describes a single transform feedback binding.");
struct XFBBuffer
{
  DOCUMENT("");
  XFBBuffer() = default;
  XFBBuffer(const XFBBuffer &) = default;

  bool operator==(const XFBBuffer &o) const
  {
    return active == o.active && bufferResourceId == o.bufferResourceId &&
           byteOffset == o.byteOffset && byteSize == o.byteSize &&
           counterBufferResourceId == o.counterBufferResourceId &&
           counterBufferOffset == o.counterBufferOffset;
  }
  bool operator<(const XFBBuffer &o) const
  {
    if(!(active == o.active))
      return active < o.active;
    if(!(bufferResourceId == o.bufferResourceId))
      return bufferResourceId < o.bufferResourceId;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(byteSize == o.byteSize))
      return byteSize < o.byteSize;
    if(!(counterBufferResourceId == o.counterBufferResourceId))
      return counterBufferResourceId < o.counterBufferResourceId;
    if(!(counterBufferOffset == o.counterBufferOffset))
      return counterBufferOffset < o.counterBufferOffset;
    return false;
  }

  DOCUMENT("A flag indicating if this buffer is active or not.");
  bool active = false;

  DOCUMENT("The :class:`ResourceId` of the bound data buffer.");
  ResourceId bufferResourceId;

  DOCUMENT("The offset in bytes to the start of the data in the :data:`bufferResourceId`.");
  uint64_t byteOffset = 0;

  DOCUMENT("The size in bytes of the data buffer.");
  uint64_t byteSize = 0;

  DOCUMENT("The :class:`ResourceId` of the buffer storing the counter value (if set).");
  ResourceId counterBufferResourceId;

  DOCUMENT("The offset in bytes to the counter in the :data:`counterBufferResourceId`.");
  uint64_t counterBufferOffset = 0;
};

DOCUMENT("Describes the state of the fixed-function transform feedback.");
struct TransformFeedback
{
  DOCUMENT("");
  TransformFeedback() = default;
  TransformFeedback(const TransformFeedback &) = default;

  DOCUMENT("The bound transform feedback buffers.");
  rdcarray<XFBBuffer> buffers;
};

DOCUMENT("Describes a render area in the current framebuffer.");
struct RenderArea
{
  DOCUMENT("");
  RenderArea() = default;
  RenderArea(const RenderArea &) = default;
  bool operator==(const RenderArea &o) const
  {
    return x == o.x && y == o.y && width == o.width && height == o.height;
  }
  bool operator<(const RenderArea &o) const
  {
    if(!(x == o.x))
      return x < o.x;
    if(!(y == o.y))
      return y < o.y;
    if(!(width == o.width))
      return width < o.width;
    if(!(height == o.height))
      return height < o.height;
    return false;
  }

  DOCUMENT("The X co-ordinate of the render area.");
  int32_t x = 0;
  DOCUMENT("The Y co-ordinate of the render area.");
  int32_t y = 0;
  DOCUMENT("The width of the render area.");
  int32_t width = 0;
  DOCUMENT("The height of the render area.");
  int32_t height = 0;
};

DOCUMENT("Describes a combined viewport and scissor region.");
struct ViewportScissor
{
  DOCUMENT("");
  ViewportScissor() = default;
  ViewportScissor(const ViewportScissor &) = default;

  bool operator==(const ViewportScissor &o) const { return vp == o.vp && scissor == o.scissor; }
  bool operator<(const ViewportScissor &o) const
  {
    if(!(vp == o.vp))
      return vp < o.vp;
    if(!(scissor == o.scissor))
      return scissor < o.scissor;
    return false;
  }
  DOCUMENT("The :class:`Viewport`.");
  Viewport vp;
  DOCUMENT("The :class:`Scissor`.");
  Scissor scissor;
};

DOCUMENT("Describes the view state in the pipeline.");
struct ViewState
{
  DOCUMENT("");
  ViewState() = default;
  ViewState(const ViewState &) = default;

  DOCUMENT("A list of :class:`VKViewportScissor`.");
  rdcarray<ViewportScissor> viewportScissors;

  DOCUMENT("A list of :class:`VKRenderArea` defining discard rectangles.");
  rdcarray<RenderArea> discardRectangles;

  DOCUMENT(R"(``True`` if a fragment in any one of the discard rectangles fails the discard test,
and a fragment in none of them passes.

``False`` if a fragment in any one of the discard rectangles passes the discard test,
and a fragment in none of them is discarded.

.. note::
  A ``True`` value and an empty list of :data:`discardRectangles` means the test is effectively
  disabled, since with no rectangles no fragment can be inside one.
)");
  bool discardRectanglesExclusive = true;
};

DOCUMENT("Describes the rasterizer state in the pipeline.");
struct Rasterizer
{
  DOCUMENT("");
  Rasterizer() = default;
  Rasterizer(const Rasterizer &) = default;

  DOCUMENT(R"(``True`` if pixels outside of the near and far depth planes should be clamped and
to ``0.0`` to ``1.0``.
)");
  bool depthClampEnable = false;
  DOCUMENT(R"(``True`` if pixels outside of the near and far depth planes should be clipped.

.. note::
  In Vulkan 1.0 this value was implicitly set to the opposite of :data:`depthClampEnable`, but with
  later extensions & versions it can be set independently.
)");
  bool depthClipEnable = true;
  DOCUMENT("``True`` if primitives should be discarded during rasterization.");
  bool rasterizerDiscardEnable = false;
  DOCUMENT(R"(``True`` if counter-clockwise polygons are front-facing.
``False`` if clockwise polygons are front-facing.
)");
  bool frontCCW = false;
  DOCUMENT("The polygon :class:`FillMode`.");
  FillMode fillMode = FillMode::Solid;
  DOCUMENT("The polygon :class:`CullMode`.");
  CullMode cullMode = CullMode::NoCull;

  DOCUMENT("The active conservative rasterization mode.");
  ConservativeRaster conservativeRasterization = ConservativeRaster::Disabled;

  DOCUMENT(R"(The extra size in pixels to increase primitives by during conservative rasterization,
in the x and y directions in screen space.

See :data:`conservativeRasterizationMode`
)");
  float extraPrimitiveOverestimationSize = 0.0f;

  DOCUMENT("The fixed depth bias value to apply to z-values.");
  float depthBias = 0.0f;
  DOCUMENT(R"(The clamp value for calculated depth bias from :data:`depthBias` and
:data:`slopeScaledDepthBias`
)");
  float depthBiasClamp = 0.0f;
  DOCUMENT("The slope-scaled depth bias value to apply to z-values.");
  float slopeScaledDepthBias = 0.0f;
  DOCUMENT("The fixed line width in pixels.");
  float lineWidth = 0.0f;
};

DOCUMENT("Describes state of custom sample locations in the pipeline.");
struct SampleLocations
{
  DOCUMENT("");
  SampleLocations() = default;
  SampleLocations(const SampleLocations &) = default;

  DOCUMENT("The width in pixels of the region configured.");
  uint32_t gridWidth = 1;
  DOCUMENT("The height in pixels of the region configured.");
  uint32_t gridHeight = 1;
  DOCUMENT(R"(A list of :class:`FloatVector` giving the custom sample locations. Only x and y are
valid, z and w are set to 0.0.

If the list is empty then the standard sample pattern is in use.
)");
  rdcarray<FloatVector> customLocations;
};

DOCUMENT("Describes the multisampling state in the pipeline.");
struct MultiSample
{
  DOCUMENT("");
  MultiSample() = default;
  MultiSample(const MultiSample &) = default;

  DOCUMENT("How many samples to use when rasterizing.");
  uint32_t rasterSamples = 0;
  DOCUMENT("``True`` if rendering should happen at sample-rate frequency.");
  bool sampleShadingEnable = false;
  DOCUMENT("The minimum sample shading rate.");
  float minSampleShading = 0.0f;
  DOCUMENT("A mask that generated samples should be masked with using bitwise ``AND``.");
  uint32_t sampleMask = 0;
  DOCUMENT("The :class:`VKSampleLocations` with any custom sample locations that are configured.");
  SampleLocations sampleLocations;
};

DOCUMENT("Describes the pipeline blending state.");
struct ColorBlendState
{
  DOCUMENT("");
  ColorBlendState() = default;
  ColorBlendState(const ColorBlendState &) = default;

  DOCUMENT("``True`` if alpha-to-coverage should be used when blending to an MSAA target.");
  bool alphaToCoverageEnable = false;
  DOCUMENT("``True`` if alpha-to-one should be used when blending to an MSAA target.");
  bool alphaToOneEnable = false;

  DOCUMENT("The list of :class:`ColorBlend` with the blending configuration per-attachment.");
  rdcarray<ColorBlend> blends;

  DOCUMENT("The constant blend factor to use in blend equations.");
  float blendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

DOCUMENT("Describes the pipeline depth-stencil state.");
struct DepthStencil
{
  DOCUMENT("");
  DepthStencil() = default;
  DepthStencil(const DepthStencil &) = default;

  DOCUMENT("``True`` if depth testing should be performed.");
  bool depthTestEnable = false;
  DOCUMENT("``True`` if depth values should be written to the depth target.");
  bool depthWriteEnable = false;
  DOCUMENT("``True`` if depth bounds tests should be applied.");
  bool depthBoundsEnable = false;
  DOCUMENT("The :class:`CompareFunction` to use for testing depth values.");
  CompareFunction depthFunction = CompareFunction::AlwaysTrue;

  DOCUMENT("``True`` if stencil operations should be performed.");
  bool stencilTestEnable = false;

  DOCUMENT("A :class:`StencilFace` describing what happens for front-facing polygons.");
  StencilFace frontFace;
  DOCUMENT("A :class:`StencilFace` describing what happens for back-facing polygons.");
  StencilFace backFace;

  DOCUMENT("The near plane bounding value.");
  float minDepthBounds = 0.0f;
  DOCUMENT("The far plane bounding value.");
  float maxDepthBounds = 0.0f;
};

DOCUMENT("Describes the setup of a renderpass and subpasses.");
struct RenderPass
{
  DOCUMENT("");
  RenderPass() = default;
  RenderPass(const RenderPass &) = default;

  DOCUMENT("The :class:`ResourceId` of the render pass.");
  ResourceId resourceId;

  DOCUMENT("The index of the current active subpass.");
  uint32_t subpass;

  // VKTODOMED renderpass and subpass information here

  DOCUMENT("A list of indices into the framebuffer attachments for input attachments.");
  rdcarray<uint32_t> inputAttachments;
  DOCUMENT("A list of indices into the framebuffer attachments for color attachments.");
  rdcarray<uint32_t> colorAttachments;
  DOCUMENT("A list of indices into the framebuffer attachments for resolve attachments.");
  rdcarray<uint32_t> resolveAttachments;
  DOCUMENT(R"(An index into the framebuffer attachments for the depth-stencil attachment.

If there is no depth-stencil attachment, this index is ``-1``.
)");
  int32_t depthstencilAttachment = -1;

  DOCUMENT(R"(An index into the framebuffer attachments for the fragment density attachment.

If there is no fragment density attachment, this index is ``-1``.
)");
  int32_t fragmentDensityAttachment = -1;

  DOCUMENT(R"(If multiview is enabled, contains a list of view indices to be broadcast to during
rendering.

If the list is empty, multiview is disabled and rendering is as normal.
)");
  rdcarray<uint32_t> multiviews;
};

DOCUMENT("Describes a single attachment in a framebuffer object.");
struct Attachment
{
  DOCUMENT("");
  Attachment() = default;
  Attachment(const Attachment &) = default;

  bool operator==(const Attachment &o) const
  {
    return viewResourceId == o.viewResourceId && imageResourceId == o.imageResourceId &&
           viewFormat == o.viewFormat && swizzle[0] == o.swizzle[0] && swizzle[1] == o.swizzle[1] &&
           swizzle[2] == o.swizzle[2] && swizzle[3] == o.swizzle[3] && firstMip == o.firstMip &&
           firstSlice == o.firstSlice && numMips == o.numMips && numSlices == o.numSlices;
  }
  bool operator<(const Attachment &o) const
  {
    if(!(viewResourceId == o.viewResourceId))
      return viewResourceId < o.viewResourceId;
    if(!(imageResourceId == o.imageResourceId))
      return imageResourceId < o.imageResourceId;
    if(!(viewFormat == o.viewFormat))
      return viewFormat < o.viewFormat;
    if(!(swizzle[0] == o.swizzle[0]))
      return swizzle[0] < o.swizzle[0];
    if(!(swizzle[1] == o.swizzle[1]))
      return swizzle[1] < o.swizzle[1];
    if(!(swizzle[2] == o.swizzle[2]))
      return swizzle[2] < o.swizzle[2];
    if(!(swizzle[3] == o.swizzle[3]))
      return swizzle[3] < o.swizzle[3];
    if(!(firstMip == o.firstMip))
      return firstMip < o.firstMip;
    if(!(firstSlice == o.firstSlice))
      return firstSlice < o.firstSlice;
    if(!(numMips == o.numMips))
      return numMips < o.numMips;
    if(!(numSlices == o.numSlices))
      return numSlices < o.numSlices;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the image view itself.");
  ResourceId viewResourceId;
  DOCUMENT("The :class:`ResourceId` of the underlying image that the view refers to.");
  ResourceId imageResourceId;

  DOCUMENT("The :class:`ResourceFormat` that the view uses.");
  ResourceFormat viewFormat;
  DOCUMENT("Four :class:`TextureSwizzle` elements indicating the swizzle applied to this texture.");
  TextureSwizzle swizzle[4] = {TextureSwizzle::Red, TextureSwizzle::Green, TextureSwizzle::Blue,
                               TextureSwizzle::Alpha};
  DOCUMENT("The first mip level used in the attachment.");
  uint32_t firstMip = 0;
  DOCUMENT("For 3D textures and texture arrays, the first slice used in the attachment.");
  uint32_t firstSlice = 0;
  DOCUMENT("The number of mip levels in the attachment.");
  uint32_t numMips = 1;
  DOCUMENT("For 3D textures and texture arrays, the number of array slices in the attachment.");
  uint32_t numSlices = 1;
};

DOCUMENT("Describes a framebuffer object and its attachments.");
struct Framebuffer
{
  DOCUMENT("");
  Framebuffer() = default;
  Framebuffer(const Framebuffer &) = default;

  DOCUMENT("The :class:`ResourceId` of the framebuffer object.");
  ResourceId resourceId;

  DOCUMENT("A list of :class:`VKAttachment` with the attachments of this framebuffer.");
  rdcarray<Attachment> attachments;

  DOCUMENT("The width of this framebuffer in pixels.");
  uint32_t width = 0;
  DOCUMENT("The height of this framebuffer in pixels.");
  uint32_t height = 0;
  DOCUMENT("The number of layers in this framebuffer.");
  uint32_t layers = 0;
};

DOCUMENT("Describes the current pass instance at the current time.");
struct CurrentPass
{
  DOCUMENT("");
  CurrentPass() = default;
  CurrentPass(const CurrentPass &) = default;

  DOCUMENT("The :class:`VKRenderPass` that is currently active.");
  RenderPass renderpass;
  DOCUMENT("The :class:`VKFramebuffer` that is currently being used.");
  Framebuffer framebuffer;
  DOCUMENT("The :class:`VKRenderArea` that is currently being rendered to.");
  RenderArea renderArea;
};

DOCUMENT("Contains the layout of a range of subresources in an image.");
struct ImageLayout
{
  DOCUMENT("");
  ImageLayout() = default;
  ImageLayout(const ImageLayout &) = default;

  bool operator==(const ImageLayout &o) const
  {
    return baseMip == o.baseMip && baseLayer == o.baseLayer && numMip == o.numMip &&
           numLayer == o.numLayer && name == o.name;
  }
  bool operator<(const ImageLayout &o) const
  {
    if(!(baseMip == o.baseMip))
      return baseMip < o.baseMip;
    if(!(baseLayer == o.baseLayer))
      return baseLayer < o.baseLayer;
    if(!(numMip == o.numMip))
      return numMip < o.numMip;
    if(!(numLayer == o.numLayer))
      return numLayer < o.numLayer;
    if(!(name == o.name))
      return name < o.name;
    return false;
  }
  DOCUMENT("The first mip level used in the range.");
  uint32_t baseMip = 0;
  DOCUMENT("For 3D textures and texture arrays, the first slice used in the range.");
  uint32_t baseLayer = 0;
  DOCUMENT("The number of mip levels in the range.");
  uint32_t numMip = 1;
  DOCUMENT("For 3D textures and texture arrays, the number of array slices in the range.");
  uint32_t numLayer = 1;
  DOCUMENT("The name of the current image state.");
  rdcstr name;
};

DOCUMENT("Contains the current layout of all subresources in the image.");
struct ImageData
{
  DOCUMENT("");
  ImageData() = default;
  ImageData(const ImageData &) = default;

  bool operator==(const ImageData &o) const { return resourceId == o.resourceId; }
  bool operator<(const ImageData &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the image.");
  ResourceId resourceId;

  DOCUMENT("A list of :class:`VKImageLayout` with the set of subresources that make up the image.");
  rdcarray<ImageLayout> layouts;
};

DOCUMENT("Contains the current conditional rendering state.");
struct ConditionalRendering
{
  DOCUMENT("");
  ConditionalRendering() = default;
  ConditionalRendering(const ConditionalRendering &) = default;

  DOCUMENT(
      "The :class:`ResourceId` of the buffer containing the predicate for conditional rendering.");
  ResourceId bufferId;

  DOCUMENT("The byte offset into buffer where the predicate is located.");
  uint64_t byteOffset = 0;

  DOCUMENT("``True`` if predicate result is inverted.");
  bool isInverted = false;

  DOCUMENT("``True`` if the current predicate would render.");
  bool isPassing = false;
};

DOCUMENT("The full current Vulkan pipeline state.");
struct State
{
#if !defined(RENDERDOC_EXPORTS)
  // disallow creation/copy of this object externally
  State() = delete;
  State(const State &) = delete;
#endif

  DOCUMENT("A :class:`VKPipeline` with the currently bound compute pipeline, if any.");
  Pipeline compute;
  DOCUMENT("A :class:`VKPipeline` with the currently bound graphics pipeline, if any.");
  Pipeline graphics;

  DOCUMENT("A ``bytes`` containing the raw push constant data.");
  bytebuf pushconsts;

  DOCUMENT("A :class:`VKInputAssembly` describing the input assembly stage.");
  InputAssembly inputAssembly;
  DOCUMENT("A :class:`VKVertexInput` describing the vertex input stage.");
  VertexInput vertexInput;

  DOCUMENT("A :class:`VKShader` describing the vertex shader stage.");
  Shader vertexShader;
  DOCUMENT("A :class:`VKShader` describing the tessellation control shader stage.");
  Shader tessControlShader;
  DOCUMENT("A :class:`VKShader` describing the tessellation evaluation shader stage.");
  Shader tessEvalShader;
  DOCUMENT("A :class:`VKShader` describing the geometry shader stage.");
  Shader geometryShader;
  DOCUMENT("A :class:`VKShader` describing the fragment shader stage.");
  Shader fragmentShader;
  DOCUMENT("A :class:`VKShader` describing the compute shader stage.");
  Shader computeShader;

  DOCUMENT("A :class:`VKTessellation` describing the tessellation stage.");
  Tessellation tessellation;

  DOCUMENT("A :class:`VKTransformFeedback` describing the tessellation stage.");
  TransformFeedback transformFeedback;

  DOCUMENT("A :class:`VKViewState` describing the viewport setup.");
  ViewState viewportScissor;
  DOCUMENT("A :class:`VKRasterizer` describing rasterization.");
  Rasterizer rasterizer;

  DOCUMENT("A :class:`VKMultiSample` describing the multisample setup.");
  MultiSample multisample;
  DOCUMENT("A :class:`VKColorBlendState` describing color blending.");
  ColorBlendState colorBlend;
  DOCUMENT("A :class:`VKDepthStencil` describing the depth-stencil stage.");
  DepthStencil depthStencil;

  DOCUMENT("A :class:`VKCurrentPass` describing the current renderpass, subpass and framebuffer.");
  CurrentPass currentPass;

  DOCUMENT("A list of :class:`VKImageData` entries, one for each image.");
  rdcarray<ImageData> images;

  DOCUMENT("A :class:`ConditionalRendering` describing the current conditional rendering state.");
  ConditionalRendering conditionalRendering;
};

};    // namespace VKPipe

DECLARE_REFLECTION_STRUCT(VKPipe::BindingElement);
DECLARE_REFLECTION_STRUCT(VKPipe::DescriptorBinding);
DECLARE_REFLECTION_STRUCT(VKPipe::DescriptorSet);
DECLARE_REFLECTION_STRUCT(VKPipe::Pipeline);
DECLARE_REFLECTION_STRUCT(VKPipe::IndexBuffer);
DECLARE_REFLECTION_STRUCT(VKPipe::InputAssembly);
DECLARE_REFLECTION_STRUCT(VKPipe::VertexAttribute);
DECLARE_REFLECTION_STRUCT(VKPipe::VertexBinding);
DECLARE_REFLECTION_STRUCT(VKPipe::VertexBuffer);
DECLARE_REFLECTION_STRUCT(VKPipe::VertexInput);
DECLARE_REFLECTION_STRUCT(VKPipe::SpecializationConstant);
DECLARE_REFLECTION_STRUCT(VKPipe::Shader);
DECLARE_REFLECTION_STRUCT(VKPipe::Tessellation);
DECLARE_REFLECTION_STRUCT(VKPipe::XFBBuffer);
DECLARE_REFLECTION_STRUCT(VKPipe::TransformFeedback);
DECLARE_REFLECTION_STRUCT(VKPipe::ViewportScissor);
DECLARE_REFLECTION_STRUCT(VKPipe::ViewState);
DECLARE_REFLECTION_STRUCT(VKPipe::Rasterizer);
DECLARE_REFLECTION_STRUCT(VKPipe::SampleLocations);
DECLARE_REFLECTION_STRUCT(VKPipe::MultiSample);
DECLARE_REFLECTION_STRUCT(VKPipe::ColorBlendState);
DECLARE_REFLECTION_STRUCT(VKPipe::DepthStencil);
DECLARE_REFLECTION_STRUCT(VKPipe::RenderPass);
DECLARE_REFLECTION_STRUCT(VKPipe::Attachment);
DECLARE_REFLECTION_STRUCT(VKPipe::Framebuffer);
DECLARE_REFLECTION_STRUCT(VKPipe::RenderArea);
DECLARE_REFLECTION_STRUCT(VKPipe::CurrentPass);
DECLARE_REFLECTION_STRUCT(VKPipe::ImageLayout);
DECLARE_REFLECTION_STRUCT(VKPipe::ImageData);
DECLARE_REFLECTION_STRUCT(VKPipe::ConditionalRendering);
DECLARE_REFLECTION_STRUCT(VKPipe::State);
