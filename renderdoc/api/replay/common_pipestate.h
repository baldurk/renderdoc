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

DOCUMENT("Information about a single resource bound to a slot in an API-specific way.");
struct BoundResource
{
  DOCUMENT("");
  BoundResource()
  {
    resourceId = ResourceId();
    dynamicallyUsed = true;
    firstMip = -1;
    firstSlice = -1;
    typeCast = CompType::Typeless;
  }
  BoundResource(ResourceId id)
  {
    resourceId = id;
    dynamicallyUsed = true;
    firstMip = -1;
    firstSlice = -1;
    typeCast = CompType::Typeless;
  }

  BoundResource(ResourceId id, Subresource subresource)
  {
    resourceId = id;
    dynamicallyUsed = true;
    firstMip = subresource.mip;
    firstSlice = subresource.slice;
    typeCast = CompType::Typeless;
  }
  BoundResource(const BoundResource &) = default;
  BoundResource &operator=(const BoundResource &) = default;

  bool operator==(const BoundResource &o) const
  {
    return resourceId == o.resourceId && firstMip == o.firstMip && firstSlice == o.firstSlice &&
           typeCast == o.typeCast;
  }
  bool operator<(const BoundResource &o) const
  {
    if(resourceId != o.resourceId)
      return resourceId < o.resourceId;
    if(firstMip != o.firstMip)
      return firstMip < o.firstMip;
    if(firstSlice != o.firstSlice)
      return firstSlice < o.firstSlice;
    if(typeCast != o.typeCast)
      return typeCast < o.typeCast;
    return false;
  }
  DOCUMENT("A :class:`~renderdoc.ResourceId` identifying the bound resource.");
  ResourceId resourceId;
  DOCUMENT(R"(``True`` if this binding element is dynamically used.

Some APIs provide fine-grained usage based on dynamic shader feedback, to support 'bindless'
scenarios where only a small sparse subset of bound resources are actually used.
)");
  bool dynamicallyUsed = true;
  DOCUMENT("For textures, the highest mip level available on this binding, or -1 for all mips");
  int firstMip;
  DOCUMENT("For textures, the first array slice available on this binding. or -1 for all slices.");
  int firstSlice;
  DOCUMENT(
      "For textures, a :class:`~renderdoc.CompType` hint for how to interpret typeless textures.");
  CompType typeCast;
};

DECLARE_REFLECTION_STRUCT(BoundResource);

// TODO this should be replaced with an rdcmap
DOCUMENT(R"(Contains all of the bound resources at a particular bindpoint. In APIs that don't
support resource arrays, there will only be one bound resource.
)");
struct BoundResourceArray
{
  DOCUMENT("");
  BoundResourceArray() = default;
  BoundResourceArray(const BoundResourceArray &) = default;
  BoundResourceArray &operator=(const BoundResourceArray &) = default;
  BoundResourceArray(Bindpoint b) : bindPoint(b) {}
  BoundResourceArray(Bindpoint b, const rdcarray<BoundResource> &r) : bindPoint(b), resources(r)
  {
    dynamicallyUsedCount = (uint32_t)r.size();
    firstIndex = 0;
  }
  // for convenience for searching the array, we compare only using the BindPoint
  bool operator==(const BoundResourceArray &o) const { return bindPoint == o.bindPoint; }
  bool operator!=(const BoundResourceArray &o) const { return !(bindPoint == o.bindPoint); }
  bool operator<(const BoundResourceArray &o) const { return bindPoint < o.bindPoint; }
  DOCUMENT(R"(The bind point for this array of bound resources.

:type: Bindpoint
)");
  Bindpoint bindPoint;

  DOCUMENT(R"(The resources at this bind point.

:type: List[BoundResource]
)");
  rdcarray<BoundResource> resources;

  DOCUMENT(R"(Lists how many bindings in :data:`resources` are dynamically used.

Some APIs provide fine-grained usage based on dynamic shader feedback, to support 'bindless'
scenarios where only a small sparse subset of bound resources are actually used.

If this information isn't present this will be set to a large number.
)");
  uint32_t dynamicallyUsedCount = ~0U;
  DOCUMENT(R"(Gives the array index of the first binding in :data:`resource`. If only a small subset
of the resources are used by the shader then the array may be rebased such that the first element is
not array index 0.

For more information see :data:`VKBindingElement.dynamicallyUsed`.
)");
  int32_t firstIndex = 0;
};

DECLARE_REFLECTION_STRUCT(BoundResourceArray);

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

DOCUMENT("Information about a single constant buffer binding.");
struct BoundCBuffer
{
  DOCUMENT("");
  BoundCBuffer() = default;
  BoundCBuffer(const BoundCBuffer &) = default;
  BoundCBuffer &operator=(const BoundCBuffer &) = default;

  DOCUMENT("A :class:`~renderdoc.ResourceId` identifying the buffer.");
  ResourceId resourceId;
  DOCUMENT("The offset in bytes from the start of the buffer to the constant data.");
  uint64_t byteOffset = 0;
  DOCUMENT("The size in bytes for the constant buffer. Access outside this size returns 0.");
  uint64_t byteSize = 0;

  DOCUMENT(R"(The inline byte data for this constant buffer, if this binding is not backed by a
typical buffer.

:type: bytes
)");
  bytebuf inlineData;
};

DECLARE_REFLECTION_STRUCT(BoundCBuffer);

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
