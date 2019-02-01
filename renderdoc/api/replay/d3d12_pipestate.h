/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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
#include "shader_types.h"

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

  DOCUMENT("The :class:`ResourceFormat` describing how the input data is interpreted.");
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

  DOCUMENT("The :class:`ResourceId` of the index buffer.");
  ResourceId resourceId;

  DOCUMENT("The byte offset from the start of the buffer to the beginning of the index data.");
  uint64_t byteOffset = 0;

  DOCUMENT("The number of bytes available in this index buffer.");
  uint32_t byteSize = 0;
};

DOCUMENT("Describes the input assembler state in the PSO.");
struct InputAssembly
{
  DOCUMENT("");
  InputAssembly() = default;
  InputAssembly(const InputAssembly &) = default;

  DOCUMENT("A list of :class:`D3D12Layout` describing the input layout elements in this layout.");
  rdcarray<Layout> layouts;

  DOCUMENT("A list of :class:`D3D12VertexBuffer` with the vertex buffers that are bound.");
  rdcarray<VertexBuffer> vertexBuffers;

  DOCUMENT("The :class:`D3D12IndexBuffer` describing the index buffer.");
  IndexBuffer indexBuffer;

  DOCUMENT(R"(The index value to use for cutting strips. Either ``0``, ``0xffff`` or ``0xffffffff``.
If the value is 0, strip cutting is disabled.
)");
  uint32_t indexStripCutValue = 0;
};

// immediate indicates either a root parameter (not in a table), or static samplers
// rootElement is the index in the original root signature that this descriptor came from.

DOCUMENT("Describes the details of a D3D12 resource view - any one of UAV, SRV, RTV or DSV.");
struct View
{
  DOCUMENT("");
  View() = default;
  View(const View &) = default;

  bool operator==(const View &o) const
  {
    return resourceId == o.resourceId && type == o.type && viewFormat == o.viewFormat &&
           swizzle[0] == o.swizzle[0] && swizzle[1] == o.swizzle[1] && swizzle[2] == o.swizzle[2] &&
           swizzle[3] == o.swizzle[3] && bufferFlags == o.bufferFlags &&
           bufferStructCount == o.bufferStructCount && elementByteSize == o.elementByteSize &&
           firstElement == o.firstElement && numElements == o.numElements &&
           counterResourceId == o.counterResourceId && counterByteOffset == o.counterByteOffset &&
           firstMip == o.firstMip && numMips == o.numMips && numSlices == o.numSlices &&
           firstSlice == o.firstSlice;
  }
  bool operator<(const View &o) const
  {
    if(!(resourceId == o.resourceId))
      return resourceId < o.resourceId;
    if(!(type == o.type))
      return type < o.type;
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
  DOCUMENT("``True`` if this view is a root parameter (i.e. not in a table).");
  bool immediate = false;
  DOCUMENT("The index in the original root signature that this descriptor came from.");
  uint32_t rootElement = ~0U;
  DOCUMENT("The index in the the parent descriptor table where this descriptor came from.");
  uint32_t tableIndex = ~0U;

  DOCUMENT("The :class:`ResourceId` of the underlying resource the view refers to.");
  ResourceId resourceId;
  DOCUMENT("The :class:`TextureType` of the view type.");
  TextureType type;
  DOCUMENT("The :class:`ResourceFormat` that the view uses.");
  ResourceFormat viewFormat;

  DOCUMENT("Four :class:`TextureSwizzle` elements indicating the swizzle applied to this texture.");
  TextureSwizzle swizzle[4] = {TextureSwizzle::Red, TextureSwizzle::Green, TextureSwizzle::Blue,
                               TextureSwizzle::Alpha};
  DOCUMENT("The :class:`D3DBufferViewFlags` set for the buffer.");
  D3DBufferViewFlags bufferFlags = D3DBufferViewFlags::NoFlags;
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

  DOCUMENT("The :class:`ResourceId` of the resource where the hidden buffer counter is stored.");
  ResourceId counterResourceId;
  DOCUMENT("The byte offset in :data:`counterResourceId` where the counter is stored.");
  uint64_t counterByteOffset = 0;

  DOCUMENT("Valid for textures - the highest mip that is available through the view.");
  uint32_t firstMip = 0;
  DOCUMENT("Valid for textures - the number of mip levels in the view.");
  uint32_t numMips = 1;

  DOCUMENT("Valid for texture arrays or 3D textures - the first slice available through the view.");
  uint32_t firstSlice = 0;
  DOCUMENT("Valid for texture arrays or 3D textures - the number of slices in the view.");
  uint32_t numSlices = 1;

  DOCUMENT("The minimum mip-level clamp applied when sampling this texture.");
  float minLODClamp = 0.0f;
};

DOCUMENT("Describes the details of a sampler descriptor.");
struct Sampler
{
  DOCUMENT("");
  Sampler() = default;
  Sampler(const Sampler &) = default;

  bool operator==(const Sampler &o) const
  {
    return immediate == o.immediate && rootElement == o.rootElement && tableIndex == o.tableIndex &&
           addressU == o.addressU && addressV == o.addressV && addressW == o.addressW &&
           borderColor[0] == o.borderColor[0] && borderColor[1] == o.borderColor[1] &&
           borderColor[2] == o.borderColor[2] && borderColor[3] == o.borderColor[3] &&
           compareFunction == o.compareFunction && filter == o.filter &&
           maxAnisotropy == o.maxAnisotropy && maxLOD == o.maxLOD && minLOD == o.minLOD &&
           mipLODBias == o.mipLODBias;
  }
  bool operator<(const Sampler &o) const
  {
    if(!(immediate == o.immediate))
      return immediate < o.immediate;
    if(!(rootElement == o.rootElement))
      return rootElement < o.rootElement;
    if(!(tableIndex == o.tableIndex))
      return tableIndex < o.tableIndex;
    if(!(addressU == o.addressU))
      return addressU < o.addressU;
    if(!(addressV == o.addressV))
      return addressV < o.addressV;
    if(!(addressW == o.addressW))
      return addressW < o.addressW;
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
  DOCUMENT("``True`` if this view is a static sampler (i.e. not in a table).");
  bool immediate = 0;
  DOCUMENT("The index in the original root signature that this descriptor came from.");
  uint32_t rootElement = ~0U;
  DOCUMENT("The index in the the parent descriptor table where this descriptor came from.");
  uint32_t tableIndex = ~0U;

  DOCUMENT("The :class:`AddressMode` in the U direction.");
  AddressMode addressU = AddressMode::Wrap;
  DOCUMENT("The :class:`AddressMode` in the V direction.");
  AddressMode addressV = AddressMode::Wrap;
  DOCUMENT("The :class:`AddressMode` in the W direction.");
  AddressMode addressW = AddressMode::Wrap;
  DOCUMENT("The RGBA border color.");
  float borderColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  DOCUMENT("The :class:`CompareFunction` for comparison samplers.");
  CompareFunction compareFunction = CompareFunction::AlwaysTrue;
  DOCUMENT("The :class:`TextureFilter` describing the filtering mode.");
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
:rtype: ``bool``
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
  ConstantBuffer(const ConstantBuffer &) = default;

  bool operator==(const ConstantBuffer &o) const
  {
    return immediate == o.immediate && rootElement == o.rootElement && tableIndex == o.tableIndex &&
           resourceId == o.resourceId && byteOffset == o.byteOffset && byteSize == o.byteSize &&
           rootValues == o.rootValues;
  }
  bool operator<(const ConstantBuffer &o) const
  {
    if(!(immediate == o.immediate))
      return immediate < o.immediate;
    if(!(rootElement == o.rootElement))
      return rootElement < o.rootElement;
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
  DOCUMENT("``True`` if this view is a root constant (i.e. not in a table).");
  bool immediate = false;
  DOCUMENT("The index in the original root signature that this descriptor came from.");
  uint32_t rootElement = ~0U;
  DOCUMENT("The index in the the parent descriptor table where this descriptor came from.");
  uint32_t tableIndex = ~0U;

  DOCUMENT("The :class:`ResourceId` of the underlying buffer resource.");
  ResourceId resourceId;
  DOCUMENT("The byte offset where the buffer view starts in the underlying buffer.");
  uint64_t byteOffset = 0;
  DOCUMENT("How many bytes are in this constant buffer view.");
  uint32_t byteSize = 0;

  DOCUMENT(R"(If :data:`immediate` is ``True`` and this is a root constant, this contains a list of
``int`` values with the root values set.
)");
  rdcarray<uint32_t> rootValues;
};

DOCUMENT("Contains all of the registers in a single register space mapped to by a root signature.");
struct RegisterSpace
{
  DOCUMENT("");
  RegisterSpace() = default;
  RegisterSpace(const RegisterSpace &) = default;

  bool operator==(const RegisterSpace &o) const
  {
    return spaceIndex == o.spaceIndex && constantBuffers == o.constantBuffers &&
           samplers == o.samplers && srvs == o.srvs && uavs == o.uavs;
  }
  bool operator<(const RegisterSpace &o) const
  {
    if(!(spaceIndex == o.spaceIndex))
      return spaceIndex < o.spaceIndex;
    if(!(constantBuffers == o.constantBuffers))
      return constantBuffers < o.constantBuffers;
    if(!(samplers == o.samplers))
      return samplers < o.samplers;
    if(!(srvs == o.srvs))
      return srvs < o.srvs;
    if(!(uavs == o.uavs))
      return uavs < o.uavs;
    return false;
  }
  DOCUMENT("The index of this space, since space indices can be sparse");
  uint32_t spaceIndex;
  DOCUMENT("List of :class:`D3D12ConstantBuffer` containing the constant buffers.");
  rdcarray<ConstantBuffer> constantBuffers;
  DOCUMENT("List of :class:`D3D12Sampler` containing the samplers.");
  rdcarray<Sampler> samplers;
  DOCUMENT("List of :class:`D3D12View` containing the SRVs.");
  rdcarray<View> srvs;
  DOCUMENT("List of :class:`D3D12View` containing the UAVs.");
  rdcarray<View> uavs;
};

DOCUMENT("Describes a D3D12 shader stage.");
struct Shader
{
  DOCUMENT("");
  Shader() = default;
  Shader(const Shader &) = default;

  DOCUMENT("The :class:`ResourceId` of the shader object itself.");
  ResourceId resourceId;

  DOCUMENT("A :class:`ShaderReflection` describing the reflection data for this shader.");
  ShaderReflection *reflection = NULL;
  DOCUMENT(R"(A :class:`ShaderBindpointMapping` to match :data:`reflection` with the bindpoint
mapping data.
)");
  ShaderBindpointMapping bindpointMapping;

  DOCUMENT("A :class:`ShaderStage` identifying which stage this shader is bound to.");
  ShaderStage stage = ShaderStage::Vertex;

  DOCUMENT("A list of :class:`D3D12RegisterSpace` with the register spaces for this stage.");
  rdcarray<RegisterSpace> spaces;

  DOCUMENT(R"(Return the index in the :data:`spaces` array of a given register space.

:return: The index if the space exists, or ``-1`` if it doesn't.
:rtype: ``int``
)");
  int32_t FindSpace(uint32_t spaceIndex) const
  {
    for(int32_t i = 0; i < spaces.count(); i++)
      if(spaces[i].spaceIndex == spaceIndex)
        return i;

    return -1;
  }
};

DOCUMENT("Describes a binding on the D3D12 stream-out stage.");
struct StreamOutBind
{
  DOCUMENT("");
  StreamOutBind() = default;
  StreamOutBind(const StreamOutBind &) = default;

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

DOCUMENT("Describes the stream-out state in the PSO.");
struct StreamOut
{
  DOCUMENT("");
  StreamOut() = default;
  StreamOut(const StreamOut &) = default;

  DOCUMENT("A list of ``D3D12SOBind`` with the bound buffers.");
  rdcarray<StreamOutBind> outputs;
};

DOCUMENT("Describes the rasterizer state in the PSO.");
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
  int32_t depthBias = 0;
  DOCUMENT(R"(The clamp value for calculated depth bias from :data:`depthBias` and
:data:`slopeScaledDepthBias`
)");
  float depthBiasClamp = 0.0f;
  DOCUMENT("The slope-scaled depth bias value to apply to z-values.");
  float slopeScaledDepthBias = 0.0f;
  DOCUMENT("``True`` if pixels outside of the near and far depth planes should be clipped.");
  bool depthClip = false;
  DOCUMENT("``True`` if the quadrilateral MSAA algorithm should be used on MSAA targets.");
  bool multisampleEnable = false;
  DOCUMENT(
      "``True`` if lines should be anti-aliased. Ignored if :data:`multisampleEnable` is "
      "``False``.");
  bool antialiasedLines = false;
  DOCUMENT(R"(A sample count to force rasterization to when UAV rendering or rasterizing, or 0 to
not force any sample count.
)");
  uint32_t forcedSampleCount = 0;
  DOCUMENT("The current :class:`ConservativeRaster` mode.");
  ConservativeRaster conservativeRasterization = ConservativeRaster::Disabled;
};

DOCUMENT("Describes the rasterization state of the D3D12 pipeline.");
struct Rasterizer
{
  DOCUMENT("");
  Rasterizer() = default;
  Rasterizer(const Rasterizer &) = default;

  DOCUMENT("The mask determining which samples are written to.");
  uint32_t sampleMask = ~0U;

  DOCUMENT("A list of :class:`Viewport` with the bound viewports.");
  rdcarray<Viewport> viewports;

  DOCUMENT("A list of :class:`Scissor` with the bound scissor regions.");
  rdcarray<Scissor> scissors;

  DOCUMENT("A :class:`D3D12RasterizerState` with the details of the rasterization state.");
  RasterizerState state;
};

DOCUMENT("Describes the state of the depth-stencil state in the PSO.");
struct DepthStencilState
{
  DOCUMENT("");
  DepthStencilState() = default;
  DepthStencilState(const DepthStencilState &) = default;

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

  DOCUMENT("A :class:`StencilFace` describing what happens for front-facing polygons.");
  StencilFace frontFace;
  DOCUMENT("A :class:`StencilFace` describing what happens for back-facing polygons.");
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

  DOCUMENT("``True`` if alpha-to-coverage should be used when blending to an MSAA target.");
  bool alphaToCoverage = false;
  DOCUMENT(R"(``True`` if independent blending for each target should be used.

``False`` if the first blend should be applied to all targets.
)");
  bool independentBlend = false;

  DOCUMENT("A list of :class:`ColorBlend` describing the blend operations for each target.");
  rdcarray<ColorBlend> blends;

  DOCUMENT("The constant blend factor to use in blend equations.");
  float blendFactor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

DOCUMENT("Describes the current state of the output-merger stage of the D3D12 pipeline.");
struct OM
{
  DOCUMENT("");
  OM() = default;
  OM(const OM &) = default;

  DOCUMENT("A :class:`D3D12DepthStencilState` with the details of the depth-stencil state.");
  DepthStencilState depthStencilState;
  DOCUMENT("A :class:`D3D12BlendState` with the details of the blend state.");
  BlendState blendState;

  DOCUMENT("A list of :class:`D3D12View` describing the bound render targets.");
  rdcarray<View> renderTargets;

  DOCUMENT("A :class:`D3D12View` with details of the bound depth-stencil target.");
  View depthTarget;
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

  DOCUMENT("A list of :class:`D3D12ResourceState` entries, one for each subresource.");
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

  DOCUMENT("A :class:`D3D12InputAssembly` describing the input assembly pipeline stage.");
  InputAssembly inputAssembly;

  DOCUMENT("A :class:`D3D12Shader` describing the vertex shader stage.");
  Shader vertexShader;
  DOCUMENT("A :class:`D3D12Shader` describing the hull shader stage.");
  Shader hullShader;
  DOCUMENT("A :class:`D3D12Shader` describing the domain shader stage.");
  Shader domainShader;
  DOCUMENT("A :class:`D3D12Shader` describing the geometry shader stage.");
  Shader geometryShader;
  DOCUMENT("A :class:`D3D12Shader` describing the pixel shader stage.");
  Shader pixelShader;
  DOCUMENT("A :class:`D3D12Shader` describing the compute shader stage.");
  Shader computeShader;

  DOCUMENT("A :class:`D3D12StreamOut` describing the stream-out pipeline stage.");
  StreamOut streamOut;

  DOCUMENT("A :class:`D3D12Rasterizer` describing the rasterizer pipeline stage.");
  Rasterizer rasterizer;

  DOCUMENT("A :class:`D3D12OM` describing the output merger pipeline stage.");
  OM outputMerger;

  DOCUMENT("A list of :class:`D3D12ResourceData` entries, one for each resource.");
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
DECLARE_REFLECTION_STRUCT(D3D12Pipe::RegisterSpace);
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
