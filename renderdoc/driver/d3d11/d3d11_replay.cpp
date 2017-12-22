/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "d3d11_replay.h"
#include "driver/dx/official/d3dcompiler.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "serialise/rdcfile.h"
#include "strings/string_utils.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_device.h"
#include "d3d11_renderstate.h"
#include "d3d11_resources.h"

static const char *DXBCDisassemblyTarget = "DXBC";

D3D11Replay::D3D11Replay()
{
  m_pDevice = NULL;
  m_Proxy = false;
  m_WARP = false;
}

void D3D11Replay::Shutdown()
{
  for(size_t i = 0; i < m_ProxyResources.size(); i++)
    m_ProxyResources[i]->Release();
  m_ProxyResources.clear();

  m_pDevice->Release();

  D3D11DebugManager::PostDeviceShutdownCounters();
}

TextureDescription D3D11Replay::GetTexture(ResourceId id)
{
  TextureDescription tex = {};
  tex.resourceId = ResourceId();

  auto it1D = WrappedID3D11Texture1D::m_TextureList.find(id);
  if(it1D != WrappedID3D11Texture1D::m_TextureList.end())
  {
    WrappedID3D11Texture1D *d3dtex = (WrappedID3D11Texture1D *)it1D->second.m_Texture;

    string str = GetDebugName(d3dtex);

    D3D11_TEXTURE1D_DESC desc;
    d3dtex->GetDesc(&desc);

    tex.resourceId = m_pDevice->GetResourceManager()->GetOriginalID(it1D->first);
    tex.dimension = 1;
    tex.width = desc.Width;
    tex.height = 1;
    tex.depth = 1;
    tex.cubemap = false;
    tex.format = MakeResourceFormat(desc.Format);

    tex.creationFlags = TextureCategory::NoFlags;
    if(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
      tex.creationFlags |= TextureCategory::ShaderRead;
    if(desc.BindFlags & D3D11_BIND_RENDER_TARGET)
      tex.creationFlags |= TextureCategory::ColorTarget;
    if(desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      tex.creationFlags |= TextureCategory::DepthTarget;
    if(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      tex.creationFlags |= TextureCategory::ShaderReadWrite;

    tex.mips = desc.MipLevels;

    if(desc.MipLevels == 0)
      tex.mips = CalcNumMips(desc.Width, 1, 1);

    tex.arraysize = desc.ArraySize;

    tex.type = tex.arraysize > 1 ? TextureType::Texture1DArray : TextureType::Texture1D;

    tex.msQual = 0;
    tex.msSamp = 1;

    tex.byteSize = 0;
    for(uint32_t s = 0; s < tex.mips * tex.arraysize; s++)
      tex.byteSize += GetByteSize(d3dtex, s);

    return tex;
  }

  auto it2D = WrappedID3D11Texture2D1::m_TextureList.find(id);
  if(it2D != WrappedID3D11Texture2D1::m_TextureList.end())
  {
    WrappedID3D11Texture2D1 *d3dtex = (WrappedID3D11Texture2D1 *)it2D->second.m_Texture;

    string str = GetDebugName(d3dtex);

    D3D11_TEXTURE2D_DESC desc;
    d3dtex->GetDesc(&desc);

    if(d3dtex->m_RealDescriptor)
      desc.Format = d3dtex->m_RealDescriptor->Format;

    tex.resourceId = m_pDevice->GetResourceManager()->GetOriginalID(it2D->first);
    tex.dimension = 2;
    tex.width = desc.Width;
    tex.height = desc.Height;
    tex.depth = 1;
    tex.format = MakeResourceFormat(desc.Format);

    tex.creationFlags = TextureCategory::NoFlags;
    if(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
      tex.creationFlags |= TextureCategory::ShaderRead;
    if(desc.BindFlags & D3D11_BIND_RENDER_TARGET)
      tex.creationFlags |= TextureCategory::ColorTarget;
    if(desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      tex.creationFlags |= TextureCategory::DepthTarget;
    if(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      tex.creationFlags |= TextureCategory::ShaderReadWrite;
    if(d3dtex->m_RealDescriptor)
      tex.creationFlags |= TextureCategory::SwapBuffer;

    tex.cubemap = false;
    if(desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE)
      tex.cubemap = true;

    tex.mips = desc.MipLevels;

    if(desc.MipLevels == 0)
      tex.mips = CalcNumMips(desc.Width, desc.Height, 1);

    tex.arraysize = desc.ArraySize;

    tex.msQual = desc.SampleDesc.Quality;
    tex.msSamp = RDCMAX(1U, desc.SampleDesc.Count);

    tex.type = tex.arraysize > 1 ? TextureType::Texture2DArray : TextureType::Texture2D;
    if(tex.cubemap)
      tex.type = tex.arraysize > 1 ? TextureType::TextureCubeArray : TextureType::TextureCube;
    if(tex.msSamp > 1)
      tex.type = tex.arraysize > 1 ? TextureType::Texture2DMSArray : TextureType::Texture2DMS;

    tex.byteSize = 0;
    for(uint32_t s = 0; s < tex.arraysize * tex.mips; s++)
      tex.byteSize += GetByteSize(d3dtex, s);

    return tex;
  }

  auto it3D = WrappedID3D11Texture3D1::m_TextureList.find(id);
  if(it3D != WrappedID3D11Texture3D1::m_TextureList.end())
  {
    WrappedID3D11Texture3D1 *d3dtex = (WrappedID3D11Texture3D1 *)it3D->second.m_Texture;

    string str = GetDebugName(d3dtex);

    D3D11_TEXTURE3D_DESC desc;
    d3dtex->GetDesc(&desc);

    tex.resourceId = m_pDevice->GetResourceManager()->GetOriginalID(it3D->first);
    tex.dimension = 3;
    tex.width = desc.Width;
    tex.height = desc.Height;
    tex.depth = desc.Depth;
    tex.cubemap = false;
    tex.format = MakeResourceFormat(desc.Format);

    tex.type = TextureType::Texture3D;

    tex.creationFlags = TextureCategory::NoFlags;
    if(desc.BindFlags & D3D11_BIND_SHADER_RESOURCE)
      tex.creationFlags |= TextureCategory::ShaderRead;
    if(desc.BindFlags & D3D11_BIND_RENDER_TARGET)
      tex.creationFlags |= TextureCategory::ColorTarget;
    if(desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)
      tex.creationFlags |= TextureCategory::DepthTarget;
    if(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
      tex.creationFlags |= TextureCategory::ShaderReadWrite;

    tex.mips = desc.MipLevels;

    if(desc.MipLevels == 0)
      tex.mips = CalcNumMips(desc.Width, desc.Height, desc.Depth);

    tex.msQual = 0;
    tex.msSamp = 1;

    tex.arraysize = 1;

    tex.byteSize = 0;
    for(uint32_t s = 0; s < tex.arraysize * tex.mips; s++)
      tex.byteSize += GetByteSize(d3dtex, s);

    return tex;
  }

  RDCERR("Unrecognised/unknown texture %llu", id);

  tex.byteSize = 0;
  tex.dimension = 2;
  tex.type = TextureType::Texture2D;
  tex.width = 1;
  tex.height = 1;
  tex.depth = 1;
  tex.cubemap = false;
  tex.mips = 1;
  tex.arraysize = 1;
  tex.msQual = 0;
  tex.msSamp = 1;

  return tex;
}

rdcarray<ShaderEntryPoint> D3D11Replay::GetShaderEntryPoints(ResourceId shader)
{
  auto it = WrappedShader::m_ShaderList.find(shader);

  if(it == WrappedShader::m_ShaderList.end())
    return {};

  ShaderReflection &ret = it->second->GetDetails();

  return {{"main", ret.stage}};
}

ShaderReflection *D3D11Replay::GetShader(ResourceId shader, string entryPoint)
{
  auto it = WrappedShader::m_ShaderList.find(shader);

  if(it == WrappedShader::m_ShaderList.end())
    return NULL;

  ShaderReflection &ret = it->second->GetDetails();

  return &ret;
}

vector<string> D3D11Replay::GetDisassemblyTargets()
{
  vector<string> ret;

  // DXBC is always first
  ret.insert(ret.begin(), DXBCDisassemblyTarget);

  return ret;
}

string D3D11Replay::DisassembleShader(ResourceId pipeline, const ShaderReflection *refl,
                                      const string &target)
{
  auto it =
      WrappedShader::m_ShaderList.find(m_pDevice->GetResourceManager()->GetLiveID(refl->resourceId));

  if(it == WrappedShader::m_ShaderList.end())
    return "; Invalid Shader Specified";

  DXBC::DXBCFile *dxbc = it->second->GetDXBC();

  if(target == DXBCDisassemblyTarget || target.empty())
    return dxbc->GetDisassembly();

  return StringFormat::Fmt("; Invalid disassembly target %s", target.c_str());
}

void D3D11Replay::FreeTargetResource(ResourceId id)
{
  if(m_pDevice->GetResourceManager()->HasLiveResource(id))
  {
    ID3D11DeviceChild *resource = m_pDevice->GetResourceManager()->GetLiveResource(id);

    SAFE_RELEASE(resource);
  }
}

void D3D11Replay::FreeCustomShader(ResourceId id)
{
  if(m_pDevice->GetResourceManager()->HasLiveResource(id))
  {
    ID3D11DeviceChild *resource = m_pDevice->GetResourceManager()->GetLiveResource(id);

    SAFE_RELEASE(resource);
  }
}

FrameRecord D3D11Replay::GetFrameRecord()
{
  return m_pDevice->GetFrameRecord();
}

vector<EventUsage> D3D11Replay::GetUsage(ResourceId id)
{
  return m_pDevice->GetImmediateContext()->GetUsage(id);
}

vector<DebugMessage> D3D11Replay::GetDebugMessages()
{
  return m_pDevice->GetDebugMessages();
}

APIProperties D3D11Replay::GetAPIProperties()
{
  APIProperties ret = m_pDevice->APIProps;

  ret.pipelineType = GraphicsAPI::D3D11;
  ret.localRenderer = GraphicsAPI::D3D11;
  ret.degraded = m_WARP;
  ret.shadersMutable = false;

  return ret;
}

ResourceDescription &D3D11Replay::GetResourceDesc(ResourceId id)
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

const std::vector<ResourceDescription> &D3D11Replay::GetResources()
{
  return m_Resources;
}

std::vector<ResourceId> D3D11Replay::GetBuffers()
{
  std::vector<ResourceId> ret;

  ret.reserve(WrappedID3D11Buffer::m_BufferList.size());

  for(auto it = WrappedID3D11Buffer::m_BufferList.begin();
      it != WrappedID3D11Buffer::m_BufferList.end(); ++it)
  {
    // skip buffers that aren't from the log
    if(m_pDevice->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(it->first);
  }

  return ret;
}

BufferDescription D3D11Replay::GetBuffer(ResourceId id)
{
  BufferDescription ret = {};
  ret.resourceId = ResourceId();

  auto it = WrappedID3D11Buffer::m_BufferList.find(id);

  if(it == WrappedID3D11Buffer::m_BufferList.end())
    return ret;

  WrappedID3D11Buffer *d3dbuf = it->second.m_Buffer;

  string str = GetDebugName(d3dbuf);

  ret.resourceId = m_pDevice->GetResourceManager()->GetOriginalID(it->first);

  D3D11_BUFFER_DESC desc;
  it->second.m_Buffer->GetDesc(&desc);

  ret.length = desc.ByteWidth;

  ret.creationFlags = BufferCategory::NoFlags;
  if(desc.BindFlags & D3D11_BIND_VERTEX_BUFFER)
    ret.creationFlags |= BufferCategory::Vertex;
  if(desc.BindFlags & D3D11_BIND_INDEX_BUFFER)
    ret.creationFlags |= BufferCategory::Index;
  if(desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS)
    ret.creationFlags |= BufferCategory::ReadWrite;
  if(desc.MiscFlags & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS)
    ret.creationFlags |= BufferCategory::Indirect;

  return ret;
}

std::vector<ResourceId> D3D11Replay::GetTextures()
{
  std::vector<ResourceId> ret;

  ret.reserve(WrappedID3D11Texture1D::m_TextureList.size() +
              WrappedID3D11Texture2D1::m_TextureList.size() +
              WrappedID3D11Texture3D1::m_TextureList.size());

  for(auto it = WrappedID3D11Texture1D::m_TextureList.begin();
      it != WrappedID3D11Texture1D::m_TextureList.end(); ++it)
  {
    // skip textures that aren't from the log
    if(m_pDevice->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(it->first);
  }

  for(auto it = WrappedID3D11Texture2D1::m_TextureList.begin();
      it != WrappedID3D11Texture2D1::m_TextureList.end(); ++it)
  {
    // skip textures that aren't from the log
    if(m_pDevice->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(it->first);
  }

  for(auto it = WrappedID3D11Texture3D1::m_TextureList.begin();
      it != WrappedID3D11Texture3D1::m_TextureList.end(); ++it)
  {
    // skip textures that aren't from the log
    if(m_pDevice->GetResourceManager()->GetOriginalID(it->first) == it->first)
      continue;

    ret.push_back(it->first);
  }

  return ret;
}

void D3D11Replay::SavePipelineState()
{
  D3D11RenderState *rs = m_pDevice->GetImmediateContext()->GetCurrentPipelineState();

  D3D11Pipe::State &ret = m_CurPipelineState;

  /////////////////////////////////////////////////
  // Input Assembler
  /////////////////////////////////////////////////

  D3D11ResourceManager *rm = m_pDevice->GetResourceManager();

  ret.inputAssembly.bytecode = NULL;

  if(rs->IA.Layout)
  {
    const vector<D3D11_INPUT_ELEMENT_DESC> &vec = m_pDevice->GetLayoutDesc(rs->IA.Layout);

    ResourceId layoutId = GetIDForResource(rs->IA.Layout);

    ret.inputAssembly.resourceId = rm->GetOriginalID(layoutId);
    ret.inputAssembly.bytecode = GetShader(layoutId, "");

    ret.inputAssembly.layouts.resize(vec.size());
    for(size_t i = 0; i < vec.size(); i++)
    {
      D3D11Pipe::Layout &l = ret.inputAssembly.layouts[i];

      l.byteOffset = vec[i].AlignedByteOffset;
      l.format = MakeResourceFormat(vec[i].Format);
      l.inputSlot = vec[i].InputSlot;
      l.perInstance = vec[i].InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA;
      l.instanceDataStepRate = vec[i].InstanceDataStepRate;
      l.semanticIndex = vec[i].SemanticIndex;
      l.semanticName = vec[i].SemanticName;
    }
  }

  ret.inputAssembly.vertexBuffers.resize(ARRAY_COUNT(rs->IA.VBs));
  for(size_t i = 0; i < ARRAY_COUNT(rs->IA.VBs); i++)
  {
    D3D11Pipe::VertexBuffer &vb = ret.inputAssembly.vertexBuffers[i];

    vb.resourceId = rm->GetOriginalID(GetIDForResource(rs->IA.VBs[i]));
    vb.byteOffset = rs->IA.Offsets[i];
    vb.byteStride = rs->IA.Strides[i];
  }

  ret.inputAssembly.indexBuffer.resourceId = rm->GetOriginalID(GetIDForResource(rs->IA.IndexBuffer));
  ret.inputAssembly.indexBuffer.byteOffset = rs->IA.IndexOffset;

  /////////////////////////////////////////////////
  // Shaders
  /////////////////////////////////////////////////

  {
    D3D11Pipe::Shader *dstArr[] = {&ret.vertexShader,   &ret.hullShader,  &ret.domainShader,
                                   &ret.geometryShader, &ret.pixelShader, &ret.computeShader};
    const D3D11RenderState::Shader *srcArr[] = {&rs->VS, &rs->HS, &rs->DS,
                                                &rs->GS, &rs->PS, &rs->CS};

    const char *stageNames[] = {"Vertex", "Hull", "Domain", "Geometry", "Pixel", "Compute"};

    for(size_t stage = 0; stage < 6; stage++)
    {
      D3D11Pipe::Shader &dst = *dstArr[stage];
      const D3D11RenderState::Shader &src = *srcArr[stage];

      dst.stage = (ShaderStage)stage;

      ResourceId id = GetIDForResource(src.Object);

      WrappedShader *shad = (WrappedShader *)(WrappedID3D11Shader<ID3D11VertexShader> *)src.Object;

      ShaderReflection *refl = NULL;

      if(shad != NULL)
      {
        refl = &shad->GetDetails();
        dst.bindpointMapping = shad->GetMapping();
      }

      dst.resourceId = rm->GetOriginalID(id);
      dst.reflection = refl;

      dst.constantBuffers.resize(D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT);
      for(size_t s = 0; s < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; s++)
      {
        dst.constantBuffers[s].resourceId =
            rm->GetOriginalID(GetIDForResource(src.ConstantBuffers[s]));
        dst.constantBuffers[s].vecOffset = src.CBOffsets[s];
        dst.constantBuffers[s].vecCount = src.CBCounts[s];
      }

      dst.samplers.resize(D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT);
      for(size_t s = 0; s < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; s++)
      {
        D3D11Pipe::Sampler &samp = dst.samplers[s];

        samp.resourceId = rm->GetOriginalID(GetIDForResource(src.Samplers[s]));

        if(samp.resourceId != ResourceId())
        {
          D3D11_SAMPLER_DESC desc;
          src.Samplers[s]->GetDesc(&desc);

          samp.addressU = MakeAddressMode(desc.AddressU);
          samp.addressV = MakeAddressMode(desc.AddressV);
          samp.addressW = MakeAddressMode(desc.AddressW);

          memcpy(samp.borderColor, desc.BorderColor, sizeof(FLOAT) * 4);

          samp.compareFunction = MakeCompareFunc(desc.ComparisonFunc);
          samp.filter = MakeFilter(desc.Filter);
          samp.maxAnisotropy = 0;
          if(samp.filter.mip == FilterMode::Anisotropic)
            samp.maxAnisotropy = desc.MaxAnisotropy;
          samp.maxLOD = desc.MaxLOD;
          samp.minLOD = desc.MinLOD;
          samp.mipLODBias = desc.MipLODBias;
        }
      }

      dst.srvs.resize(D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
      for(size_t s = 0; s < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; s++)
      {
        D3D11Pipe::View &view = dst.srvs[s];

        view.viewResourceId = rm->GetOriginalID(GetIDForResource(src.SRVs[s]));

        if(view.viewResourceId != ResourceId())
        {
          D3D11_SHADER_RESOURCE_VIEW_DESC desc;
          src.SRVs[s]->GetDesc(&desc);

          view.viewFormat = MakeResourceFormat(desc.Format);

          ID3D11Resource *res = NULL;
          src.SRVs[s]->GetResource(&res);

          view.structured = false;
          view.bufferStructCount = 0;

          view.elementByteSize =
              desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

          view.resourceResourceId = rm->GetOriginalID(GetIDForResource(res));

          view.type = MakeTextureDim(desc.ViewDimension);

          if(desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER)
          {
            view.firstElement = desc.Buffer.FirstElement;
            view.numElements = desc.Buffer.NumElements;

            D3D11_BUFFER_DESC bufdesc;
            ((ID3D11Buffer *)res)->GetDesc(&bufdesc);

            view.structured = bufdesc.StructureByteStride > 0 && desc.Format == DXGI_FORMAT_UNKNOWN;

            if(view.structured)
              view.elementByteSize = bufdesc.StructureByteStride;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX)
          {
            view.firstElement = desc.BufferEx.FirstElement;
            view.numElements = desc.BufferEx.NumElements;
            view.bufferFlags = D3DBufferViewFlags(desc.BufferEx.Flags);
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1D)
          {
            view.firstMip = desc.Texture1D.MostDetailedMip;
            view.numMips = desc.Texture1D.MipLevels;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1DARRAY)
          {
            view.numSlices = desc.Texture1DArray.ArraySize;
            view.firstSlice = desc.Texture1DArray.FirstArraySlice;
            view.firstMip = desc.Texture1DArray.MostDetailedMip;
            view.numMips = desc.Texture1DArray.MipLevels;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
          {
            view.firstMip = desc.Texture2D.MostDetailedMip;
            view.numMips = desc.Texture2D.MipLevels;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
          {
            view.numSlices = desc.Texture2DArray.ArraySize;
            view.firstSlice = desc.Texture2DArray.FirstArraySlice;
            view.firstMip = desc.Texture2DArray.MostDetailedMip;
            view.numMips = desc.Texture2DArray.MipLevels;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMS)
          {
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY)
          {
            view.numSlices = desc.Texture2DArray.ArraySize;
            view.firstSlice = desc.Texture2DArray.FirstArraySlice;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE3D)
          {
            view.firstMip = desc.Texture3D.MostDetailedMip;
            view.numMips = desc.Texture3D.MipLevels;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBE)
          {
            view.numSlices = 6;
            view.firstMip = desc.TextureCube.MostDetailedMip;
            view.numMips = desc.TextureCube.MipLevels;
          }
          else if(desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBEARRAY)
          {
            view.numSlices = desc.TextureCubeArray.NumCubes * 6;
            view.firstSlice = desc.TextureCubeArray.First2DArrayFace;
            view.firstMip = desc.TextureCubeArray.MostDetailedMip;
            view.numMips = desc.TextureCubeArray.MipLevels;
          }

          SAFE_RELEASE(res);
        }
        else
        {
          view.resourceResourceId = ResourceId();
        }
      }

      dst.uavs.resize(D3D11_1_UAV_SLOT_COUNT);
      for(size_t s = 0; dst.stage == ShaderStage::Compute && s < D3D11_1_UAV_SLOT_COUNT; s++)
      {
        D3D11Pipe::View &view = dst.uavs[s];

        view.viewResourceId = rm->GetOriginalID(GetIDForResource(rs->CSUAVs[s]));

        if(view.viewResourceId != ResourceId())
        {
          D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
          rs->CSUAVs[s]->GetDesc(&desc);

          ID3D11Resource *res = NULL;
          rs->CSUAVs[s]->GetResource(&res);

          view.structured = false;
          view.bufferStructCount = 0;

          view.elementByteSize =
              desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

          if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
             (desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER)))
          {
            view.bufferStructCount = m_pDevice->GetDebugManager()->GetStructCount(rs->CSUAVs[s]);
          }

          view.resourceResourceId = rm->GetOriginalID(GetIDForResource(res));

          view.viewFormat = MakeResourceFormat(desc.Format);
          view.type = MakeTextureDim(desc.ViewDimension);

          if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
          {
            view.firstElement = desc.Buffer.FirstElement;
            view.numElements = desc.Buffer.NumElements;
            view.bufferFlags = D3DBufferViewFlags(desc.Buffer.Flags);

            D3D11_BUFFER_DESC bufdesc;
            ((ID3D11Buffer *)res)->GetDesc(&bufdesc);

            view.structured = bufdesc.StructureByteStride > 0 && desc.Format == DXGI_FORMAT_UNKNOWN;

            if(view.structured)
              view.elementByteSize = bufdesc.StructureByteStride;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1D)
          {
            view.firstMip = desc.Texture1D.MipSlice;
            view.numMips = 1;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
          {
            view.numSlices = desc.Texture1DArray.ArraySize;
            view.firstSlice = desc.Texture1DArray.FirstArraySlice;
            view.firstMip = desc.Texture1DArray.MipSlice;
            view.numMips = 1;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D)
          {
            view.firstMip = desc.Texture2D.MipSlice;
            view.numMips = 1;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
          {
            view.numSlices = desc.Texture2DArray.ArraySize;
            view.firstSlice = desc.Texture2DArray.FirstArraySlice;
            view.firstMip = desc.Texture2DArray.MipSlice;
            view.numMips = 1;
          }
          else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE3D)
          {
            view.numSlices = desc.Texture3D.WSize;
            view.firstSlice = desc.Texture3D.FirstWSlice;
            view.firstMip = desc.Texture3D.MipSlice;
            view.numMips = 1;
          }

          SAFE_RELEASE(res);
        }
        else
        {
          view.resourceResourceId = ResourceId();
        }
      }

      dst.classInstances.reserve(src.NumInstances);
      for(UINT s = 0; s < src.NumInstances; s++)
      {
        D3D11_CLASS_INSTANCE_DESC desc;
        src.Instances[s]->GetDesc(&desc);

        char typeName[256] = {0};
        SIZE_T count = 255;
        src.Instances[s]->GetTypeName(typeName, &count);

        char instName[256] = {0};
        count = 255;
        src.Instances[s]->GetInstanceName(instName, &count);

        dst.classInstances.push_back(instName);
      }
    }
  }

  /////////////////////////////////////////////////
  // Stream Out
  /////////////////////////////////////////////////

  {
    ret.streamOut.outputs.resize(D3D11_SO_BUFFER_SLOT_COUNT);
    for(size_t s = 0; s < D3D11_SO_BUFFER_SLOT_COUNT; s++)
    {
      ret.streamOut.outputs[s].resourceId = rm->GetOriginalID(GetIDForResource(rs->SO.Buffers[s]));
      ret.streamOut.outputs[s].byteOffset = rs->SO.Offsets[s];
    }
  }

  /////////////////////////////////////////////////
  // Rasterizer
  /////////////////////////////////////////////////

  {
    D3D11_RASTERIZER_DESC desc;

    if(rs->RS.State)
    {
      rs->RS.State->GetDesc(&desc);

      ret.rasterizer.state.antialiasedLines = desc.AntialiasedLineEnable == TRUE;

      ret.rasterizer.state.cullMode = CullMode::NoCull;
      if(desc.CullMode == D3D11_CULL_FRONT)
        ret.rasterizer.state.cullMode = CullMode::Front;
      if(desc.CullMode == D3D11_CULL_BACK)
        ret.rasterizer.state.cullMode = CullMode::Back;

      ret.rasterizer.state.fillMode = FillMode::Solid;
      if(desc.FillMode == D3D11_FILL_WIREFRAME)
        ret.rasterizer.state.fillMode = FillMode::Wireframe;

      ret.rasterizer.state.depthBias = desc.DepthBias;
      ret.rasterizer.state.depthBiasClamp = desc.DepthBiasClamp;
      ret.rasterizer.state.depthClip = desc.DepthClipEnable == TRUE;
      ret.rasterizer.state.frontCCW = desc.FrontCounterClockwise == TRUE;
      ret.rasterizer.state.multisampleEnable = desc.MultisampleEnable == TRUE;
      ret.rasterizer.state.scissorEnable = desc.ScissorEnable == TRUE;
      ret.rasterizer.state.slopeScaledDepthBias = desc.SlopeScaledDepthBias;
      ret.rasterizer.state.forcedSampleCount = 0;

      D3D11_RASTERIZER_DESC1 desc1;
      RDCEraseEl(desc1);

      if(CanQuery<ID3D11RasterizerState1>(rs->RS.State))
      {
        ((ID3D11RasterizerState1 *)rs->RS.State)->GetDesc1(&desc1);
        ret.rasterizer.state.forcedSampleCount = desc1.ForcedSampleCount;
      }

      D3D11_RASTERIZER_DESC2 desc2;
      RDCEraseEl(desc2);

      if(CanQuery<ID3D11RasterizerState2>(rs->RS.State))
      {
        ((ID3D11RasterizerState2 *)rs->RS.State)->GetDesc2(&desc2);
        ret.rasterizer.state.conservativeRasterization =
            desc2.ConservativeRaster == D3D11_CONSERVATIVE_RASTERIZATION_MODE_ON;
      }

      ret.rasterizer.state.resourceId = rm->GetOriginalID(GetIDForResource(rs->RS.State));
    }
    else
    {
      ret.rasterizer.state.antialiasedLines = FALSE;
      ret.rasterizer.state.cullMode = CullMode::Back;
      ret.rasterizer.state.depthBias = 0;
      ret.rasterizer.state.depthBiasClamp = 0.0f;
      ret.rasterizer.state.depthClip = TRUE;
      ret.rasterizer.state.fillMode = FillMode::Solid;
      ret.rasterizer.state.frontCCW = FALSE;
      ret.rasterizer.state.multisampleEnable = FALSE;
      ret.rasterizer.state.scissorEnable = FALSE;
      ret.rasterizer.state.slopeScaledDepthBias = 0.0f;
      ret.rasterizer.state.forcedSampleCount = 0;
      ret.rasterizer.state.resourceId = ResourceId();
    }

    size_t i = 0;
    ret.rasterizer.scissors.resize(D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for(i = 0; i < rs->RS.NumScissors; i++)
      ret.rasterizer.scissors[i] = Scissor(rs->RS.Scissors[i].left, rs->RS.Scissors[i].top,
                                           rs->RS.Scissors[i].right - rs->RS.Scissors[i].left,
                                           rs->RS.Scissors[i].bottom - rs->RS.Scissors[i].top, true);

    for(; i < D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; i++)
      ret.rasterizer.scissors[i] = Scissor(0, 0, 0, 0, false);

    ret.rasterizer.viewports.resize(D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for(i = 0; i < rs->RS.NumViews; i++)
      ret.rasterizer.viewports[i] =
          Viewport(rs->RS.Viewports[i].TopLeftX, rs->RS.Viewports[i].TopLeftY,
                   rs->RS.Viewports[i].Width, rs->RS.Viewports[i].Height,
                   rs->RS.Viewports[i].MinDepth, rs->RS.Viewports[i].MaxDepth, true);

    for(; i < D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; i++)
      ret.rasterizer.viewports[i] = Viewport(0, 0, 0, 0, 0, 0, false);
  }

  /////////////////////////////////////////////////
  // Output Merger
  /////////////////////////////////////////////////

  {
    ret.outputMerger.renderTargets.resize(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
    for(size_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
      D3D11Pipe::View &view = ret.outputMerger.renderTargets[i];

      view.viewResourceId = rm->GetOriginalID(GetIDForResource(rs->OM.RenderTargets[i]));

      if(view.viewResourceId != ResourceId())
      {
        D3D11_RENDER_TARGET_VIEW_DESC desc;
        rs->OM.RenderTargets[i]->GetDesc(&desc);

        ID3D11Resource *res = NULL;
        rs->OM.RenderTargets[i]->GetResource(&res);

        view.structured = false;
        view.bufferStructCount = 0;
        view.elementByteSize =
            desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

        view.resourceResourceId = rm->GetOriginalID(GetIDForResource(res));

        view.viewFormat = MakeResourceFormat(desc.Format);
        view.type = MakeTextureDim(desc.ViewDimension);

        if(desc.ViewDimension == D3D11_RTV_DIMENSION_BUFFER)
        {
          view.firstElement = desc.Buffer.FirstElement;
          view.numElements = desc.Buffer.NumElements;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1D)
        {
          view.firstMip = desc.Texture1D.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1DARRAY)
        {
          view.numSlices = desc.Texture1DArray.ArraySize;
          view.firstSlice = desc.Texture1DArray.FirstArraySlice;
          view.firstMip = desc.Texture1DArray.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D)
        {
          view.firstMip = desc.Texture2D.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
        {
          view.numSlices = desc.Texture2DArray.ArraySize;
          view.firstSlice = desc.Texture2DArray.FirstArraySlice;
          view.firstMip = desc.Texture2DArray.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE3D)
        {
          view.numSlices = desc.Texture3D.WSize;
          view.firstSlice = desc.Texture3D.FirstWSlice;
          view.firstMip = desc.Texture3D.MipSlice;
          view.numMips = 1;
        }

        SAFE_RELEASE(res);
      }
      else
      {
        view.resourceResourceId = ResourceId();
      }
    }

    ret.outputMerger.uavStartSlot = rs->OM.UAVStartSlot;

    ret.outputMerger.uavs.resize(D3D11_1_UAV_SLOT_COUNT);
    for(size_t s = 0; s < D3D11_1_UAV_SLOT_COUNT; s++)
    {
      D3D11Pipe::View view;

      view.viewResourceId = rm->GetOriginalID(GetIDForResource(rs->OM.UAVs[s]));

      if(view.viewResourceId != ResourceId())
      {
        D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
        rs->OM.UAVs[s]->GetDesc(&desc);

        ID3D11Resource *res = NULL;
        rs->OM.UAVs[s]->GetResource(&res);

        view.structured = false;
        view.bufferStructCount = 0;
        view.elementByteSize =
            desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

        if(desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER))
        {
          view.bufferStructCount = m_pDevice->GetDebugManager()->GetStructCount(rs->OM.UAVs[s]);
        }

        view.resourceResourceId = rm->GetOriginalID(GetIDForResource(res));

        view.viewFormat = MakeResourceFormat(desc.Format);
        view.type = MakeTextureDim(desc.ViewDimension);

        if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
        {
          view.firstElement = desc.Buffer.FirstElement;
          view.numElements = desc.Buffer.NumElements;
          view.bufferFlags = D3DBufferViewFlags(desc.Buffer.Flags);

          D3D11_BUFFER_DESC bufdesc;
          ((ID3D11Buffer *)res)->GetDesc(&bufdesc);

          view.structured = bufdesc.StructureByteStride > 0 && desc.Format == DXGI_FORMAT_UNKNOWN;

          if(view.structured)
            view.elementByteSize = bufdesc.StructureByteStride;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1D)
        {
          view.firstMip = desc.Texture1D.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
        {
          view.numSlices = desc.Texture1DArray.ArraySize;
          view.firstSlice = desc.Texture1DArray.FirstArraySlice;
          view.firstMip = desc.Texture1DArray.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D)
        {
          view.firstMip = desc.Texture2D.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
        {
          view.numSlices = desc.Texture2DArray.ArraySize;
          view.firstSlice = desc.Texture2DArray.FirstArraySlice;
          view.firstMip = desc.Texture2DArray.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE3D)
        {
          view.numSlices = desc.Texture3D.WSize;
          view.firstSlice = desc.Texture3D.FirstWSlice;
          view.firstMip = desc.Texture3D.MipSlice;
          view.numMips = 1;
        }

        SAFE_RELEASE(res);
      }

      ret.outputMerger.uavs[s] = view;
    }

    {
      D3D11Pipe::View &view = ret.outputMerger.depthTarget;

      view.viewResourceId = rm->GetOriginalID(GetIDForResource(rs->OM.DepthView));

      if(view.viewResourceId != ResourceId())
      {
        D3D11_DEPTH_STENCIL_VIEW_DESC desc;
        rs->OM.DepthView->GetDesc(&desc);

        ID3D11Resource *res = NULL;
        rs->OM.DepthView->GetResource(&res);

        view.structured = false;
        view.bufferStructCount = 0;
        view.elementByteSize =
            desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

        ret.outputMerger.depthReadOnly = false;
        ret.outputMerger.stencilReadOnly = false;

        if(desc.Flags & D3D11_DSV_READ_ONLY_DEPTH)
          ret.outputMerger.depthReadOnly = true;
        if(desc.Flags & D3D11_DSV_READ_ONLY_STENCIL)
          ret.outputMerger.stencilReadOnly = true;

        view.resourceResourceId = rm->GetOriginalID(GetIDForResource(res));

        view.viewFormat = MakeResourceFormat(desc.Format);
        view.type = MakeTextureDim(desc.ViewDimension);

        if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1D)
        {
          view.firstMip = desc.Texture1D.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1DARRAY)
        {
          view.numSlices = desc.Texture1DArray.ArraySize;
          view.firstSlice = desc.Texture1DArray.FirstArraySlice;
          view.firstMip = desc.Texture1DArray.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D)
        {
          view.firstMip = desc.Texture2D.MipSlice;
          view.numMips = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
        {
          view.numSlices = desc.Texture2DArray.ArraySize;
          view.firstSlice = desc.Texture2DArray.FirstArraySlice;
          view.firstMip = desc.Texture2DArray.MipSlice;
          view.numMips = 1;
        }

        SAFE_RELEASE(res);
      }
    }

    ret.outputMerger.blendState.sampleMask = rs->OM.SampleMask;

    memcpy(ret.outputMerger.blendState.blendFactor, rs->OM.BlendFactor, sizeof(FLOAT) * 4);

    if(rs->OM.BlendState)
    {
      D3D11_BLEND_DESC desc;
      rs->OM.BlendState->GetDesc(&desc);

      ret.outputMerger.blendState.resourceId = rm->GetOriginalID(GetIDForResource(rs->OM.BlendState));

      ret.outputMerger.blendState.alphaToCoverage = desc.AlphaToCoverageEnable == TRUE;
      ret.outputMerger.blendState.independentBlend = desc.IndependentBlendEnable == TRUE;

      bool state1 = false;
      D3D11_BLEND_DESC1 desc1;
      RDCEraseEl(desc1);

      if(CanQuery<ID3D11BlendState1>(rs->OM.BlendState))
      {
        ((WrappedID3D11BlendState1 *)rs->OM.BlendState)->GetDesc1(&desc1);

        state1 = true;
      }

      ret.outputMerger.blendState.blends.resize(8);
      for(size_t i = 0; i < 8; i++)
      {
        ColorBlend &blend = ret.outputMerger.blendState.blends[i];

        blend.enabled = desc.RenderTarget[i].BlendEnable == TRUE;

        blend.logicOperationEnabled = state1 && desc1.RenderTarget[i].LogicOpEnable == TRUE;
        blend.logicOperation =
            state1 ? MakeLogicOp(desc1.RenderTarget[i].LogicOp) : LogicOperation::NoOp;

        blend.alphaBlend.source = MakeBlendMultiplier(desc.RenderTarget[i].SrcBlendAlpha, true);
        blend.alphaBlend.destination = MakeBlendMultiplier(desc.RenderTarget[i].DestBlendAlpha, true);
        blend.alphaBlend.operation = MakeBlendOp(desc.RenderTarget[i].BlendOpAlpha);

        blend.colorBlend.source = MakeBlendMultiplier(desc.RenderTarget[i].SrcBlend, false);
        blend.colorBlend.destination = MakeBlendMultiplier(desc.RenderTarget[i].DestBlend, false);
        blend.colorBlend.operation = MakeBlendOp(desc.RenderTarget[i].BlendOp);

        blend.writeMask = desc.RenderTarget[i].RenderTargetWriteMask;
      }
    }
    else
    {
      ret.outputMerger.blendState.resourceId = ResourceId();

      ret.outputMerger.blendState.alphaToCoverage = false;
      ret.outputMerger.blendState.independentBlend = false;

      ColorBlend blend;

      blend.enabled = false;

      blend.alphaBlend.source = BlendMultiplier::One;
      blend.alphaBlend.destination = BlendMultiplier::Zero;
      blend.alphaBlend.operation = BlendOperation::Add;

      blend.colorBlend.source = BlendMultiplier::One;
      blend.colorBlend.destination = BlendMultiplier::Zero;
      blend.colorBlend.operation = BlendOperation::Add;

      blend.logicOperationEnabled = false;
      blend.logicOperation = LogicOperation::NoOp;

      blend.writeMask = D3D11_COLOR_WRITE_ENABLE_ALL;

      ret.outputMerger.blendState.blends.resize(8);
      for(size_t i = 0; i < 8; i++)
        ret.outputMerger.blendState.blends[i] = blend;
    }

    if(rs->OM.DepthStencilState)
    {
      D3D11_DEPTH_STENCIL_DESC desc;
      rs->OM.DepthStencilState->GetDesc(&desc);

      ret.outputMerger.depthStencilState.depthEnable = desc.DepthEnable == TRUE;
      ret.outputMerger.depthStencilState.depthFunction = MakeCompareFunc(desc.DepthFunc);
      ret.outputMerger.depthStencilState.depthWrites =
          desc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL;
      ret.outputMerger.depthStencilState.stencilEnable = desc.StencilEnable == TRUE;
      ret.outputMerger.depthStencilState.resourceId =
          rm->GetOriginalID(GetIDForResource(rs->OM.DepthStencilState));

      ret.outputMerger.depthStencilState.frontFace.function =
          MakeCompareFunc(desc.FrontFace.StencilFunc);
      ret.outputMerger.depthStencilState.frontFace.depthFailOperation =
          MakeStencilOp(desc.FrontFace.StencilDepthFailOp);
      ret.outputMerger.depthStencilState.frontFace.passOperation =
          MakeStencilOp(desc.FrontFace.StencilPassOp);
      ret.outputMerger.depthStencilState.frontFace.failOperation =
          MakeStencilOp(desc.FrontFace.StencilFailOp);

      ret.outputMerger.depthStencilState.backFace.function =
          MakeCompareFunc(desc.BackFace.StencilFunc);
      ret.outputMerger.depthStencilState.backFace.depthFailOperation =
          MakeStencilOp(desc.BackFace.StencilDepthFailOp);
      ret.outputMerger.depthStencilState.backFace.passOperation =
          MakeStencilOp(desc.BackFace.StencilPassOp);
      ret.outputMerger.depthStencilState.backFace.failOperation =
          MakeStencilOp(desc.BackFace.StencilFailOp);

      // due to shared structs, this is slightly duplicated - D3D doesn't have separate states for
      // front/back.
      ret.outputMerger.depthStencilState.frontFace.reference = rs->OM.StencRef;
      ret.outputMerger.depthStencilState.frontFace.compareMask = desc.StencilReadMask;
      ret.outputMerger.depthStencilState.frontFace.writeMask = desc.StencilWriteMask;
      ret.outputMerger.depthStencilState.backFace.reference = rs->OM.StencRef;
      ret.outputMerger.depthStencilState.backFace.compareMask = desc.StencilReadMask;
      ret.outputMerger.depthStencilState.backFace.writeMask = desc.StencilWriteMask;
    }
    else
    {
      ret.outputMerger.depthStencilState.depthEnable = true;
      ret.outputMerger.depthStencilState.depthFunction = CompareFunction::Less;
      ret.outputMerger.depthStencilState.depthWrites = true;
      ret.outputMerger.depthStencilState.stencilEnable = false;
      ret.outputMerger.depthStencilState.resourceId = ResourceId();

      ret.outputMerger.depthStencilState.frontFace.function = CompareFunction::AlwaysTrue;
      ret.outputMerger.depthStencilState.frontFace.depthFailOperation = StencilOperation::Keep;
      ret.outputMerger.depthStencilState.frontFace.passOperation = StencilOperation::Keep;
      ret.outputMerger.depthStencilState.frontFace.failOperation = StencilOperation::Keep;

      ret.outputMerger.depthStencilState.backFace.function = CompareFunction::AlwaysTrue;
      ret.outputMerger.depthStencilState.backFace.depthFailOperation = StencilOperation::Keep;
      ret.outputMerger.depthStencilState.backFace.passOperation = StencilOperation::Keep;
      ret.outputMerger.depthStencilState.backFace.failOperation = StencilOperation::Keep;

      // due to shared structs, this is slightly duplicated - D3D doesn't have separate states for
      // front/back.
      ret.outputMerger.depthStencilState.frontFace.reference = rs->OM.StencRef;
      ret.outputMerger.depthStencilState.frontFace.compareMask = D3D11_DEFAULT_STENCIL_READ_MASK;
      ret.outputMerger.depthStencilState.frontFace.writeMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
      ret.outputMerger.depthStencilState.backFace.reference = rs->OM.StencRef;
      ret.outputMerger.depthStencilState.backFace.compareMask = D3D11_DEFAULT_STENCIL_READ_MASK;
      ret.outputMerger.depthStencilState.backFace.writeMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    }
  }

  /////////////////////////////////////////////////
  // Predication
  /////////////////////////////////////////////////

  ret.predication.resourceId = rm->GetOriginalID(GetIDForResource(rs->Predicate));
  ret.predication.value = rs->PredicateValue == TRUE ? true : false;
  ret.predication.isPassing = rs->PredicationWouldPass();
}

ReplayStatus D3D11Replay::ReadLogInitialisation(RDCFile *rdc, bool storeStructuredBuffers)
{
  return m_pDevice->ReadLogInitialisation(rdc, storeStructuredBuffers);
}

void D3D11Replay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  m_pDevice->ReplayLog(0, endEventID, replayType);
}

const SDFile &D3D11Replay::GetStructuredFile()
{
  return m_pDevice->GetStructuredFile();
}

vector<uint32_t> D3D11Replay::GetPassEvents(uint32_t eventId)
{
  vector<uint32_t> passEvents;

  const DrawcallDescription *draw = m_pDevice->GetDrawcall(eventId);

  const DrawcallDescription *start = draw;
  while(start && start->previous != 0 &&
        !(m_pDevice->GetDrawcall((uint32_t)start->previous)->flags & DrawFlags::Clear))
  {
    const DrawcallDescription *prev = m_pDevice->GetDrawcall((uint32_t)start->previous);

    if(memcmp(start->outputs, prev->outputs, sizeof(start->outputs)) ||
       start->depthOut != prev->depthOut)
      break;

    start = prev;
  }

  while(start)
  {
    if(start == draw)
      break;

    if(start->flags & DrawFlags::Drawcall)
      passEvents.push_back(start->eventId);

    start = m_pDevice->GetDrawcall((uint32_t)start->next);
  }

  return passEvents;
}

uint64_t D3D11Replay::MakeOutputWindow(WindowingSystem system, void *data, bool depth)
{
  return m_pDevice->GetDebugManager()->MakeOutputWindow(system, data, depth);
}

void D3D11Replay::DestroyOutputWindow(uint64_t id)
{
  m_pDevice->GetDebugManager()->DestroyOutputWindow(id);
}

bool D3D11Replay::CheckResizeOutputWindow(uint64_t id)
{
  return m_pDevice->GetDebugManager()->CheckResizeOutputWindow(id);
}

void D3D11Replay::GetOutputWindowDimensions(uint64_t id, int32_t &w, int32_t &h)
{
  m_pDevice->GetDebugManager()->GetOutputWindowDimensions(id, w, h);
}

void D3D11Replay::ClearOutputWindowColor(uint64_t id, FloatVector col)
{
  m_pDevice->GetDebugManager()->ClearOutputWindowColor(id, col);
}

void D3D11Replay::ClearOutputWindowDepth(uint64_t id, float depth, uint8_t stencil)
{
  m_pDevice->GetDebugManager()->ClearOutputWindowDepth(id, depth, stencil);
}

void D3D11Replay::BindOutputWindow(uint64_t id, bool depth)
{
  m_pDevice->GetDebugManager()->BindOutputWindow(id, depth);
}

bool D3D11Replay::IsOutputWindowVisible(uint64_t id)
{
  return m_pDevice->GetDebugManager()->IsOutputWindowVisible(id);
}

void D3D11Replay::FlipOutputWindow(uint64_t id)
{
  m_pDevice->GetDebugManager()->FlipOutputWindow(id);
}

void D3D11Replay::InitPostVSBuffers(uint32_t eventId)
{
  m_pDevice->GetDebugManager()->InitPostVSBuffers(eventId);
}

void D3D11Replay::InitPostVSBuffers(const vector<uint32_t> &passEvents)
{
  uint32_t prev = 0;

  // since we can always replay between drawcalls, just loop through all the events
  // doing partial replays and calling InitPostVSBuffers for each
  for(size_t i = 0; i < passEvents.size(); i++)
  {
    if(prev != passEvents[i])
    {
      m_pDevice->ReplayLog(prev, passEvents[i], eReplay_WithoutDraw);

      prev = passEvents[i];
    }

    const DrawcallDescription *d = m_pDevice->GetDrawcall(passEvents[i]);

    if(d)
      m_pDevice->GetDebugManager()->InitPostVSBuffers(passEvents[i]);
  }
}

ResourceId D3D11Replay::GetLiveID(ResourceId id)
{
  if(!m_pDevice->GetResourceManager()->HasLiveResource(id))
    return ResourceId();
  return m_pDevice->GetResourceManager()->GetLiveID(id);
}

bool D3D11Replay::GetMinMax(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                            CompType typeHint, float *minval, float *maxval)
{
  return m_pDevice->GetDebugManager()->GetMinMax(texid, sliceFace, mip, sample, typeHint, minval,
                                                 maxval);
}

bool D3D11Replay::GetHistogram(ResourceId texid, uint32_t sliceFace, uint32_t mip, uint32_t sample,
                               CompType typeHint, float minval, float maxval, bool channels[4],
                               vector<uint32_t> &histogram)
{
  return m_pDevice->GetDebugManager()->GetHistogram(texid, sliceFace, mip, sample, typeHint, minval,
                                                    maxval, channels, histogram);
}

MeshFormat D3D11Replay::GetPostVSBuffers(uint32_t eventId, uint32_t instID, MeshDataStage stage)
{
  return m_pDevice->GetDebugManager()->GetPostVSBuffers(eventId, instID, stage);
}

void D3D11Replay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, bytebuf &retData)
{
  m_pDevice->GetDebugManager()->GetBufferData(buff, offset, len, retData);
}

void D3D11Replay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                 const GetTextureDataParams &params, bytebuf &data)
{
  m_pDevice->GetDebugManager()->GetTextureData(tex, arrayIdx, mip, params, data);
}

void D3D11Replay::ReplaceResource(ResourceId from, ResourceId to)
{
  m_pDevice->GetResourceManager()->ReplaceResource(from, to);
  m_pDevice->GetDebugManager()->ClearPostVSCache();
}

void D3D11Replay::RemoveReplacement(ResourceId id)
{
  m_pDevice->GetResourceManager()->RemoveReplacement(id);
  m_pDevice->GetDebugManager()->ClearPostVSCache();
}

vector<GPUCounter> D3D11Replay::EnumerateCounters()
{
  return m_pDevice->GetDebugManager()->EnumerateCounters();
}

CounterDescription D3D11Replay::DescribeCounter(GPUCounter counterID)
{
  return m_pDevice->GetDebugManager()->DescribeCounter(counterID);
}

vector<CounterResult> D3D11Replay::FetchCounters(const vector<GPUCounter> &counters)
{
  return m_pDevice->GetDebugManager()->FetchCounters(counters);
}

void D3D11Replay::RenderMesh(uint32_t eventId, const vector<MeshFormat> &secondaryDraws,
                             const MeshDisplay &cfg)
{
  return m_pDevice->GetDebugManager()->RenderMesh(eventId, secondaryDraws, cfg);
}

void D3D11Replay::BuildTargetShader(string source, string entry,
                                    const ShaderCompileFlags &compileFlags, ShaderStage type,
                                    ResourceId *id, string *errors)
{
  ShaderCompileFlags debugCompileFlags =
      DXBC::EncodeFlags(DXBC::DecodeFlags(compileFlags) | D3DCOMPILE_DEBUG);

  m_pDevice->GetDebugManager()->BuildShader(source, entry, debugCompileFlags, type, id, errors);
}

void D3D11Replay::BuildCustomShader(string source, string entry,
                                    const ShaderCompileFlags &compileFlags, ShaderStage type,
                                    ResourceId *id, string *errors)
{
  m_pDevice->GetDebugManager()->BuildShader(source, entry, compileFlags, type, id, errors);
}

bool D3D11Replay::RenderTexture(TextureDisplay cfg)
{
  return m_pDevice->GetDebugManager()->RenderTexture(cfg, true);
}

void D3D11Replay::RenderCheckerboard()
{
  m_pDevice->GetDebugManager()->RenderCheckerboard();
}

void D3D11Replay::RenderHighlightBox(float w, float h, float scale)
{
  m_pDevice->GetDebugManager()->RenderHighlightBox(w, h, scale);
}

void D3D11Replay::FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                                       vector<ShaderVariable> &outvars, const bytebuf &data)
{
  auto it = WrappedShader::m_ShaderList.find(shader);

  if(it == WrappedShader::m_ShaderList.end())
    return;

  DXBC::DXBCFile *dxbc = it->second->GetDXBC();

  RDCASSERT(dxbc);

  if(cbufSlot < dxbc->m_CBuffers.size())
    m_pDevice->GetDebugManager()->FillCBufferVariables(dxbc->m_CBuffers[cbufSlot].variables,
                                                       outvars, false, data);
  return;
}

vector<PixelModification> D3D11Replay::PixelHistory(vector<EventUsage> events, ResourceId target,
                                                    uint32_t x, uint32_t y, uint32_t slice,
                                                    uint32_t mip, uint32_t sampleIdx,
                                                    CompType typeHint)
{
  return m_pDevice->GetDebugManager()->PixelHistory(events, target, x, y, slice, mip, sampleIdx,
                                                    typeHint);
}

ShaderDebugTrace D3D11Replay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                          uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
  return m_pDevice->GetDebugManager()->DebugVertex(eventId, vertid, instid, idx, instOffset,
                                                   vertOffset);
}

ShaderDebugTrace D3D11Replay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y, uint32_t sample,
                                         uint32_t primitive)
{
  return m_pDevice->GetDebugManager()->DebugPixel(eventId, x, y, sample, primitive);
}

ShaderDebugTrace D3D11Replay::DebugThread(uint32_t eventId, const uint32_t groupid[3],
                                          const uint32_t threadid[3])
{
  return m_pDevice->GetDebugManager()->DebugThread(eventId, groupid, threadid);
}

uint32_t D3D11Replay::PickVertex(uint32_t eventId, const MeshDisplay &cfg, uint32_t x, uint32_t y)
{
  return m_pDevice->GetDebugManager()->PickVertex(eventId, cfg, x, y);
}

void D3D11Replay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                            uint32_t mip, uint32_t sample, CompType typeHint, float pixel[4])
{
  m_pDevice->GetDebugManager()->PickPixel(texture, x, y, sliceFace, mip, sample, typeHint, pixel);
}

ResourceId D3D11Replay::RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                      uint32_t eventId, const vector<uint32_t> &passEvents)
{
  return m_pDevice->GetDebugManager()->RenderOverlay(texid, typeHint, overlay, eventId, passEvents);
}

ResourceId D3D11Replay::ApplyCustomShader(ResourceId shader, ResourceId texid, uint32_t mip,
                                          uint32_t arrayIdx, uint32_t sampleIdx, CompType typeHint)
{
  return m_pDevice->GetDebugManager()->ApplyCustomShader(shader, texid, mip, arrayIdx, sampleIdx,
                                                         typeHint);
}

bool D3D11Replay::IsRenderOutput(ResourceId id)
{
  for(size_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
  {
    if(m_CurPipelineState.outputMerger.renderTargets[i].viewResourceId == id ||
       m_CurPipelineState.outputMerger.renderTargets[i].resourceResourceId == id)
      return true;
  }

  if(m_CurPipelineState.outputMerger.depthTarget.viewResourceId == id ||
     m_CurPipelineState.outputMerger.depthTarget.resourceResourceId == id)
    return true;

  return false;
}

ResourceId D3D11Replay::CreateProxyTexture(const TextureDescription &templateTex)
{
  ResourceId ret;

  ID3D11Resource *resource = NULL;

  if(templateTex.dimension == 1)
  {
    ID3D11Texture1D *throwaway = NULL;
    D3D11_TEXTURE1D_DESC desc;

    desc.ArraySize = templateTex.arraysize;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    if(templateTex.creationFlags & TextureCategory::DepthTarget)
      desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

    desc.CPUAccessFlags = 0;
    desc.Format = MakeDXGIFormat(templateTex.format);
    desc.MipLevels = templateTex.mips;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Width = templateTex.width;

    HRESULT hr = m_pDevice->CreateTexture1D(&desc, NULL, &throwaway);
    if(FAILED(hr))
    {
      RDCERR("Failed to create 1D proxy texture");
      return ResourceId();
    }

    resource = throwaway;

    if(templateTex.creationFlags & TextureCategory::DepthTarget)
      desc.Format = GetTypelessFormat(desc.Format);

    ret = ((WrappedID3D11Texture1D *)throwaway)->GetResourceID();

    if(templateTex.creationFlags & TextureCategory::DepthTarget)
      WrappedID3D11Texture1D::m_TextureList[ret].m_Type = TEXDISPLAY_DEPTH_TARGET;
  }
  else if(templateTex.dimension == 2)
  {
    ID3D11Texture2D *throwaway = NULL;
    D3D11_TEXTURE2D_DESC desc;

    desc.ArraySize = templateTex.arraysize;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    desc.CPUAccessFlags = 0;
    desc.Format = MakeDXGIFormat(templateTex.format);
    desc.MipLevels = templateTex.mips;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Width = templateTex.width;
    desc.Height = templateTex.height;
    desc.SampleDesc.Count = RDCMAX(1U, templateTex.msSamp);
    desc.SampleDesc.Quality = templateTex.msQual;

    if(templateTex.creationFlags & TextureCategory::DepthTarget || IsDepthFormat(desc.Format))
    {
      desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
      desc.Format = GetTypelessFormat(desc.Format);
    }

    if(templateTex.cubemap)
      desc.MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;

    HRESULT hr = m_pDevice->CreateTexture2D(&desc, NULL, &throwaway);
    if(FAILED(hr))
    {
      RDCERR("Failed to create 2D proxy texture");
      return ResourceId();
    }

    resource = throwaway;

    ret = ((WrappedID3D11Texture2D1 *)throwaway)->GetResourceID();
    if(templateTex.creationFlags & TextureCategory::DepthTarget)
      WrappedID3D11Texture2D1::m_TextureList[ret].m_Type = TEXDISPLAY_DEPTH_TARGET;
  }
  else if(templateTex.dimension == 3)
  {
    ID3D11Texture3D *throwaway = NULL;
    D3D11_TEXTURE3D_DESC desc;

    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    if(templateTex.creationFlags & TextureCategory::DepthTarget)
      desc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

    desc.CPUAccessFlags = 0;
    desc.Format = MakeDXGIFormat(templateTex.format);
    desc.MipLevels = templateTex.mips;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.Width = templateTex.width;
    desc.Height = templateTex.height;
    desc.Depth = templateTex.depth;

    HRESULT hr = m_pDevice->CreateTexture3D(&desc, NULL, &throwaway);
    if(FAILED(hr))
    {
      RDCERR("Failed to create 3D proxy texture");
      return ResourceId();
    }

    resource = throwaway;

    ret = ((WrappedID3D11Texture3D1 *)throwaway)->GetResourceID();
  }
  else
  {
    RDCERR("Invalid texture dimension: %d", templateTex.dimension);
  }

  m_ProxyResources.push_back(resource);

  return ret;
}

void D3D11Replay::SetProxyTextureData(ResourceId texid, uint32_t arrayIdx, uint32_t mip, byte *data,
                                      size_t dataSize)
{
  if(texid == ResourceId())
    return;

  ID3D11DeviceContext *ctx = m_pDevice->GetImmediateContext()->GetReal();

  if(WrappedID3D11Texture1D::m_TextureList.find(texid) != WrappedID3D11Texture1D::m_TextureList.end())
  {
    WrappedID3D11Texture1D *tex =
        (WrappedID3D11Texture1D *)WrappedID3D11Texture1D::m_TextureList[texid].m_Texture;

    D3D11_TEXTURE1D_DESC desc;
    tex->GetDesc(&desc);

    uint32_t mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, 1, 1);

    if(mip >= mips || arrayIdx >= desc.ArraySize)
    {
      RDCERR("arrayIdx %d and mip %d invalid for tex", arrayIdx, mip);
      return;
    }

    uint32_t sub = arrayIdx * mips + mip;

    if(dataSize < GetByteSize(desc.Width, 1, 1, desc.Format, mip))
    {
      RDCERR("Insufficient data provided to SetProxyTextureData");
      return;
    }

    ctx->UpdateSubresource(tex->GetReal(), sub, NULL, data,
                           GetByteSize(desc.Width, 1, 1, desc.Format, mip),
                           GetByteSize(desc.Width, 1, 1, desc.Format, mip));
  }
  else if(WrappedID3D11Texture2D1::m_TextureList.find(texid) !=
          WrappedID3D11Texture2D1::m_TextureList.end())
  {
    WrappedID3D11Texture2D1 *tex =
        (WrappedID3D11Texture2D1 *)WrappedID3D11Texture2D1::m_TextureList[texid].m_Texture;

    D3D11_TEXTURE2D_DESC desc;
    tex->GetDesc(&desc);

    uint32_t mips = desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, 1);

    if(mip >= mips || arrayIdx >= desc.ArraySize)
    {
      RDCERR("arrayIdx %d and mip %d invalid for tex", arrayIdx, mip);
      return;
    }

    uint32_t sub = arrayIdx * mips + mip;

    if(dataSize < GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip))
    {
      RDCERR("Insufficient data provided to SetProxyTextureData");
      return;
    }

    ctx->UpdateSubresource(tex->GetReal(), sub, NULL, data,
                           GetByteSize(desc.Width, 1, 1, desc.Format, mip),
                           GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip));
  }
  else if(WrappedID3D11Texture3D1::m_TextureList.find(texid) !=
          WrappedID3D11Texture3D1::m_TextureList.end())
  {
    WrappedID3D11Texture3D1 *tex =
        (WrappedID3D11Texture3D1 *)WrappedID3D11Texture3D1::m_TextureList[texid].m_Texture;

    D3D11_TEXTURE3D_DESC desc;
    tex->GetDesc(&desc);

    uint32_t mips =
        desc.MipLevels ? desc.MipLevels : CalcNumMips(desc.Width, desc.Height, desc.Depth);

    if(mip >= mips)
    {
      RDCERR("arrayIdx %d and mip %d invalid for tex", arrayIdx, mip);
      return;
    }

    if(dataSize < GetByteSize(desc.Width, desc.Height, desc.Depth, desc.Format, mip))
    {
      RDCERR("Insufficient data provided to SetProxyTextureData");
      return;
    }

    ctx->UpdateSubresource(tex->GetReal(), mip, NULL, data,
                           GetByteSize(desc.Width, 1, 1, desc.Format, mip),
                           GetByteSize(desc.Width, desc.Height, 1, desc.Format, mip));
  }
  else
  {
    RDCERR("Invalid texture id passed to SetProxyTextureData");
  }
}

bool D3D11Replay::IsTextureSupported(const ResourceFormat &format)
{
  return MakeDXGIFormat(format) != DXGI_FORMAT_UNKNOWN;
}

bool D3D11Replay::NeedRemapForFetch(const ResourceFormat &format)
{
  return false;
}

ResourceId D3D11Replay::CreateProxyBuffer(const BufferDescription &templateBuf)
{
  ResourceId ret;

  ID3D11Resource *resource = NULL;

  {
    ID3D11Buffer *throwaway = NULL;
    D3D11_BUFFER_DESC desc;

    // D3D11_BIND_CONSTANT_BUFFER size must be 16-byte aligned.
    desc.ByteWidth = AlignUp16((UINT)templateBuf.length);
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.StructureByteStride = 0;

    if(templateBuf.creationFlags & BufferCategory::Indirect)
    {
      desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
      desc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    }
    if(templateBuf.creationFlags & BufferCategory::Index)
      desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    // D3D11_BIND_CONSTANT_BUFFER size must be <= 65536 on some drivers.
    if(desc.ByteWidth <= D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16)
    {
      if(templateBuf.creationFlags & BufferCategory::Constants)
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    }
    if(templateBuf.creationFlags & BufferCategory::ReadWrite)
      desc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

    HRESULT hr = m_pDevice->CreateBuffer(&desc, NULL, &throwaway);
    if(FAILED(hr))
    {
      RDCERR("Failed to create proxy buffer");
      return ResourceId();
    }

    resource = throwaway;

    ret = ((WrappedID3D11Buffer *)throwaway)->GetResourceID();
  }

  m_ProxyResources.push_back(resource);

  return ret;
}

void D3D11Replay::SetProxyBufferData(ResourceId bufid, byte *data, size_t dataSize)
{
  if(bufid == ResourceId())
    return;

  ID3D11DeviceContext *ctx = m_pDevice->GetImmediateContext()->GetReal();

  if(WrappedID3D11Buffer::m_BufferList.find(bufid) != WrappedID3D11Buffer::m_BufferList.end())
  {
    WrappedID3D11Buffer *buf =
        (WrappedID3D11Buffer *)WrappedID3D11Buffer::m_BufferList[bufid].m_Buffer;

    D3D11_BUFFER_DESC desc;
    buf->GetDesc(&desc);

    if(AlignUp16(dataSize) < desc.ByteWidth)
    {
      RDCERR("Insufficient data provided to SetProxyBufferData");
      return;
    }

    ctx->UpdateSubresource(buf->GetReal(), 0, NULL, data, (UINT)dataSize, (UINT)dataSize);
  }
  else
  {
    RDCERR("Invalid buffer id passed to SetProxyBufferData");
  }
}

ID3DDevice *GetD3D11DeviceIfAlloc(IUnknown *dev);

extern "C" HRESULT RENDERDOC_CreateWrappedD3D11DeviceAndSwapChain(
    __in_opt IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT,
    __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *, UINT FeatureLevels, UINT,
    __in_opt CONST DXGI_SWAP_CHAIN_DESC *, __out_opt IDXGISwapChain **, __out_opt ID3D11Device **,
    __out_opt D3D_FEATURE_LEVEL *, __out_opt ID3D11DeviceContext **);

ReplayStatus D3D11_CreateReplayDevice(RDCFile *rdc, IReplayDriver **driver)
{
  RDCDEBUG("Creating a D3D11 replay device");

  HMODULE lib = NULL;
  lib = LoadLibraryA("d3d11.dll");
  if(lib == NULL)
  {
    RDCERR("Failed to load d3d11.dll");
    return ReplayStatus::APIInitFailed;
  }

  lib = LoadLibraryA("d3d9.dll");
  if(lib == NULL)
  {
    RDCERR("Failed to load d3d9.dll");
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

  D3D11InitParams initParams;

  uint64_t ver = D3D11InitParams::CurrentVersion;

  WrappedIDXGISwapChain4::RegisterD3DDeviceCallback(GetD3D11DeviceIfAlloc);

  // if we have an RDCFile, open the frame capture section and serialise the init params.
  // if not, we're creating a proxy-capable device so use default-initialised init params.
  if(rdc)
  {
    int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

    if(sectionIdx < 0)
      return ReplayStatus::InternalError;

    ver = rdc->GetSectionProperties(sectionIdx).version;

    if(!D3D11InitParams::IsSupportedVersion(ver))
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

  ID3D11Device *device = NULL;

  if(initParams.SDKVersion != D3D11_SDK_VERSION)
  {
    RDCWARN(
        "Capture file used a different SDK version %lu from replay app %lu. Results may be "
        "undefined",
        initParams.SDKVersion, D3D11_SDK_VERSION);
  }

  if(initParams.DriverType == D3D_DRIVER_TYPE_UNKNOWN)
    initParams.DriverType = D3D_DRIVER_TYPE_HARDWARE;

  int i = -2;

  // force using our feature levels as we require >= 11_0 for analysis
  D3D_FEATURE_LEVEL featureLevelArray11_1[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
  UINT numFeatureLevels11_1 = ARRAY_COUNT(featureLevelArray11_1);

  D3D_FEATURE_LEVEL featureLevelArray11_0[] = {D3D_FEATURE_LEVEL_11_0};
  UINT numFeatureLevels11_0 = ARRAY_COUNT(featureLevelArray11_0);

  D3D_DRIVER_TYPE driverTypes[] = {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP,
                                   D3D_DRIVER_TYPE_REFERENCE};
  int numDrivers = ARRAY_COUNT(driverTypes);

  D3D_FEATURE_LEVEL *featureLevelArray = featureLevelArray11_1;
  UINT numFeatureLevels = numFeatureLevels11_1;
  D3D_DRIVER_TYPE driverType = initParams.DriverType;
  UINT flags = initParams.Flags;

  HRESULT hr = E_FAIL;

  D3D_FEATURE_LEVEL maxFeatureLevel = D3D_FEATURE_LEVEL_9_1;

  // check for feature level 11 support - passing NULL feature level array implicitly checks for
  // 11_0 before others
  hr = RENDERDOC_CreateWrappedD3D11DeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL,
                                                      0, D3D11_SDK_VERSION, NULL, NULL, NULL,
                                                      &maxFeatureLevel, NULL);

  bool warpFallback = false;

  if(SUCCEEDED(hr) && maxFeatureLevel < D3D_FEATURE_LEVEL_11_0)
  {
    RDCWARN(
        "Couldn't create FEATURE_LEVEL_11_0 device - RenderDoc requires FEATURE_LEVEL_11_0 "
        "availability - falling back to WARP rasterizer");
    driverTypes[0] = driverType = D3D_DRIVER_TYPE_WARP;
    warpFallback = true;
  }

  D3D11DebugManager::PreDeviceInitCounters();

  hr = E_FAIL;
  for(;;)
  {
    hr = RENDERDOC_CreateWrappedD3D11DeviceAndSwapChain(
        /*pAdapter=*/NULL, driverType, /*Software=*/NULL, flags,
        /*pFeatureLevels=*/featureLevelArray, /*nFeatureLevels=*/numFeatureLevels, D3D11_SDK_VERSION,
        /*pSwapChainDesc=*/NULL, (IDXGISwapChain **)NULL, (ID3D11Device **)&device,
        (D3D_FEATURE_LEVEL *)NULL, (ID3D11DeviceContext **)NULL);

    if(SUCCEEDED(hr))
    {
      WrappedID3D11Device *wrappedDev = (WrappedID3D11Device *)device;
      wrappedDev->SetInitParams(initParams, ver);

      RDCLOG("Created device.");
      D3D11Replay *replay = wrappedDev->GetReplay();

      replay->SetProxy(rdc == NULL, warpFallback);
      if(warpFallback)
      {
        wrappedDev->AddDebugMessage(
            MessageCategory::Initialization, MessageSeverity::High, MessageSource::RuntimeWarning,
            "Couldn't create FEATURE_LEVEL_11_0 device - RenderDoc requires FEATURE_LEVEL_11_0 "
            "availability - falling back to WARP rasterizer.\n"
            "Performance and usability will be significantly degraded.");
      }

      *driver = (IReplayDriver *)replay;
      return ReplayStatus::Succeeded;
    }

    if(i == -1)
    {
      RDCWARN("Couldn't create device with similar settings to capture.");
    }

    SAFE_RELEASE(device);

    i++;

    if(i >= numDrivers * 2)
      break;

    if(i >= 0)
      initParams.DriverType = driverTypes[i / 2];

    if(i % 2 == 0)
    {
      featureLevelArray = featureLevelArray11_1;
      numFeatureLevels = numFeatureLevels11_1;
    }
    else
    {
      featureLevelArray = featureLevelArray11_0;
      numFeatureLevels = numFeatureLevels11_0;
    }
  }

  D3D11DebugManager::PostDeviceShutdownCounters();

  RDCERR("Couldn't create any compatible d3d11 device :(.");

  return ReplayStatus::APIHardwareUnsupported;
}

static DriverRegistration D3D11DriverRegistration(RDC_D3D11, "D3D11", &D3D11_CreateReplayDevice);

void D3D11_ProcessStructured(RDCFile *rdc, SDFile &output)
{
  WrappedID3D11Device device(NULL, NULL);

  int sectionIdx = rdc->SectionIndex(SectionType::FrameCapture);

  if(sectionIdx < 0)
    return;

  device.SetStructuredExport(rdc->GetSectionProperties(sectionIdx).version);
  ReplayStatus status = device.ReadLogInitialisation(rdc, true);

  if(status == ReplayStatus::Succeeded)
    device.GetStructuredFile().Swap(output);
}

static StructuredProcessRegistration D3D11ProcessRegistration(RDC_D3D11, &D3D11_ProcessStructured);