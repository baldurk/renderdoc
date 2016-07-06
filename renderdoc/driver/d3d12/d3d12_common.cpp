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

// we know the object will be a non-dispatchable object type
#define SerialiseObject(type, name, obj)                              \
  {                                                                   \
    D3D12ResourceManager *rm = (D3D12ResourceManager *)GetUserData(); \
    ResourceId id;                                                    \
    if(m_Mode >= WRITING)                                             \
      id = GetResID(obj);                                             \
    Serialise(name, id);                                              \
    if(m_Mode < WRITING)                                              \
      obj = (id == ResourceId() || !rm->HasLiveResource(id))          \
                ? NULL                                                \
                : Unwrap((type *)rm->GetLiveResource(id));            \
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
  SerialiseBuffer("pShaderBytecode", (byte *&)el.pShaderBytecode, sz);
}

template <>
void Serialiser::Deserialise(const D3D12_SHADER_BYTECODE *const el) const
{
  if(m_Mode == READING)
    delete[](byte *)(el->pShaderBytecode);
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
    ScopedContext targetscope(this, name, "D3D11_RENDER_TARGET_BLEND_DESC", 0, true);

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
void Serialiser::Serialise(const char *name, D3D12_VERTEX_BUFFER_VIEW &el)
{
  ScopedContext scope(this, name, "D3D12_VERTEX_BUFFER_VIEW", 0, true);

  // TODO serialise gpu virtual address as heap ID and idx
  Serialise("SizeInBytes", el.SizeInBytes);
  Serialise("StrideInBytes", el.StrideInBytes);
}

template <>
void Serialiser::Serialise(const char *name, D3D12_INDEX_BUFFER_VIEW &el)
{
  ScopedContext scope(this, name, "D3D12_INDEX_BUFFER_VIEW", 0, true);

  // TODO serialise gpu virtual address as heap ID and idx
  Serialise("SizeInBytes", el.SizeInBytes);
  Serialise("Format", el.Format);
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
      Serialise("Transition.pResource", el.Transition.pResource);
      Serialise("Transition.Subresource", el.Transition.Subresource);
      Serialise("Transition.StateBefore", el.Transition.StateBefore);
      Serialise("Transition.StateAfter", el.Transition.StateAfter);
      break;
    case D3D12_RESOURCE_BARRIER_TYPE_ALIASING:
      Serialise("Aliasing.pResourceBefore", el.Aliasing.pResourceBefore);
      Serialise("Aliasing.pResourceAfter", el.Aliasing.pResourceAfter);
      break;
    case D3D12_RESOURCE_BARRIER_TYPE_UAV: Serialise("UAV.pResource", el.UAV.pResource); break;
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
void Serialiser::Serialise(const char *name, D3D12_DESCRIPTOR_HEAP_DESC &el)
{
  ScopedContext scope(this, name, "D3D12_DESCRIPTOR_HEAP_DESC", 0, true);

  Serialise("Type", el.Type);
  Serialise("NumDescriptors", el.NumDescriptors);
  Serialise("Flags", el.Flags);
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

string ToStrHelper<false, D3D12_VIEWPORT>::Get(const D3D12_VIEWPORT &el)
{
  return StringFormat::Fmt("Viewport<%.0fx%.0f+%.0f+%.0f z=%f->%f>", el.Width, el.Height,
                           el.TopLeftX, el.TopLeftY, el.MinDepth, el.MaxDepth);
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
