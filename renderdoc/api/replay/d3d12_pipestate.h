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

namespace D3D12Pipe
{
DOCUMENT(R"(Describes a single D3D12 input layout element for one vertex input.

.. data:: TightlyPacked

  Value for :data:`byteOffset` that indicates this element is tightly packed.
)");
struct Layout
{
  DOCUMENT("");
  Layout() = default;
  Layout(const Layout &) = default;
  Layout &operator=(const Layout &) = default;

  bool operator==(const Layout &o) const
  {
    return semanticName == o.semanticName && semanticIndex == o.semanticIndex &&
           format == o.format && inputSlot == o.inputSlot && byteOffset == o.byteOffset &&
           perInstance == o.perInstance && instanceDataStepRate == o.instanceDataStepRate;
  }
  bool operator<(const Layout &o) const
  {
    if(!(semanticName == o.semanticName))
      return semanticName < o.semanticName;
    if(!(semanticIndex == o.semanticIndex))
      return semanticIndex < o.semanticIndex;
    if(!(format == o.format))
      return format < o.format;
    if(!(inputSlot == o.inputSlot))
      return inputSlot < o.inputSlot;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(perInstance == o.perInstance))
      return perInstance < o.perInstance;
    if(!(instanceDataStepRate == o.instanceDataStepRate))
      return instanceDataStepRate < o.instanceDataStepRate;
    return false;
  }
  DOCUMENT("The semantic name for this input.");
  rdcstr semanticName;

  DOCUMENT("The semantic index for this input.");
  uint32_t semanticIndex = 0;

  DOCUMENT(R"(The format describing how the input data is interpreted.

:type: ResourceFormat
)");
  ResourceFormat format;

  DOCUMENT("The vertex buffer input slot where the data is sourced from.");
  uint32_t inputSlot = 0;

  DOCUMENT(R"(The byte offset from the start of the vertex data in the vertex buffer from
:data:`inputSlot`.

If the value is :data:`TightlyPacked` then the element is packed tightly after the previous element, or 0
if this is the first element.
)");
  uint32_t byteOffset = 0;

  DOCUMENT("``True`` if the vertex data is instance-rate.");
  bool perInstance = false;

  DOCUMENT(R"(If :data:`perInstance` is ``True`` then this is how many times each instance data is
used before advancing to the next instance.

E.g. if this value is two, then two instances will be drawn with the first instance data, then two
with the next instance data.
)");
  uint32_t instanceDataStepRate = 0;

  // D3D12_APPEND_ALIGNED_ELEMENT
  static const uint32_t TightlyPacked = ~0U;
};

DOCUMENT("Describes a single D3D12 vertex buffer binding.")
struct VertexBuffer
{
  DOCUMENT("");
  VertexBuffer() = default;
  VertexBuffer(const VertexBuffer &) = default;
  VertexBuffer &operator=(const VertexBuffer &) = default;

  bool operator==(const VertexBuffer &o) const
  {
    return resourceId == o.resourceId && byteStride == o.byteStride && byteSize == o.byteSize &&
           byteOffset == o.byteOffset;
  }
  bool operator<(const VertexBuffer &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(byteStride == o.byteStride))
      return byteStride < o.byteStride;
    if(!(byteSize == o.byteSize))
      return byteSize < o.byteSize;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer bound to this slot.");
  ResourceId resourceId;

  DOCUMENT("The byte offset from the start of the buffer to the beginning of the vertex data.");
  uint64_t byteOffset = 0;

  DOCUMENT("The number of bytes available in this vertex buffer.");
  uint32_t byteSize = 0;

  DOCUMENT("The byte stride between the start of one set of vertex data and the next.");
  uint32_t byteStride = 0;
};

DOCUMENT("Describes the D3D12 index buffer binding.")
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

  DOCUMENT("The number of bytes available in this index buffer.");
  uint32_t byteSize = 0;

  DOCUMENT(R"(The number of bytes for each index in the index buffer. Typically 2 or 4 bytes but
it can be 0 if no index buffer is bound.
)");
  uint32_t byteStride = 0;
};

DOCUMENT("Describes the input assembler state in the PSO.");
struct InputAssembly
{
  DOCUMENT("");
  InputAssembly() = default;
  InputAssembly(const InputAssembly &) = default;
  InputAssembly &operator=(const InputAssembly &) = default;

  DOCUMENT(R"(The input layout elements in this layout.

:type: List[D3D12Layout]
)");
  rdcarray<Layout> layouts;

  DOCUMENT(R"(The bound vertex buffers

:type: List[D3D12VertexBuffer]
)");
  rdcarray<VertexBuffer> vertexBuffers;

  DOCUMENT(R"(The bound index buffer.

:type: D3D12IndexBuffer
)");
  IndexBuffer indexBuffer;

  DOCUMENT(R"(The index value to use for cutting strips. Either ``0``, ``0xffff`` or ``0xffffffff``.
If the value is 0, strip cutting is disabled.
)");
  uint32_t indexStripCutValue = 0;

  DOCUMENT(R"(The current primitive topology.

:type: Topology
)");
  Topology topology = Topology::Unknown;
};

// immediate indicates either a root parameter (not in a table), or static samplers
// rootElement is the index in the original root signature that this descriptor came from.

DOCUMENT("Describes the details of a D3D12 resource view - any one of UAV, SRV, RTV or DSV.");
struct View
{
  DOCUMENT("");
  View() = default;
  View(uint32_t binding) : bind(binding) {}
  View(const View &) = default;
  View &operator=(const View &) = default;

  bool operator==(const View &o) const
  {
    return bind == o.bind && tableIndex == o.tableIndex && resourceId == o.resourceId &&
           type == o.type && viewFormat == o.viewFormat && swizzle == o.swizzle &&
           bufferFlags == o.bufferFlags && bufferStructCount == o.bufferStructCount &&
           elementByteSize == o.elementByteSize && firstElement == o.firstElement &&
           numElements == o.numElements && counterResourceId == o.counterResourceId &&
           counterByteOffset == o.counterByteOffset && firstMip == o.firstMip &&
           numMips == o.numMips && numSlices == o.numSlices && firstSlice == o.firstSlice;
  }
  bool operator<(const View &o) const
  {
    if(!(bind == o.bind))
      return bind < o.bind;
    if(!(tableIndex == o.tableIndex))
      return tableIndex < o.tableIndex;
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(type == o.type))
      return type < o.type;
    if(!(viewFormat == o.viewFormat))
      return viewFormat < o.viewFormat;
    if(!(swizzle == o.swizzle))
      return swizzle < o.swizzle;
    if(!(bufferFlags == o.bufferFlags))
      return bufferFlags < o.bufferFlags;
    if(!(bufferStructCount == o.bufferStructCount))
      return bufferStructCount < o.bufferStructCount;
    if(!(elementByteSize == o.elementByteSize))
      return elementByteSize < o.elementByteSize;
    if(!(firstElement == o.firstElement))
      return firstElement < o.firstElement;
    if(!(numElements == o.numElements))
      return numElements < o.numElements;
    if(!(counterResourceId == o.counterResourceId))
      return counterResourceId < o.counterResourceId;
    if(!(counterByteOffset == o.counterByteOffset))
      return counterByteOffset < o.counterByteOffset;
    if(!(firstMip == o.firstMip))
      return firstMip < o.firstMip;
    if(!(numMips == o.numMips))
      return numMips < o.numMips;
    if(!(numSlices == o.numSlices))
      return numSlices < o.numSlices;
    if(!(firstSlice == o.firstSlice))
      return firstSlice < o.firstSlice;
    return false;
  }
  DOCUMENT("The shader register that this view is bound to.");
  uint32_t bind = ~0U;
  DOCUMENT("The index in the the parent descriptor table where this descriptor came from.");
  uint32_t tableIndex = ~0U;

  DOCUMENT("The :class:`ResourceId` of the underlying resource the view refers to.");
  ResourceId resourceId;
  DOCUMENT("The :class:`ResourceId` of the resource where the hidden buffer counter is stored.");
  ResourceId counterResourceId;
  DOCUMENT("The byte offset in :data:`counterResourceId` where the counter is stored.");
  uint32_t counterByteOffset = 0;
  DOCUMENT("The :class:`TextureType` of the view type.");
  TextureType type;
  DOCUMENT(R"(The format cast that the view uses.

:type: ResourceFormat
)");
  ResourceFormat viewFormat;

  DOCUMENT(R"(The swizzle applied to a texture by the view.

:type: TextureSwizzle4
)");
  TextureSwizzle4 swizzle;
  DOCUMENT(R"(``True`` if this binding element is dynamically used.

If set to ``False`` this means that the binding was available to the shader but during execution it
was not referenced. The data gathered for setting this variable is conservative, meaning that only
accesses through arrays will have this calculated to reduce the required feedback bandwidth - single
non-arrayed descriptors may have this value set to ``True`` even if the shader did not use them,
since single descriptors may only be dynamically skipped by control flow.
)");
  bool dynamicallyUsed = true;
  DOCUMENT("The :class:`D3DBufferViewFlags` set for the buffer.");
  D3DBufferViewFlags bufferFlags = D3DBufferViewFlags::NoFlags;
  DOCUMENT("Valid for textures - the highest mip that is available through the view.");
  uint8_t firstMip = 0;
  DOCUMENT("Valid for textures - the number of mip levels in the view.");
  uint8_t numMips = 1;
  DOCUMENT("Valid for texture arrays or 3D textures - the first slice available through the view.");
  uint16_t firstSlice = 0;
  DOCUMENT("Valid for texture arrays or 3D textures - the number of slices in the view.");
  uint16_t numSlices = 1;
  DOCUMENT("If the view has a hidden counter, this stores the current value of the counter.");
  uint32_t bufferStructCount = 0;
  DOCUMENT(R"(The byte size of a single element in the view. Either the byte size of
:data:`viewFormat`, or the structured buffer element size, as appropriate.
)");
  uint32_t elementByteSize = 0;
  DOCUMENT("Valid for buffers - the first element to be used in the view.");
  uint64_t firstElement = 0;
  DOCUMENT("Valid for buffers - the number of elements to be used in the view.");
  uint32_t numElements = 1;

  DOCUMENT("The minimum mip-level clamp applied when sampling this texture.");
  float minLODClamp = 0.0f;
};

DOCUMENT("Describes the details of a sampler descriptor.");
struct Sampler
{
  DOCUMENT("");
  Sampler() = default;
  Sampler(uint32_t binding) : bind(binding) {}
  Sampler(const Sampler &) = default;
  Sampler &operator=(const Sampler &) = default;

  bool operator==(const Sampler &o) const
  {
    return bind == o.bind && tableIndex == o.tableIndex && addressU == o.addressU &&
           addressV == o.addressV && addressW == o.addressW &&
           borderColorValue.uintValue == o.borderColorValue.uintValue &&
           borderColorType == o.borderColorType && unnormalized == o.unnormalized &&
           compareFunction == o.compareFunction && filter == o.filter &&
           maxAnisotropy == o.maxAnisotropy && maxLOD == o.maxLOD && minLOD == o.minLOD &&
           mipLODBias == o.mipLODBias;
  }
  bool operator<(const Sampler &o) const
  {
    if(!(bind == o.bind))
      return bind < o.bind;
    if(!(tableIndex == o.tableIndex))
      return tableIndex < o.tableIndex;
    if(!(addressU == o.addressU))
      return addressU < o.addressU;
    if(!(addressV == o.addressV))
      return addressV < o.addressV;
    if(!(addressW == o.addressW))
      return addressW < o.addressW;
    if(!(borderColorValue.uintValue == o.borderColorValue.uintValue))
      return borderColorValue.uintValue < o.borderColorValue.uintValue;
    if(!(borderColorType == o.borderColorType))
      return borderColorType < o.borderColorType;
    if(!(unnormalized == o.unnormalized))
      return unnormalized < o.unnormalized;
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
    if(!(mipLODBias == o.mipLODBias))
      return mipLODBias < o.mipLODBias;
    return false;
  }
  DOCUMENT("The shader register that this sampler is bound to.");
  uint32_t bind = ~0U;
  DOCUMENT("The index in the the parent descriptor table where this descriptor came from.");
  uint32_t tableIndex = ~0U;

  DOCUMENT("The :class:`AddressMode` in the U direction.");
  AddressMode addressU = AddressMode::Wrap;
  DOCUMENT("The :class:`AddressMode` in the V direction.");
  AddressMode addressV = AddressMode::Wrap;
  DOCUMENT("The :class:`AddressMode` in the W direction.");
  AddressMode addressW = AddressMode::Wrap;
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
  DOCUMENT("``True`` if unnormalized co-ordinates are used in this sampler.");
  bool unnormalized = false;
  DOCUMENT("The :class:`CompareFunction` for comparison samplers.");
  CompareFunction compareFunction = CompareFunction::AlwaysTrue;
  DOCUMENT(R"(The filtering mode.

:type: TextureFilter
)");
  TextureFilter filter;
  DOCUMENT("The maximum anisotropic filtering level to use.");
  uint32_t maxAnisotropy = 0;
  DOCUMENT("The maximum mip level that can be used.");
  float maxLOD = 0.0f;
  DOCUMENT("The minimum mip level that can be used.");
  float minLOD = 0.0f;
  DOCUMENT("A bias to apply to the calculated mip level before sampling.");
  float mipLODBias = 0.0f;

  DOCUMENT(R"(Check if the border color is used in this D3D12 sampler.

:return: ``True`` if the border color is used, ``False`` otherwise.
:rtype: bool
)");
  bool UseBorder() const
  {
    return addressU == AddressMode::ClampBorder || addressV == AddressMode::ClampBorder ||
           addressW == AddressMode::ClampBorder;
  }
};

DOCUMENT("Describes the details of a constant buffer view descriptor.");
struct ConstantBuffer
{
  DOCUMENT("");
  ConstantBuffer() = default;
  ConstantBuffer(uint32_t binding) : bind(binding) {}
  ConstantBuffer(const ConstantBuffer &) = default;
  ConstantBuffer &operator=(const ConstantBuffer &) = default;

  bool operator==(const ConstantBuffer &o) const
  {
    return bind == o.bind && tableIndex == o.tableIndex && resourceId == o.resourceId &&
           byteOffset == o.byteOffset && byteSize == o.byteSize && rootValues == o.rootValues;
  }
  bool operator<(const ConstantBuffer &o) const
  {
    if(!(bind == o.bind))
      return bind < o.bind;
    if(!(tableIndex == o.tableIndex))
      return tableIndex < o.tableIndex;
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(byteSize == o.byteSize))
      return byteSize < o.byteSize;
    if(!(rootValues == o.rootValues))
      return rootValues < o.rootValues;
    return false;
  }
  DOCUMENT("The shader register that this constant buffer is bound to.");
  uint32_t bind = ~0U;
  DOCUMENT("The index in the the parent descriptor table where this descriptor came from.");
  uint32_t tableIndex = ~0U;

  DOCUMENT("The :class:`ResourceId` of the underlying buffer resource.");
  ResourceId resourceId;
  DOCUMENT("The byte offset where the buffer view starts in the underlying buffer.");
  uint64_t byteOffset = 0;
  DOCUMENT("How many bytes are in this constant buffer view.");
  uint32_t byteSize = 0;

  DOCUMENT(R"(If :data:`immediate` is ``True`` and this is a root constant, this contains the root
values set as interpreted as a series of DWORD values.

:type: List[int]
)");
  rdcarray<uint32_t> rootValues;
};

DOCUMENT("Contains information for a single root signature element range");
struct RootSignatureRange
{
  DOCUMENT("");
  RootSignatureRange() = default;
  RootSignatureRange(const RootSignatureRange &) = default;
  RootSignatureRange &operator=(const RootSignatureRange &) = default;

  bool operator==(const RootSignatureRange &o) const
  {
    return immediate == o.immediate && rootSignatureIndex == o.rootSignatureIndex &&
           visibility == o.visibility && registerSpace == o.registerSpace &&
           dynamicallyUsedCount == o.dynamicallyUsedCount && firstUsedIndex == o.firstUsedIndex &&
           lastUsedIndex == o.lastUsedIndex && constantBuffers == o.constantBuffers &&
           samplers == o.samplers && views == o.views;
  }
  bool operator<(const RootSignatureRange &o) const
  {
    if(!(immediate == o.immediate))
      return immediate < o.immediate;
    if(!(rootSignatureIndex == o.rootSignatureIndex))
      return rootSignatureIndex < o.rootSignatureIndex;
    if(!(visibility == o.visibility))
      return visibility < o.visibility;
    if(!(registerSpace == o.registerSpace))
      return registerSpace < o.registerSpace;
    if(!(dynamicallyUsedCount == o.dynamicallyUsedCount))
      return dynamicallyUsedCount < o.dynamicallyUsedCount;
    if(!(firstUsedIndex == o.firstUsedIndex))
      return firstUsedIndex < o.firstUsedIndex;
    if(!(lastUsedIndex == o.lastUsedIndex))
      return lastUsedIndex < o.lastUsedIndex;
    if(!(constantBuffers == o.constantBuffers))
      return constantBuffers < o.constantBuffers;
    if(!(samplers == o.samplers))
      return samplers < o.samplers;
    if(!(views == o.views))
      return views < o.views;
    return false;
  }

  DOCUMENT("``True`` if this root element is a root constant (i.e. not in a table).");
  bool immediate = false;
  DOCUMENT("The index in the original root signature that this descriptor came from.");
  uint32_t rootSignatureIndex = ~0U;
  DOCUMENT("The :class:`BindType` contained by this element.");
  BindType type = BindType::Unknown;
  DOCUMENT("The :class:`ShaderStageMask` of this element.");
  ShaderStageMask visibility = ShaderStageMask::All;
  DOCUMENT("The register space of this element.");
  uint32_t registerSpace;
  DOCUMENT(R"(Lists how many bindings in :data:`views` are dynamically used. Useful to avoid
redundant iteration to determine whether any bindings are present.

For more information see :data:`D3D12View.dynamicallyUsed`.
)");
  uint32_t dynamicallyUsedCount = ~0U;
  DOCUMENT(R"(Gives the index of the first binding in :data:`views` that is dynamically used. Useful
to avoid redundant iteration in very large descriptor arrays with a small subset that are used.

For more information see :data:`D3D12View.dynamicallyUsed`.
)");
  int32_t firstUsedIndex = 0;
  DOCUMENT(R"(Gives the index of the first binding in :data:`views` that is dynamically used. Useful
to avoid redundant iteration in very large descriptor arrays with a small subset that are used.

.. note::
  This may be set to a higher value than the number of bindings, if no dynamic use information is
  available. Ensure that this is an additional check on the bind and the count is still respected.

For more information see :data:`D3D12View.dynamicallyUsed`.
)");
  int32_t lastUsedIndex = 0x7fffffff;
  DOCUMENT(R"(The constant buffers in this range.

:type: List[D3D12ConstantBuffer]
)");
  rdcarray<ConstantBuffer> constantBuffers;
  DOCUMENT(R"(The samplers in this range.

:type: List[D3D12Sampler]
)");
  rdcarray<Sampler> samplers;
  DOCUMENT(R"(The SRVs or UAVs in this range.

:type: List[D3D12View]
)");
  rdcarray<View> views;
};

DOCUMENT("Describes a D3D12 shader stage.");
struct Shader
{
  DOCUMENT("");
  Shader() = default;
  Shader(const Shader &) = default;
  Shader &operator=(const Shader &) = default;

  DOCUMENT("The :class:`ResourceId` of the shader object itself.");
  ResourceId resourceId;

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
};

DOCUMENT("Describes a binding on the D3D12 stream-out stage.");
struct StreamOutBind
{
  DOCUMENT("");
  StreamOutBind() = default;
  StreamOutBind(const StreamOutBind &) = default;
  StreamOutBind &operator=(const StreamOutBind &) = default;

  bool operator==(const StreamOutBind &o) const
  {
    return resourceId == o.resourceId && byteOffset == o.byteOffset && byteSize == o.byteSize &&
           writtenCountResourceId == o.writtenCountResourceId &&
           writtenCountByteOffset == o.writtenCountByteOffset;
  }
  bool operator<(const StreamOutBind &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(byteOffset == o.byteOffset))
      return byteOffset < o.byteOffset;
    if(!(byteSize == o.byteSize))
      return byteSize < o.byteSize;
    if(!(writtenCountResourceId == o.writtenCountResourceId))
      return writtenCountResourceId < o.writtenCountResourceId;
    if(!(writtenCountByteOffset == o.writtenCountByteOffset))
      return writtenCountByteOffset < o.writtenCountByteOffset;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the buffer.");
  ResourceId resourceId;
  DOCUMENT(R"(The byte offset in :data:`resourceId` where the buffer view starts in the underlying
buffer.
)");
  uint64_t byteOffset = 0;
  DOCUMENT("How many bytes are in this stream-out buffer view.");
  uint64_t byteSize = 0;

  DOCUMENT("The :class:`ResourceId` of the buffer where the written count will be stored.");
  ResourceId writtenCountResourceId;
  DOCUMENT(R"(The byte offset in :data:`writtenCountResourceId` where the stream-out count will be
written.
)");
  uint64_t writtenCountByteOffset = 0;
};

DOCUMENT(R"(Describes the stream-out state in the PSO.

.. data:: NoRasterization

  Value for :data:`rasterizedStream` that indicates no stream is being rasterized.
)");
struct StreamOut
{
  DOCUMENT("");
  StreamOut() = default;
  StreamOut(const StreamOut &) = default;
  StreamOut &operator=(const StreamOut &) = default;

  DOCUMENT(R"(The bound stream-out buffer bindings.

:type: List[D3D12StreamOutBind]
)");
  rdcarray<StreamOutBind> outputs;

  DOCUMENT(R"(Which stream-out stream is being used for rasterization.

If the value is :data:`NoRasterization` then no stream has been selected for rasterization.

:type: int
)");
  uint32_t rasterizedStream = 0;

  // D3D11_SO_NO_RASTERIZED_STREAM
  static const uint32_t NoRasterization = ~0U;
};

DOCUMENT("Describes the rasterizer state in the PSO.");
struct RasterizerState
{
  DOCUMENT("");
  RasterizerState() = default;
  RasterizerState(const RasterizerState &) = default;
  RasterizerState &operator=(const RasterizerState &) = default;

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
  DOCUMENT(R"(The clamp value for calculated depth bias from :data:`depthBias` and
:data:`slopeScaledDepthBias`
)");
  float depthBiasClamp = 0.0f;
  DOCUMENT("The slope-scaled depth bias value to apply to z-values.");
  float slopeScaledDepthBias = 0.0f;
  DOCUMENT("``True`` if pixels outside of the near and far depth planes should be clipped.");
  bool depthClip = false;
  DOCUMENT("The line rasterization mode.");
  LineRaster lineRasterMode = LineRaster::Default;
  DOCUMENT(R"(A sample count to force rasterization to when UAV rendering or rasterizing, or 0 to
not force any sample count.
)");
  uint32_t forcedSampleCount = 0;
  DOCUMENT("The current :class:`ConservativeRaster` mode.");
  ConservativeRaster conservativeRasterization = ConservativeRaster::Disabled;
  DOCUMENT(R"(The current base variable shading rate. This will always be 1x1 when variable shading
is disabled.

:type: Tuple[int,int]
)");
  rdcpair<uint32_t, uint32_t> baseShadingRate = {1, 1};
  DOCUMENT(R"(The shading rate combiners.

The combiners are applied as follows, according to the D3D spec:

  ``intermediateRate = combiner[0] ( baseShadingRate,  shaderExportedShadingRate )``
  ``finalRate        = combiner[1] ( intermediateRate, imageBasedShadingRate     )``

Where the first input is from :data:`baseShadingRate` and the second is the exported shading rate
from a vertex or geometry shader, which defaults to 1x1 if not exported.

The intermediate result is then used as the first input to the second combiner, together with the
shading rate sampled from the shading rate image.

:type: Tuple[ShadingRateCombiner,ShadingRateCombiner]
)");
  rdcpair<ShadingRateCombiner, ShadingRateCombiner> shadingRateCombiners = {
      ShadingRateCombiner::Passthrough, ShadingRateCombiner::Passthrough};
  DOCUMENT(R"(The image bound as a shading rate image.

:type: ResourceId
)");
  ResourceId shadingRateImage;
};

DOCUMENT("Describes the rasterization state of the D3D12 pipeline.");
struct Rasterizer
{
  DOCUMENT("");
  Rasterizer() = default;
  Rasterizer(const Rasterizer &) = default;
  Rasterizer &operator=(const Rasterizer &) = default;

  DOCUMENT("The mask determining which samples are written to.");
  uint32_t sampleMask = ~0U;

  DOCUMENT(R"(The bound viewports.

:type: List[Viewport]
)");
  rdcarray<Viewport> viewports;

  DOCUMENT(R"(The bound scissor regions.

:type: List[Scissor]
)");
  rdcarray<Scissor> scissors;

  DOCUMENT(R"(The details of the rasterization state.

:type: D3D12RasterizerState
)");
  RasterizerState state;
};

DOCUMENT("Describes the state of the depth-stencil state in the PSO.");
struct DepthStencilState
{
  DOCUMENT("");
  DepthStencilState() = default;
  DepthStencilState(const DepthStencilState &) = default;
  DepthStencilState &operator=(const DepthStencilState &) = default;

  DOCUMENT("``True`` if depth testing should be performed.");
  bool depthEnable = false;
  DOCUMENT("``True`` if depth values should be written to the depth target.");
  bool depthWrites = false;
  DOCUMENT("``True`` if depth bounds tests should be applied.");
  bool depthBoundsEnable = false;
  DOCUMENT("The :class:`CompareFunction` to use for testing depth values.");
  CompareFunction depthFunction = CompareFunction::AlwaysTrue;
  DOCUMENT("``True`` if stencil operations should be performed.");
  bool stencilEnable = false;

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

DOCUMENT("Describes the blend state in the PSO.");
struct BlendState
{
  DOCUMENT("");
  BlendState() = default;
  BlendState(const BlendState &) = default;
  BlendState &operator=(const BlendState &) = default;

  DOCUMENT("``True`` if alpha-to-coverage should be used when blending to an MSAA target.");
  bool alphaToCoverage = false;
  DOCUMENT(R"(``True`` if independent blending for each target should be used.

``False`` if the first blend should be applied to all targets.
)");
  bool independentBlend = false;

  DOCUMENT(R"(The blend operations for each target.

:type: List[ColorBlend]
)");
  rdcarray<ColorBlend> blends;

  DOCUMENT(R"(The constant blend factor to use in blend equations.

:type: Tuple[float,float,float,float]
)");
  rdcfixedarray<float, 4> blendFactor = {1.0f, 1.0f, 1.0f, 1.0f};
};

DOCUMENT("Describes the current state of the output-merger stage of the D3D12 pipeline.");
struct OM
{
  DOCUMENT("");
  OM() = default;
  OM(const OM &) = default;
  OM &operator=(const OM &) = default;

  DOCUMENT(R"(The current depth-stencil state details.

:type: D3D12DepthStencilState
)");
  DepthStencilState depthStencilState;

  DOCUMENT(R"(The current blend state details.

:type: D3D12BlendState
)");
  BlendState blendState;

  DOCUMENT(R"(The bound render targets.

:type: List[D3D12View]
)");
  rdcarray<View> renderTargets;

  DOCUMENT(R"(The currently bound depth-stencil target.

:type: D3D12View
)");
  View depthTarget = D3D12Pipe::View(0);
  DOCUMENT("``True`` if depth access to the depth-stencil target is read-only.");
  bool depthReadOnly = false;
  DOCUMENT("``True`` if stenncil access to the depth-stencil target is read-only.");
  bool stencilReadOnly = false;

  DOCUMENT("The sample count used for rendering.");
  uint32_t multiSampleCount = 1;
  DOCUMENT("The MSAA quality level used for rendering.");
  uint32_t multiSampleQuality = 0;
};

DOCUMENT("Describes the current state that a sub-resource is in.");
struct ResourceState
{
  DOCUMENT("");
  ResourceState() = default;
  ResourceState(const ResourceState &) = default;
  ResourceState &operator=(const ResourceState &) = default;

  bool operator==(const ResourceState &o) const { return name == o.name; }
  bool operator<(const ResourceState &o) const
  {
    if(!(name == o.name))
      return name < o.name;
    return false;
  }
  DOCUMENT("A human-readable name for the current state.");
  rdcstr name;
};

DOCUMENT("Contains the current state of a given resource.");
struct ResourceData
{
  DOCUMENT("");
  ResourceData() = default;
  ResourceData(const ResourceData &) = default;
  ResourceData &operator=(const ResourceData &) = default;

  bool operator==(const ResourceData &o) const
  {
    return resourceId == o.resourceId && states == o.states;
  }
  bool operator<(const ResourceData &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(states == o.states))
      return states < o.states;
    return false;
  }
  DOCUMENT("The :class:`ResourceId` of the resource.");
  ResourceId resourceId;

  DOCUMENT(R"(The subresource states in this resource.

:type: List[D3D12ResourceState]
)");
  rdcarray<ResourceState> states;
};

DOCUMENT("The full current D3D12 pipeline state.");
struct State
{
#if !defined(RENDERDOC_EXPORTS)
  // disallow creation/copy of this object externally
  State() = delete;
  State(const State &) = delete;
#endif

  DOCUMENT("The :class:`ResourceId` of the pipeline state object.");
  ResourceId pipelineResourceId;

  DOCUMENT("The :class:`ResourceId` of the root signature object.");
  ResourceId rootSignatureResourceId;

  DOCUMENT(R"(The root signature, as a range per element.
    
:type: List[D3D12RootSignatureRange]
)");
  rdcarray<RootSignatureRange> rootElements;

  DOCUMENT(R"(The input assembly pipeline stage.

:type: D3D12InputAssembly
)");
  InputAssembly inputAssembly;

  DOCUMENT(R"(The vertex shader stage.

:type: D3D12Shader
)");
  Shader vertexShader;
  DOCUMENT(R"(The hull shader stage.

:type: D3D12Shader
)");
  Shader hullShader;
  DOCUMENT(R"(The domain shader stage.

:type: D3D12Shader
)");
  Shader domainShader;
  DOCUMENT(R"(The geometry shader stage.

:type: D3D12Shader
)");
  Shader geometryShader;
  DOCUMENT(R"(The pixel shader stage.

:type: D3D12Shader
)");
  Shader pixelShader;
  DOCUMENT(R"(The compute shader stage.

:type: D3D12Shader
)");
  Shader computeShader;

  DOCUMENT(R"(The stream-out pipeline stage.

:type: D3D12StreamOut
)");
  StreamOut streamOut;

  DOCUMENT(R"(The rasterizer pipeline stage.

:type: D3D12Rasterizer
)");
  Rasterizer rasterizer;

  DOCUMENT(R"(The output merger pipeline stage.

:type: D3D12OM
)");
  OM outputMerger;

  DOCUMENT(R"(The resource states for the currently live resources.

:type: List[D3D12ResourceData]
)");
  rdcarray<ResourceData> resourceStates;
};

};    // namespace D3D12Pipe

DECLARE_REFLECTION_STRUCT(D3D12Pipe::Layout);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::VertexBuffer);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::IndexBuffer);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::InputAssembly);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::View);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::Sampler);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::ConstantBuffer);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::RootSignatureRange);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::Shader);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::StreamOutBind);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::StreamOut);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::RasterizerState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::Rasterizer);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::DepthStencilState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::BlendState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::OM);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::ResourceState);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::ResourceData);
DECLARE_REFLECTION_STRUCT(D3D12Pipe::State);
