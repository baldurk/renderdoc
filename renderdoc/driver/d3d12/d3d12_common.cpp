/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include "d3d12_common.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/dxgi/dxgi_wrapped.h"
#include "d3d12_manager.h"
#include "d3d12_resources.h"

enum D3D12ResourceBarrierSubresource
{
  D3D12AllSubresources = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
};

string ToStrHelper<false, D3D12ResourceBarrierSubresource>::Get(const D3D12ResourceBarrierSubresource &el)
{
  if(el == D3D12AllSubresources)
    return "All Subresources";

  return ToStr::Get(uint32_t(el));
}

enum D3D12ComponentMapping
{
};

UINT GetResourceNumMipLevels(const D3D12_RESOURCE_DESC *desc)
{
  switch(desc->Dimension)
  {
    default:
    case D3D12_RESOURCE_DIMENSION_UNKNOWN:
      RDCERR("Unexpected resource dimension! %d", desc->Dimension);
      break;
    case D3D12_RESOURCE_DIMENSION_BUFFER: return 1;
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
    {
      if(desc->MipLevels)
        return desc->MipLevels;
      UINT w = RDCMAX(1U, UINT(desc->Width));
      UINT count = 1;
      while(w > 1)
      {
        ++count;
        w = RDCMAX(1U, w >> 1U);
      }
      return count;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
    {
      if(desc->MipLevels)
        return desc->MipLevels;
      UINT w = RDCMAX(1U, UINT(desc->Width));
      UINT h = RDCMAX(1U, desc->Height);
      UINT count = 1;
      while(w > 1 || h > 1)
      {
        ++count;
        w = RDCMAX(1U, w >> 1U);
        h = RDCMAX(1U, h >> 1U);
      }
      return count;
    }
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
    {
      if(desc->MipLevels)
        return desc->MipLevels;
      UINT w = RDCMAX(1U, UINT(desc->Width));
      UINT h = RDCMAX(1U, desc->Height);
      UINT d = RDCMAX(1U, UINT(desc->DepthOrArraySize));
      UINT count = 1;
      while(w > 1 || h > 1 || d > 1)
      {
        ++count;
        w = RDCMAX(1U, w >> 1U);
        h = RDCMAX(1U, h >> 1U);
        d = RDCMAX(1U, d >> 1U);
      }
      return count;
    }
  }

  return 1;
}

UINT GetNumSubresources(const D3D12_RESOURCE_DESC *desc)
{
  switch(desc->Dimension)
  {
    default:
    case D3D12_RESOURCE_DIMENSION_UNKNOWN:
      RDCERR("Unexpected resource dimension! %d", desc->Dimension);
      break;
    case D3D12_RESOURCE_DIMENSION_BUFFER: return 1;
    case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
    case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
      return RDCMAX((UINT16)1, desc->DepthOrArraySize) * GetResourceNumMipLevels(desc);
    case D3D12_RESOURCE_DIMENSION_TEXTURE3D: return GetResourceNumMipLevels(desc);
  }

  return 1;
}

string ToStrHelper<false, D3D12ComponentMapping>::Get(const D3D12ComponentMapping &el)
{
  string ret;

  // value should always be <= 5, see D3D12_SHADER_COMPONENT_MAPPING
  const char mapping[] = {'R', 'G', 'B', 'A', '0', '1', '?', '?'};

  uint32_t swizzle = (uint32_t)el;

  for(int i = 0; i < 4; i++)
    ret += mapping[D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(i, swizzle)];

  return ret;
}

#define SerialiseObject(type, name, obj)                                                        \
  {                                                                                             \
    D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData();                           \
    ResourceId id;                                                                              \
    if(m_Mode >= WRITING)                                                                       \
      id = GetResID(obj);                                                                       \
    Serialise(name, id);                                                                        \
    if(m_Mode < WRITING)                                                                        \
      obj = (id == ResourceId() || !rm->HasLiveResource(id)) ? NULL                             \
                                                             : Unwrap(rm->GetLiveAs<type>(id)); \
  }

#define SerialiseWrappedObject(type, name, obj)                                                \
  {                                                                                            \
    D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData();                          \
    ResourceId id;                                                                             \
    if(m_Mode >= WRITING)                                                                      \
      id = GetResID(obj);                                                                      \
    Serialise(name, id);                                                                       \
    if(m_Mode < WRITING)                                                                       \
      obj = (id == ResourceId() || !rm->HasLiveResource(id)) ? NULL : rm->GetLiveAs<type>(id); \
  }

template <>
void Serialiser::Serialise(const char *name, D3D12Descriptor &el)
{
  ScopedContext scope(this, name, "D3D12Descriptor", 0, true);

  D3D12Descriptor::DescriptorType type = el.GetType();
  Serialise("type", type);

  // we serialise the heap by hand because we want to keep it wrapped
  {
    D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData();

    PortableHandle handle;

    if(m_Mode >= WRITING)
      handle = PortableHandle(el.samp.heap->GetResourceID(), el.samp.idx);

    Serialise("handle", handle);

    if(m_Mode < WRITING)
    {
      el.samp.heap = (handle.heap == ResourceId() || !rm->HasLiveResource(handle.heap))
                         ? NULL
                         : rm->GetLiveAs<WrappedID3D12DescriptorHeap>(handle.heap);
      el.samp.idx = handle.index;
    }
  }

  // for sampler types, this will be overwritten when serialising the sampler descriptor
  el.nonsamp.type = type;

  switch(type)
  {
    case D3D12Descriptor::TypeSampler:
    {
      Serialise("Descriptor", el.samp.desc);
      RDCASSERTEQUAL(el.GetType(), D3D12Descriptor::TypeSampler);
      break;
    }
    case D3D12Descriptor::TypeCBV:
    {
      Serialise("Descriptor", el.nonsamp.cbv);
      break;
    }
    case D3D12Descriptor::TypeSRV:
    {
      SerialiseWrappedObject(ID3D12Resource, "Resource", el.nonsamp.resource);
      Serialise("Descriptor", el.nonsamp.srv);
      break;
    }
    case D3D12Descriptor::TypeRTV:
    {
      SerialiseWrappedObject(ID3D12Resource, "Resource", el.nonsamp.resource);
      Serialise("Descriptor", el.nonsamp.rtv);
      break;
    }
    case D3D12Descriptor::TypeDSV:
    {
      SerialiseWrappedObject(ID3D12Resource, "Resource", el.nonsamp.resource);
      Serialise("Descriptor", el.nonsamp.dsv);
      break;
    }
    case D3D12Descriptor::TypeUAV:
    {
      SerialiseWrappedObject(ID3D12Resource, "Resource", el.nonsamp.resource);
      SerialiseWrappedObject(ID3D12Resource, "CounterResource", el.nonsamp.uav.counterResource);

      // special case because of extra resource and squeezed descriptor
      D3D12_UNORDERED_ACCESS_VIEW_DESC desc = el.nonsamp.uav.desc.AsDesc();
      Serialise("Descriptor", desc);
      el.nonsamp.uav.desc.Init(desc);
      break;
    }
    case D3D12Descriptor::TypeUndefined:
    {
      el.nonsamp.type = type;
      break;
    }
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_RESOURCE_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_RESOURCE_DESC", 0, true);

  Serialise("Dimension", el.Dimension);
  Serialise("Alignment", el.Alignment);
  Serialise("Width", el.Width);
  Serialise("Height", el.Height);
  Serialise("DepthOrArraySize", el.DepthOrArraySize);
  Serialise("MipLevels", el.MipLevels);
  Serialise("Format", el.Format);
  Serialise("SampleDesc", el.SampleDesc);
  Serialise("Layout", el.Layout);
  Serialise("Flags", el.Flags);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_COMMAND_QUEUE_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_COMMAND_QUEUE_DESC", 0, true);

  Serialise("Type", el.Type);
  Serialise("Priority", el.Priority);
  Serialise("Flags", el.Flags);
  Serialise("NodeMask", el.NodeMask);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_SHADER_BYTECODE &el)
{
  ScopedContext scope(this, name, "D3D12_SHADER_BYTECODE", 0, true);

  uint64_t dataSize = el.BytecodeLength;
  Serialise("BytecodeLength", dataSize);
  size_t sz = (size_t)dataSize;
  if(m_Mode == READING)
  {
    el.pShaderBytecode = NULL;
    el.BytecodeLength = sz;
  }

  if(dataSize > 0)
    SerialiseBuffer("pShaderBytecode", (byte *&)el.pShaderBytecode, sz);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_SO_DECLARATION_ENTRY &el)
{
  ScopedContext scope(this, name, "D3D12_SO_DECLARATION_ENTRY", 0, true);

  Serialise("Stream", el.Stream);

  {
    string s = "";
    if(m_Mode == WRITING && el.SemanticName)
      s = el.SemanticName;

    Serialise("SemanticName", s);

    if(m_Mode == READING)
    {
      m_StringDB.insert(s);
      el.SemanticName = m_StringDB.find(s)->c_str();
    }
  }

  Serialise("SemanticIndex", el.SemanticIndex);
  Serialise("StartComponent", el.StartComponent);
  Serialise("ComponentCount", el.ComponentCount);
  Serialise("OutputSlot", el.OutputSlot);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_STREAM_OUTPUT_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_STREAM_OUTPUT_DESC", 0, true);

  if(m_Mode == READING)
  {
    el.pSODeclaration = NULL;
    el.pBufferStrides = NULL;
  }

  SerialiseComplexArray("pSODeclaration", (D3D12_SO_DECLARATION_ENTRY *&)el.pSODeclaration,
                        el.NumEntries);
  SerialisePODArray("pBufferStrides", (UINT *&)el.pBufferStrides, el.NumStrides);
  Serialise("RasterizedStream", el.RasterizedStream);
}

template <>
void Serialiser::Deserialise(const D3D12_STREAM_OUTPUT_DESC *const el) const
{
  if(m_Mode == READING)
  {
    delete[] el->pSODeclaration;
    delete[] el->pBufferStrides;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_RENDER_TARGET_BLEND_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_RENDER_TARGET_BLEND_DESC", 0, true);

  Serialise("BlendEnable", el.BlendEnable);
  Serialise("LogicOpEnable", el.LogicOpEnable);
  Serialise("SrcBlend", el.SrcBlend);
  Serialise("DestBlend", el.DestBlend);
  Serialise("BlendOp", el.BlendOp);
  Serialise("SrcBlendAlpha", el.SrcBlendAlpha);
  Serialise("DestBlendAlpha", el.DestBlendAlpha);
  Serialise("BlendOpAlpha", el.BlendOpAlpha);
  Serialise("LogicOp", el.LogicOp);
  Serialise("RenderTargetWriteMask", el.RenderTargetWriteMask);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_BLEND_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_BLEND_DESC", 0, true);

  Serialise("AlphaToCoverageEnable", el.AlphaToCoverageEnable);
  Serialise("IndependentBlendEnable", el.IndependentBlendEnable);
  for(int i = 0; i < 8; i++)
  {
    ScopedContext targetscope(this, name, "D3D12_RENDER_TARGET_BLEND_DESC", 0, true);

    bool enable = el.RenderTarget[i].BlendEnable == TRUE;
    Serialise("BlendEnable", enable);
    el.RenderTarget[i].BlendEnable = enable;

    enable = el.RenderTarget[i].LogicOpEnable == TRUE;
    Serialise("LogicOpEnable", enable);
    el.RenderTarget[i].LogicOpEnable = enable;

    Serialise("SrcBlend", el.RenderTarget[i].SrcBlend);
    Serialise("DestBlend", el.RenderTarget[i].DestBlend);
    Serialise("BlendOp", el.RenderTarget[i].BlendOp);
    Serialise("SrcBlendAlpha", el.RenderTarget[i].SrcBlendAlpha);
    Serialise("DestBlendAlpha", el.RenderTarget[i].DestBlendAlpha);
    Serialise("BlendOpAlpha", el.RenderTarget[i].BlendOpAlpha);
    Serialise("LogicOp", el.RenderTarget[i].LogicOp);
    Serialise("RenderTargetWriteMask", el.RenderTarget[i].RenderTargetWriteMask);
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_RASTERIZER_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_RASTERIZER_DESC", 0, true);

  Serialise("FillMode", el.FillMode);
  Serialise("CullMode", el.CullMode);
  Serialise("FrontCounterClockwise", el.FrontCounterClockwise);
  Serialise("DepthBias", el.DepthBias);
  Serialise("DepthBiasClamp", el.DepthBiasClamp);
  Serialise("SlopeScaledDepthBias", el.SlopeScaledDepthBias);
  Serialise("DepthClipEnable", el.DepthClipEnable);
  Serialise("MultisampleEnable", el.MultisampleEnable);
  Serialise("AntialiasedLineEnable", el.AntialiasedLineEnable);
  Serialise("ForcedSampleCount", el.ForcedSampleCount);
  Serialise("ConservativeRaster", el.ConservativeRaster);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_DEPTH_STENCILOP_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_DEPTH_STENCILOP_DESC", 0, true);

  Serialise("StencilFailOp", el.StencilFailOp);
  Serialise("StencilDepthFailOp", el.StencilDepthFailOp);
  Serialise("StencilPassOp", el.StencilPassOp);
  Serialise("StencilFunc", el.StencilFunc);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_DEPTH_STENCIL_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_DEPTH_STENCIL_DESC", 0, true);

  Serialise("DepthEnable", el.DepthEnable);
  Serialise("DepthWriteMask", el.DepthWriteMask);
  Serialise("DepthFunc", el.DepthFunc);
  Serialise("StencilEnable", el.StencilEnable);
  Serialise("StencilReadMask", el.StencilReadMask);
  Serialise("StencilWriteMask", el.StencilWriteMask);
  Serialise("FrontFace", el.FrontFace);
  Serialise("BackFace", el.BackFace);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_INPUT_ELEMENT_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_INPUT_ELEMENT_DESC", 0, true);

  {
    string s = "";
    if(m_Mode == WRITING && el.SemanticName)
      s = el.SemanticName;

    Serialise("SemanticName", s);

    if(m_Mode == READING)
    {
      m_StringDB.insert(s);
      el.SemanticName = m_StringDB.find(s)->c_str();
    }
  }

  Serialise("SemanticIndex", el.SemanticIndex);
  Serialise("Format", el.Format);
  Serialise("InputSlot", el.InputSlot);
  Serialise("AlignedByteOffset", el.AlignedByteOffset);
  Serialise("InputSlotClass", el.InputSlotClass);
  Serialise("InstanceDataStepRate", el.InstanceDataStepRate);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_INPUT_LAYOUT_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_INPUT_LAYOUT_DESC", 0, true);

  SerialiseComplexArray("pInputElementDescs", (D3D12_INPUT_ELEMENT_DESC *&)el.pInputElementDescs,
                        el.NumElements);
}

template <>
void Serialiser::Deserialise(const D3D12_INPUT_LAYOUT_DESC *const el) const
{
  if(m_Mode == READING)
    delete[] el->pInputElementDescs;
}

template <>
void Serialiser::Serialise(const char *name, D3D12_INDIRECT_ARGUMENT_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_INDIRECT_ARGUMENT_DESC", 0, true);

  Serialise("Type", el.Type);

  switch(el.Type)
  {
    case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
    case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
    case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH:
    case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
      // nothing to serialise
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
      Serialise("VertexBuffer.Slot", el.VertexBuffer.Slot);
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT:
      Serialise("Constant.RootParameterIndex", el.Constant.RootParameterIndex);
      Serialise("Constant.DestOffsetIn32BitValues", el.Constant.DestOffsetIn32BitValues);
      Serialise("Constant.Num32BitValuesToSet", el.Constant.Num32BitValuesToSet);
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW:
      Serialise("ConstantBufferView.RootParameterIndex", el.ConstantBufferView.RootParameterIndex);
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW:
      Serialise("ShaderResourceView.RootParameterIndex", el.ShaderResourceView.RootParameterIndex);
      break;
    case D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW:
      Serialise("UnorderedAccessView.RootParameterIndex", el.UnorderedAccessView.RootParameterIndex);
      break;
    default: RDCERR("Unexpected indirect argument type: %u", el.Type); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_COMMAND_SIGNATURE_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_COMMAND_SIGNATURE_DESC", 0, true);

  Serialise("ByteStride", el.ByteStride);
  Serialise("NodeMask", el.NodeMask);
  SerialiseComplexArray("pArgumentDescs", (D3D12_INDIRECT_ARGUMENT_DESC *&)el.pArgumentDescs,
                        el.NumArgumentDescs);
}

template <>
void Serialiser::Deserialise(const D3D12_COMMAND_SIGNATURE_DESC *const el) const
{
  if(m_Mode == READING)
    delete[] el->pArgumentDescs;
}

template <>
void Serialiser::Serialise(const char *name, D3D12_GRAPHICS_PIPELINE_STATE_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_GRAPHICS_PIPELINE_STATE_DESC", 0, true);

  SerialiseObject(ID3D12RootSignature, "pRootSignature", el.pRootSignature);
  Serialise("VS", el.VS);
  Serialise("PS", el.PS);
  Serialise("DS", el.DS);
  Serialise("HS", el.HS);
  Serialise("GS", el.GS);
  Serialise("StreamOutput", el.StreamOutput);
  Serialise("BlendState", el.BlendState);
  Serialise("SampleMask", el.SampleMask);
  Serialise("RasterizerState", el.RasterizerState);
  Serialise("DepthStencilState", el.DepthStencilState);
  Serialise("InputLayout", el.InputLayout);
  Serialise("IBStripCutValue", el.IBStripCutValue);
  Serialise("PrimitiveTopologyType", el.PrimitiveTopologyType);
  Serialise("NumRenderTargets", el.NumRenderTargets);
  SerialisePODArray<8>("RTVFormats", el.RTVFormats);
  Serialise("DSVFormat", el.DSVFormat);
  Serialise("SampleDesc", el.SampleDesc);
  Serialise("NodeMask", el.NodeMask);
  Serialise("Flags", el.Flags);

  if(m_Mode == READING)
  {
    el.CachedPSO.CachedBlobSizeInBytes = 0;
    el.CachedPSO.pCachedBlob = NULL;
  }
}

template <>
void Serialiser::Deserialise(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *const el) const
{
  if(m_Mode == READING)
  {
    delete[](byte *)(el->VS.pShaderBytecode);
    delete[](byte *)(el->PS.pShaderBytecode);
    delete[](byte *)(el->DS.pShaderBytecode);
    delete[](byte *)(el->HS.pShaderBytecode);
    delete[](byte *)(el->GS.pShaderBytecode);
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_COMPUTE_PIPELINE_STATE_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_COMPUTE_PIPELINE_STATE_DESC", 0, true);

  SerialiseObject(ID3D12RootSignature, "pRootSignature", el.pRootSignature);
  Serialise("CS", el.CS);
  Serialise("NodeMask", el.NodeMask);
  Serialise("Flags", el.Flags);

  if(m_Mode == READING)
  {
    el.CachedPSO.CachedBlobSizeInBytes = 0;
    el.CachedPSO.pCachedBlob = NULL;
  }
}

template <>
void Serialiser::Deserialise(const D3D12_COMPUTE_PIPELINE_STATE_DESC *const el) const
{
  if(m_Mode == READING)
    delete[](byte *)(el->CS.pShaderBytecode);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_VERTEX_BUFFER_VIEW &el)
{
  ScopedContext scope(this, name, "D3D12_VERTEX_BUFFER_VIEW", 0, true);

  D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData();

  ResourceId buffer;
  UINT64 offs = 0;

  if(m_Mode == WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(el.BufferLocation, buffer, offs);

  Serialise("BufferLocation", buffer);
  Serialise("BufferLocation_Offset", offs);

  if(m_Mode == READING)
  {
    ID3D12Resource *res = rm->GetLiveAs<ID3D12Resource>(buffer);
    if(res)
      el.BufferLocation = res->GetGPUVirtualAddress() + offs;
    else
      el.BufferLocation = 0;
  }

  Serialise("SizeInBytes", el.SizeInBytes);
  Serialise("StrideInBytes", el.StrideInBytes);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_INDEX_BUFFER_VIEW &el)
{
  ScopedContext scope(this, name, "D3D12_INDEX_BUFFER_VIEW", 0, true);

  D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData();

  ResourceId buffer;
  UINT64 offs = 0;

  if(m_Mode == WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(el.BufferLocation, buffer, offs);

  Serialise("BufferLocation", buffer);
  Serialise("BufferLocation_Offset", offs);

  if(m_Mode == READING)
  {
    ID3D12Resource *res = rm->GetLiveAs<ID3D12Resource>(buffer);
    if(res)
      el.BufferLocation = res->GetGPUVirtualAddress() + offs;
    else
      el.BufferLocation = 0;
  }

  Serialise("SizeInBytes", el.SizeInBytes);
  Serialise("Format", el.Format);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_CONSTANT_BUFFER_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_CONSTANT_BUFFER_VIEW_DESC", 0, true);

  D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData();

  ResourceId buffer;
  UINT64 offs = 0;

  if(m_Mode == WRITING)
    WrappedID3D12Resource::GetResIDFromAddr(el.BufferLocation, buffer, offs);

  Serialise("BufferLocation", buffer);
  Serialise("BufferLocation_Offset", offs);

  if(m_Mode == READING)
  {
    ID3D12Resource *res = NULL;

    if(buffer != ResourceId() && rm->HasLiveResource(buffer))
      res = rm->GetLiveAs<ID3D12Resource>(buffer);

    if(res)
      el.BufferLocation = res->GetGPUVirtualAddress() + offs;
    else
      el.BufferLocation = 0;
  }

  Serialise("SizeInBytes", el.SizeInBytes);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_SHADER_RESOURCE_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_SHADER_RESOURCE_VIEW_DESC", 0, true);

  Serialise("Format", el.Format);
  Serialise("ViewDimension", el.ViewDimension);
  // cast to a special enum so we print nicely
  Serialise("Shader4ComponentMapping", (D3D12ComponentMapping &)el.Shader4ComponentMapping);

  switch(el.ViewDimension)
  {
    case D3D12_SRV_DIMENSION_UNKNOWN:
      // indicates an empty descriptor, which comes from a NULL parameter to Create.
      break;
    case D3D12_SRV_DIMENSION_BUFFER:
      Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
      Serialise("Buffer.NumElements", el.Buffer.NumElements);
      Serialise("Buffer.StructureByteStride", el.Buffer.StructureByteStride);
      Serialise("Buffer.Flags", el.Buffer.Flags);
      break;
    case D3D12_SRV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MostDetailedMip", el.Texture1D.MostDetailedMip);
      Serialise("Texture1D.MipLevels", el.Texture1D.MipLevels);
      Serialise("Texture1D.ResourceMinLODClamp", el.Texture1D.ResourceMinLODClamp);
      break;
    case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MostDetailedMip", el.Texture1DArray.MostDetailedMip);
      Serialise("Texture1DArray.MipLevels", el.Texture1DArray.MipLevels);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      Serialise("Texture1DArray.ResourceMinLODClamp", el.Texture1DArray.ResourceMinLODClamp);
      break;
    case D3D12_SRV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MostDetailedMip", el.Texture2D.MostDetailedMip);
      Serialise("Texture2D.MipLevels", el.Texture2D.MipLevels);
      Serialise("Texture2D.PlaneSlice", el.Texture2D.PlaneSlice);
      Serialise("Texture2D.ResourceMinLODClamp", el.Texture2D.ResourceMinLODClamp);
      break;
    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MostDetailedMip", el.Texture2DArray.MostDetailedMip);
      Serialise("Texture2DArray.MipLevels", el.Texture2DArray.MipLevels);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      Serialise("Texture2DArray.PlaneSlice", el.Texture2DArray.PlaneSlice);
      Serialise("Texture2DArray.ResourceMinLODClamp", el.Texture2DArray.ResourceMinLODClamp);
      break;
    case D3D12_SRV_DIMENSION_TEXTURE2DMS:
      // el.Texture2DMS.UnusedField_NothingToDefine
      break;
    case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY:
      Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
      Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
      break;
    case D3D12_SRV_DIMENSION_TEXTURE3D:
      Serialise("Texture3D.MipLevels", el.Texture3D.MipLevels);
      Serialise("Texture3D.MostDetailedMip", el.Texture3D.MostDetailedMip);
      Serialise("Texture3D.ResourceMinLODClamp", el.Texture3D.ResourceMinLODClamp);
      break;
    case D3D12_SRV_DIMENSION_TEXTURECUBE:
      Serialise("TextureCube.MostDetailedMip", el.TextureCube.MostDetailedMip);
      Serialise("TextureCube.MipLevels", el.TextureCube.MipLevels);
      Serialise("TextureCube.ResourceMinLODClamp", el.TextureCube.ResourceMinLODClamp);
      break;
    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
      Serialise("TextureCubeArray.MostDetailedMip", el.TextureCubeArray.MostDetailedMip);
      Serialise("TextureCubeArray.MipLevels", el.TextureCubeArray.MipLevels);
      Serialise("TextureCubeArray.First2DArrayFace", el.TextureCubeArray.First2DArrayFace);
      Serialise("TextureCubeArray.NumCubes", el.TextureCubeArray.NumCubes);
      Serialise("TextureCubeArray.ResourceMinLODClamp", el.TextureCubeArray.ResourceMinLODClamp);
      break;
    default: RDCERR("Unrecognised SRV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_RENDER_TARGET_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_RENDER_TARGET_VIEW_DESC", 0, true);

  Serialise("Format", el.Format);
  Serialise("ViewDimension", el.ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D12_RTV_DIMENSION_UNKNOWN:
      // indicates an empty descriptor, which comes from a NULL parameter to Create.
      break;
    case D3D12_RTV_DIMENSION_BUFFER:
      Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
      Serialise("Buffer.NumElements", el.Buffer.NumElements);
      break;
    case D3D12_RTV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
      break;
    case D3D12_RTV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      break;
    case D3D12_RTV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
      Serialise("Texture2D.PlaneSlice", el.Texture2D.PlaneSlice);
      break;
    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      Serialise("Texture2DArray.PlaneSlice", el.Texture2DArray.PlaneSlice);
      break;
    case D3D12_RTV_DIMENSION_TEXTURE2DMS:
      // el.Texture2DMS.UnusedField_NothingToDefine
      break;
    case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY:
      Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
      Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
      break;
    case D3D12_RTV_DIMENSION_TEXTURE3D:
      Serialise("Texture3D.MipSlice", el.Texture3D.MipSlice);
      Serialise("Texture3D.FirstWSlice", el.Texture3D.FirstWSlice);
      Serialise("Texture3D.WSize", el.Texture3D.WSize);
      break;
    default: RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_DEPTH_STENCIL_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_DEPTH_STENCIL_VIEW_DESC", 0, true);

  Serialise("Format", el.Format);
  Serialise("Flags", el.Flags);
  Serialise("ViewDimension", el.ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D12_DSV_DIMENSION_UNKNOWN:
      // indicates an empty descriptor, which comes from a NULL parameter to Create.
      break;
    case D3D12_DSV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
      break;
    case D3D12_DSV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      break;
    case D3D12_DSV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
      break;
    case D3D12_DSV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      break;
    case D3D12_DSV_DIMENSION_TEXTURE2DMS:
      // el.Texture2DMS.UnusedField_NothingToDefine
      break;
    case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY:
      Serialise("Texture2DMSArray.FirstArraySlice", el.Texture2DMSArray.FirstArraySlice);
      Serialise("Texture2DMSArray.ArraySize", el.Texture2DMSArray.ArraySize);
      break;
    default: RDCERR("Unrecognised DSV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_UNORDERED_ACCESS_VIEW_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_UNORDERED_ACCESS_VIEW_DESC", 0, true);

  Serialise("Format", el.Format);
  Serialise("ViewDimension", el.ViewDimension);

  switch(el.ViewDimension)
  {
    case D3D12_UAV_DIMENSION_UNKNOWN:
      // indicates an empty descriptor, which comes from a NULL parameter to Create.
      break;
    case D3D12_UAV_DIMENSION_BUFFER:
      Serialise("Buffer.FirstElement", el.Buffer.FirstElement);
      Serialise("Buffer.NumElements", el.Buffer.NumElements);
      Serialise("Buffer.StructureByteStride", el.Buffer.StructureByteStride);
      Serialise("Buffer.CounterOffsetInBytes", el.Buffer.CounterOffsetInBytes);
      Serialise("Buffer.Flags", el.Buffer.Flags);
      break;
    case D3D12_UAV_DIMENSION_TEXTURE1D:
      Serialise("Texture1D.MipSlice", el.Texture1D.MipSlice);
      break;
    case D3D12_UAV_DIMENSION_TEXTURE1DARRAY:
      Serialise("Texture1DArray.MipSlice", el.Texture1DArray.MipSlice);
      Serialise("Texture1DArray.FirstArraySlice", el.Texture1DArray.FirstArraySlice);
      Serialise("Texture1DArray.ArraySize", el.Texture1DArray.ArraySize);
      break;
    case D3D12_UAV_DIMENSION_TEXTURE2D:
      Serialise("Texture2D.MipSlice", el.Texture2D.MipSlice);
      Serialise("Texture2D.PlaneSlice", el.Texture2D.PlaneSlice);
      break;
    case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
      Serialise("Texture2DArray.MipSlice", el.Texture2DArray.MipSlice);
      Serialise("Texture2DArray.FirstArraySlice", el.Texture2DArray.FirstArraySlice);
      Serialise("Texture2DArray.ArraySize", el.Texture2DArray.ArraySize);
      Serialise("Texture2DArray.PlaneSlice", el.Texture2DArray.PlaneSlice);
      break;
    case D3D12_UAV_DIMENSION_TEXTURE3D:
      Serialise("Texture3D.MipSlice", el.Texture3D.MipSlice);
      Serialise("Texture3D.FirstWSlice", el.Texture3D.FirstWSlice);
      Serialise("Texture3D.WSize", el.Texture3D.WSize);
      break;
    default: RDCERR("Unrecognised RTV Dimension %d", el.ViewDimension); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_RESOURCE_BARRIER &el)
{
  ScopedContext scope(this, name, "D3D12_RESOURCE_BARRIER", 0, true);

  Serialise("Type", el.Type);
  Serialise("Flags", el.Flags);

  switch(el.Type)
  {
    case D3D12_RESOURCE_BARRIER_TYPE_TRANSITION:
    {
      SerialiseObject(ID3D12Resource, "Transition.pResource", el.Transition.pResource);
      // cast to a special enum so we print 'all subresources' nicely
      RDCCOMPILE_ASSERT(sizeof(D3D12ResourceBarrierSubresource) == sizeof(UINT),
                        "Enum isn't uint sized");
      Serialise("Transition.Subresource",
                (D3D12ResourceBarrierSubresource &)el.Transition.Subresource);
      Serialise("Transition.StateBefore", el.Transition.StateBefore);
      Serialise("Transition.StateAfter", el.Transition.StateAfter);
      break;
    }
    case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
    {
      SerialiseObject(ID3D12Resource, "Aliasing.pResourceBefore", el.Aliasing.pResourceBefore);
      SerialiseObject(ID3D12Resource, "Aliasing.pResourceAfter", el.Aliasing.pResourceAfter);
      break;
    }
    case D3D12_RESOURCE_BARRIER_TYPE_UAV:
    {
      SerialiseObject(ID3D12Resource, "UAV.pResource", el.UAV.pResource);
      break;
    }
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_HEAP_PROPERTIES &el)
{
  ScopedContext scope(this, name, "D3D12_HEAP_PROPERTIES", 0, true);

  Serialise("Type", el.Type);
  Serialise("CPUPageProperty", el.CPUPageProperty);
  Serialise("MemoryPoolPreference", el.MemoryPoolPreference);
  Serialise("CreationNodeMask", el.CreationNodeMask);
  Serialise("VisibleNodeMask", el.VisibleNodeMask);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_HEAP_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_HEAP_DESC", 0, true);

  Serialise("SizeInBytes", el.SizeInBytes);
  Serialise("Properties", el.Properties);
  Serialise("Alignment", el.Alignment);
  Serialise("Flags", el.Flags);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_DESCRIPTOR_HEAP_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_DESCRIPTOR_HEAP_DESC", 0, true);

  Serialise("Type", el.Type);
  Serialise("NumDescriptors", el.NumDescriptors);
  Serialise("Flags", el.Flags);
  Serialise("NodeMask", el.NodeMask);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_QUERY_HEAP_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_QUERY_HEAP_DESC", 0, true);

  Serialise("Type", el.Type);
  Serialise("Count", el.Count);
  Serialise("NodeMask", el.NodeMask);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_CLEAR_VALUE &el)
{
  ScopedContext scope(this, name, "D3D12_CLEAR_VALUE", 0, true);

  Serialise("Format", el.Format);

  if(!IsDepthFormat(el.Format))
  {
    SerialisePODArray<4>("Color", el.Color);
  }
  else
  {
    Serialise("Depth", el.DepthStencil.Depth);
    Serialise("Stencil", el.DepthStencil.Stencil);
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_SUBRESOURCE_FOOTPRINT &el)
{
  ScopedContext scope(this, name, "D3D12_SUBRESOURCE_FOOTPRINT", 0, true);

  Serialise("Format", el.Format);
  Serialise("Width", el.Width);
  Serialise("Height", el.Height);
  Serialise("Depth", el.Depth);
  Serialise("RowPitch", el.RowPitch);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_TEXTURE_COPY_LOCATION &el)
{
  ScopedContext scope(this, name, "D3D12_TEXTURE_COPY_LOCATION", 0, true);

  SerialiseObject(ID3D12Resource, "pResource", el.pResource);
  Serialise("Type", el.Type);

  switch(el.Type)
  {
    case D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT:
      Serialise("PlacedFootprint.Footprint", el.PlacedFootprint.Footprint);
      Serialise("PlacedFootprint.Offset", el.PlacedFootprint.Offset);
      break;
    case D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX:
      Serialise("SubresourceIndex", el.SubresourceIndex);
      break;
    default: RDCERR("Unexpected texture copy type %d", el.Type); break;
  }
}

template <>
void Serialiser::Serialise(const char *name, D3D12_DISCARD_REGION &el)
{
  ScopedContext scope(this, name, "D3D12_DISCARD_REGION", 0, true);

  Serialise("FirstSubresource", el.FirstSubresource);
  Serialise("NumSubresources", el.NumSubresources);

  SerialiseComplexArray("pRects", (D3D12_RECT *&)el.pRects, el.NumRects);
}

template <>
void Serialiser::Deserialise(const D3D12_DISCARD_REGION *const el) const
{
  if(m_Mode == READING)
    delete[] el->pRects;
}

template <>
void Serialiser::Serialise(const char *name, D3D12_SAMPLER_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_SAMPLER_DESC", 0, true);

  Serialise("Filter", el.Filter);
  Serialise("AddressU", el.AddressU);
  Serialise("AddressV", el.AddressV);
  Serialise("AddressW", el.AddressW);
  Serialise("MipLODBias", el.MipLODBias);
  Serialise("MaxAnisotropy", el.MaxAnisotropy);
  Serialise("ComparisonFunc", el.ComparisonFunc);
  SerialisePODArray<4>("BorderColor", el.BorderColor);
  Serialise("MinLOD", el.MinLOD);
  Serialise("MaxLOD", el.MaxLOD);
}

string ToStrHelper<false, D3D12_VIEWPORT>::Get(const D3D12_VIEWPORT &el)
{
  return StringFormat::Fmt("Viewport<%.0fx%.0f+%.0f+%.0f z=%f->%f>", el.Width, el.Height,
                           el.TopLeftX, el.TopLeftY, el.MinDepth, el.MaxDepth);
}

string ToStrHelper<false, D3D12_BOX>::Get(const D3D12_BOX &el)
{
  return StringFormat::Fmt("Box<%u,%u,%u -> %u,%u,%u>", el.left, el.top, el.front, el.right,
                           el.bottom, el.back);
}

string ToStrHelper<false, PortableHandle>::Get(const PortableHandle &el)
{
  if(el.heap == ResourceId())
    return "NULL";

  return StringFormat::Fmt("D3D12_CPU_DESCRIPTOR_HANDLE(%s, %u)", ToStr::Get(el.heap).c_str(),
                           el.index);
}

string ToStrHelper<false, D3D12Descriptor::DescriptorType>::Get(const D3D12Descriptor::DescriptorType &el)
{
  switch(el)
  {
    case D3D12Descriptor::TypeCBV: return "CBV";
    case D3D12Descriptor::TypeSRV: return "SRV";
    case D3D12Descriptor::TypeUAV: return "UAV";
    case D3D12Descriptor::TypeRTV: return "RTV";
    case D3D12Descriptor::TypeDSV: return "DSV";
    case D3D12Descriptor::TypeUndefined: return "Undefined";
    default: break;
  }

  if((uint32_t)el < D3D12Descriptor::TypeCBV)
    return "Sampler";

  return StringFormat::Fmt("DescriptorType<%d>", el);
}

string ToStrHelper<false, D3D12ResourceType>::Get(const D3D12ResourceType &el)
{
  switch(el)
  {
    case Resource_Device: return "Device";
    case Resource_Unknown: return "Unknown";
    case Resource_CommandAllocator: return "Command Allocator";
    case Resource_CommandQueue: return "Command Queue";
    case Resource_CommandSignature: return "Command Signature";
    case Resource_DescriptorHeap: return "Descriptor Heap";
    case Resource_Fence: return "Fence";
    case Resource_Heap: return "Heap";
    case Resource_PipelineState: return "Pipeline State";
    case Resource_QueryHeap: return "Query Heap";
    case Resource_Resource: return "Resource";
    case Resource_GraphicsCommandList: return "Graphics CommandList";
    case Resource_RootSignature: return "Root Signature";
    default: break;
  }

  return StringFormat::Fmt("D3D12ResourceType<%d>", el);
}

string ToStrHelper<false, D3D12_HEAP_TYPE>::Get(const D3D12_HEAP_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_HEAP_TYPE_DEFAULT)
    TOSTR_CASE_STRINGIZE(D3D12_HEAP_TYPE_UPLOAD)
    TOSTR_CASE_STRINGIZE(D3D12_HEAP_TYPE_READBACK)
    TOSTR_CASE_STRINGIZE(D3D12_HEAP_TYPE_CUSTOM)
    default: break;
  }

  return StringFormat::Fmt("D3D12_HEAP_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_CPU_PAGE_PROPERTY>::Get(const D3D12_CPU_PAGE_PROPERTY &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_CPU_PAGE_PROPERTY_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE)
    TOSTR_CASE_STRINGIZE(D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE)
    TOSTR_CASE_STRINGIZE(D3D12_CPU_PAGE_PROPERTY_WRITE_BACK)
    default: break;
  }

  return StringFormat::Fmt("D3D12_CPU_PAGE_PROPERTY<%d>", el);
}

string ToStrHelper<false, D3D12_MEMORY_POOL>::Get(const D3D12_MEMORY_POOL &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_MEMORY_POOL_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_MEMORY_POOL_L0)
    TOSTR_CASE_STRINGIZE(D3D12_MEMORY_POOL_L1)
    default: break;
  }

  return StringFormat::Fmt("D3D12_MEMORY_POOL<%d>", el);
}

string ToStrHelper<false, D3D12_QUERY_HEAP_TYPE>::Get(const D3D12_QUERY_HEAP_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_HEAP_TYPE_OCCLUSION)
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_HEAP_TYPE_TIMESTAMP)
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_HEAP_TYPE_PIPELINE_STATISTICS)
    TOSTR_CASE_STRINGIZE(D3D12_QUERY_HEAP_TYPE_SO_STATISTICS)
    default: break;
  }

  return StringFormat::Fmt("D3D12_QUERY_HEAP_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_DESCRIPTOR_HEAP_TYPE>::Get(const D3D12_DESCRIPTOR_HEAP_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
    TOSTR_CASE_STRINGIZE(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
    TOSTR_CASE_STRINGIZE(D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
    TOSTR_CASE_STRINGIZE(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
    default: break;
  }

  return StringFormat::Fmt("D3D12_DESCRIPTOR_HEAP_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_RESOURCE_BARRIER_TYPE>::Get(const D3D12_RESOURCE_BARRIER_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_BARRIER_TYPE_TRANSITION)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_BARRIER_TYPE_ALIASING)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_BARRIER_TYPE_UAV)
    default: break;
  }

  return StringFormat::Fmt("D3D12_RESOURCE_BARRIER_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_SRV_DIMENSION>::Get(const D3D12_SRV_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_BUFFER)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE1DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE2DMS)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURE3D)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURECUBE)
    TOSTR_CASE_STRINGIZE(D3D12_SRV_DIMENSION_TEXTURECUBEARRAY)
    default: break;
  }

  return StringFormat::Fmt("D3D12_SRV_DIMENSION<%d>", el);
}

string ToStrHelper<false, D3D12_RTV_DIMENSION>::Get(const D3D12_RTV_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_BUFFER)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE1DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE2DMS)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_RTV_DIMENSION_TEXTURE3D)
    default: break;
  }

  return StringFormat::Fmt("D3D12_RTV_DIMENSION<%d>", el);
}

string ToStrHelper<false, D3D12_UAV_DIMENSION>::Get(const D3D12_UAV_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_UAV_DIMENSION_BUFFER)
    TOSTR_CASE_STRINGIZE(D3D12_UAV_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D12_UAV_DIMENSION_TEXTURE1DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_UAV_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D12_UAV_DIMENSION_TEXTURE2DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_UAV_DIMENSION_TEXTURE3D)
    default: break;
  }

  return StringFormat::Fmt("D3D12_UAV_DIMENSION<%d>", el);
}

string ToStrHelper<false, D3D12_DSV_DIMENSION>::Get(const D3D12_DSV_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_TEXTURE1DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_TEXTURE2DARRAY)
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_TEXTURE2DMS)
    TOSTR_CASE_STRINGIZE(D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY)
    default: break;
  }

  return StringFormat::Fmt("D3D12_DSV_DIMENSION<%d>", el);
}

string ToStrHelper<false, D3D12_FILTER>::Get(const D3D12_FILTER &el)
{
  switch(el)
  {
    case D3D12_FILTER_MIN_MAG_MIP_POINT: return "MIN_MAG_MIP_POINT";
    case D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR: return "MIN_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT: return "MIN_POINT_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR: return "MIN_POINT_MAG_MIP_LINEAR";
    case D3D12_FILTER_MIN_LINEAR_MAG_MIP_POINT: return "MIN_LINEAR_MAG_MIP_POINT";
    case D3D12_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR: return "MIN_LINEAR_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT: return "MIN_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_MIN_MAG_MIP_LINEAR: return "MIN_MAG_MIP_LINEAR";
    case D3D12_FILTER_ANISOTROPIC: return "ANISOTROPIC";

    case D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT: return "CMP:MIN_MAG_MIP_POINT";
    case D3D12_FILTER_COMPARISON_MIN_MAG_POINT_MIP_LINEAR: return "CMP:MIN_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_COMPARISON_MIN_POINT_MAG_LINEAR_MIP_POINT:
      return "CMP:MIN_POINT_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_COMPARISON_MIN_POINT_MAG_MIP_LINEAR: return "CMP:MIN_POINT_MAG_MIP_LINEAR";
    case D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT: return "CMP:MIN_LINEAR_MAG_MIP_POINT";
    case D3D12_FILTER_COMPARISON_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
      return "CMP:MIN_LINEAR_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT: return "CMP:MIN_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR: return "CMP:MIN_MAG_MIP_LINEAR";
    case D3D12_FILTER_COMPARISON_ANISOTROPIC: return "CMP:ANISOTROPIC";

    case D3D12_FILTER_MINIMUM_MIN_MAG_MIP_POINT: return "MIN:MIN_MAG_MIP_POINT";
    case D3D12_FILTER_MINIMUM_MIN_MAG_POINT_MIP_LINEAR: return "MIN:MIN_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_MINIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
      return "MIN:MIN_POINT_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_MINIMUM_MIN_POINT_MAG_MIP_LINEAR: return "MIN:MIN_POINT_MAG_MIP_LINEAR";
    case D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_MIP_POINT: return "MIN:MIN_LINEAR_MAG_MIP_POINT";
    case D3D12_FILTER_MINIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
      return "MIN:MIN_LINEAR_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_MINIMUM_MIN_MAG_LINEAR_MIP_POINT: return "MIN:MIN_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_MINIMUM_MIN_MAG_MIP_LINEAR: return "MIN:MIN_MAG_MIP_LINEAR";
    case D3D12_FILTER_MINIMUM_ANISOTROPIC: return "MIN:ANISOTROPIC";

    case D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_POINT: return "MAX:MIN_MAG_MIP_POINT";
    case D3D12_FILTER_MAXIMUM_MIN_MAG_POINT_MIP_LINEAR: return "MAX:MIN_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_LINEAR_MIP_POINT:
      return "MAX:MIN_POINT_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_MAXIMUM_MIN_POINT_MAG_MIP_LINEAR: return "MAX:MIN_POINT_MAG_MIP_LINEAR";
    case D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_MIP_POINT: return "MAX:MIN_LINEAR_MAG_MIP_POINT";
    case D3D12_FILTER_MAXIMUM_MIN_LINEAR_MAG_POINT_MIP_LINEAR:
      return "MAX:MIN_LINEAR_MAG_POINT_MIP_LINEAR";
    case D3D12_FILTER_MAXIMUM_MIN_MAG_LINEAR_MIP_POINT: return "MAX:MIN_MAG_LINEAR_MIP_POINT";
    case D3D12_FILTER_MAXIMUM_MIN_MAG_MIP_LINEAR: return "MAX:MIN_MAG_MIP_LINEAR";
    case D3D12_FILTER_MAXIMUM_ANISOTROPIC: return "MAX:ANISOTROPIC";

    default: break;
  }

  return StringFormat::Fmt("D3D12_FILTER<%d>", el);
}

string ToStrHelper<false, D3D12_TEXTURE_ADDRESS_MODE>::Get(const D3D12_TEXTURE_ADDRESS_MODE &el)
{
  switch(el)
  {
    case D3D12_TEXTURE_ADDRESS_MODE_WRAP: return "WRAP";
    case D3D12_TEXTURE_ADDRESS_MODE_MIRROR: return "MIRROR";
    case D3D12_TEXTURE_ADDRESS_MODE_CLAMP: return "CLAMP";
    case D3D12_TEXTURE_ADDRESS_MODE_BORDER: return "BORDER";
    case D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE: return "MIRROR_ONCE";
    default: break;
  }

  return StringFormat::Fmt("D3D12_TEXTURE_ADDRESS_MODE<%d>", el);
}

string ToStrHelper<false, D3D12_BLEND>::Get(const D3D12_BLEND &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_ZERO)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_ONE)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_SRC_COLOR)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_INV_SRC_COLOR)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_SRC_ALPHA)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_INV_SRC_ALPHA)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_DEST_ALPHA)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_INV_DEST_ALPHA)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_DEST_COLOR)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_INV_DEST_COLOR)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_SRC_ALPHA_SAT)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_BLEND_FACTOR)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_INV_BLEND_FACTOR)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_SRC1_COLOR)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_INV_SRC1_COLOR)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_SRC1_ALPHA)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_INV_SRC1_ALPHA)
    default: break;
  }

  return StringFormat::Fmt("D3D12_BLEND<%d>", el);
}

string ToStrHelper<false, D3D12_BLEND_OP>::Get(const D3D12_BLEND_OP &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_OP_ADD)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_OP_SUBTRACT)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_OP_REV_SUBTRACT)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_OP_MIN)
    TOSTR_CASE_STRINGIZE(D3D12_BLEND_OP_MAX)
    default: break;
  }

  return StringFormat::Fmt("D3D12_BLEND_OP<%d>", el);
}

string ToStrHelper<false, D3D12_LOGIC_OP>::Get(const D3D12_LOGIC_OP &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_CLEAR)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_SET)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_COPY)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_COPY_INVERTED)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_NOOP)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_INVERT)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_AND)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_NAND)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_OR)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_NOR)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_XOR)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_EQUIV)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_AND_REVERSE)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_AND_INVERTED)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_OR_REVERSE)
    TOSTR_CASE_STRINGIZE(D3D12_LOGIC_OP_OR_INVERTED)
    default: break;
  }

  return StringFormat::Fmt("D3D12_LOGIC_OP<%d>", el);
}

string ToStrHelper<false, D3D12_FILL_MODE>::Get(const D3D12_FILL_MODE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_FILL_MODE_WIREFRAME)
    TOSTR_CASE_STRINGIZE(D3D12_FILL_MODE_SOLID)
    default: break;
  }

  return StringFormat::Fmt("D3D12_FILL_MODE<%d>", el);
}

string ToStrHelper<false, D3D12_CULL_MODE>::Get(const D3D12_CULL_MODE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_CULL_MODE_NONE)
    TOSTR_CASE_STRINGIZE(D3D12_CULL_MODE_FRONT)
    TOSTR_CASE_STRINGIZE(D3D12_CULL_MODE_BACK)
    default: break;
  }

  return StringFormat::Fmt("D3D12_CULL_MODE<%d>", el);
}

string ToStrHelper<false, D3D12_CONSERVATIVE_RASTERIZATION_MODE>::Get(
    const D3D12_CONSERVATIVE_RASTERIZATION_MODE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF)
    TOSTR_CASE_STRINGIZE(D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON)
    default: break;
  }

  return StringFormat::Fmt("D3D12_CONSERVATIVE_RASTERIZATION_MODE<%d>", el);
}

string ToStrHelper<false, D3D12_COMPARISON_FUNC>::Get(const D3D12_COMPARISON_FUNC &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_COMPARISON_FUNC_NEVER)
    TOSTR_CASE_STRINGIZE(D3D12_COMPARISON_FUNC_LESS)
    TOSTR_CASE_STRINGIZE(D3D12_COMPARISON_FUNC_EQUAL)
    TOSTR_CASE_STRINGIZE(D3D12_COMPARISON_FUNC_LESS_EQUAL)
    TOSTR_CASE_STRINGIZE(D3D12_COMPARISON_FUNC_GREATER)
    TOSTR_CASE_STRINGIZE(D3D12_COMPARISON_FUNC_NOT_EQUAL)
    TOSTR_CASE_STRINGIZE(D3D12_COMPARISON_FUNC_GREATER_EQUAL)
    TOSTR_CASE_STRINGIZE(D3D12_COMPARISON_FUNC_ALWAYS)
    default: break;
  }

  return StringFormat::Fmt("D3D12_COMPARISON_FUNC<%d>", el);
}

string ToStrHelper<false, D3D12_DEPTH_WRITE_MASK>::Get(const D3D12_DEPTH_WRITE_MASK &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_DEPTH_WRITE_MASK_ZERO)
    TOSTR_CASE_STRINGIZE(D3D12_DEPTH_WRITE_MASK_ALL)
    default: break;
  }

  return StringFormat::Fmt("D3D12_DEPTH_WRITE_MASK<%d>", el);
}

string ToStrHelper<false, D3D12_STENCIL_OP>::Get(const D3D12_STENCIL_OP &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_STENCIL_OP_KEEP)
    TOSTR_CASE_STRINGIZE(D3D12_STENCIL_OP_ZERO)
    TOSTR_CASE_STRINGIZE(D3D12_STENCIL_OP_REPLACE)
    TOSTR_CASE_STRINGIZE(D3D12_STENCIL_OP_INCR_SAT)
    TOSTR_CASE_STRINGIZE(D3D12_STENCIL_OP_DECR_SAT)
    TOSTR_CASE_STRINGIZE(D3D12_STENCIL_OP_INVERT)
    TOSTR_CASE_STRINGIZE(D3D12_STENCIL_OP_INCR)
    TOSTR_CASE_STRINGIZE(D3D12_STENCIL_OP_DECR)
    default: break;
  }

  return StringFormat::Fmt("D3D12_STENCIL_OP<%d>", el);
}

string ToStrHelper<false, D3D12_INPUT_CLASSIFICATION>::Get(const D3D12_INPUT_CLASSIFICATION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA)
    TOSTR_CASE_STRINGIZE(D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA)
    default: break;
  }

  return StringFormat::Fmt("D3D12_INPUT_CLASSIFICATION<%d>", el);
}

string ToStrHelper<false, D3D12_INDEX_BUFFER_STRIP_CUT_VALUE>::Get(
    const D3D12_INDEX_BUFFER_STRIP_CUT_VALUE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED)
    TOSTR_CASE_STRINGIZE(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF)
    TOSTR_CASE_STRINGIZE(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF)
    default: break;
  }

  return StringFormat::Fmt("D3D12_INDEX_BUFFER_STRIP_CUT_VALUE<%d>", el);
}

string ToStrHelper<false, D3D12_PRIMITIVE_TOPOLOGY_TYPE>::Get(const D3D12_PRIMITIVE_TOPOLOGY_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED)
    TOSTR_CASE_STRINGIZE(D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT)
    TOSTR_CASE_STRINGIZE(D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE)
    TOSTR_CASE_STRINGIZE(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE)
    TOSTR_CASE_STRINGIZE(D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH)
    default: break;
  }

  return StringFormat::Fmt("D3D12_PRIMITIVE_TOPOLOGY_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_INDIRECT_ARGUMENT_TYPE>::Get(const D3D12_INDIRECT_ARGUMENT_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_DRAW)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_SHADER_RESOURCE_VIEW)
    TOSTR_CASE_STRINGIZE(D3D12_INDIRECT_ARGUMENT_TYPE_UNORDERED_ACCESS_VIEW)
    default: break;
  }

  return StringFormat::Fmt("D3D12_INDIRECT_ARGUMENT_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_COMMAND_LIST_TYPE>::Get(const D3D12_COMMAND_LIST_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_COMMAND_LIST_TYPE_DIRECT)
    TOSTR_CASE_STRINGIZE(D3D12_COMMAND_LIST_TYPE_BUNDLE)
    TOSTR_CASE_STRINGIZE(D3D12_COMMAND_LIST_TYPE_COMPUTE)
    TOSTR_CASE_STRINGIZE(D3D12_COMMAND_LIST_TYPE_COPY)
    default: break;
  }

  return StringFormat::Fmt("D3D12_COMMAND_LIST_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_TEXTURE_COPY_TYPE>::Get(const D3D12_TEXTURE_COPY_TYPE &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX)
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT)
    default: break;
  }

  return StringFormat::Fmt("D3D12_TEXTURE_COPY_TYPE<%d>", el);
}

string ToStrHelper<false, D3D12_RESOURCE_DIMENSION>::Get(const D3D12_RESOURCE_DIMENSION &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_BUFFER)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_TEXTURE1D)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    TOSTR_CASE_STRINGIZE(D3D12_RESOURCE_DIMENSION_TEXTURE3D)
    default: break;
  }

  return StringFormat::Fmt("D3D12_RESOURCE_DIMENSION<%d>", el);
}

string ToStrHelper<false, D3D12_TEXTURE_LAYOUT>::Get(const D3D12_TEXTURE_LAYOUT &el)
{
  switch(el)
  {
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_LAYOUT_UNKNOWN)
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_LAYOUT_ROW_MAJOR)
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE)
    TOSTR_CASE_STRINGIZE(D3D12_TEXTURE_LAYOUT_64KB_STANDARD_SWIZZLE)
    default: break;
  }

  return StringFormat::Fmt("D3D12_TEXTURE_LAYOUT<%d>", el);
}

string ToStrHelper<false, D3D12_CLEAR_FLAGS>::Get(const D3D12_CLEAR_FLAGS &el)
{
  string ret;

  if(el & D3D12_CLEAR_FLAG_DEPTH)
    ret += " | D3D12_CLEAR_FLAG_DEPTH";

  if(el & D3D12_CLEAR_FLAG_STENCIL)
    ret += " | D3D12_CLEAR_FLAG_STENCIL";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_BUFFER_SRV_FLAGS>::Get(const D3D12_BUFFER_SRV_FLAGS &el)
{
  string ret;

  if(el == D3D12_BUFFER_SRV_FLAG_NONE)
    return "D3D12_BUFFER_SRV_FLAG_NONE";

  if(el & D3D12_BUFFER_SRV_FLAG_RAW)
    ret += " | D3D12_BUFFER_SRV_FLAG_RAW";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_DSV_FLAGS>::Get(const D3D12_DSV_FLAGS &el)
{
  string ret;

  if(el == D3D12_DSV_FLAG_NONE)
    return "D3D12_DSV_FLAG_NONE";

  if(el & D3D12_DSV_FLAG_READ_ONLY_DEPTH)
    ret += " | D3D12_DSV_FLAG_READ_ONLY_DEPTH";
  if(el & D3D12_DSV_FLAG_READ_ONLY_STENCIL)
    ret += " | D3D12_DSV_FLAG_READ_ONLY_STENCIL";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_BUFFER_UAV_FLAGS>::Get(const D3D12_BUFFER_UAV_FLAGS &el)
{
  string ret;

  if(el == D3D12_BUFFER_UAV_FLAG_NONE)
    return "D3D12_BUFFER_UAV_FLAG_NONE";

  if(el & D3D12_BUFFER_UAV_FLAG_RAW)
    ret += " | D3D12_BUFFER_UAV_FLAG_RAW";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_HEAP_FLAGS>::Get(const D3D12_HEAP_FLAGS &el)
{
  string ret;

  if(el == D3D12_HEAP_FLAG_NONE)
    return "D3D12_HEAP_FLAG_NONE";

  if(el & D3D12_HEAP_FLAG_SHARED)
    ret += " | D3D12_HEAP_FLAG_SHARED";
  if(el & D3D12_HEAP_FLAG_DENY_BUFFERS)
    ret += " | D3D12_HEAP_FLAG_DENY_BUFFERS";
  if(el & D3D12_HEAP_FLAG_ALLOW_DISPLAY)
    ret += " | D3D12_HEAP_FLAG_ALLOW_DISPLAY";
  if(el & D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER)
    ret += " | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER";
  if(el & D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES)
    ret += " | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES";
  if(el & D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES)
    ret += " | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES";
  if(el & D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES)
    ret += " | D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES";
  if(el & D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS)
    ret += " | D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS";
  if(el & D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES)
    ret += " | D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES";
  if(el & D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES)
    ret += " | D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_FENCE_FLAGS>::Get(const D3D12_FENCE_FLAGS &el)
{
  string ret;

  if(el == D3D12_FENCE_FLAG_NONE)
    return "D3D12_FENCE_FLAG_NONE";

  if(el & D3D12_FENCE_FLAG_SHARED)
    ret += " | D3D12_FENCE_FLAG_SHARED";
  if(el & D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER)
    ret += " | D3D12_FENCE_FLAG_SHARED_CROSS_ADAPTER";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_DESCRIPTOR_HEAP_FLAGS>::Get(const D3D12_DESCRIPTOR_HEAP_FLAGS &el)
{
  string ret;

  if(el == D3D12_DESCRIPTOR_HEAP_FLAG_NONE)
    return "D3D12_DESCRIPTOR_HEAP_FLAG_NONE";

  if(el & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
    ret += " | D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_RESOURCE_BARRIER_FLAGS>::Get(const D3D12_RESOURCE_BARRIER_FLAGS &el)
{
  string ret;

  if(el == D3D12_RESOURCE_BARRIER_FLAG_NONE)
    return "D3D12_RESOURCE_BARRIER_FLAG_NONE";

  if(el & D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY)
    ret += " | D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY";
  if(el & D3D12_RESOURCE_BARRIER_FLAG_END_ONLY)
    ret += " | D3D12_RESOURCE_BARRIER_FLAG_END_ONLY";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_RESOURCE_STATES>::Get(const D3D12_RESOURCE_STATES &el)
{
  string ret;

  if(el == D3D12_RESOURCE_STATE_COMMON)
    return "COMMON/PRESENT";

  if(el == D3D12_RESOURCE_STATE_GENERIC_READ)
    return "GENERIC_READ";

  if(el & D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER)
    ret += " | VB & CB";
  if(el & D3D12_RESOURCE_STATE_INDEX_BUFFER)
    ret += " | IB";
  if(el & D3D12_RESOURCE_STATE_RENDER_TARGET)
    ret += " | RTV";
  if(el & D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    ret += " | UAV";
  if(el & D3D12_RESOURCE_STATE_DEPTH_WRITE)
    ret += " | DSV Write";
  if(el & D3D12_RESOURCE_STATE_DEPTH_READ)
    ret += " | DSV Read";
  if(el & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
    ret += " | SRV (Non-Pixel)";
  if(el & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
    ret += " | SRV (Pixel)";
  if(el & D3D12_RESOURCE_STATE_STREAM_OUT)
    ret += " | Stream Out";
  if(el & D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT)
    ret += " | Indirect";
  if(el & D3D12_RESOURCE_STATE_COPY_DEST)
    ret += " | Copy (Dst)";
  if(el & D3D12_RESOURCE_STATE_COPY_SOURCE)
    ret += " | Copy (Src)";
  if(el & D3D12_RESOURCE_STATE_RESOLVE_DEST)
    ret += " | Resolve (Dst)";
  if(el & D3D12_RESOURCE_STATE_RESOLVE_SOURCE)
    ret += " | Resolve (Src)";
  if(el & D3D12_RESOURCE_STATE_PREDICATION)
    ret += " | Predication";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_PIPELINE_STATE_FLAGS>::Get(const D3D12_PIPELINE_STATE_FLAGS &el)
{
  string ret;

  if(el == D3D12_PIPELINE_STATE_FLAG_NONE)
    return "D3D12_PIPELINE_STATE_FLAG_NONE";

  if(el & D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG)
    ret += " | D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_RESOURCE_FLAGS>::Get(const D3D12_RESOURCE_FLAGS &el)
{
  string ret;

  if(el == D3D12_RESOURCE_FLAG_NONE)
    return "D3D12_RESOURCE_FLAG_NONE";

  if(el & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET";
  if(el & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL";
  if(el & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS";
  if(el & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE)
    ret += " | D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE";
  if(el & D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER";
  if(el & D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS)
    ret += " | D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}

string ToStrHelper<false, D3D12_COMMAND_QUEUE_FLAGS>::Get(const D3D12_COMMAND_QUEUE_FLAGS &el)
{
  string ret;

  if(el == D3D12_COMMAND_QUEUE_FLAG_NONE)
    return "D3D12_COMMAND_QUEUE_FLAG_NONE";

  if(el & D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT)
    ret += " | D3D12_COMMAND_QUEUE_FLAG_DISABLE_GPU_TIMEOUT";

  if(!ret.empty())
    ret = ret.substr(3);

  return ret;
}
