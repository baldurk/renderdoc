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
#include "driver/ihv/amd/amd_isa.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "serialise/string_utils.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_device.h"
#include "d3d11_renderstate.h"
#include "d3d11_resources.h"

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
  TextureDescription tex;
  tex.ID = ResourceId();

  auto it1D = WrappedID3D11Texture1D::m_TextureList.find(id);
  if(it1D != WrappedID3D11Texture1D::m_TextureList.end())
  {
    WrappedID3D11Texture1D *d3dtex = (WrappedID3D11Texture1D *)it1D->second.m_Texture;

    string str = GetDebugName(d3dtex);

    D3D11_TEXTURE1D_DESC desc;
    d3dtex->GetDesc(&desc);

    tex.ID = m_pDevice->GetResourceManager()->GetOriginalID(it1D->first);
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

    tex.resType = tex.arraysize > 1 ? TextureDim::Texture1DArray : TextureDim::Texture1D;

    tex.msQual = 0;
    tex.msSamp = 1;

    tex.customName = true;

    if(str == "")
    {
      const char *suffix = "";

      if(tex.creationFlags & TextureCategory::ColorTarget)
        suffix = " RTV";
      if(tex.creationFlags & TextureCategory::DepthTarget)
        suffix = " DSV";

      tex.customName = false;

      if(tex.arraysize > 1)
        str = StringFormat::Fmt("Texture1DArray%s %llu", suffix, tex.ID);
      else
        str = StringFormat::Fmt("Texture1D%s %llu", suffix, tex.ID);
    }

    tex.name = str;

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

    tex.ID = m_pDevice->GetResourceManager()->GetOriginalID(it2D->first);
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

    tex.resType = tex.arraysize > 1 ? TextureDim::Texture2DArray : TextureDim::Texture2D;
    if(tex.cubemap)
      tex.resType = tex.arraysize > 1 ? TextureDim::TextureCubeArray : TextureDim::TextureCube;
    if(tex.msSamp > 1)
      tex.resType = tex.arraysize > 1 ? TextureDim::Texture2DMSArray : TextureDim::Texture2DMS;

    tex.customName = true;

    if(str == "")
    {
      const char *suffix = "";
      const char *ms = "";

      if(tex.msSamp > 1)
        ms = "MS";

      if(tex.creationFlags & TextureCategory::ColorTarget)
        suffix = " RTV";
      if(tex.creationFlags & TextureCategory::DepthTarget)
        suffix = " DSV";

      tex.customName = false;

      if(tex.cubemap)
      {
        if(tex.arraysize > 6)
          str = StringFormat::Fmt("TextureCube%sArray%s %llu", ms, suffix, tex.ID);
        else
          str = StringFormat::Fmt("TextureCube%s%s %llu", ms, suffix, tex.ID);
      }
      else
      {
        if(tex.arraysize > 1)
          str = StringFormat::Fmt("Texture2D%sArray%s %llu", ms, suffix, tex.ID);
        else
          str = StringFormat::Fmt("Texture2D%s%s %llu", ms, suffix, tex.ID);
      }
    }

    tex.name = str;

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

    tex.ID = m_pDevice->GetResourceManager()->GetOriginalID(it3D->first);
    tex.dimension = 3;
    tex.width = desc.Width;
    tex.height = desc.Height;
    tex.depth = desc.Depth;
    tex.cubemap = false;
    tex.format = MakeResourceFormat(desc.Format);

    tex.resType = TextureDim::Texture3D;

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

    tex.customName = true;

    if(str == "")
    {
      const char *suffix = "";

      if(tex.creationFlags & TextureCategory::ColorTarget)
        suffix = " RTV";
      if(tex.creationFlags & TextureCategory::DepthTarget)
        suffix = " DSV";

      tex.customName = false;

      str = StringFormat::Fmt("Texture3D%s %llu", suffix, tex.ID);
    }

    tex.name = str;

    tex.byteSize = 0;
    for(uint32_t s = 0; s < tex.arraysize * tex.mips; s++)
      tex.byteSize += GetByteSize(d3dtex, s);

    return tex;
  }

  RDCERR("Unrecognised/unknown texture %llu", id);

  tex.name = "Unrecognised/unknown texture";
  tex.customName = true;
  tex.byteSize = 0;
  tex.dimension = 2;
  tex.resType = TextureDim::Texture2D;
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

ShaderReflection *D3D11Replay::GetShader(ResourceId shader, string entryPoint)
{
  auto it = WrappedShader::m_ShaderList.find(shader);

  if(it == WrappedShader::m_ShaderList.end())
    return NULL;

  ShaderReflection *ret = it->second->GetDetails();
  RDCASSERT(ret);

  return ret;
}

vector<string> D3D11Replay::GetDisassemblyTargets()
{
  vector<string> ret;

  GCNISA::GetTargets(GraphicsAPI::D3D11, ret);

  // DXBC is always first
  ret.insert(ret.begin(), "DXBC");

  return ret;
}

string D3D11Replay::DisassembleShader(const ShaderReflection *refl, const string &target)
{
  auto it = WrappedShader::m_ShaderList.find(m_pDevice->GetResourceManager()->GetLiveID(refl->ID));

  if(it == WrappedShader::m_ShaderList.end())
    return "Invalid Shader Specified";

  DXBC::DXBCFile *dxbc = it->second->GetDXBC();

  if(target == "DXBC" || target.empty())
    return dxbc->GetDisassembly();

  return GCNISA::Disassemble(dxbc, target);
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
  APIProperties ret;

  ret.pipelineType = GraphicsAPI::D3D11;
  ret.localRenderer = GraphicsAPI::D3D11;
  ret.degraded = m_WARP;

  return ret;
}

vector<ResourceId> D3D11Replay::GetBuffers()
{
  vector<ResourceId> ret;

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
  BufferDescription ret;
  ret.ID = ResourceId();

  auto it = WrappedID3D11Buffer::m_BufferList.find(id);

  if(it == WrappedID3D11Buffer::m_BufferList.end())
    return ret;

  WrappedID3D11Buffer *d3dbuf = it->second.m_Buffer;

  string str = GetDebugName(d3dbuf);

  ret.ID = m_pDevice->GetResourceManager()->GetOriginalID(it->first);
  ret.customName = true;

  if(str == "")
  {
    ret.customName = false;
    str = StringFormat::Fmt("Buffer %llu", ret.ID);
  }

  D3D11_BUFFER_DESC desc;
  it->second.m_Buffer->GetDesc(&desc);

  ret.name = str;
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

vector<ResourceId> D3D11Replay::GetTextures()
{
  vector<ResourceId> ret;

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

D3D11Pipe::State D3D11Replay::MakePipelineState()
{
  D3D11RenderState *rs = m_pDevice->GetImmediateContext()->GetCurrentPipelineState();

  D3D11Pipe::State ret;

  /////////////////////////////////////////////////
  // Input Assembler
  /////////////////////////////////////////////////

  D3D11ResourceManager *rm = m_pDevice->GetResourceManager();

  ret.m_IA.Bytecode = NULL;

  if(rs->IA.Layout)
  {
    const vector<D3D11_INPUT_ELEMENT_DESC> &vec = m_pDevice->GetLayoutDesc(rs->IA.Layout);

    ResourceId layoutId = GetIDForResource(rs->IA.Layout);

    ret.m_IA.layout = rm->GetOriginalID(layoutId);
    ret.m_IA.Bytecode = GetShader(layoutId, "");

    string str = GetDebugName(rs->IA.Layout);
    ret.m_IA.customName = true;

    if(str == "" && ret.m_IA.layout != ResourceId())
    {
      ret.m_IA.customName = false;
      str = StringFormat::Fmt("Input Layout %llu", ret.m_IA.layout);
    }

    ret.m_IA.name = str;

    create_array_uninit(ret.m_IA.layouts, vec.size());

    for(size_t i = 0; i < vec.size(); i++)
    {
      D3D11Pipe::Layout &l = ret.m_IA.layouts[i];

      l.ByteOffset = vec[i].AlignedByteOffset;
      l.Format = MakeResourceFormat(vec[i].Format);
      l.InputSlot = vec[i].InputSlot;
      l.PerInstance = vec[i].InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA;
      l.InstanceDataStepRate = vec[i].InstanceDataStepRate;
      l.SemanticIndex = vec[i].SemanticIndex;
      l.SemanticName = vec[i].SemanticName;
    }
  }

  create_array_uninit(ret.m_IA.vbuffers, ARRAY_COUNT(rs->IA.VBs));

  for(size_t i = 0; i < ARRAY_COUNT(rs->IA.VBs); i++)
  {
    D3D11Pipe::VB &vb = ret.m_IA.vbuffers[i];

    vb.Buffer = rm->GetOriginalID(GetIDForResource(rs->IA.VBs[i]));
    vb.Offset = rs->IA.Offsets[i];
    vb.Stride = rs->IA.Strides[i];
  }

  ret.m_IA.ibuffer.Buffer = rm->GetOriginalID(GetIDForResource(rs->IA.IndexBuffer));
  ret.m_IA.ibuffer.Offset = rs->IA.IndexOffset;

  /////////////////////////////////////////////////
  // Shaders
  /////////////////////////////////////////////////

  {
    D3D11Pipe::Shader *dstArr[] = {&ret.m_VS, &ret.m_HS, &ret.m_DS,
                                   &ret.m_GS, &ret.m_PS, &ret.m_CS};
    const D3D11RenderState::shader *srcArr[] = {&rs->VS, &rs->HS, &rs->DS,
                                                &rs->GS, &rs->PS, &rs->CS};

    const char *stageNames[] = {"Vertex", "Hull", "Domain", "Geometry", "Pixel", "Compute"};

    for(size_t stage = 0; stage < 6; stage++)
    {
      D3D11Pipe::Shader &dst = *dstArr[stage];
      const D3D11RenderState::shader &src = *srcArr[stage];

      dst.stage = (ShaderStage)stage;

      ResourceId id = GetIDForResource(src.Shader);

      WrappedShader *shad = (WrappedShader *)(WrappedID3D11Shader<ID3D11VertexShader> *)src.Shader;

      ShaderReflection *refl = NULL;

      if(shad != NULL)
        refl = shad->GetDetails();

      dst.Object = rm->GetOriginalID(id);
      dst.ShaderDetails = NULL;

      string str = GetDebugName(src.Shader);
      dst.customName = true;

      if(str == "" && dst.Object != ResourceId())
      {
        dst.customName = false;
        str = StringFormat::Fmt("%s Shader %llu", stageNames[stage], dst.Object);
      }

      dst.name = str;

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
        D3D11Pipe::Sampler &samp = dst.Samplers[s];

        samp.Samp = rm->GetOriginalID(GetIDForResource(src.Samplers[s]));

        if(samp.Samp != ResourceId())
        {
          samp.name = GetDebugName(src.Samplers[s]);
          samp.customName = true;

          if(samp.name.count == 0)
          {
            samp.customName = false;
            samp.name = StringFormat::Fmt("Sampler %llu", samp.Samp);
          }

          D3D11_SAMPLER_DESC desc;
          src.Samplers[s]->GetDesc(&desc);

          samp.AddressU = MakeAddressMode(desc.AddressU);
          samp.AddressV = MakeAddressMode(desc.AddressV);
          samp.AddressW = MakeAddressMode(desc.AddressW);

          memcpy(samp.BorderColor, desc.BorderColor, sizeof(FLOAT) * 4);

          samp.Comparison = MakeCompareFunc(desc.ComparisonFunc);
          samp.Filter = MakeFilter(desc.Filter);
          samp.MaxAniso = 0;
          if(samp.Filter.mip == FilterMode::Anisotropic)
            samp.MaxAniso = desc.MaxAnisotropy;
          samp.MaxLOD = desc.MaxLOD;
          samp.MinLOD = desc.MinLOD;
          samp.MipLODBias = desc.MipLODBias;
        }
      }

      create_array_uninit(dst.SRVs, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);
      for(size_t s = 0; s < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; s++)
      {
        D3D11Pipe::View &view = dst.SRVs[s];

        view.Object = rm->GetOriginalID(GetIDForResource(src.SRVs[s]));

        if(view.Object != ResourceId())
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

          view.Type = MakeTextureDim(desc.ViewDimension);

          if(desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER)
          {
            view.FirstElement = desc.Buffer.FirstElement;
            view.NumElements = desc.Buffer.NumElements;

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
            view.Flags = D3DBufferViewFlags(desc.BufferEx.Flags);
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
      for(size_t s = 0; dst.stage == ShaderStage::Compute && s < D3D11_1_UAV_SLOT_COUNT; s++)
      {
        D3D11Pipe::View &view = dst.UAVs[s];

        view.Object = rm->GetOriginalID(GetIDForResource(rs->CSUAVs[s]));

        if(view.Object != ResourceId())
        {
          D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
          rs->CSUAVs[s]->GetDesc(&desc);

          ID3D11Resource *res = NULL;
          rs->CSUAVs[s]->GetResource(&res);

          view.Structured = false;
          view.BufferStructCount = 0;

          view.ElementSize =
              desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

          if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER &&
             (desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER)))
          {
            view.BufferStructCount = m_pDevice->GetDebugManager()->GetStructCount(rs->CSUAVs[s]);
          }

          view.Resource = rm->GetOriginalID(GetIDForResource(res));

          view.Format = MakeResourceFormat(desc.Format);
          view.Type = MakeTextureDim(desc.ViewDimension);

          if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
          {
            view.FirstElement = desc.Buffer.FirstElement;
            view.NumElements = desc.Buffer.NumElements;
            view.Flags = D3DBufferViewFlags(desc.Buffer.Flags);

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

      create_array_uninit(dst.ClassInstances, src.NumInstances);
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

        dst.ClassInstances[s] = instName;
      }
    }
  }

  /////////////////////////////////////////////////
  // Stream Out
  /////////////////////////////////////////////////

  {
    create_array_uninit(ret.m_SO.Outputs, D3D11_SO_BUFFER_SLOT_COUNT);
    for(size_t s = 0; s < D3D11_SO_BUFFER_SLOT_COUNT; s++)
    {
      ret.m_SO.Outputs[s].Buffer = rm->GetOriginalID(GetIDForResource(rs->SO.Buffers[s]));
      ret.m_SO.Outputs[s].Offset = rs->SO.Offsets[s];
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

      ret.m_RS.m_State.AntialiasedLineEnable = desc.AntialiasedLineEnable == TRUE;

      ret.m_RS.m_State.cullMode = CullMode::NoCull;
      if(desc.CullMode == D3D11_CULL_FRONT)
        ret.m_RS.m_State.cullMode = CullMode::Front;
      if(desc.CullMode == D3D11_CULL_BACK)
        ret.m_RS.m_State.cullMode = CullMode::Back;

      ret.m_RS.m_State.fillMode = FillMode::Solid;
      if(desc.FillMode == D3D11_FILL_WIREFRAME)
        ret.m_RS.m_State.fillMode = FillMode::Wireframe;

      ret.m_RS.m_State.DepthBias = desc.DepthBias;
      ret.m_RS.m_State.DepthBiasClamp = desc.DepthBiasClamp;
      ret.m_RS.m_State.DepthClip = desc.DepthClipEnable == TRUE;
      ret.m_RS.m_State.FrontCCW = desc.FrontCounterClockwise == TRUE;
      ret.m_RS.m_State.MultisampleEnable = desc.MultisampleEnable == TRUE;
      ret.m_RS.m_State.ScissorEnable = desc.ScissorEnable == TRUE;
      ret.m_RS.m_State.SlopeScaledDepthBias = desc.SlopeScaledDepthBias;
      ret.m_RS.m_State.ForcedSampleCount = 0;

      D3D11_RASTERIZER_DESC1 desc1;
      RDCEraseEl(desc1);

      if(CanQuery<ID3D11RasterizerState1>(rs->RS.State))
      {
        ((ID3D11RasterizerState1 *)rs->RS.State)->GetDesc1(&desc1);
        ret.m_RS.m_State.ForcedSampleCount = desc1.ForcedSampleCount;
      }

      D3D11_RASTERIZER_DESC2 desc2;
      RDCEraseEl(desc2);

      if(CanQuery<ID3D11RasterizerState2>(rs->RS.State))
      {
        ((ID3D11RasterizerState2 *)rs->RS.State)->GetDesc2(&desc2);
        ret.m_RS.m_State.ConservativeRasterization =
            desc2.ConservativeRaster == D3D11_CONSERVATIVE_RASTERIZATION_MODE_ON;
      }

      ret.m_RS.m_State.State = rm->GetOriginalID(GetIDForResource(rs->RS.State));
    }
    else
    {
      ret.m_RS.m_State.AntialiasedLineEnable = FALSE;
      ret.m_RS.m_State.cullMode = CullMode::Back;
      ret.m_RS.m_State.DepthBias = 0;
      ret.m_RS.m_State.DepthBiasClamp = 0.0f;
      ret.m_RS.m_State.DepthClip = TRUE;
      ret.m_RS.m_State.fillMode = FillMode::Solid;
      ret.m_RS.m_State.FrontCCW = FALSE;
      ret.m_RS.m_State.MultisampleEnable = FALSE;
      ret.m_RS.m_State.ScissorEnable = FALSE;
      ret.m_RS.m_State.SlopeScaledDepthBias = 0.0f;
      ret.m_RS.m_State.ForcedSampleCount = 0;
      ret.m_RS.m_State.State = ResourceId();
    }

    size_t i = 0;
    create_array_uninit(ret.m_RS.Scissors, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for(i = 0; i < rs->RS.NumScissors; i++)
      ret.m_RS.Scissors[i] =
          D3D11Pipe::Scissor(rs->RS.Scissors[i].left, rs->RS.Scissors[i].top,
                             rs->RS.Scissors[i].right, rs->RS.Scissors[i].bottom, true);

    for(; i < D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; i++)
      ret.m_RS.Scissors[i] = D3D11Pipe::Scissor(0, 0, 0, 0, false);

    create_array_uninit(ret.m_RS.Viewports, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE);
    for(i = 0; i < rs->RS.NumViews; i++)
      ret.m_RS.Viewports[i] =
          D3D11Pipe::Viewport(rs->RS.Viewports[i].TopLeftX, rs->RS.Viewports[i].TopLeftY,
                              rs->RS.Viewports[i].Width, rs->RS.Viewports[i].Height,
                              rs->RS.Viewports[i].MinDepth, rs->RS.Viewports[i].MaxDepth, true);

    for(; i < D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE; i++)
      ret.m_RS.Viewports[i] = D3D11Pipe::Viewport(0, 0, 0, 0, 0, 0, false);
  }

  /////////////////////////////////////////////////
  // Output Merger
  /////////////////////////////////////////////////

  {
    create_array_uninit(ret.m_OM.RenderTargets, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT);
    for(size_t i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
    {
      D3D11Pipe::View &view = ret.m_OM.RenderTargets[i];

      view.Object = rm->GetOriginalID(GetIDForResource(rs->OM.RenderTargets[i]));

      if(view.Object != ResourceId())
      {
        D3D11_RENDER_TARGET_VIEW_DESC desc;
        rs->OM.RenderTargets[i]->GetDesc(&desc);

        ID3D11Resource *res = NULL;
        rs->OM.RenderTargets[i]->GetResource(&res);

        view.Structured = false;
        view.BufferStructCount = 0;
        view.ElementSize =
            desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

        view.Resource = rm->GetOriginalID(GetIDForResource(res));

        view.Format = MakeResourceFormat(desc.Format);
        view.Type = MakeTextureDim(desc.ViewDimension);

        if(desc.ViewDimension == D3D11_RTV_DIMENSION_BUFFER)
        {
          view.FirstElement = desc.Buffer.FirstElement;
          view.NumElements = desc.Buffer.NumElements;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1D)
        {
          view.HighestMip = desc.Texture1D.MipSlice;
          view.NumMipLevels = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE1DARRAY)
        {
          view.ArraySize = desc.Texture1DArray.ArraySize;
          view.FirstArraySlice = desc.Texture1DArray.FirstArraySlice;
          view.HighestMip = desc.Texture1DArray.MipSlice;
          view.NumMipLevels = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D)
        {
          view.HighestMip = desc.Texture2D.MipSlice;
          view.NumMipLevels = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY)
        {
          view.ArraySize = desc.Texture2DArray.ArraySize;
          view.FirstArraySlice = desc.Texture2DArray.FirstArraySlice;
          view.HighestMip = desc.Texture2DArray.MipSlice;
          view.NumMipLevels = 1;
        }
        else if(desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE3D)
        {
          view.ArraySize = desc.Texture3D.WSize;
          view.FirstArraySlice = desc.Texture3D.FirstWSlice;
          view.HighestMip = desc.Texture3D.MipSlice;
          view.NumMipLevels = 1;
        }

        SAFE_RELEASE(res);
      }
    }

    ret.m_OM.UAVStartSlot = rs->OM.UAVStartSlot;

    create_array_uninit(ret.m_OM.UAVs, D3D11_1_UAV_SLOT_COUNT);
    for(size_t s = 0; s < D3D11_1_UAV_SLOT_COUNT; s++)
    {
      D3D11Pipe::View view;

      view.Object = rm->GetOriginalID(GetIDForResource(rs->OM.UAVs[s]));

      if(view.Object != ResourceId())
      {
        D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
        rs->OM.UAVs[s]->GetDesc(&desc);

        ID3D11Resource *res = NULL;
        rs->OM.UAVs[s]->GetResource(&res);

        view.Structured = false;
        view.BufferStructCount = 0;
        view.ElementSize =
            desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

        if(desc.Buffer.Flags & (D3D11_BUFFER_UAV_FLAG_APPEND | D3D11_BUFFER_UAV_FLAG_COUNTER))
        {
          view.BufferStructCount = m_pDevice->GetDebugManager()->GetStructCount(rs->OM.UAVs[s]);
        }

        view.Resource = rm->GetOriginalID(GetIDForResource(res));

        view.Format = MakeResourceFormat(desc.Format);
        view.Type = MakeTextureDim(desc.ViewDimension);

        if(desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
        {
          view.FirstElement = desc.Buffer.FirstElement;
          view.NumElements = desc.Buffer.NumElements;
          view.Flags = D3DBufferViewFlags(desc.Buffer.Flags);

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

      ret.m_OM.UAVs[s] = view;
    }

    {
      D3D11Pipe::View &view = ret.m_OM.DepthTarget;

      view.Object = rm->GetOriginalID(GetIDForResource(rs->OM.DepthView));

      if(view.Object != ResourceId())
      {
        D3D11_DEPTH_STENCIL_VIEW_DESC desc;
        rs->OM.DepthView->GetDesc(&desc);

        ID3D11Resource *res = NULL;
        rs->OM.DepthView->GetResource(&res);

        view.Structured = false;
        view.BufferStructCount = 0;
        view.ElementSize =
            desc.Format == DXGI_FORMAT_UNKNOWN ? 1 : GetByteSize(1, 1, 1, desc.Format, 0);

        ret.m_OM.DepthReadOnly = false;
        ret.m_OM.StencilReadOnly = false;

        if(desc.Flags & D3D11_DSV_READ_ONLY_DEPTH)
          ret.m_OM.DepthReadOnly = true;
        if(desc.Flags & D3D11_DSV_READ_ONLY_STENCIL)
          ret.m_OM.StencilReadOnly = true;

        view.Resource = rm->GetOriginalID(GetIDForResource(res));

        view.Format = MakeResourceFormat(desc.Format);
        view.Type = MakeTextureDim(desc.ViewDimension);

        if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1D)
        {
          view.HighestMip = desc.Texture1D.MipSlice;
          view.NumMipLevels = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE1DARRAY)
        {
          view.ArraySize = desc.Texture1DArray.ArraySize;
          view.FirstArraySlice = desc.Texture1DArray.FirstArraySlice;
          view.HighestMip = desc.Texture1DArray.MipSlice;
          view.NumMipLevels = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D)
        {
          view.HighestMip = desc.Texture2D.MipSlice;
          view.NumMipLevels = 1;
        }
        else if(desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY)
        {
          view.ArraySize = desc.Texture2DArray.ArraySize;
          view.FirstArraySlice = desc.Texture2DArray.FirstArraySlice;
          view.HighestMip = desc.Texture2DArray.MipSlice;
          view.NumMipLevels = 1;
        }

        SAFE_RELEASE(res);
      }
    }

    ret.m_OM.m_BlendState.SampleMask = rs->OM.SampleMask;

    memcpy(ret.m_OM.m_BlendState.BlendFactor, rs->OM.BlendFactor, sizeof(FLOAT) * 4);

    if(rs->OM.BlendState)
    {
      D3D11_BLEND_DESC desc;
      rs->OM.BlendState->GetDesc(&desc);

      ret.m_OM.m_BlendState.State = GetIDForResource(rs->OM.BlendState);

      ret.m_OM.m_BlendState.AlphaToCoverage = desc.AlphaToCoverageEnable == TRUE;
      ret.m_OM.m_BlendState.IndependentBlend = desc.IndependentBlendEnable == TRUE;

      bool state1 = false;
      D3D11_BLEND_DESC1 desc1;
      RDCEraseEl(desc1);

      if(CanQuery<ID3D11BlendState1>(rs->OM.BlendState))
      {
        ((WrappedID3D11BlendState1 *)rs->OM.BlendState)->GetDesc1(&desc1);

        state1 = true;
      }

      create_array_uninit(ret.m_OM.m_BlendState.Blends, 8);
      for(size_t i = 0; i < 8; i++)
      {
        D3D11Pipe::Blend &blend = ret.m_OM.m_BlendState.Blends[i];

        blend.Enabled = desc.RenderTarget[i].BlendEnable == TRUE;

        blend.LogicEnabled = state1 && desc1.RenderTarget[i].LogicOpEnable == TRUE;
        blend.Logic = state1 ? MakeLogicOp(desc1.RenderTarget[i].LogicOp) : LogicOp::NoOp;

        blend.m_AlphaBlend.Source = MakeBlendMultiplier(desc.RenderTarget[i].SrcBlendAlpha, true);
        blend.m_AlphaBlend.Destination =
            MakeBlendMultiplier(desc.RenderTarget[i].DestBlendAlpha, true);
        blend.m_AlphaBlend.Operation = MakeBlendOp(desc.RenderTarget[i].BlendOpAlpha);

        blend.m_Blend.Source = MakeBlendMultiplier(desc.RenderTarget[i].SrcBlend, false);
        blend.m_Blend.Destination = MakeBlendMultiplier(desc.RenderTarget[i].DestBlend, false);
        blend.m_Blend.Operation = MakeBlendOp(desc.RenderTarget[i].BlendOp);

        blend.WriteMask = desc.RenderTarget[i].RenderTargetWriteMask;
      }
    }
    else
    {
      ret.m_OM.m_BlendState.State = ResourceId();

      ret.m_OM.m_BlendState.AlphaToCoverage = false;
      ret.m_OM.m_BlendState.IndependentBlend = false;

      D3D11Pipe::Blend blend;

      blend.Enabled = false;

      blend.m_AlphaBlend.Source = BlendMultiplier::One;
      blend.m_AlphaBlend.Destination = BlendMultiplier::Zero;
      blend.m_AlphaBlend.Operation = BlendOp::Add;

      blend.m_Blend.Source = BlendMultiplier::One;
      blend.m_Blend.Destination = BlendMultiplier::Zero;
      blend.m_Blend.Operation = BlendOp::Add;

      blend.LogicEnabled = false;
      blend.Logic = LogicOp::NoOp;

      blend.WriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

      create_array_uninit(ret.m_OM.m_BlendState.Blends, 8);
      for(size_t i = 0; i < 8; i++)
        ret.m_OM.m_BlendState.Blends[i] = blend;
    }

    if(rs->OM.DepthStencilState)
    {
      D3D11_DEPTH_STENCIL_DESC desc;
      rs->OM.DepthStencilState->GetDesc(&desc);

      ret.m_OM.m_State.DepthEnable = desc.DepthEnable == TRUE;
      ret.m_OM.m_State.DepthFunc = MakeCompareFunc(desc.DepthFunc);
      ret.m_OM.m_State.DepthWrites = desc.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL;
      ret.m_OM.m_State.StencilEnable = desc.StencilEnable == TRUE;
      ret.m_OM.m_State.StencilRef = rs->OM.StencRef;
      ret.m_OM.m_State.StencilReadMask = desc.StencilReadMask;
      ret.m_OM.m_State.StencilWriteMask = desc.StencilWriteMask;
      ret.m_OM.m_State.State = rm->GetOriginalID(GetIDForResource(rs->OM.DepthStencilState));

      ret.m_OM.m_State.m_FrontFace.Func = MakeCompareFunc(desc.FrontFace.StencilFunc);
      ret.m_OM.m_State.m_FrontFace.DepthFailOp = MakeStencilOp(desc.FrontFace.StencilDepthFailOp);
      ret.m_OM.m_State.m_FrontFace.PassOp = MakeStencilOp(desc.FrontFace.StencilPassOp);
      ret.m_OM.m_State.m_FrontFace.FailOp = MakeStencilOp(desc.FrontFace.StencilFailOp);

      ret.m_OM.m_State.m_BackFace.Func = MakeCompareFunc(desc.BackFace.StencilFunc);
      ret.m_OM.m_State.m_BackFace.DepthFailOp = MakeStencilOp(desc.BackFace.StencilDepthFailOp);
      ret.m_OM.m_State.m_BackFace.PassOp = MakeStencilOp(desc.BackFace.StencilPassOp);
      ret.m_OM.m_State.m_BackFace.FailOp = MakeStencilOp(desc.BackFace.StencilFailOp);
    }
    else
    {
      ret.m_OM.m_State.DepthEnable = true;
      ret.m_OM.m_State.DepthFunc = CompareFunc::Less;
      ret.m_OM.m_State.DepthWrites = true;
      ret.m_OM.m_State.StencilEnable = false;
      ret.m_OM.m_State.StencilRef = rs->OM.StencRef;
      ret.m_OM.m_State.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
      ret.m_OM.m_State.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
      ret.m_OM.m_State.State = ResourceId();

      ret.m_OM.m_State.m_FrontFace.Func = CompareFunc::AlwaysTrue;
      ret.m_OM.m_State.m_FrontFace.DepthFailOp = StencilOp::Keep;
      ret.m_OM.m_State.m_FrontFace.PassOp = StencilOp::Keep;
      ret.m_OM.m_State.m_FrontFace.FailOp = StencilOp::Keep;

      ret.m_OM.m_State.m_BackFace.Func = CompareFunc::AlwaysTrue;
      ret.m_OM.m_State.m_BackFace.DepthFailOp = StencilOp::Keep;
      ret.m_OM.m_State.m_BackFace.PassOp = StencilOp::Keep;
      ret.m_OM.m_State.m_BackFace.FailOp = StencilOp::Keep;
    }
  }

  return ret;
}

void D3D11Replay::ReadLogInitialisation()
{
  m_pDevice->ReadLogInitialisation();
}

void D3D11Replay::ReplayLog(uint32_t endEventID, ReplayLogType replayType)
{
  m_pDevice->ReplayLog(0, endEventID, replayType);
}

vector<uint32_t> D3D11Replay::GetPassEvents(uint32_t eventID)
{
  vector<uint32_t> passEvents;

  const DrawcallDescription *draw = m_pDevice->GetDrawcall(eventID);

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
      passEvents.push_back(start->eventID);

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

void D3D11Replay::ClearOutputWindowColor(uint64_t id, float col[4])
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

void D3D11Replay::InitPostVSBuffers(uint32_t eventID)
{
  m_pDevice->GetDebugManager()->InitPostVSBuffers(eventID);
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

MeshFormat D3D11Replay::GetPostVSBuffers(uint32_t eventID, uint32_t instID, MeshDataStage stage)
{
  return m_pDevice->GetDebugManager()->GetPostVSBuffers(eventID, instID, stage);
}

void D3D11Replay::GetBufferData(ResourceId buff, uint64_t offset, uint64_t len, vector<byte> &retData)
{
  m_pDevice->GetDebugManager()->GetBufferData(buff, offset, len, retData);
}

byte *D3D11Replay::GetTextureData(ResourceId tex, uint32_t arrayIdx, uint32_t mip,
                                  const GetTextureDataParams &params, size_t &dataSize)
{
  return m_pDevice->GetDebugManager()->GetTextureData(tex, arrayIdx, mip, params, dataSize);
}

void D3D11Replay::ReplaceResource(ResourceId from, ResourceId to)
{
  m_pDevice->GetResourceManager()->ReplaceResource(from, to);
}

void D3D11Replay::RemoveReplacement(ResourceId id)
{
  m_pDevice->GetResourceManager()->RemoveReplacement(id);
}

vector<GPUCounter> D3D11Replay::EnumerateCounters()
{
  return m_pDevice->GetDebugManager()->EnumerateCounters();
}

void D3D11Replay::DescribeCounter(GPUCounter counterID, CounterDescription &desc)
{
  m_pDevice->GetDebugManager()->DescribeCounter(counterID, desc);
}

vector<CounterResult> D3D11Replay::FetchCounters(const vector<GPUCounter> &counters)
{
  return m_pDevice->GetDebugManager()->FetchCounters(counters);
}

void D3D11Replay::RenderMesh(uint32_t eventID, const vector<MeshFormat> &secondaryDraws,
                             const MeshDisplay &cfg)
{
  return m_pDevice->GetDebugManager()->RenderMesh(eventID, secondaryDraws, cfg);
}

void D3D11Replay::BuildTargetShader(string source, string entry, const uint32_t compileFlags,
                                    ShaderStage type, ResourceId *id, string *errors)
{
  m_pDevice->GetDebugManager()->BuildShader(source, entry, D3DCOMPILE_DEBUG | compileFlags, type,
                                            id, errors);
}

void D3D11Replay::BuildCustomShader(string source, string entry, const uint32_t compileFlags,
                                    ShaderStage type, ResourceId *id, string *errors)
{
  m_pDevice->GetDebugManager()->BuildShader(source, entry, compileFlags, type, id, errors);
}

bool D3D11Replay::RenderTexture(TextureDisplay cfg)
{
  return m_pDevice->GetDebugManager()->RenderTexture(cfg, true);
}

void D3D11Replay::RenderCheckerboard(Vec3f light, Vec3f dark)
{
  m_pDevice->GetDebugManager()->RenderCheckerboard(light, dark);
}

void D3D11Replay::RenderHighlightBox(float w, float h, float scale)
{
  m_pDevice->GetDebugManager()->RenderHighlightBox(w, h, scale);
}

void D3D11Replay::FillCBufferVariables(ResourceId shader, string entryPoint, uint32_t cbufSlot,
                                       vector<ShaderVariable> &outvars, const vector<byte> &data)
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

ShaderDebugTrace D3D11Replay::DebugVertex(uint32_t eventID, uint32_t vertid, uint32_t instid,
                                          uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
  return m_pDevice->GetDebugManager()->DebugVertex(eventID, vertid, instid, idx, instOffset,
                                                   vertOffset);
}

ShaderDebugTrace D3D11Replay::DebugPixel(uint32_t eventID, uint32_t x, uint32_t y, uint32_t sample,
                                         uint32_t primitive)
{
  return m_pDevice->GetDebugManager()->DebugPixel(eventID, x, y, sample, primitive);
}

ShaderDebugTrace D3D11Replay::DebugThread(uint32_t eventID, const uint32_t groupid[3],
                                          const uint32_t threadid[3])
{
  return m_pDevice->GetDebugManager()->DebugThread(eventID, groupid, threadid);
}

uint32_t D3D11Replay::PickVertex(uint32_t eventID, const MeshDisplay &cfg, uint32_t x, uint32_t y)
{
  return m_pDevice->GetDebugManager()->PickVertex(eventID, cfg, x, y);
}

void D3D11Replay::PickPixel(ResourceId texture, uint32_t x, uint32_t y, uint32_t sliceFace,
                            uint32_t mip, uint32_t sample, CompType typeHint, float pixel[4])
{
  m_pDevice->GetDebugManager()->PickPixel(texture, x, y, sliceFace, mip, sample, typeHint, pixel);
}

ResourceId D3D11Replay::RenderOverlay(ResourceId texid, CompType typeHint, DebugOverlay overlay,
                                      uint32_t eventID, const vector<uint32_t> &passEvents)
{
  return m_pDevice->GetDebugManager()->RenderOverlay(texid, typeHint, overlay, eventID, passEvents);
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
    if(m_CurPipelineState.m_OM.RenderTargets[i].Object == id ||
       m_CurPipelineState.m_OM.RenderTargets[i].Resource == id)
      return true;
  }

  if(m_CurPipelineState.m_OM.DepthTarget.Object == id ||
     m_CurPipelineState.m_OM.DepthTarget.Resource == id)
    return true;

  return false;
}

void D3D11Replay::InitCallstackResolver()
{
  m_pDevice->GetSerialiser()->InitCallstackResolver();
}

bool D3D11Replay::HasCallstacks()
{
  return m_pDevice->GetSerialiser()->HasCallstacks();
}

Callstack::StackResolver *D3D11Replay::GetCallstackResolver()
{
  return m_pDevice->GetSerialiser()->GetCallstackResolver();
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

  if(resource != NULL && templateTex.customName)
  {
    string name = templateTex.name.elems;
    SetDebugName(resource, templateTex.name.elems);
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

  if(resource != NULL && templateBuf.customName)
  {
    string name = templateBuf.name.elems;
    SetDebugName(resource, templateBuf.name.elems);
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

ReplayStatus D3D11_CreateReplayDevice(const char *logfile, IReplayDriver **driver)
{
  RDCDEBUG("Creating a D3D11 replay device");

  WrappedIDXGISwapChain4::RegisterD3DDeviceCallback(GetD3D11DeviceIfAlloc);

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

  typedef HRESULT(__cdecl * PFN_RENDERDOC_CREATE_DEVICE_AND_SWAP_CHAIN)(
      __in_opt IDXGIAdapter *, D3D_DRIVER_TYPE, HMODULE, UINT,
      __in_ecount_opt(FeatureLevels) CONST D3D_FEATURE_LEVEL *, UINT FeatureLevels, UINT,
      __in_opt CONST DXGI_SWAP_CHAIN_DESC *, __out_opt IDXGISwapChain **, __out_opt ID3D11Device **,
      __out_opt D3D_FEATURE_LEVEL *, __out_opt ID3D11DeviceContext **);

  PFN_RENDERDOC_CREATE_DEVICE_AND_SWAP_CHAIN createDevice =
      (PFN_RENDERDOC_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(
          GetModuleHandleA("renderdoc.dll"), "RENDERDOC_CreateWrappedD3D11DeviceAndSwapChain");

  RDCASSERT(createDevice);

  ID3D11Device *device = NULL;

  D3D11InitParams initParams;
  RDCDriver driverFileType = RDC_D3D11;
  string driverName = "D3D11";
  uint64_t machineIdent = 0;
  if(logfile)
  {
    auto status = RenderDoc::Inst().FillInitParams(logfile, driverFileType, driverName,
                                                   machineIdent, (RDCInitParams *)&initParams);
    if(status != ReplayStatus::Succeeded)
      return status;
  }

  // initParams.SerialiseVersion is guaranteed to be valid/supported since otherwise the
  // FillInitParams (which calls D3D11InitParams::Serialise) would have failed above, so no need to
  // check it here.

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
  hr = createDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, NULL,
                    NULL, &maxFeatureLevel, NULL);

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
    hr = createDevice(
        /*pAdapter=*/NULL, driverType, /*Software=*/NULL, flags,
        /*pFeatureLevels=*/featureLevelArray, /*nFeatureLevels=*/numFeatureLevels, D3D11_SDK_VERSION,
        /*pSwapChainDesc=*/NULL, (IDXGISwapChain **)NULL, (ID3D11Device **)&device,
        (D3D_FEATURE_LEVEL *)NULL, (ID3D11DeviceContext **)NULL);

    if(SUCCEEDED(hr))
    {
      WrappedID3D11Device *wrappedDev = (WrappedID3D11Device *)device;
      if(logfile)
        wrappedDev->SetLogFile(logfile);
      wrappedDev->SetLogVersion(initParams.SerialiseVersion);

      if(logfile && wrappedDev->GetSerialiser()->HasError())
      {
        SAFE_RELEASE(wrappedDev);
        return ReplayStatus::FileIOFailed;
      }

      RDCLOG("Created device.");
      D3D11Replay *replay = wrappedDev->GetReplay();

      replay->SetProxy(logfile == NULL, warpFallback);
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
