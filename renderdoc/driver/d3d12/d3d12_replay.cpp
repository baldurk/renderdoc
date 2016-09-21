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

#include "d3d12_replay.h"
#include "driver/dxgi/dxgi_common.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"

D3D12Replay::D3D12Replay()
{
  m_pDevice = NULL;
  m_Proxy = false;
}

void D3D12Replay::Shutdown()
{
  for(size_t i = 0; i < m_ProxyResources.size(); i++)
    m_ProxyResources[i]->Release();
  m_ProxyResources.clear();

  m_pDevice->Release();
}

void D3D12Replay::ReadLogInitialisation()
{
  m_pDevice->ReadLogInitialisation();
}

void D3D12Replay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  m_pDevice->ReplayLog(0, endEventID, replayType);
}

vector<ResourceId> D3D12Replay::GetBuffers()
{
  vector<ResourceId> ret;

  for(auto it = WrappedID3D12Resource::m_List.begin(); it != WrappedID3D12Resource::m_List.end(); it++)
    if(it->second->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
      ret.push_back(it->first);

  return ret;
}

vector<ResourceId> D3D12Replay::GetTextures()
{
  vector<ResourceId> ret;

  for(auto it = WrappedID3D12Resource::m_List.begin(); it != WrappedID3D12Resource::m_List.end(); it++)
  {
    if(it->second->GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER &&
       m_pDevice->GetResourceManager()->GetOriginalID(it->first) != it->first)
      ret.push_back(it->first);
  }

  return ret;
}

FetchBuffer D3D12Replay::GetBuffer(ResourceId id)
{
  FetchBuffer ret;
  ret.ID = m_pDevice->GetResourceManager()->GetOriginalID(id);

  auto it = WrappedID3D12Resource::m_List.find(id);

  if(it == WrappedID3D12Resource::m_List.end())
    return ret;

  D3D12_RESOURCE_DESC desc = it->second->GetDesc();

  ret.customName = true;
  string str = m_pDevice->GetResourceName(ret.ID);

  if(str == "")
  {
    ret.customName = false;
    str = StringFormat::Fmt("Buffer %llu", ret.ID);
  }

  ret.name = str;

  ret.length = desc.Width;

  D3D12NOTIMP("Buffer creation flags from implicit usage");

  ret.creationFlags = eBufferCreate_VB | eBufferCreate_IB | eBufferCreate_CB | eBufferCreate_Indirect;
  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    ret.creationFlags |= eBufferCreate_UAV;

  return ret;
}

FetchTexture D3D12Replay::GetTexture(ResourceId id)
{
  FetchTexture ret;
  ret.ID = m_pDevice->GetResourceManager()->GetOriginalID(id);

  auto it = WrappedID3D12Resource::m_List.find(id);

  if(it == WrappedID3D12Resource::m_List.end())
    return ret;

  D3D12_RESOURCE_DESC desc = it->second->GetDesc();

  ret.format = MakeResourceFormat(desc.Format);
  ret.dimension = desc.Dimension - D3D12_RESOURCE_DIMENSION_BUFFER;

  ret.width = (uint32_t)desc.Width;
  ret.height = desc.Height;
  ret.depth = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? desc.DepthOrArraySize : 1;
  ret.arraysize = desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D ? desc.DepthOrArraySize : 1;
  ret.numSubresources = GetNumSubresources(&desc);
  ret.mips = desc.MipLevels;
  ret.msQual = desc.SampleDesc.Quality;
  ret.msSamp = desc.SampleDesc.Count;
  ret.byteSize = 0;
  for(uint32_t i = 0; i < ret.mips; i++)
    ret.byteSize += GetByteSize(ret.width, ret.height, ret.depth, desc.Format, i);

  switch(ret.dimension)
  {
    case 1: ret.resType = ret.arraysize > 1 ? eResType_Texture1DArray : eResType_Texture1D; break;
    case 2:
      if(ret.msSamp > 1)
        ret.resType = ret.arraysize > 1 ? eResType_Texture2DMSArray : eResType_Texture2DMS;
      else
        ret.resType = ret.arraysize > 1 ? eResType_Texture2DArray : eResType_Texture2D;
      break;
    case 3: ret.resType = eResType_Texture3D; break;
  }

  D3D12NOTIMP("Texture cubemap-ness from implicit usage");
  ret.cubemap = false;    // eResType_TextureCube, eResType_TextureCubeArray

  ret.creationFlags = eTextureCreate_SRV;

  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    ret.creationFlags |= eTextureCreate_RTV;
  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    ret.creationFlags |= eTextureCreate_DSV;
  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    ret.creationFlags |= eTextureCreate_UAV;

  if(ret.ID == m_pDevice->GetQueue()->GetBackbufferResourceID())
  {
    ret.format = MakeResourceFormat(GetTypedFormat(desc.Format, eCompType_UNorm));
    ret.creationFlags |= eTextureCreate_SwapBuffer;
  }

  ret.customName = true;
  string str = m_pDevice->GetResourceName(ret.ID);

  if(str == "")
  {
    const char *suffix = "";
    const char *ms = "";

    if(ret.msSamp > 1)
      ms = "MS";

    if(ret.creationFlags & eTextureCreate_RTV)
      suffix = " RTV";
    if(ret.creationFlags & eTextureCreate_DSV)
      suffix = " DSV";

    ret.customName = false;

    if(ret.arraysize > 1)
      str = StringFormat::Fmt("Texture%uD%sArray%s %llu", ret.dimension, ms, suffix, ret.ID);
    else
      str = StringFormat::Fmt("Texture%uD%s%s %llu", ret.dimension, ms, suffix, ret.ID);
  }

  ret.name = str;

  return ret;
}

ShaderReflection *D3D12Replay::GetShader(ResourceId shader, string entryPoint)
{
  WrappedID3D12PipelineState::ShaderEntry *sh =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12PipelineState::ShaderEntry>(shader);

  if(sh)
    return &sh->GetDetails();

  return NULL;
}

void D3D12Replay::FreeTargetResource(ResourceId id)
{
  if(m_pDevice->GetResourceManager()->HasLiveResource(id))
  {
    ID3D12DeviceChild *resource = m_pDevice->GetResourceManager()->GetLiveResource(id);

    SAFE_RELEASE(resource);
  }
}

void D3D12Replay::FreeCustomShader(ResourceId id)
{
  if(m_pDevice->GetResourceManager()->HasLiveResource(id))
  {
    ID3D12DeviceChild *resource = m_pDevice->GetResourceManager()->GetLiveResource(id);

    SAFE_RELEASE(resource);
  }
}

FetchFrameRecord D3D12Replay::GetFrameRecord()
{
  return m_pDevice->GetFrameRecord();
}

ResourceId D3D12Replay::GetLiveID(ResourceId id)
{
  return m_pDevice->GetResourceManager()->GetLiveID(id);
}

vector<EventUsage> D3D12Replay::GetUsage(ResourceId id)
{
  return m_pDevice->GetQueue()->GetUsage(id);
}

void D3D12Replay::FillResourceView(D3D12PipelineState::ResourceView &view,
                                   const PortableHandle &resHandle)
{
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  if(resHandle.heap == ResourceId())
    return;

  WrappedID3D12DescriptorHeap *heap = rm->GetLiveAs<WrappedID3D12DescriptorHeap>(resHandle.heap);
  D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();

  D3D12Descriptor *desc = (D3D12Descriptor *)handle.ptr;
  desc += resHandle.index;

  if(desc->GetType() == D3D12Descriptor::TypeSampler || desc->GetType() == D3D12Descriptor::TypeCBV)
  {
    RDCERR("Invalid descriptors - expected a resource view");
    return;
  }

  view.Resource = rm->GetOriginalID(GetResID(desc->nonsamp.resource));

  if(view.Resource == ResourceId())
    return;

  D3D12_RESOURCE_DESC res = desc->nonsamp.resource->GetDesc();

  {
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;

    if(desc->GetType() == D3D12Descriptor::TypeRTV)
      fmt = desc->nonsamp.rtv.Format;
    else if(desc->GetType() == D3D12Descriptor::TypeSRV)
      fmt = desc->nonsamp.srv.Format;
    else if(desc->GetType() == D3D12Descriptor::TypeUAV)
      fmt = (DXGI_FORMAT)desc->nonsamp.uav.desc.Format;

    if(fmt == DXGI_FORMAT_UNKNOWN)
      fmt = res.Format;

    view.ElementSize = fmt == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, fmt, 0);

    view.Format = MakeResourceFormat(fmt);

    if(desc->GetType() == D3D12Descriptor::TypeRTV)
    {
      view.Type = ToStr::Get(desc->nonsamp.rtv.ViewDimension);

      if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_BUFFER)
      {
        view.FirstElement = desc->nonsamp.rtv.Buffer.FirstElement;
        view.NumElements = desc->nonsamp.rtv.Buffer.NumElements;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE1D)
      {
        view.HighestMip = desc->nonsamp.rtv.Texture1D.MipSlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE1DARRAY)
      {
        view.ArraySize = desc->nonsamp.rtv.Texture1DArray.ArraySize;
        view.FirstArraySlice = desc->nonsamp.rtv.Texture1DArray.FirstArraySlice;
        view.HighestMip = desc->nonsamp.rtv.Texture1DArray.MipSlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2D)
      {
        view.HighestMip = desc->nonsamp.rtv.Texture2D.MipSlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
      {
        view.ArraySize = desc->nonsamp.rtv.Texture2DArray.ArraySize;
        view.FirstArraySlice = desc->nonsamp.rtv.Texture2DArray.FirstArraySlice;
        view.HighestMip = desc->nonsamp.rtv.Texture2DArray.MipSlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DMS)
      {
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY)
      {
        view.ArraySize = desc->nonsamp.rtv.Texture2DMSArray.ArraySize;
        view.FirstArraySlice = desc->nonsamp.rtv.Texture2DArray.FirstArraySlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE3D)
      {
        view.ArraySize = desc->nonsamp.rtv.Texture3D.WSize;
        view.FirstArraySlice = desc->nonsamp.rtv.Texture3D.FirstWSlice;
        view.HighestMip = desc->nonsamp.rtv.Texture3D.MipSlice;
      }
    }
    else if(desc->GetType() == D3D12Descriptor::TypeDSV)
    {
      view.Type = ToStr::Get(desc->nonsamp.dsv.ViewDimension);

      if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE1D)
      {
        view.HighestMip = desc->nonsamp.dsv.Texture1D.MipSlice;
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE1DARRAY)
      {
        view.ArraySize = desc->nonsamp.dsv.Texture1DArray.ArraySize;
        view.FirstArraySlice = desc->nonsamp.dsv.Texture1DArray.FirstArraySlice;
        view.HighestMip = desc->nonsamp.dsv.Texture1DArray.MipSlice;
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2D)
      {
        view.HighestMip = desc->nonsamp.dsv.Texture2D.MipSlice;
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DARRAY)
      {
        view.ArraySize = desc->nonsamp.dsv.Texture2DArray.ArraySize;
        view.FirstArraySlice = desc->nonsamp.dsv.Texture2DArray.FirstArraySlice;
        view.HighestMip = desc->nonsamp.dsv.Texture2DArray.MipSlice;
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DMS)
      {
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY)
      {
        view.ArraySize = desc->nonsamp.dsv.Texture2DMSArray.ArraySize;
        view.FirstArraySlice = desc->nonsamp.dsv.Texture2DArray.FirstArraySlice;
      }
    }
    else if(desc->GetType() == D3D12Descriptor::TypeSRV)
    {
      view.Type = ToStr::Get(desc->nonsamp.srv.ViewDimension);

      view.swizzle[0] = (TextureSwizzle)D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(
          0, desc->nonsamp.srv.Shader4ComponentMapping);
      view.swizzle[1] = (TextureSwizzle)D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(
          1, desc->nonsamp.srv.Shader4ComponentMapping);
      view.swizzle[2] = (TextureSwizzle)D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(
          2, desc->nonsamp.srv.Shader4ComponentMapping);
      view.swizzle[3] = (TextureSwizzle)D3D12_DECODE_SHADER_4_COMPONENT_MAPPING(
          3, desc->nonsamp.srv.Shader4ComponentMapping);

      if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
      {
        view.FirstElement = desc->nonsamp.srv.Buffer.FirstElement;
        view.NumElements = desc->nonsamp.srv.Buffer.NumElements;

        view.BufferFlags = desc->nonsamp.srv.Buffer.Flags;
        view.ElementSize = desc->nonsamp.srv.Buffer.StructureByteStride;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE1D)
      {
        view.HighestMip = desc->nonsamp.srv.Texture1D.MostDetailedMip;
        view.NumMipLevels = desc->nonsamp.srv.Texture1D.MipLevels;
        view.MinLODClamp = desc->nonsamp.srv.Texture1D.ResourceMinLODClamp;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE1DARRAY)
      {
        view.ArraySize = desc->nonsamp.srv.Texture1DArray.ArraySize;
        view.FirstArraySlice = desc->nonsamp.srv.Texture1DArray.FirstArraySlice;
        view.HighestMip = desc->nonsamp.srv.Texture1DArray.MostDetailedMip;
        view.NumMipLevels = desc->nonsamp.srv.Texture1DArray.MipLevels;
        view.MinLODClamp = desc->nonsamp.srv.Texture1DArray.ResourceMinLODClamp;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D)
      {
        view.HighestMip = desc->nonsamp.srv.Texture2D.MostDetailedMip;
        view.NumMipLevels = desc->nonsamp.srv.Texture2D.MipLevels;
        view.MinLODClamp = desc->nonsamp.srv.Texture2D.ResourceMinLODClamp;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
      {
        view.ArraySize = desc->nonsamp.srv.Texture2DArray.ArraySize;
        view.FirstArraySlice = desc->nonsamp.srv.Texture2DArray.FirstArraySlice;
        view.HighestMip = desc->nonsamp.srv.Texture2DArray.MostDetailedMip;
        view.NumMipLevels = desc->nonsamp.srv.Texture2DArray.MipLevels;
        view.MinLODClamp = desc->nonsamp.srv.Texture2DArray.ResourceMinLODClamp;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMS)
      {
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY)
      {
        view.ArraySize = desc->nonsamp.srv.Texture2DMSArray.ArraySize;
        view.FirstArraySlice = desc->nonsamp.srv.Texture2DMSArray.FirstArraySlice;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE3D)
      {
        view.HighestMip = desc->nonsamp.srv.Texture3D.MostDetailedMip;
        view.NumMipLevels = desc->nonsamp.srv.Texture3D.MipLevels;
        view.MinLODClamp = desc->nonsamp.srv.Texture3D.ResourceMinLODClamp;
      }
    }
    else if(desc->GetType() == D3D12Descriptor::TypeUAV)
    {
      D3D12_UNORDERED_ACCESS_VIEW_DESC uav = desc->nonsamp.uav.desc.AsDesc();

      view.CounterResource = rm->GetOriginalID(GetResID(desc->nonsamp.uav.counterResource));

      view.Type = ToStr::Get(uav.ViewDimension);

      if(uav.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
      {
        view.FirstElement = uav.Buffer.FirstElement;
        view.NumElements = uav.Buffer.NumElements;

        view.BufferFlags = uav.Buffer.Flags;
        view.ElementSize = uav.Buffer.StructureByteStride;

        view.CounterByteOffset = uav.Buffer.CounterOffsetInBytes;
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE1D)
      {
        view.HighestMip = uav.Texture1D.MipSlice;
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE1DARRAY)
      {
        view.ArraySize = uav.Texture1DArray.ArraySize;
        view.FirstArraySlice = uav.Texture1DArray.FirstArraySlice;
        view.HighestMip = uav.Texture1DArray.MipSlice;
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2D)
      {
        view.HighestMip = uav.Texture2D.MipSlice;
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2DARRAY)
      {
        view.ArraySize = uav.Texture2DArray.ArraySize;
        view.FirstArraySlice = uav.Texture2DArray.FirstArraySlice;
        view.HighestMip = uav.Texture2DArray.MipSlice;
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE3D)
      {
        view.ArraySize = uav.Texture3D.WSize;
        view.FirstArraySlice = uav.Texture3D.FirstWSlice;
        view.HighestMip = uav.Texture3D.MipSlice;
      }
    }
  }
}

void D3D12Replay::MakePipelineState()
{
  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  D3D12PipelineState &state = m_PipelineState;

  /////////////////////////////////////////////////
  // Input Assembler
  /////////////////////////////////////////////////

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  state.pipeline = rm->GetOriginalID(rs.pipe);

  state.customName = true;
  string str = m_pDevice->GetResourceName(rs.pipe);

  WrappedID3D12PipelineState *pipe = NULL;

  if(rs.pipe != ResourceId())
    pipe = rm->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  if(str == "")
  {
    state.customName = false;

    if(pipe)
      str = StringFormat::Fmt(pipe->IsGraphics() ? "Graphics Pipe %llu" : "Compute Pipe %llu",
                              state.pipeline);
    else
      str = "Unbound";
  }

  state.PipelineName = str;

  if(pipe && pipe->IsGraphics())
  {
    const D3D12_INPUT_ELEMENT_DESC *inputEl = pipe->graphics->InputLayout.pInputElementDescs;
    UINT numInput = pipe->graphics->InputLayout.NumElements;

    create_array_uninit(state.m_IA.layouts, numInput);
    for(UINT i = 0; i < numInput; i++)
    {
      D3D12PipelineState::InputAssembler::LayoutInput &l = state.m_IA.layouts[i];

      l.ByteOffset = inputEl[i].AlignedByteOffset;
      l.Format = MakeResourceFormat(inputEl[i].Format);
      l.InputSlot = inputEl[i].InputSlot;
      l.PerInstance = inputEl[i].InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
      l.InstanceDataStepRate = inputEl[i].InstanceDataStepRate;
      l.SemanticIndex = inputEl[i].SemanticIndex;
      l.SemanticName = inputEl[i].SemanticName;
    }

    state.m_IA.indexStripCutValue = 0;
    switch(pipe->graphics->IBStripCutValue)
    {
      case D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF: state.m_IA.indexStripCutValue = 0xFFFF; break;
      case D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF:
        state.m_IA.indexStripCutValue = 0xFFFFFFFF;
        break;
      default: break;
    }

    create_array_uninit(state.m_IA.vbuffers, rs.vbuffers.size());
    for(size_t i = 0; i < rs.vbuffers.size(); i++)
    {
      D3D12PipelineState::InputAssembler::VertexBuffer &vb = state.m_IA.vbuffers[i];

      vb.Buffer = rm->GetOriginalID(rs.vbuffers[i].buf);
      vb.Offset = rs.vbuffers[i].offs;
      vb.Size = rs.vbuffers[i].size;
      vb.Stride = rs.vbuffers[i].stride;
    }

    state.m_IA.ibuffer.Buffer = rm->GetOriginalID(rs.ibuffer.buf);
    state.m_IA.ibuffer.Offset = rs.ibuffer.offs;
    state.m_IA.ibuffer.Size = rs.ibuffer.size;
  }

  /////////////////////////////////////////////////
  // Shaders
  /////////////////////////////////////////////////

  ResourceId rootSig;

  if(pipe && pipe->IsCompute())
  {
    WrappedID3D12PipelineState::ShaderEntry *sh =
        (WrappedID3D12PipelineState::ShaderEntry *)pipe->compute->CS.pShaderBytecode;

    state.m_CS.Shader = sh->GetResourceID();
    state.m_CS.stage = eShaderStage_Compute;

    rootSig = rs.compute.rootsig;
  }
  else if(pipe)
  {
    D3D12PipelineState::ShaderStage *dstArr[] = {&state.m_VS, &state.m_HS, &state.m_DS, &state.m_GS,
                                                 &state.m_PS};

    D3D12_SHADER_BYTECODE *srcArr[] = {&pipe->graphics->VS, &pipe->graphics->HS, &pipe->graphics->DS,
                                       &pipe->graphics->GS, &pipe->graphics->PS};

    for(size_t stage = 0; stage < 5; stage++)
    {
      D3D12PipelineState::ShaderStage &dst = *dstArr[stage];
      D3D12_SHADER_BYTECODE &src = *srcArr[stage];

      dst.stage = (ShaderStageType)stage;

      WrappedID3D12PipelineState::ShaderEntry *sh =
          (WrappedID3D12PipelineState::ShaderEntry *)src.pShaderBytecode;

      if(sh)
      {
        dst.Shader = sh->GetResourceID();
        dst.BindpointMapping = sh->GetMapping();
      }
    }

    rootSig = rs.graphics.rootsig;
  }

#if 0
  {
    D3D11PipelineState::ShaderStage &dst = *dstArr[stage];
    const D3D11RenderState::shader &src = *srcArr[stage];

    dst.stage = (ShaderStageType)stage;

    ResourceId id = GetIDForResource(src.Shader);

    WrappedShader *shad = (WrappedShader *)(WrappedID3D11Shader<ID3D11VertexShader> *)src.Shader;

    ShaderReflection *refl = NULL;

    if(shad != NULL)
      refl = shad->GetDetails();

    dst.Shader = rm->GetOriginalID(id);
    dst.ShaderDetails = NULL;

    string str = GetDebugName(src.Shader);
    dst.customName = true;

    if(str == "" && dst.Shader != ResourceId())
    {
      dst.customName = false;
      str = StringFormat::Fmt("%s Shader %llu", stageNames[stage], dst.Shader);
    }

    dst.ShaderName = str;

    // create identity bindpoint mapping
    create_array_uninit(dst.BindpointMapping.InputAttributes,
                        D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT);
    for(int s = 0; s < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; s++)
    {
      // TODO: this should do any semantic rematching as defined by the bytecode
      // the input layout was built with (not necessarily the vertex shader's bytecode -
      // in the case of a mismatch). It's commonly, but not always the identity mapping
      dst.BindpointMapping.InputAttributes[s] = s;
    }

    create_array_uninit(dst.BindpointMapping.ConstantBlocks,
                        D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
    for(int s = 0; s < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; s++)
    {
      dst.BindpointMapping.ConstantBlocks[s].bindset = 0;
      dst.BindpointMapping.ConstantBlocks[s].bind = s;
      dst.BindpointMapping.ConstantBlocks[s].used = false;
      dst.BindpointMapping.ConstantBlocks[s].arraySize = 1;
    }

    create_array_uninit(dst.BindpointMapping.ReadOnlyResources,
                        D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
    for(int32_t s = 0; s < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; s++)
    {
      dst.BindpointMapping.ReadOnlyResources[s].bindset = 0;
      dst.BindpointMapping.ReadOnlyResources[s].bind = s;
      dst.BindpointMapping.ReadOnlyResources[s].used = false;
      dst.BindpointMapping.ReadOnlyResources[s].arraySize = 1;
    }

    create_array_uninit(dst.BindpointMapping.ReadWriteResources, D3D11_1_UAV_SLOT_COUNT);
    for(int32_t s = 0; s < D3D11_1_UAV_SLOT_COUNT; s++)
    {
      dst.BindpointMapping.ReadWriteResources[s].bindset = 0;
      dst.BindpointMapping.ReadWriteResources[s].bind = s;
      dst.BindpointMapping.ReadWriteResources[s].used = false;
      dst.BindpointMapping.ReadWriteResources[s].arraySize = 1;
    }

    // mark resources as used if they are referenced by the shader
    if(refl)
    {
      for(int32_t i = 0; i < refl->ConstantBlocks.count; i++)
        if(refl->ConstantBlocks[i].bufferBacked)
          dst.BindpointMapping.ConstantBlocks[refl->ConstantBlocks[i].bindPoint].used = true;

      for(int32_t i = 0; i < refl->ReadOnlyResources.count; i++)
        if(!refl->ReadOnlyResources[i].IsSampler)
          dst.BindpointMapping.ReadOnlyResources[refl->ReadOnlyResources[i].bindPoint].used = true;

      for(int32_t i = 0; i < refl->ReadWriteResources.count; i++)
        dst.BindpointMapping.ReadWriteResources[refl->ReadWriteResources[i].bindPoint].used = true;
    }

    create_array_uninit(dst.ConstantBuffers, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
    for(size_t s = 0; s < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; s++)
    {
      dst.ConstantBuffers[s].Buffer = rm->GetOriginalID(GetIDForResource(src.ConstantBuffers[s]));
      dst.ConstantBuffers[s].VecOffset = src.CBOffsets[s];
      dst.ConstantBuffers[s].VecCount = src.CBCounts[s];
    }

    create_array_uninit(dst.Samplers, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT);
    for(size_t s = 0; s < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; s++)
    {
      D3D11PipelineState::ShaderStage::Sampler &samp = dst.Samplers[s];

      samp.Samp = rm->GetOriginalID(GetIDForResource(src.Samplers[s]));

      if(samp.Samp != ResourceId())
      {
        samp.SamplerName = GetDebugName(src.Samplers[s]);
        samp.customSamplerName = true;

        if(samp.SamplerName.count == 0)
        {
          samp.customSamplerName = false;
          samp.SamplerName = StringFormat::Fmt("Sampler %llu", samp.Samp);
        }

        D3D11_SAMPLER_DESC desc;
        src.Samplers[s]->GetDesc(&desc);

        samp.AddressU = ToStr::Get(desc.AddressU);
        samp.AddressV = ToStr::Get(desc.AddressV);
        samp.AddressW = ToStr::Get(desc.AddressW);

        memcpy(samp.BorderColor, desc.BorderColor, sizeof(FLOAT) * 4);

        samp.Comparison = ToStr::Get(desc.ComparisonFunc);
        samp.Filter = ToStr::Get(desc.Filter);
        samp.MaxAniso = 0;
        if(desc.Filter == D3D11_FILTER_ANISOTROPIC ||
           desc.Filter == D3D11_FILTER_COMPARISON_ANISOTROPIC)
          samp.MaxAniso = desc.MaxAnisotropy;
        samp.MaxLOD = desc.MaxLOD;
        samp.MinLOD = desc.MinLOD;
        samp.MipLODBias = desc.MipLODBias;
        samp.UseComparison = (desc.Filter >= D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT);
        samp.UseBorder = (desc.AddressU == D3D11_TEXTURE_ADDRESS_BORDER ||
                          desc.AddressV == D3D11_TEXTURE_ADDRESS_BORDER ||
                          desc.AddressW == D3D11_TEXTURE_ADDRESS_BORDER);
      }
    }

    create_array_uninit(dst.SRVs, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
    for(size_t s = 0; s < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; s++)
    {
      D3D11PipelineState::ShaderStage::ResourceView &view = dst.SRVs[s];

      view.View = rm->GetOriginalID(GetIDForResource(src.SRVs[s]));

      if(view.View != ResourceId())
      {
        D3D11_SHADER_RESOURCE_VIEW_DESC desc;
        src.SRVs[s]->GetDesc(&desc);

        view.Format = MakeResourceFormat(desc.Format);

        ID3D11Resource *res = NULL;
        src.SRVs[s]->GetResource(&res);

        view.Structured = false;
        view.BufferStructCount = 0;

        view.ElementSize =
            desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

        view.Resource = rm->GetOriginalID(GetIDForResource(res));

        view.Type = ToStr::Get(desc.ViewDimension);

        if(desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER)
        {
          view.FirstElement = desc.Buffer.FirstElement;
          view.NumElements = desc.Buffer.NumElements;
          view.ElementOffset = desc.Buffer.ElementOffset;
          view.ElementWidth = desc.Buffer.ElementWidth;

          D3D11_BUFFER_DESC bufdesc;
          ((ID3D11Buffer *)res)->GetDesc(&bufdesc);

          view.Structured = bufdesc.StructureByteStride > 0 && desc.Format == DXGI_FORMAT_UNKNOWN;

          if(view.Structured)
            view.ElementSize = bufdesc.StructureByteStride;
        }
        else if(desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX)
        {
          view.FirstElement = desc.BufferEx.FirstElement;
          view.NumElements = desc.BufferEx.NumElements;
          view.Flags = desc.BufferEx.Flags;
        }
        else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1D)
        {
          view.HighestMip = desc.Texture1D.MostDetailedMip;
          view.NumMipLevels = desc.Texture1D.MipLevels;
        }
        else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1DARRAY)
        {
          view.ArraySize = desc.Texture1DArray.ArraySize;
          view.FirstArraySlice = desc.Texture1DArray.FirstArraySlice;
          view.HighestMip = desc.Texture1DArray.MostDetailedMip;
          view.NumMipLevels = desc.Texture1DArray.MipLevels;
        }
        else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
        {
          view.HighestMip = desc.Texture2D.MostDetailedMip;
          view.NumMipLevels = desc.Texture2D.MipLevels;
        }
        else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
        {
          view.ArraySize = desc.Texture2DArray.ArraySize;
          view.FirstArraySlice = desc.Texture2DArray.FirstArraySlice;
          view.HighestMip = desc.Texture2DArray.MostDetailedMip;
          view.NumMipLevels = desc.Texture2DArray.MipLevels;
        }
        else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMS)
        {
        }
        else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY)
        {
          view.ArraySize = desc.Texture2DArray.ArraySize;
          view.FirstArraySlice = desc.Texture2DArray.FirstArraySlice;
        }
        else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE3D)
        {
          view.HighestMip = desc.Texture3D.MostDetailedMip;
          view.NumMipLevels = desc.Texture3D.MipLevels;
        }
        else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBE)
        {
          view.ArraySize = 6;
          view.HighestMip = desc.TextureCube.MostDetailedMip;
          view.NumMipLevels = desc.TextureCube.MipLevels;
        }
        else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBEARRAY)
        {
          view.ArraySize = desc.TextureCubeArray.NumCubes * 6;
          view.FirstArraySlice = desc.TextureCubeArray.First2DArrayFace;
          view.HighestMip = desc.TextureCubeArray.MostDetailedMip;
          view.NumMipLevels = desc.TextureCubeArray.MipLevels;
        }

        SAFE_RELEASE(res);
      }
    }

    create_array(dst.UAVs, D3D11_1_UAV_SLOT_COUNT);
    for(size_t s = 0; dst.stage == eShaderStage_Compute && s < D3D11_1_UAV_SLOT_COUNT; s++)
    {
      D3D11PipelineState::ShaderStage::ResourceView &view = dst.UAVs[s];

      view.View = rm->GetOriginalID(GetIDForResource(rs.CSUAVs[s]));

      if(view.View != ResourceId())
      {
        D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
        rs.CSUAVs[s]->GetDesc(&desc);

        ID3D11Resource *res = NULL;
        rs.CSUAVs[s]->GetResource(&res);

        view.Structured = false;
        view.BufferStructCount = 0;

        view.ElementSize =
            desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

        if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
           (desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER)))
        {
          view.BufferStructCount = m_pDevice->GetDebugManager()->GetStructCount(rs.CSUAVs[s]);
        }

        view.Resource = rm->GetOriginalID(GetIDForResource(res));

        view.Format = MakeResourceFormat(desc.Format);
        view.Type = ToStr::Get(desc.ViewDimension);

        if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
        {
          view.FirstElement = desc.Buffer.FirstElement;
          view.NumElements = desc.Buffer.NumElements;
          view.Flags = desc.Buffer.Flags;

          D3D11_BUFFER_DESC bufdesc;
          ((ID3D11Buffer *)res)->GetDesc(&bufdesc);

          view.Structured = bufdesc.StructureByteStride > 0 && desc.Format == DXGI_FORMAT_UNKNOWN;

          if(view.Structured)
            view.ElementSize = bufdesc.StructureByteStride;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1D)
        {
          view.HighestMip = desc.Texture1D.MipSlice;
          view.NumMipLevels = 1;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
        {
          view.ArraySize = desc.Texture1DArray.ArraySize;
          view.FirstArraySlice = desc.Texture1DArray.FirstArraySlice;
          view.HighestMip = desc.Texture1DArray.MipSlice;
          view.NumMipLevels = 1;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D)
        {
          view.HighestMip = desc.Texture2D.MipSlice;
          view.NumMipLevels = 1;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
        {
          view.ArraySize = desc.Texture2DArray.ArraySize;
          view.FirstArraySlice = desc.Texture2DArray.FirstArraySlice;
          view.HighestMip = desc.Texture2DArray.MipSlice;
          view.NumMipLevels = 1;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE3D)
        {
          view.ArraySize = desc.Texture3D.WSize;
          view.FirstArraySlice = desc.Texture3D.FirstWSlice;
          view.HighestMip = desc.Texture3D.MipSlice;
          view.NumMipLevels = 1;
        }

        SAFE_RELEASE(res);
      }
    }
  }
#endif

  if(pipe && pipe->IsGraphics())
  {
    /////////////////////////////////////////////////
    // Stream Out
    /////////////////////////////////////////////////

    create_array_uninit(state.m_SO.Outputs, rs.streamouts.size());
    for(size_t s = 0; s < rs.streamouts.size(); s++)
    {
      state.m_SO.Outputs[s].Buffer = rm->GetOriginalID(rs.streamouts[s].buf);
      state.m_SO.Outputs[s].Offset = rs.streamouts[s].offs;
      state.m_SO.Outputs[s].Size = rs.streamouts[s].size;

      state.m_SO.Outputs[s].WrittenCountBuffer = rm->GetOriginalID(rs.streamouts[s].countbuf);
      state.m_SO.Outputs[s].WrittenCountOffset = rs.streamouts[s].countoffs;
    }

    /////////////////////////////////////////////////
    // Rasterizer
    /////////////////////////////////////////////////

    state.m_RS.SampleMask = pipe->graphics->SampleMask;

    {
      D3D12PipelineState::Rasterizer::RasterizerState &dst = state.m_RS.m_State;
      D3D12_RASTERIZER_DESC &src = pipe->graphics->RasterizerState;

      dst.AntialiasedLineEnable = src.AntialiasedLineEnable == TRUE;

      dst.CullMode = eCull_None;
      if(src.CullMode == D3D12_CULL_MODE_FRONT)
        dst.CullMode = eCull_Front;
      if(src.CullMode == D3D12_CULL_MODE_BACK)
        dst.CullMode = eCull_Back;

      dst.FillMode = eFill_Solid;
      if(src.FillMode == D3D12_FILL_MODE_WIREFRAME)
        dst.FillMode = eFill_Wireframe;

      dst.DepthBias = src.DepthBias;
      dst.DepthBiasClamp = src.DepthBiasClamp;
      dst.DepthClip = src.DepthClipEnable == TRUE;
      dst.FrontCCW = src.FrontCounterClockwise == TRUE;
      dst.MultisampleEnable = src.MultisampleEnable == TRUE;
      dst.SlopeScaledDepthBias = src.SlopeScaledDepthBias;
      dst.ForcedSampleCount = src.ForcedSampleCount;
      dst.ConservativeRasterization =
          src.ConservativeRaster == D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
    }

    size_t i = 0;
    create_array_uninit(state.m_RS.Scissors,
                        D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for(i = 0; i < rs.scissors.size(); i++)
      state.m_RS.Scissors[i] = D3D12PipelineState::Rasterizer::Scissor(
          rs.scissors[i].left, rs.scissors[i].top, rs.scissors[i].right, rs.scissors[i].bottom);

    for(; i < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; i++)
      state.m_RS.Scissors[i] = D3D12PipelineState::Rasterizer::Scissor(0, 0, 0, 0);

    create_array_uninit(state.m_RS.Viewports,
                        D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for(i = 0; i < rs.views.size(); i++)
      state.m_RS.Viewports[i] = D3D12PipelineState::Rasterizer::Viewport(
          rs.views[i].TopLeftX, rs.views[i].TopLeftY, rs.views[i].Width, rs.views[i].Height,
          rs.views[i].MinDepth, rs.views[i].MaxDepth);

    for(; i < D3D12_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; i++)
      state.m_RS.Viewports[i] = D3D12PipelineState::Rasterizer::Viewport(0, 0, 0, 0, 0, 0);

    /////////////////////////////////////////////////
    // Output Merger
    /////////////////////////////////////////////////

    create_array(state.m_OM.RenderTargets, rs.rts.size());
    for(size_t i = 0; i < rs.rts.size(); i++)
    {
      D3D12PipelineState::ResourceView &view = state.m_OM.RenderTargets[i];

      PortableHandle h = rs.rtSingle ? rs.rts[i] : rs.rts[i];

      if(rs.rtSingle)
        h.index += (uint32_t)i;

      FillResourceView(view, h);
    }

    {
      D3D12PipelineState::ResourceView &view = state.m_OM.DepthTarget;

      FillResourceView(view, rs.dsv);
    }

    memcpy(state.m_OM.m_BlendState.BlendFactor, rs.blendFactor, sizeof(FLOAT) * 4);

    {
      D3D12_BLEND_DESC &src = pipe->graphics->BlendState;

      state.m_OM.m_BlendState.AlphaToCoverage = src.AlphaToCoverageEnable == TRUE;
      state.m_OM.m_BlendState.IndependentBlend = src.IndependentBlendEnable == TRUE;

      create_array_uninit(state.m_OM.m_BlendState.Blends, 8);
      for(size_t i = 0; i < 8; i++)
      {
        D3D12PipelineState::OutputMerger::BlendState::RTBlend &blend =
            state.m_OM.m_BlendState.Blends[i];

        blend.Enabled = src.RenderTarget[i].BlendEnable == TRUE;

        blend.LogicEnabled = src.RenderTarget[i].LogicOpEnable == TRUE;
        blend.LogicOp = ToStr::Get(src.RenderTarget[i].LogicOp);

        blend.m_AlphaBlend.Source = ToStr::Get(src.RenderTarget[i].SrcBlendAlpha);
        blend.m_AlphaBlend.Destination = ToStr::Get(src.RenderTarget[i].DestBlendAlpha);
        blend.m_AlphaBlend.Operation = ToStr::Get(src.RenderTarget[i].BlendOpAlpha);

        blend.m_Blend.Source = ToStr::Get(src.RenderTarget[i].SrcBlend);
        blend.m_Blend.Destination = ToStr::Get(src.RenderTarget[i].DestBlend);
        blend.m_Blend.Operation = ToStr::Get(src.RenderTarget[i].BlendOp);

        blend.WriteMask = src.RenderTarget[i].RenderTargetWriteMask;
      }
    }

    {
      D3D12_DEPTH_STENCIL_DESC &src = pipe->graphics->DepthStencilState;

      state.m_OM.m_State.DepthEnable = src.DepthEnable == TRUE;
      state.m_OM.m_State.DepthFunc = ToStr::Get(src.DepthFunc);
      state.m_OM.m_State.DepthWrites = src.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ALL;
      state.m_OM.m_State.StencilEnable = src.StencilEnable == TRUE;
      state.m_OM.m_State.StencilRef = rs.stencilRef;
      state.m_OM.m_State.StencilReadMask = src.StencilReadMask;
      state.m_OM.m_State.StencilWriteMask = src.StencilWriteMask;

      state.m_OM.m_State.m_FrontFace.Func = ToStr::Get(src.FrontFace.StencilFunc);
      state.m_OM.m_State.m_FrontFace.DepthFailOp = ToStr::Get(src.FrontFace.StencilDepthFailOp);
      state.m_OM.m_State.m_FrontFace.PassOp = ToStr::Get(src.FrontFace.StencilPassOp);
      state.m_OM.m_State.m_FrontFace.FailOp = ToStr::Get(src.FrontFace.StencilFailOp);

      state.m_OM.m_State.m_BackFace.Func = ToStr::Get(src.BackFace.StencilFunc);
      state.m_OM.m_State.m_BackFace.DepthFailOp = ToStr::Get(src.BackFace.StencilDepthFailOp);
      state.m_OM.m_State.m_BackFace.PassOp = ToStr::Get(src.BackFace.StencilPassOp);
      state.m_OM.m_State.m_BackFace.FailOp = ToStr::Get(src.BackFace.StencilFailOp);
    }
  }
}

void D3D12Replay::RenderCheckerboard(Vec3f light, Vec3f dark)
{
  m_pDevice->GetDebugManager()->RenderCheckerboard(light, dark);
}

bool D3D12Replay::RenderTexture(TextureDisplay cfg)
{
  return m_pDevice->GetDebugManager()->RenderTexture(cfg, true);
}

void D3D12Replay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                            uint32_t mip, uint32_t sample, FormatComponentType typeHint,
                            float pixel[4])
{
  m_pDevice->GetDebugManager()->PickPixel(texture, x, y, sliceFace, mip, sample, typeHint, pixel);
}

uint64_t D3D12Replay::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
{
  return m_pDevice->GetDebugManager()->MakeOutputWindow(system, data, depth);
}

void D3D12Replay::DestroyOutputWindow(uint64_t id)
{
  m_pDevice->GetDebugManager()->DestroyOutputWindow(id);
}

bool D3D12Replay::CheckResizeOutputWindow(uint64_t id)
{
  return m_pDevice->GetDebugManager()->CheckResizeOutputWindow(id);
}

void D3D12Replay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  m_pDevice->GetDebugManager()->GetOutputWindowDimensions(id, w, h);
}

void D3D12Replay::ClearOutputWindowColour(uint64_t id, float col[4])
{
  m_pDevice->GetDebugManager()->ClearOutputWindowColour(id, col);
}

void D3D12Replay::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
  m_pDevice->GetDebugManager()->ClearOutputWindowDepth(id, depth, stencil);
}

void D3D12Replay::BindOutputWindow(uint64_t id, bool depth)
{
  m_pDevice->GetDebugManager()->BindOutputWindow(id, depth);
}

bool D3D12Replay::IsOutputWindowVisible(uint64_t id)
{
  return m_pDevice->GetDebugManager()->IsOutputWindowVisible(id);
}

void D3D12Replay::FlipOutputWindow(uint64_t id)
{
  m_pDevice->GetDebugManager()->FlipOutputWindow(id);
}

void D3D12Replay::ReplaceResource(ResourceId from, ResourceId to)
{
  m_pDevice->GetResourceManager()->ReplaceResource(from, to);
}

void D3D12Replay::RemoveReplacement(ResourceId id)
{
  m_pDevice->GetResourceManager()->RemoveReplacement(id);
}

void D3D12Replay::InitCallstackResolver()
{
  m_pDevice->GetSerialiser()->InitCallstackResolver();
}

bool D3D12Replay::HasCallstacks()
{
  return m_pDevice->GetSerialiser()->HasCallstacks();
}

Callstack::StackResolver *D3D12Replay::GetCallstackResolver()
{
  return m_pDevice->GetSerialiser()->GetCallstackResolver();
}

#pragma region not yet implemented

APIProperties D3D12Replay::GetAPIProperties()
{
  APIProperties ret;

  ret.pipelineType = eGraphicsAPI_D3D12;
  ret.localRenderer = eGraphicsAPI_D3D12;
  ret.degraded = false;

  return ret;
}

vector<DebugMessage> D3D12Replay::GetDebugMessages()
{
  return vector<DebugMessage>();
}

vector<uint32_t> D3D12Replay::GetPassEvents(uint32_t eventID)
{
  vector<uint32_t> passEvents;

  return passEvents;
}

void D3D12Replay::InitPostVSBuffers(uint32_t eventID)
{
}

void D3D12Replay::InitPostVSBuffers(const vector<uint32_t> &passEvents)
{
}

bool D3D12Replay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                            FormatComponentType typeHint, float *minval, float *maxval)
{
  *minval = 0.0f;
  *maxval = 1.0f;
  return false;
}

bool D3D12Replay::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                               FormatComponentType typeHint, float minval, float maxval,
                               bool channels[4], vector<uint32_t> &histogram)
{
  histogram.resize(256, 0);
  return false;
}

MeshFormat D3D12Replay::GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
  return MeshFormat();
}

void D3D12Replay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &retData)
{
}

byte *D3D12Replay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip, bool forDiskSave,
                                  FormatComponentType typeHint, bool resolve, bool forceRGBA8unorm,
                                  float blackPoint, float whitePoint, size_t &dataSize)
{
  dataSize = 0;
  return NULL;
}

vector<uint32_t> D3D12Replay::EnumerateCounters()
{
  return vector<uint32_t>();
}

void D3D12Replay::DescribeCounter(uint32_t counterID, CounterDescription &desc)
{
  desc = CounterDescription();
}

vector<CounterResult> D3D12Replay::FetchCounters(const vector<uint32_t> &counters)
{
  return vector<CounterResult>();
}

void D3D12Replay::RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws,
                             const MeshDisplay &cfg)
{
}

void D3D12Replay::BuildTargetShader(string source, string entry, const uint32_t compileFlags,
                                    ShaderStageType type, ResourceId *id, string *errors)
{
}

void D3D12Replay::BuildCustomShader(string source, string entry, const uint32_t compileFlags,
                                    ShaderStageType type, ResourceId *id, string *errors)
{
}

void D3D12Replay::RenderHighlightBox(float w, float h, float scale)
{
}

void D3D12Replay::FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                                       vector<ShaderVariable> &outvars, const vector<byte> &data)
{
  return;
}

vector<PixelModification> D3D12Replay::PixelHistory(vector<EventUsage> events, ResourceId target,
                                                    uint32_t x, uint32_t y, uint32_t slice,
                                                    uint32_t mip, uint32_t sampleIdx,
                                                    FormatComponentType typeHint)
{
  return vector<PixelModification>();
}

ShaderDebugTrace D3D12Replay::DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid,
                                          uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
  return ShaderDebugTrace();
}

ShaderDebugTrace D3D12Replay::DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                                         uint32_t primitive)
{
  return ShaderDebugTrace();
}

ShaderDebugTrace D3D12Replay::DebugThread(uint32_t eventID, uint32_t groupid[3], uint32_t threadid[3])
{
  return ShaderDebugTrace();
}

uint32_t D3D12Replay::PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y)
{
  return ~0U;
}

ResourceId D3D12Replay::RenderOverlay(ResourceId texid, FormatComponentType typeHint,
                                      TextureDisplayOverlay overlay, uint32_t eventID,
                                      const vector<uint32_t> &passEvents)
{
  return ResourceId();
}

ResourceId D3D12Replay::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip,
                                          uint32_t arrayIdx, uint32_t sampleIdx,
                                          FormatComponentType typeHint)
{
  return ResourceId();
}

bool D3D12Replay::IsRenderOutput(ResourceId id)
{
  return false;
}

ResourceId D3D12Replay::CreateProxyTexture(const FetchTexture &templateTex)
{
  return ResourceId();
}

void D3D12Replay::SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                                      size_t dataSize)
{
}

ResourceId D3D12Replay::CreateProxyBuffer(const FetchBuffer &templateBuf)
{
  return ResourceId();
}

void D3D12Replay::SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
{
}

#pragma endregion

extern "C" __declspec(dllexport) HRESULT
    __cdecl RENDERDOC_CreateWrappedD3D12Device(IUnknown *pAdapter,
                                               D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid,
                                               void **ppDevice);
ID3DDevice *GetD3D12DeviceIfAlloc(IUnknown *dev);

ReplayCreateStatus D3D12_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
  RDCDEBUG("Creating a D3D12 replay device");

  WrappedIDXGISwapChain3::RegisterD3DDeviceCallback(GetD3D12DeviceIfAlloc);

  HMODULE lib = NULL;
  lib = LoadLibraryA("d3d12.dll");
  if(lib == NULL)
  {
    RDCERR("Failed to load d3d12.dll");
    return eReplayCreate_APIInitFailed;
  }

  lib = LoadLibraryA("dxgi.dll");
  if(lib == NULL)
  {
    RDCERR("Failed to load dxgi.dll");
    return eReplayCreate_APIInitFailed;
  }

  if(GetD3DCompiler() == NULL)
  {
    RDCERR("Failed to load d3dcompiler_??.dll");
    return eReplayCreate_APIInitFailed;
  }

  D3D12InitParams initParams;
  RDCDriver driverFileType = RDC_D3D12;
  string driverName = "D3D12";
  uint64_t machineIdent = 0;
  if(logfile)
  {
    auto status = RenderDoc::Inst().FillInitParams(logfile, driverFileType, driverName,
                                                   machineIdent, (RDCInitParams *)&initParams);
    if(status != eReplayCreate_Success)
      return status;
  }

  // initParams.SerialiseVersion is guaranteed to be valid/supported since otherwise the
  // FillInitParams (which calls D3D12InitParams::Serialise) would have failed above, so no need to
  // check it here.

  if(initParams.MinimumFeatureLevel < D3D_FEATURE_LEVEL_11_0)
    initParams.MinimumFeatureLevel = D3D_FEATURE_LEVEL_11_0;

  ID3D12Device *dev = NULL;
  HRESULT hr = RENDERDOC_CreateWrappedD3D12Device(NULL, initParams.MinimumFeatureLevel,
                                                  __uuidof(ID3D12Device), (void **)&dev);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create a d3d12 device :(.");

    return eReplayCreate_APIHardwareUnsupported;
  }

  WrappedID3D12Device *wrappedDev = (WrappedID3D12Device *)dev;
  if(logfile)
    wrappedDev->SetLogFile(logfile);
  wrappedDev->SetLogVersion(initParams.SerialiseVersion);

  RDCLOG("Created device.");
  D3D12Replay *replay = wrappedDev->GetReplay();

  replay->SetProxy(logfile == NULL);

  *driver = (IReplayDriver *)replay;
  return eReplayCreate_Success;
}

static DriverRegistration D3D12DriverRegistration(RDC_D3D12, "D3D12", &D3D12_CreateReplayDevice);
