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

#include "d3d11_debug.h"
#include "common/shader_cache.h"
#include "data/resource.h"
#include "driver/d3d11/d3d11_resources.h"
#include "maths/camera.h"
#include "maths/formatpacking.h"
#include "maths/matrix.h"
#include "d3d11_context.h"
#include "d3d11_manager.h"
#include "d3d11_renderstate.h"
#include "d3d11_shader_cache.h"

#include "data/hlsl/hlsl_cbuffers.h"

D3D11DebugManager::D3D11DebugManager(WrappedID3D11Device *wrapper)
{
  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->RegisterMemoryRegion(this, sizeof(D3D11DebugManager));

  m_pDevice = wrapper;
  m_pImmediateContext = wrapper->GetImmediateContext();

  m_pDevice->GetShaderCache()->SetCaching(true);

  // create things needed both during capture and replay
  InitCommonResources();

  // now do replay-only initialisation
  if(RenderDoc::Inst().IsReplayApp())
    InitReplayResources();

  m_pDevice->GetShaderCache()->SetCaching(false);
}

D3D11DebugManager::~D3D11DebugManager()
{
  ShutdownResources();

  while(!m_ShaderItemCache.empty())
  {
    CacheElem &elem = m_ShaderItemCache.back();
    elem.Release();
    m_ShaderItemCache.pop_back();
  }

  if(RenderDoc::Inst().GetCrashHandler())
    RenderDoc::Inst().GetCrashHandler()->UnregisterMemoryRegion(this);
}

//////////////////////////////////////////////////////
// debug/replay functions

ID3D11Buffer *D3D11DebugManager::MakeCBuffer(UINT size)
{
  D3D11_BUFFER_DESC bufDesc;

  bufDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  bufDesc.Usage = D3D11_USAGE_DYNAMIC;
  bufDesc.ByteWidth = size;
  bufDesc.StructureByteStride = 0;
  bufDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  bufDesc.MiscFlags = 0;

  ID3D11Buffer *ret = NULL;

  HRESULT hr = m_pDevice->CreateBuffer(&bufDesc, NULL, &ret);

  if(FAILED(hr))
  {
    RDCERR("Failed to create CBuffer HRESULT: %s", ToStr(hr).c_str());
    return NULL;
  }

  return ret;
}

void D3D11DebugManager::FillCBuffer(ID3D11Buffer *buf, const void *data, size_t size)
{
  D3D11_MAPPED_SUBRESOURCE mapped;

  HRESULT hr = m_pImmediateContext->GetReal()->Map(UNWRAP(WrappedID3D11Buffer, buf), 0,
                                                   D3D11_MAP_WRITE_DISCARD, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Can't fill cbuffer HRESULT: %s", ToStr(hr).c_str());
  }
  else
  {
    memcpy(mapped.pData, data, size);
    m_pImmediateContext->GetReal()->Unmap(UNWRAP(WrappedID3D11Buffer, buf), 0);
  }
}

ID3D11Buffer *D3D11DebugManager::MakeCBuffer(const void *data, size_t size)
{
  int idx = publicCBufIdx;

  RDCASSERT(size <= PublicCBufferSize, size, PublicCBufferSize);

  FillCBuffer(PublicCBuffers[idx], data, RDCMIN(size, PublicCBufferSize));

  publicCBufIdx = (publicCBufIdx + 1) % ARRAY_COUNT(PublicCBuffers);

  return PublicCBuffers[idx];
}

void D3D11DebugManager::InitCommonResources()
{
  D3D11ShaderCache *shaderCache = m_pDevice->GetShaderCache();
  D3D11ResourceManager *rm = m_pDevice->GetResourceManager();

  std::string multisamplehlsl = GetEmbeddedResource(multisample_hlsl);
  std::string hlsl = GetEmbeddedResource(misc_hlsl);

  if(m_pDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0)
  {
    CopyMSToArrayPS =
        shaderCache->MakePShader(multisamplehlsl.c_str(), "RENDERDOC_CopyMSToArray", "ps_5_0");
    if(CopyMSToArrayPS)
      m_pDevice->InternalRef();
    CopyArrayToMSPS =
        shaderCache->MakePShader(multisamplehlsl.c_str(), "RENDERDOC_CopyArrayToMS", "ps_5_0");
    if(CopyArrayToMSPS)
      m_pDevice->InternalRef();
    FloatCopyMSToArrayPS =
        shaderCache->MakePShader(multisamplehlsl.c_str(), "RENDERDOC_FloatCopyMSToArray", "ps_5_0");
    if(FloatCopyMSToArrayPS)
      m_pDevice->InternalRef();
    FloatCopyArrayToMSPS =
        shaderCache->MakePShader(multisamplehlsl.c_str(), "RENDERDOC_FloatCopyArrayToMS", "ps_5_0");
    if(FloatCopyArrayToMSPS)
      m_pDevice->InternalRef();
    DepthCopyMSToArrayPS =
        shaderCache->MakePShader(multisamplehlsl.c_str(), "RENDERDOC_DepthCopyMSToArray", "ps_5_0");
    if(DepthCopyMSToArrayPS)
      m_pDevice->InternalRef();
    DepthCopyArrayToMSPS =
        shaderCache->MakePShader(multisamplehlsl.c_str(), "RENDERDOC_DepthCopyArrayToMS", "ps_5_0");
    if(DepthCopyArrayToMSPS)
      m_pDevice->InternalRef();
    MSArrayCopyVS = shaderCache->MakeVShader(hlsl.c_str(), "RENDERDOC_FullscreenVS", "vs_4_0");
    if(MSArrayCopyVS)
      m_pDevice->InternalRef();
  }
  else
  {
    RDCWARN("Device feature level below 11_0, MSAA <-> Array copies will not be supported.");
    CopyMSToArrayPS = NULL;
    CopyArrayToMSPS = NULL;
    FloatCopyMSToArrayPS = NULL;
    FloatCopyArrayToMSPS = NULL;
    DepthCopyMSToArrayPS = NULL;
    DepthCopyArrayToMSPS = NULL;
  }

  // mark created resources as internal during capture so they aren't included in capture files.
  rm->SetInternalResource(CopyMSToArrayPS);
  rm->SetInternalResource(CopyArrayToMSPS);
  rm->SetInternalResource(FloatCopyMSToArrayPS);
  rm->SetInternalResource(FloatCopyArrayToMSPS);
  rm->SetInternalResource(DepthCopyMSToArrayPS);
  rm->SetInternalResource(DepthCopyArrayToMSPS);
  rm->SetInternalResource(MSArrayCopyVS);

  for(int i = 0; i < ARRAY_COUNT(PublicCBuffers); i++)
  {
    PublicCBuffers[i] = MakeCBuffer(PublicCBufferSize);
    m_pDevice->InternalRef();
    rm->SetInternalResource(PublicCBuffers[i]);
  }

  publicCBufIdx = 0;
}

void D3D11DebugManager::InitReplayResources()
{
  D3D11ShaderCache *shaderCache = m_pDevice->GetShaderCache();

  HRESULT hr = S_OK;

  {
    std::string hlsl = GetEmbeddedResource(pixelhistory_hlsl);

    PixelHistoryUnusedCS =
        shaderCache->MakeCShader(hlsl.c_str(), "RENDERDOC_PixelHistoryUnused", "cs_5_0");
    PixelHistoryCopyCS =
        shaderCache->MakeCShader(hlsl.c_str(), "RENDERDOC_PixelHistoryCopyPixel", "cs_5_0");
  }

  RDCCOMPILE_ASSERT(eTexType_1D == RESTYPE_TEX1D, "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_2D == RESTYPE_TEX2D, "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_3D == RESTYPE_TEX3D, "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_Depth == RESTYPE_DEPTH, "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_Stencil == RESTYPE_DEPTH_STENCIL,
                    "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_DepthMS == RESTYPE_DEPTH_MS,
                    "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_StencilMS == RESTYPE_DEPTH_STENCIL_MS,
                    "Tex type enum doesn't match shader defines");
  RDCCOMPILE_ASSERT(eTexType_2DMS == RESTYPE_TEX2D_MS,
                    "Tex type enum doesn't match shader defines");

  {
    D3D11_BUFFER_DESC desc;

    desc.StructureByteStride = 0;
    desc.ByteWidth = STAGE_BUFFER_BYTE_SIZE;
    desc.BindFlags = 0;
    desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.Usage = D3D11_USAGE_STAGING;

    hr = m_pDevice->CreateBuffer(&desc, NULL, &StageBuffer);

    if(FAILED(hr))
      RDCERR("Failed to create map staging buffer HRESULT: %s", ToStr(hr).c_str());
  }

  {
    D3D11_TEXTURE2D_DESC desc = {};

    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_D16_UNORM;
    desc.Width = 1;
    desc.Height = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Texture2D *dummyTex = NULL;
    hr = m_pDevice->CreateTexture2D(&desc, NULL, &dummyTex);

    if(FAILED(hr))
    {
      RDCERR("Failed to create dummy predicate tex HRESULT: %s", ToStr(hr).c_str());
    }
    else
    {
      hr = m_pDevice->CreateDepthStencilView(dummyTex, NULL, &PredicateDSV);

      if(FAILED(hr))
      {
        RDCERR("Failed to predicate DSV HRESULT: %s", ToStr(hr).c_str());
      }
    }

    SAFE_RELEASE(dummyTex);
  }
}

void D3D11DebugManager::ShutdownResources()
{
  SAFE_RELEASE(StageBuffer);

  SAFE_RELEASE(PredicateDSV);

  SAFE_RELEASE(CopyMSToArrayPS);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(CopyArrayToMSPS);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(FloatCopyMSToArrayPS);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(FloatCopyArrayToMSPS);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(DepthCopyMSToArrayPS);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(DepthCopyArrayToMSPS);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(PixelHistoryUnusedCS);
  m_pDevice->InternalRelease();
  SAFE_RELEASE(PixelHistoryCopyCS);
  m_pDevice->InternalRelease();

  SAFE_RELEASE(MSArrayCopyVS);
  m_pDevice->InternalRelease();

  for(int i = 0; i < ARRAY_COUNT(PublicCBuffers); i++)
  {
    SAFE_RELEASE(PublicCBuffers[i]);
    m_pDevice->InternalRelease();
  }
}

uint32_t D3D11DebugManager::GetStructCount(ID3D11UnorderedAccessView *uav)
{
  m_pImmediateContext->CopyStructureCount(StageBuffer, 0, uav);

  D3D11_MAPPED_SUBRESOURCE mapped;
  HRESULT hr = m_pImmediateContext->Map(StageBuffer, 0, D3D11_MAP_READ, 0, &mapped);

  if(FAILED(hr))
  {
    RDCERR("Failed to Map HRESULT: %s", ToStr(hr).c_str());
    return ~0U;
  }

  uint32_t ret = *((uint32_t *)mapped.pData);

  m_pImmediateContext->Unmap(StageBuffer, 0);

  return ret;
}

void D3D11DebugManager::GetBufferData(ID3D11Buffer *buffer, uint64_t offset, uint64_t length,
                                      bytebuf &ret)
{
  D3D11_MAPPED_SUBRESOURCE mapped;

  if(buffer == NULL)
    return;

  RDCASSERT(offset < 0xffffffff);
  RDCASSERT(length <= 0xffffffff);

  uint32_t offs = (uint32_t)offset;
  uint32_t len = (uint32_t)length;

  D3D11_BUFFER_DESC desc;
  buffer->GetDesc(&desc);

  if(offs >= desc.ByteWidth)
  {
    // can't read past the end of the buffer, return empty
    return;
  }

  if(len == 0)
  {
    len = desc.ByteWidth - offs;
  }

  if(len > 0 && offs + len > desc.ByteWidth)
  {
    RDCWARN("Attempting to read off the end of the buffer (%llu %llu). Will be clamped (%u)",
            offset, length, desc.ByteWidth);
    len = RDCMIN(len, desc.ByteWidth - offs);
  }

  uint32_t outOffs = 0;

  ret.resize(len);

  D3D11_BOX box;
  box.top = 0;
  box.bottom = 1;
  box.front = 0;
  box.back = 1;

  while(len > 0)
  {
    uint32_t chunkSize = RDCMIN(len, STAGE_BUFFER_BYTE_SIZE);

    if(desc.StructureByteStride > 0)
      chunkSize -= (chunkSize % desc.StructureByteStride);

    box.left = RDCMIN(offs + outOffs, desc.ByteWidth);
    box.right = RDCMIN(offs + outOffs + chunkSize, desc.ByteWidth);

    if(box.right - box.left == 0)
      break;

    m_pImmediateContext->GetReal()->CopySubresourceRegion(
        UNWRAP(WrappedID3D11Buffer, StageBuffer), 0, 0, 0, 0, UNWRAP(WrappedID3D11Buffer, buffer),
        0, &box);

    HRESULT hr = m_pImmediateContext->GetReal()->Map(UNWRAP(WrappedID3D11Buffer, StageBuffer), 0,
                                                     D3D11_MAP_READ, 0, &mapped);

    if(FAILED(hr))
    {
      RDCERR("Failed to map bufferdata buffer HRESULT: %s", ToStr(hr).c_str());
      return;
    }
    else
    {
      memcpy(&ret[outOffs], mapped.pData, RDCMIN(len, STAGE_BUFFER_BYTE_SIZE));

      m_pImmediateContext->GetReal()->Unmap(UNWRAP(WrappedID3D11Buffer, StageBuffer), 0);
    }

    outOffs += chunkSize;
    len -= chunkSize;
  }
}

void D3D11DebugManager::RenderForPredicate()
{
  // just somehow draw a quad that renders some pixels to fill the predicate with TRUE
  m_pImmediateContext->ClearState();
  D3D11_VIEWPORT viewport = {0, 0, 1, 1, 0.0f, 1.0f};
  m_pImmediateContext->RSSetViewports(1, &viewport);
  m_pImmediateContext->VSSetShader(MSArrayCopyVS, NULL, 0);
  m_pImmediateContext->PSSetShader(NULL, NULL, 0);
  m_pImmediateContext->OMSetRenderTargets(0, NULL, PredicateDSV);
  m_pImmediateContext->Draw(3, 0);
}

void D3D11Replay::GeneralMisc::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  HRESULT hr = S_OK;

  D3D11_RASTERIZER_DESC rastDesc;
  RDCEraseEl(rastDesc);

  rastDesc.CullMode = D3D11_CULL_NONE;
  rastDesc.FillMode = D3D11_FILL_SOLID;
  rastDesc.DepthBias = 0;

  hr = device->CreateRasterizerState(&rastDesc, &RasterState);

  if(FAILED(hr))
    RDCERR("Failed to create default rasterizer state HRESULT: %s", ToStr(hr).c_str());

  rastDesc.ScissorEnable = TRUE;

  hr = device->CreateRasterizerState(&rastDesc, &RasterScissorState);

  if(FAILED(hr))
    RDCERR("Failed to create scissoring rasterizer state HRESULT: %s", ToStr(hr).c_str());

  {
    std::string hlsl = GetEmbeddedResource(misc_hlsl);

    FullscreenVS = shaderCache->MakeVShader(hlsl.c_str(), "RENDERDOC_FullscreenVS", "vs_4_0");

    FixedColPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_FixedColPS", "ps_4_0");
    CheckerboardPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_CheckerboardPS", "ps_4_0");
  }
}

void D3D11Replay::GeneralMisc::Release()
{
  SAFE_RELEASE(RasterState);
  SAFE_RELEASE(RasterScissorState);

  SAFE_RELEASE(FullscreenVS);
  SAFE_RELEASE(CheckerboardPS);
  SAFE_RELEASE(FixedColPS);
}

void D3D11Replay::TextureRendering::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  HRESULT hr = S_OK;

  {
    std::string hlsl = GetEmbeddedResource(texdisplay_hlsl);

    TexDisplayVS = shaderCache->MakeVShader(hlsl.c_str(), "RENDERDOC_TexDisplayVS", "vs_4_0");
    TexDisplayPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_TexDisplayPS", "ps_5_0");
  }

  {
    D3D11_BLEND_DESC blendDesc;
    RDCEraseEl(blendDesc);

    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = device->CreateBlendState(&blendDesc, &BlendState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create default blendstate HRESULT: %s", ToStr(hr).c_str());
    }
  }

  {
    D3D11_SAMPLER_DESC sampDesc;
    RDCEraseEl(sampDesc);

    sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sampDesc.MaxAnisotropy = 1;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = FLT_MAX;
    sampDesc.MipLODBias = 0.0f;

    hr = device->CreateSamplerState(&sampDesc, &LinearSampState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create linear sampler state HRESULT: %s", ToStr(hr).c_str());
    }

    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;

    hr = device->CreateSamplerState(&sampDesc, &PointSampState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create point sampler state HRESULT: %s", ToStr(hr).c_str());
    }
  }
}

void D3D11Replay::TextureRendering::Release()
{
  SAFE_RELEASE(PointSampState);
  SAFE_RELEASE(LinearSampState);
  SAFE_RELEASE(BlendState);
  SAFE_RELEASE(TexDisplayVS);
  SAFE_RELEASE(TexDisplayPS);
}

void D3D11Replay::OverlayRendering::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  {
    std::string hlsl = GetEmbeddedResource(misc_hlsl);

    FullscreenVS = shaderCache->MakeVShader(hlsl.c_str(), "RENDERDOC_FullscreenVS", "vs_4_0");

    hlsl = GetEmbeddedResource(quadoverdraw_hlsl);

    QuadOverdrawPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_QuadOverdrawPS", "ps_5_0");
    QOResolvePS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_QOResolvePS", "ps_5_0");
  }

  {
    std::string meshhlsl = GetEmbeddedResource(mesh_hlsl);

    TriangleSizeGS =
        shaderCache->MakeGShader(meshhlsl.c_str(), "RENDERDOC_TriangleSizeGS", "gs_4_0");
    TriangleSizePS =
        shaderCache->MakePShader(meshhlsl.c_str(), "RENDERDOC_TriangleSizePS", "ps_4_0");
  }
}

void D3D11Replay::OverlayRendering::Release()
{
  SAFE_RELEASE(FullscreenVS);
  SAFE_RELEASE(QuadOverdrawPS);
  SAFE_RELEASE(QOResolvePS);
  SAFE_RELEASE(TriangleSizeGS);
  SAFE_RELEASE(TriangleSizePS);

  SAFE_RELEASE(Texture);
}

void D3D11Replay::MeshRendering::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  HRESULT hr = S_OK;

  {
    D3D11_DEPTH_STENCIL_DESC desc;

    desc.BackFace.StencilFailOp = desc.BackFace.StencilPassOp = desc.BackFace.StencilDepthFailOp =
        D3D11_STENCIL_OP_KEEP;
    desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp =
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthEnable = FALSE;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.StencilEnable = FALSE;
    desc.StencilReadMask = desc.StencilWriteMask = 0xff;

    hr = device->CreateDepthStencilState(&desc, &NoDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create no-depth depthstencilstate HRESULT: %s", ToStr(hr).c_str());
    }

    desc.DepthEnable = TRUE;
    desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;

    hr = device->CreateDepthStencilState(&desc, &LessEqualDepthState);
  }

  {
    D3D11_RASTERIZER_DESC desc;
    desc.AntialiasedLineEnable = TRUE;
    desc.DepthBias = 0;
    desc.DepthBiasClamp = 0.0f;
    desc.DepthClipEnable = FALSE;
    desc.FrontCounterClockwise = FALSE;
    desc.MultisampleEnable = TRUE;
    desc.ScissorEnable = FALSE;
    desc.SlopeScaledDepthBias = 0.0f;
    desc.FillMode = D3D11_FILL_WIREFRAME;
    desc.CullMode = D3D11_CULL_NONE;

    hr = device->CreateRasterizerState(&desc, &WireframeRasterState);
    if(FAILED(hr))
      RDCERR("Failed to create m_WireframeHelpersRS HRESULT: %s", ToStr(hr).c_str());

    desc.FrontCounterClockwise = FALSE;
    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = D3D11_CULL_NONE;

    hr = device->CreateRasterizerState(&desc, &SolidRasterState);
    if(FAILED(hr))
      RDCERR("Failed to create m_SolidHelpersRS HRESULT: %s", ToStr(hr).c_str());
  }

  {
    D3D11_BLEND_DESC desc;
    RDCEraseEl(desc);

    desc.AlphaToCoverageEnable = TRUE;
    desc.IndependentBlendEnable = FALSE;
    desc.RenderTarget[0].BlendEnable = TRUE;
    desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    desc.RenderTarget[0].RenderTargetWriteMask = 0xf;

    hr = device->CreateBlendState(&desc, &WireframeHelpersBS);
    if(FAILED(hr))
      RDCERR("Failed to create m_WireframeHelpersRS HRESULT: %s", ToStr(hr).c_str());
  }

  {
    // these elements are just for signature matching sake, so we don't need correct offsets/slots
    D3D11_INPUT_ELEMENT_DESC inputDescSecondary[2] = {
        {"pos", 0, DXGI_FORMAT_R32G32B32A32_FLOAT}, {"sec", 0, DXGI_FORMAT_R8G8B8A8_UNORM},
    };

    std::string meshhlsl = GetEmbeddedResource(mesh_hlsl);

    std::vector<byte> bytecode;

    MeshVS = shaderCache->MakeVShader(meshhlsl.c_str(), "RENDERDOC_MeshVS", "vs_4_0", 2,
                                      inputDescSecondary, &GenericLayout, &bytecode);
    MeshGS = shaderCache->MakeGShader(meshhlsl.c_str(), "RENDERDOC_MeshGS", "gs_4_0");
    MeshPS = shaderCache->MakePShader(meshhlsl.c_str(), "RENDERDOC_MeshPS", "ps_4_0");

    MeshVSBytecode = new byte[bytecode.size()];
    MeshVSBytelen = (uint32_t)bytecode.size();
    memcpy(MeshVSBytecode, &bytecode[0], bytecode.size());
  }

  {
    Vec4f TLN = Vec4f(-1.0f, 1.0f, 0.0f, 1.0f);    // TopLeftNear, etc...
    Vec4f TRN = Vec4f(1.0f, 1.0f, 0.0f, 1.0f);
    Vec4f BLN = Vec4f(-1.0f, -1.0f, 0.0f, 1.0f);
    Vec4f BRN = Vec4f(1.0f, -1.0f, 0.0f, 1.0f);

    Vec4f TLF = Vec4f(-1.0f, 1.0f, 1.0f, 1.0f);
    Vec4f TRF = Vec4f(1.0f, 1.0f, 1.0f, 1.0f);
    Vec4f BLF = Vec4f(-1.0f, -1.0f, 1.0f, 1.0f);
    Vec4f BRF = Vec4f(1.0f, -1.0f, 1.0f, 1.0f);

    // 12 frustum lines => 24 verts
    Vec4f axisVB[24] = {
        TLN, TRN, TRN, BRN, BRN, BLN, BLN, TLN,

        TLN, TLF, TRN, TRF, BLN, BLF, BRN, BRF,

        TLF, TRF, TRF, BRF, BRF, BLF, BLF, TLF,
    };

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = axisVB;
    data.SysMemPitch = data.SysMemSlicePitch = 0;

    D3D11_BUFFER_DESC bdesc;
    bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bdesc.CPUAccessFlags = 0;
    bdesc.ByteWidth = sizeof(axisVB);
    bdesc.MiscFlags = 0;
    bdesc.Usage = D3D11_USAGE_IMMUTABLE;

    hr = device->CreateBuffer(&bdesc, &data, &FrustumHelper);

    if(FAILED(hr))
      RDCERR("Failed to create m_FrustumHelper HRESULT: %s", ToStr(hr).c_str());
  }

  {
    Vec4f axisVB[6] = {
        Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f),
        Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f),
    };

    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = axisVB;
    data.SysMemPitch = data.SysMemSlicePitch = 0;

    D3D11_BUFFER_DESC bdesc;
    bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bdesc.CPUAccessFlags = 0;
    bdesc.ByteWidth = sizeof(axisVB);
    bdesc.MiscFlags = 0;
    bdesc.Usage = D3D11_USAGE_IMMUTABLE;

    hr = device->CreateBuffer(&bdesc, &data, &AxisHelper);
    if(FAILED(hr))
      RDCERR("Failed to create m_AxisHelper HRESULT: %s", ToStr(hr).c_str());
  }

  {
    D3D11_BUFFER_DESC bdesc;
    bdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bdesc.ByteWidth = sizeof(Vec4f) * 24;
    bdesc.MiscFlags = 0;
    bdesc.Usage = D3D11_USAGE_DYNAMIC;

    hr = device->CreateBuffer(&bdesc, NULL, &TriHighlightHelper);

    if(FAILED(hr))
      RDCERR("Failed to create m_TriHighlightHelper HRESULT: %s", ToStr(hr).c_str());
  }
}

void D3D11Replay::MeshRendering::Release()
{
  SAFE_RELEASE(MeshLayout);

  SAFE_RELEASE(GenericLayout);
  SAFE_RELEASE(NoDepthState);
  SAFE_RELEASE(LessEqualDepthState);

  SAFE_RELEASE(MeshVS);
  SAFE_RELEASE(MeshGS);
  SAFE_RELEASE(MeshPS);

  SAFE_DELETE_ARRAY(MeshVSBytecode);

  SAFE_RELEASE(WireframeRasterState);
  SAFE_RELEASE(WireframeHelpersBS);
  SAFE_RELEASE(SolidRasterState);

  SAFE_RELEASE(FrustumHelper);
  SAFE_RELEASE(AxisHelper);
  SAFE_RELEASE(TriHighlightHelper);
}

void D3D11Replay::VertexPicking::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  HRESULT hr = S_OK;

  std::string meshhlsl = GetEmbeddedResource(mesh_hlsl);

  MeshPickCS = shaderCache->MakeCShader(meshhlsl.c_str(), "RENDERDOC_MeshPickCS", "cs_5_0");

  D3D11_BUFFER_DESC bDesc;

  bDesc.ByteWidth = sizeof(Vec4f) * MaxMeshPicks;
  bDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
  bDesc.CPUAccessFlags = 0;
  bDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
  bDesc.StructureByteStride = sizeof(Vec4f);
  bDesc.Usage = D3D11_USAGE_DEFAULT;

  hr = device->CreateBuffer(&bDesc, NULL, &PickResultBuf);

  if(FAILED(hr))
    RDCERR("Failed to create mesh pick result buff HRESULT: %s", ToStr(hr).c_str());

  D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;

  uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  uavDesc.Format = DXGI_FORMAT_UNKNOWN;
  uavDesc.Buffer.FirstElement = 0;
  uavDesc.Buffer.NumElements = MaxMeshPicks;
  uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_APPEND;

  hr = device->CreateUnorderedAccessView(PickResultBuf, &uavDesc, &PickResultUAV);

  if(FAILED(hr))
    RDCERR("Failed to create mesh pick result UAV HRESULT: %s", ToStr(hr).c_str());

  // created/sized on demand
  PickIBBuf = PickVBBuf = NULL;
  PickIBSRV = PickVBSRV = NULL;
  PickIBSize = PickVBSize = 0;
}

void D3D11Replay::VertexPicking::Release()
{
  SAFE_RELEASE(MeshPickCS);
  SAFE_RELEASE(PickIBBuf);
  SAFE_RELEASE(PickVBBuf);
  SAFE_RELEASE(PickIBSRV);
  SAFE_RELEASE(PickVBSRV);
  SAFE_RELEASE(PickResultBuf);
  SAFE_RELEASE(PickResultUAV);
}

void D3D11Replay::PixelPicking::Init(WrappedID3D11Device *device)
{
  HRESULT hr = S_OK;

  {
    D3D11_TEXTURE2D_DESC desc;

    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
    desc.Width = 100;
    desc.Height = 100;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.CPUAccessFlags = 0;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = 0;

    hr = device->CreateTexture2D(&desc, NULL, &Texture);

    if(FAILED(hr))
    {
      RDCERR("Failed to create pick tex HRESULT: %s", ToStr(hr).c_str());
    }

    if(Texture)
    {
      hr = device->CreateRenderTargetView(Texture, NULL, &RTV);

      if(FAILED(hr))
      {
        RDCERR("Failed to create pick rt HRESULT: %s", ToStr(hr).c_str());
      }
    }
  }

  {
    D3D11_TEXTURE2D_DESC desc;
    RDCEraseEl(desc);
    desc.ArraySize = 1;
    desc.MipLevels = 1;
    desc.Width = 1;
    desc.Height = 1;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

    hr = device->CreateTexture2D(&desc, NULL, &StageTexture);

    if(FAILED(hr))
    {
      RDCERR("Failed to create pick stage tex HRESULT: %s", ToStr(hr).c_str());
    }
  }
}

void D3D11Replay::PixelPicking::Release()
{
  SAFE_RELEASE(Texture);
  SAFE_RELEASE(RTV);
  SAFE_RELEASE(StageTexture);
}

void D3D11Replay::HistogramMinMax::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  HRESULT hr = S_OK;

  const uint32_t maxTexDim = 16384;
  const uint32_t blockPixSize = HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK;
  const uint32_t maxBlocksNeeded = (maxTexDim * maxTexDim) / (blockPixSize * blockPixSize);

  D3D11_BUFFER_DESC bDesc;

  bDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
  bDesc.ByteWidth =
      2 * 4 * sizeof(float) * HGRAM_TILES_PER_BLOCK * HGRAM_TILES_PER_BLOCK * maxBlocksNeeded;
  bDesc.CPUAccessFlags = 0;
  bDesc.MiscFlags = 0;
  bDesc.StructureByteStride = 0;
  bDesc.Usage = D3D11_USAGE_DEFAULT;

  hr = device->CreateBuffer(&bDesc, NULL, &TileResultBuff);

  if(FAILED(hr))
  {
    RDCERR("Failed to create tile result buffer HRESULT: %s", ToStr(hr).c_str());
  }

  D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
  srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
  srvDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  srvDesc.Buffer.FirstElement = 0;
  srvDesc.Buffer.NumElements = bDesc.ByteWidth / sizeof(Vec4f);

  hr = device->CreateShaderResourceView(TileResultBuff, &srvDesc, &TileResultSRV[0]);

  if(FAILED(hr))
    RDCERR("Failed to create tile result SRV 0 HRESULT: %s", ToStr(hr).c_str());

  srvDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
  hr = device->CreateShaderResourceView(TileResultBuff, &srvDesc, &TileResultSRV[1]);

  if(FAILED(hr))
    RDCERR("Failed to create tile result SRV 1 HRESULT: %s", ToStr(hr).c_str());

  srvDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
  hr = device->CreateShaderResourceView(TileResultBuff, &srvDesc, &TileResultSRV[2]);

  if(FAILED(hr))
    RDCERR("Failed to create tile result SRV 2 HRESULT: %s", ToStr(hr).c_str());

  D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;

  uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
  uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  uavDesc.Buffer.FirstElement = 0;
  uavDesc.Buffer.Flags = 0;
  uavDesc.Buffer.NumElements = srvDesc.Buffer.NumElements;

  hr = device->CreateUnorderedAccessView(TileResultBuff, &uavDesc, &TileResultUAV[0]);

  if(FAILED(hr))
    RDCERR("Failed to create tile result UAV 0 HRESULT: %s", ToStr(hr).c_str());

  uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
  hr = device->CreateUnorderedAccessView(TileResultBuff, &uavDesc, &TileResultUAV[1]);

  if(FAILED(hr))
    RDCERR("Failed to create tile result UAV 1 HRESULT: %s", ToStr(hr).c_str());

  uavDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
  hr = device->CreateUnorderedAccessView(TileResultBuff, &uavDesc, &TileResultUAV[2]);

  if(FAILED(hr))
    RDCERR("Failed to create tile result UAV 2 HRESULT: %s", ToStr(hr).c_str());

  uavDesc.Format = DXGI_FORMAT_R32_UINT;
  uavDesc.Buffer.NumElements = HGRAM_NUM_BUCKETS;
  bDesc.ByteWidth = uavDesc.Buffer.NumElements * sizeof(int);

  hr = device->CreateBuffer(&bDesc, NULL, &ResultBuff);

  if(FAILED(hr))
    RDCERR("Failed to create histogram buff HRESULT: %s", ToStr(hr).c_str());

  hr = device->CreateUnorderedAccessView(ResultBuff, &uavDesc, &HistogramUAV);

  if(FAILED(hr))
    RDCERR("Failed to create histogram UAV HRESULT: %s", ToStr(hr).c_str());

  bDesc.BindFlags = 0;
  bDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  bDesc.Usage = D3D11_USAGE_STAGING;

  hr = device->CreateBuffer(&bDesc, NULL, &ResultStageBuff);

  if(FAILED(hr))
    RDCERR("Failed to create histogram stage buff HRESULT: %s", ToStr(hr).c_str());

  uavDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
  uavDesc.Buffer.NumElements = 2;

  hr = device->CreateUnorderedAccessView(ResultBuff, &uavDesc, &ResultUAV[0]);

  if(FAILED(hr))
    RDCERR("Failed to create result UAV 0 HRESULT: %s", ToStr(hr).c_str());

  uavDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
  hr = device->CreateUnorderedAccessView(ResultBuff, &uavDesc, &ResultUAV[1]);

  if(FAILED(hr))
    RDCERR("Failed to create result UAV 1 HRESULT: %s", ToStr(hr).c_str());

  uavDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
  hr = device->CreateUnorderedAccessView(ResultBuff, &uavDesc, &ResultUAV[2]);

  if(FAILED(hr))
    RDCERR("Failed to create result UAV 2 HRESULT: %s", ToStr(hr).c_str());

  std::string histogramhlsl = GetEmbeddedResource(histogram_hlsl);

  for(int t = eTexType_1D; t < eTexType_Max; t++)
  {
    if(t == eTexType_Unused)
      continue;

    // float, uint, sint
    for(int i = 0; i < 3; i++)
    {
      std::string hlsl = std::string("#define SHADER_RESTYPE ") + ToStr(t) + "\n";
      hlsl += std::string("#define UINT_TEX ") + (i == 1 ? "1" : "0") + "\n";
      hlsl += std::string("#define SINT_TEX ") + (i == 2 ? "1" : "0") + "\n";
      hlsl += histogramhlsl;

      TileMinMaxCS[t][i] =
          shaderCache->MakeCShader(hlsl.c_str(), "RENDERDOC_TileMinMaxCS", "cs_5_0");
      HistogramCS[t][i] = shaderCache->MakeCShader(hlsl.c_str(), "RENDERDOC_HistogramCS", "cs_5_0");

      if(t == 1)
        ResultMinMaxCS[i] =
            shaderCache->MakeCShader(hlsl.c_str(), "RENDERDOC_ResultMinMaxCS", "cs_5_0");
    }
  }
}

void D3D11Replay::HistogramMinMax::Release()
{
  SAFE_RELEASE(TileResultBuff);
  SAFE_RELEASE(ResultBuff);
  SAFE_RELEASE(ResultStageBuff);

  for(int i = 0; i < 3; i++)
  {
    SAFE_RELEASE(TileResultUAV[i]);
    SAFE_RELEASE(ResultUAV[i]);
    SAFE_RELEASE(TileResultSRV[i]);
  }

  for(int i = 0; i < ARRAY_COUNT(TileMinMaxCS); i++)
  {
    for(int j = 0; j < 3; j++)
    {
      SAFE_RELEASE(TileMinMaxCS[i][j]);
      SAFE_RELEASE(HistogramCS[i][j]);

      if(i == 0)
        SAFE_RELEASE(ResultMinMaxCS[j]);
    }
  }

  SAFE_RELEASE(HistogramUAV);
}

void D3D11Replay::PixelHistory::Init(WrappedID3D11Device *device)
{
  D3D11ShaderCache *shaderCache = device->GetShaderCache();

  HRESULT hr = S_OK;

  {
    D3D11_BLEND_DESC blendDesc = {};

    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;

    hr = device->CreateBlendState(&blendDesc, &NopBlendState);
  }

  {
    D3D11_DEPTH_STENCIL_DESC desc;

    desc.BackFace.StencilFailOp = desc.BackFace.StencilPassOp = desc.BackFace.StencilDepthFailOp =
        D3D11_STENCIL_OP_KEEP;
    desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp =
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthEnable = FALSE;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.StencilReadMask = desc.StencilWriteMask = 0;
    desc.StencilEnable = FALSE;

    hr = device->CreateDepthStencilState(&desc, &NopDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create nop depthstencilstate HRESULT: %s", ToStr(hr).c_str());
    }

    desc.DepthEnable = TRUE;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.StencilEnable = TRUE;
    desc.StencilReadMask = desc.StencilWriteMask = 0xff;

    hr = device->CreateDepthStencilState(&desc, &AllPassDepthState);

    if(FAILED(hr))
      RDCERR("Failed to create always pass depthstencilstate HRESULT: %s", ToStr(hr).c_str());

    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    desc.DepthEnable = FALSE;
    desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    desc.BackFace.StencilFailOp = desc.BackFace.StencilPassOp = desc.BackFace.StencilDepthFailOp =
        D3D11_STENCIL_OP_INCR_SAT;
    desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
    desc.FrontFace.StencilFailOp = desc.FrontFace.StencilPassOp =
        desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR_SAT;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

    hr = device->CreateDepthStencilState(&desc, &AllPassIncrDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create always pass stencil increment depthstencilstate HRESULT: %s",
             ToStr(hr).c_str());
    }

    desc.DepthEnable = TRUE;
    desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    desc.BackFace.StencilFunc = D3D11_COMPARISON_EQUAL;
    desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;

    hr = device->CreateDepthStencilState(&desc, &StencIncrEqDepthState);

    if(FAILED(hr))
    {
      RDCERR("Failed to create always pass stencil increment depthstencilstate HRESULT: %s",
             ToStr(hr).c_str());
    }
  }

  {
    std::string hlsl = GetEmbeddedResource(pixelhistory_hlsl);

    PrimitiveIDPS = shaderCache->MakePShader(hlsl.c_str(), "RENDERDOC_PrimitiveIDPS", "ps_5_0");
  }
}

void D3D11Replay::PixelHistory::Release()
{
  SAFE_RELEASE(NopBlendState);
  SAFE_RELEASE(NopDepthState);
  SAFE_RELEASE(AllPassDepthState);
  SAFE_RELEASE(AllPassIncrDepthState);
  SAFE_RELEASE(StencIncrEqDepthState);
  SAFE_RELEASE(PrimitiveIDPS);
}
