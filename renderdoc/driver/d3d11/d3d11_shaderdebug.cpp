/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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
#include "driver/shaders/dxbc/dxbc_bytecode.h"
#include "driver/shaders/dxbc/dxbc_debug.h"
#include "maths/formatpacking.h"
#include "maths/vec.h"
#include "strings/string_utils.h"
#include "d3d11_context.h"
#include "d3d11_debug.h"
#include "d3d11_manager.h"
#include "d3d11_renderstate.h"
#include "d3d11_replay.h"
#include "d3d11_resources.h"
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

class D3D11DebugAPIWrapper : public DXBCDebug::DebugAPIWrapper
{
public:
  D3D11DebugAPIWrapper(WrappedID3D11Device *device, const DXBC::DXBCContainer *dxbc,
                       DXBCDebug::GlobalState &globalState);

  void SetCurrentInstruction(uint32_t instruction) { m_instruction = instruction; }
  void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, rdcstr d);

  bool FetchSRV(const DXBCDebug::BindingSlot &slot);
  bool FetchUAV(const DXBCDebug::BindingSlot &slot);

  bool CalculateMathIntrinsic(DXBCBytecode::OpcodeType opcode, const ShaderVariable &input,
                              ShaderVariable &output1, ShaderVariable &output2);

  ShaderVariable GetSampleInfo(DXBCBytecode::OperandType type, bool isAbsoluteResource,
                               const DXBCDebug::BindingSlot &slot, const char *opString);
  ShaderVariable GetBufferInfo(DXBCBytecode::OperandType type, const DXBCDebug::BindingSlot &slot,
                               const char *opString);
  ShaderVariable GetResourceInfo(DXBCBytecode::OperandType type, const DXBCDebug::BindingSlot &slot,
                                 uint32_t mipLevel, int &dim);

  bool CalculateSampleGather(DXBCBytecode::OpcodeType opcode,
                             DXBCDebug::SampleGatherResourceData resourceData,
                             DXBCDebug::SampleGatherSamplerData samplerData, ShaderVariable uv,
                             ShaderVariable ddxCalc, ShaderVariable ddyCalc,
                             const int texelOffsets[3], int multisampleIndex, float lodOrCompareValue,
                             const uint8_t swizzle[4], DXBCDebug::GatherChannel gatherChannel,
                             const char *opString, ShaderVariable &output);

private:
  DXBC::ShaderType GetShaderType() { return m_dxbc ? m_dxbc->m_Type : DXBC::ShaderType::Pixel; }
  WrappedID3D11Device *m_pDevice;
  const DXBC::DXBCContainer *m_dxbc;
  DXBCDebug::GlobalState &m_globalState;
  uint32_t m_instruction;
};

D3D11DebugAPIWrapper::D3D11DebugAPIWrapper(WrappedID3D11Device *device,
                                           const DXBC::DXBCContainer *dxbc,
                                           DXBCDebug::GlobalState &globalState)
    : m_pDevice(device), m_dxbc(dxbc), m_globalState(globalState), m_instruction(0)
{
}

void D3D11DebugAPIWrapper::AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                                           rdcstr d)
{
  m_pDevice->AddDebugMessage(c, sv, src, d);
}

bool D3D11DebugAPIWrapper::FetchSRV(const DXBCDebug::BindingSlot &slot)
{
  RDCASSERT(slot.registerSpace == 0);
  RDCASSERT(slot.shaderRegister < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT);

  D3D11RenderState *rs = m_pDevice->GetImmediateContext()->GetCurrentPipelineState();
  ID3D11ShaderResourceView *pSRV = NULL;
  if(GetShaderType() == DXBC::ShaderType::Vertex)
    pSRV = rs->VS.SRVs[slot.shaderRegister];
  else if(GetShaderType() == DXBC::ShaderType::Pixel)
    pSRV = rs->PS.SRVs[slot.shaderRegister];
  else if(GetShaderType() == DXBC::ShaderType::Compute)
    pSRV = rs->CS.SRVs[slot.shaderRegister];

  if(!pSRV)
    return false;

  ID3D11Resource *res = NULL;
  pSRV->GetResource(&res);

  if(!res)
    return false;

  D3D11_SHADER_RESOURCE_VIEW_DESC sdesc;
  pSRV->GetDesc(&sdesc);

  DXBCDebug::GlobalState::SRVData &srvData = m_globalState.srvs[slot];

  if(sdesc.Format != DXGI_FORMAT_UNKNOWN)
  {
    DXBCDebug::FillViewFmt(sdesc.Format, srvData.format);
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

      srvData.format.stride = bufdesc.StructureByteStride;

      // if we didn't get a type from the SRV description, try to pull it from the declaration
      DXBCDebug::LookupSRVFormatFromShaderReflection(*m_dxbc->GetReflection(), slot, srvData.format);
    }
  }

  if(sdesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER)
  {
    // I know this isn't what the docs say, but as best as I can tell
    // this is how it's used.
    srvData.firstElement = sdesc.Buffer.FirstElement;
    srvData.numElements = sdesc.Buffer.NumElements;
  }
  else if(sdesc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX)
  {
    srvData.firstElement = sdesc.BufferEx.FirstElement;
    srvData.numElements = sdesc.BufferEx.NumElements;
  }

  if(res)
  {
    if(WrappedID3D11Buffer::IsAlloc(res))
    {
      m_pDevice->GetDebugManager()->GetBufferData((ID3D11Buffer *)res, 0, 0, srvData.data);
    }
  }

  SAFE_RELEASE(res);
  return true;
}

bool D3D11DebugAPIWrapper::FetchUAV(const DXBCDebug::BindingSlot &slot)
{
  RDCASSERT(slot.registerSpace == 0);
  RDCASSERT(slot.shaderRegister < D3D11_1_UAV_SLOT_COUNT);

  WrappedID3D11DeviceContext *pContext = m_pDevice->GetImmediateContext();
  D3D11RenderState *rs = pContext->GetCurrentPipelineState();
  ID3D11UnorderedAccessView *pUAV = NULL;
  if(GetShaderType() == DXBC::ShaderType::Pixel)
    pUAV = rs->OM.UAVs[slot.shaderRegister - rs->OM.UAVStartSlot];
  else if(GetShaderType() == DXBC::ShaderType::Compute)
    pUAV = rs->CSUAVs[slot.shaderRegister];

  if(!pUAV)
    return false;

  ID3D11Resource *res = NULL;
  pUAV->GetResource(&res);

  DXBCDebug::GlobalState::UAVData &uavData = m_globalState.uavs[slot];
  uavData.hiddenCounter = m_pDevice->GetDebugManager()->GetStructCount(pUAV);

  D3D11_UNORDERED_ACCESS_VIEW_DESC udesc;
  pUAV->GetDesc(&udesc);

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

    uavData.format.byteWidth = fmt.compByteWidth;
    uavData.format.numComps = fmt.compCount;
    uavData.format.fmt = fmt.compType;

    if(udesc.Format == DXGI_FORMAT_R11G11B10_FLOAT)
      uavData.format.byteWidth = 11;
    if(udesc.Format == DXGI_FORMAT_R10G10B10A2_UINT || udesc.Format == DXGI_FORMAT_R10G10B10A2_UNORM)
      uavData.format.byteWidth = 10;
  }

  if(udesc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
  {
    uavData.firstElement = udesc.Buffer.FirstElement;
    uavData.numElements = udesc.Buffer.NumElements;
  }

  if(res)
  {
    if(WrappedID3D11Buffer::IsAlloc(res))
    {
      m_pDevice->GetDebugManager()->GetBufferData((ID3D11Buffer *)res, 0, 0, uavData.data);
    }
    else
    {
      uavData.tex = true;

      uint32_t &rowPitch = uavData.rowPitch;
      uint32_t &depthPitch = uavData.depthPitch;

      bytebuf &data = uavData.data;

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

        pContext->CopyResource(stagingTex, res);

        D3D11_MAPPED_SUBRESOURCE mapped;
        pContext->Map(stagingTex, udesc.Texture1D.MipSlice, D3D11_MAP_READ, 0, &mapped);

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

        pContext->Unmap(stagingTex, udesc.Texture1D.MipSlice);

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

        pContext->CopyResource(stagingTex, res);

        // MipSlice in union is shared between Texture2D and Texture2DArray unions, so safe to
        // use either
        D3D11_MAPPED_SUBRESOURCE mapped;
        pContext->Map(stagingTex, udesc.Texture2D.MipSlice, D3D11_MAP_READ, 0, &mapped);

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

        pContext->Unmap(stagingTex, udesc.Texture2D.MipSlice);

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

        pContext->CopyResource(stagingTex, res);

        // MipSlice in union is shared between Texture2D and Texture2DArray unions, so safe to
        // use either
        D3D11_MAPPED_SUBRESOURCE mapped;
        pContext->Map(stagingTex, udesc.Texture3D.MipSlice, D3D11_MAP_READ, 0, &mapped);

        rowPitch = mapped.RowPitch;
        depthPitch = mapped.DepthPitch;

        byte *srcdata = (byte *)mapped.pData;
        srcdata += udesc.Texture3D.FirstWSlice * mapped.DepthPitch;
        uint32_t numSlices = udesc.Texture3D.WSize;
        size_t datasize = depthPitch * numSlices;

        data.resize(datasize);

        // copy with all padding etc intact
        memcpy(&data[0], srcdata, datasize);

        pContext->Unmap(stagingTex, udesc.Texture3D.MipSlice);

        SAFE_RELEASE(stagingTex);
      }
    }
  }

  SAFE_RELEASE(res);

  return true;
}

ShaderVariable D3D11DebugAPIWrapper::GetSampleInfo(DXBCBytecode::OperandType type,
                                                   bool isAbsoluteResource,
                                                   const DXBCDebug::BindingSlot &slot,
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
      case DXBC::ShaderType::Vertex:
        context->VSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Hull:
        context->HSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Domain:
        context->DSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Geometry:
        context->GSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Pixel:
        context->PSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Compute:
        context->CSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      default: RDCERR("Unhandled shader type %d", GetShaderType()); break;
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

ShaderVariable D3D11DebugAPIWrapper::GetBufferInfo(DXBCBytecode::OperandType type,
                                                   const DXBCDebug::BindingSlot &slot,
                                                   const char *opString)
{
  ID3D11DeviceContext *context = NULL;
  m_pDevice->GetImmediateContext(&context);

  ShaderVariable result("", 0U, 0U, 0U, 0U);

  if(type == DXBCBytecode::TYPE_UNORDERED_ACCESS_VIEW)
  {
    ID3D11UnorderedAccessView *uav = NULL;
    if(GetShaderType() == DXBC::ShaderType::Compute)
      context->CSGetUnorderedAccessViews(slot.shaderRegister, 1, &uav);
    else
      context->OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, slot.shaderRegister, 1, &uav);

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
      case DXBC::ShaderType::Vertex:
        context->VSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Hull:
        context->HSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Domain:
        context->DSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Geometry:
        context->GSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Pixel:
        context->PSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Compute:
        context->CSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      default: RDCERR("Unhandled shader type %d", GetShaderType()); break;
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

ShaderVariable D3D11DebugAPIWrapper::GetResourceInfo(DXBCBytecode::OperandType type,
                                                     const DXBCDebug::BindingSlot &slot,
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
      case DXBC::ShaderType::Vertex:
        context->VSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Hull:
        context->HSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Domain:
        context->DSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Geometry:
        context->GSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Pixel:
        context->PSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      case DXBC::ShaderType::Compute:
        context->CSGetShaderResources(slot.shaderRegister, 1, &srv);
        break;
      default: RDCERR("Unhandled shader type %d", GetShaderType()); break;
    }

    if(srv)
    {
      D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
      srv->GetDesc(&srvDesc);

      switch(srvDesc.ViewDimension)
      {
        case D3D11_SRV_DIMENSION_UNKNOWN:
        case D3D11_SRV_DIMENSION_BUFFER:
        {
          RDCWARN("Invalid view dimension for GetResourceInfo");
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
            bool isarray = srvDesc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;

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
      context->CSGetUnorderedAccessViews(slot.shaderRegister, 1, &uav);
    }
    else
    {
      ID3D11RenderTargetView *rtvs[8] = {0};
      ID3D11DepthStencilView *dsv = NULL;
      context->OMGetRenderTargetsAndUnorderedAccessViews(0, rtvs, &dsv, slot.shaderRegister, 1, &uav);

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
        case D3D11_UAV_DIMENSION_UNKNOWN:
        case D3D11_UAV_DIMENSION_BUFFER:
        {
          RDCWARN("Invalid view dimension for GetResourceInfo");
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
    DXBCBytecode::OpcodeType opcode, DXBCDebug::SampleGatherResourceData resourceData,
    DXBCDebug::SampleGatherSamplerData samplerData, ShaderVariable uv, ShaderVariable ddxCalc,
    ShaderVariable ddyCalc, const int texelOffsets[3], int multisampleIndex,
    float lodOrCompareValue, const uint8_t swizzle[4], DXBCDebug::GatherChannel gatherChannel,
    const char *opString, ShaderVariable &output)
{
  using namespace DXBCBytecode;

  rdcstr funcRet = "";
  DXGI_FORMAT retFmt = DXGI_FORMAT_UNKNOWN;

  if(opcode == OPCODE_SAMPLE_C || opcode == OPCODE_SAMPLE_C_LZ || opcode == OPCODE_GATHER4_C ||
     opcode == OPCODE_GATHER4_PO_C || opcode == OPCODE_LOD)
  {
    retFmt = DXGI_FORMAT_R32G32B32A32_FLOAT;
    funcRet = "float4";
  }

  rdcstr samplerDecl = "";
  if(samplerData.mode == SAMPLER_MODE_DEFAULT)
    samplerDecl = "SamplerState s";
  else if(samplerData.mode == SAMPLER_MODE_COMPARISON)
    samplerDecl = "SamplerComparisonState s";

  rdcstr textureDecl = "";
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

    rdcstr type = StringFormat::Fmt("%s4", typeStr[resourceData.retType]);

    if(retFmt == DXGI_FORMAT_UNKNOWN)
    {
      funcRet = type;
      retFmt = fmts[resourceData.retType];
    }

    if(resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DMS ||
       resourceData.dim == RESOURCE_DIMENSION_TEXTURE2DMSARRAY)
    {
      if(resourceData.sampleCount > 0)
        type += StringFormat::Fmt(", %d", resourceData.sampleCount);
    }

    textureDecl += "<" + type + "> t";
  }

  char *formats[4][2] = {
      {"float(%.10f)", "int(%d)"},
      {"float2(%.10f, %.10f)", "int2(%d, %d)"},
      {"float3(%.10f, %.10f, %.10f)", "int3(%d, %d, %d)"},
      {"float4(%.10f, %.10f, %.10f, %.10f)", "int4(%d, %d, %d, %d)"},
  };

  int texcoordType = 0;
  int texdimOffs = 0;

  if(opcode == OPCODE_SAMPLE || opcode == OPCODE_SAMPLE_L || opcode == OPCODE_SAMPLE_B ||
     opcode == OPCODE_SAMPLE_D || opcode == OPCODE_SAMPLE_C || opcode == OPCODE_SAMPLE_C_LZ ||
     opcode == OPCODE_GATHER4 || opcode == OPCODE_GATHER4_C || opcode == OPCODE_GATHER4_PO ||
     opcode == OPCODE_GATHER4_PO_C || opcode == OPCODE_LOD)
  {
    // all floats
    texcoordType = 0;
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
    if(!RDCISFINITE(ddxCalc.value.fv[i]))
    {
      RDCWARN("NaN or Inf in texlookup");
      ddxCalc.value.fv[i] = 0.0f;

      m_pDevice->AddDebugMessage(MessageCategory::Shaders, MessageSeverity::High,
                                 MessageSource::RuntimeWarning,
                                 StringFormat::Fmt("Shader debugging %d: %s\nNaN or Inf found in "
                                                   "texture lookup ddx - using 0.0 instead",
                                                   m_instruction, opString));
    }
    if(!RDCISFINITE(ddyCalc.value.fv[i]))
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
    if(texcoordType == 0 && !RDCISFINITE(uv.value.fv[i]))
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

  rdcstr texcoords;

  // because of unions in .value we can pass the float versions and printf will interpret it as
  // the right type according to formats
  if(texcoordType == 0)
    texcoords = StringFormat::Fmt(formats[texdim + texdimOffs - 1][texcoordType], uv.value.f.x,
                                  uv.value.f.y, uv.value.f.z, uv.value.f.w);
  else
    texcoords = StringFormat::Fmt(formats[texdim + texdimOffs - 1][texcoordType], uv.value.i.x,
                                  uv.value.i.y, uv.value.i.z, uv.value.i.w);

  rdcstr offsets = "";

  if(useOffsets)
  {
    if(offsetDim == 1)
      offsets = StringFormat::Fmt(", int(%d)", texelOffsets[0]);
    else if(offsetDim == 2)
      offsets = StringFormat::Fmt(", int2(%d, %d)", texelOffsets[0], texelOffsets[1]);
    else if(offsetDim == 3)
      offsets =
          StringFormat::Fmt(", int3(%d, %d, %d)", texelOffsets[0], texelOffsets[1], texelOffsets[2]);
    // texdim == 4 is cube arrays, no offset supported
  }

  char elems[] = "xyzw";
  rdcstr strSwizzle = ".";
  for(int i = 0; i < 4; ++i)
    strSwizzle += elems[swizzle[i]];

  rdcstr strGatherChannel;
  switch(gatherChannel)
  {
    case DXBCDebug::GatherChannel::Red: strGatherChannel = "Red"; break;
    case DXBCDebug::GatherChannel::Green: strGatherChannel = "Green"; break;
    case DXBCDebug::GatherChannel::Blue: strGatherChannel = "Blue"; break;
    case DXBCDebug::GatherChannel::Alpha: strGatherChannel = "Alpha"; break;
  }

  rdcstr vsProgram = "float4 main(uint id : SV_VertexID) : SV_Position {\n";
  vsProgram += "return float4((id == 2) ? 3.0f : -1.0f, (id == 0) ? -3.0f : 1.0f, 0.5, 1.0);\n";
  vsProgram += "}";

  rdcstr sampleProgram;

  if(opcode == OPCODE_SAMPLE || opcode == OPCODE_SAMPLE_B || opcode == OPCODE_SAMPLE_D)
  {
    rdcstr ddx = StringFormat::Fmt(formats[offsetDim + texdimOffs - 1][0], ddxCalc.value.f.x,
                                   ddxCalc.value.f.y, ddxCalc.value.f.z, ddxCalc.value.f.w);

    rdcstr ddy = StringFormat::Fmt(formats[offsetDim + texdimOffs - 1][0], ddyCalc.value.f.x,
                                   ddyCalc.value.f.y, ddyCalc.value.f.z, ddyCalc.value.f.w);

    sampleProgram = StringFormat::Fmt("%s : register(t0);\n%s : register(s0);\n\n",
                                      textureDecl.c_str(), samplerDecl.c_str());
    sampleProgram += funcRet + " main() : SV_Target0\n{\n";
    sampleProgram +=
        StringFormat::Fmt("return t.SampleGrad(s, %s, %s, %s %s)%s;\n", texcoords.c_str(),
                          ddx.c_str(), ddy.c_str(), offsets.c_str(), strSwizzle.c_str());
    sampleProgram += "}\n";
  }
  else if(opcode == OPCODE_SAMPLE_L)
  {
    sampleProgram = StringFormat::Fmt("%s : register(t0);\n%s : register(s0);\n\n",
                                      textureDecl.c_str(), samplerDecl.c_str());
    sampleProgram += funcRet + " main() : SV_Target0\n{\n";
    sampleProgram +=
        StringFormat::Fmt("return t.SampleLevel(s, %s, %.10f %s)%s;\n", texcoords.c_str(),
                          lodOrCompareValue, offsets.c_str(), strSwizzle.c_str());
    sampleProgram += "}\n";
  }
  else if(opcode == OPCODE_SAMPLE_C || opcode == OPCODE_LOD)
  {
    // these operations need derivatives but have no hlsl function to call to provide them, so
    // we fake it in the vertex shader

    rdcstr uvdecl = StringFormat::Fmt("float%d uv : uvs", texdim + texdimOffs);

    vsProgram =
        "void main(uint id : SV_VertexID, out float4 pos : SV_Position, out " + uvdecl + ") {\n";

    rdcstr uvPlusDDX = StringFormat::Fmt(
        formats[texdim + texdimOffs - 1][texcoordType], uv.value.f.x + ddyCalc.value.f.x * 2.0f,
        uv.value.f.y + ddyCalc.value.f.y * 2.0f, uv.value.f.z + ddyCalc.value.f.z * 2.0f,
        uv.value.f.w + ddyCalc.value.f.w * 2.0f);

    rdcstr uvPlusDDY = StringFormat::Fmt(
        formats[texdim + texdimOffs - 1][texcoordType], uv.value.f.x + ddxCalc.value.f.x * 2.0f,
        uv.value.f.y + ddxCalc.value.f.y * 2.0f, uv.value.f.z + ddxCalc.value.f.z * 2.0f,
        uv.value.f.w + ddxCalc.value.f.w * 2.0f);

    vsProgram += "if(id == 0) uv = " + uvPlusDDX + ";\n";
    vsProgram += "if(id == 1) uv = " + texcoords + ";\n";
    vsProgram += "if(id == 2) uv = " + uvPlusDDY + ";\n";
    vsProgram += "pos = float4((id == 2) ? 3.0f : -1.0f, (id == 0) ? -3.0f : 1.0f, 0.5, 1.0);\n";
    vsProgram += "}";

    if(opcode == OPCODE_SAMPLE_C)
    {
      sampleProgram = StringFormat::Fmt("%s : register(t0);\n%s : register(s0);\n\n",
                                        textureDecl.c_str(), samplerDecl.c_str());
      sampleProgram +=
          funcRet + " main(float4 pos : SV_Position, " + uvdecl + ") : SV_Target0\n{\n";
      sampleProgram += StringFormat::Fmt("return t.SampleCmpLevelZero(s, uv, %.10f %s).xxxx;\n",
                                         lodOrCompareValue, offsets.c_str());
      sampleProgram += "}\n";
    }
    else if(opcode == OPCODE_LOD)
    {
      sampleProgram = StringFormat::Fmt("%s : register(t0);\n%s : register(s0);\n\n",
                                        textureDecl.c_str(), samplerDecl.c_str());
      sampleProgram +=
          funcRet + " main(float4 pos : SV_Position, " + uvdecl + ") : SV_Target0\n{\n";
      sampleProgram +=
          "return float4(t.CalculateLevelOfDetail(s, uv),\n"
          "              t.CalculateLevelOfDetailUnclamped(s, uv),\n"
          "              0.0f, 0.0f);\n";
      sampleProgram += "}\n";
    }
  }
  else if(opcode == OPCODE_SAMPLE_C_LZ)
  {
    sampleProgram = StringFormat::Fmt("%s : register(t0);\n%s : register(s0);\n\n",
                                      textureDecl.c_str(), samplerDecl.c_str());
    sampleProgram += funcRet + " main() : SV_Target0\n{\n";
    sampleProgram +=
        StringFormat::Fmt("return t.SampleCmpLevelZero(s, %s, %.10f %s)%s;\n", texcoords.c_str(),
                          lodOrCompareValue, offsets.c_str(), strSwizzle.c_str());
    sampleProgram += "}\n";
  }
  else if(opcode == OPCODE_LD)
  {
    sampleProgram = StringFormat::Fmt("%s : register(t0);\n\n", textureDecl.c_str());
    sampleProgram += funcRet + " main() : SV_Target0\n{\n";
    sampleProgram += "return t.Load(" + texcoords + offsets + ")" + strSwizzle + ";";
    sampleProgram += "\n}\n";
  }
  else if(opcode == OPCODE_LD_MS)
  {
    sampleProgram = StringFormat::Fmt("%s : register(t0);\n\n", textureDecl.c_str());
    sampleProgram += funcRet + " main() : SV_Target0\n{\n";
    sampleProgram += StringFormat::Fmt("return t.Load(%s, int(%d) %s)%s;\n", texcoords.c_str(),
                                       multisampleIndex, offsets.c_str(), strSwizzle.c_str());
    sampleProgram += "\n}\n";
  }
  else if(opcode == OPCODE_GATHER4 || opcode == OPCODE_GATHER4_PO)
  {
    sampleProgram = StringFormat::Fmt("%s : register(t0);\n%s : register(s0);\n\n",
                                      textureDecl.c_str(), samplerDecl.c_str());
    sampleProgram += funcRet + " main() : SV_Target0\n{\n";
    sampleProgram += StringFormat::Fmt("return t.Gather%s(s, %s %s)%s;\n", strGatherChannel.c_str(),
                                       texcoords.c_str(), offsets.c_str(), strSwizzle.c_str());
    sampleProgram += "}\n";
  }
  else if(opcode == OPCODE_GATHER4_C || opcode == OPCODE_GATHER4_PO_C)
  {
    sampleProgram = StringFormat::Fmt("%s : register(t0);\n%s : register(s0);\n\n",
                                      textureDecl.c_str(), samplerDecl.c_str());
    sampleProgram += funcRet + " main() : SV_Target0\n{\n";
    sampleProgram += StringFormat::Fmt("return t.GatherCmp%s(s, %s, %.10f %s)%s;\n",
                                       strGatherChannel.c_str(), texcoords.c_str(),
                                       lodOrCompareValue, offsets.c_str(), strSwizzle.c_str());
    sampleProgram += "}\n";
  }

  ID3D11VertexShader *vs =
      m_pDevice->GetShaderCache()->MakeVShader(vsProgram.c_str(), "main", "vs_5_0");
  ID3D11PixelShader *ps =
      m_pDevice->GetShaderCache()->MakePShader(sampleProgram.c_str(), "main", "ps_5_0");

  if(!vs || !ps)
  {
    RDCERR("Sample VS or PS failed to compile");
    return false;
  }

  D3D11RenderStateTracker tracker(m_pDevice->GetImmediateContext());

  ID3D11DeviceContext *context = NULL;

  m_pDevice->GetImmediateContext(&context);

  // back up SRV/sampler on PS slot 0

  ID3D11ShaderResourceView *usedSRV = NULL;
  ID3D11SamplerState *usedSamp = NULL;

  // fetch SRV and sampler from the shader stage we're debugging that this opcode wants to load from
  UINT texSlot = resourceData.binding.shaderRegister;
  UINT samplerSlot = samplerData.binding.shaderRegister;
  switch(GetShaderType())
  {
    case DXBC::ShaderType::Vertex:
      context->VSGetShaderResources(texSlot, 1, &usedSRV);
      context->VSGetSamplers(samplerSlot, 1, &usedSamp);
      break;
    case DXBC::ShaderType::Hull:
      context->HSGetShaderResources(texSlot, 1, &usedSRV);
      context->HSGetSamplers(samplerSlot, 1, &usedSamp);
      break;
    case DXBC::ShaderType::Domain:
      context->DSGetShaderResources(texSlot, 1, &usedSRV);
      context->DSGetSamplers(samplerSlot, 1, &usedSamp);
      break;
    case DXBC::ShaderType::Geometry:
      context->GSGetShaderResources(texSlot, 1, &usedSRV);
      context->GSGetSamplers(samplerSlot, 1, &usedSamp);
      break;
    case DXBC::ShaderType::Pixel:
      context->PSGetShaderResources(texSlot, 1, &usedSRV);
      context->PSGetSamplers(samplerSlot, 1, &usedSamp);
      break;
    case DXBC::ShaderType::Compute:
      context->CSGetShaderResources(texSlot, 1, &usedSRV);
      context->CSGetSamplers(samplerSlot, 1, &usedSamp);
      break;
    default: RDCERR("Unhandled shader type %d", GetShaderType()); break;
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

  SAFE_RELEASE(context);

  SAFE_RELEASE(usedSRV);
  SAFE_RELEASE(usedSamp);

  output = lookupResult;
  return true;
}

bool D3D11DebugAPIWrapper::CalculateMathIntrinsic(DXBCBytecode::OpcodeType opcode,
                                                  const ShaderVariable &input,
                                                  ShaderVariable &output1, ShaderVariable &output2)
{
  rdcstr csProgram =
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

  D3D11RenderStateTracker tracker(m_pDevice->GetImmediateContext());

  ID3D11DeviceContext *context = NULL;
  m_pDevice->GetImmediateContext(&context);

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

  SAFE_RELEASE(context);

  return true;
}

void AddCBuffersToGlobalState(const DXBCBytecode::Program &program, D3D11DebugManager &debugManager,
                              DXBCDebug::GlobalState &global,
                              rdcarray<SourceVariableMapping> &sourceVars,
                              const D3D11RenderState::Shader &shader, const ShaderReflection &refl,
                              const ShaderBindpointMapping &mapping)
{
  bytebuf cbufData;
  for(int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; i++)
  {
    if(shader.ConstantBuffers[i])
    {
      DXBCDebug::BindingSlot slot(i, 0);
      cbufData.clear();
      debugManager.GetBufferData(shader.ConstantBuffers[i], shader.CBOffsets[i] * sizeof(Vec4f),
                                 shader.CBCounts[i] * sizeof(Vec4f), cbufData);

      AddCBufferToGlobalState(program, global, sourceVars, refl, mapping, slot, cbufData);
    }
  }
}

ShaderDebugTrace *D3D11Replay::DebugVertex(uint32_t eventId, uint32_t vertid, uint32_t instid,
                                           uint32_t idx)
{
  using namespace DXBCBytecode;
  using namespace DXBCDebug;

  D3D11MarkerRegion region(
      StringFormat::Fmt("DebugVertex @ %u of (%u,%u,%u)", eventId, vertid, instid, idx));

  const DrawcallDescription *draw = m_pDevice->GetDrawcall(eventId);

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  ID3D11VertexShader *stateVS = NULL;
  m_pImmediateContext->VSGetShader(&stateVS, NULL, NULL);

  WrappedID3D11Shader<ID3D11VertexShader> *vs = (WrappedID3D11Shader<ID3D11VertexShader> *)stateVS;

  SAFE_RELEASE(stateVS);

  if(!vs)
    return new ShaderDebugTrace;

  DXBC::DXBCContainer *dxbc = vs->GetDXBC();
  const ShaderReflection &refl = vs->GetDetails();

  if(!dxbc)
    return new ShaderDebugTrace;

  dxbc->GetDisassembly();

  D3D11RenderState *rs = m_pImmediateContext->GetCurrentPipelineState();

  rdcarray<D3D11_INPUT_ELEMENT_DESC> inputlayout = m_pDevice->GetLayoutDesc(rs->IA.Layout);

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
      GetDebugManager()->GetBufferData(
          rs->IA.VBs[i], rs->IA.Offsets[i] + rs->IA.Strides[i] * (draw->vertexOffset + idx),
          rs->IA.Strides[i], vertData[i]);

      for(UINT isr = 1; isr <= MaxStepRate; isr++)
      {
        GetDebugManager()->GetBufferData(
            rs->IA.VBs[i],
            rs->IA.Offsets[i] + rs->IA.Strides[i] * (draw->instanceOffset + (instid / isr)),
            rs->IA.Strides[i], instData[i * MaxStepRate + isr - 1]);
      }

      GetDebugManager()->GetBufferData(rs->IA.VBs[i],
                                       rs->IA.Offsets[i] + rs->IA.Strides[i] * draw->instanceOffset,
                                       rs->IA.Strides[i], staticData[i]);
    }
  }

  InterpretDebugger *interpreter = new InterpretDebugger;
  ShaderDebugTrace *ret = interpreter->BeginDebug(dxbc, refl, vs->GetMapping(), 0);
  GlobalState &global = interpreter->global;
  ThreadState &state = interpreter->activeLane();

  AddCBuffersToGlobalState(*dxbc->GetDXBCByteCode(), *GetDebugManager(), global, ret->sourceVars,
                           rs->VS, refl, vs->GetMapping());

  for(size_t i = 0; i < state.inputs.size(); i++)
  {
    if(dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::Undefined ||
       dxbc->GetReflection()->InputSig[i].systemValue ==
           ShaderBuiltin::Position)    // SV_Position seems to get promoted
                                       // automatically, but it's invalid for
                                       // vertex input
    {
      const D3D11_INPUT_ELEMENT_DESC *el = NULL;

      rdcstr signame = strlower(dxbc->GetReflection()->InputSig[i].semanticName);

      for(size_t l = 0; l < inputlayout.size(); l++)
      {
        rdcstr layoutname = strlower(inputlayout[l].SemanticName);

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
        state.inputs[i].value.u.w = 1;

        if(fmt.compType == CompType::Float)
          state.inputs[i].value.f.w = 1.0f;
      }

      // interpret resource format types
      if(fmt.Special())
      {
        Vec3f *v3 = (Vec3f *)state.inputs[i].value.fv;
        Vec4f *v4 = (Vec4f *)state.inputs[i].value.fv;

        // only pull in all or nothing from these,
        // if there's only e.g. 3 bytes remaining don't read and unpack some of
        // a 4-byte resource format type
        size_t packedsize = 4;
        if(fmt.type == ResourceFormatType::R5G5B5A1 || fmt.type == ResourceFormatType::R5G6B5 ||
           fmt.type == ResourceFormatType::R4G4B4A4)
          packedsize = 2;

        if(srcData == NULL || packedsize > dataSize)
        {
          state.inputs[i].value.u.x = state.inputs[i].value.u.y = state.inputs[i].value.u.z = 0;
          if(fmt.compType == CompType::UInt || fmt.compType == CompType::SInt)
            state.inputs[i].value.u.w = 1;
          else
            state.inputs[i].value.f.w = 1.0f;
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
            state.inputs[i].value.u.z = (packed >> 0) & 0x3ff;
            state.inputs[i].value.u.y = (packed >> 10) & 0x3ff;
            state.inputs[i].value.u.x = (packed >> 20) & 0x3ff;
            state.inputs[i].value.u.w = (packed >> 30) & 0x003;
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
        if(srcData == NULL || size_t(fmt.compByteWidth * fmt.compCount) > dataSize)
        {
          state.inputs[i].value.u.x = state.inputs[i].value.u.y = state.inputs[i].value.u.z = 0;
          if(fmt.compType == CompType::UInt || fmt.compType == CompType::SInt)
            state.inputs[i].value.u.w = 1;
          else
            state.inputs[i].value.f.w = 1.0f;
        }
        else
        {
          for(uint32_t c = 0; c < fmt.compCount; c++)
          {
            if(fmt.compByteWidth == 1)
            {
              byte *src = srcData + c * fmt.compByteWidth;

              if(fmt.compType == CompType::UInt)
                state.inputs[i].value.uv[c] = *src;
              else if(fmt.compType == CompType::SInt)
                state.inputs[i].value.iv[c] = *((int8_t *)src);
              else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
                state.inputs[i].value.fv[c] = float(*src) / 255.0f;
              else if(fmt.compType == CompType::SNorm)
              {
                signed char *schar = (signed char *)src;

                // -128 is mapped to -1, then -127 to -127 are mapped to -1 to 1
                if(*schar == -128)
                  state.inputs[i].value.fv[c] = -1.0f;
                else
                  state.inputs[i].value.fv[c] = float(*schar) / 127.0f;
              }
              else
                RDCERR("Unexpected component type");
            }
            else if(fmt.compByteWidth == 2)
            {
              uint16_t *src = (uint16_t *)(srcData + c * fmt.compByteWidth);

              if(fmt.compType == CompType::Float)
                state.inputs[i].value.fv[c] = ConvertFromHalf(*src);
              else if(fmt.compType == CompType::UInt)
                state.inputs[i].value.uv[c] = *src;
              else if(fmt.compType == CompType::SInt)
                state.inputs[i].value.iv[c] = *((int16_t *)src);
              else if(fmt.compType == CompType::UNorm || fmt.compType == CompType::UNormSRGB)
                state.inputs[i].value.fv[c] = float(*src) / float(UINT16_MAX);
              else if(fmt.compType == CompType::SNorm)
              {
                int16_t *sint = (int16_t *)src;

                // -32768 is mapped to -1, then -32767 to -32767 are mapped to -1 to 1
                if(*sint == -32768)
                  state.inputs[i].value.fv[c] = -1.0f;
                else
                  state.inputs[i].value.fv[c] = float(*sint) / 32767.0f;
              }
              else
                RDCERR("Unexpected component type");
            }
            else if(fmt.compByteWidth == 4)
            {
              uint32_t *src = (uint32_t *)(srcData + c * fmt.compByteWidth);

              if(fmt.compType == CompType::Float || fmt.compType == CompType::UInt ||
                 fmt.compType == CompType::SInt)
                memcpy(&state.inputs[i].value.uv[c], src, 4);
              else
                RDCERR("Unexpected component type");
            }
          }

          if(fmt.BGRAOrder())
          {
            RDCASSERT(fmt.compCount == 4);
            std::swap(state.inputs[i].value.fv[2], state.inputs[i].value.fv[0]);
          }
        }
      }
    }
    else if(dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::VertexIndex)
    {
      uint32_t sv_vertid = vertid;

      if(draw->flags & DrawFlags::Indexed)
      {
        sv_vertid = idx - draw->baseVertex;
      }

      if(dxbc->GetReflection()->InputSig[i].varType == VarType::Float)
        state.inputs[i].value.f.x = state.inputs[i].value.f.y = state.inputs[i].value.f.z =
            state.inputs[i].value.f.w = (float)sv_vertid;
      else
        state.inputs[i].value.u.x = state.inputs[i].value.u.y = state.inputs[i].value.u.z =
            state.inputs[i].value.u.w = sv_vertid;
    }
    else if(dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::InstanceIndex)
    {
      if(dxbc->GetReflection()->InputSig[i].varType == VarType::Float)
        state.inputs[i].value.f.x = state.inputs[i].value.f.y = state.inputs[i].value.f.z =
            state.inputs[i].value.f.w = (float)instid;
      else
        state.inputs[i].value.u.x = state.inputs[i].value.u.y = state.inputs[i].value.u.z =
            state.inputs[i].value.u.w = instid;
    }
    else
    {
      RDCERR("Unhandled system value semantic on VS input");
    }
  }

  ret->constantBlocks = global.constantBlocks;
  ret->inputs = state.inputs;

  delete[] instData;

  dxbc->FillTraceLineInfo(*ret);

  return ret;
}

ShaderDebugTrace *D3D11Replay::DebugPixel(uint32_t eventId, uint32_t x, uint32_t y, uint32_t sample,
                                          uint32_t primitive)
{
  using namespace DXBCBytecode;
  using namespace DXBCDebug;

  D3D11MarkerRegion region(
      StringFormat::Fmt("DebugPixel @ %u of (%u,%u) %u / %u", eventId, x, y, sample, primitive));

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
    return new ShaderDebugTrace;

  D3D11RenderState *rs = m_pImmediateContext->GetCurrentPipelineState();

  DXBC::DXBCContainer *dxbc = ps->GetDXBC();
  const ShaderReflection &refl = ps->GetDetails();

  if(!dxbc)
    return new ShaderDebugTrace;

  dxbc->GetDisassembly();

  DXBC::DXBCContainer *prevdxbc = NULL;

  if(prevdxbc == NULL && gs != NULL)
    prevdxbc = gs->GetDXBC();
  if(prevdxbc == NULL && ds != NULL)
    prevdxbc = ds->GetDXBC();
  if(prevdxbc == NULL && vs != NULL)
    prevdxbc = vs->GetDXBC();
  RDCASSERT(prevdxbc);

  rdcarray<PSInputElement> initialValues;
  rdcarray<rdcstr> floatInputs;
  rdcarray<rdcstr> inputVarNames;
  rdcstr extractHlsl;
  int structureStride = 0;

  DXBCDebug::GatherPSInputDataForInitialValues(dxbc, *prevdxbc->GetReflection(), initialValues,
                                               floatInputs, inputVarNames, extractHlsl,
                                               structureStride);

  uint32_t overdrawLevels = 100;    // maximum number of overdraw levels

  // If the pipe contains a geometry shader, then SV_PrimitiveID cannot be used in the pixel
  // shader without being emitted from the geometry shader. For now, check if this semantic
  // will succeed in a new pixel shader with the rest of the pipe unchanged
  bool usePrimitiveID = (prevdxbc->m_Type != DXBC::ShaderType::Geometry);
  for(const PSInputElement &e : initialValues)
  {
    if(e.sysattribute == ShaderBuiltin::PrimitiveIndex)
    {
      usePrimitiveID = true;
      break;
    }
  }

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

  if(usePrimitiveID)
  {
    extractHlsl += R"(
void ExtractInputsPS(PSInput IN, float4 debug_pixelPos : SV_Position, uint prim : SV_PrimitiveID,
                     uint sample : SV_SampleIndex, uint covge : SV_Coverage,
                     bool fface : SV_IsFrontFace)
{
)";
  }
  else
  {
    extractHlsl += R"(
void ExtractInputsPS(PSInput IN, float4 debug_pixelPos : SV_Position,
                     uint sample : SV_SampleIndex, uint covge : SV_Coverage,
                     bool fface : SV_IsFrontFace)
{
)";
  }

  extractHlsl += "  uint idx = " + ToStr(overdrawLevels) + ";\n";
  extractHlsl += StringFormat::Fmt(
      "  if(abs(debug_pixelPos.x - %u.5) < 0.5f && abs(debug_pixelPos.y - %u.5) < 0.5f)\n", x, y);
  extractHlsl += "    InterlockedAdd(PSInitialBuffer[0].hit, 1, idx);\n\n";
  extractHlsl += "  idx = min(idx, " + ToStr(overdrawLevels) + ");\n\n";
  extractHlsl += "  PSInitialBuffer[idx].pos = debug_pixelPos.xyz;\n";

  if(usePrimitiveID)
    extractHlsl += "  PSInitialBuffer[idx].prim = prim;\n";
  else
    extractHlsl += "  PSInitialBuffer[idx].prim = 0;\n";

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
      rdcstr name, swizzle = "xyzw";
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
      rdcstr expandSwizzle = swizzle;
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
    const rdcstr &name = floatInputs[i];
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
    return new ShaderDebugTrace;
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
      return new ShaderDebugTrace;
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
    return new ShaderDebugTrace;
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
      return new ShaderDebugTrace;
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
    return new ShaderDebugTrace;
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
      return new ShaderDebugTrace;
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
    return new ShaderDebugTrace;
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
      SAFE_DELETE_ARRAY(initialData);
      return new ShaderDebugTrace;
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
    return new ShaderDebugTrace;
  }

  // if we encounter multiple hits at our destination pixel co-ord (or any other) we
  // check to see if a specific primitive was requested (via primitive parameter not
  // being set to ~0U). If it was, debug that pixel, otherwise do a best-estimate
  // of which fragment was the last to successfully depth test and debug that, just by
  // checking if the depth test is ordered and picking the final fragment in the series

  // figure out the TL pixel's coords. Assume even top left (towards 0,0)
  // this isn't spec'd but is a reasonable assumption.
  int xTL = x & (~1);
  int yTL = y & (~1);

  // get the index of our desired pixel
  int destIdx = (x - xTL) + 2 * (y - yTL);

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

      if(winner == NULL)
      {
        // If we haven't picked a winner at all yet, use the first one
        winner = hit;
        evalSampleCache = ((float *)evalData) + evalSampleCacheData.size() * 4 * i;
      }
      else if(hit->sample == sample)
      {
        // If this hit is for the sample we want, check whether it's a better pick
        if(winner->sample != sample)
        {
          // The previously selected winner was for the wrong sample, use this one
          winner = hit;
          evalSampleCache = ((float *)evalData) + evalSampleCacheData.size() * 4 * i;
        }
        else if((depthFunc == D3D11_COMPARISON_ALWAYS || depthFunc == D3D11_COMPARISON_NEVER ||
                 depthFunc == D3D11_COMPARISON_NOT_EQUAL || depthFunc == D3D11_COMPARISON_EQUAL))
        {
          // For depth functions without an inequality comparison, use the last sample encountered
          winner = hit;
          evalSampleCache = ((float *)evalData) + evalSampleCacheData.size() * 4 * i;
        }
        else if((depthFunc == D3D11_COMPARISON_LESS && hit->depth < winner->depth) ||
                (depthFunc == D3D11_COMPARISON_LESS_EQUAL && hit->depth <= winner->depth) ||
                (depthFunc == D3D11_COMPARISON_GREATER && hit->depth > winner->depth) ||
                (depthFunc == D3D11_COMPARISON_GREATER_EQUAL && hit->depth >= winner->depth))
        {
          // For depth functions with an inequality, find the hit that "wins" the most
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
    return new ShaderDebugTrace;
  }

  tracker.State().ApplyState(m_pImmediateContext);

  InterpretDebugger *interpreter = new InterpretDebugger;
  ShaderDebugTrace *ret = interpreter->BeginDebug(dxbc, refl, ps->GetMapping(), destIdx);
  GlobalState &global = interpreter->global;
  ThreadState &state = interpreter->activeLane();

  AddCBuffersToGlobalState(*dxbc->GetDXBCByteCode(), *GetDebugManager(), global, ret->sourceVars,
                           rs->PS, refl, ps->GetMapping());

  global.sampleEvalRegisterMask = sampleEvalRegisterMask;

  {
    DebugHit *hit = winner;

    rdcarray<ShaderVariable> &ins = state.inputs;
    if(!ins.empty() &&
       ins.back().name ==
           dxbc->GetDXBCByteCode()->GetRegisterName(DXBCBytecode::TYPE_INPUT_COVERAGE_MASK, 0))
      ins.back().value.u.x = hit->coverage;

    state.semantics.coverage = hit->coverage;
    state.semantics.primID = hit->primitive;
    state.semantics.isFrontFace = hit->isFrontFace;

    uint32_t *data = &hit->rawdata;

    float *pos_ddx = (float *)data;

    // ddx(SV_Position.x) MUST be 1.0
    if(*pos_ddx != 1.0f)
    {
      RDCERR("Derivatives invalid");
      SAFE_DELETE_ARRAY(initialData);
      SAFE_DELETE_ARRAY(evalData);
      delete interpreter;
      delete ret;
      return new ShaderDebugTrace;
    }

    data++;

    for(size_t i = 0; i < initialValues.size(); i++)
    {
      int32_t *rawout = NULL;

      if(initialValues[i].reg >= 0)
      {
        ShaderVariable &invar = ins[initialValues[i].reg];

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
      {
        interpreter->workgroup[i].inputs = state.inputs;
        interpreter->workgroup[i].semantics = state.semantics;
        interpreter->workgroup[i].variables = state.variables;
        interpreter->workgroup[i].SetHelper();
      }
    }

    // fetch any inputs that were evaluated at sample granularity
    for(const GlobalState::SampleEvalCacheKey &key : evalSampleCacheData)
    {
      // start with the basic input value
      ShaderVariable var = state.inputs[key.inputRegisterIndex];

      // copy over the value into the variable
      memcpy(var.value.fv, evalSampleCache, var.columns * sizeof(float));

      // store in the global cache for each quad. We'll apply derivatives below to adjust for each
      GlobalState::SampleEvalCacheKey k = key;
      for(int i = 0; i < 4; i++)
      {
        k.quadIndex = i;
        global.sampleEvalCache[k] = var;
      }

      // advance past this data - always by float4 as that's the buffer stride
      evalSampleCache += 4;
    }

    ApplyAllDerivatives(global, interpreter->workgroup, destIdx, initialValues, (float *)data);
  }

  ret->inputs = state.inputs;
  ret->constantBlocks = global.constantBlocks;

  SAFE_DELETE_ARRAY(initialData);
  SAFE_DELETE_ARRAY(evalData);

  dxbc->FillTraceLineInfo(*ret);

  return ret;
}

ShaderDebugTrace *D3D11Replay::DebugThread(uint32_t eventId, const uint32_t groupid[3],
                                           const uint32_t threadid[3])
{
  using namespace DXBCBytecode;
  using namespace DXBCDebug;

  D3D11MarkerRegion region(StringFormat::Fmt("DebugThread @ %u: [%u, %u, %u] (%u, %u, %u)", eventId,
                                             groupid[0], groupid[1], groupid[2], threadid[0],
                                             threadid[1], threadid[2]));

  D3D11RenderStateTracker tracker(m_pImmediateContext);

  ID3D11ComputeShader *stateCS = NULL;
  m_pImmediateContext->CSGetShader(&stateCS, NULL, NULL);

  WrappedID3D11Shader<ID3D11ComputeShader> *cs = (WrappedID3D11Shader<ID3D11ComputeShader> *)stateCS;

  SAFE_RELEASE(stateCS);

  if(!cs)
    return new ShaderDebugTrace;

  DXBC::DXBCContainer *dxbc = cs->GetDXBC();
  const ShaderReflection &refl = cs->GetDetails();

  if(!dxbc)
    return new ShaderDebugTrace;

  dxbc->GetDisassembly();

  D3D11RenderState *rs = m_pImmediateContext->GetCurrentPipelineState();

  InterpretDebugger *interpreter = new InterpretDebugger;
  ShaderDebugTrace *ret = interpreter->BeginDebug(dxbc, refl, cs->GetMapping(), 0);
  GlobalState &global = interpreter->global;
  ThreadState &state = interpreter->activeLane();

  AddCBuffersToGlobalState(*dxbc->GetDXBCByteCode(), *GetDebugManager(), global, ret->sourceVars,
                           rs->CS, refl, cs->GetMapping());

  for(int i = 0; i < 3; i++)
  {
    state.semantics.GroupID[i] = groupid[i];
    state.semantics.ThreadID[i] = threadid[i];
  }

  ret->constantBlocks = global.constantBlocks;

  dxbc->FillTraceLineInfo(*ret);

  // add fake inputs for semantics
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
          memcpy(v.value.uv, state.semantics.GroupID, sizeof(uint32_t) * 3);
          v.columns = 3;
          break;
        case TYPE_INPUT_THREAD_ID_IN_GROUP:
          memcpy(v.value.uv, state.semantics.ThreadID, sizeof(uint32_t) * 3);
          v.columns = 3;
          break;
        case TYPE_INPUT_THREAD_ID:
          v.value.u.x =
              state.semantics.GroupID[0] * dxbc->GetReflection()->DispatchThreadsDimension[0] +
              state.semantics.ThreadID[0];
          v.value.u.y =
              state.semantics.GroupID[1] * dxbc->GetReflection()->DispatchThreadsDimension[1] +
              state.semantics.ThreadID[1];
          v.value.u.z =
              state.semantics.GroupID[2] * dxbc->GetReflection()->DispatchThreadsDimension[2] +
              state.semantics.ThreadID[2];
          v.columns = 3;
          break;
        case TYPE_INPUT_THREAD_ID_IN_GROUP_FLATTENED:
          v.value.u.x =
              state.semantics.ThreadID[2] * dxbc->GetReflection()->DispatchThreadsDimension[0] *
                  dxbc->GetReflection()->DispatchThreadsDimension[1] +
              state.semantics.ThreadID[1] * dxbc->GetReflection()->DispatchThreadsDimension[0] +
              state.semantics.ThreadID[0];
          v.columns = 1;
          break;
        default: v.columns = 4; break;
      }

      ret->inputs.push_back(v);
    }
  }

  return ret;
}

rdcarray<ShaderDebugState> D3D11Replay::ContinueDebug(ShaderDebugger *debugger)
{
  DXBCDebug::InterpretDebugger *interpreter = (DXBCDebug::InterpretDebugger *)debugger;

  if(!interpreter)
    return NULL;

  D3D11DebugAPIWrapper apiWrapper(m_pDevice, interpreter->dxbc, interpreter->global);

  D3D11MarkerRegion region("ContinueDebug Simulation Loop");

  return interpreter->ContinueDebug(&apiWrapper);
}

void D3D11Replay::FreeDebugger(ShaderDebugger *debugger)
{
  delete debugger;
}
