/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "apidefs.h"
#include "data_types.h"
#include "rdcarray.h"
#include "shader_types.h"
#include "stringise.h"

DOCUMENT("Information about a viewport.");
struct Viewport
{
  DOCUMENT("");
  Viewport() = default;
  Viewport(float X, float Y, float W, float H, float MN, float MX, bool en = true)
      : x(X), y(Y), width(W), height(H), minDepth(MN), maxDepth(MX), enabled(en)
  {
  }
  Viewport(const Viewport &) = default;
  Viewport &operator=(const Viewport &) = default;

  bool operator==(const Viewport &o) const
  {
    return x == o.x && y == o.y && width == o.width && height == o.height &&
           minDepth == o.minDepth && maxDepth == o.maxDepth;
  }
  bool operator<(const Viewport &o) const
  {
    if(!(x == o.x))
      return x < o.x;
    if(!(y == o.y))
      return y < o.y;
    if(!(width == o.width))
      return width < o.width;
    if(!(height == o.height))
      return height < o.height;
    if(!(minDepth == o.minDepth))
      return minDepth < o.minDepth;
    if(!(maxDepth == o.maxDepth))
      return maxDepth < o.maxDepth;
    return false;
  }

  DOCUMENT("Is this viewport enabled.");
  bool enabled = true;
  DOCUMENT("The X co-ordinate of the viewport.");
  float x = 0.0f;
  DOCUMENT("The Y co-ordinate of the viewport.");
  float y = 0.0f;
  DOCUMENT("The width of the viewport.");
  float width = 0.0f;
  DOCUMENT("The height of the viewport.");
  float height = 0.0f;
  DOCUMENT("The minimum depth of the viewport.");
  float minDepth = 0.0f;
  DOCUMENT("The maximum depth of the viewport.");
  float maxDepth = 0.0f;
};

DECLARE_REFLECTION_STRUCT(Viewport);

DOCUMENT("Describes a single scissor region.");
struct Scissor
{
  DOCUMENT("");
  Scissor() = default;
  Scissor(int X, int Y, int W, int H, bool en = true) : x(X), y(Y), width(W), height(H), enabled(en)
  {
  }
  Scissor(const Scissor &) = default;
  Scissor &operator=(const Scissor &) = default;

  bool operator==(const Scissor &o) const
  {
    return x == o.x && y == o.y && width == o.width && height == o.height && enabled == o.enabled;
  }
  bool operator<(const Scissor &o) const
  {
    if(!(x == o.x))
      return x < o.x;
    if(!(y == o.y))
      return y < o.y;
    if(!(width == o.width))
      return width < o.width;
    if(!(height == o.height))
      return height < o.height;
    if(!(enabled == o.enabled))
      return enabled < o.enabled;
    return false;
  }
  DOCUMENT("X co-ordinate of the scissor region.");
  int32_t x = 0;
  DOCUMENT("Y co-ordinate of the scissor region.");
  int32_t y = 0;
  DOCUMENT("Width of the scissor region.");
  int32_t width = 0;
  DOCUMENT("Height of the scissor region.");
  int32_t height = 0;
  DOCUMENT("``True`` if this scissor region is enabled.");
  bool enabled = true;
};

DECLARE_REFLECTION_STRUCT(Scissor);

DOCUMENT("Describes the details of a blend operation.");
struct BlendEquation
{
  DOCUMENT("");
  BlendEquation() = default;
  BlendEquation(const BlendEquation &) = default;
  BlendEquation &operator=(const BlendEquation &) = default;

  bool operator==(const BlendEquation &o) const
  {
    return source == o.source && destination == o.destination && operation == o.operation;
  }
  bool operator<(const BlendEquation &o) const
  {
    if(!(source == o.source))
      return source < o.source;
    if(!(destination == o.destination))
      return destination < o.destination;
    if(!(operation == o.operation))
      return operation < o.operation;
    return false;
  }
  DOCUMENT("The :class:`BlendMultiplier` for the source blend value.");
  BlendMultiplier source = BlendMultiplier::One;
  DOCUMENT("The :class:`BlendMultiplier` for the destination blend value.");
  BlendMultiplier destination = BlendMultiplier::One;
  DOCUMENT("The :class:`BlendOperation` to use in the blend calculation.");
  BlendOperation operation = BlendOperation::Add;
};

DECLARE_REFLECTION_STRUCT(BlendEquation);

DOCUMENT("Describes the blend configuration for a given output target.");
struct ColorBlend
{
  DOCUMENT("");
  ColorBlend() = default;
  ColorBlend(const ColorBlend &) = default;
  ColorBlend &operator=(const ColorBlend &) = default;

  bool operator==(const ColorBlend &o) const
  {
    return enabled == o.enabled && logicOperationEnabled == o.logicOperationEnabled &&
           colorBlend == o.colorBlend && alphaBlend == o.alphaBlend &&
           logicOperation == o.logicOperation && writeMask == o.writeMask;
  }
  bool operator<(const ColorBlend &o) const
  {
    if(!(enabled == o.enabled))
      return enabled < o.enabled;
    if(!(logicOperationEnabled == o.logicOperationEnabled))
      return logicOperationEnabled < o.logicOperationEnabled;
    if(!(colorBlend == o.colorBlend))
      return colorBlend < o.colorBlend;
    if(!(alphaBlend == o.alphaBlend))
      return alphaBlend < o.alphaBlend;
    if(!(logicOperation == o.logicOperation))
      return logicOperation < o.logicOperation;
    if(!(writeMask == o.writeMask))
      return writeMask < o.writeMask;
    return false;
  }

  DOCUMENT(R"(The blending equation for color values.
    
:type: BlendEquation
)");
  BlendEquation colorBlend;
  DOCUMENT(R"(The blending equation for alpha values.
    
:type: BlendEquation
)");
  BlendEquation alphaBlend;

  DOCUMENT(R"(The :class:`LogicOperation` to use for logic operations, if
:data:`logicOperationEnabled` is ``True``.
)");
  LogicOperation logicOperation = LogicOperation::NoOp;

  DOCUMENT("``True`` if blending is enabled for this target.");
  bool enabled = false;
  DOCUMENT("``True`` if the logic operation in :data:`logicOperation` should be used.");
  bool logicOperationEnabled = false;
  DOCUMENT("The mask for writes to the render target.");
  byte writeMask = 0;
};

DECLARE_REFLECTION_STRUCT(ColorBlend);

DOCUMENT("Describes the details of a stencil operation.");
struct StencilFace
{
  DOCUMENT("");
  StencilFace() = default;
  StencilFace(const StencilFace &) = default;
  StencilFace &operator=(const StencilFace &) = default;

  DOCUMENT("The :class:`StencilOperation` to apply if the stencil-test fails.");
  StencilOperation failOperation = StencilOperation::Keep;
  DOCUMENT("the :class:`StencilOperation` to apply if the depth-test fails.");
  StencilOperation depthFailOperation = StencilOperation::Keep;
  DOCUMENT("the :class:`StencilOperation` to apply if the stencil-test passes.");
  StencilOperation passOperation = StencilOperation::Keep;
  DOCUMENT("the :class:`CompareFunction` to use for testing stencil values.");
  CompareFunction function = CompareFunction::AlwaysTrue;
  DOCUMENT("The current stencil reference value.");
  uint32_t reference = 0;
  DOCUMENT("The mask for testing stencil values.");
  uint32_t compareMask = 0;
  DOCUMENT("The mask for writing stencil values.");
  uint32_t writeMask = 0;
};

DECLARE_REFLECTION_STRUCT(StencilFace);

DOCUMENT("Information about a single vertex or index buffer binding.");
struct BoundVBuffer
{
  DOCUMENT("");
  BoundVBuffer() = default;
  BoundVBuffer(const BoundVBuffer &) = default;
  BoundVBuffer &operator=(const BoundVBuffer &) = default;

  bool operator==(const BoundVBuffer &o) const
  {
    return resourceId == o.resourceId && byteOffset == o.byteOffset && byteStride == o.byteStride &&
           byteSize == o.byteSize;
  }
  bool operator<(const BoundVBuffer &o) const
  {
    if(resourceId != o.resourceId)
      return resourceId < o.resourceId;
    if(byteOffset != o.byteOffset)
      return byteOffset < o.byteOffset;
    if(byteStride != o.byteStride)
      return byteStride < o.byteStride;
    if(byteSize != o.byteSize)
      return byteSize < o.byteSize;
    return false;
  }
  DOCUMENT("A :class:`~renderdoc.ResourceId` identifying the buffer.");
  ResourceId resourceId;
  DOCUMENT("The offset in bytes from the start of the buffer to the data.");
  uint64_t byteOffset = 0;
  DOCUMENT("The stride in bytes between the start of one element and the start of the next.");
  uint32_t byteStride = 0;
  DOCUMENT("The size of the buffer binding, or 0xFFFFFFFF if the whole buffer is bound.");
  uint64_t byteSize = 0;
};

DECLARE_REFLECTION_STRUCT(BoundVBuffer);

DOCUMENT(R"(The contents of a descriptor. Not all contents will be valid depending on API and
descriptor type, others will be set to sensible defaults.

For sampler descriptors, the sampler-specific data can be queried separately and returned as
:class:`SamplerDescriptor` for sampler types.
)");
struct Descriptor
{
  DOCUMENT("");
  Descriptor() = default;
  Descriptor(const Descriptor &) = default;
  Descriptor &operator=(const Descriptor &) = default;

  bool operator==(const Descriptor &o) const
  {
    return type == o.type && flags == o.flags && format == o.format && resource == o.resource &&
           secondary == o.secondary && view == o.view && byteOffset == o.byteOffset &&
           byteSize == o.byteSize && counterByteOffset == o.counterByteOffset &&
           bufferStructCount == o.bufferStructCount && elementByteSize == o.elementByteSize &&
           firstSlice == o.firstSlice && numSlices == o.numSlices && firstMip == o.firstMip &&
           numMips == o.numMips && swizzle == o.swizzle && textureType == o.textureType;
  }
  bool operator<(const Descriptor &o) const
  {
    if(type != o.type)
      return type < o.type;
    if(flags != o.flags)
      return flags < o.flags;
    if(format != o.format)
      return format < o.format;
    if(resource != o.resource)
      return resource < o.resource;
    if(secondary != o.secondary)
      return secondary < o.secondary;
    if(view != o.view)
      return view < o.view;
    if(byteOffset != o.byteOffset)
      return byteOffset < o.byteOffset;
    if(byteSize != o.byteSize)
      return byteSize < o.byteSize;
    if(counterByteOffset != o.counterByteOffset)
      return counterByteOffset < o.counterByteOffset;
    if(bufferStructCount != o.bufferStructCount)
      return bufferStructCount < o.bufferStructCount;
    if(elementByteSize != o.elementByteSize)
      return elementByteSize < o.elementByteSize;
    if(firstSlice != o.firstSlice)
      return firstSlice < o.firstSlice;
    if(numSlices != o.numSlices)
      return numSlices < o.numSlices;
    if(firstMip != o.firstMip)
      return firstMip < o.firstMip;
    if(numMips != o.numMips)
      return numMips < o.numMips;
    if(!(swizzle == o.swizzle))
      return swizzle < o.swizzle;
    if(textureType != o.textureType)
      return textureType < o.textureType;
    return false;
  }

  DOCUMENT(R"(The type of this descriptor as a general category.

:type: DescriptorType
)");
  DescriptorType type = DescriptorType::Unknown;

  DOCUMENT(R"(The flags for additional API-specific and generally non-semantically impactful
properties.

:type: DescriptorFlags
)");
  DescriptorFlags flags = DescriptorFlags::NoFlags;

  DOCUMENT(R"(The format cast that the view uses, for typed buffer and image descriptors.

:type: ResourceFormat
)");
  ResourceFormat format;

  DOCUMENT(R"(The primary bound resource at this descriptor, either a buffer or an image resource.

Note that sampler descriptors will not be listed here, see :data:`secondary`.

:type: ResourceId
)");
  ResourceId resource;

  DOCUMENT(R"(The secondary bound resource at this descriptor.

For any descriptor containing a sampler, this will be the sampler. For buffer descriptors with an
associated counter buffer this will be the counter buffer.

:type: ResourceId
)");
  ResourceId secondary;

  DOCUMENT(R"(The view object used to create this descriptor, which formats or subsets the
bound resource referenced by :data:`resource`.

:type: ResourceId
)");
  ResourceId view;

  DOCUMENT(R"(For any kind of buffer descriptor, the base byte offset within the resource where the
referenced range by the descriptor begins.

:type: int
)");
  uint64_t byteOffset = 0;

  DOCUMENT(R"(For any kind of buffer descriptor, the number of bytes in the range covered by the
descriptor.

:type: int
)");
  uint64_t byteSize = 0;

  DOCUMENT(R"(The byte offset in :data:`secondary` where the counter is stored, for buffer
descriptors with a secondary counter.

:type: int
)");
  uint32_t counterByteOffset = 0;

  DOCUMENT(R"(If the view has a hidden counter, this stores the current value of the counter.

:type: int
)");
  uint32_t bufferStructCount = 0;

  DOCUMENT(R"(The byte size of a single element in the view. Either the byte size of
:data:`viewFormat`, or the structured buffer element size, as appropriate.

:type: int
)");
  uint32_t elementByteSize = 0;

  DOCUMENT(R"(The clamp applied to the minimum LOD by the resource view, separate and in addition to
any clamp by a sampler used.

:type: float
)");
  float minLODClamp = 0.0f;

  DOCUMENT(R"(For texture descriptors, the first slice in a 3D or array texture which is visible
to the descriptor

:type: int
)");
  uint16_t firstSlice = 0;
  DOCUMENT(R"(For texture descriptors, the number of slices in a 3D or array texture which are visible
to the descriptor

:type: int
)");
  uint16_t numSlices = 1;

  DOCUMENT(R"(For texture descriptors, the first mip in the texture which is visible to the
descriptor

:type: int
)");
  uint8_t firstMip = 0;

  DOCUMENT(R"(For texture descriptors, the number of mips in the texture which are visible to the
descriptor

:type: int
)");
  uint8_t numMips = 1;

  DOCUMENT(R"(The swizzle applied to texture descriptors.

:type: TextureSwizzle4
)");
  TextureSwizzle4 swizzle;

  DOCUMENT(R"(The specific type of a texture descriptor.

:type: TextureType
)");
  TextureType textureType = TextureType::Unknown;
};

DECLARE_REFLECTION_STRUCT(Descriptor);

DOCUMENT(R"(The contents of a sampler descriptor. Not all contents will be valid depending on API
and capabilities, others will be set to sensible defaults.

For normal descriptors, the resource data should be queried and returned in :class:`Descriptor`.
)");
struct SamplerDescriptor
{
  DOCUMENT("");
  SamplerDescriptor() = default;
  SamplerDescriptor(const SamplerDescriptor &) = default;
  SamplerDescriptor &operator=(const SamplerDescriptor &) = default;

  bool operator==(const SamplerDescriptor &o) const
  {
    return type == o.type && object == o.object && addressU == o.addressU && addressV == o.addressV &&
           addressW == o.addressW && compareFunction == o.compareFunction && filter == o.filter &&
           maxAnisotropy == o.maxAnisotropy && maxLOD == o.maxLOD && minLOD == o.minLOD &&
           mipBias == o.mipBias && borderColorValue.uintValue == o.borderColorValue.uintValue &&
           borderColorType == o.borderColorType && srgbBorder == o.srgbBorder &&
           seamlessCubemaps == o.seamlessCubemaps && unnormalized == o.unnormalized;
  }
  bool operator<(const SamplerDescriptor &o) const
  {
    if(!(type == o.type))
      return type < o.type;
    if(!(object == o.object))
      return object < o.object;
    if(!(addressU == o.addressU))
      return addressU < o.addressU;
    if(!(addressV == o.addressV))
      return addressV < o.addressV;
    if(!(addressW == o.addressW))
      return addressW < o.addressW;
    if(!(compareFunction == o.compareFunction))
      return compareFunction < o.compareFunction;
    if(!(filter == o.filter))
      return filter < o.filter;
    if(!(maxAnisotropy == o.maxAnisotropy))
      return maxAnisotropy < o.maxAnisotropy;
    if(!(maxLOD == o.maxLOD))
      return maxLOD < o.maxLOD;
    if(!(minLOD == o.minLOD))
      return minLOD < o.minLOD;
    if(!(mipBias == o.mipBias))
      return mipBias < o.mipBias;
    if(!(borderColorValue.uintValue == o.borderColorValue.uintValue))
      return borderColorValue.uintValue < o.borderColorValue.uintValue;
    if(!(borderColorType == o.borderColorType))
      return borderColorType < o.borderColorType;
    if(!(srgbBorder == o.srgbBorder))
      return srgbBorder < o.srgbBorder;
    if(!(seamlessCubemaps == o.seamlessCubemaps))
      return seamlessCubemaps < o.seamlessCubemaps;
    if(!(unnormalized == o.unnormalized))
      return unnormalized < o.unnormalized;
    return false;
  }

  DOCUMENT(R"(For APIs where samplers are an explicit object, the :class:`ResourceId` of the sampler
itself

:type: ResourceId
)");
  ResourceId object;

  DOCUMENT(R"(The type of this descriptor as a general category.

If this is not set to :data:`DescriptorType.Sampler` or :data:`DescriptorType.ImageSampler` the rest
of the contents of this structure are not valid as the descriptor is not a sampler descriptor.

:type: DescriptorType
)");
  DescriptorType type = DescriptorType::Unknown;

  DOCUMENT("The :class:`AddressMode` in the U direction.");
  AddressMode addressU = AddressMode::Wrap;
  DOCUMENT("The :class:`AddressMode` in the V direction.");
  AddressMode addressV = AddressMode::Wrap;
  DOCUMENT("The :class:`AddressMode` in the W direction.");
  AddressMode addressW = AddressMode::Wrap;
  DOCUMENT("The :class:`CompareFunction` for comparison samplers.");
  CompareFunction compareFunction = CompareFunction::AlwaysTrue;

  DOCUMENT(R"(The filtering mode.

:type: TextureFilter
)");
  TextureFilter filter;
  DOCUMENT("``True`` if the border colour is swizzled with an sRGB formatted image.");
  bool srgbBorder = false;
  DOCUMENT("``True`` if this sampler is seamless across cubemap boundaries (the default).");
  bool seamlessCubemaps = true;
  DOCUMENT("``True`` if unnormalized co-ordinates are used in this sampler.");
  bool unnormalized = false;

  DOCUMENT("The maximum anisotropic filtering level to use.");
  float maxAnisotropy = 0;
  DOCUMENT("The maximum mip level that can be used.");
  float maxLOD = 0.0f;
  DOCUMENT("The minimum mip level that can be used.");
  float minLOD = 0.0f;
  DOCUMENT("A bias to apply to the calculated mip level before sampling.");
  float mipBias = 0.0f;

  DOCUMENT(R"(The RGBA border color value. Typically the float tuple inside will be used,
but the exact component type can be checked with :data:`borderColorType`.

:type: PixelValue
)");
  PixelValue borderColorValue = {};
  DOCUMENT(R"(The RGBA border color type. This determines how the data in
:data:`borderColorValue` will be interpreted.

:type: CompType
)");
  CompType borderColorType = CompType::Float;
  DOCUMENT(R"(The swizzle applied. Primarily for ycbcr samplers applied before
conversion but for non-ycbcr samplers can be used for implementations that require sampler swizzle
information for border colors.

:type: TextureSwizzle4
)");
  TextureSwizzle4 swizzle;

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
  bool forceExplicitReconstruction = false;

  DOCUMENT(R"(``True`` if this sampler was initialised at creation time for a pipeline or
descriptor layout, the method being API specific. If so this sampler is not dynamic and was not
explicitly set and may have no real descriptor storage.

:type: bool
)");
  bool creationTimeConstant = false;

  DOCUMENT(R"(The :class:`ResourceId` of the ycbcr conversion object associated with
this sampler.
)");
  ResourceId ycbcrSampler;

  DOCUMENT(R"(Check if the border color is used in this D3D11 sampler.

:return: ``True`` if the border color is used, ``False`` otherwise.
:rtype: bool
)");
  bool UseBorder() const
  {
    return addressU == AddressMode::ClampBorder || addressV == AddressMode::ClampBorder ||
           addressW == AddressMode::ClampBorder;
  }
};

DECLARE_REFLECTION_STRUCT(SamplerDescriptor);

DOCUMENT(R"(The details of a single accessed descriptor as fetched by a shader and which descriptor
in the descriptor store was fetched.

This may be a somewhat conservative access, reported as possible but not actually executed on the
GPU itself.

.. data:: NoShaderBinding

  No shader binding corresponds to this descriptor access, it happened directly without going
  through any kind of binding.
)");
struct DescriptorAccess
{
  DOCUMENT("");
  DescriptorAccess() = default;
  DescriptorAccess(const DescriptorAccess &) = default;
  DescriptorAccess &operator=(const DescriptorAccess &) = default;

  bool operator==(const ShaderBindIndex &o) const
  {
    return CategoryForDescriptorType(type) == o.category && index == o.index &&
           arrayElement == o.arrayElement;
  }
  bool operator==(const DescriptorAccess &o) const
  {
    return stage == o.stage && type == o.type && index == o.index &&
           arrayElement == o.arrayElement && descriptorStore == o.descriptorStore &&
           byteOffset == o.byteOffset && byteSize == o.byteSize;
  }
  bool operator<(const DescriptorAccess &o) const
  {
    if(stage != o.stage)
      return stage < o.stage;
    if(type != o.type)
      return type < o.type;
    if(index != o.index)
      return index < o.index;
    if(arrayElement != o.arrayElement)
      return arrayElement < o.arrayElement;
    if(descriptorStore != o.descriptorStore)
      return descriptorStore < o.descriptorStore;
    if(byteOffset != o.byteOffset)
      return byteOffset < o.byteOffset;
    if(byteSize != o.byteSize)
      return byteSize < o.byteSize;
    return false;
  }

  DOCUMENT(R"(The shader stage that this descriptor access came from.

:type: ShaderStage
)");
  ShaderStage stage = ShaderStage::Count;
  DOCUMENT(R"(The type of the descriptor being accessed.

:type: DescriptorType
)");
  DescriptorType type = DescriptorType::Unknown;

  DOCUMENT(R"(The index within the shader's reflection list corresponding to :data:`type` of the
accessing resource.

If this value is set to :data:`NoShaderBinding` then the shader synthesised a direct access into
descriptor storage without passing through a declared binding.

:type: int
)");
  uint16_t index = 0;

  static const uint16_t NoShaderBinding = 0xFFFF;

  DOCUMENT(R"(For an arrayed resource declared in a shader, the array element used.

:type: int
)");
  uint32_t arrayElement = 0;

  DOCUMENT(R"(The backing storage of the descriptor.

:type: ResourceId
)");
  ResourceId descriptorStore;
  DOCUMENT(R"(The offset in bytes to the descriptor in the descriptor store.

:type: int
)");
  uint32_t byteOffset = 0;
  DOCUMENT(R"(The size in bytes of the descriptor.

:type: int
)");
  uint32_t byteSize = 0;

  DOCUMENT(R"(For informational purposes, some descriptors that are declared in the shader
interface but are provably unused may still be reported as descriptor accesses. This flag will be
set to ``True`` to indicate that the descriptor was definitely not used.

This flag only states that a descriptor is definitely unused on all paths. If set to ``False`` this
does not necessarily guarantee that the descriptor was accessed on the GPU during execution.

:type: bool
)");
  bool staticallyUnused = false;
};

DECLARE_REFLECTION_STRUCT(DescriptorAccess);

inline ShaderBindIndex::ShaderBindIndex(const DescriptorAccess &access)
    : ShaderBindIndex(CategoryForDescriptorType(access.type), access.index, access.arrayElement)
{
}

DOCUMENT(R"(In many cases there may be a logical location or fixed binding point for a particular
descriptor which is not conveyed with a simple byte offset into a descriptor store.
This is particularly true for any descriptor stores that are not equivalent to a buffer of bytes
but actually have an API structure - for example D3D11 and GL with fixed binding points, or Vulkan
with descriptor sets.

In some cases on APIs with explicit descriptor storage this may convey information about virtualised
descriptors that are not explicitly backed with real storage.

This structure describes such a location queried for a given descriptor.

For example on D3D11 this would give the register number of the binding, and on GL it would give the
unit index. Both cases would be able to query the type and shader stage visibility of descriptors
that are not accessed or even bound.

On Vulkan this would give the set, binding, and visibility. In most cases this information will be
available for all descriptors but in some cases the type of descriptor may not be available if it
is unused and has not been initialised.

On D3D12 this would only give the index into the heap, as no other information is available purely
by the descriptor itself.

.. note::

  This information may not be fully present on all APIs so the returned structures may be empty or
  partially filled out, depending on what information is relevant per API.
)");
struct DescriptorLogicalLocation
{
  DOCUMENT("");
  DescriptorLogicalLocation() = default;
  DescriptorLogicalLocation(const DescriptorLogicalLocation &) = default;
  DescriptorLogicalLocation &operator=(const DescriptorLogicalLocation &) = default;

  bool operator==(const DescriptorLogicalLocation &o) const
  {
    return fixedBindNumber == o.fixedBindNumber && stageMask == o.stageMask &&
           category == o.category && logicalBindName == o.logicalBindName;
  }
  bool operator<(const DescriptorLogicalLocation &o) const
  {
    // note there are two different conflicting sorts that would be sensible here:
    // stage > category > fixed bind would work best for D3D11/GL
    // fixed bind first works better for Vulkan/D3D12.
    //
    // We assume users will access or sort D3D11/GL descriptors directly by binds if needed so are
    // less likely to rely on this sort behaviour.
    if(fixedBindNumber != o.fixedBindNumber)
      return fixedBindNumber < o.fixedBindNumber;
    if(stageMask != o.stageMask)
      return stageMask < o.stageMask;
    if(category != o.category)
      return category < o.category;
    if(logicalBindName != o.logicalBindName)
      return logicalBindName < o.logicalBindName;
    return false;
  }

  DOCUMENT(R"(The set of shader stages that this descriptor is intrinsically available to. This is
primarily relevant for D3D11 with its fixed per-stage register binding points.

Note this *only* shows if a descriptor itself can only ever be accessed by some shader stages by
definition, not if a descriptor is generally available but happened to only be accessed by one or
more stage. That information is available directly in the :class:`DescriptorAccess` itself.

:type: ShaderStageMask
)");
  ShaderStageMask stageMask = ShaderStageMask::All;

  DOCUMENT(R"(The general category of a descriptor stored. This may not be available for
uninitialised descriptors on all APIs.

:type: DescriptorCategory
)");
  DescriptorCategory category = DescriptorCategory::Unknown;

  DOCUMENT(R"(The fixed binding number for this descriptor. The interpretation of this is
API-specific and it is provided purely for informational purposes and has no bearing on how data
is accessed or described.

Generally speaking sorting by this number will give a reasonable ordering by binding if it exists.

.. note::
  Because this number is API-specific, there is no guarantee that it will be unique across all
  descriptors. It should be used only within contexts that can interpret it API-specifically, or
  else for purely informational/non-semantic purposes like sorting.

:type: int
)");
  uint32_t fixedBindNumber = 0;

  DOCUMENT(R"(The logical binding name, as suitable for displaying to a user when displaying
the contents of a descriptor queried directly from a heap.

Depending on the API, this name may be identical or less specific than the one obtained from
shader reflection. Generally speaking it's preferred to use any information from shader reflection
first, and fall back to this name if no reflection information is available in the context.

:type: str
)");
  rdcinflexiblestr logicalBindName;
};

DECLARE_REFLECTION_STRUCT(DescriptorLogicalLocation);

DOCUMENT(R"(Combined information about a single descriptor that has been used, both the information
about its access and its contents.

This is a helper struct for the common pipeline state abstraction to trade off simplicity of access
against optimal access.
)");
struct UsedDescriptor
{
  DOCUMENT("");
  UsedDescriptor() = default;
  UsedDescriptor(const UsedDescriptor &) = default;
  UsedDescriptor &operator=(const UsedDescriptor &) = default;

  bool operator==(const DescriptorAccess &o) const { return access == o; }
  bool operator==(const ShaderBindIndex &o) const { return access == o; }
  bool operator==(const UsedDescriptor &o) const
  {
    return access == o.access && descriptor == o.descriptor && sampler == o.sampler;
  }
  bool operator<(const UsedDescriptor &o) const
  {
    if(!(access == o.access))
      return access < o.access;
    if(!(descriptor == o.descriptor))
      return descriptor < o.descriptor;
    if(!(sampler == o.sampler))
      return sampler < o.sampler;
    return false;
  }

  DOCUMENT(R"(The access information of which shader reflection object accessed which descriptor.

:type: DescriptorAccess
)");
  DescriptorAccess access;

  DOCUMENT(R"(The contents of the accessed descriptor, if it is a normal non-sampler descriptor.

For sampler descriptors this is empty.

:type: Descriptor
)");
  Descriptor descriptor;

  DOCUMENT(R"(The contents of the accessed descriptor, if it is a sampler descriptor.

For normal descriptors this is empty.

:type: SamplerDescriptor
)");
  SamplerDescriptor sampler;
};

DECLARE_REFLECTION_STRUCT(UsedDescriptor);

DOCUMENT("Describes a 2-dimensional int offset");
struct Offset
{
  DOCUMENT("");
  Offset() = default;
  Offset(const Offset &) = default;
  Offset(int32_t x, int32_t y) : x(x), y(y){};
  Offset &operator=(const Offset &) = default;

  bool operator==(const Offset &o) const { return x == o.x && y == o.y; }
  bool operator<(const Offset &o) const
  {
    if(x != o.x)
      return x < o.x;
    return y < o.y;
  }
  DOCUMENT(R"(The X offset value.

:type: int
)");
  int32_t x = 0;

  DOCUMENT(R"(The Y offset value.

:type: int
)");
  int32_t y = 0;
};

DECLARE_REFLECTION_STRUCT(Offset);

DOCUMENT("Information about a vertex input attribute feeding the vertex shader.");
struct VertexInputAttribute
{
  DOCUMENT("");
  VertexInputAttribute() = default;
  VertexInputAttribute(const VertexInputAttribute &) = default;
  VertexInputAttribute &operator=(const VertexInputAttribute &) = default;

  bool operator==(const VertexInputAttribute &o) const
  {
    return name == o.name && vertexBuffer == o.vertexBuffer && byteOffset == o.byteOffset &&
           perInstance == o.perInstance && instanceRate == o.instanceRate && format == o.format &&
           !memcmp(&genericValue, &o.genericValue, sizeof(genericValue)) &&
           genericEnabled == o.genericEnabled && used == o.used;
  }
  bool operator<(const VertexInputAttribute &o) const
  {
    if(name != o.name)
      return name < o.name;
    if(vertexBuffer != o.vertexBuffer)
      return vertexBuffer < o.vertexBuffer;
    if(byteOffset != o.byteOffset)
      return byteOffset < o.byteOffset;
    if(perInstance != o.perInstance)
      return perInstance < o.perInstance;
    if(instanceRate != o.instanceRate)
      return instanceRate < o.instanceRate;
    if(format != o.format)
      return format < o.format;
    if(memcmp(&genericValue, &o.genericValue, sizeof(genericValue)) < 0)
      return true;
    if(genericEnabled != o.genericEnabled)
      return genericEnabled < o.genericEnabled;
    if(used != o.used)
      return used < o.used;
    return false;
  }

  DOCUMENT("The name of this input. This may be a variable name or a semantic name.");
  rdcstr name;
  DOCUMENT("The index of the vertex buffer used to provide this attribute.");
  int vertexBuffer;
  DOCUMENT("The byte offset from the start of the vertex data for this VB to this attribute.");
  uint32_t byteOffset;
  DOCUMENT("``True`` if this attribute runs at instance rate.");
  bool perInstance;
  DOCUMENT(R"(If :data:`perInstance` is ``True``, the number of instances that source the same value
from the vertex buffer before advancing to the next value.
)");
  int instanceRate;
  DOCUMENT(R"(The interpreted format of this attribute.

:type: ResourceFormat
)");
  ResourceFormat format;
  DOCUMENT(R"(The generic value for this attribute if it has no vertex buffer bound.

:type: PixelValue
)");
  PixelValue genericValue;
  DOCUMENT("``True`` if this attribute is using :data:`genericValue` for its data.");
  bool genericEnabled = false;
  DOCUMENT(R"(Only valid for attributes on OpenGL. If the attribute has been set up for integers to
be converted to floats (glVertexAttribFormat with GL_INT) we store the format as integers. This is
fine if the application has a float input in the shader it just means we display the raw integer
instead of the casted float. However if the shader has an integer input this is invalid and it will
read something undefined - possibly the int bits of the casted float.

This property is set to ``True`` if the cast happens to an integer input and that bad cast needs to
be emulated.
)");
  bool floatCastWrong = false;
  DOCUMENT("``True`` if this attribute is enabled and used by the vertex shader.");
  bool used;
};

DECLARE_REFLECTION_STRUCT(VertexInputAttribute);

DOCUMENT(R"(A task or mesh message's location.

.. data:: NotUsed

  Set for values of task group/thread index when no task shaders were run.

  Also set for values of a mesh group or thread index when that dimensionality is unused. For
  example if the shader declares a group dimension of (128,1,1) then the y and z values for
  thread index will be indicated as not used.
)");
struct ShaderMeshMessageLocation
{
  DOCUMENT("");
  ShaderMeshMessageLocation() = default;
  ShaderMeshMessageLocation(const ShaderMeshMessageLocation &) = default;
  ShaderMeshMessageLocation &operator=(const ShaderMeshMessageLocation &) = default;

  bool operator==(const ShaderMeshMessageLocation &o) const
  {
    return taskGroup == o.taskGroup && meshGroup == o.meshGroup && thread == o.thread;
  }
  bool operator<(const ShaderMeshMessageLocation &o) const
  {
    if(!(taskGroup == o.taskGroup))
      return taskGroup < o.taskGroup;
    if(!(meshGroup == o.meshGroup))
      return meshGroup < o.meshGroup;
    if(!(thread == o.thread))
      return thread < o.thread;
    return false;
  }

  DOCUMENT(R"(The task workgroup index between the task dispatch.

.. note::
  If no task shader is in use, this will be :data:`NotUsed`, :data:`NotUsed`, :data:`NotUsed`.

:type: Tuple[int,int,int]
)");
  rdcfixedarray<uint32_t, 3> taskGroup;

  DOCUMENT(R"(The mesh workgroup index within the dispatch or launching task workgroup.

:type: Tuple[int,int,int]
)");
  rdcfixedarray<uint32_t, 3> meshGroup;

  DOCUMENT(R"(The thread index within the workgroup, either for a task shader or mesh shader.

.. note::
  Since task shaders can only emit one set of meshes per group, the task thread is not relevant
  for mesh shader messages, so this is the thread either for a task or a mesh shader message.

:type: Tuple[int,int,int]
)");
  rdcfixedarray<uint32_t, 3> thread;

  static const uint32_t NotUsed = ~0U;
};

DECLARE_REFLECTION_STRUCT(ShaderMeshMessageLocation);

DOCUMENT("A compute shader message's location.");
struct ShaderComputeMessageLocation
{
  DOCUMENT("");
  ShaderComputeMessageLocation() = default;
  ShaderComputeMessageLocation(const ShaderComputeMessageLocation &) = default;
  ShaderComputeMessageLocation &operator=(const ShaderComputeMessageLocation &) = default;

  bool operator==(const ShaderComputeMessageLocation &o) const
  {
    return workgroup == o.workgroup && thread == o.thread;
  }
  bool operator<(const ShaderComputeMessageLocation &o) const
  {
    if(!(workgroup == o.workgroup))
      return workgroup < o.workgroup;
    if(!(thread == o.thread))
      return thread < o.thread;
    return false;
  }

  DOCUMENT(R"(The workgroup index within the dispatch.

:type: Tuple[int,int,int]
)");
  rdcfixedarray<uint32_t, 3> workgroup;

  DOCUMENT(R"(The thread index within the workgroup.

:type: Tuple[int,int,int]
)");
  rdcfixedarray<uint32_t, 3> thread;
};

DECLARE_REFLECTION_STRUCT(ShaderComputeMessageLocation);

DOCUMENT("A vertex shader message's location.");
struct ShaderVertexMessageLocation
{
  DOCUMENT(R"(The vertex or index for this vertex.

:type: int
)");
  uint32_t vertexIndex;

  DOCUMENT(R"(The instance for this vertex.

:type: int
)");
  uint32_t instance;

  DOCUMENT(R"(The multiview view for this vertex, or ``0`` if multiview is disabled.

:type: int
)");
  uint32_t view;
};

DECLARE_REFLECTION_STRUCT(ShaderVertexMessageLocation);

DOCUMENT(R"(A pixel shader message's location.

.. data:: NoLocation

  No frame number is available.
)");
struct ShaderPixelMessageLocation
{
  DOCUMENT(R"(The x co-ordinate of the pixel.

:type: int
)");
  uint32_t x;

  DOCUMENT(R"(The y co-ordinate of the pixel.

:type: int
)");
  uint32_t y;

  DOCUMENT(R"(The sample, or :data:`NoLocation` if sample shading is disabled.

:type: int
)");
  uint32_t sample;

  DOCUMENT(R"(The generating primitive, or :data:`NoLocation` if the primitive ID is unavailable.

:type: int
)");
  uint32_t primitive;

  DOCUMENT(R"(The multiview view for this fragment, or ``0`` if multiview is disabled.

:type: int
)");
  uint32_t view;

  static const uint32_t NoLocation = ~0U;
};

DECLARE_REFLECTION_STRUCT(ShaderPixelMessageLocation);

DOCUMENT(R"(A geometry shader message's location.

.. data:: NoLocation

  No frame number is available.
)");
struct ShaderGeometryMessageLocation
{
  DOCUMENT(R"(The primitive index

:type: int
)");
  uint32_t primitive;

  DOCUMENT(R"(The multiview view for this primitive, or ``0`` if multiview is disabled.

:type: int
)");
  uint32_t view;
};

DECLARE_REFLECTION_STRUCT(ShaderGeometryMessageLocation);

DOCUMENT("A shader message's location.");
union ShaderMessageLocation
{
  DOCUMENT(R"(The location if the shader is a compute shader.

:type: ShaderComputeMessageLocation
)");
  ShaderComputeMessageLocation compute;

  DOCUMENT(R"(The location if the shader is a task or mesh shader.

:type: ShaderMeshMessageLocation
)");
  ShaderMeshMessageLocation mesh;

  DOCUMENT(R"(The location if the shader is a vertex shader.

:type: ShaderVertexMessageLocation
)");
  ShaderVertexMessageLocation vertex;

  DOCUMENT(R"(The location if the shader is a pixel shader.

:type: ShaderPixelMessageLocation
)");
  ShaderPixelMessageLocation pixel;

  DOCUMENT(R"(The location if the shader is a geometry shader.

:type: ShaderGeometryMessageLocation
)");
  ShaderGeometryMessageLocation geometry;
};

DECLARE_REFLECTION_STRUCT(ShaderMessageLocation);

DOCUMENT("A shader printed message.");
struct ShaderMessage
{
  DOCUMENT("");
  ShaderMessage() = default;
  ShaderMessage(const ShaderMessage &) = default;
  ShaderMessage &operator=(const ShaderMessage &) = default;

  bool operator==(const ShaderMessage &o) const
  {
    return stage == o.stage && disassemblyLine == o.disassemblyLine &&
           location.mesh == o.location.mesh && message == o.message;
  }
  bool operator<(const ShaderMessage &o) const
  {
    if(!(stage == o.stage))
      return stage < o.stage;
    if(!(disassemblyLine == o.disassemblyLine))
      return disassemblyLine < o.disassemblyLine;
    if(!(location.mesh == o.location.mesh))
      return location.mesh < o.location.mesh;
    if(!(message == o.message))
      return message < o.message;
    return false;
  }

  DOCUMENT(R"(The shader stage this message comes from.

:type: ShaderStage
)");
  ShaderStage stage;

  DOCUMENT(R"(The line (starting from 1) of the disassembly where this message came from, or -1 if
it is not associated with any line.

:type: int
)");
  int32_t disassemblyLine;

  DOCUMENT(R"(The location (thread/invocation) of the shader that this message comes from.

:type: ShaderMessageLocation
)");
  ShaderMessageLocation location;

  DOCUMENT(R"(The formatted message.

:type: str
)");
  rdcstr message;
};

DECLARE_REFLECTION_STRUCT(ShaderMessage);
