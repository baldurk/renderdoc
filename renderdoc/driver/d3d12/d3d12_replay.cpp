/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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
#include "driver/dx/official/d3dcompiler.h"
#include "driver/dxgi/dxgi_common.h"
#include "driver/ihv/amd/amd_isa.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"

template <class T>
T &resize_and_add(std::vector<T> &vec, size_t idx)
{
  if(idx >= vec.size())
    vec.resize(idx + 1);

  return vec[idx];
}

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

  PreDeviceShutdownCounters();

  m_pDevice->Release();

  D3D12Replay::PostDeviceShutdownCounters();
}

void D3D12Replay::ReadLogInitialisation()
{
  m_pDevice->ReadLogInitialisation();
}

APIProperties D3D12Replay::GetAPIProperties()
{
  APIProperties ret;

  ret.pipelineType = GraphicsAPI::D3D12;
  ret.localRenderer = GraphicsAPI::D3D12;
  ret.degraded = false;

  return ret;
}

void D3D12Replay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  m_pDevice->ReplayLog(0, endEventID, replayType);
}

vector<ResourceId> D3D12Replay::GetBuffers()
{
  vector<ResourceId> ret;

  for(auto it = WrappedID3D12Resource::GetList().begin();
      it != WrappedID3D12Resource::GetList().end(); it++)
    if(it->second->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
      ret.push_back(it->first);

  return ret;
}

vector<ResourceId> D3D12Replay::GetTextures()
{
  vector<ResourceId> ret;

  for(auto it = WrappedID3D12Resource::GetList().begin();
      it != WrappedID3D12Resource::GetList().end(); it++)
  {
    if(it->second->GetDesc().Dimension != D3D12_RESOURCE_DIMENSION_BUFFER &&
       m_pDevice->GetResourceManager()->GetOriginalID(it->first) != it->first)
      ret.push_back(it->first);
  }

  return ret;
}

BufferDescription D3D12Replay::GetBuffer(ResourceId id)
{
  BufferDescription ret;
  ret.ID = m_pDevice->GetResourceManager()->GetOriginalID(id);

  auto it = WrappedID3D12Resource::GetList().find(id);

  if(it == WrappedID3D12Resource::GetList().end())
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

  ret.creationFlags = BufferCategory::NoFlags;

  const std::vector<EventUsage> &usage = m_pDevice->GetQueue()->GetUsage(id);

  for(size_t i = 0; i < usage.size(); i++)
  {
    if(usage[i].usage == ResourceUsage::VS_RWResource ||
       usage[i].usage == ResourceUsage::HS_RWResource ||
       usage[i].usage == ResourceUsage::DS_RWResource ||
       usage[i].usage == ResourceUsage::GS_RWResource ||
       usage[i].usage == ResourceUsage::PS_RWResource ||
       usage[i].usage == ResourceUsage::CS_RWResource)
      ret.creationFlags |= BufferCategory::ReadWrite;
    else if(usage[i].usage == ResourceUsage::VertexBuffer)
      ret.creationFlags |= BufferCategory::Vertex;
    else if(usage[i].usage == ResourceUsage::IndexBuffer)
      ret.creationFlags |= BufferCategory::Index;
    else if(usage[i].usage == ResourceUsage::Indirect)
      ret.creationFlags |= BufferCategory::Indirect;
  }

  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    ret.creationFlags |= BufferCategory::ReadWrite;

  return ret;
}

TextureDescription D3D12Replay::GetTexture(ResourceId id)
{
  TextureDescription ret;
  ret.ID = m_pDevice->GetResourceManager()->GetOriginalID(id);

  auto it = WrappedID3D12Resource::GetList().find(id);

  if(it == WrappedID3D12Resource::GetList().end())
    return ret;

  D3D12_RESOURCE_DESC desc = it->second->GetDesc();

  ret.format = MakeResourceFormat(desc.Format);
  ret.dimension = desc.Dimension - D3D12_RESOURCE_DIMENSION_BUFFER;

  ret.width = (uint32_t)desc.Width;
  ret.height = desc.Height;
  ret.depth = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? desc.DepthOrArraySize : 1;
  ret.arraysize = desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D ? desc.DepthOrArraySize : 1;
  ret.mips = desc.MipLevels;
  ret.msQual = desc.SampleDesc.Quality;
  ret.msSamp = RDCMAX(1U, desc.SampleDesc.Count);
  ret.byteSize = 0;
  for(uint32_t i = 0; i < ret.mips; i++)
    ret.byteSize += GetByteSize(ret.width, ret.height, ret.depth, desc.Format, i);
  ret.byteSize *= ret.arraysize;

  switch(ret.dimension)
  {
    case 1:
      ret.resType = ret.arraysize > 1 ? TextureDim::Texture1DArray : TextureDim::Texture1D;
      break;
    case 2:
      if(ret.msSamp > 1)
        ret.resType = ret.arraysize > 1 ? TextureDim::Texture2DMSArray : TextureDim::Texture2DMS;
      else
        ret.resType = ret.arraysize > 1 ? TextureDim::Texture2DArray : TextureDim::Texture2D;
      break;
    case 3: ret.resType = TextureDim::Texture3D; break;
  }

  ret.cubemap = m_pDevice->IsCubemap(id);

  ret.creationFlags = TextureCategory::ShaderRead;

  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    ret.creationFlags |= TextureCategory::ColorTarget;
  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    ret.creationFlags |= TextureCategory::DepthTarget;
  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    ret.creationFlags |= TextureCategory::ShaderReadWrite;

  if(ret.ID == m_pDevice->GetQueue()->GetBackbufferResourceID())
  {
    ret.format = MakeResourceFormat(GetTypedFormat(desc.Format, CompType::UNorm));
    ret.creationFlags |= TextureCategory::SwapBuffer;
  }

  ret.customName = true;
  string str = m_pDevice->GetResourceName(ret.ID);

  if(str == "")
  {
    const char *suffix = "";
    const char *ms = "";

    if(ret.msSamp > 1)
      ms = "MS";

    if(ret.creationFlags & TextureCategory::ColorTarget)
      suffix = " RTV";
    if(ret.creationFlags & TextureCategory::DepthTarget)
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
  WrappedID3D12Shader *sh =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(shader);

  if(sh)
    return &sh->GetDetails();

  return NULL;
}

vector<string> D3D12Replay::GetDisassemblyTargets()
{
  vector<string> ret;

  GCNISA::GetTargets(GraphicsAPI::D3D12, ret);

  // DXBC is always first
  ret.insert(ret.begin(), "DXBC");

  return ret;
}

string D3D12Replay::DisassembleShader(const ShaderReflection *refl, const string &target)
{
  WrappedID3D12Shader *sh = m_pDevice->GetResourceManager()->GetLiveAs<WrappedID3D12Shader>(refl->ID);

  if(!sh)
    return "Invalid Shader Specified";

  DXBC::DXBCFile *dxbc = sh->GetDXBC();

  if(target == "DXBC" || target.empty())
    return dxbc->GetDisassembly();

  return GCNISA::Disassemble(dxbc, target);
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

FrameRecord D3D12Replay::GetFrameRecord()
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

void D3D12Replay::FillResourceView(D3D12Pipe::View &view, D3D12Descriptor *desc)
{
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  if(desc->GetType() == D3D12Descriptor::TypeSampler || desc->GetType() == D3D12Descriptor::TypeCBV)
  {
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
      view.Type = MakeTextureDim(desc->nonsamp.rtv.ViewDimension);

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
      view.Type = MakeTextureDim(desc->nonsamp.dsv.ViewDimension);

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
      view.Type = MakeTextureDim(desc->nonsamp.srv.ViewDimension);

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

        view.BufferFlags = D3DBufferViewFlags(desc->nonsamp.srv.Buffer.Flags);
        if(desc->nonsamp.srv.Buffer.StructureByteStride > 0)
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

      view.Type = MakeTextureDim(uav.ViewDimension);

      if(uav.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
      {
        view.FirstElement = uav.Buffer.FirstElement;
        view.NumElements = uav.Buffer.NumElements;

        view.BufferFlags = D3DBufferViewFlags(uav.Buffer.Flags);
        if(uav.Buffer.StructureByteStride > 0)
          view.ElementSize = uav.Buffer.StructureByteStride;

        view.CounterByteOffset = uav.Buffer.CounterOffsetInBytes;

        if(view.CounterResource != ResourceId())
        {
          vector<byte> counterVal;
          m_pDevice->GetDebugManager()->GetBufferData(desc->nonsamp.uav.counterResource,
                                                      view.CounterByteOffset, 4, counterVal);
          uint32_t *val = (uint32_t *)&counterVal[0];
          view.BufferStructCount = *val;
        }
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

void D3D12Replay::FillRegisterSpaces(const D3D12RenderState::RootSignature &rootSig,
                                     rdctype::array<D3D12Pipe::RegisterSpace> &dstSpaces,
                                     D3D12_SHADER_VISIBILITY visibility)
{
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  WrappedID3D12RootSignature *sig =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rootSig.rootsig);

  struct Space
  {
    vector<D3D12Pipe::CBuffer> cbuffers;
    vector<D3D12Pipe::Sampler> samplers;
    vector<D3D12Pipe::View> srvs;
    vector<D3D12Pipe::View> uavs;
  };

  Space *spaces = new Space[sig->sig.numSpaces];
  create_array_uninit(dstSpaces, sig->sig.numSpaces);

  for(size_t rootEl = 0; rootEl < sig->sig.params.size(); rootEl++)
  {
    const D3D12RootSignatureParameter &p = sig->sig.params[rootEl];

    if(p.ShaderVisibility != D3D12_SHADER_VISIBILITY_ALL && p.ShaderVisibility != visibility)
      continue;

    if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
    {
      D3D12Pipe::CBuffer &cb =
          resize_and_add(spaces[p.Constants.RegisterSpace].cbuffers, p.Constants.ShaderRegister);
      cb.Immediate = true;
      cb.RootElement = (uint32_t)rootEl;
      cb.ByteSize = uint32_t(sizeof(uint32_t) * p.Constants.Num32BitValues);

      if(rootEl < rootSig.sigelems.size())
      {
        const D3D12RenderState::SignatureElement &e = rootSig.sigelems[rootEl];
        if(e.type == eRootConst)
          create_array_init(cb.RootValues, RDCMIN(e.constants.size(), (size_t)cb.ByteSize),
                            &e.constants[0]);
      }

      if(cb.RootValues.count == 0)
        create_array(cb.RootValues, p.Constants.Num32BitValues);
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV)
    {
      D3D12Pipe::CBuffer &cb =
          resize_and_add(spaces[p.Descriptor.RegisterSpace].cbuffers, p.Descriptor.ShaderRegister);
      cb.Immediate = true;
      cb.RootElement = (uint32_t)rootEl;

      if(rootEl < rootSig.sigelems.size())
      {
        const D3D12RenderState::SignatureElement &e = rootSig.sigelems[rootEl];
        if(e.type == eRootCBV)
        {
          ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(e.id);

          cb.Buffer = rm->GetOriginalID(e.id);
          cb.Offset = e.offset;
          cb.ByteSize = uint32_t(res->GetDesc().Width - cb.Offset);
        }
      }
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV)
    {
      D3D12Pipe::View &view =
          resize_and_add(spaces[p.Descriptor.RegisterSpace].srvs, p.Descriptor.ShaderRegister);
      view.Immediate = true;
      view.RootElement = (uint32_t)rootEl;

      if(rootEl < rootSig.sigelems.size())
      {
        const D3D12RenderState::SignatureElement &e = rootSig.sigelems[rootEl];
        if(e.type == eRootSRV)
        {
          ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(e.id);

          // parameters from resource/view
          view.Resource = rm->GetOriginalID(e.id);
          view.Type = TextureDim::Buffer;
          view.Format = MakeResourceFormat(DXGI_FORMAT_R32_UINT);

          view.ElementSize = sizeof(uint32_t);
          view.FirstElement = e.offset / sizeof(uint32_t);
          view.NumElements = uint32_t((res->GetDesc().Width - e.offset) / sizeof(uint32_t));
        }
      }
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV)
    {
      D3D12Pipe::View &view =
          resize_and_add(spaces[p.Descriptor.RegisterSpace].uavs, p.Descriptor.ShaderRegister);
      view.Immediate = true;
      view.RootElement = (uint32_t)rootEl;

      if(rootEl < rootSig.sigelems.size())
      {
        const D3D12RenderState::SignatureElement &e = rootSig.sigelems[rootEl];
        if(e.type == eRootUAV)
        {
          ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(e.id);

          // parameters from resource/view
          view.Resource = rm->GetOriginalID(e.id);
          view.Type = TextureDim::Buffer;
          view.Format = MakeResourceFormat(DXGI_FORMAT_R32_UINT);

          view.ElementSize = sizeof(uint32_t);
          view.FirstElement = e.offset / sizeof(uint32_t);
          view.NumElements = uint32_t((res->GetDesc().Width - e.offset) / sizeof(uint32_t));
        }
      }
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
    {
      const D3D12RenderState::SignatureElement *e = NULL;
      WrappedID3D12DescriptorHeap *heap = NULL;

      if(rootEl < rootSig.sigelems.size() && rootSig.sigelems[rootEl].type == eRootTable)
      {
        e = &rootSig.sigelems[rootEl];

        heap = rm->GetCurrentAs<WrappedID3D12DescriptorHeap>(e->id);
      }

      UINT prevTableOffset = 0;

      for(size_t r = 0; r < p.ranges.size(); r++)
      {
        const D3D12_DESCRIPTOR_RANGE1 &range = p.ranges[r];

        UINT shaderReg = range.BaseShaderRegister;
        UINT regSpace = range.RegisterSpace;

        D3D12Descriptor *desc = NULL;

        UINT offset = range.OffsetInDescriptorsFromTableStart;

        if(range.OffsetInDescriptorsFromTableStart == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND)
          offset = prevTableOffset;

        UINT num = range.NumDescriptors;

        if(heap)
        {
          desc = (D3D12Descriptor *)heap->GetCPUDescriptorHandleForHeapStart().ptr;
          desc += e->offset;
          desc += offset;

          if(num == UINT_MAX)
          {
            // find out how many descriptors are left after
            num = heap->GetNumDescriptors() - offset - UINT(e->offset);
          }
        }
        else if(num == UINT_MAX)
        {
          RDCWARN(
              "Heap not available on replay with unbounded descriptor range, clamping to 1 "
              "descriptor.");
          num = 1;
        }

        prevTableOffset = offset + num;

        if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
        {
          UINT maxReg = shaderReg + num - 1;
          if(maxReg >= spaces[regSpace].samplers.size())
            spaces[regSpace].samplers.resize(maxReg + 1);

          for(UINT i = 0; i < num; i++, shaderReg++)
          {
            D3D12Pipe::Sampler &samp = spaces[regSpace].samplers[shaderReg];
            samp.Immediate = false;
            samp.RootElement = (uint32_t)rootEl;
            samp.TableIndex = offset + i;

            if(desc)
            {
              D3D12_SAMPLER_DESC &sampDesc = desc->samp.desc;

              samp.AddressU = MakeAddressMode(sampDesc.AddressU);
              samp.AddressV = MakeAddressMode(sampDesc.AddressV);
              samp.AddressW = MakeAddressMode(sampDesc.AddressW);

              memcpy(samp.BorderColor, sampDesc.BorderColor, sizeof(FLOAT) * 4);

              samp.Comparison = MakeCompareFunc(sampDesc.ComparisonFunc);
              samp.Filter = MakeFilter(sampDesc.Filter);
              samp.MaxAniso = 0;
              if(samp.Filter.minify == FilterMode::Anisotropic)
                samp.MaxAniso = sampDesc.MaxAnisotropy;
              samp.MaxLOD = sampDesc.MaxLOD;
              samp.MinLOD = sampDesc.MinLOD;
              samp.MipLODBias = sampDesc.MipLODBias;

              desc++;
            }
          }
        }
        else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
        {
          UINT maxReg = shaderReg + num - 1;
          if(maxReg >= spaces[regSpace].cbuffers.size())
            spaces[regSpace].cbuffers.resize(maxReg + 1);

          for(UINT i = 0; i < num; i++, shaderReg++)
          {
            D3D12Pipe::CBuffer &cb = spaces[regSpace].cbuffers[shaderReg];
            cb.Immediate = false;
            cb.RootElement = (uint32_t)rootEl;
            cb.TableIndex = offset + i;

            if(desc)
            {
              WrappedID3D12Resource::GetResIDFromAddr(desc->nonsamp.cbv.BufferLocation, cb.Buffer,
                                                      cb.Offset);
              cb.Buffer = rm->GetOriginalID(cb.Buffer);
              cb.ByteSize = desc->nonsamp.cbv.SizeInBytes;

              desc++;
            }
          }
        }
        else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SRV)
        {
          UINT maxReg = shaderReg + num - 1;
          if(maxReg >= spaces[regSpace].srvs.size())
            spaces[regSpace].srvs.resize(maxReg + 1);

          for(UINT i = 0; i < num; i++, shaderReg++)
          {
            D3D12Pipe::View &view = spaces[regSpace].srvs[shaderReg];
            view.Immediate = false;
            view.RootElement = (uint32_t)rootEl;
            view.TableIndex = offset + i;

            if(desc)
            {
              FillResourceView(view, desc);

              desc++;
            }
          }
        }
        else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_UAV)
        {
          UINT maxReg = shaderReg + num - 1;
          if(maxReg >= spaces[regSpace].uavs.size())
            spaces[regSpace].uavs.resize(maxReg + 1);

          for(UINT i = 0; i < num; i++, shaderReg++)
          {
            D3D12Pipe::View &view = spaces[regSpace].uavs[shaderReg];
            view.Immediate = false;
            view.RootElement = (uint32_t)rootEl;
            view.TableIndex = offset + i;

            if(desc)
            {
              FillResourceView(view, desc);

              desc++;
            }
          }
        }
      }
    }
  }

  for(size_t i = 0; i < sig->sig.samplers.size(); i++)
  {
    D3D12_STATIC_SAMPLER_DESC &sampDesc = sig->sig.samplers[i];

    if(sampDesc.ShaderVisibility != D3D12_SHADER_VISIBILITY_ALL &&
       sampDesc.ShaderVisibility != visibility)
      continue;

    D3D12Pipe::Sampler &samp =
        resize_and_add(spaces[sampDesc.RegisterSpace].samplers, sampDesc.ShaderRegister);
    samp.Immediate = true;
    samp.RootElement = (uint32_t)i;

    samp.AddressU = MakeAddressMode(sampDesc.AddressU);
    samp.AddressV = MakeAddressMode(sampDesc.AddressV);
    samp.AddressW = MakeAddressMode(sampDesc.AddressW);

    if(sampDesc.BorderColor == D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK)
    {
      samp.BorderColor[0] = 0.0f;
      samp.BorderColor[1] = 0.0f;
      samp.BorderColor[2] = 0.0f;
      samp.BorderColor[3] = 0.0f;
    }
    else if(sampDesc.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK)
    {
      samp.BorderColor[0] = 0.0f;
      samp.BorderColor[1] = 0.0f;
      samp.BorderColor[2] = 0.0f;
      samp.BorderColor[3] = 1.0f;
    }
    else if(sampDesc.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE)
    {
      samp.BorderColor[0] = 1.0f;
      samp.BorderColor[1] = 1.0f;
      samp.BorderColor[2] = 1.0f;
      samp.BorderColor[3] = 1.0f;
    }
    else
    {
      RDCERR("Unexpected static border colour: %u", sampDesc.BorderColor);
    }

    samp.Comparison = MakeCompareFunc(sampDesc.ComparisonFunc);
    samp.Filter = MakeFilter(sampDesc.Filter);
    samp.MaxAniso = 0;
    if(samp.Filter.minify == FilterMode::Anisotropic)
      samp.MaxAniso = sampDesc.MaxAnisotropy;
    samp.MaxLOD = sampDesc.MaxLOD;
    samp.MinLOD = sampDesc.MinLOD;
    samp.MipLODBias = sampDesc.MipLODBias;
  }

  for(uint32_t i = 0; i < sig->sig.numSpaces; i++)
  {
    dstSpaces[i].ConstantBuffers = spaces[i].cbuffers;
    dstSpaces[i].Samplers = spaces[i].samplers;
    dstSpaces[i].SRVs = spaces[i].srvs;
    dstSpaces[i].UAVs = spaces[i].uavs;
  }

  SAFE_DELETE_ARRAY(spaces);
}

void D3D12Replay::MakePipelineState()
{
  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  D3D12Pipe::State state;

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

  state.name = str;

  if(pipe && pipe->IsGraphics())
  {
    const D3D12_INPUT_ELEMENT_DESC *inputEl = pipe->graphics->InputLayout.pInputElementDescs;
    UINT numInput = pipe->graphics->InputLayout.NumElements;

    create_array_uninit(state.m_IA.layouts, numInput);
    for(UINT i = 0; i < numInput; i++)
    {
      D3D12Pipe::Layout &l = state.m_IA.layouts[i];

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
      D3D12Pipe::VB &vb = state.m_IA.vbuffers[i];

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

  if(pipe && pipe->IsCompute())
  {
    WrappedID3D12Shader *sh = (WrappedID3D12Shader *)pipe->compute->CS.pShaderBytecode;

    state.m_CS.Object = sh->GetResourceID();
    state.m_CS.stage = ShaderStage::Compute;
    state.m_CS.BindpointMapping = sh->GetMapping();

    state.rootSig = rm->GetOriginalID(rs.compute.rootsig);

    if(rs.compute.rootsig != ResourceId())
      FillRegisterSpaces(rs.compute, state.m_CS.Spaces, D3D12_SHADER_VISIBILITY_ALL);
  }
  else if(pipe)
  {
    D3D12Pipe::Shader *dstArr[] = {&state.m_VS, &state.m_HS, &state.m_DS, &state.m_GS, &state.m_PS};

    D3D12_SHADER_BYTECODE *srcArr[] = {&pipe->graphics->VS, &pipe->graphics->HS, &pipe->graphics->DS,
                                       &pipe->graphics->GS, &pipe->graphics->PS};

    D3D12_SHADER_VISIBILITY visibility[] = {
        D3D12_SHADER_VISIBILITY_VERTEX, D3D12_SHADER_VISIBILITY_HULL, D3D12_SHADER_VISIBILITY_DOMAIN,
        D3D12_SHADER_VISIBILITY_GEOMETRY, D3D12_SHADER_VISIBILITY_PIXEL};

    for(size_t stage = 0; stage < 5; stage++)
    {
      D3D12Pipe::Shader &dst = *dstArr[stage];
      D3D12_SHADER_BYTECODE &src = *srcArr[stage];

      dst.stage = (ShaderStage)stage;

      WrappedID3D12Shader *sh = (WrappedID3D12Shader *)src.pShaderBytecode;

      if(sh)
      {
        dst.Object = sh->GetResourceID();
        dst.BindpointMapping = sh->GetMapping();
      }

      if(rs.graphics.rootsig != ResourceId())
        FillRegisterSpaces(rs.graphics, dst.Spaces, visibility[stage]);
    }

    state.rootSig = rm->GetOriginalID(rs.graphics.rootsig);
  }

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
      D3D12Pipe::RasterizerState &dst = state.m_RS.m_State;
      D3D12_RASTERIZER_DESC &src = pipe->graphics->RasterizerState;

      dst.AntialiasedLineEnable = src.AntialiasedLineEnable == TRUE;

      dst.cullMode = CullMode::NoCull;
      if(src.CullMode == D3D12_CULL_MODE_FRONT)
        dst.cullMode = CullMode::Front;
      if(src.CullMode == D3D12_CULL_MODE_BACK)
        dst.cullMode = CullMode::Back;

      dst.fillMode = FillMode::Solid;
      if(src.FillMode == D3D12_FILL_MODE_WIREFRAME)
        dst.fillMode = FillMode::Wireframe;

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

    create_array_uninit(state.m_RS.Scissors, rs.scissors.size());
    for(size_t i = 0; i < rs.scissors.size(); i++)
      state.m_RS.Scissors[i] = D3D12Pipe::Scissor(rs.scissors[i].left, rs.scissors[i].top,
                                                  rs.scissors[i].right, rs.scissors[i].bottom);

    create_array_uninit(state.m_RS.Viewports, rs.views.size());
    for(size_t i = 0; i < rs.views.size(); i++)
      state.m_RS.Viewports[i] =
          D3D12Pipe::Viewport(rs.views[i].TopLeftX, rs.views[i].TopLeftY, rs.views[i].Width,
                              rs.views[i].Height, rs.views[i].MinDepth, rs.views[i].MaxDepth);

    /////////////////////////////////////////////////
    // Output Merger
    /////////////////////////////////////////////////

    create_array(state.m_OM.RenderTargets, rs.rts.size());
    for(size_t i = 0; i < rs.rts.size(); i++)
    {
      D3D12Pipe::View &view = state.m_OM.RenderTargets[i];

      PortableHandle h = rs.rtSingle ? rs.rts[0] : rs.rts[i];

      if(h.heap != ResourceId())
      {
        D3D12Descriptor *desc = DescriptorFromPortableHandle(rm, h);

        if(rs.rtSingle)
          desc += i;

        view.RootElement = (uint32_t)i;
        view.Immediate = false;

        FillResourceView(view, desc);
      }
    }

    {
      D3D12Pipe::View &view = state.m_OM.DepthTarget;

      if(rs.dsv.heap != ResourceId())
      {
        D3D12Descriptor *desc = DescriptorFromPortableHandle(rm, rs.dsv);

        view.RootElement = 0;
        view.Immediate = false;

        FillResourceView(view, desc);
      }
    }

    memcpy(state.m_OM.m_BlendState.BlendFactor, rs.blendFactor, sizeof(FLOAT) * 4);

    {
      D3D12_BLEND_DESC &src = pipe->graphics->BlendState;

      state.m_OM.m_BlendState.AlphaToCoverage = src.AlphaToCoverageEnable == TRUE;
      state.m_OM.m_BlendState.IndependentBlend = src.IndependentBlendEnable == TRUE;

      create_array_uninit(state.m_OM.m_BlendState.Blends, 8);
      for(size_t i = 0; i < 8; i++)
      {
        D3D12Pipe::Blend &blend = state.m_OM.m_BlendState.Blends[i];

        blend.Enabled = src.RenderTarget[i].BlendEnable == TRUE;

        blend.LogicEnabled = src.RenderTarget[i].LogicOpEnable == TRUE;
        blend.Logic = MakeLogicOp(src.RenderTarget[i].LogicOp);

        blend.m_AlphaBlend.Source = MakeBlendMultiplier(src.RenderTarget[i].SrcBlendAlpha, true);
        blend.m_AlphaBlend.Destination =
            MakeBlendMultiplier(src.RenderTarget[i].DestBlendAlpha, true);
        blend.m_AlphaBlend.Operation = MakeBlendOp(src.RenderTarget[i].BlendOpAlpha);

        blend.m_Blend.Source = MakeBlendMultiplier(src.RenderTarget[i].SrcBlend, false);
        blend.m_Blend.Destination = MakeBlendMultiplier(src.RenderTarget[i].DestBlend, false);
        blend.m_Blend.Operation = MakeBlendOp(src.RenderTarget[i].BlendOp);

        blend.WriteMask = src.RenderTarget[i].RenderTargetWriteMask;
      }
    }

    {
      D3D12_DEPTH_STENCIL_DESC &src = pipe->graphics->DepthStencilState;

      state.m_OM.m_State.DepthEnable = src.DepthEnable == TRUE;
      state.m_OM.m_State.DepthFunc = MakeCompareFunc(src.DepthFunc);
      state.m_OM.m_State.DepthWrites = src.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ALL;
      state.m_OM.m_State.StencilEnable = src.StencilEnable == TRUE;
      state.m_OM.m_State.StencilRef = rs.stencilRef;
      state.m_OM.m_State.StencilReadMask = src.StencilReadMask;
      state.m_OM.m_State.StencilWriteMask = src.StencilWriteMask;

      state.m_OM.m_State.m_FrontFace.Func = MakeCompareFunc(src.FrontFace.StencilFunc);
      state.m_OM.m_State.m_FrontFace.DepthFailOp = MakeStencilOp(src.FrontFace.StencilDepthFailOp);
      state.m_OM.m_State.m_FrontFace.PassOp = MakeStencilOp(src.FrontFace.StencilPassOp);
      state.m_OM.m_State.m_FrontFace.FailOp = MakeStencilOp(src.FrontFace.StencilFailOp);

      state.m_OM.m_State.m_BackFace.Func = MakeCompareFunc(src.BackFace.StencilFunc);
      state.m_OM.m_State.m_BackFace.DepthFailOp = MakeStencilOp(src.BackFace.StencilDepthFailOp);
      state.m_OM.m_State.m_BackFace.PassOp = MakeStencilOp(src.BackFace.StencilPassOp);
      state.m_OM.m_State.m_BackFace.FailOp = MakeStencilOp(src.BackFace.StencilFailOp);
    }
  }

  // resource states
  {
    const map<ResourceId, SubresourceStateVector> &states = m_pDevice->GetSubresourceStates();
    create_array_uninit(state.Resources, states.size());
    size_t i = 0;
    for(auto it = states.begin(); it != states.end(); ++it)
    {
      D3D12Pipe::ResourceData &res = state.Resources[i];

      res.id = rm->GetOriginalID(it->first);

      create_array_uninit(res.states, it->second.size());
      for(size_t l = 0; l < it->second.size(); l++)
        res.states[l].name = ToStr::Get(it->second[l]);

      i++;
    }
  }

  m_PipelineState = state;
}

void D3D12Replay::RenderCheckerboard(Vec3f light, Vec3f dark)
{
  m_pDevice->GetDebugManager()->RenderCheckerboard(light, dark);
}

void D3D12Replay::RenderHighlightBox(float w, float h, float scale)
{
  m_pDevice->GetDebugManager()->RenderHighlightBox(w, h, scale);
}

bool D3D12Replay::RenderTexture(TextureDisplay cfg)
{
  return m_pDevice->GetDebugManager()->RenderTexture(cfg, true);
}

void D3D12Replay::RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws,
                             const MeshDisplay &cfg)
{
  return m_pDevice->GetDebugManager()->RenderMesh(eventID, secondaryDraws, cfg);
}

bool D3D12Replay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                            CompType typeHint, float *minval, float *maxval)
{
  return m_pDevice->GetDebugManager()->GetMinMax(texid, sliceFace, mip, sample, typeHint, minval,
                                                 maxval);
}

bool D3D12Replay::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                               CompType typeHint, float minval, float maxval, bool channels[4],
                               vector<uint32_t> &histogram)
{
  return m_pDevice->GetDebugManager()->GetHistogram(texid, sliceFace, mip, sample, typeHint, minval,
                                                    maxval, channels, histogram);
}

ResourceId D3D12Replay::RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                      uint32_t eventID, const vector<uint32_t> &passEvents)
{
  return m_pDevice->GetDebugManager()->RenderOverlay(texid, typeHint, overlay, eventID, passEvents);
}

bool D3D12Replay::IsRenderOutput(ResourceId id)
{
  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  id = m_pDevice->GetResourceManager()->GetLiveID(id);

  for(size_t i = 0; i < rs.rts.size(); i++)
  {
    PortableHandle h = rs.rtSingle ? rs.rts[0] : rs.rts[i];

    if(h.heap != ResourceId())
    {
      D3D12Descriptor *desc = DescriptorFromPortableHandle(m_pDevice->GetResourceManager(), h);

      if(rs.rtSingle)
        desc += i;

      if(id == GetResID(desc->nonsamp.resource))
        return true;
    }
  }

  if(rs.dsv.heap != ResourceId())
  {
    D3D12Descriptor *desc = DescriptorFromPortableHandle(m_pDevice->GetResourceManager(), rs.dsv);

    if(id == GetResID(desc->nonsamp.resource))
      return true;
  }

  return false;
}

vector<uint32_t> D3D12Replay::GetPassEvents(uint32_t eventID)
{
  vector<uint32_t> passEvents;

  const DrawcallDescription *draw = m_pDevice->GetDrawcall(eventID);

  if(!draw)
    return passEvents;

  // for D3D12 a pass == everything writing to the same RTs in a command list.
  const DrawcallDescription *start = draw;
  while(start)
  {
    // if we've come to the beginning of a list, break out of the loop, we've
    // found the start.
    if(start->flags & DrawFlags::BeginPass)
      break;

    // if we come to the END of a list, since we were iterating backwards that
    // means we started outside of a list, so return empty set.
    if(start->flags & DrawFlags::EndPass)
      return passEvents;

    // if we've come to the start of the log we were outside of a list
    // to start with
    if(start->previous == 0)
      return passEvents;

    // step back
    const DrawcallDescription *prev = m_pDevice->GetDrawcall((uint32_t)start->previous);

    // something went wrong, start->previous was non-zero but we didn't
    // get a draw. Abort
    if(!prev)
      return passEvents;

    // if the outputs changed, we're done
    if(memcmp(start->outputs, prev->outputs, sizeof(start->outputs)) ||
       start->depthOut != prev->depthOut)
      break;

    start = prev;
  }

  // store all the draw eventIDs up to the one specified at the start
  while(start)
  {
    if(start == draw)
      break;

    // include pass boundaries, these will be filtered out later
    // so we don't actually do anything (init postvs/draw overlay)
    // but it's useful to have the first part of the pass as part
    // of the list
    if(start->flags & (DrawFlags::Drawcall | DrawFlags::PassBoundary))
      passEvents.push_back(start->eventID);

    start = m_pDevice->GetDrawcall((uint32_t)start->next);
  }

  return passEvents;
}

bool D3D12Replay::IsTextureSupported(const ResourceFormat &format)
{
  return MakeDXGIFormat(format) != DXGI_FORMAT_UNKNOWN;
}

void D3D12Replay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &retData)
{
  m_pDevice->GetDebugManager()->GetBufferData(buff, offset, len, retData);
}

void D3D12Replay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                            uint32_t mip, uint32_t sample, CompType typeHint, float pixel[4])
{
  m_pDevice->GetDebugManager()->PickPixel(texture, x, y, sliceFace, mip, sample, typeHint, pixel);
}

void D3D12Replay::FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                                       vector<ShaderVariable> &outvars, const vector<byte> &data)
{
  if(shader == ResourceId())
    return;

  ID3D12DeviceChild *res = m_pDevice->GetResourceManager()->GetCurrentResource(shader);

  if(!WrappedID3D12Shader::IsAlloc(res))
  {
    RDCERR("Shader ID %llu does not correspond to a known fake shader", shader);
    return;
  }

  WrappedID3D12Shader *sh = (WrappedID3D12Shader *)res;

  DXBC::DXBCFile *dxbc = sh->GetDXBC();
  const ShaderBindpointMapping &bindMap = sh->GetMapping();

  RDCASSERT(dxbc);

  DXBC::CBuffer *cb = NULL;

  uint32_t idx = 0;
  for(size_t i = 0; i < dxbc->m_CBuffers.size(); i++)
  {
    if(dxbc->m_CBuffers[i].descriptor.type != DXBC::CBuffer::Descriptor::TYPE_CBUFFER)
      continue;

    if(idx == cbufSlot)
      cb = &dxbc->m_CBuffers[i];

    idx++;
  }

  if(cb && cbufSlot < (uint32_t)bindMap.ConstantBlocks.count)
  {
    // check if the data actually comes from root constants

    const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
    BindpointMap bind = bindMap.ConstantBlocks[cbufSlot];

    WrappedID3D12RootSignature *sig = NULL;
    const vector<D3D12RenderState::SignatureElement> *sigElems = NULL;

    if(dxbc->m_Type == D3D11_ShaderType_Compute && rs.compute.rootsig != ResourceId())
    {
      sig = m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(
          rs.compute.rootsig);
      sigElems = &rs.compute.sigelems;
    }
    else if(dxbc->m_Type != D3D11_ShaderType_Compute && rs.graphics.rootsig != ResourceId())
    {
      sig = m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(
          rs.graphics.rootsig);
      sigElems = &rs.graphics.sigelems;
    }

    vector<byte> rootData;

    for(size_t i = 0; sig && i < sig->sig.params.size(); i++)
    {
      const D3D12RootSignatureParameter &p = sig->sig.params[i];

      if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS &&
         p.Constants.RegisterSpace == (UINT)bind.bindset &&
         p.Constants.ShaderRegister == (UINT)bind.bind)
      {
        size_t dstSize = sig->sig.params[i].Constants.Num32BitValues * sizeof(uint32_t);
        rootData.resize(dstSize);

        if(i < sigElems->size())
        {
          const D3D12RenderState::SignatureElement &el = (*sigElems)[i];

          if(el.type == eRootConst)
          {
            byte *dst = &rootData[0];
            const UINT *src = &el.constants[0];
            size_t srcSize = el.constants.size() * sizeof(uint32_t);

            memcpy(dst, src, RDCMIN(srcSize, dstSize));
          }
        }
      }
    }

    m_pDevice->GetDebugManager()->FillCBufferVariables(cb->variables, outvars, false,
                                                       rootData.empty() ? data : rootData);
  }
}

void D3D12Replay::InitPostVSBuffers(uint32_t eventID)
{
  m_pDevice->GetDebugManager()->InitPostVSBuffers(eventID);
}

struct D3D12InitPostVSCallback : public D3D12DrawcallCallback
{
  D3D12InitPostVSCallback(WrappedID3D12Device *dev, const vector<uint32_t> &events)
      : m_pDevice(dev), m_Events(events)
  {
    m_pDevice->GetQueue()->GetCommandData()->m_DrawcallCallback = this;
  }
  ~D3D12InitPostVSCallback() { m_pDevice->GetQueue()->GetCommandData()->m_DrawcallCallback = NULL; }
  void PreDraw(uint32_t eid, ID3D12GraphicsCommandList *cmd)
  {
    if(std::find(m_Events.begin(), m_Events.end(), eid) != m_Events.end())
      m_pDevice->GetDebugManager()->InitPostVSBuffers(eid);
  }

  bool PostDraw(uint32_t eid, ID3D12GraphicsCommandList *cmd) { return false; }
  void PostRedraw(uint32_t eid, ID3D12GraphicsCommandList *cmd) {}
  // Dispatches don't rasterize, so do nothing
  void PreDispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd) {}
  bool PostDispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd) { return false; }
  void PostRedispatch(uint32_t eid, ID3D12GraphicsCommandList *cmd) {}
  bool RecordAllCmds() { return false; }
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    if(std::find(m_Events.begin(), m_Events.end(), primary) != m_Events.end())
      m_pDevice->GetDebugManager()->AliasPostVSBuffers(primary, alias);
  }

  WrappedID3D12Device *m_pDevice;
  const vector<uint32_t> &m_Events;
};

void D3D12Replay::InitPostVSBuffers(const vector<uint32_t> &events)
{
  // first we must replay up to the first event without replaying it. This ensures any
  // non-command buffer calls like memory unmaps etc all happen correctly before this
  // command buffer
  m_pDevice->ReplayLog(0, events.front(), eReplay_WithoutDraw);

  D3D12InitPostVSCallback cb(m_pDevice, events);

  // now we replay the events, which are guaranteed (because we generated them in
  // GetPassEvents above) to come from the same command buffer, so the event IDs are
  // still locally continuous, even if we jump into replaying.
  m_pDevice->ReplayLog(events.front(), events.back(), eReplay_Full);
}

MeshFormat D3D12Replay::GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
  return m_pDevice->GetDebugManager()->GetPostVSBuffers(eventID, instID, stage);
}

uint32_t D3D12Replay::PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y)
{
  return m_pDevice->GetDebugManager()->PickVertex(eventID, cfg, x, y);
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

void D3D12Replay::ClearOutputWindowColor(uint64_t id, float col[4])
{
  m_pDevice->GetDebugManager()->ClearOutputWindowColor(id, col);
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

void D3D12Replay::InitCallstackResolver()
{
  m_pDevice->GetMainSerialiser()->InitCallstackResolver();
}

bool D3D12Replay::HasCallstacks()
{
  return m_pDevice->GetMainSerialiser()->HasCallstacks();
}

Callstack::StackResolver *D3D12Replay::GetCallstackResolver()
{
  return m_pDevice->GetMainSerialiser()->GetCallstackResolver();
}

vector<DebugMessage> D3D12Replay::GetDebugMessages()
{
  return m_pDevice->GetDebugMessages();
}

void D3D12Replay::BuildTargetShader(string source, string entry, const uint32_t compileFlags,
                                    ShaderStage type, ResourceId *id, string *errors)
{
  m_pDevice->GetDebugManager()->BuildShader(source, entry, D3DCOMPILE_DEBUG | compileFlags, type,
                                            id, errors);
}

void D3D12Replay::ReplaceResource(ResourceId from, ResourceId to)
{
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  // remove any previous replacement
  RemoveReplacement(from);

  if(rm->HasLiveResource(from))
  {
    ID3D12DeviceChild *resource = rm->GetLiveResource(from);

    if(WrappedID3D12Shader::IsAlloc(resource))
    {
      WrappedID3D12Shader *sh = (WrappedID3D12Shader *)resource;

      for(size_t i = 0; i < sh->m_Pipes.size(); i++)
      {
        WrappedID3D12PipelineState *pipe = sh->m_Pipes[i];

        ResourceId id = rm->GetOriginalID(pipe->GetResourceID());

        ID3D12PipelineState *replpipe = NULL;

        D3D12_SHADER_BYTECODE shDesc = rm->GetLiveAs<WrappedID3D12Shader>(to)->GetDesc();

        if(pipe->graphics)
        {
          D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = *pipe->graphics;

          D3D12_SHADER_BYTECODE *shaders[] = {
              &desc.VS, &desc.HS, &desc.DS, &desc.GS, &desc.PS,
          };

          for(size_t s = 0; s < ARRAY_COUNT(shaders); s++)
          {
            if(shaders[s]->BytecodeLength > 0)
            {
              WrappedID3D12Shader *stage = (WrappedID3D12Shader *)shaders[s]->pShaderBytecode;

              if(stage->GetResourceID() == from)
                *shaders[s] = shDesc;
              else
                *shaders[s] = stage->GetDesc();
            }
          }

          m_pDevice->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState),
                                                 (void **)&replpipe);
        }
        else
        {
          D3D12_COMPUTE_PIPELINE_STATE_DESC desc = *pipe->compute;

          // replace the shader
          desc.CS = shDesc;

          m_pDevice->CreateComputePipelineState(&desc, __uuidof(ID3D12PipelineState),
                                                (void **)&replpipe);
        }

        rm->ReplaceResource(id, GetResID(replpipe));
      }
    }

    rm->ReplaceResource(from, to);
  }
}

void D3D12Replay::RemoveReplacement(ResourceId id)
{
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  rm->RemoveReplacement(id);

  if(rm->HasLiveResource(id))
  {
    ID3D12DeviceChild *resource = rm->GetLiveResource(id);

    if(WrappedID3D12Shader::IsAlloc(resource))
    {
      WrappedID3D12Shader *sh = (WrappedID3D12Shader *)resource;

      for(size_t i = 0; i < sh->m_Pipes.size(); i++)
      {
        WrappedID3D12PipelineState *pipe = sh->m_Pipes[i];

        ResourceId pipeid = rm->GetOriginalID(pipe->GetResourceID());

        rm->RemoveReplacement(pipeid);
      }
    }
  }
}

byte *D3D12Replay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                  const GetTextureDataParams &params, size_t &dataSize)
{
  return m_pDevice->GetDebugManager()->GetTextureData(tex, arrayIdx, mip, params, dataSize);
}

void D3D12Replay::BuildCustomShader(string source, string entry, const uint32_t compileFlags,
                                    ShaderStage type, ResourceId *id, string *errors)
{
  m_pDevice->GetDebugManager()->BuildShader(source, entry, compileFlags, type, id, errors);
}

ResourceId D3D12Replay::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip,
                                          uint32_t arrayIdx, uint32_t sampleIdx, CompType typeHint)
{
  return m_pDevice->GetDebugManager()->ApplyCustomShader(shader, texid, mip, arrayIdx, sampleIdx,
                                                         typeHint);
}

#pragma region not yet implemented

vector<PixelModification> D3D12Replay::PixelHistory(vector<EventUsage> events, ResourceId target,
                                                    uint32_t x, uint32_t y, uint32_t slice,
                                                    uint32_t mip, uint32_t sampleIdx,
                                                    CompType typeHint)
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

ShaderDebugTrace D3D12Replay::DebugThread(uint32_t eventID, const uint32_t groupid[3],
                                          const uint32_t threadid[3])
{
  return ShaderDebugTrace();
}

ResourceId D3D12Replay::CreateProxyTexture(const TextureDescription &templateTex)
{
  return ResourceId();
}

void D3D12Replay::SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                                      size_t dataSize)
{
}

ResourceId D3D12Replay::CreateProxyBuffer(const BufferDescription &templateBuf)
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

ReplayStatus D3D12_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
  RDCDEBUG("Creating a D3D12 replay device");

  WrappedIDXGISwapChain4::RegisterD3DDeviceCallback(GetD3D12DeviceIfAlloc);

  HMODULE lib = NULL;
  lib = LoadLibraryA("d3d12.dll");
  if(lib == NULL)
  {
    RDCERR("Failed to load d3d12.dll");
    return ReplayStatus::APIInitFailed;
  }

  lib = LoadLibraryA("dxgi.dll");
  if(lib == NULL)
  {
    RDCERR("Failed to load dxgi.dll");
    return ReplayStatus::APIInitFailed;
  }

  if(GetD3DCompiler() == NULL)
  {
    RDCERR("Failed to load d3dcompiler_??.dll");
    return ReplayStatus::APIInitFailed;
  }

  D3D12InitParams initParams;
  RDCDriver driverFileType = RDC_D3D12;
  string driverName = "D3D12";
  uint64_t machineIdent = 0;
  if(logfile)
  {
    auto status = RenderDoc::Inst().FillInitParams(logfile, driverFileType, driverName,
                                                   machineIdent, (RDCInitParams *)&initParams);
    if(status != ReplayStatus::Succeeded)
      return status;
  }

  // initParams.SerialiseVersion is guaranteed to be valid/supported since otherwise the
  // FillInitParams (which calls D3D12InitParams::Serialise) would have failed above, so no need to
  // check it here.

  if(initParams.MinimumFeatureLevel < D3D_FEATURE_LEVEL_11_0)
    initParams.MinimumFeatureLevel = D3D_FEATURE_LEVEL_11_0;

  D3D12Replay::PreDeviceInitCounters();

  ID3D12Device *dev = NULL;
  HRESULT hr = RENDERDOC_CreateWrappedD3D12Device(NULL, initParams.MinimumFeatureLevel,
                                                  __uuidof(ID3D12Device), (void **)&dev);

  if(FAILED(hr))
  {
    RDCERR("Couldn't create a d3d12 device :(.");

    return ReplayStatus::APIHardwareUnsupported;
  }

  WrappedID3D12Device *wrappedDev = (WrappedID3D12Device *)dev;
  if(logfile)
    wrappedDev->SetLogFile(logfile);
  wrappedDev->SetLogVersion(initParams.SerialiseVersion);

  if(logfile && wrappedDev->GetMainSerialiser()->HasError())
  {
    SAFE_RELEASE(wrappedDev);
    return ReplayStatus::FileIOFailed;
  }

  RDCLOG("Created device.");
  D3D12Replay *replay = wrappedDev->GetReplay();

  replay->SetProxy(logfile == NULL);

  *driver = (IReplayDriver *)replay;
  return ReplayStatus::Succeeded;
}

static DriverRegistration D3D12DriverRegistration(RDC_D3D12, "D3D12", &D3D12_CreateReplayDevice);
