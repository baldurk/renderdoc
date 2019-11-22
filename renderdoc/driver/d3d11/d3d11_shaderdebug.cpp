/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "data/resource.h"
#include "driver/d3d11/d3d11_renderstate.h"
#include "driver/d3d11/d3d11_resources.h"
#include "driver/shaders/dxbc/dxbc_bytecode.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "maths/formatpacking.h"
#include "maths/vec.h"
#include "strings/string_utils.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_manager.h"
#include "d3d11_shader_cache.h"

struct DebugHit
{
  uint32_t numHits;
  float posx;
  float posy;
  float depth;
  uint32_t primitive;
  uint32_t isFrontFace;
  uint32_t sample;
  uint32_t coverage;
  uint32_t rawdata;    // arbitrary, depending on shader
};

class D3D11DebugAPIWrapper : public ShaderDebug::DebugAPIWrapper
{
public:
  D3D11DebugAPIWrapper(WrappedID3D11Device *device, DXBC::DXBCContainer *dxbc,
                       const ShaderDebug::GlobalState &globalState);

  void SetCurrentInstruction(uint32_t instruction) { m_instruction = instruction; }
  void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, std::string d);

  bool CalculateMathIntrinsic(DXBCBytecode::OpcodeType opcode, const ShaderVariable &input,
                              ShaderVariable &output1, ShaderVariable &output2);

  ShaderVariable GetSampleInfo(DXBCBytecode::OperandType type, bool isAbsoluteResource, UINT slot,
                               const char *opString);
  ShaderVariable GetBufferInfo(DXBCBytecode::OperandType type, UINT slot, const char *opString);
  ShaderVariable GetResourceInfo(DXBCBytecode::OperandType type, UINT slot, uint32_t mipLevel,
                                 int &dim);

  bool CalculateSampleGather(DXBCBytecode::OpcodeType opcode,
                             ShaderDebug::SampleGatherResourceData resourceData,
                             ShaderDebug::SampleGatherSamplerData samplerData, ShaderVariable uv,
                             ShaderVariable ddxCalc, ShaderVariable ddyCalc,
                             const int texelOffsets[3], int multisampleIndex, float lodOrCompareValue,
                             const uint8_t swizzle[4], ShaderDebug::GatherChannel gatherChannel,
                             const char *opString, ShaderVariable &output);

private:
  DXBC::ShaderType GetShaderType() { return m_dxbc ? m_dxbc->m_Type : DXBC::ShaderType::Pixel; }
  WrappedID3D11Device *m_pDevice;
  DXBC::DXBCContainer *m_dxbc;
  const ShaderDebug::GlobalState &m_globalState;
  uint32_t m_instruction;
};

D3D11DebugAPIWrapper::D3D11DebugAPIWrapper(WrappedID3D11Device *device, DXBC::DXBCContainer *dxbc,
                                           const ShaderDebug::GlobalState &globalState)
    : m_pDevice(device), m_dxbc(dxbc), m_globalState(globalState), m_instruction(0)
{
}

void D3D11DebugAPIWrapper::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                                           std::string d)
{
  m_pDevice->AddDebugMessage(c, sv, src, d);
}

ShaderVariable D3D11DebugAPIWrapper::GetSampleInfo(DXBCBytecode::OperandType type,
                                                   bool isAbsoluteResource, UINT slot,
                                                   const char *opString)
{
  ID3D11DeviceContext *context = NULL;
  m_pDevice->GetImmediateContext(&context);

  ShaderVariable result("", 0U, 0U, 0U, 0U);

  ID3D11Resource *res = NULL;

  if(type == DXBCBytecode::TYPE_RASTERIZER)
  {
    ID3D11RenderTargetView *rtv[8] = {};
    ID3D11DepthStencilView *dsv = NULL;

    context->OMGetRenderTargets(8, rtv, &dsv);

    // try depth first - both should match sample count though to be valid
    if(dsv)
    {
      dsv->GetResource(&res);
    }
    else
    {
      for(size_t i = 0; i < ARRAY_COUNT(rtv); i++)
      {
        if(rtv[i])
        {
          rtv[i]->GetResource(&res);
          break;
        }
      }
    }

    if(!res)
    {
      RDCWARN("No targets bound for output when calling sampleinfo on rasterizer");

      m_pDevice->AddDebugMessage(
          MessageCategory::Shaders, MessageSeverity::Medium, MessageSource::RuntimeWarning,
          StringFormat::Fmt("Shader debugging %d: %s\n"
                            "No targets bound for output when calling sampleinfo on rasterizer",
                            m_instruction, opString));
    }

    for(size_t i = 0; i < ARRAY_COUNT(rtv); i++)
      SAFE_RELEASE(rtv[i]);
    SAFE_RELEASE(dsv);
  }
  else if(type == DXBCBytecode::TYPE_RESOURCE && isAbsoluteResource)
  {
    ID3D11ShaderResourceView *srv = NULL;
    switch(GetShaderType())
    {
      case DXBC::ShaderType::Vertex: context->VSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Hull: context->HSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Domain: context->DSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Geometry: context->GSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Pixel: context->PSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Compute: context->CSGetShaderResources(slot, 1, &srv); break;
    }

    if(srv)
    {
      srv->GetResource(&res);
    }
    else
    {
      RDCWARN("SRV is NULL being queried by sampleinfo");

      m_pDevice->AddDebugMessage(
          MessageCategory::Shaders, MessageSeverity::Medium, MessageSource::RuntimeWarning,
          StringFormat::Fmt("Shader debugging %d: %s\nSRV is NULL being queried by sampleinfo",
                            m_instruction, opString));
    }

    SAFE_RELEASE(srv);
  }
  else
  {
    RDCWARN("unexpected operand type to sample_info");
  }

  if(res)
  {
    D3D11_RESOURCE_DIMENSION dim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
    res->GetType(&dim);

    if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
    {
      D3D11_TEXTURE2D_DESC desc;
      ((ID3D11Texture2D *)res)->GetDesc(&desc);

      // returns 1 for non-multisampled resources
      result.value.u.x = RDCMAX(1U, desc.SampleDesc.Count);
    }
    else
    {
      if(type == DXBCBytecode::TYPE_RASTERIZER)
      {
        // special behaviour for non-2D (i.e. by definition non-multisampled) textures when
        // querying the rasterizer, just return 1.
        result.value.u.x = 1;
      }
      else
      {
        m_pDevice->AddDebugMessage(
            MessageCategory::Shaders, MessageSeverity::Medium, MessageSource::RuntimeWarning,
            StringFormat::Fmt("Shader debugging %d: %s\nResource specified is not a 2D texture",
                              m_instruction, opString));

        result.value.u.x = 0;
      }
    }

    SAFE_RELEASE(res);
  }
  SAFE_RELEASE(context);
  return result;
}

ShaderVariable D3D11DebugAPIWrapper::GetBufferInfo(DXBCBytecode::OperandType type, UINT slot,
                                                   const char *opString)
{
  ID3D11DeviceContext *context = NULL;
  m_pDevice->GetImmediateContext(&context);

  ShaderVariable result("", 0U, 0U, 0U, 0U);

  if(type == DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
  {
    ID3D11UnorderedAccessView *uav = NULL;
    if(GetShaderType() == DXBC::ShaderType::Compute)
      context->CSGetUnorderedAccessViews(slot, 1, &uav);
    else
      context->OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, slot, 1, &uav);

    if(uav)
    {
      D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
      uav->GetDesc(&uavDesc);

      if(uavDesc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
      {
        result.value.u.x = result.value.u.y = result.value.u.z = result.value.u.w =
            uavDesc.Buffer.NumElements;
      }
      else
      {
        RDCWARN("Unexpected UAV dimension %d passed to bufinfo", uavDesc.ViewDimension);

        m_pDevice->AddDebugMessage(
            MessageCategory::Shaders, MessageSeverity::High, MessageSource::RuntimeWarning,
            StringFormat::Fmt(
                "Shader debugging %d: %s\nUAV being queried by bufinfo is not a buffer",
                m_instruction, opString));
      }
    }
    else
    {
      RDCWARN("UAV is NULL being queried by bufinfo");

      m_pDevice->AddDebugMessage(
          MessageCategory::Shaders, MessageSeverity::Medium, MessageSource::RuntimeWarning,
          StringFormat::Fmt("Shader debugging %d: %s\nUAV being queried by bufinfo is NULL",
                            m_instruction, opString));
    }

    SAFE_RELEASE(uav);
  }
  else
  {
    ID3D11ShaderResourceView *srv = NULL;
    switch(GetShaderType())
    {
      case DXBC::ShaderType::Vertex: context->VSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Hull: context->HSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Domain: context->DSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Geometry: context->GSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Pixel: context->PSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Compute: context->CSGetShaderResources(slot, 1, &srv); break;
    }

    if(srv)
    {
      D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
      srv->GetDesc(&srvDesc);

      if(srvDesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER)
      {
        result.value.u.x = result.value.u.y = result.value.u.z = result.value.u.w =
            srvDesc.Buffer.NumElements;
      }
      else if(srvDesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX)
      {
        result.value.u.x = result.value.u.y = result.value.u.z = result.value.u.w =
            srvDesc.BufferEx.NumElements;
      }
      else
      {
        RDCWARN("Unexpected SRV dimension %d passed to bufinfo", srvDesc.ViewDimension);

        m_pDevice->AddDebugMessage(
            MessageCategory::Shaders, MessageSeverity::High, MessageSource::RuntimeWarning,
            StringFormat::Fmt(
                "Shader debugging %d: %s\nSRV being queried by bufinfo is not a buffer",
                m_instruction, opString));
      }
    }
    else
    {
      RDCWARN("SRV is NULL being queried by bufinfo");

      m_pDevice->AddDebugMessage(
          MessageCategory::Shaders, MessageSeverity::Medium, MessageSource::RuntimeWarning,
          StringFormat::Fmt("Shader debugging %d: %s\nSRV being queried by bufinfo is NULL",
                            m_instruction, opString));
    }

    SAFE_RELEASE(srv);
  }

  SAFE_RELEASE(context);
  return result;
}

ShaderVariable D3D11DebugAPIWrapper::GetResourceInfo(DXBCBytecode::OperandType type, UINT slot,
                                                     uint32_t mipLevel, int &dim)
{
  ID3D11DeviceContext *context = NULL;
  m_pDevice->GetImmediateContext(&context);

  ShaderVariable result("", 0.0f, 0.0f, 0.0f, 0.0f);

  if(type != DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
  {
    ID3D11ShaderResourceView *srv = NULL;
    switch(GetShaderType())
    {
      case DXBC::ShaderType::Vertex: context->VSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Hull: context->HSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Domain: context->DSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Geometry: context->GSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Pixel: context->PSGetShaderResources(slot, 1, &srv); break;
      case DXBC::ShaderType::Compute: context->CSGetShaderResources(slot, 1, &srv); break;
    }

    if(srv)
    {
      D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
      srv->GetDesc(&srvDesc);

      switch(srvDesc.ViewDimension)
      {
        case D3D11_SRV_DIMENSION_BUFFER:
        {
          dim = 1;

          result.value.u.x = srvDesc.Buffer.NumElements;
          result.value.u.y = 0;
          result.value.u.z = 0;
          result.value.u.w = 0;
          break;
        }
        case D3D11_SRV_DIMENSION_BUFFEREX:
        {
          dim = 1;

          result.value.u.x = srvDesc.BufferEx.NumElements;
          result.value.u.y = 0;
          result.value.u.z = 0;
          result.value.u.w = 0;
          break;
        }
        case D3D11_SRV_DIMENSION_TEXTURE1D:
        case D3D11_SRV_DIMENSION_TEXTURE1DARRAY:
        {
          ID3D11Texture1D *tex = NULL;
          srv->GetResource((ID3D11Resource **)&tex);

          dim = 1;

          if(tex)
          {
            bool isarray = srvDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1DARRAY;

            D3D11_TEXTURE1D_DESC desc;
            tex->GetDesc(&desc);
            result.value.u.x = RDCMAX(1U, desc.Width >> mipLevel);
            result.value.u.y = isarray ? srvDesc.Texture1DArray.ArraySize : 0;
            result.value.u.z = 0;
            result.value.u.w =
                isarray ? srvDesc.Texture1DArray.MipLevels : srvDesc.Texture1D.MipLevels;

            if(mipLevel >= result.value.u.w)
              result.value.u.x = result.value.u.y = 0;

            SAFE_RELEASE(tex);
          }
          break;
        }
        case D3D11_SRV_DIMENSION_TEXTURE2D:
        case D3D11_SRV_DIMENSION_TEXTURE2DARRAY:
        case D3D11_SRV_DIMENSION_TEXTURE2DMS:
        case D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
        {
          ID3D11Texture2D *tex = NULL;
          srv->GetResource((ID3D11Resource **)&tex);

          dim = 2;

          if(tex)
          {
            D3D11_TEXTURE2D_DESC desc;
            tex->GetDesc(&desc);
            result.value.u.x = RDCMAX(1U, desc.Width >> mipLevel);
            result.value.u.y = RDCMAX(1U, desc.Height >> mipLevel);

            if(srvDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D)
            {
              result.value.u.z = 0;
              result.value.u.w = srvDesc.Texture2D.MipLevels;
            }
            else if(srvDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY)
            {
              result.value.u.z = srvDesc.Texture2DArray.ArraySize;
              result.value.u.w = srvDesc.Texture2DArray.MipLevels;
            }
            else if(srvDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMS)
            {
              result.value.u.z = 0;
              result.value.u.w = 1;
            }
            else if(srvDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY)
            {
              result.value.u.z = srvDesc.Texture2DMSArray.ArraySize;
              result.value.u.w = 1;
            }

            if(mipLevel >= result.value.u.w)
              result.value.u.x = result.value.u.y = result.value.u.z = 0;

            SAFE_RELEASE(tex);
          }
          break;
        }
        case D3D11_SRV_DIMENSION_TEXTURE3D:
        {
          ID3D11Texture3D *tex = NULL;
          srv->GetResource((ID3D11Resource **)&tex);

          dim = 3;

          if(tex)
          {
            D3D11_TEXTURE3D_DESC desc;
            tex->GetDesc(&desc);
            result.value.u.x = RDCMAX(1U, desc.Width >> mipLevel);
            result.value.u.y = RDCMAX(1U, desc.Height >> mipLevel);
            result.value.u.z = RDCMAX(1U, desc.Depth >> mipLevel);
            result.value.u.w = srvDesc.Texture3D.MipLevels;

            if(mipLevel >= result.value.u.w)
              result.value.u.x = result.value.u.y = result.value.u.z = 0;

            SAFE_RELEASE(tex);
          }
          break;
        }
        case D3D11_SRV_DIMENSION_TEXTURECUBE:
        case D3D11_SRV_DIMENSION_TEXTURECUBEARRAY:
        {
          ID3D11Texture2D *tex = NULL;
          srv->GetResource((ID3D11Resource **)&tex);

          dim = 2;

          if(tex)
          {
            bool isarray = srvDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE1DARRAY;

            D3D11_TEXTURE2D_DESC desc;
            tex->GetDesc(&desc);
            result.value.u.x = RDCMAX(1U, desc.Width >> mipLevel);
            result.value.u.y = RDCMAX(1U, desc.Height >> mipLevel);

            // the spec says "If srcResource is a TextureCubeArray, [...]. dest.z is set to an
            // undefined value."
            // but that's stupid, and implementations seem to return the number of cubes
            result.value.u.z = isarray ? srvDesc.TextureCubeArray.NumCubes : 0;
            result.value.u.w =
                isarray ? srvDesc.TextureCubeArray.MipLevels : srvDesc.TextureCube.MipLevels;

            if(mipLevel >= result.value.u.w)
              result.value.u.x = result.value.u.y = result.value.u.z = 0;

            SAFE_RELEASE(tex);
          }
          break;
        }
      }

      SAFE_RELEASE(srv);
    }
  }
  else
  {
    ID3D11UnorderedAccessView *uav = NULL;
    if(GetShaderType() == DXBC::ShaderType::Compute)
    {
      context->CSGetUnorderedAccessViews(slot, 1, &uav);
    }
    else
    {
      ID3D11RenderTargetView *rtvs[8] = {0};
      ID3D11DepthStencilView *dsv = NULL;
      context->OMGetRenderTargetsAndUnorderedAccessViews(0, rtvs, &dsv, slot, 1, &uav);

      for(int i = 0; i < 8; i++)
        SAFE_RELEASE(rtvs[i]);
      SAFE_RELEASE(dsv);
    }

    if(uav)
    {
      D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
      uav->GetDesc(&uavDesc);

      switch(uavDesc.ViewDimension)
      {
        case D3D11_UAV_DIMENSION_BUFFER:
        {
          ID3D11Buffer *buf = NULL;
          uav->GetResource((ID3D11Resource **)&buf);

          dim = 1;

          if(buf)
          {
            D3D11_BUFFER_DESC desc;
            buf->GetDesc(&desc);
            result.value.u.x = desc.ByteWidth;
            result.value.u.y = 0;
            result.value.u.z = 0;
            result.value.u.w = 0;

            SAFE_RELEASE(buf);
          }
          break;
        }
        case D3D11_UAV_DIMENSION_TEXTURE1D:
        case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
        {
          ID3D11Texture1D *tex = NULL;
          uav->GetResource((ID3D11Resource **)&tex);

          dim = 1;

          if(tex)
          {
            bool isarray = uavDesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY;

            D3D11_TEXTURE1D_DESC desc;
            tex->GetDesc(&desc);
            result.value.u.x = RDCMAX(1U, desc.Width >> mipLevel);
            result.value.u.y = isarray ? uavDesc.Texture1DArray.ArraySize : 0;
            result.value.u.z = 0;

            // spec says "For UAVs (u#), the number of mip levels is always 1."
            result.value.u.w = 1;

            if(mipLevel >= result.value.u.w)
              result.value.u.x = result.value.u.y = 0;

            SAFE_RELEASE(tex);
          }
          break;
        }
        case D3D11_UAV_DIMENSION_TEXTURE2D:
        case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
        {
          ID3D11Texture2D *tex = NULL;
          uav->GetResource((ID3D11Resource **)&tex);

          dim = 2;

          if(tex)
          {
            D3D11_TEXTURE2D_DESC desc;
            tex->GetDesc(&desc);
            result.value.u.x = RDCMAX(1U, desc.Width >> mipLevel);
            result.value.u.y = RDCMAX(1U, desc.Height >> mipLevel);

            if(uavDesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D)
              result.value.u.z = 0;
            else if(uavDesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
              result.value.u.z = uavDesc.Texture2DArray.ArraySize;

            // spec says "For UAVs (u#), the number of mip levels is always 1."
            result.value.u.w = 1;

            if(mipLevel >= result.value.u.w)
              result.value.u.x = result.value.u.y = result.value.u.z = 0;

            SAFE_RELEASE(tex);
          }
          break;
        }
        case D3D11_UAV_DIMENSION_TEXTURE3D:
        {
          ID3D11Texture3D *tex = NULL;
          uav->GetResource((ID3D11Resource **)&tex);

          dim = 3;

          if(tex)
          {
            D3D11_TEXTURE3D_DESC desc;
            tex->GetDesc(&desc);
            result.value.u.x = RDCMAX(1U, desc.Width >> mipLevel);
            result.value.u.y = RDCMAX(1U, desc.Height >> mipLevel);
            result.value.u.z = RDCMAX(1U, desc.Depth >> mipLevel);

            // spec says "For UAVs (u#), the number of mip levels is always 1."
            result.value.u.w = 1;

            if(mipLevel >= result.value.u.w)
              result.value.u.x = result.value.u.y = result.value.u.z = 0;

            SAFE_RELEASE(tex);
          }
          break;
        }
      }

      SAFE_RELEASE(uav);
    }
  }

  SAFE_RELEASE(context);
  return result;
}

bool D3D11DebugAPIWrapper::CalculateSampleGather(
    DXBCBytecode::OpcodeType opcode, ShaderDebug::SampleGatherResourceData resourceData,
    ShaderDebug::SampleGatherSamplerData samplerData, ShaderVariable uv, ShaderVariable ddxCalc,
    ShaderVariable ddyCalc, const int texelOffsets[3], int multisampleIndex,
    float lodOrCompareValue, const uint8_t swizzle[4], ShaderDebug::GatherChannel gatherChannel,
    const char *opString, ShaderVariable &output)
{
  using namespace DXBCBytecode;

  std::string funcRet = "";
  DXGI_FORMAT retFmt = DXGI_FORMAT_UNKNOWN;

  if(opcode == OPCODE_SAMPLE_C || opcode == OPCODE_SAMPLE_C_LZ || opcode == OPCODE_GATHER4_C ||
     opcode == OPCODE_GATHER4_PO_C || opcode == OPCODE_LOD)
  {
    retFmt = DXGI_FORMAT_R32G32B32A32_FLOAT;
    funcRet = "float4";
  }

  std::string samplerDecl = "";
  if(samplerData.mode == SAMPLER_MODE_DEFAULT)
    samplerDecl = "SamplerState s";
  else if(samplerData.mode == SAMPLER_MODE_COMPARISON)
    samplerDecl = "SamplerComparisonState s";

  std::string textureDecl = "";
  int texdim = 2;
  int offsetDim = 2;
  bool useOffsets = true;
  if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE1D)
  {
    textureDecl = "Texture1D";
    texdim = 1;
    offsetDim = 1;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2D)
  {
    textureDecl = "Texture2D";
    texdim = 2;
    offsetDim = 2;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DMS)
  {
    textureDecl = "Texture2DMS";
    texdim = 2;
    offsetDim = 2;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE3D)
  {
    textureDecl = "Texture3D";
    texdim = 3;
    offsetDim = 3;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURECUBE)
  {
    textureDecl = "TextureCube";
    texdim = 3;
    offsetDim = 3;
    useOffsets = false;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE1DARRAY)
  {
    textureDecl = "Texture1DArray";
    texdim = 2;
    offsetDim = 1;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DARRAY)
  {
    textureDecl = "Texture2DArray";
    texdim = 3;
    offsetDim = 2;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DMSARRAY)
  {
    textureDecl = "Texture2DMSArray";
    texdim = 3;
    offsetDim = 2;
  }
  else if(resourceData.dim == RESOURCE_DIMENSION_TEXTURECUBEARRAY)
  {
    textureDecl = "TextureCubeArray";
    texdim = 4;
    offsetDim = 3;
    useOffsets = false;
  }
  else
  {
    RDCERR("Unsupported resource type %d in sample operation", resourceData.dim);
  }

  {
    char *typeStr[DXBC::NUM_RETURN_TYPES] = {
        "",    // enum starts at ==1
        "unorm float",
        "snorm float",
        "int",
        "uint",
        "float",
        "__",    // RETURN_TYPE_MIXED
        "double",
        "__",    // RETURN_TYPE_CONTINUED
        "__",    // RETURN_TYPE_UNUSED
    };

    // obviously these may be overly optimistic in some cases
    // but since we don't know at debug time what the source texture format is
    // we just use the fattest one necessary. There's no harm in retrieving at
    // higher precision
    DXGI_FORMAT fmts[DXBC::NUM_RETURN_TYPES] = {
        DXGI_FORMAT_UNKNOWN,               // enum starts at ==1
        DXGI_FORMAT_R32G32B32A32_FLOAT,    // unorm float
        DXGI_FORMAT_R32G32B32A32_FLOAT,    // snorm float
        DXGI_FORMAT_R32G32B32A32_SINT,     // int
        DXGI_FORMAT_R32G32B32A32_UINT,     // uint
        DXGI_FORMAT_R32G32B32A32_FLOAT,    // float
        DXGI_FORMAT_UNKNOWN,               // RETURN_TYPE_MIXED

        // should maybe be double, but there is no double texture format anyway!
        // spec is unclear but I presume reads are done at most at float
        // precision anyway since that's the source, and converted to doubles.
        DXGI_FORMAT_R32G32B32A32_FLOAT,    // double

        DXGI_FORMAT_UNKNOWN,    // RETURN_TYPE_CONTINUED
        DXGI_FORMAT_UNKNOWN,    // RETURN_TYPE_UNUSED
    };

    char buf[64] = {0};
    StringFormat::snprintf(buf, 63, "%s4", typeStr[resourceData.retType]);

    if(retFmt == DXGI_FORMAT_UNKNOWN)
    {
      funcRet = buf;
      retFmt = fmts[resourceData.retType];
    }

    if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DMS ||
       resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DMSARRAY)
    {
      if(resourceData.sampleCount > 0)
        StringFormat::snprintf(buf, 63, "%s4, %d", typeStr[resourceData.retType],
                               resourceData.sampleCount);
    }

    textureDecl += "<";
    textureDecl += buf;
    textureDecl += "> t";
  }

  char *formats[4][2] = {
      {"float(%.10f)", "int(%d)"},
      {"float2(%.10f, %.10f)", "int2(%d, %d)"},
      {"float3(%.10f, %.10f, %.10f)", "int3(%d, %d, %d)"},
      {"float4(%.10f, %.10f, %.10f, %.10f)", "int4(%d, %d, %d, %d)"},
  };

  int texcoordType = 0;
  int ddxType = 0;
  int ddyType = 0;
  int texdimOffs = 0;

  if(opcode == OPCODE_SAMPLE || opcode == OPCODE_SAMPLE_L || opcode == OPCODE_SAMPLE_B ||
     opcode == OPCODE_SAMPLE_D || opcode == OPCODE_SAMPLE_C || opcode == OPCODE_SAMPLE_C_LZ ||
     opcode == OPCODE_GATHER4 || opcode == OPCODE_GATHER4_C || opcode == OPCODE_GATHER4_PO ||
     opcode == OPCODE_GATHER4_PO_C || opcode == OPCODE_LOD)
  {
    // all floats
    texcoordType = ddxType = ddyType = 0;
  }
  else if(opcode == OPCODE_LD)
  {
    // int address, one larger than texdim (to account for mip/slice parameter)
    texdimOffs = 1;
    texcoordType = 1;

    if(texdim == 4)
    {
      RDCERR("Unexpectedly large texture in load operation");
    }
  }
  else if(opcode == OPCODE_LD_MS)
  {
    texcoordType = 1;

    if(texdim == 4)
    {
      RDCERR("Unexpectedly large texture in load operation");
    }
  }

  for(uint32_t i = 0; i < ddxCalc.columns; i++)
  {
    if(ddxType == 0 && (_isnan(ddxCalc.value.fv[i]) || !_finite(ddxCalc.value.fv[i])))
    {
      RDCWARN("NaN or Inf in texlookup");
      ddxCalc.value.fv[i] = 0.0f;

      m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("Shader debugging %d: %s\nNaN or Inf found in "
                                                   "texture lookup ddx - using 0.0 instead",
                                                   m_instruction, opString));
    }
    if(ddyType == 0 && (_isnan(ddyCalc.value.fv[i]) || !_finite(ddyCalc.value.fv[i])))
    {
      RDCWARN("NaN or Inf in texlookup");
      ddyCalc.value.fv[i] = 0.0f;

      m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("Shader debugging %d: %s\nNaN or Inf found in "
                                                   "texture lookup ddy - using 0.0 instead",
                                                   m_instruction, opString));
    }
  }

  for(uint32_t i = 0; i < uv.columns; i++)
  {
    if(texcoordType == 0 && (_isnan(uv.value.fv[i]) || !_finite(uv.value.fv[i])))
    {
      RDCWARN("NaN or Inf in texlookup");
      uv.value.fv[i] = 0.0f;

      m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("Shader debugging %d: %s\nNaN or Inf found in "
                                                   "texture lookup uv - using 0.0 instead",
                                                   m_instruction, opString));
    }
  }

  char buf[256] = {0};
  char buf2[256] = {0};
  char buf3[256] = {0};

  // because of unions in .value we can pass the float versions and printf will interpret it as
  // the right type according to formats
  if(texcoordType == 0)
    StringFormat::snprintf(buf, 255, formats[texdim + texdimOffs - 1][texcoordType], uv.value.f.x,
                           uv.value.f.y, uv.value.f.z, uv.value.f.w);
  else
    StringFormat::snprintf(buf, 255, formats[texdim + texdimOffs - 1][texcoordType], uv.value.i.x,
                           uv.value.i.y, uv.value.i.z, uv.value.i.w);

  if(ddxType == 0)
    StringFormat::snprintf(buf2, 255, formats[offsetDim + texdimOffs - 1][ddxType], ddxCalc.value.f.x,
                           ddxCalc.value.f.y, ddxCalc.value.f.z, ddxCalc.value.f.w);
  else
    StringFormat::snprintf(buf2, 255, formats[offsetDim + texdimOffs - 1][ddxType], ddxCalc.value.i.x,
                           ddxCalc.value.i.y, ddxCalc.value.i.z, ddxCalc.value.i.w);

  if(ddyType == 0)
    StringFormat::snprintf(buf3, 255, formats[offsetDim + texdimOffs - 1][ddyType], ddyCalc.value.f.x,
                           ddyCalc.value.f.y, ddyCalc.value.f.z, ddyCalc.value.f.w);
  else
    StringFormat::snprintf(buf3, 255, formats[offsetDim + texdimOffs - 1][ddyType], ddyCalc.value.i.x,
                           ddyCalc.value.i.y, ddyCalc.value.i.z, ddyCalc.value.i.w);

  std::string texcoords = buf;
  std::string ddx = buf2;
  std::string ddy = buf3;

  if(opcode == OPCODE_LD_MS)
  {
    StringFormat::snprintf(buf, 255, formats[0][1], multisampleIndex);
  }

  std::string sampleIdx = buf;

  std::string offsets = "";

  if(useOffsets)
  {
    if(offsetDim == 1)
      StringFormat::snprintf(buf, 255, ", int(%d)", texelOffsets[0]);
    else if(offsetDim == 2)
      StringFormat::snprintf(buf, 255, ", int2(%d, %d)", texelOffsets[0], texelOffsets[1]);
    else if(offsetDim == 3)
      StringFormat::snprintf(buf, 255, ", int3(%d, %d, %d)", texelOffsets[0], texelOffsets[1],
                             texelOffsets[2]);
    // texdim == 4 is cube arrays, no offset supported

    offsets = buf;
  }

  char elems[] = "xyzw";
  std::string strSwizzle = ".";
  for(int i = 0; i < 4; ++i)
  {
    strSwizzle += elems[swizzle[i]];
  }

  std::string strGatherChannel;
  switch(gatherChannel)
  {
    case ShaderDebug::GatherChannel::Red: strGatherChannel = "Red"; break;
    case ShaderDebug::GatherChannel::Green: strGatherChannel = "Green"; break;
    case ShaderDebug::GatherChannel::Blue: strGatherChannel = "Blue"; break;
    case ShaderDebug::GatherChannel::Alpha: strGatherChannel = "Alpha"; break;
  }

  std::string vsProgram = "float4 main(uint id : SV_VertexID) : SV_Position {\n";
  vsProgram += "return float4((id == 2) ? 3.0f : -1.0f, (id == 0) ? -3.0f : 1.0f, 0.5, 1.0);\n";
  vsProgram += "}";

  std::string sampleProgram;

  if(opcode == OPCODE_SAMPLE || opcode == OPCODE_SAMPLE_B || opcode == OPCODE_SAMPLE_D)
  {
    sampleProgram = textureDecl + " : register(t0);\n" + samplerDecl + " : register(s0);\n\n";
    sampleProgram += funcRet + " main() : SV_Target0\n{\nreturn ";
    sampleProgram +=
        "t.SampleGrad(s, " + texcoords + ", " + ddx + ", " + ddy + offsets + ")" + strSwizzle + ";";
    sampleProgram += "\n}\n";
  }
  else if(opcode == OPCODE_SAMPLE_L)
  {
    // lod selection
    StringFormat::snprintf(buf, 255, "%.10f", lodOrCompareValue);

    sampleProgram = textureDecl + " : register(t0);\n" + samplerDecl + " : register(s0);\n\n";
    sampleProgram += funcRet + " main() : SV_Target0\n{\nreturn ";
    sampleProgram += "t.SampleLevel(s, " + texcoords + ", " + buf + offsets + ")" + strSwizzle + ";";
    sampleProgram += "\n}\n";
  }
  else if(opcode == OPCODE_SAMPLE_C || opcode == OPCODE_LOD)
  {
    // these operations need derivatives but have no hlsl function to call to provide them, so
    // we fake it in the vertex shader

    std::string uvDim = "1";
    uvDim[0] += char(texdim + texdimOffs - 1);

    vsProgram = "void main(uint id : SV_VertexID, out float4 pos : SV_Position, out float" + uvDim +
                " uv : uvs) {\n";

    StringFormat::snprintf(
        buf, 255, formats[texdim + texdimOffs - 1][texcoordType],
        uv.value.f.x + ddyCalc.value.f.x * 2.0f, uv.value.f.y + ddyCalc.value.f.y * 2.0f,
        uv.value.f.z + ddyCalc.value.f.z * 2.0f, uv.value.f.w + ddyCalc.value.f.w * 2.0f);

    vsProgram += "if(id == 0) uv = " + std::string(buf) + ";\n";

    StringFormat::snprintf(buf, 255, formats[texdim + texdimOffs - 1][texcoordType], uv.value.f.x,
                           uv.value.f.y, uv.value.f.z, uv.value.f.w);

    vsProgram += "if(id == 1) uv = " + std::string(buf) + ";\n";

    StringFormat::snprintf(
        buf, 255, formats[texdim + texdimOffs - 1][texcoordType],
        uv.value.f.x + ddxCalc.value.f.x * 2.0f, uv.value.f.y + ddxCalc.value.f.y * 2.0f,
        uv.value.f.z + ddxCalc.value.f.z * 2.0f, uv.value.f.w + ddxCalc.value.f.w * 2.0f);

    vsProgram += "if(id == 2) uv = " + std::string(buf) + ";\n";

    vsProgram += "pos = float4((id == 2) ? 3.0f : -1.0f, (id == 0) ? -3.0f : 1.0f, 0.5, 1.0);\n";
    vsProgram += "}";

    if(opcode == OPCODE_SAMPLE_C)
    {
      // comparison value
      StringFormat::snprintf(buf, 255, "%.10f", lodOrCompareValue);

      sampleProgram = textureDecl + " : register(t0);\n" + samplerDecl + " : register(s0);\n\n";
      sampleProgram += funcRet + " main(float4 pos : SV_Position, float" + uvDim +
                       " uv : uvs) : SV_Target0\n{\n";
      sampleProgram +=
          "return t.SampleCmpLevelZero(s, uv, " + std::string(buf) + offsets + ").xxxx;";
      sampleProgram += "\n}\n";
    }
    else if(opcode == OPCODE_LOD)
    {
      sampleProgram = textureDecl + " : register(t0);\n" + samplerDecl + " : register(s0);\n\n";
      sampleProgram += funcRet + " main(float4 pos : SV_Position, float" + uvDim +
                       " uv : uvs) : SV_Target0\n{\n";
      sampleProgram +=
          "return float4(t.CalculateLevelOfDetail(s, uv), t.CalculateLevelOfDetailUnclamped(s, "
          "uv), 0.0f, 0.0f);";
      sampleProgram += "\n}\n";
    }
  }
  else if(opcode == OPCODE_SAMPLE_C_LZ)
  {
    // comparison value
    StringFormat::snprintf(buf, 255, "%.10f", lodOrCompareValue);

    sampleProgram = textureDecl + " : register(t0);\n" + samplerDecl + " : register(s0);\n\n";
    sampleProgram += funcRet + " main() : SV_Target0\n{\nreturn ";
    sampleProgram +=
        "t.SampleCmpLevelZero(s, " + texcoords + ", " + buf + offsets + ")" + strSwizzle + ";";
    sampleProgram += "\n}\n";
  }
  else if(opcode == OPCODE_LD)
  {
    sampleProgram = textureDecl + " : register(t0);\n\n";
    sampleProgram += funcRet + " main() : SV_Target0\n{\nreturn ";
    sampleProgram += "t.Load(" + texcoords + offsets + ")" + strSwizzle + ";";
    sampleProgram += "\n}\n";
  }
  else if(opcode == OPCODE_LD_MS)
  {
    sampleProgram = textureDecl + " : register(t0);\n\n";
    sampleProgram += funcRet + " main() : SV_Target0\n{\nreturn ";
    sampleProgram += "t.Load(" + texcoords + ", " + sampleIdx + offsets + ")" + strSwizzle + ";";
    sampleProgram += "\n}\n";
  }
  else if(opcode == OPCODE_GATHER4 || opcode == OPCODE_GATHER4_PO)
  {
    sampleProgram = textureDecl + " : register(t0);\n" + samplerDecl + " : register(s0);\n\n";
    sampleProgram += funcRet + " main() : SV_Target0\n{\nreturn ";
    sampleProgram +=
        "t.Gather" + strGatherChannel + "(s, " + texcoords + offsets + ")" + strSwizzle + ";";
    sampleProgram += "\n}\n";
  }
  else if(opcode == OPCODE_GATHER4_C || opcode == OPCODE_GATHER4_PO_C)
  {
    // comparison value
    StringFormat::snprintf(buf, 255, ", %.10f", lodOrCompareValue);

    sampleProgram = textureDecl + " : register(t0);\n" + samplerDecl + " : register(s0);\n\n";
    sampleProgram += funcRet + " main() : SV_Target0\n{\nreturn ";
    sampleProgram += "t.GatherCmp" + strGatherChannel + "(s, " + texcoords + buf + offsets + ")" +
                     strSwizzle + ";";
    sampleProgram += "\n}\n";
  }

  ID3D11VertexShader *vs =
      m_pDevice->GetShaderCache()->MakeVShader(vsProgram.c_str(), "main", "vs_5_0");
  ID3D11PixelShader *ps =
      m_pDevice->GetShaderCache()->MakePShader(sampleProgram.c_str(), "main", "ps_5_0");

  ID3D11DeviceContext *context = NULL;

  m_pDevice->GetImmediateContext(&context);

  // back up SRV/sampler on PS slot 0

  ID3D11ShaderResourceView *prevSRV = NULL;
  ID3D11SamplerState *prevSamp = NULL;

  context->PSGetShaderResources(0, 1, &prevSRV);
  context->PSGetSamplers(0, 1, &prevSamp);

  ID3D11ShaderResourceView *usedSRV = NULL;
  ID3D11SamplerState *usedSamp = NULL;

  // fetch SRV and sampler from the shader stage we're debugging that this opcode wants to load from
  switch(GetShaderType())
  {
    case DXBC::ShaderType::Vertex:
      context->VSGetShaderResources(resourceData.slot, 1, &usedSRV);
      context->VSGetSamplers(samplerData.slot, 1, &usedSamp);
      break;
    case DXBC::ShaderType::Hull:
      context->HSGetShaderResources(resourceData.slot, 1, &usedSRV);
      context->HSGetSamplers(samplerData.slot, 1, &usedSamp);
      break;
    case DXBC::ShaderType::Domain:
      context->DSGetShaderResources(resourceData.slot, 1, &usedSRV);
      context->DSGetSamplers(samplerData.slot, 1, &usedSamp);
      break;
    case DXBC::ShaderType::Geometry:
      context->GSGetShaderResources(resourceData.slot, 1, &usedSRV);
      context->GSGetSamplers(samplerData.slot, 1, &usedSamp);
      break;
    case DXBC::ShaderType::Pixel:
      context->PSGetShaderResources(resourceData.slot, 1, &usedSRV);
      context->PSGetSamplers(samplerData.slot, 1, &usedSamp);
      break;
    case DXBC::ShaderType::Compute:
      context->CSGetShaderResources(resourceData.slot, 1, &usedSRV);
      context->CSGetSamplers(samplerData.slot, 1, &usedSamp);
      break;
  }

  // set onto PS while we perform the sample
  context->PSSetShaderResources(0, 1, &usedSRV);
  if(opcode == OPCODE_SAMPLE_B && samplerData.bias != 0.0f)
  {
    RDCASSERT(usedSamp);

    D3D11_SAMPLER_DESC desc;
    usedSamp->GetDesc(&desc);

    desc.MipLODBias = RDCCLAMP(desc.MipLODBias + samplerData.bias, -15.99f, 15.99f);

    ID3D11SamplerState *replacementSamp = NULL;
    HRESULT hr = m_pDevice->CreateSamplerState(&desc, &replacementSamp);
    if(FAILED(hr))
    {
      RDCERR("Failed to create new sampler state in debugging HRESULT: %s", ToStr(hr).c_str());
    }
    else
    {
      context->PSSetSamplers(0, 1, &replacementSamp);
      SAFE_RELEASE(replacementSamp);
    }
  }
  else
  {
    context->PSSetSamplers(0, 1, &usedSamp);
  }

  context->IASetInputLayout(NULL);
  context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

  context->VSSetShader(vs, NULL, 0);
  context->PSSetShader(ps, NULL, 0);

  D3D11_VIEWPORT view = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
  context->RSSetViewports(1, &view);

  context->GSSetShader(NULL, NULL, 0);
  context->DSSetShader(NULL, NULL, 0);
  context->HSSetShader(NULL, NULL, 0);
  context->CSSetShader(NULL, NULL, 0);

  context->SOSetTargets(0, NULL, NULL);

  context->RSSetState(NULL);
  context->OMSetBlendState(NULL, NULL, (UINT)~0);
  context->OMSetDepthStencilState(NULL, 0);

  ID3D11RenderTargetView *rtv = NULL;

  ID3D11Texture2D *rtTex = NULL;
  ID3D11Texture2D *copyTex = NULL;

  D3D11_TEXTURE2D_DESC tdesc;

  RDCASSERT(retFmt != DXGI_FORMAT_UNKNOWN);

  tdesc.ArraySize = 1;
  tdesc.BindFlags = D3D11_BIND_RENDER_TARGET;
  tdesc.CPUAccessFlags = 0;
  tdesc.Format = retFmt;
  tdesc.Width = 1;
  tdesc.Height = 1;
  tdesc.MipLevels = 0;
  tdesc.MiscFlags = 0;
  tdesc.SampleDesc.Count = 1;
  tdesc.SampleDesc.Quality = 0;
  tdesc.Usage = D3D11_USAGE_DEFAULT;

  HRESULT hr = S_OK;

  hr = m_pDevice->CreateTexture2D(&tdesc, NULL, &rtTex);

  if(FAILED(hr))
  {
    RDCERR("Failed to create RT tex HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  tdesc.BindFlags = 0;
  tdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  tdesc.Usage = D3D11_USAGE_STAGING;

  hr = m_pDevice->CreateTexture2D(&tdesc, NULL, &copyTex);

  if(FAILED(hr))
  {
    RDCERR("Failed to create copy tex HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  D3D11_RENDER_TARGET_VIEW_DESC rtDesc;

  rtDesc.Format = retFmt;
  rtDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
  rtDesc.Texture2D.MipSlice = 0;

  hr = m_pDevice->CreateRenderTargetView(rtTex, &rtDesc, &rtv);

  if(FAILED(hr))
  {
    RDCERR("Failed to create rt rtv HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  context->OMSetRenderTargetsAndUnorderedAccessViews(1, &rtv, NULL, 0, 0, NULL, NULL);
  context->Draw(3, 0);

  context->CopyResource(copyTex, rtTex);

  D3D11_MAPPED_SUBRESOURCE mapped;
  hr = context->Map(copyTex, 0, D3D11_MAP_READ, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Failed to map results HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  ShaderVariable lookupResult("tex", 0.0f, 0.0f, 0.0f, 0.0f);

  memcpy(lookupResult.value.iv, mapped.pData, sizeof(uint32_t) * 4);

  context->Unmap(copyTex, 0);

  SAFE_RELEASE(rtTex);
  SAFE_RELEASE(copyTex);
  SAFE_RELEASE(rtv);
  SAFE_RELEASE(vs);
  SAFE_RELEASE(ps);

  // restore whatever was on PS slot 0 before we messed with it

  context->PSSetShaderResources(0, 1, &prevSRV);
  context->PSSetSamplers(0, 1, &prevSamp);

  SAFE_RELEASE(context);

  SAFE_RELEASE(prevSRV);
  SAFE_RELEASE(prevSamp);

  SAFE_RELEASE(usedSRV);
  SAFE_RELEASE(usedSamp);

  output = lookupResult;
  return true;
}

bool D3D11DebugAPIWrapper::CalculateMathIntrinsic(DXBCBytecode::OpcodeType opcode,
                                                  const ShaderVariable &input,
                                                  ShaderVariable &output1, ShaderVariable &output2)
{
  std::string csProgram =
      "RWBuffer<float4> outval : register(u0);\n"
      "cbuffer srcOper : register(b0) { float4 inval; };\n"
      "[numthreads(1, 1, 1)]\n"
      "void main() {\n";

  switch(opcode)
  {
    case DXBCBytecode::OPCODE_RCP: csProgram += "outval[0] = rcp(inval);\n"; break;
    case DXBCBytecode::OPCODE_RSQ: csProgram += "outval[0] = rsqrt(inval);\n"; break;
    case DXBCBytecode::OPCODE_EXP: csProgram += "outval[0] = exp2(inval);\n"; break;
    case DXBCBytecode::OPCODE_LOG: csProgram += "outval[0] = log2(inval);\n"; break;
    case DXBCBytecode::OPCODE_SINCOS: csProgram += "sincos(inval, outval[0], outval[1]);\n"; break;
    default: RDCERR("Unexpected opcode %d passed to CalculateMathIntrinsic", opcode); return false;
  }

  csProgram += "}\n";

  ID3D11ComputeShader *cs =
      m_pDevice->GetShaderCache()->MakeCShader(csProgram.c_str(), "main", "cs_5_0");

  ID3D11DeviceContext *context = NULL;
  m_pDevice->GetImmediateContext(&context);

  // back up CB/UAV on CS slot 0

  ID3D11Buffer *prevCB = NULL;
  ID3D11UnorderedAccessView *prevUAV = NULL;

  context->CSGetConstantBuffers(0, 1, &prevCB);
  context->CSGetUnorderedAccessViews(0, 1, &prevUAV);

  ID3D11Buffer *constBuf = NULL;

  D3D11_BUFFER_DESC cdesc;

  cdesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  cdesc.CPUAccessFlags = 0;
  cdesc.MiscFlags = 0;
  cdesc.StructureByteStride = sizeof(Vec4f);
  cdesc.ByteWidth = sizeof(Vec4f);
  cdesc.Usage = D3D11_USAGE_DEFAULT;

  D3D11_SUBRESOURCE_DATA operData = {};
  operData.pSysMem = &input.value.uv[0];
  operData.SysMemPitch = sizeof(Vec4f);
  operData.SysMemSlicePitch = sizeof(Vec4f);

  HRESULT hr = m_pDevice->CreateBuffer(&cdesc, &operData, &constBuf);
  if(FAILED(hr))
  {
    RDCERR("Failed to create constant buf HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  context->CSSetConstantBuffers(0, 1, &constBuf);

  context->CSSetShader(cs, NULL, 0);

  ID3D11UnorderedAccessView *uav = NULL;

  ID3D11Buffer *uavBuf = NULL;
  ID3D11Buffer *copyBuf = NULL;

  D3D11_BUFFER_DESC bdesc;

  bdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
  bdesc.CPUAccessFlags = 0;
  bdesc.MiscFlags = 0;
  bdesc.StructureByteStride = sizeof(Vec4f);
  bdesc.ByteWidth = sizeof(Vec4f) * 2;
  bdesc.Usage = D3D11_USAGE_DEFAULT;

  hr = m_pDevice->CreateBuffer(&bdesc, NULL, &uavBuf);

  if(FAILED(hr))
  {
    RDCERR("Failed to create UAV buf HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  bdesc.BindFlags = 0;
  bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  bdesc.Usage = D3D11_USAGE_STAGING;

  hr = m_pDevice->CreateBuffer(&bdesc, NULL, &copyBuf);

  if(FAILED(hr))
  {
    RDCERR("Failed to create copy buf HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};

  uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  uavDesc.Buffer.FirstElement = 0;
  uavDesc.Buffer.NumElements = 2;
  uavDesc.Buffer.Flags = 0;

  hr = m_pDevice->CreateUnorderedAccessView(uavBuf, &uavDesc, &uav);

  if(FAILED(hr))
  {
    RDCERR("Failed to create uav HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  context->CSSetUnorderedAccessViews(0, 1, &uav, NULL);
  context->Dispatch(1, 1, 1);

  context->CopyResource(copyBuf, uavBuf);

  D3D11_MAPPED_SUBRESOURCE mapped;
  hr = context->Map(copyBuf, 0, D3D11_MAP_READ, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Failed to map results HRESULT: %s", ToStr(hr).c_str());
    return false;
  }

  uint32_t *resA = (uint32_t *)mapped.pData;
  uint32_t *resB = resA + 4;

  memcpy(output1.value.uv, resA, sizeof(uint32_t) * 4);
  memcpy(output2.value.uv, resB, sizeof(uint32_t) * 4);

  context->Unmap(copyBuf, 0);

  SAFE_RELEASE(constBuf);
  SAFE_RELEASE(uavBuf);
  SAFE_RELEASE(copyBuf);
  SAFE_RELEASE(uav);
  SAFE_RELEASE(cs);

  // restore whatever was on CS slot 0 before we messed with it

  UINT append[] = {~0U};
  context->CSSetConstantBuffers(0, 1, &prevCB);
  context->CSSetUnorderedAccessViews(0, 1, &prevUAV, append);

  SAFE_RELEASE(context);

  SAFE_RELEASE(prevCB);
  SAFE_RELEASE(prevUAV);

  return true;
}

void D3D11DebugManager::CreateShaderGlobalState(ShaderDebug::GlobalState &global,
                                                DXBC::DXBCContainer *dxbc, uint32_t UAVStartSlot,
                                                ID3D11UnorderedAccessView **UAVs,
                                                ID3D11ShaderResourceView **SRVs)
{
  for(int i = 0; UAVs != NULL && i + UAVStartSlot < D3D11_1_UAV_SLOT_COUNT; i++)
  {
    int dsti = i + UAVStartSlot;
    if(UAVs[i])
    {
      ID3D11Resource *res = NULL;
      UAVs[i]->GetResource(&res);

      global.uavs[dsti].hiddenCounter = GetStructCount(UAVs[i]);

      D3D11_UNORDERED_ACCESS_VIEW_DESC udesc;
      UAVs[i]->GetDesc(&udesc);

      DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

      if(udesc.Format != DXGI_FORMAT_UNKNOWN)
        format = udesc.Format;

      if(format == DXGI_FORMAT_UNKNOWN)
      {
        if(WrappedID3D11Texture1D::IsAlloc(res))
        {
          D3D11_TEXTURE1D_DESC desc;
          ((WrappedID3D11Texture1D *)res)->GetDesc(&desc);
          format = desc.Format;
        }
        else if(WrappedID3D11Texture2D1::IsAlloc(res))
        {
          D3D11_TEXTURE2D_DESC desc;
          ((WrappedID3D11Texture2D1 *)res)->GetDesc(&desc);
          format = desc.Format;
        }
        else if(WrappedID3D11Texture3D1::IsAlloc(res))
        {
          D3D11_TEXTURE3D_DESC desc;
          ((WrappedID3D11Texture3D1 *)res)->GetDesc(&desc);
          format = desc.Format;
        }
      }

      if(format != DXGI_FORMAT_UNKNOWN)
      {
        ResourceFormat fmt = MakeResourceFormat(GetTypedFormat(udesc.Format));

        global.uavs[dsti].format.byteWidth = fmt.compByteWidth;
        global.uavs[dsti].format.numComps = fmt.compCount;
        global.uavs[dsti].format.fmt = fmt.compType;

        if(udesc.Format == DXGI_FORMAT_R11G11B10_FLOAT)
          global.uavs[dsti].format.byteWidth = 11;
        if(udesc.Format == DXGI_FORMAT_R10G10B10A2_UINT ||
           udesc.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
          global.uavs[dsti].format.byteWidth = 10;
      }

      if(udesc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
      {
        global.uavs[dsti].firstElement = udesc.Buffer.FirstElement;
        global.uavs[dsti].numElements = udesc.Buffer.NumElements;
      }

      if(res)
      {
        if(WrappedID3D11Buffer::IsAlloc(res))
        {
          GetBufferData((ID3D11Buffer *)res, 0, 0, global.uavs[dsti].data);
        }
        else
        {
          global.uavs[dsti].tex = true;

          uint32_t &rowPitch = global.uavs[dsti].rowPitch;
          uint32_t &depthPitch = global.uavs[dsti].depthPitch;

          bytebuf &data = global.uavs[dsti].data;

          if(udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1D ||
             udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
          {
            D3D11_TEXTURE1D_DESC desc;
            ((WrappedID3D11Texture1D *)res)->GetDesc(&desc);

            desc.MiscFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            desc.BindFlags = 0;
            desc.Usage = D3D11_USAGE_STAGING;

            ID3D11Texture1D *stagingTex = NULL;
            m_pDevice->CreateTexture1D(&desc, NULL, &stagingTex);

            m_pImmediateContext->CopyResource(stagingTex, res);

            D3D11_MAPPED_SUBRESOURCE mapped;
            m_pImmediateContext->Map(stagingTex, udesc.Texture1D.MipSlice, D3D11_MAP_READ, 0,
                                     &mapped);

            rowPitch = 0;
            depthPitch = 0;
            size_t datasize = GetByteSize(desc.Width, 1, 1, desc.Format, udesc.Texture1D.MipSlice);

            uint32_t numSlices = 1;

            byte *srcdata = (byte *)mapped.pData;
            if(udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE1DARRAY)
            {
              rowPitch = mapped.RowPitch;
              srcdata += udesc.Texture1DArray.FirstArraySlice * rowPitch;
              numSlices = udesc.Texture1DArray.ArraySize;
              datasize = numSlices * rowPitch;
            }

            data.resize(datasize);

            // copy with all padding etc intact
            memcpy(&data[0], srcdata, datasize);

            m_pImmediateContext->Unmap(stagingTex, udesc.Texture1D.MipSlice);

            SAFE_RELEASE(stagingTex);
          }
          else if(udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D ||
                  udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
          {
            D3D11_TEXTURE2D_DESC desc;
            ((WrappedID3D11Texture2D1 *)res)->GetDesc(&desc);

            desc.MiscFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            desc.BindFlags = 0;
            desc.Usage = D3D11_USAGE_STAGING;

            ID3D11Texture2D *stagingTex = NULL;
            m_pDevice->CreateTexture2D(&desc, NULL, &stagingTex);

            m_pImmediateContext->CopyResource(stagingTex, res);

            // MipSlice in union is shared between Texture2D and Texture2DArray unions, so safe to
            // use either
            D3D11_MAPPED_SUBRESOURCE mapped;
            m_pImmediateContext->Map(stagingTex, udesc.Texture2D.MipSlice, D3D11_MAP_READ, 0,
                                     &mapped);

            rowPitch = mapped.RowPitch;
            depthPitch = 0;
            size_t datasize = rowPitch * RDCMAX(1U, desc.Height >> udesc.Texture2D.MipSlice);

            uint32_t numSlices = 1;

            byte *srcdata = (byte *)mapped.pData;
            if(udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
            {
              depthPitch = mapped.DepthPitch;
              srcdata += udesc.Texture2DArray.FirstArraySlice * depthPitch;
              numSlices = udesc.Texture2DArray.ArraySize;
              datasize = numSlices * depthPitch;
            }

            // copy with all padding etc intact
            data.resize(datasize);

            memcpy(&data[0], srcdata, datasize);

            m_pImmediateContext->Unmap(stagingTex, udesc.Texture2D.MipSlice);

            SAFE_RELEASE(stagingTex);
          }
          else if(udesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE3D)
          {
            D3D11_TEXTURE3D_DESC desc;
            ((WrappedID3D11Texture3D1 *)res)->GetDesc(&desc);

            desc.MiscFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
            desc.BindFlags = 0;
            desc.Usage = D3D11_USAGE_STAGING;

            ID3D11Texture3D *stagingTex = NULL;
            m_pDevice->CreateTexture3D(&desc, NULL, &stagingTex);

            m_pImmediateContext->CopyResource(stagingTex, res);

            // MipSlice in union is shared between Texture2D and Texture2DArray unions, so safe to
            // use either
            D3D11_MAPPED_SUBRESOURCE mapped;
            m_pImmediateContext->Map(stagingTex, udesc.Texture3D.MipSlice, D3D11_MAP_READ, 0,
                                     &mapped);

            rowPitch = mapped.RowPitch;
            depthPitch = mapped.DepthPitch;

            byte *srcdata = (byte *)mapped.pData;
            srcdata += udesc.Texture3D.FirstWSlice * mapped.DepthPitch;
            uint32_t numSlices = udesc.Texture3D.WSize;
            size_t datasize = depthPitch * numSlices;

            data.resize(datasize);

            // copy with all padding etc intact
            memcpy(&data[0], srcdata, datasize);

            m_pImmediateContext->Unmap(stagingTex, udesc.Texture3D.MipSlice);

            SAFE_RELEASE(stagingTex);
          }
        }
      }

      SAFE_RELEASE(res);
    }
  }

  for(int i = 0; SRVs != NULL && i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++)
  {
    if(SRVs[i])
    {
      ID3D11Resource *res = NULL;
      SRVs[i]->GetResource(&res);

      D3D11_SHADER_RESOURCE_VIEW_DESC sdesc;
      SRVs[i]->GetDesc(&sdesc);

      if(sdesc.Format != DXGI_FORMAT_UNKNOWN)
      {
        ShaderDebug::FillViewFmt(sdesc.Format, global.srvs[i].format);
      }
      else
      {
        D3D11_RESOURCE_DIMENSION dim;
        res->GetType(&dim);

        if(dim == D3D11_RESOURCE_DIMENSION_BUFFER)
        {
          ID3D11Buffer *buf = (ID3D11Buffer *)res;
          D3D11_BUFFER_DESC bufdesc;
          buf->GetDesc(&bufdesc);

          global.srvs[i].format.stride = bufdesc.StructureByteStride;

          // if we didn't get a type from the SRV description, try to pull it from the declaration
          ShaderDebug::LookupSRVFormatFromShaderReflection(*dxbc->GetReflection(), (uint32_t)i,
                                                           global.srvs[i].format);
        }
      }

      if(sdesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER)
      {
        // I know this isn't what the docs say, but as best as I can tell
        // this is how it's used.
        global.srvs[i].firstElement = sdesc.Buffer.FirstElement;
        global.srvs[i].numElements = sdesc.Buffer.NumElements;
      }
      else if(sdesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX)
      {
        global.srvs[i].firstElement = sdesc.BufferEx.FirstElement;
        global.srvs[i].numElements = sdesc.BufferEx.NumElements;
      }

      if(res)
      {
        if(WrappedID3D11Buffer::IsAlloc(res))
        {
          GetBufferData((ID3D11Buffer *)res, 0, 0, global.srvs[i].data);
        }
      }

      SAFE_RELEASE(res);
    }
  }

  global.PopulateGroupshared(dxbc->GetDXBCByteCode());
}

ShaderDebugTrace D3D11Replay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                          uint32_t idx, uint32_t instOffset, uint32_t vertOffset)
{
  using namespace DXBCBytecode;
  using namespace ShaderDebug;

  D3D11MarkerRegion debugpixRegion(
      StringFormat::Fmt("DebugVertex @ %u of (%u,%u,%u)", eventId, vertid, instid, idx));

  ShaderDebugTrace empty;

  const DrawcallDescription *draw = m_pDevice->GetDrawcall(eventId);

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  ID3D11VertexShader *stateVS = NULL;
  m_pImmediateContext->VSGetShader(&stateVS, NULL, NULL);

  WrappedID3D11Shader<ID3D11VertexShader> *vs = (WrappedID3D11Shader<ID3D11VertexShader> *)stateVS;

  SAFE_RELEASE(stateVS);

  if(!vs)
    return empty;

  DXBC::DXBCContainer *dxbc = vs->GetDXBC();
  const ShaderReflection &refl = vs->GetDetails();

  if(!dxbc)
    return empty;

  dxbc->GetDisassembly();

  D3D11RenderState *rs = m_pImmediateContext->GetCurrentPipelineState();

  std::vector<D3D11_INPUT_ELEMENT_DESC> inputlayout = m_pDevice->GetLayoutDesc(rs->IA.Layout);

  std::set<UINT> vertexbuffers;
  uint32_t trackingOffs[32] = {0};

  UINT MaxStepRate = 1U;

  // need special handling for other step rates
  for(size_t i = 0; i < inputlayout.size(); i++)
  {
    if(inputlayout[i].InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA &&
       inputlayout[i].InstanceDataStepRate < draw->numInstances)
      MaxStepRate = RDCMAX(inputlayout[i].InstanceDataStepRate, MaxStepRate);

    UINT slot =
        RDCCLAMP(inputlayout[i].InputSlot, 0U, UINT(D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1));

    vertexbuffers.insert(slot);

    if(inputlayout[i].AlignedByteOffset == ~0U)
    {
      inputlayout[i].AlignedByteOffset = trackingOffs[slot];
    }
    else
    {
      trackingOffs[slot] = inputlayout[i].AlignedByteOffset;
    }

    ResourceFormat fmt = MakeResourceFormat(inputlayout[i].Format);

    trackingOffs[slot] += fmt.compByteWidth * fmt.compCount;
  }

  bytebuf vertData[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  bytebuf *instData = new bytebuf[MaxStepRate * D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  bytebuf staticData[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

  for(auto it = vertexbuffers.begin(); it != vertexbuffers.end(); ++it)
  {
    UINT i = *it;
    if(rs->IA.VBs[i])
    {
      GetDebugManager()->GetBufferData(rs->IA.VBs[i],
                                       rs->IA.Offsets[i] + rs->IA.Strides[i] * (vertOffset + idx),
                                       rs->IA.Strides[i], vertData[i]);

      for(UINT isr = 1; isr <= MaxStepRate; isr++)
      {
        GetDebugManager()->GetBufferData(
            rs->IA.VBs[i], rs->IA.Offsets[i] + rs->IA.Strides[i] * (instOffset + (instid / isr)),
            rs->IA.Strides[i], instData[i * MaxStepRate + isr - 1]);
      }

      GetDebugManager()->GetBufferData(rs->IA.VBs[i],
                                       rs->IA.Offsets[i] + rs->IA.Strides[i] * instOffset,
                                       rs->IA.Strides[i], staticData[i]);
    }
  }

  bytebuf cbufData[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

  for(int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
    if(rs->VS.ConstantBuffers[i])
      GetDebugManager()->GetBufferData(rs->VS.ConstantBuffers[i],
                                       rs->VS.CBOffsets[i] * sizeof(Vec4f), 0, cbufData[i]);

  ShaderDebugTrace ret;

  GlobalState global;
  GetDebugManager()->CreateShaderGlobalState(global, dxbc, 0, NULL, rs->VS.SRVs);
  State initialState;
  CreateShaderDebugStateAndTrace(initialState, ret, -1, dxbc, refl, cbufData);

  for(size_t i = 0; i < ret.inputs.size(); i++)
  {
    if(dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::Undefined ||
       dxbc->GetReflection()->InputSig[i].systemValue ==
           ShaderBuiltin::Position)    // SV_Position seems to get promoted
                                       // automatically, but it's invalid for
                                       // vertex input
    {
      const D3D11_INPUT_ELEMENT_DESC *el = NULL;

      std::string signame = strlower(dxbc->GetReflection()->InputSig[i].semanticName);

      for(size_t l = 0; l < inputlayout.size(); l++)
      {
        std::string layoutname = strlower(inputlayout[l].SemanticName);

        if(signame == layoutname &&
           dxbc->GetReflection()->InputSig[i].semanticIndex == inputlayout[l].SemanticIndex)
        {
          el = &inputlayout[l];
          break;
        }
        if(signame == layoutname + ToStr(inputlayout[l].SemanticIndex))
        {
          el = &inputlayout[l];
          break;
        }
      }

      RDCASSERT(el);

      if(!el)
        continue;

      byte *srcData = NULL;
      size_t dataSize = 0;

      if(el->InputSlotClass == D3D11_INPUT_PER_VERTEX_DATA)
      {
        if(vertData[el->InputSlot].size() >= el->AlignedByteOffset)
        {
          srcData = &vertData[el->InputSlot][el->AlignedByteOffset];
          dataSize = vertData[el->InputSlot].size() - el->AlignedByteOffset;
        }
      }
      else
      {
        if(el->InstanceDataStepRate == 0 || el->InstanceDataStepRate >= draw->numInstances)
        {
          if(staticData[el->InputSlot].size() >= el->AlignedByteOffset)
          {
            srcData = &staticData[el->InputSlot][el->AlignedByteOffset];
            dataSize = staticData[el->InputSlot].size() - el->AlignedByteOffset;
          }
        }
        else
        {
          UINT isrIdx = el->InputSlot * MaxStepRate + (el->InstanceDataStepRate - 1);
          if(instData[isrIdx].size() >= el->AlignedByteOffset)
          {
            srcData = &instData[isrIdx][el->AlignedByteOffset];
            dataSize = instData[isrIdx].size() - el->AlignedByteOffset;
          }
        }
      }

      ResourceFormat fmt = MakeResourceFormat(el->Format);

      // more data needed than is provided
      if(dxbc->GetReflection()->InputSig[i].compCount > fmt.compCount)
      {
        ret.inputs[i].value.u.w = 1;

        if(fmt.compType == CompType::Float)
          ret.inputs[i].value.f.w = 1.0f;
      }

      // interpret resource format types
      if(fmt.Special())
      {
        Vec3f *v3 = (Vec3f *)ret.inputs[i].value.fv;
        Vec4f *v4 = (Vec4f *)ret.inputs[i].value.fv;

        // only pull in all or nothing from these,
        // if there's only e.g. 3 bytes remaining don't read and unpack some of
        // a 4-byte resource format type
        size_t packedsize = 4;
        if(fmt.type == ResourceFormatType::R5G5B5A1 || fmt.type == ResourceFormatType::R5G6B5 ||
           fmt.type == ResourceFormatType::R4G4B4A4)
          packedsize = 2;

        if(srcData == NULL || packedsize > dataSize)
        {
          ret.inputs[i].value.u.x = ret.inputs[i].value.u.y = ret.inputs[i].value.u.z =
              ret.inputs[i].value.u.w = 0;
        }
        else if(fmt.type == ResourceFormatType::R5G5B5A1)
        {
          RDCASSERT(fmt.BGRAOrder());
          uint16_t packed = ((uint16_t *)srcData)[0];
          *v4 = ConvertFromB5G5R5A1(packed);
        }
        else if(fmt.type == ResourceFormatType::R5G6B5)
        {
          RDCASSERT(fmt.BGRAOrder());
          uint16_t packed = ((uint16_t *)srcData)[0];
          *v3 = ConvertFromB5G6R5(packed);
        }
        else if(fmt.type == ResourceFormatType::R4G4B4A4)
        {
          RDCASSERT(fmt.BGRAOrder());
          uint16_t packed = ((uint16_t *)srcData)[0];
          *v4 = ConvertFromB4G4R4A4(packed);
        }
        else if(fmt.type == ResourceFormatType::R10G10B10A2)
        {
          uint32_t packed = ((uint32_t *)srcData)[0];

          if(fmt.compType == CompType::UInt)
          {
            ret.inputs[i].value.u.z = (packed >> 0) & 0x3ff;
            ret.inputs[i].value.u.y = (packed >> 10) & 0x3ff;
            ret.inputs[i].value.u.x = (packed >> 20) & 0x3ff;
            ret.inputs[i].value.u.w = (packed >> 30) & 0x003;
          }
          else
          {
            *v4 = ConvertFromR10G10B10A2(packed);
          }
        }
        else if(fmt.type == ResourceFormatType::R11G11B10)
        {
          uint32_t packed = ((uint32_t *)srcData)[0];
          *v3 = ConvertFromR11G11B10(packed);
        }
      }
      else
      {
        for(uint32_t c = 0; c < fmt.compCount; c++)
        {
          if(srcData == NULL || fmt.compByteWidth > dataSize)
          {
            ret.inputs[i].value.uv[c] = 0;
            continue;
          }

          dataSize -= fmt.compByteWidth;

          if(fmt.compByteWidth == 1)
          {
            byte *src = srcData + c * fmt.compByteWidth;

            if(fmt.compType == CompType::UInt)
              ret.inputs[i].value.uv[c] = *src;
            else if(fmt.compType == CompType::SInt)
              ret.inputs[i].value.iv[c] = *((int8_t *)src);
            else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
              ret.inputs[i].value.fv[c] = float(*src) / 255.0f;
            else if(fmt.compType == CompType::SNorm)
            {
              signed char *schar = (signed char *)src;

              // -128 is mapped to -1, then -127 to -127 are mapped to -1 to 1
              if(*schar == -128)
                ret.inputs[i].value.fv[c] = -1.0f;
              else
                ret.inputs[i].value.fv[c] = float(*schar) / 127.0f;
            }
            else
              RDCERR("Unexpected component type");
          }
          else if(fmt.compByteWidth == 2)
          {
            uint16_t *src = (uint16_t *)(srcData + c * fmt.compByteWidth);

            if(fmt.compType == CompType::Float)
              ret.inputs[i].value.fv[c] = ConvertFromHalf(*src);
            else if(fmt.compType == CompType::UInt)
              ret.inputs[i].value.uv[c] = *src;
            else if(fmt.compType == CompType::SInt)
              ret.inputs[i].value.iv[c] = *((int16_t *)src);
            else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
              ret.inputs[i].value.fv[c] = float(*src) / float(UINT16_MAX);
            else if(fmt.compType == CompType::SNorm)
            {
              int16_t *sint = (int16_t *)src;

              // -32768 is mapped to -1, then -32767 to -32767 are mapped to -1 to 1
              if(*sint == -32768)
                ret.inputs[i].value.fv[c] = -1.0f;
              else
                ret.inputs[i].value.fv[c] = float(*sint) / 32767.0f;
            }
            else
              RDCERR("Unexpected component type");
          }
          else if(fmt.compByteWidth == 4)
          {
            uint32_t *src = (uint32_t *)(srcData + c * fmt.compByteWidth);

            if(fmt.compType == CompType::Float || fmt.compType == CompType::UInt ||
               fmt.compType == CompType::SInt)
              memcpy(&ret.inputs[i].value.uv[c], src, 4);
            else
              RDCERR("Unexpected component type");
          }
        }

        if(fmt.BGRAOrder())
        {
          RDCASSERT(fmt.compCount == 4);
          std::swap(ret.inputs[i].value.fv[2], ret.inputs[i].value.fv[0]);
        }
      }
    }
    else if(dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::VertexIndex)
    {
      uint32_t sv_vertid = vertid;

      if(draw->flags & DrawFlags::Indexed)
        sv_vertid = idx;

      if(dxbc->GetReflection()->InputSig[i].compType == CompType::Float)
        ret.inputs[i].value.f.x = ret.inputs[i].value.f.y = ret.inputs[i].value.f.z =
            ret.inputs[i].value.f.w = (float)sv_vertid;
      else
        ret.inputs[i].value.u.x = ret.inputs[i].value.u.y = ret.inputs[i].value.u.z =
            ret.inputs[i].value.u.w = sv_vertid;
    }
    else if(dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::InstanceIndex)
    {
      if(dxbc->GetReflection()->InputSig[i].compType == CompType::Float)
        ret.inputs[i].value.f.x = ret.inputs[i].value.f.y = ret.inputs[i].value.f.z =
            ret.inputs[i].value.f.w = (float)instid;
      else
        ret.inputs[i].value.u.x = ret.inputs[i].value.u.y = ret.inputs[i].value.u.z =
            ret.inputs[i].value.u.w = instid;
    }
    else
    {
      RDCERR("Unhandled system value semantic on VS input");
    }
  }

  delete[] instData;

  State last;

  std::vector<ShaderDebugState> states;

  if(dxbc->GetDebugInfo())
    dxbc->GetDebugInfo()->GetLocals(0, dxbc->GetDXBCByteCode()->GetInstruction(0).offset,
                                    initialState.locals);

  states.push_back((State)initialState);

  D3D11MarkerRegion simloop("Simulation Loop");

  D3D11DebugAPIWrapper apiWrapper(m_pDevice, dxbc, global);

  for(int cycleCounter = 0;; cycleCounter++)
  {
    if(initialState.Finished())
      break;

    initialState = initialState.GetNext(global, &apiWrapper, NULL);

    if(dxbc->GetDebugInfo())
    {
      const DXBCBytecode::Operation &op =
          dxbc->GetDXBCByteCode()->GetInstruction((size_t)initialState.nextInstruction);
      dxbc->GetDebugInfo()->GetLocals(initialState.nextInstruction, op.offset, initialState.locals);
    }

    states.push_back((State)initialState);

    if(cycleCounter == SHADER_DEBUG_WARN_THRESHOLD)
    {
      if(PromptDebugTimeout(cycleCounter))
        break;
    }
  }

  ret.states = states;

  ret.hasLocals = dxbc->GetDebugInfo() && dxbc->GetDebugInfo()->HasLocals();

  ret.lineInfo.resize(dxbc->GetDXBCByteCode()->GetNumInstructions());
  for(size_t i = 0; dxbc->GetDebugInfo() && i < dxbc->GetDXBCByteCode()->GetNumInstructions(); i++)
  {
    const DXBCBytecode::Operation &op = dxbc->GetDXBCByteCode()->GetInstruction(i);
    dxbc->GetDebugInfo()->GetLineInfo(i, op.offset, ret.lineInfo[i]);
  }

  return ret;
}

ShaderDebugTrace D3D11Replay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y, uint32_t sample,
                                         uint32_t primitive)
{
  using namespace DXBCBytecode;
  using namespace ShaderDebug;

  D3D11MarkerRegion debugpixRegion(
      StringFormat::Fmt("DebugPixel @ %u of (%u,%u) %u / %u", eventId, x, y, sample, primitive));

  ShaderDebugTrace empty;

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  ID3D11PixelShader *statePS = NULL;
  m_pImmediateContext->PSGetShader(&statePS, NULL, NULL);

  WrappedID3D11Shader<ID3D11PixelShader> *ps = (WrappedID3D11Shader<ID3D11PixelShader> *)statePS;

  SAFE_RELEASE(statePS);

  ID3D11GeometryShader *stateGS = NULL;
  m_pImmediateContext->GSGetShader(&stateGS, NULL, NULL);

  WrappedID3D11Shader<ID3D11GeometryShader> *gs =
      (WrappedID3D11Shader<ID3D11GeometryShader> *)stateGS;

  SAFE_RELEASE(stateGS);

  ID3D11DomainShader *stateDS = NULL;
  m_pImmediateContext->DSGetShader(&stateDS, NULL, NULL);

  WrappedID3D11Shader<ID3D11DomainShader> *ds = (WrappedID3D11Shader<ID3D11DomainShader> *)stateDS;

  SAFE_RELEASE(stateDS);

  ID3D11VertexShader *stateVS = NULL;
  m_pImmediateContext->VSGetShader(&stateVS, NULL, NULL);

  WrappedID3D11Shader<ID3D11VertexShader> *vs = (WrappedID3D11Shader<ID3D11VertexShader> *)stateVS;

  SAFE_RELEASE(stateVS);

  if(!ps)
    return empty;

  D3D11RenderState *rs = m_pImmediateContext->GetCurrentPipelineState();

  DXBC::DXBCContainer *dxbc = ps->GetDXBC();
  const ShaderReflection &refl = ps->GetDetails();

  if(!dxbc)
    return empty;

  dxbc->GetDisassembly();

  DXBC::DXBCContainer *prevdxbc = NULL;

  if(prevdxbc == NULL && gs != NULL)
    prevdxbc = gs->GetDXBC();
  if(prevdxbc == NULL && ds != NULL)
    prevdxbc = ds->GetDXBC();
  if(prevdxbc == NULL && vs != NULL)
    prevdxbc = vs->GetDXBC();
  RDCASSERT(prevdxbc);

  std::vector<PSInputElement> initialValues;
  std::vector<std::string> floatInputs;
  std::vector<std::string> inputVarNames;
  std::string extractHlsl;
  int structureStride = 0;

  ShaderDebug::GatherPSInputDataForInitialValues(*dxbc->GetReflection(), *prevdxbc->GetReflection(),
                                                 initialValues, floatInputs, inputVarNames,
                                                 extractHlsl, structureStride);

  uint32_t overdrawLevels = 100;    // maximum number of overdraw levels

  uint32_t uavslot = 0;

  ID3D11DepthStencilView *depthView = NULL;
  ID3D11RenderTargetView *rtView = NULL;
  // preserve at least one render target and/or the depth view, so that
  // we have the right multisample level on output either way
  m_pImmediateContext->OMGetRenderTargets(1, &rtView, &depthView);
  if(rtView != NULL)
    uavslot = 1;

  // get the multisample count
  uint32_t outputSampleCount = 1;

  {
    ID3D11Resource *res = NULL;

    if(depthView)
      depthView->GetResource(&res);
    else if(rtView)
      rtView->GetResource(&res);

    if(res)
    {
      D3D11_RESOURCE_DIMENSION dim = D3D11_RESOURCE_DIMENSION_UNKNOWN;
      res->GetType(&dim);

      if(dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D)
      {
        D3D11_TEXTURE2D_DESC desc;
        ((ID3D11Texture2D *)res)->GetDesc(&desc);

        outputSampleCount = RDCMAX(1U, desc.SampleDesc.Count);
      }

      SAFE_RELEASE(res);
    }
  }

  std::set<GlobalState::SampleEvalCacheKey> evalSampleCacheData;

  uint64_t sampleEvalRegisterMask = 0;

  // if we're not rendering at MSAA, no need to fill the cache because evaluates will all return the
  // plain input anyway.
  if(outputSampleCount > 1)
  {
    // scan the instructions to see if it contains any evaluates.
    for(size_t i = 0; i < dxbc->GetDXBCByteCode()->GetNumInstructions(); i++)
    {
      const Operation &op = dxbc->GetDXBCByteCode()->GetInstruction(i);

      // skip any non-eval opcodes
      if(op.operation != OPCODE_EVAL_CENTROID && op.operation != OPCODE_EVAL_SAMPLE_INDEX &&
         op.operation != OPCODE_EVAL_SNAPPED)
        continue;

      // the generation of this key must match what we'll generate in the corresponding lookup
      GlobalState::SampleEvalCacheKey key;

      // all the eval opcodes have rDst, vIn as the first two operands
      key.inputRegisterIndex = (int32_t)op.operands[1].indices[0].index;

      for(int c = 0; c < 4; c++)
      {
        if(op.operands[0].comps[c] == 0xff)
          break;

        key.numComponents = c + 1;
      }

      key.firstComponent = op.operands[1].comps[op.operands[0].comps[0]];

      sampleEvalRegisterMask |= 1ULL << key.inputRegisterIndex;

      if(op.operation == OPCODE_EVAL_CENTROID)
      {
        // nothing to do - default key is centroid, sample is -1 and offset x/y is 0
        evalSampleCacheData.insert(key);
      }
      else if(op.operation == OPCODE_EVAL_SAMPLE_INDEX)
      {
        if(op.operands[2].type == TYPE_IMMEDIATE32 || op.operands[2].type == TYPE_IMMEDIATE64)
        {
          // hooray, only sampling a single index, just add this key
          key.sample = (int32_t)op.operands[2].values[0];

          evalSampleCacheData.insert(key);
        }
        else
        {
          // parameter is a register and we don't know which sample will be needed, fetch them all.
          // In most cases this will be a loop over them all, so they'll all be needed anyway
          for(uint32_t c = 0; c < outputSampleCount; c++)
          {
            key.sample = (int32_t)c;
            evalSampleCacheData.insert(key);
          }
        }
      }
      else if(op.operation == OPCODE_EVAL_SNAPPED)
      {
        if(op.operands[2].type == TYPE_IMMEDIATE32 || op.operands[2].type == TYPE_IMMEDIATE64)
        {
          // hooray, only sampling a single offset, just add this key
          key.offsetx = (int32_t)op.operands[2].values[0];
          key.offsety = (int32_t)op.operands[2].values[1];

          evalSampleCacheData.insert(key);
        }
        else
        {
          m_pDevice->AddDebugMessage(
              MessageCategory::Shaders, MessageSeverity::Medium, MessageSource::RuntimeWarning,
              "EvaluateAttributeSnapped called with dynamic parameter, caching all possible "
              "evaluations which could have performance impact.");

          for(key.offsetx = -8; key.offsetx <= 7; key.offsetx++)
            for(key.offsety = -8; key.offsety <= 7; key.offsety++)
              evalSampleCacheData.insert(key);
        }
      }
    }
  }

  extractHlsl += R"(
struct PSInitialData
{
  // metadata we need ourselves
  uint hit;
  float3 pos;
  uint prim;
  uint fface;
  uint sample;
  uint covge;
  float derivValid;

  // input values
  PSInput IN;
  PSInput INddx;
  PSInput INddy;
  PSInput INddxfine;
  PSInput INddyfine;
};

)";

  extractHlsl +=
      "RWStructuredBuffer<PSInitialData> PSInitialBuffer : register(u" + ToStr(uavslot) + ");\n\n";

  if(!evalSampleCacheData.empty())
  {
    // float4 is wasteful in some cases but it's easier than using ByteAddressBuffer and manual
    // packing
    extractHlsl += "RWBuffer<float4> PSEvalBuffer : register(u" + ToStr(uavslot + 1) + ");\n\n";
  }

  extractHlsl += R"(
void ExtractInputsPS(PSInput IN, float4 debug_pixelPos : SV_Position, uint prim : SV_PrimitiveID,
                     uint sample : SV_SampleIndex, uint covge : SV_Coverage,
                     bool fface : SV_IsFrontFace)
{
)";
  extractHlsl += "  uint idx = " + ToStr(overdrawLevels) + ";\n";
  extractHlsl += StringFormat::Fmt(
      "  if(abs(debug_pixelPos.x - %u.5) < 0.5f && abs(debug_pixelPos.y - %u.5) < 0.5f)\n", x, y);
  extractHlsl += "    InterlockedAdd(PSInitialBuffer[0].hit, 1, idx);\n\n";
  extractHlsl += "  idx = min(idx, " + ToStr(overdrawLevels) + ");\n\n";
  extractHlsl += "  PSInitialBuffer[idx].pos = debug_pixelPos.xyz;\n";
  extractHlsl += "  PSInitialBuffer[idx].prim = prim;\n";
  extractHlsl += "  PSInitialBuffer[idx].fface = fface;\n";
  extractHlsl += "  PSInitialBuffer[idx].covge = covge;\n";
  extractHlsl += "  PSInitialBuffer[idx].sample = sample;\n";
  extractHlsl += "  PSInitialBuffer[idx].IN = IN;\n";
  extractHlsl += "  PSInitialBuffer[idx].derivValid = ddx(debug_pixelPos.x);\n";
  extractHlsl += "  PSInitialBuffer[idx].INddx = (PSInput)0;\n";
  extractHlsl += "  PSInitialBuffer[idx].INddy = (PSInput)0;\n";
  extractHlsl += "  PSInitialBuffer[idx].INddxfine = (PSInput)0;\n";
  extractHlsl += "  PSInitialBuffer[idx].INddyfine = (PSInput)0;\n";

  if(!evalSampleCacheData.empty())
  {
    extractHlsl += StringFormat::Fmt("  uint evalIndex = idx * %zu;\n", evalSampleCacheData.size());

    uint32_t evalIdx = 0;
    for(const GlobalState::SampleEvalCacheKey &key : evalSampleCacheData)
    {
      uint32_t keyMask = 0;

      for(int32_t i = 0; i < key.numComponents; i++)
        keyMask |= (1 << (key.firstComponent + i));

      // find the name of the variable matching the operand, in the case of merged input variables.
      std::string name, swizzle = "xyzw";
      for(size_t i = 0; i < dxbc->GetReflection()->InputSig.size(); i++)
      {
        if(dxbc->GetReflection()->InputSig[i].regIndex == (uint32_t)key.inputRegisterIndex &&
           dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::Undefined &&
           (dxbc->GetReflection()->InputSig[i].regChannelMask & keyMask) == keyMask)
        {
          name = inputVarNames[i];

          if(!name.empty())
            break;
        }
      }

      swizzle.resize(key.numComponents);

      if(name.empty())
      {
        RDCERR("Couldn't find matching input variable for v%d [%d:%d]", key.inputRegisterIndex,
               key.firstComponent, key.numComponents);
        extractHlsl += StringFormat::Fmt("  PSEvalBuffer[evalIndex+%u] = 0;\n", evalIdx);
        evalIdx++;
        continue;
      }

      name = StringFormat::Fmt("IN.%s.%s", name.c_str(), swizzle.c_str());

      // we must write all components, so just swizzle the values - they'll be ignored later.
      std::string expandSwizzle = swizzle;
      while(expandSwizzle.size() < 4)
        expandSwizzle.push_back('x');

      if(key.sample >= 0)
      {
        extractHlsl += StringFormat::Fmt(
            "  PSEvalBuffer[evalIndex+%u] = EvaluateAttributeAtSample(%s, %d).%s;\n", evalIdx,
            name.c_str(), key.sample, expandSwizzle.c_str());
      }
      else
      {
        // we don't need to special-case EvaluateAttributeAtCentroid, since it's just a case with
        // 0,0
        extractHlsl += StringFormat::Fmt(
            "  PSEvalBuffer[evalIndex+%u] = EvaluateAttributeSnapped(%s, int2(%d, %d)).%s;\n",
            evalIdx, name.c_str(), key.offsetx, key.offsety, expandSwizzle.c_str());
      }
      evalIdx++;
    }
  }

  for(size_t i = 0; i < floatInputs.size(); i++)
  {
    const std::string &name = floatInputs[i];
    extractHlsl += "  PSInitialBuffer[idx].INddx." + name + " = ddx(IN." + name + ");\n";
    extractHlsl += "  PSInitialBuffer[idx].INddy." + name + " = ddy(IN." + name + ");\n";
    extractHlsl += "  PSInitialBuffer[idx].INddxfine." + name + " = ddx_fine(IN." + name + ");\n";
    extractHlsl += "  PSInitialBuffer[idx].INddyfine." + name + " = ddy_fine(IN." + name + ");\n";
  }
  extractHlsl += "\n}";

  ID3D11PixelShader *extract =
      m_pDevice->GetShaderCache()->MakePShader(extractHlsl.c_str(), "ExtractInputsPS", "ps_5_0");

  uint32_t structStride = sizeof(uint32_t)       // uint hit;
                          + sizeof(float) * 3    // float3 pos;
                          + sizeof(uint32_t)     // uint prim;
                          + sizeof(uint32_t)     // uint fface;
                          + sizeof(uint32_t)     // uint sample;
                          + sizeof(uint32_t)     // uint covge;
                          + sizeof(float)        // float derivValid;
                          +
                          structureStride * 5;    // PSInput IN, INddx, INddy, INddxfine, INddyfine;

  HRESULT hr = S_OK;

  D3D11_BUFFER_DESC bdesc;
  bdesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
  bdesc.CPUAccessFlags = 0;
  bdesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  bdesc.Usage = D3D11_USAGE_DEFAULT;
  bdesc.StructureByteStride = structStride;
  bdesc.ByteWidth = structStride * (overdrawLevels + 1);

  ID3D11Buffer *initialBuf = NULL;
  hr = m_pDevice->CreateBuffer(&bdesc, NULL, &initialBuf);

  if(FAILED(hr))
  {
    RDCERR("Failed to create buffer HRESULT: %s", ToStr(hr).c_str());
    return empty;
  }

  ID3D11Buffer *evalBuf = NULL;
  if(!evalSampleCacheData.empty())
  {
    bdesc.StructureByteStride = 0;
    bdesc.MiscFlags = 0;
    bdesc.ByteWidth = UINT(evalSampleCacheData.size() * sizeof(Vec4f) * (overdrawLevels + 1));

    hr = m_pDevice->CreateBuffer(&bdesc, NULL, &evalBuf);

    if(FAILED(hr))
    {
      RDCERR("Failed to create buffer HRESULT: %s", ToStr(hr).c_str());
      return empty;
    }
  }

  bdesc.BindFlags = 0;
  bdesc.MiscFlags = 0;
  bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  bdesc.Usage = D3D11_USAGE_STAGING;
  bdesc.StructureByteStride = 0;
  bdesc.ByteWidth = structStride * (overdrawLevels + 1);

  ID3D11Buffer *initialStageBuf = NULL;
  hr = m_pDevice->CreateBuffer(&bdesc, NULL, &initialStageBuf);

  if(FAILED(hr))
  {
    RDCERR("Failed to create buffer HRESULT: %s", ToStr(hr).c_str());
    return empty;
  }

  uint32_t evalStructStride = uint32_t(evalSampleCacheData.size() * sizeof(Vec4f));

  ID3D11Buffer *evalStageBuf = NULL;
  if(evalBuf)
  {
    bdesc.ByteWidth = evalStructStride * (overdrawLevels + 1);

    hr = m_pDevice->CreateBuffer(&bdesc, NULL, &evalStageBuf);

    if(FAILED(hr))
    {
      RDCERR("Failed to create buffer HRESULT: %s", ToStr(hr).c_str());
      return empty;
    }
  }

  D3D11_UNORDERED_ACCESS_VIEW_DESC uavdesc;
  uavdesc.Format = DXGI_FORMAT_UNKNOWN;
  uavdesc.Buffer.FirstElement = 0;
  uavdesc.Buffer.Flags = 0;
  uavdesc.Buffer.NumElements = overdrawLevels + 1;
  uavdesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

  ID3D11UnorderedAccessView *initialUAV = NULL;
  hr = m_pDevice->CreateUnorderedAccessView(initialBuf, &uavdesc, &initialUAV);

  if(FAILED(hr))
  {
    RDCERR("Failed to create buffer HRESULT: %s", ToStr(hr).c_str());
    return empty;
  }

  ID3D11UnorderedAccessView *evalUAV = NULL;
  if(evalBuf)
  {
    uavdesc.Buffer.NumElements = (overdrawLevels + 1) * (uint32_t)evalSampleCacheData.size();
    uavdesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    hr = m_pDevice->CreateUnorderedAccessView(evalBuf, &uavdesc, &evalUAV);

    if(FAILED(hr))
    {
      RDCERR("Failed to create buffer HRESULT: %s", ToStr(hr).c_str());
      return empty;
    }
  }

  UINT zero = 0;
  m_pImmediateContext->ClearUnorderedAccessViewUint(initialUAV, &zero);
  if(evalUAV)
    m_pImmediateContext->ClearUnorderedAccessViewUint(evalUAV, &zero);

  ID3D11UnorderedAccessView *uavs[] = {initialUAV, evalUAV};

  UINT count = (UINT)-1;
  m_pImmediateContext->OMSetRenderTargetsAndUnorderedAccessViews(uavslot, &rtView, depthView,
                                                                 uavslot, 2, uavs, &count);
  m_pImmediateContext->PSSetShader(extract, NULL, 0);

  SAFE_RELEASE(rtView);
  SAFE_RELEASE(depthView);

  {
    D3D11MarkerRegion initState("Replaying event for initial states");

    m_pDevice->ReplayLog(0, eventId, eReplay_OnlyDraw);

    m_pImmediateContext->CopyResource(initialStageBuf, initialBuf);
    if(evalStageBuf)
      m_pImmediateContext->CopyResource(evalStageBuf, evalBuf);
  }

  D3D11_MAPPED_SUBRESOURCE mapped;
  hr = m_pImmediateContext->Map(initialStageBuf, 0, D3D11_MAP_READ, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Failed to map stage buff HRESULT: %s", ToStr(hr).c_str());
    return empty;
  }

  byte *initialData = new byte[structStride * (overdrawLevels + 1)];
  memcpy(initialData, mapped.pData, structStride * (overdrawLevels + 1));

  m_pImmediateContext->Unmap(initialStageBuf, 0);

  byte *evalData = NULL;

  if(evalStageBuf)
  {
    hr = m_pImmediateContext->Map(evalStageBuf, 0, D3D11_MAP_READ, 0, &mapped);

    if(FAILED(hr))
    {
      RDCERR("Failed to map stage buff HRESULT: %s", ToStr(hr).c_str());
      return empty;
    }

    evalData = new byte[evalStructStride * (overdrawLevels + 1)];
    memcpy(evalData, mapped.pData, evalStructStride * (overdrawLevels + 1));

    m_pImmediateContext->Unmap(evalStageBuf, 0);
  }

  SAFE_RELEASE(initialUAV);
  SAFE_RELEASE(initialBuf);
  SAFE_RELEASE(initialStageBuf);

  SAFE_RELEASE(evalUAV);
  SAFE_RELEASE(evalBuf);
  SAFE_RELEASE(evalStageBuf);

  SAFE_RELEASE(extract);

  DebugHit *buf = (DebugHit *)initialData;

  D3D11MarkerRegion::Set(StringFormat::Fmt("Got %u hits", buf[0].numHits));

  if(buf[0].numHits == 0)
  {
    RDCLOG("No hit for this event");
    SAFE_DELETE_ARRAY(initialData);
    SAFE_DELETE_ARRAY(evalData);
    return empty;
  }

  // if we encounter multiple hits at our destination pixel co-ord (or any other) we
  // check to see if a specific primitive was requested (via primitive parameter not
  // being set to ~0U). If it was, debug that pixel, otherwise do a best-estimate
  // of which fragment was the last to successfully depth test and debug that, just by
  // checking if the depth test is ordered and picking the final fragment in the series

  // our debugging quad. Order is TL, TR, BL, BR
  State quad[4];

  // figure out the TL pixel's coords. Assume even top left (towards 0,0)
  // this isn't spec'd but is a reasonable assumption.
  int xTL = x & (~1);
  int yTL = y & (~1);

  // get the index of our desired pixel
  int destIdx = (x - xTL) + 2 * (y - yTL);

  bytebuf cbufData[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

  for(int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
    if(rs->PS.ConstantBuffers[i])
      GetDebugManager()->GetBufferData(rs->PS.ConstantBuffers[i],
                                       rs->PS.CBOffsets[i] * sizeof(Vec4f), 0, cbufData[i]);

  D3D11_COMPARISON_FUNC depthFunc = D3D11_COMPARISON_LESS;

  if(rs->OM.DepthStencilState)
  {
    D3D11_DEPTH_STENCIL_DESC desc;
    rs->OM.DepthStencilState->GetDesc(&desc);
    depthFunc = desc.DepthFunc;
  }

  DebugHit *winner = NULL;
  float *evalSampleCache = (float *)evalData;

  if(sample == ~0U)
    sample = 0;

  if(primitive != ~0U)
  {
    for(size_t i = 0; i < buf[0].numHits && i < overdrawLevels; i++)
    {
      DebugHit *hit = (DebugHit *)(initialData + i * structStride);

      if(hit->primitive == primitive && hit->sample == sample)
      {
        winner = hit;
        evalSampleCache = ((float *)evalData) + evalSampleCacheData.size() * 4 * i;
      }
    }
  }

  if(winner == NULL)
  {
    for(size_t i = 0; i < buf[0].numHits && i < overdrawLevels; i++)
    {
      DebugHit *hit = (DebugHit *)(initialData + i * structStride);

      if(winner == NULL || (winner->sample != sample && hit->sample == sample) ||
         depthFunc == D3D11_COMPARISON_ALWAYS || depthFunc == D3D11_COMPARISON_NEVER ||
         depthFunc == D3D11_COMPARISON_NOT_EQUAL || depthFunc == D3D11_COMPARISON_EQUAL)
      {
        winner = hit;
        evalSampleCache = ((float *)evalData) + evalSampleCacheData.size() * 4 * i;
        continue;
      }

      if((depthFunc == D3D11_COMPARISON_LESS && hit->depth < winner->depth) ||
         (depthFunc == D3D11_COMPARISON_LESS_EQUAL && hit->depth <= winner->depth) ||
         (depthFunc == D3D11_COMPARISON_GREATER && hit->depth > winner->depth) ||
         (depthFunc == D3D11_COMPARISON_GREATER_EQUAL && hit->depth >= winner->depth))
      {
        if(hit->sample == sample)
        {
          winner = hit;
          evalSampleCache = ((float *)evalData) + evalSampleCacheData.size() * 4 * i;
        }
      }
    }
  }

  if(winner == NULL)
  {
    RDCLOG("Couldn't find any pixels that passed depth test at target co-ordinates");
    SAFE_DELETE_ARRAY(initialData);
    SAFE_DELETE_ARRAY(evalData);
    return empty;
  }

  ShaderDebugTrace traces[4];

  tracker.State().ApplyState(m_pImmediateContext);

  GlobalState global;
  GetDebugManager()->CreateShaderGlobalState(global, dxbc, rs->OM.UAVStartSlot, rs->OM.UAVs,
                                             rs->PS.SRVs);

  global.sampleEvalRegisterMask = sampleEvalRegisterMask;

  {
    DebugHit *hit = winner;

    State initialState;
    CreateShaderDebugStateAndTrace(initialState, traces[destIdx], destIdx, dxbc, refl, cbufData);

    rdcarray<ShaderVariable> &ins = traces[destIdx].inputs;
    if(!ins.empty() && ins.back().name == "vCoverage")
      ins.back().value.u.x = hit->coverage;

    initialState.semantics.coverage = hit->coverage;
    initialState.semantics.primID = hit->primitive;
    initialState.semantics.isFrontFace = hit->isFrontFace;

    uint32_t *data = &hit->rawdata;

    float *pos_ddx = (float *)data;

    // ddx(SV_Position.x) MUST be 1.0
    if(*pos_ddx != 1.0f)
    {
      RDCERR("Derivatives invalid");
      SAFE_DELETE_ARRAY(initialData);
      SAFE_DELETE_ARRAY(evalData);
      return empty;
    }

    data++;

    for(size_t i = 0; i < initialValues.size(); i++)
    {
      int32_t *rawout = NULL;

      if(initialValues[i].reg >= 0)
      {
        ShaderVariable &invar = traces[destIdx].inputs[initialValues[i].reg];

        if(initialValues[i].sysattribute == ShaderBuiltin::PrimitiveIndex)
        {
          invar.value.u.x = hit->primitive;
        }
        else if(initialValues[i].sysattribute == ShaderBuiltin::MSAASampleIndex)
        {
          invar.value.u.x = hit->sample;
        }
        else if(initialValues[i].sysattribute == ShaderBuiltin::MSAACoverage)
        {
          invar.value.u.x = hit->coverage;
        }
        else if(initialValues[i].sysattribute == ShaderBuiltin::IsFrontFace)
        {
          invar.value.u.x = hit->isFrontFace ? ~0U : 0;
        }
        else
        {
          rawout = &invar.value.iv[initialValues[i].elem];

          memcpy(rawout, data, initialValues[i].numwords * 4);
        }
      }

      if(initialValues[i].included)
        data += initialValues[i].numwords;
    }

    for(int i = 0; i < 4; i++)
    {
      if(i != destIdx)
        traces[i] = traces[destIdx];
      quad[i] = initialState;
      quad[i].SetTrace(i, &traces[i]);
      if(i != destIdx)
        quad[i].SetHelper();
    }

    // fetch any inputs that were evaluated at sample granularity
    for(const GlobalState::SampleEvalCacheKey &key : evalSampleCacheData)
    {
      // start with the basic input value
      ShaderVariable var = traces[destIdx].inputs[key.inputRegisterIndex];

      // copy over the value into the variable
      memcpy(var.value.fv, evalSampleCache, var.columns * sizeof(float));

      // store in the global cache for each quad. We'll apply derivatives below to adjust for each
      GlobalState::SampleEvalCacheKey k = key;
      for(int i = 0; i < 4; i++)
      {
        k.quadIndex = i;
        global.sampleEvalCache[k] = var;
      }

      // advance past this data - always by float4 as that's the buffer st ride
      evalSampleCache += 4;
    }

    ApplyAllDerivatives(global, traces, destIdx, initialValues, (float *)data);
  }

  SAFE_DELETE_ARRAY(initialData);
  SAFE_DELETE_ARRAY(evalData);

  std::vector<ShaderDebugState> states;

  if(dxbc->GetDebugInfo())
    dxbc->GetDebugInfo()->GetLocals(0, dxbc->GetDXBCByteCode()->GetInstruction(0).offset,
                                    quad[destIdx].locals);

  states.push_back((State)quad[destIdx]);

  // ping pong between so that we can have 'current' quad to update into new one
  State quad2[4];

  State *curquad = quad;
  State *newquad = quad2;

  // marks any threads stalled waiting for others to catch up
  bool activeMask[4] = {true, true, true, true};

  int cycleCounter = 0;

  D3D11MarkerRegion simloop("Simulation Loop");

  D3D11DebugAPIWrapper apiWrapper(m_pDevice, dxbc, global);

  // simulate lockstep until all threads are finished
  bool finished = true;
  do
  {
    for(size_t i = 0; i < 4; i++)
    {
      if(activeMask[i])
        newquad[i] = curquad[i].GetNext(global, &apiWrapper, curquad);
      else
        newquad[i] = curquad[i];
    }

    State *a = curquad;
    curquad = newquad;
    newquad = a;

    // if our destination quad is paused don't record multiple identical states.
    if(activeMask[destIdx])
    {
      State &s = curquad[destIdx];

      if(dxbc->GetDebugInfo())
      {
        size_t inst =
            RDCMIN((size_t)s.nextInstruction, dxbc->GetDXBCByteCode()->GetNumInstructions() - 1);
        const DXBCBytecode::Operation &op = dxbc->GetDXBCByteCode()->GetInstruction(inst);
        dxbc->GetDebugInfo()->GetLocals(s.nextInstruction, op.offset, s.locals);
      }

      states.push_back(s);
    }

    // we need to make sure that control flow which converges stays in lockstep so that
    // derivatives are still valid. While diverged, we don't have to keep threads in lockstep
    // since using derivatives is invalid.

    // Threads diverge either in ifs, loops, or switches. Due to the nature of the bytecode,
    // all threads *must* pass through the same exit instruction for each, there's no jumping
    // around with gotos. Note also for the same reason, the only time threads are on earlier
    // instructions is if they are still catching up to a thread that has exited the control
    // flow.

    // So the scheme is as follows:
    // * If all threads have the same nextInstruction, just continue we are still in lockstep.
    // * If threads are out of lockstep, find any thread which has nextInstruction pointing
    //   immediately *after* an ENDIF, ENDLOOP or ENDSWITCH. Pointing directly at one is not
    //   an indication the thread is done, as the next step for an ENDLOOP will jump back to
    //   the matching LOOP and continue iterating.
    // * Pause any thread matching the above until all threads are pointing to the same
    //   instruction. By the assumption above, all threads will eventually pass through this
    //   terminating instruction so we just pause any other threads and don't do anything
    //   until the control flow has converged and we can continue stepping in lockstep.

    // mark all threads as active again.
    // if we've converged, or we were never diverged, this keeps everything ticking
    activeMask[0] = activeMask[1] = activeMask[2] = activeMask[3] = true;

    if(curquad[0].nextInstruction != curquad[1].nextInstruction ||
       curquad[0].nextInstruction != curquad[2].nextInstruction ||
       curquad[0].nextInstruction != curquad[3].nextInstruction)
    {
      // this isn't *perfect* but it will still eventually continue. We look for the most
      // advanced thread, and check to see if it's just finished a control flow. If it has
      // then we assume it's at the convergence point and wait for every other thread to
      // catch up, pausing any threads that reach the convergence point before others.

      // Note this might mean we don't have any threads paused even within divergent flow.
      // This is fine and all we care about is pausing to make sure threads don't run ahead
      // into code that should be lockstep. We don't care at all about what they do within
      // the code that is divergent.

      // The reason this isn't perfect is that the most advanced thread could be on an
      // inner loop or inner if, not the convergence point, and we could be pausing it
      // fruitlessly. Worse still - it could be on a branch none of the other threads will
      // take so they will never reach that exact instruction.
      // But we know that all threads will eventually go through the convergence point, so
      // even in that worst case if we didn't pick the right waiting point, another thread
      // will overtake and become the new most advanced thread and the previous waiting
      // thread will resume. So in this case we caused a thread to wait more than it should
      // have but that's not a big deal as it's within divergent flow so they don't have to
      // stay in lockstep. Also if all threads will eventually pass that point we picked,
      // we just waited to converge even in technically divergent code which is also
      // harmless.

      // Phew!

      uint32_t convergencePoint = 0;

      // find which thread is most advanced
      for(size_t i = 0; i < 4; i++)
        if(curquad[i].nextInstruction > convergencePoint)
          convergencePoint = curquad[i].nextInstruction;

      if(convergencePoint > 0)
      {
        OpcodeType op = dxbc->GetDXBCByteCode()->GetInstruction(convergencePoint - 1).operation;

        // if the most advnaced thread hasn't just finished control flow, then all
        // threads are still running, so don't converge
        if(op != OPCODE_ENDIF && op != OPCODE_ENDLOOP && op != OPCODE_ENDSWITCH)
          convergencePoint = 0;
      }

      // pause any threads at that instruction (could be none)
      for(size_t i = 0; i < 4; i++)
        if(curquad[i].nextInstruction == convergencePoint)
          activeMask[i] = false;
    }

    finished = curquad[destIdx].Finished();

    cycleCounter++;

    if(cycleCounter == SHADER_DEBUG_WARN_THRESHOLD)
    {
      if(PromptDebugTimeout(cycleCounter))
        break;
    }
  } while(!finished);

  traces[destIdx].states = states;

  traces[destIdx].hasLocals = dxbc->GetDebugInfo() && dxbc->GetDebugInfo()->HasLocals();

  traces[destIdx].lineInfo.resize(dxbc->GetDXBCByteCode()->GetNumInstructions());
  for(size_t i = 0; dxbc->GetDebugInfo() && i < dxbc->GetDXBCByteCode()->GetNumInstructions(); i++)
  {
    const DXBCBytecode::Operation &op = dxbc->GetDXBCByteCode()->GetInstruction(i);
    dxbc->GetDebugInfo()->GetLineInfo(i, op.offset, traces[destIdx].lineInfo[i]);
  }

  return traces[destIdx];
}

ShaderDebugTrace D3D11Replay::DebugThread(uint32_t eventId, const uint32_t groupid[3],
                                          const uint32_t threadid[3])
{
  using namespace DXBCBytecode;
  using namespace ShaderDebug;

  D3D11MarkerRegion simloop(StringFormat::Fmt("DebugThread @ %u: [%u, %u, %u] (%u, %u, %u)",
                                              eventId, groupid[0], groupid[1], groupid[2],
                                              threadid[0], threadid[1], threadid[2]));

  ShaderDebugTrace empty;

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  ID3D11ComputeShader *stateCS = NULL;
  m_pImmediateContext->CSGetShader(&stateCS, NULL, NULL);

  WrappedID3D11Shader<ID3D11ComputeShader> *cs = (WrappedID3D11Shader<ID3D11ComputeShader> *)stateCS;

  SAFE_RELEASE(stateCS);

  if(!cs)
    return empty;

  DXBC::DXBCContainer *dxbc = cs->GetDXBC();
  const ShaderReflection &refl = cs->GetDetails();

  if(!dxbc)
    return empty;

  dxbc->GetDisassembly();

  D3D11RenderState *rs = m_pImmediateContext->GetCurrentPipelineState();

  bytebuf cbufData[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];

  for(int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
    if(rs->CS.ConstantBuffers[i])
      GetDebugManager()->GetBufferData(rs->CS.ConstantBuffers[i],
                                       rs->CS.CBOffsets[i] * sizeof(Vec4f), 0, cbufData[i]);

  ShaderDebugTrace ret;

  GlobalState global;
  GetDebugManager()->CreateShaderGlobalState(global, dxbc, 0, rs->CSUAVs, rs->CS.SRVs);
  State initialState;
  CreateShaderDebugStateAndTrace(initialState, ret, -1, dxbc, refl, cbufData);

  for(int i = 0; i < 3; i++)
  {
    initialState.semantics.GroupID[i] = groupid[i];
    initialState.semantics.ThreadID[i] = threadid[i];
  }

  std::vector<ShaderDebugState> states;

  if(dxbc->GetDebugInfo())
    dxbc->GetDebugInfo()->GetLocals(0, dxbc->GetDXBCByteCode()->GetInstruction(0).offset,
                                    initialState.locals);

  states.push_back((State)initialState);

  D3D11DebugAPIWrapper apiWrapper(m_pDevice, dxbc, global);

  for(int cycleCounter = 0;; cycleCounter++)
  {
    if(initialState.Finished())
      break;

    initialState = initialState.GetNext(global, &apiWrapper, NULL);

    if(dxbc->GetDebugInfo())
    {
      const DXBCBytecode::Operation &op =
          dxbc->GetDXBCByteCode()->GetInstruction((size_t)initialState.nextInstruction);
      dxbc->GetDebugInfo()->GetLocals(initialState.nextInstruction, op.offset, initialState.locals);
    }

    states.push_back((State)initialState);

    if(cycleCounter == SHADER_DEBUG_WARN_THRESHOLD)
    {
      if(PromptDebugTimeout(cycleCounter))
        break;
    }
  }

  ret.states = states;

  ret.hasLocals = dxbc->GetDebugInfo() && dxbc->GetDebugInfo()->HasLocals();

  ret.lineInfo.resize(dxbc->GetDXBCByteCode()->GetNumInstructions());
  for(size_t i = 0; dxbc->GetDebugInfo() && i < dxbc->GetDXBCByteCode()->GetNumInstructions(); i++)
  {
    const Operation &op = dxbc->GetDXBCByteCode()->GetInstruction(i);
    dxbc->GetDebugInfo()->GetLineInfo(i, op.offset, ret.lineInfo[i]);
  }

  for(size_t i = 0; i < dxbc->GetDXBCByteCode()->GetNumDeclarations(); i++)
  {
    const DXBCBytecode::Declaration &decl = dxbc->GetDXBCByteCode()->GetDeclaration(i);

    if(decl.declaration == OPCODE_DCL_INPUT &&
       (decl.operand.type == TYPE_INPUT_THREAD_ID || decl.operand.type == TYPE_INPUT_THREAD_GROUP_ID ||
        decl.operand.type == TYPE_INPUT_THREAD_ID_IN_GROUP ||
        decl.operand.type == TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED))
    {
      ShaderVariable v;

      v.name = decl.operand.toString(dxbc->GetReflection(), ToString::IsDecl);
      v.rows = 1;
      v.type = VarType::UInt;

      switch(decl.operand.type)
      {
        case TYPE_INPUT_THREAD_GROUP_ID:
          memcpy(v.value.uv, initialState.semantics.GroupID, sizeof(uint32_t) * 3);
          v.columns = 3;
          break;
        case TYPE_INPUT_THREAD_ID_IN_GROUP:
          memcpy(v.value.uv, initialState.semantics.ThreadID, sizeof(uint32_t) * 3);
          v.columns = 3;
          break;
        case TYPE_INPUT_THREAD_ID:
          v.value.u.x = initialState.semantics.GroupID[0] *
                            dxbc->GetReflection()->DispatchThreadsDimension[0] +
                        initialState.semantics.ThreadID[0];
          v.value.u.y = initialState.semantics.GroupID[1] *
                            dxbc->GetReflection()->DispatchThreadsDimension[1] +
                        initialState.semantics.ThreadID[1];
          v.value.u.z = initialState.semantics.GroupID[2] *
                            dxbc->GetReflection()->DispatchThreadsDimension[2] +
                        initialState.semantics.ThreadID[2];
          v.columns = 3;
          break;
        case TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
          v.value.u.x = initialState.semantics.ThreadID[2] *
                            dxbc->GetReflection()->DispatchThreadsDimension[0] *
                            dxbc->GetReflection()->DispatchThreadsDimension[1] +
                        initialState.semantics.ThreadID[1] *
                            dxbc->GetReflection()->DispatchThreadsDimension[0] +
                        initialState.semantics.ThreadID[0];
          v.columns = 1;
          break;
        default: v.columns = 4; break;
      }

      ret.inputs.push_back(v);
    }
  }

  return ret;
}
