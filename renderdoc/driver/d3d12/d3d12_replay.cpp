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
#include "serialise/rdcfile.h"
#include "d3d12_command_queue.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"

static const char *DXBCDisassemblyTarget = "DXBC";

template <class T>
T &resize_and_add(rdcarray<T> &vec, size_t idx)
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

ReplayStatus D3D12Replay::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  return m_pDevice->ReadLogInitialisation(rdc, storeStructuredBuffers);
}

APIProperties D3D12Replay::GetAPIProperties()
{
  APIProperties ret = m_pDevice->APIProps;

  ret.pipelineType = GraphicsAPI::D3D12;
  ret.localRenderer = GraphicsAPI::D3D12;
  ret.degraded = false;
  ret.shadersMutable = false;

  return ret;
}

void D3D12Replay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  m_pDevice->ReplayLog(0, endEventID, replayType);
}

const SDFile &D3D12Replay::GetStructuredFile()
{
  return m_pDevice->GetStructuredFile();
}

ResourceDescription &D3D12Replay::GetResourceDesc(ResourceId id)
{
  auto it = m_ResourceIdx.find(id);
  if(it == m_ResourceIdx.end())
  {
    m_ResourceIdx[id] = m_Resources.size();
    m_Resources.push_back(ResourceDescription());
    m_Resources.back().resourceId = id;
    return m_Resources.back();
  }

  return m_Resources[it->second];
}

const std::vector<ResourceDescription> &D3D12Replay::GetResources()
{
  return m_Resources;
}

std::vector<ResourceId> D3D12Replay::GetBuffers()
{
  std::vector<ResourceId> ret;

  for(auto it = WrappedID3D12Resource::GetList().begin();
      it != WrappedID3D12Resource::GetList().end(); it++)
    if(it->second->GetDesc().Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
      ret.push_back(it->first);

  return ret;
}

std::vector<ResourceId> D3D12Replay::GetTextures()
{
  std::vector<ResourceId> ret;

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
  BufferDescription ret = {};
  ret.resourceId = m_pDevice->GetResourceManager()->GetOriginalID(id);

  auto it = WrappedID3D12Resource::GetList().find(id);

  if(it == WrappedID3D12Resource::GetList().end())
    return ret;

  D3D12_RESOURCE_DESC desc = it->second->GetDesc();

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
  TextureDescription ret = {};
  ret.resourceId = m_pDevice->GetResourceManager()->GetOriginalID(id);

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
      ret.type = ret.arraysize > 1 ? TextureType::Texture1DArray : TextureType::Texture1D;
      break;
    case 2:
      if(ret.msSamp > 1)
        ret.type = ret.arraysize > 1 ? TextureType::Texture2DMSArray : TextureType::Texture2DMS;
      else
        ret.type = ret.arraysize > 1 ? TextureType::Texture2DArray : TextureType::Texture2D;
      break;
    case 3: ret.type = TextureType::Texture3D; break;
  }

  ret.cubemap = m_pDevice->IsCubemap(id);

  ret.creationFlags = TextureCategory::ShaderRead;

  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    ret.creationFlags |= TextureCategory::ColorTarget;
  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    ret.creationFlags |= TextureCategory::DepthTarget;
  if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    ret.creationFlags |= TextureCategory::ShaderReadWrite;

  if(ret.resourceId == m_pDevice->GetQueue()->GetBackbufferResourceID())
  {
    ret.format = MakeResourceFormat(GetTypedFormat(desc.Format, CompType::UNorm));
    ret.creationFlags |= TextureCategory::SwapBuffer;
  }

  return ret;
}

rdcarray<ShaderEntryPoint> D3D12Replay::GetShaderEntryPoints(ResourceId shader)
{
  WrappedID3D12Shader *sh =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12Shader>(shader);

  if(!sh)
    return {};

  ShaderReflection &ret = sh->GetDetails();

  return {{"main", ret.stage}};
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

  // DXBC is always first
  ret.insert(ret.begin(), DXBCDisassemblyTarget);

  return ret;
}

string D3D12Replay::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                      const string &target)
{
  WrappedID3D12Shader *sh =
      m_pDevice->GetResourceManager()->GetLiveAs<WrappedID3D12Shader>(refl->resourceId);

  if(!sh)
    return "; Invalid Shader Specified";

  DXBC::DXBCFile *dxbc = sh->GetDXBC();

  if(target == DXBCDisassemblyTarget || target.empty())
    return dxbc->GetDisassembly();

  return StringFormat::Fmt("; Invalid disassembly target %s", target.c_str());
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
  if(!m_pDevice->GetResourceManager()->HasLiveResource(id))
    return ResourceId();
  return m_pDevice->GetResourceManager()->GetLiveID(id);
}

vector<EventUsage> D3D12Replay::GetUsage(ResourceId id)
{
  return m_pDevice->GetQueue()->GetUsage(id);
}

void D3D12Replay::FillResourceView(D3D12Pipe::View &view, D3D12Descriptor *desc)
{
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  if(desc->GetType() == D3D12DescriptorType::Sampler || desc->GetType() == D3D12DescriptorType::CBV)
  {
    return;
  }

  view.resourceId = rm->GetOriginalID(GetResID(desc->nonsamp.resource));

  if(view.resourceId == ResourceId())
    return;

  D3D12_RESOURCE_DESC res = desc->nonsamp.resource->GetDesc();

  {
    DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;

    if(desc->GetType() == D3D12DescriptorType::RTV)
      fmt = desc->nonsamp.rtv.Format;
    else if(desc->GetType() == D3D12DescriptorType::SRV)
      fmt = desc->nonsamp.srv.Format;
    else if(desc->GetType() == D3D12DescriptorType::UAV)
      fmt = (DXGI_FORMAT)desc->nonsamp.uav.desc.Format;

    if(fmt == DXGI_FORMAT_UNKNOWN)
      fmt = res.Format;

    view.elementByteSize = fmt == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, fmt, 0);

    view.viewFormat = MakeResourceFormat(fmt);

    if(desc->GetType() == D3D12DescriptorType::RTV)
    {
      view.type = MakeTextureDim(desc->nonsamp.rtv.ViewDimension);

      if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_BUFFER)
      {
        view.firstElement = desc->nonsamp.rtv.Buffer.FirstElement;
        view.numElements = desc->nonsamp.rtv.Buffer.NumElements;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE1D)
      {
        view.firstMip = desc->nonsamp.rtv.Texture1D.MipSlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE1DARRAY)
      {
        view.numSlices = desc->nonsamp.rtv.Texture1DArray.ArraySize;
        view.firstSlice = desc->nonsamp.rtv.Texture1DArray.FirstArraySlice;
        view.firstMip = desc->nonsamp.rtv.Texture1DArray.MipSlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2D)
      {
        view.firstMip = desc->nonsamp.rtv.Texture2D.MipSlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DARRAY)
      {
        view.numSlices = desc->nonsamp.rtv.Texture2DArray.ArraySize;
        view.firstSlice = desc->nonsamp.rtv.Texture2DArray.FirstArraySlice;
        view.firstMip = desc->nonsamp.rtv.Texture2DArray.MipSlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DMS)
      {
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY)
      {
        view.numSlices = desc->nonsamp.rtv.Texture2DMSArray.ArraySize;
        view.firstSlice = desc->nonsamp.rtv.Texture2DArray.FirstArraySlice;
      }
      else if(desc->nonsamp.rtv.ViewDimension == D3D12_RTV_DIMENSION_TEXTURE3D)
      {
        view.numSlices = desc->nonsamp.rtv.Texture3D.WSize;
        view.firstSlice = desc->nonsamp.rtv.Texture3D.FirstWSlice;
        view.firstMip = desc->nonsamp.rtv.Texture3D.MipSlice;
      }
    }
    else if(desc->GetType() == D3D12DescriptorType::DSV)
    {
      view.type = MakeTextureDim(desc->nonsamp.dsv.ViewDimension);

      if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE1D)
      {
        view.firstMip = desc->nonsamp.dsv.Texture1D.MipSlice;
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE1DARRAY)
      {
        view.numSlices = desc->nonsamp.dsv.Texture1DArray.ArraySize;
        view.firstSlice = desc->nonsamp.dsv.Texture1DArray.FirstArraySlice;
        view.firstMip = desc->nonsamp.dsv.Texture1DArray.MipSlice;
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2D)
      {
        view.firstMip = desc->nonsamp.dsv.Texture2D.MipSlice;
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DARRAY)
      {
        view.numSlices = desc->nonsamp.dsv.Texture2DArray.ArraySize;
        view.firstSlice = desc->nonsamp.dsv.Texture2DArray.FirstArraySlice;
        view.firstMip = desc->nonsamp.dsv.Texture2DArray.MipSlice;
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DMS)
      {
      }
      else if(desc->nonsamp.dsv.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY)
      {
        view.numSlices = desc->nonsamp.dsv.Texture2DMSArray.ArraySize;
        view.firstSlice = desc->nonsamp.dsv.Texture2DArray.FirstArraySlice;
      }
    }
    else if(desc->GetType() == D3D12DescriptorType::SRV)
    {
      view.type = MakeTextureDim(desc->nonsamp.srv.ViewDimension);

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
        view.firstElement = desc->nonsamp.srv.Buffer.FirstElement;
        view.numElements = desc->nonsamp.srv.Buffer.NumElements;

        view.bufferFlags = MakeBufferFlags(desc->nonsamp.srv.Buffer.Flags);
        if(desc->nonsamp.srv.Buffer.StructureByteStride > 0)
          view.elementByteSize = desc->nonsamp.srv.Buffer.StructureByteStride;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE1D)
      {
        view.firstMip = desc->nonsamp.srv.Texture1D.MostDetailedMip;
        view.numMips = desc->nonsamp.srv.Texture1D.MipLevels;
        view.minLODClamp = desc->nonsamp.srv.Texture1D.ResourceMinLODClamp;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE1DARRAY)
      {
        view.numSlices = desc->nonsamp.srv.Texture1DArray.ArraySize;
        view.firstSlice = desc->nonsamp.srv.Texture1DArray.FirstArraySlice;
        view.firstMip = desc->nonsamp.srv.Texture1DArray.MostDetailedMip;
        view.numMips = desc->nonsamp.srv.Texture1DArray.MipLevels;
        view.minLODClamp = desc->nonsamp.srv.Texture1DArray.ResourceMinLODClamp;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D)
      {
        view.firstMip = desc->nonsamp.srv.Texture2D.MostDetailedMip;
        view.numMips = desc->nonsamp.srv.Texture2D.MipLevels;
        view.minLODClamp = desc->nonsamp.srv.Texture2D.ResourceMinLODClamp;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DARRAY)
      {
        view.numSlices = desc->nonsamp.srv.Texture2DArray.ArraySize;
        view.firstSlice = desc->nonsamp.srv.Texture2DArray.FirstArraySlice;
        view.firstMip = desc->nonsamp.srv.Texture2DArray.MostDetailedMip;
        view.numMips = desc->nonsamp.srv.Texture2DArray.MipLevels;
        view.minLODClamp = desc->nonsamp.srv.Texture2DArray.ResourceMinLODClamp;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMS)
      {
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY)
      {
        view.numSlices = desc->nonsamp.srv.Texture2DMSArray.ArraySize;
        view.firstSlice = desc->nonsamp.srv.Texture2DMSArray.FirstArraySlice;
      }
      else if(desc->nonsamp.srv.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE3D)
      {
        view.firstMip = desc->nonsamp.srv.Texture3D.MostDetailedMip;
        view.numMips = desc->nonsamp.srv.Texture3D.MipLevels;
        view.minLODClamp = desc->nonsamp.srv.Texture3D.ResourceMinLODClamp;
      }
    }
    else if(desc->GetType() == D3D12DescriptorType::UAV)
    {
      D3D12_UNORDERED_ACCESS_VIEW_DESC uav = desc->nonsamp.uav.desc.AsDesc();

      view.counterResourceId = rm->GetOriginalID(GetResID(desc->nonsamp.uav.counterResource));

      view.type = MakeTextureDim(uav.ViewDimension);

      if(uav.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
      {
        view.firstElement = uav.Buffer.FirstElement;
        view.numElements = uav.Buffer.NumElements;

        view.bufferFlags = MakeBufferFlags(uav.Buffer.Flags);
        if(uav.Buffer.StructureByteStride > 0)
          view.elementByteSize = uav.Buffer.StructureByteStride;

        view.counterByteOffset = uav.Buffer.CounterOffsetInBytes;

        if(view.counterResourceId != ResourceId())
        {
          bytebuf counterVal;
          m_pDevice->GetDebugManager()->GetBufferData(desc->nonsamp.uav.counterResource,
                                                      view.counterByteOffset, 4, counterVal);
          uint32_t *val = (uint32_t *)&counterVal[0];
          view.bufferStructCount = *val;
        }
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE1D)
      {
        view.firstMip = uav.Texture1D.MipSlice;
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE1DARRAY)
      {
        view.numSlices = uav.Texture1DArray.ArraySize;
        view.firstSlice = uav.Texture1DArray.FirstArraySlice;
        view.firstMip = uav.Texture1DArray.MipSlice;
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2D)
      {
        view.firstMip = uav.Texture2D.MipSlice;
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2DARRAY)
      {
        view.numSlices = uav.Texture2DArray.ArraySize;
        view.firstSlice = uav.Texture2DArray.FirstArraySlice;
        view.firstMip = uav.Texture2DArray.MipSlice;
      }
      else if(uav.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE3D)
      {
        view.numSlices = uav.Texture3D.WSize;
        view.firstSlice = uav.Texture3D.FirstWSlice;
        view.firstMip = uav.Texture3D.MipSlice;
      }
    }
  }
}

void D3D12Replay::FillRegisterSpaces(const D3D12RenderState::RootSignature &rootSig,
                                     rdcarray<D3D12Pipe::RegisterSpace> &dstSpaces,
                                     D3D12_SHADER_VISIBILITY visibility)
{
  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  WrappedID3D12RootSignature *sig =
      m_pDevice->GetResourceManager()->GetCurrentAs<WrappedID3D12RootSignature>(rootSig.rootsig);

  dstSpaces.resize(sig->sig.numSpaces);
  D3D12Pipe::RegisterSpace *spaces = dstSpaces.data();

  for(size_t rootEl = 0; rootEl < sig->sig.params.size(); rootEl++)
  {
    const D3D12RootSignatureParameter &p = sig->sig.params[rootEl];

    if(p.ShaderVisibility != D3D12_SHADER_VISIBILITY_ALL && p.ShaderVisibility != visibility)
      continue;

    if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS)
    {
      D3D12Pipe::ConstantBuffer &cb = resize_and_add(
          spaces[p.Constants.RegisterSpace].constantBuffers, p.Constants.ShaderRegister);
      cb.immediate = true;
      cb.rootElement = (uint32_t)rootEl;
      cb.byteSize = uint32_t(sizeof(uint32_t) * p.Constants.Num32BitValues);

      if(rootEl < rootSig.sigelems.size())
      {
        const D3D12RenderState::SignatureElement &e = rootSig.sigelems[rootEl];
        if(e.type == eRootConst)
          cb.rootValues.assign(e.constants.data(),
                               RDCMIN(e.constants.size(), (size_t)p.Constants.Num32BitValues));
      }

      if(cb.rootValues.empty())
        cb.rootValues.resize(p.Constants.Num32BitValues);
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_CBV)
    {
      D3D12Pipe::ConstantBuffer &cb = resize_and_add(
          spaces[p.Descriptor.RegisterSpace].constantBuffers, p.Descriptor.ShaderRegister);
      cb.immediate = true;
      cb.rootElement = (uint32_t)rootEl;

      if(rootEl < rootSig.sigelems.size())
      {
        const D3D12RenderState::SignatureElement &e = rootSig.sigelems[rootEl];
        if(e.type == eRootCBV)
        {
          ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(e.id);

          cb.resourceId = rm->GetOriginalID(e.id);
          cb.byteOffset = e.offset;
          cb.byteSize = uint32_t(res->GetDesc().Width - cb.byteOffset);
        }
      }
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_SRV)
    {
      D3D12Pipe::View &view =
          resize_and_add(spaces[p.Descriptor.RegisterSpace].srvs, p.Descriptor.ShaderRegister);
      view.immediate = true;
      view.rootElement = (uint32_t)rootEl;

      if(rootEl < rootSig.sigelems.size())
      {
        const D3D12RenderState::SignatureElement &e = rootSig.sigelems[rootEl];
        if(e.type == eRootSRV)
        {
          ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(e.id);

          // parameters from resource/view
          view.resourceId = rm->GetOriginalID(e.id);
          view.type = TextureType::Buffer;
          view.viewFormat = MakeResourceFormat(DXGI_FORMAT_R32_UINT);

          view.elementByteSize = sizeof(uint32_t);
          view.firstElement = e.offset / sizeof(uint32_t);
          view.numElements = uint32_t((res->GetDesc().Width - e.offset) / sizeof(uint32_t));
        }
      }
    }
    else if(p.ParameterType == D3D12_ROOT_PARAMETER_TYPE_UAV)
    {
      D3D12Pipe::View &view =
          resize_and_add(spaces[p.Descriptor.RegisterSpace].uavs, p.Descriptor.ShaderRegister);
      view.immediate = true;
      view.rootElement = (uint32_t)rootEl;

      if(rootEl < rootSig.sigelems.size())
      {
        const D3D12RenderState::SignatureElement &e = rootSig.sigelems[rootEl];
        if(e.type == eRootUAV)
        {
          ID3D12Resource *res = rm->GetCurrentAs<ID3D12Resource>(e.id);

          // parameters from resource/view
          view.resourceId = rm->GetOriginalID(e.id);
          view.type = TextureType::Buffer;
          view.viewFormat = MakeResourceFormat(DXGI_FORMAT_R32_UINT);

          view.elementByteSize = sizeof(uint32_t);
          view.firstElement = e.offset / sizeof(uint32_t);
          view.numElements = uint32_t((res->GetDesc().Width - e.offset) / sizeof(uint32_t));
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
            samp.immediate = false;
            samp.rootElement = (uint32_t)rootEl;
            samp.tableIndex = offset + i;

            if(desc)
            {
              D3D12_SAMPLER_DESC &sampDesc = desc->samp.desc;

              samp.addressU = MakeAddressMode(sampDesc.AddressU);
              samp.addressV = MakeAddressMode(sampDesc.AddressV);
              samp.addressW = MakeAddressMode(sampDesc.AddressW);

              memcpy(samp.borderColor, sampDesc.BorderColor, sizeof(FLOAT) * 4);

              samp.compareFunction = MakeCompareFunc(sampDesc.ComparisonFunc);
              samp.filter = MakeFilter(sampDesc.Filter);
              samp.maxAnisotropy = 0;
              if(samp.filter.minify == FilterMode::Anisotropic)
                samp.maxAnisotropy = sampDesc.MaxAnisotropy;
              samp.maxLOD = sampDesc.MaxLOD;
              samp.minLOD = sampDesc.MinLOD;
              samp.mipLODBias = sampDesc.MipLODBias;

              desc++;
            }
          }
        }
        else if(range.RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_CBV)
        {
          UINT maxReg = shaderReg + num - 1;
          if(maxReg >= spaces[regSpace].constantBuffers.size())
            spaces[regSpace].constantBuffers.resize(maxReg + 1);

          for(UINT i = 0; i < num; i++, shaderReg++)
          {
            D3D12Pipe::ConstantBuffer &cb = spaces[regSpace].constantBuffers[shaderReg];
            cb.immediate = false;
            cb.rootElement = (uint32_t)rootEl;
            cb.tableIndex = offset + i;

            if(desc)
            {
              WrappedID3D12Resource::GetResIDFromAddr(desc->nonsamp.cbv.BufferLocation,
                                                      cb.resourceId, cb.byteOffset);
              cb.resourceId = rm->GetOriginalID(cb.resourceId);
              cb.byteSize = desc->nonsamp.cbv.SizeInBytes;

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
            view.immediate = false;
            view.rootElement = (uint32_t)rootEl;
            view.tableIndex = offset + i;

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
            view.immediate = false;
            view.rootElement = (uint32_t)rootEl;
            view.tableIndex = offset + i;

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
    samp.immediate = true;
    samp.rootElement = (uint32_t)i;

    samp.addressU = MakeAddressMode(sampDesc.AddressU);
    samp.addressV = MakeAddressMode(sampDesc.AddressV);
    samp.addressW = MakeAddressMode(sampDesc.AddressW);

    if(sampDesc.BorderColor == D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK)
    {
      samp.borderColor[0] = 0.0f;
      samp.borderColor[1] = 0.0f;
      samp.borderColor[2] = 0.0f;
      samp.borderColor[3] = 0.0f;
    }
    else if(sampDesc.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK)
    {
      samp.borderColor[0] = 0.0f;
      samp.borderColor[1] = 0.0f;
      samp.borderColor[2] = 0.0f;
      samp.borderColor[3] = 1.0f;
    }
    else if(sampDesc.BorderColor == D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE)
    {
      samp.borderColor[0] = 1.0f;
      samp.borderColor[1] = 1.0f;
      samp.borderColor[2] = 1.0f;
      samp.borderColor[3] = 1.0f;
    }
    else
    {
      RDCERR("Unexpected static border colour: %u", sampDesc.BorderColor);
    }

    samp.compareFunction = MakeCompareFunc(sampDesc.ComparisonFunc);
    samp.filter = MakeFilter(sampDesc.Filter);
    samp.maxAnisotropy = 0;
    if(samp.filter.minify == FilterMode::Anisotropic)
      samp.maxAnisotropy = sampDesc.MaxAnisotropy;
    samp.maxLOD = sampDesc.MaxLOD;
    samp.minLOD = sampDesc.MinLOD;
    samp.mipLODBias = sampDesc.MipLODBias;
  }
}

void D3D12Replay::SavePipelineState()
{
  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  D3D12Pipe::State state;

  /////////////////////////////////////////////////
  // Input Assembler
  /////////////////////////////////////////////////

  D3D12ResourceManager *rm = m_pDevice->GetResourceManager();

  state.pipelineResourceId = rm->GetOriginalID(rs.pipe);

  WrappedID3D12PipelineState *pipe = NULL;

  if(rs.pipe != ResourceId())
    pipe = rm->GetCurrentAs<WrappedID3D12PipelineState>(rs.pipe);

  if(pipe && pipe->IsGraphics())
  {
    const D3D12_INPUT_ELEMENT_DESC *inputEl = pipe->graphics->InputLayout.pInputElementDescs;
    UINT numInput = pipe->graphics->InputLayout.NumElements;

    state.inputAssembly.layouts.resize(numInput);
    for(UINT i = 0; i < numInput; i++)
    {
      D3D12Pipe::Layout &l = state.inputAssembly.layouts[i];

      l.byteOffset = inputEl[i].AlignedByteOffset;
      l.format = MakeResourceFormat(inputEl[i].Format);
      l.inputSlot = inputEl[i].InputSlot;
      l.perInstance = inputEl[i].InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
      l.instanceDataStepRate = inputEl[i].InstanceDataStepRate;
      l.semanticIndex = inputEl[i].SemanticIndex;
      l.semanticName = inputEl[i].SemanticName;
    }

    state.inputAssembly.indexStripCutValue = 0;
    switch(pipe->graphics->IBStripCutValue)
    {
      case D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF:
        state.inputAssembly.indexStripCutValue = 0xFFFF;
        break;
      case D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFFFFFF:
        state.inputAssembly.indexStripCutValue = 0xFFFFFFFF;
        break;
      default: break;
    }

    state.inputAssembly.vertexBuffers.resize(rs.vbuffers.size());
    for(size_t i = 0; i < rs.vbuffers.size(); i++)
    {
      D3D12Pipe::VertexBuffer &vb = state.inputAssembly.vertexBuffers[i];

      vb.resourceId = rm->GetOriginalID(rs.vbuffers[i].buf);
      vb.byteOffset = rs.vbuffers[i].offs;
      vb.byteSize = rs.vbuffers[i].size;
      vb.byteStride = rs.vbuffers[i].stride;
    }

    state.inputAssembly.indexBuffer.resourceId = rm->GetOriginalID(rs.ibuffer.buf);
    state.inputAssembly.indexBuffer.byteOffset = rs.ibuffer.offs;
    state.inputAssembly.indexBuffer.byteSize = rs.ibuffer.size;
  }

  /////////////////////////////////////////////////
  // Shaders
  /////////////////////////////////////////////////

  if(pipe && pipe->IsCompute())
  {
    WrappedID3D12Shader *sh = (WrappedID3D12Shader *)pipe->compute->CS.pShaderBytecode;

    state.computeShader.resourceId = sh->GetResourceID();
    state.computeShader.stage = ShaderStage::Compute;
    state.computeShader.reflection = &sh->GetDetails();
    state.computeShader.bindpointMapping = sh->GetMapping();

    state.rootSignatureResourceId = rm->GetOriginalID(rs.compute.rootsig);

    if(rs.compute.rootsig != ResourceId())
      FillRegisterSpaces(rs.compute, state.computeShader.spaces, D3D12_SHADER_VISIBILITY_ALL);
  }
  else if(pipe)
  {
    D3D12Pipe::Shader *dstArr[] = {&state.vertexShader, &state.hullShader, &state.domainShader,
                                   &state.geometryShader, &state.pixelShader};

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
        dst.resourceId = sh->GetResourceID();
        dst.bindpointMapping = sh->GetMapping();
        dst.reflection = &sh->GetDetails();
      }

      if(rs.graphics.rootsig != ResourceId())
        FillRegisterSpaces(rs.graphics, dst.spaces, visibility[stage]);
    }

    state.rootSignatureResourceId = rm->GetOriginalID(rs.graphics.rootsig);
  }

  if(pipe && pipe->IsGraphics())
  {
    /////////////////////////////////////////////////
    // Stream Out
    /////////////////////////////////////////////////

    state.streamOut.outputs.resize(rs.streamouts.size());
    for(size_t s = 0; s < rs.streamouts.size(); s++)
    {
      state.streamOut.outputs[s].resourceId = rm->GetOriginalID(rs.streamouts[s].buf);
      state.streamOut.outputs[s].byteOffset = rs.streamouts[s].offs;
      state.streamOut.outputs[s].byteSize = rs.streamouts[s].size;

      state.streamOut.outputs[s].writtenCountResourceId =
          rm->GetOriginalID(rs.streamouts[s].countbuf);
      state.streamOut.outputs[s].writtenCountByteOffset = rs.streamouts[s].countoffs;
    }

    /////////////////////////////////////////////////
    // Rasterizer
    /////////////////////////////////////////////////

    state.rasterizer.sampleMask = pipe->graphics->SampleMask;

    {
      D3D12Pipe::RasterizerState &dst = state.rasterizer.state;
      D3D12_RASTERIZER_DESC &src = pipe->graphics->RasterizerState;

      dst.antialiasedLines = src.AntialiasedLineEnable == TRUE;

      dst.cullMode = CullMode::NoCull;
      if(src.CullMode == D3D12_CULL_MODE_FRONT)
        dst.cullMode = CullMode::Front;
      if(src.CullMode == D3D12_CULL_MODE_BACK)
        dst.cullMode = CullMode::Back;

      dst.fillMode = FillMode::Solid;
      if(src.FillMode == D3D12_FILL_MODE_WIREFRAME)
        dst.fillMode = FillMode::Wireframe;

      dst.depthBias = src.DepthBias;
      dst.depthBiasClamp = src.DepthBiasClamp;
      dst.depthClip = src.DepthClipEnable == TRUE;
      dst.frontCCW = src.FrontCounterClockwise == TRUE;
      dst.multisampleEnable = src.MultisampleEnable == TRUE;
      dst.slopeScaledDepthBias = src.SlopeScaledDepthBias;
      dst.forcedSampleCount = src.ForcedSampleCount;
      dst.conservativeRasterization =
          src.ConservativeRaster == D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;
    }

    state.rasterizer.scissors.resize(rs.scissors.size());
    for(size_t i = 0; i < rs.scissors.size(); i++)
      state.rasterizer.scissors[i] = Scissor(rs.scissors[i].left, rs.scissors[i].top,
                                             rs.scissors[i].right - rs.scissors[i].left,
                                             rs.scissors[i].bottom - rs.scissors[i].top);

    state.rasterizer.viewports.resize(rs.views.size());
    for(size_t i = 0; i < rs.views.size(); i++)
      state.rasterizer.viewports[i] =
          Viewport(rs.views[i].TopLeftX, rs.views[i].TopLeftY, rs.views[i].Width,
                   rs.views[i].Height, rs.views[i].MinDepth, rs.views[i].MaxDepth);

    /////////////////////////////////////////////////
    // Output Merger
    /////////////////////////////////////////////////

    state.outputMerger.renderTargets.resize(rs.rts.size());
    for(size_t i = 0; i < rs.rts.size(); i++)
    {
      D3D12Pipe::View &view = state.outputMerger.renderTargets[i];

      D3D12Descriptor *desc = rs.rtSingle ? GetWrapped(rs.rts[0]) : GetWrapped(rs.rts[i]);

      if(desc)
      {
        if(rs.rtSingle)
          desc += i;

        view.rootElement = (uint32_t)i;
        view.immediate = false;

        FillResourceView(view, desc);
      }
    }

    {
      D3D12Pipe::View &view = state.outputMerger.depthTarget;

      if(rs.dsv.ptr)
      {
        D3D12Descriptor *desc = GetWrapped(rs.dsv);

        view.rootElement = 0;
        view.immediate = false;

        FillResourceView(view, desc);
      }
    }

    memcpy(state.outputMerger.blendState.blendFactor, rs.blendFactor, sizeof(FLOAT) * 4);

    {
      D3D12_BLEND_DESC &src = pipe->graphics->BlendState;

      state.outputMerger.blendState.alphaToCoverage = src.AlphaToCoverageEnable == TRUE;
      state.outputMerger.blendState.independentBlend = src.IndependentBlendEnable == TRUE;

      state.outputMerger.blendState.blends.resize(8);
      for(size_t i = 0; i < 8; i++)
      {
        ColorBlend &blend = state.outputMerger.blendState.blends[i];

        blend.enabled = src.RenderTarget[i].BlendEnable == TRUE;

        blend.logicOperationEnabled = src.RenderTarget[i].LogicOpEnable == TRUE;
        blend.logicOperation = MakeLogicOp(src.RenderTarget[i].LogicOp);

        blend.alphaBlend.source = MakeBlendMultiplier(src.RenderTarget[i].SrcBlendAlpha, true);
        blend.alphaBlend.destination = MakeBlendMultiplier(src.RenderTarget[i].DestBlendAlpha, true);
        blend.alphaBlend.operation = MakeBlendOp(src.RenderTarget[i].BlendOpAlpha);

        blend.colorBlend.source = MakeBlendMultiplier(src.RenderTarget[i].SrcBlend, false);
        blend.colorBlend.destination = MakeBlendMultiplier(src.RenderTarget[i].DestBlend, false);
        blend.colorBlend.operation = MakeBlendOp(src.RenderTarget[i].BlendOp);

        blend.writeMask = src.RenderTarget[i].RenderTargetWriteMask;
      }
    }

    {
      D3D12_DEPTH_STENCIL_DESC &src = pipe->graphics->DepthStencilState;

      state.outputMerger.depthStencilState.depthEnable = src.DepthEnable == TRUE;
      state.outputMerger.depthStencilState.depthFunction = MakeCompareFunc(src.DepthFunc);
      state.outputMerger.depthStencilState.depthWrites =
          src.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ALL;
      state.outputMerger.depthStencilState.stencilEnable = src.StencilEnable == TRUE;

      state.outputMerger.depthStencilState.frontFace.function =
          MakeCompareFunc(src.FrontFace.StencilFunc);
      state.outputMerger.depthStencilState.frontFace.depthFailOperation =
          MakeStencilOp(src.FrontFace.StencilDepthFailOp);
      state.outputMerger.depthStencilState.frontFace.passOperation =
          MakeStencilOp(src.FrontFace.StencilPassOp);
      state.outputMerger.depthStencilState.frontFace.failOperation =
          MakeStencilOp(src.FrontFace.StencilFailOp);

      state.outputMerger.depthStencilState.backFace.function =
          MakeCompareFunc(src.BackFace.StencilFunc);
      state.outputMerger.depthStencilState.backFace.depthFailOperation =
          MakeStencilOp(src.BackFace.StencilDepthFailOp);
      state.outputMerger.depthStencilState.backFace.passOperation =
          MakeStencilOp(src.BackFace.StencilPassOp);
      state.outputMerger.depthStencilState.backFace.failOperation =
          MakeStencilOp(src.BackFace.StencilFailOp);

      // due to shared structs, this is slightly duplicated - D3D doesn't have separate states for
      // front/back.
      state.outputMerger.depthStencilState.frontFace.reference = rs.stencilRef;
      state.outputMerger.depthStencilState.frontFace.compareMask = src.StencilReadMask;
      state.outputMerger.depthStencilState.frontFace.writeMask = src.StencilWriteMask;
      state.outputMerger.depthStencilState.backFace.reference = rs.stencilRef;
      state.outputMerger.depthStencilState.backFace.compareMask = src.StencilReadMask;
      state.outputMerger.depthStencilState.backFace.writeMask = src.StencilWriteMask;
    }
  }

  // resource states
  {
    const std::map<ResourceId, SubresourceStateVector> &states = m_pDevice->GetSubresourceStates();
    state.resourceStates.resize(states.size());
    size_t i = 0;
    for(auto it = states.begin(); it != states.end(); ++it)
    {
      D3D12Pipe::ResourceData &res = state.resourceStates[i];

      res.resourceId = rm->GetOriginalID(it->first);

      res.states.resize(it->second.size());
      for(size_t l = 0; l < it->second.size(); l++)
        res.states[l].name = ToStr(it->second[l]);

      i++;
    }
  }

  m_PipelineState = state;
}

void D3D12Replay::RenderCheckerboard()
{
  m_pDevice->GetDebugManager()->RenderCheckerboard();
}

void D3D12Replay::RenderHighlightBox(float w, float h, float scale)
{
  m_pDevice->GetDebugManager()->RenderHighlightBox(w, h, scale);
}

bool D3D12Replay::RenderTexture(TextureDisplay cfg)
{
  return m_pDevice->GetDebugManager()->RenderTexture(cfg, true);
}

void D3D12Replay::RenderMesh(uint32_t eventId, const vector<MeshFormat> &secondaryDraws,
                             const MeshDisplay &cfg)
{
  return m_pDevice->GetDebugManager()->RenderMesh(eventId, secondaryDraws, cfg);
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
                                      uint32_t eventId, const vector<uint32_t> &passEvents)
{
  return m_pDevice->GetDebugManager()->RenderOverlay(texid, typeHint, overlay, eventId, passEvents);
}

bool D3D12Replay::IsRenderOutput(ResourceId id)
{
  const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;

  id = m_pDevice->GetResourceManager()->GetLiveID(id);

  for(size_t i = 0; i < rs.rts.size(); i++)
  {
    D3D12Descriptor *desc = rs.rtSingle ? GetWrapped(rs.rts[0]) : GetWrapped(rs.rts[i]);

    if(desc)
    {
      if(rs.rtSingle)
        desc += i;

      if(id == GetResID(desc->nonsamp.resource))
        return true;
    }
  }

  if(rs.dsv.ptr)
  {
    D3D12Descriptor *desc = GetWrapped(rs.dsv);

    if(id == GetResID(desc->nonsamp.resource))
      return true;
  }

  return false;
}

vector<uint32_t> D3D12Replay::GetPassEvents(uint32_t eventId)
{
  vector<uint32_t> passEvents;

  const DrawcallDescription *draw = m_pDevice->GetDrawcall(eventId);

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
      passEvents.push_back(start->eventId);

    start = m_pDevice->GetDrawcall((uint32_t)start->next);
  }

  return passEvents;
}

bool D3D12Replay::IsTextureSupported(const ResourceFormat &format)
{
  return MakeDXGIFormat(format) != DXGI_FORMAT_UNKNOWN;
}

bool D3D12Replay::NeedRemapForFetch(const ResourceFormat &format)
{
  return false;
}

void D3D12Replay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData)
{
  m_pDevice->GetDebugManager()->GetBufferData(buff, offset, len, retData);
}

void D3D12Replay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                            uint32_t mip, uint32_t sample, CompType typeHint, float pixel[4])
{
  m_pDevice->GetDebugManager()->PickPixel(texture, x, y, sliceFace, mip, sample, typeHint, pixel);
}

void D3D12Replay::FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                                       vector<ShaderVariable> &outvars, const bytebuf &data)
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

  if(cb && cbufSlot < (uint32_t)bindMap.constantBlocks.count())
  {
    // check if the data actually comes from root constants

    const D3D12RenderState &rs = m_pDevice->GetQueue()->GetCommandData()->m_RenderState;
    Bindpoint bind = bindMap.constantBlocks[cbufSlot];

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

    bytebuf rootData;

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

void D3D12Replay::InitPostVSBuffers(uint32_t eventId)
{
  m_pDevice->GetDebugManager()->InitPostVSBuffers(eventId);
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

MeshFormat D3D12Replay::GetPostVSBuffers(uint32_t eventId, uint32_t instID, MeshDataStage stage)
{
  return m_pDevice->GetDebugManager()->GetPostVSBuffers(eventId, instID, stage);
}

uint32_t D3D12Replay::PickVertex(uint32_t eventId, const MeshDisplay &cfg, uint32_t x, uint32_t y)
{
  return m_pDevice->GetDebugManager()->PickVertex(eventId, cfg, x, y);
}

uint64_t D3D12Replay::MakeOutputWindow(WindowingData window, bool depth)
{
  return m_pDevice->GetDebugManager()->MakeOutputWindow(window, depth);
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

void D3D12Replay::ClearOutputWindowColor(uint64_t id, FloatVector col)
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

vector<DebugMessage> D3D12Replay::GetDebugMessages()
{
  return m_pDevice->GetDebugMessages();
}

void D3D12Replay::BuildTargetShader(string source, string entry,
                                    const ShaderCompileFlags &compileFlags, ShaderStage type,
                                    ResourceId *id, string *errors)
{
  ShaderCompileFlags debugCompileFlags =
      DXBC::EncodeFlags(DXBC::DecodeFlags(compileFlags) | D3DCOMPILE_DEBUG);

  m_pDevice->GetDebugManager()->BuildShader(source, entry, debugCompileFlags, type, id, errors);
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

  m_pDevice->GetDebugManager()->ClearPostVSCache();
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

        if(rm->HasReplacement(pipeid))
        {
          // if there was an active replacement, remove the dependent replaced pipelines.
          ID3D12DeviceChild *replpipe = rm->GetLiveResource(pipeid);

          rm->RemoveReplacement(pipeid);

          SAFE_RELEASE(replpipe);
        }
      }
    }
  }

  m_pDevice->GetDebugManager()->ClearPostVSCache();
}

void D3D12Replay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                 const GetTextureDataParams &params, bytebuf &data)
{
  return m_pDevice->GetDebugManager()->GetTextureData(tex, arrayIdx, mip, params, data);
}

void D3D12Replay::BuildCustomShader(string source, string entry,
                                    const ShaderCompileFlags &compileFlags, ShaderStage type,
                                    ResourceId *id, string *errors)
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

ShaderDebugTrace D3D12Replay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                          uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
  return ShaderDebugTrace();
}

ShaderDebugTrace D3D12Replay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y, uint32_t sample,
                                         uint32_t primitive)
{
  return ShaderDebugTrace();
}

ShaderDebugTrace D3D12Replay::DebugThread(uint32_t eventId, const uint32_t groupid[3],
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

ReplayStatus D3D12_CreateReplayDevice(RDCFile *rdc, IReplayDriver **driver)
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

  uint64_t ver = D3D12InitParams::CurrentVersion;

  // if we have an RDCFile, open the frame capture section and serialise the init params.
  // if not, we're creating a proxy-capable device so use default-initialised init params.
  if(rdc)
  {
    int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

    if(sectionIdx < 0)
      return ReplayStatus::InternalError;

    ver = rdc->GetSectionProperties(sectionIdx).version;

    if(!D3D12InitParams::IsSupportedVersion(ver))
    {
      RDCERR("Incompatible D3D11 serialise version %llu", ver);
      return ReplayStatus::APIIncompatibleVersion;
    }

    StreamReader *reader = rdc->ReadSection(sectionIdx);

    ReadSerialiser ser(reader, Ownership::Stream);

    SystemChunk chunk = ser.ReadChunk<SystemChunk>();

    if(chunk != SystemChunk::DriverInit)
    {
      RDCERR("Expected to get a DriverInit chunk, instead got %u", chunk);
      return ReplayStatus::FileCorrupted;
    }

    SERIALISE_ELEMENT(initParams);

    if(ser.IsErrored())
    {
      RDCERR("Failed reading driver init params.");
      return ReplayStatus::FileIOFailed;
    }
  }

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
  wrappedDev->SetInitParams(initParams, ver);

  RDCLOG("Created device.");
  D3D12Replay *replay = wrappedDev->GetReplay();

  replay->SetProxy(rdc == NULL);

  *driver = (IReplayDriver *)replay;
  return ReplayStatus::Succeeded;
}

static DriverRegistration D3D12DriverRegistration(RDCDriver::D3D12, &D3D12_CreateReplayDevice);

void D3D12_ProcessStructured(RDCFile *rdc, SDFile &output)
{
  WrappedID3D12Device device(NULL, NULL);

  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    return;

  device.SetStructuredExport(rdc->GetSectionProperties(sectionIdx).version);
  ReplayStatus status = device.ReadLogInitialisation(rdc, true);

  if(status == ReplayStatus::Succeeded)
    device.GetStructuredFile().Swap(output);
}

static StructuredProcessRegistration D3D12ProcessRegistration(RDCDriver::D3D12,
                                                              &D3D12_ProcessStructured);
