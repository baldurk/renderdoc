/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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
  BindingElement &operator=(const BindingElement &) = default;

  bool operator==(const BindingElement &o) const
  {
    return type == o.type && dynamicallyUsed == o.dynamicallyUsed &&
           viewResourceId == o.viewResourceId && resourceResourceId == o.resourceResourceId &&
           samplerResourceId == o.samplerResourceId && immutableSampler == o.immutableSampler &&
           inlineBlock == o.inlineBlock && viewFormat == o.viewFormat && swizzle == o.swizzle &&
           firstMip == o.firstMip && firstSlice == o.firstSlice && numMips == o.numMips &&
           numSlices == o.numSlices && byteOffset == o.byteOffset && byteSize == o.byteSize &&
           filter == o.filter && addressU == o.addressU && addressV == o.addressV &&
           addressW == o.addressW && mipBias == o.mipBias && maxAnisotropy == o.maxAnisotropy &&
           compareFunction == o.compareFunction && minLOD == o.minLOD && maxLOD == o.maxLOD &&
           borderColorValue.uintValue == o.borderColorValue.uintValue &&
           borderColorType == o.borderColorType && unnormalized == o.unnormalized &&
           srgbBorder == o.srgbBorder && seamless == o.seamless;
  }
  bool operator<(const BindingElement &o) const
  {
    if(!(type == o.type))
      return type < o.type;
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
    if(!(inlineBlock == o.inlineBlock))
      return inlineBlock < o.inlineBlock;
    if(!(viewFormat == o.viewFormat))
      return viewFormat < o.viewFormat;
    if(!(swizzle == o.swizzle))
      return swizzle < o.swizzle;
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
    if(!(borderColorValue.uintValue == o.borderColorValue.uintValue))
      return borderColorValue.uintValue < o.borderColorValue.uintValue;
    if(!(borderColorType == o.borderColorType))
      return borderColorType < o.borderColorType;
    if(!(unnormalized == o.unnormalized))
      return unnormalized < o.unnormalized;
    if(!(srgbBorder == o.srgbBorder))
      return srgbBorder < o.srgbBorder;
    if(!(seamless == o.seamless))
      return seamless < o.seamless;
    return false;
  }

  DOCUMENT("The :class:`BindType` of this binding element.");
  BindType type = BindType::Unknown;
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

  DOCUMENT(R"(The format cast that the view uses.

:type: ResourceFormat
)");
  ResourceFormat viewFormat;

  DOCUMENT(R"(The swizzle applied to a texture by the view.

:type: TextureSwizzle4
)");
  TextureSwizzle4 swizzle;

  DOCUMENT("For textures - the first mip level used in the view.");
  uint32_t firstMip = 0;
  DOCUMENT("For textures - the number of mip levels in the view.");
  uint32_t numMips = 0;

  DOCUMENT("For 3D textures and texture arrays - the first slice used in the view.");
  uint32_t firstSlice = 0;
  DOCUMENT("For 3D textures and texture arrays - the number of array slices in the view.");
  uint32_t numSlices = 0;

  DOCUMENT(R"(For buffers - the byte offset where the buffer view starts in the underlying buffer.

For inline block uniforms (see :data:`inlineBlock`) this is the byte offset into the descriptor
set's inline block data.
)");
  uint64_t byteOffset = 0;
  DOCUMENT("For buffers - how many bytes are in this buffer view.");
  uint64_t byteSize = 0;

  DOCUMENT(R"(The filtering mode.

:type: TextureFilter
)");
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
  DOCUMENT("For samplers and image views - the minimum mip level that can be used.");
  float minLOD = 0.0f;
  DOCUMENT("For samplers - the maximum mip level that can be used.");
  float maxLOD = 0.0f;
  DOCUMENT(R"(For samplers - the RGBA border color value. Typically the float tuple inside will be used,
but the exact component type can be checked with :data:`borderColorType`.

:type: PixelValue
)");
  PixelValue borderColorValue = {};
  DOCUMENT(R"(For samplers - the RGBA border color type. This determines how the data in
:data:`borderColorValue` will be interpreted.

:type: CompType
)");
  CompType borderColorType = CompType::Float;
  DOCUMENT(R"(The swizzle applied to samplers. Primarily for ycbcr samplers applied before
conversion but for non-ycbcr samplers can be used for implementations that require sampler swizzle
information for border colors.

:type: TextureSwizzle4
)");
  TextureSwizzle4 samplerSwizzle;
  DOCUMENT(
      "For samplers - ``True`` if the border colour is swizzled with an sRGB formatted image.");
  bool srgbBorder = false;
  DOCUMENT("For samplers - ``True`` if unnormalized co-ordinates are used in this sampler.");
  bool unnormalized = false;

  DOCUMENT("``True`` if this is an inline uniform block binding.");
  bool inlineBlock = false;

  DOCUMENT("``True`` if this sampler is seamless across cubemap boundaries (the default).");
  bool seamless = true;

  DOCUMENT(R"(For samplers - the :class:`ResourceId` of the ycbcr conversion object associated with
this sampler.
)");
  ResourceId ycbcrSampler;

  DOCUMENT("For ycbcr samplers - the :class:`YcbcrConversion` used for conversion.");
  YcbcrConversion ycbcrModel;
  DOCUMENT("For ycbcr samplers - the :class:`YcbcrRange` used for conversion.");
  YcbcrRange ycbcrRange;
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
:rtype: bool
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
  DescriptorBinding &operator=(const DescriptorBinding &) = default;

  bool operator==(const DescriptorBinding &o) const
  {
    return descriptorCount == o.descriptorCount && dynamicallyUsedCount == o.dynamicallyUsedCount &&
           firstUsedIndex == o.firstUsedIndex && lastUsedIndex == o.lastUsedIndex &&
           stageFlags == o.stageFlags && binds == o.binds;
  }
  bool operator<(const DescriptorBinding &o) const
  {
    if(!(descriptorCount == o.descriptorCount))
      return descriptorCount < o.descriptorCount;
    if(!(dynamicallyUsedCount == o.dynamicallyUsedCount))
      return dynamicallyUsedCount < o.dynamicallyUsedCount;
    if(!(firstUsedIndex == o.firstUsedIndex))
      return firstUsedIndex < o.firstUsedIndex;
    if(!(lastUsedIndex == o.lastUsedIndex))
      return lastUsedIndex < o.lastUsedIndex;
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
  uint32_t dynamicallyUsedCount = ~0U;
  DOCUMENT(R"(Gives the index of the first binding in :data:`binds` that is dynamically used. Useful
to avoid redundant iteration in very large descriptor arrays with a small subset that are used.

For more information see :data:`VKBindingElement.dynamicallyUsed`.
)");
  int32_t firstUsedIndex = 0;
  DOCUMENT(R"(Gives the index of the first binding in :data:`binds` that is dynamically used. Useful
to avoid redundant iteration in very large descriptor arrays with a small subset that are used.

.. note::
  This may be set to a higher value than the number of bindings, if no dynamic use information is
  available. Ensure that this is an additional check on the bind and the count is still respected.

For more information see :data:`VKBindingElement.dynamicallyUsed`.
)");
  int32_t lastUsedIndex = 0x7fffffff;
  DOCUMENT("The :class:`ShaderStageMask` where this binding is visible.");
  ShaderStageMask stageFlags = ShaderStageMask::Unknown;

  DOCUMENT(R"(The binding elements.
If :data:`descriptorCount` is 1 then this list has only one element and the binding is not arrayed.

:type: List[VKBindingElement]
)");
  rdcarray<BindingElement> binds;
};

DOCUMENT("The contents of a descriptor set.");
struct DescriptorSet
{
  DOCUMENT("");
  DescriptorSet() = default;
  DescriptorSet(const DescriptorSet &) = default;
  DescriptorSet &operator=(const DescriptorSet &) = default;

  bool operator==(const DescriptorSet &o) const
  {
    return layoutResourceId == o.layoutResourceId &&
           descriptorSetResourceId == o.descriptorSetResourceId &&
           pushDescriptor == o.pushDescriptor && bindings == o.bindings &&
           inlineData == o.inlineData;
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
    if(!(inlineData == o.inlineData))
      return inlineData < o.inlineData;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the descriptor set layout that matches this set.");
  ResourceId layoutResourceId;
  DOCUMENT("The :class:`ResourceId` of the descriptor set object.");
  ResourceId descriptorSetResourceId;
  DOCUMENT("Indicates if this is a virtual 'push' descriptor set.");
  bool pushDescriptor = false;

  DOCUMENT(R"(The bindings within this set.
This list is indexed by the binding, so it may be sparse (some entries do not contain any elements).

:type: List[VKDescriptorBinding]
)");
  rdcarray<DescriptorBinding> bindings;

  DOCUMENT(R"(The inline byte data within this descriptor set. Individual bindings will have an
offset and size into this buffer.

:type: bytes
)");
  bytebuf inlineData;
};

DOCUMENT("Describes the object and descriptor set bindings of a Vulkan pipeline object.");
struct Pipeline
{
  DOCUMENT("");
  Pipeline() = default;
  Pipeline(const Pipeline &) = default;
  Pipeline &operator=(const Pipeline &) = default;

  DOCUMENT("The :class:`ResourceId` of the pipeline object.");
  ResourceId pipelineResourceId;
  DOCUMENT("The :class:`ResourceId` of the compute pipeline layout object.");
  ResourceId pipelineComputeLayoutResourceId;
  DOCUMENT(R"(The :class:`ResourceId` of the pre-rasterization pipeline layout object.

When not using pipeline libraries, this will be identical to :data:`pipelineFragmentLayoutResourceId`.
)");
  ResourceId pipelinePreRastLayoutResourceId;
  DOCUMENT(R"(The :class:`ResourceId` of the fragment pipeline layout object.

When not using pipeline libraries, this will be identical to :data:`pipelinePreRastLayoutResourceId`.
)");
  ResourceId pipelineFragmentLayoutResourceId;
  DOCUMENT("The flags used to create the pipeline object.");
  uint32_t flags = 0;

  DOCUMENT(R"(The bound descriptor sets.

:type: List[VKDescriptorSet]
)");
  rdcarray<DescriptorSet> descriptorSets;
};

DOCUMENT("Describes the Vulkan index buffer binding.")
struct IndexBuffer
{
  DOCUMENT("");
  IndexBuffer() = default;
  IndexBuffer(const IndexBuffer &) = default;
  IndexBuffer &operator=(const IndexBuffer &) = default;

  DOCUMENT("The :class:`ResourceId` of the index buffer.");
  ResourceId resourceId;

  DOCUMENT("The byte offset from the start of the buffer to the beginning of the index data.");
  uint64_t byteOffset = 0;

  DOCUMENT(R"(The number of bytes for each index in the index buffer. Typically 2 or 4 bytes but
it can be 0 if no index buffer is bound.
)");
  uint32_t byteStride = 0;
};

DOCUMENT("Describes the vulkan input assembly configuration.");
struct InputAssembly
{
  DOCUMENT("");
  InputAssembly() = default;
  InputAssembly(const InputAssembly &) = default;
  InputAssembly &operator=(const InputAssembly &) = default;

  DOCUMENT("``True`` if primitive restart is enabled for strip primitives.");
  bool primitiveRestartEnable = false;

  DOCUMENT(R"(The index buffer binding.

:type: VKIndexBuffer
)");
  IndexBuffer indexBuffer;

  DOCUMENT(R"(The current primitive topology.

:type: Topology
)");
  Topology topology = Topology::Unknown;
};

DOCUMENT("Describes the configuration of a single vertex attribute.");
struct VertexAttribute
{
  DOCUMENT("");
  VertexAttribute() = default;
  VertexAttribute(const VertexAttribute &) = default;
  VertexAttribute &operator=(const VertexAttribute &) = default;

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
  DOCUMENT(R"(The format describing how the input element is interpreted.

:type: ResourceFormat
)");
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
  VertexBinding &operator=(const VertexBinding &) = default;

  bool operator==(const VertexBinding &o) const
  {
    return vertexBufferBinding == o.vertexBufferBinding && perInstance == o.perInstance &&
           instanceDivisor == o.instanceDivisor;
  }
  bool operator<(const VertexBinding &o) const
  {
    if(!(vertexBufferBinding == o.vertexBufferBinding))
      return vertexBufferBinding < o.vertexBufferBinding;
    if(!(perInstance == o.perInstance))
      return perInstance < o.perInstance;
    if(!(instanceDivisor == o.instanceDivisor))
      return instanceDivisor < o.instanceDivisor;
    return false;
  }
  DOCUMENT("The vertex binding where data will be sourced from.");
  uint32_t vertexBufferBinding = 0;
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
  VertexBuffer &operator=(const VertexBuffer &) = default;

  bool operator==(const VertexBuffer &o) const
  {
    return resourceId == o.resourceId && byteOffset == o.byteOffset && byteStride == o.byteStride &&
           byteSize == o.byteSize;
  }
  bool operator<(const VertexBuffer &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(byteStride == o.byteStride))
      return byteStride < o.byteStride;
    if(!(byteSize == o.byteSize))
      return byteSize < o.byteSize;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer bound to this slot.");
  ResourceId resourceId;
  DOCUMENT("The byte offset from the start of the buffer to the beginning of the vertex data.");
  uint64_t byteOffset = 0;
  DOCUMENT("The byte stride between the start of one set of vertex data and the next.");
  uint32_t byteStride = 0;
  DOCUMENT("The size of the vertex buffer.");
  uint32_t byteSize = 0;
};

DOCUMENT("Describes the fixed-function vertex input fetch setup.");
struct VertexInput
{
  DOCUMENT("");
  VertexInput() = default;
  VertexInput(const VertexInput &) = default;
  VertexInput &operator=(const VertexInput &) = default;

  DOCUMENT(R"(The vertex attributes.

:type: List[VKVertexAttribute]
)");
  rdcarray<VertexAttribute> attributes;
  DOCUMENT(R"(The vertex bindings.

:type: List[VKVertexBinding]
)");
  rdcarray<VertexBinding> bindings;
  DOCUMENT(R"(The vertex buffers.

:type: List[VKVertexBuffer]
)");
  rdcarray<VertexBuffer> vertexBuffers;
};

DOCUMENT("Describes a Vulkan shader stage.");
struct Shader
{
  DOCUMENT("");
  Shader() = default;
  Shader(const Shader &) = default;
  Shader &operator=(const Shader &) = default;

  DOCUMENT("The :class:`ResourceId` of the shader module object.");
  ResourceId resourceId;
  DOCUMENT("The name of the entry point in the shader module that is used.");
  rdcstr entryPoint;

  DOCUMENT(R"(The reflection data for this shader.

:type: ShaderReflection
)");
  ShaderReflection *reflection = NULL;
  DOCUMENT(R"(The bindpoint mapping data to match :data:`reflection`.

:type: ShaderBindpointMapping
)");
  ShaderBindpointMapping bindpointMapping;

  DOCUMENT("A :class:`ShaderStage` identifying which stage this shader is bound to.");
  ShaderStage stage = ShaderStage::Vertex;

  DOCUMENT("The byte offset into the push constant data that is visible to this shader.");
  uint32_t pushConstantRangeByteOffset = 0;

  DOCUMENT("The number of bytes in the push constant data that is visible to this shader.");
  uint32_t pushConstantRangeByteSize = 0;

  DOCUMENT(R"(The provided specialization constant data. Shader constants store the byte offset into
this buffer as their byteOffset. This data includes the applied specialization constants over the
top of the default values, so it is safe to read any constant from here and get the correct current
value.

:type: bytes
)");
  bytebuf specializationData;

  DOCUMENT(R"(The specialization constant ID for each entry in the specialization constant block of
reflection info. This corresponds to the constantID in VkSpecializationMapEntry, while the offset
and size into specializationData can be obtained from the reflection info.

:type: List[int]
)")
  rdcarray<uint32_t> specializationIds;
};

DOCUMENT("Describes the state of the fixed-function tessellator.");
struct Tessellation
{
  DOCUMENT("");
  Tessellation() = default;
  Tessellation(const Tessellation &) = default;
  Tessellation &operator=(const Tessellation &) = default;

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
  XFBBuffer &operator=(const XFBBuffer &) = default;

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
  TransformFeedback &operator=(const TransformFeedback &) = default;

  DOCUMENT(R"(The bound transform feedback buffers.

:type: List[VKXFBBuffer]
)");
  rdcarray<XFBBuffer> buffers;

  DOCUMENT(R"(Which stream-out stream is being used for rasterization.

:type: int
)");
  uint32_t rasterizedStream = 0;
};

DOCUMENT("Describes a render area in the current framebuffer.");
struct RenderArea
{
  DOCUMENT("");
  RenderArea() = default;
  RenderArea(const RenderArea &) = default;
  RenderArea &operator=(const RenderArea &) = default;
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
  ViewportScissor &operator=(const ViewportScissor &) = default;

  bool operator==(const ViewportScissor &o) const { return vp == o.vp && scissor == o.scissor; }
  bool operator<(const ViewportScissor &o) const
  {
    if(!(vp == o.vp))
      return vp < o.vp;
    if(!(scissor == o.scissor))
      return scissor < o.scissor;
    return false;
  }
  DOCUMENT(R"(The viewport.

:type: Viewport
)");
  Viewport vp;
  DOCUMENT(R"(The scissor.

:type: Scissor
)");
  Scissor scissor;
};

DOCUMENT("Describes the view state in the pipeline.");
struct ViewState
{
  DOCUMENT("");
  ViewState() = default;
  ViewState(const ViewState &) = default;
  ViewState &operator=(const ViewState &) = default;

  DOCUMENT(R"(The bound viewports and scissors.

:type: List[VKViewportScissor]
)");
  rdcarray<ViewportScissor> viewportScissors;

  DOCUMENT(R"(The discard rectangles, if enabled.

:type: List[VKRenderArea]
)");
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

  DOCUMENT(R"(Whether depth clip range is set to [-1, 1] through VK_EXT_depth_clip_control.)");
  bool depthNegativeOneToOne = false;
};

DOCUMENT("Describes the rasterizer state in the pipeline.");
struct Rasterizer
{
  DOCUMENT("");
  Rasterizer() = default;
  Rasterizer(const Rasterizer &) = default;
  Rasterizer &operator=(const Rasterizer &) = default;

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

  DOCUMENT("Whether depth biasing is enabled.");
  bool depthBiasEnable = false;
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

  DOCUMENT("The line rasterization mode.");
  LineRaster lineRasterMode = LineRaster::Default;
  DOCUMENT("The line stipple factor, or 0 if line stipple is disabled.");
  uint32_t lineStippleFactor = 0;
  DOCUMENT("The line stipple bit-pattern.");
  uint16_t lineStipplePattern = 0;
  DOCUMENT(R"(The current pipeline fragment shading rate. This will always be 1x1 when a fragment
shading rate has not been specified.

:type: Tuple[int,int]
)");
  rdcpair<uint32_t, uint32_t> pipelineShadingRate = {1, 1};
  DOCUMENT(R"(The fragment shading rate combiners.

The combiners are applied as follows, according to the Vulkan spec:

  ``intermediateRate = combiner[0] ( pipelineShadingRate,  shaderExportedShadingRate )``
  ``finalRate        = combiner[1] ( intermediateRate,     imageBasedShadingRate     )``

Where the first input is from :data:`pipelineShadingRate` and the second is the exported shading
rate from the last pre-rasterization shader stage, which defaults to 1x1 if not exported.

The intermediate result is then used as the first input to the second combiner, together with the
shading rate sampled from the fragment shading rate attachment.

:type: Tuple[ShadingRateCombiner,ShadingRateCombiner]
)");
  rdcpair<ShadingRateCombiner, ShadingRateCombiner> shadingRateCombiners = {
      ShadingRateCombiner::Keep, ShadingRateCombiner::Keep};
};

DOCUMENT("Describes state of custom sample locations in the pipeline.");
struct SampleLocations
{
  DOCUMENT("");
  SampleLocations() = default;
  SampleLocations(const SampleLocations &) = default;
  SampleLocations &operator=(const SampleLocations &) = default;

  DOCUMENT("The width in pixels of the region configured.");
  uint32_t gridWidth = 1;
  DOCUMENT("The height in pixels of the region configured.");
  uint32_t gridHeight = 1;
  DOCUMENT(R"(The custom sample locations. Only x and y are valid, z and w are set to 0.0.

If the list is empty then the standard sample pattern is in use.

:type: List[FloatVector]
)");
  rdcarray<FloatVector> customLocations;
};

DOCUMENT("Describes the multisampling state in the pipeline.");
struct MultiSample
{
  DOCUMENT("");
  MultiSample() = default;
  MultiSample(const MultiSample &) = default;
  MultiSample &operator=(const MultiSample &) = default;

  DOCUMENT("How many samples to use when rasterizing.");
  uint32_t rasterSamples = 0;
  DOCUMENT("``True`` if rendering should happen at sample-rate frequency.");
  bool sampleShadingEnable = false;
  DOCUMENT("The minimum sample shading rate.");
  float minSampleShading = 0.0f;
  DOCUMENT("A mask that generated samples should be masked with using bitwise ``AND``.");
  uint32_t sampleMask = 0;
  DOCUMENT(R"(The custom sample locations configuration.

:type: VKSampleLocations
)");
  SampleLocations sampleLocations;
};

DOCUMENT("Describes the pipeline blending state.");
struct ColorBlendState
{
  DOCUMENT("");
  ColorBlendState() = default;
  ColorBlendState(const ColorBlendState &) = default;
  ColorBlendState &operator=(const ColorBlendState &) = default;

  DOCUMENT("``True`` if alpha-to-coverage should be used when blending to an MSAA target.");
  bool alphaToCoverageEnable = false;
  DOCUMENT("``True`` if alpha-to-one should be used when blending to an MSAA target.");
  bool alphaToOneEnable = false;

  DOCUMENT(R"(The blend operations for each target.

:type: List[ColorBlend]
)");
  rdcarray<ColorBlend> blends;

  DOCUMENT(R"(The constant blend factor to use in blend equations.

:type: Tuple[float,float,float,float]
)");
  rdcfixedarray<float, 4> blendFactor = {1.0f, 1.0f, 1.0f, 1.0f};
};

DOCUMENT("Describes the pipeline depth-stencil state.");
struct DepthStencil
{
  DOCUMENT("");
  DepthStencil() = default;
  DepthStencil(const DepthStencil &) = default;
  DepthStencil &operator=(const DepthStencil &) = default;

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

  DOCUMENT(R"(The stencil state for front-facing polygons.

:type: StencilFace
)");
  StencilFace frontFace;

  DOCUMENT(R"(The stencil state for back-facing polygons.

:type: StencilFace
)");
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
  RenderPass &operator=(const RenderPass &) = default;

  DOCUMENT("The :class:`ResourceId` of the render pass.");
  ResourceId resourceId;

  DOCUMENT("Whether or not dynamic rendering is in use (no render pass or framebuffer objects).");
  bool dynamic = false;

  DOCUMENT("Whether or not dynamic rendering is currently suspended.");
  bool suspended = false;

  DOCUMENT("Whether or not there is a potential feedback loop.");
  bool feedbackLoop = false;

  DOCUMENT("The index of the current active subpass.");
  uint32_t subpass;

  // VKTODOMED renderpass and subpass information here

  DOCUMENT(R"(The input attachments for the current subpass, as indices into the framebuffer
attachments.

:type: List[int]
)");
  rdcarray<uint32_t> inputAttachments;
  DOCUMENT(R"(The color attachments for the current subpass, as indices into the framebuffer
attachments.

:type: List[int]
)");
  rdcarray<uint32_t> colorAttachments;
  DOCUMENT(R"(The resolve attachments for the current subpass, as indices into the framebuffer
attachments.

:type: List[int]
)");
  rdcarray<uint32_t> resolveAttachments;
  DOCUMENT(R"(An index into the framebuffer attachments for the depth-stencil attachment.

If there is no depth-stencil attachment, this index is ``-1``.
)");
  int32_t depthstencilAttachment = -1;
  DOCUMENT(R"(An index into the framebuffer attachments for the depth-stencil resolve attachment.

If there is no depth-stencil resolve attachment, this index is ``-1``.
)");
  int32_t depthstencilResolveAttachment = -1;

  DOCUMENT(R"(An index into the framebuffer attachments for the fragment density attachment.

If there is no fragment density attachment, this index is ``-1``.

.. note::
  Only one at most of :data:`fragmentDensityAttachment` and :data:`shadingRateAttachment` will be
  set.
)");
  int32_t fragmentDensityAttachment = -1;

  DOCUMENT(R"(An index into the framebuffer attachments for the fragment shading rate attachment.

If there is no fragment shading rate attachment, this index is ``-1``.

.. note::
  Only one at most of :data:`fragmentDensityAttachment` and :data:`shadingRateAttachment` will be
  set.
)");
  int32_t shadingRateAttachment = -1;

  DOCUMENT(R"(The size of the framebuffer region represented by each texel in
:data:`shadingRateAttachment`.

For example if this is (2,2) then every texel in the attachment gives the shading rate of a 2x2
block in the framebuffer so the shading rate attachment is half the size of the other attachments in
each dimension.

If no attachment is set in :data:`shadingRateAttachment` this will be (1,1).

:type: Tuple[int,int]
)");
  rdcpair<uint32_t, uint32_t> shadingRateTexelSize = {1, 1};

  DOCUMENT(R"(If multiview is enabled, contains a list of view indices to be broadcast to during
rendering.

If the list is empty, multiview is disabled and rendering is as normal.

:type: List[int]
)");
  rdcarray<uint32_t> multiviews;

  DOCUMENT(R"(If VK_QCOM_fragment_density_map_offset is enabled, contains a list of offsets applied 
to the fragment density map during rendering.

If the list is empty, fdm_offset is disabled and rendering is as normal.

:type: List[Offset]
)");
  rdcarray<Offset> fragmentDensityOffsets;

  DOCUMENT(R"(If VK_EXT_multisampled_render_to_single_sampled is enabled, contains the number of
samples used to render this subpass.

If the subpass is not internally multisampled, tileOnlyMSAASampleCount is set to 0.
)");
  uint32_t tileOnlyMSAASampleCount = 0;
};

DOCUMENT("Describes a single attachment in a framebuffer object.");
struct Attachment
{
  DOCUMENT("");
  Attachment() = default;
  Attachment(const Attachment &) = default;
  Attachment &operator=(const Attachment &) = default;

  bool operator==(const Attachment &o) const
  {
    return viewResourceId == o.viewResourceId && imageResourceId == o.imageResourceId &&
           viewFormat == o.viewFormat && swizzle == o.swizzle && firstMip == o.firstMip &&
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
    if(!(swizzle == o.swizzle))
      return swizzle < o.swizzle;
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

  DOCUMENT(R"(The format cast that the view uses.

:type: ResourceFormat
)");
  ResourceFormat viewFormat;
  DOCUMENT(R"(The swizzle applied to the texture by the view.

:type: TextureSwizzle4
)");
  TextureSwizzle4 swizzle;
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
  Framebuffer &operator=(const Framebuffer &) = default;

  DOCUMENT("The :class:`ResourceId` of the framebuffer object.");
  ResourceId resourceId;

  DOCUMENT(R"(The attachments of this framebuffer.

:type: List[VKAttachment]
)");
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
  CurrentPass &operator=(const CurrentPass &) = default;

  DOCUMENT(R"(The renderpass and subpass that is currently active.

:type: VKRenderPass
)");
  RenderPass renderpass;
  DOCUMENT(R"(The framebuffer that is currently being used.

:type: VKFramebuffer
)");
  Framebuffer framebuffer;
  DOCUMENT(R"(The render area that is currently being rendered to.

:type: VKRenderArea
)");
  RenderArea renderArea;
};

DOCUMENT("Contains the layout of a range of subresources in an image.");
struct ImageLayout
{
  DOCUMENT("");
  ImageLayout() = default;
  ImageLayout(const ImageLayout &) = default;
  ImageLayout &operator=(const ImageLayout &) = default;

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
  ImageData &operator=(const ImageData &) = default;

  bool operator==(const ImageData &o) const { return resourceId == o.resourceId; }
  bool operator<(const ImageData &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the image.");
  ResourceId resourceId;

  DOCUMENT(R"(The subresource regions in this resource.

:type: List[VKImageLayout]
)");
  rdcarray<ImageLayout> layouts;
};

DOCUMENT("Contains the current conditional rendering state.");
struct ConditionalRendering
{
  DOCUMENT("");
  ConditionalRendering() = default;
  ConditionalRendering(const ConditionalRendering &) = default;
  ConditionalRendering &operator=(const ConditionalRendering &) = default;

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

  DOCUMENT(R"(The currently bound compute pipeline, if any.

:type: VKPipeline
)");
  Pipeline compute;
  DOCUMENT(R"(The currently bound graphics pipeline, if any.

:type: VKPipeline
)");
  Pipeline graphics;

  DOCUMENT(R"(The raw push constant data.

:type: bytes
)");
  bytebuf pushconsts;

  DOCUMENT(R"(The input assembly stage.

:type: VKInputAssembly
)");
  InputAssembly inputAssembly;
  DOCUMENT(R"(The vertex input stage.

:type: VKVertexInput
)");
  VertexInput vertexInput;

  DOCUMENT(R"(The vertex shader stage.

:type: VKShader
)");
  Shader vertexShader;
  DOCUMENT(R"(The tessellation control shader stage.

:type: VKShader
)");
  Shader tessControlShader;
  DOCUMENT(R"(The tessellation evaluation shader stage.

:type: VKShader
)");
  Shader tessEvalShader;
  DOCUMENT(R"(The geometry shader stage.

:type: VKShader
)");
  Shader geometryShader;
  DOCUMENT(R"(The fragment shader stage.

:type: VKShader
)");
  Shader fragmentShader;
  DOCUMENT(R"(The compute shader stage.

:type: VKShader
)");
  Shader computeShader;

  DOCUMENT(R"(The tessellation stage.

:type: VKTessellation
)");
  Tessellation tessellation;

  DOCUMENT(R"(The transform feedback stage.

:type: VKTransformFeedback
)");
  TransformFeedback transformFeedback;

  DOCUMENT(R"(The viewport setup.

:type: VKViewState
)");
  ViewState viewportScissor;
  DOCUMENT(R"(The rasterization configuration.

:type: VKRasterizer
)");
  Rasterizer rasterizer;

  DOCUMENT(R"(The multisampling configuration.

:type: VKMultiSample
)");
  MultiSample multisample;
  DOCUMENT(R"(The color blending configuration.

:type: VKColorBlendState
)");
  ColorBlendState colorBlend;
  DOCUMENT(R"(The depth-stencil state.

:type: VKDepthStencil
)");
  DepthStencil depthStencil;

  DOCUMENT(R"(The current renderpass, subpass and framebuffer.

:type: VKCurrentPass
)");
  CurrentPass currentPass;

  DOCUMENT(R"(The resource states for the currently live resources.

:type: List[VKImageData]
)");
  rdcarray<ImageData> images;

  DOCUMENT(R"(The shader messages retrieved for this action.

:type: List[ShaderMessage]
)");
  rdcarray<ShaderMessage> shaderMessages;

  DOCUMENT(R"(The current conditional rendering state.

:type: VKConditionalRendering
)");
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
