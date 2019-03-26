/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

#include "d3d11_video.h"
#include "d3d11_context.h"
#include "d3d11_device.h"
#include "d3d11_resources.h"

WRAPPED_POOL_INST(WrappedID3D11VideoDecoderOutputView);
WRAPPED_POOL_INST(WrappedID3D11VideoProcessorInputView);
WRAPPED_POOL_INST(WrappedID3D11VideoProcessorOutputView);

ID3D11Resource *UnwrapD3D11Resource(ID3D11Resource *dxObject)
{
  if(WrappedID3D11Buffer::IsAlloc(dxObject))
  {
    WrappedID3D11Buffer *w = (WrappedID3D11Buffer *)dxObject;
    return w->GetReal();
  }
  else if(WrappedID3D11Texture1D::IsAlloc(dxObject))
  {
    WrappedID3D11Texture1D *w = (WrappedID3D11Texture1D *)dxObject;
    return w->GetReal();
  }
  else if(WrappedID3D11Texture2D1::IsAlloc(dxObject))
  {
    WrappedID3D11Texture2D1 *w = (WrappedID3D11Texture2D1 *)dxObject;
    return w->GetReal();
  }
  else if(WrappedID3D11Texture3D1::IsAlloc(dxObject))
  {
    WrappedID3D11Texture3D1 *w = (WrappedID3D11Texture3D1 *)dxObject;
    return w->GetReal();
  }

  return NULL;
}

ULONG STDMETHODCALLTYPE WrappedID3D11VideoDevice2::AddRef()
{
  return m_pDevice->AddRef();
}

ULONG STDMETHODCALLTYPE WrappedID3D11VideoDevice2::Release()
{
  return m_pDevice->Release();
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D11VideoDevice))
  {
    *ppvObject = (ID3D11VideoDevice *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D11VideoDevice1))
  {
    if(m_pReal1)
    {
      *ppvObject = (ID3D11VideoDevice1 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D11VideoDevice2))
  {
    if(m_pReal2)
    {
      *ppvObject = (ID3D11VideoDevice2 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return E_NOINTERFACE;
    }
  }

  return m_pDevice->QueryInterface(riid, ppvObject);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::CreateVideoDecoder(
    /* [annotation] */ _In_ const D3D11_VIDEO_DECODER_DESC *pVideoDesc,
    /* [annotation] */ _In_ const D3D11_VIDEO_DECODER_CONFIG *pConfig,
    /* [annotation] */ _COM_Outptr_ ID3D11VideoDecoder **ppDecoder)
{
  if(ppDecoder == NULL)
    return m_pReal->CreateVideoDecoder(pVideoDesc, pConfig, NULL);

  ID3D11VideoDecoder *real = NULL;

  HRESULT hr = m_pReal->CreateVideoDecoder(pVideoDesc, pConfig, &real);

  if(SUCCEEDED(hr))
  {
    *ppDecoder = new WrappedID3D11VideoDecoder(real, m_pDevice);
  }
  else
  {
    SAFE_RELEASE(real);
  }

  return hr;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::CreateVideoProcessor(
    /* [annotation] */ _In_ ID3D11VideoProcessorEnumerator *pEnum,
    /* [annotation] */ _In_ UINT RateConversionIndex,
    /* [annotation] */ _COM_Outptr_ ID3D11VideoProcessor **ppVideoProcessor)
{
  if(ppVideoProcessor == NULL)
    return m_pReal->CreateVideoProcessor(
        VIDEO_UNWRAP(WrappedID3D11VideoProcessorEnumerator1, pEnum), RateConversionIndex, NULL);

  ID3D11VideoProcessor *real = NULL;

  HRESULT hr = m_pReal->CreateVideoProcessor(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessorEnumerator1, pEnum), RateConversionIndex, &real);

  if(SUCCEEDED(hr))
  {
    *ppVideoProcessor = new WrappedID3D11VideoProcessor(real, m_pDevice);
  }
  else
  {
    SAFE_RELEASE(real);
  }

  return hr;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::CreateAuthenticatedChannel(
    /* [annotation] */ _In_ D3D11_AUTHENTICATED_CHANNEL_TYPE ChannelType,
    /* [annotation] */ _COM_Outptr_ ID3D11AuthenticatedChannel **ppAuthenticatedChannel)
{
  if(ppAuthenticatedChannel == NULL)
    return m_pReal->CreateAuthenticatedChannel(ChannelType, NULL);

  ID3D11AuthenticatedChannel *real = NULL;

  HRESULT hr = m_pReal->CreateAuthenticatedChannel(ChannelType, &real);

  if(SUCCEEDED(hr))
  {
    *ppAuthenticatedChannel = new WrappedID3D11AuthenticatedChannel(real, m_pDevice);
  }
  else
  {
    SAFE_RELEASE(real);
  }

  return hr;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::CreateCryptoSession(
    /* [annotation] */ _In_ const GUID *pCryptoType,
    /* [annotation] */ _In_opt_ const GUID *pDecoderProfile,
    /* [annotation] */ _In_ const GUID *pKeyExchangeType,
    /* [annotation] */ _COM_Outptr_ ID3D11CryptoSession **ppCryptoSession)
{
  if(ppCryptoSession == NULL)
    return m_pReal->CreateCryptoSession(pCryptoType, pDecoderProfile, pKeyExchangeType, NULL);

  ID3D11CryptoSession *real = NULL;

  HRESULT hr = m_pReal->CreateCryptoSession(pCryptoType, pDecoderProfile, pKeyExchangeType, &real);

  if(SUCCEEDED(hr))
  {
    *ppCryptoSession = new WrappedID3D11CryptoSession(real, m_pDevice);
  }
  else
  {
    SAFE_RELEASE(real);
  }

  return hr;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::CreateVideoDecoderOutputView(
    /* [annotation] */ _In_ ID3D11Resource *pResource,
    /* [annotation] */ _In_ const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC *pDesc,
    /* [annotation] */ _COM_Outptr_opt_ ID3D11VideoDecoderOutputView **ppVDOVView)
{
  if(ppVDOVView == NULL)
    return m_pReal->CreateVideoDecoderOutputView(UnwrapD3D11Resource(pResource), pDesc, NULL);

  ID3D11VideoDecoderOutputView *real = NULL;

  HRESULT hr = m_pReal->CreateVideoDecoderOutputView(UnwrapD3D11Resource(pResource), pDesc, &real);

  if(SUCCEEDED(hr))
  {
    *ppVDOVView = new WrappedID3D11VideoDecoderOutputView(real, m_pDevice);

    m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(pResource));
  }
  else
  {
    SAFE_RELEASE(real);
  }

  return hr;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::CreateVideoProcessorInputView(
    /* [annotation] */ _In_ ID3D11Resource *pResource,
    /* [annotation] */ _In_ ID3D11VideoProcessorEnumerator *pEnum,
    /* [annotation] */ _In_ const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC *pDesc,
    /* [annotation] */ _COM_Outptr_opt_ ID3D11VideoProcessorInputView **ppVPIView)
{
  if(ppVPIView == NULL)
    return m_pReal->CreateVideoProcessorInputView(
        UnwrapD3D11Resource(pResource), VIDEO_UNWRAP(WrappedID3D11VideoProcessorEnumerator1, pEnum),
        pDesc, NULL);

  ID3D11VideoProcessorInputView *real = NULL;

  HRESULT hr = m_pReal->CreateVideoProcessorInputView(
      UnwrapD3D11Resource(pResource), VIDEO_UNWRAP(WrappedID3D11VideoProcessorEnumerator1, pEnum),
      pDesc, &real);

  if(SUCCEEDED(hr))
  {
    *ppVPIView = new WrappedID3D11VideoProcessorInputView(real, m_pDevice);

    m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(pResource));
  }
  else
  {
    SAFE_RELEASE(real);
  }

  return hr;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::CreateVideoProcessorOutputView(
    /* [annotation] */ _In_ ID3D11Resource *pResource,
    /* [annotation] */ _In_ ID3D11VideoProcessorEnumerator *pEnum,
    /* [annotation] */ _In_ const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC *pDesc,
    /* [annotation] */ _COM_Outptr_opt_ ID3D11VideoProcessorOutputView **ppVPOView)
{
  if(ppVPOView == NULL)
    return m_pReal->CreateVideoProcessorOutputView(
        UnwrapD3D11Resource(pResource), VIDEO_UNWRAP(WrappedID3D11VideoProcessorEnumerator1, pEnum),
        pDesc, NULL);

  ID3D11VideoProcessorOutputView *real = NULL;

  HRESULT hr = m_pReal->CreateVideoProcessorOutputView(
      UnwrapD3D11Resource(pResource), VIDEO_UNWRAP(WrappedID3D11VideoProcessorEnumerator1, pEnum),
      pDesc, &real);

  if(SUCCEEDED(hr))
  {
    *ppVPOView = new WrappedID3D11VideoProcessorOutputView(real, m_pDevice);

    m_pDevice->GetResourceManager()->MarkDirtyResource(GetIDForResource(pResource));
  }
  else
  {
    SAFE_RELEASE(real);
  }

  return hr;
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::CreateVideoProcessorEnumerator(
    /* [annotation] */ _In_ const D3D11_VIDEO_PROCESSOR_CONTENT_DESC *pDesc,
    /* [annotation] */ _COM_Outptr_ ID3D11VideoProcessorEnumerator **ppEnum)
{
  if(ppEnum == NULL)
    return m_pReal->CreateVideoProcessorEnumerator(pDesc, NULL);

  ID3D11VideoProcessorEnumerator *real = NULL;

  HRESULT hr = m_pReal->CreateVideoProcessorEnumerator(pDesc, &real);

  if(SUCCEEDED(hr))
  {
    *ppEnum = new WrappedID3D11VideoProcessorEnumerator1(real, m_pDevice);
  }
  else
  {
    SAFE_RELEASE(real);
  }

  return hr;
}

UINT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::GetVideoDecoderProfileCount(void)
{
  return m_pReal->GetVideoDecoderProfileCount();
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::GetVideoDecoderProfile(
    /* [annotation] */ _In_ UINT Index, /* [annotation] */ _Out_ GUID *pDecoderProfile)
{
  return m_pReal->GetVideoDecoderProfile(Index, pDecoderProfile);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::CheckVideoDecoderFormat(
    /* [annotation] */ _In_ const GUID *pDecoderProfile, /* [annotation] */ _In_ DXGI_FORMAT Format,
    /* [annotation] */ _Out_ BOOL *pSupported)
{
  return m_pReal->CheckVideoDecoderFormat(pDecoderProfile, Format, pSupported);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::GetVideoDecoderConfigCount(
    /* [annotation] */ _In_ const D3D11_VIDEO_DECODER_DESC *pDesc,
    /* [annotation] */ _Out_ UINT *pCount)
{
  return m_pReal->GetVideoDecoderConfigCount(pDesc, pCount);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::GetVideoDecoderConfig(
    /* [annotation] */ _In_ const D3D11_VIDEO_DECODER_DESC *pDesc,
    /* [annotation] */ _In_ UINT Index, /* [annotation] */ _Out_ D3D11_VIDEO_DECODER_CONFIG *pConfig)
{
  return m_pReal->GetVideoDecoderConfig(pDesc, Index, pConfig);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::GetContentProtectionCaps(
    /* [annotation] */ _In_opt_ const GUID *pCryptoType,
    /* [annotation] */ _In_opt_ const GUID *pDecoderProfile,
    /* [annotation] */ _Out_ D3D11_VIDEO_CONTENT_PROTECTION_CAPS *pCaps)
{
  return m_pReal->GetContentProtectionCaps(pCryptoType, pDecoderProfile, pCaps);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::CheckCryptoKeyExchange(
    /* [annotation] */ _In_ const GUID *pCryptoType,
    /* [annotation] */ _In_opt_ const GUID *pDecoderProfile, /* [annotation] */ _In_ UINT Index,
    /* [annotation] */ _Out_ GUID *pKeyExchangeType)
{
  return m_pReal->CheckCryptoKeyExchange(pCryptoType, pDecoderProfile, Index, pKeyExchangeType);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::GetCryptoSessionPrivateDataSize(
    /* [annotation] */ _In_ const GUID *pCryptoType,
    /* [annotation] */ _In_opt_ const GUID *pDecoderProfile,
    /* [annotation] */ _In_ const GUID *pKeyExchangeType,
    /* [annotation] */ _Out_ UINT *pPrivateInputSize,
    /* [annotation] */ _Out_ UINT *pPrivateOutputSize)
{
  if(!m_pReal1)
    return E_NOINTERFACE;
  return m_pReal1->GetCryptoSessionPrivateDataSize(pCryptoType, pDecoderProfile, pKeyExchangeType,
                                                   pPrivateInputSize, pPrivateOutputSize);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::GetVideoDecoderCaps(
    /* [annotation] */ _In_ const GUID *pDecoderProfile, /* [annotation] */ _In_ UINT SampleWidth,
    /* [annotation] */ _In_ UINT SampleHeight,
    /* [annotation] */ _In_ const DXGI_RATIONAL *pFrameRate, /* [annotation] */ _In_ UINT BitRate,
    /* [annotation] */ _In_opt_ const GUID *pCryptoType, /* [annotation] */ _Out_ UINT *pDecoderCaps)
{
  if(!m_pReal1)
    return E_NOINTERFACE;
  return m_pReal1->GetVideoDecoderCaps(pDecoderProfile, SampleWidth, SampleHeight, pFrameRate,
                                       BitRate, pCryptoType, pDecoderCaps);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::CheckVideoDecoderDownsampling(
    /* [annotation] */ _In_ const D3D11_VIDEO_DECODER_DESC *pInputDesc,
    /* [annotation] */ _In_ DXGI_COLOR_SPACE_TYPE InputColorSpace,
    /* [annotation] */ _In_ const D3D11_VIDEO_DECODER_CONFIG *pInputConfig,
    /* [annotation] */ _In_ const DXGI_RATIONAL *pFrameRate,
    /* [annotation] */ _In_ const D3D11_VIDEO_SAMPLE_DESC *pOutputDesc,
    /* [annotation] */ _Out_ BOOL *pSupported, /* [annotation] */ _Out_ BOOL *pRealTimeHint)
{
  if(!m_pReal1)
    return E_NOINTERFACE;
  return m_pReal1->CheckVideoDecoderDownsampling(pInputDesc, InputColorSpace, pInputConfig,
                                                 pFrameRate, pOutputDesc, pSupported, pRealTimeHint);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::RecommendVideoDecoderDownsampleParameters(
    /* [annotation] */ _In_ const D3D11_VIDEO_DECODER_DESC *pInputDesc,
    /* [annotation] */ _In_ DXGI_COLOR_SPACE_TYPE InputColorSpace,
    /* [annotation] */ _In_ const D3D11_VIDEO_DECODER_CONFIG *pInputConfig,
    /* [annotation] */ _In_ const DXGI_RATIONAL *pFrameRate,
    /* [annotation] */ _Out_ D3D11_VIDEO_SAMPLE_DESC *pRecommendedOutputDesc)
{
  if(!m_pReal1)
    return E_NOINTERFACE;
  return m_pReal1->RecommendVideoDecoderDownsampleParameters(
      pInputDesc, InputColorSpace, pInputConfig, pFrameRate, pRecommendedOutputDesc);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::CheckFeatureSupport(
    D3D11_FEATURE_VIDEO Feature,
    /* [annotation] */ _Out_writes_bytes_(FeatureSupportDataSize) void *pFeatureSupportData,
    UINT FeatureSupportDataSize)
{
  if(!m_pReal2)
    return E_NOINTERFACE;
  return m_pReal2->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoDevice2::NegotiateCryptoSessionKeyExchangeMT(
    /* [annotation] */ _In_ ID3D11CryptoSession *pCryptoSession,
    /* [annotation] */ _In_ D3D11_CRYPTO_SESSION_KEY_EXCHANGE_FLAGS flags,
    /* [annotation] */ _In_ UINT DataSize,
    /* [annotation] */ _Inout_updates_bytes_(DataSize) void *pData)
{
  if(!m_pReal2)
    return E_NOINTERFACE;
  return m_pReal2->NegotiateCryptoSessionKeyExchangeMT(
      VIDEO_UNWRAP(WrappedID3D11CryptoSession, pCryptoSession), flags, DataSize, pData);
}

ULONG STDMETHODCALLTYPE WrappedID3D11VideoContext2::AddRef()
{
  return m_pContext->AddRef();
}

ULONG STDMETHODCALLTYPE WrappedID3D11VideoContext2::Release()
{
  return m_pContext->Release();
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D11VideoContext))
  {
    *ppvObject = (ID3D11VideoContext *)this;
    AddRef();
    return S_OK;
  }
  else if(riid == __uuidof(ID3D11VideoContext1))
  {
    if(m_pReal1)
    {
      *ppvObject = (ID3D11VideoContext1 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return E_NOINTERFACE;
    }
  }
  else if(riid == __uuidof(ID3D11VideoContext2))
  {
    if(m_pReal2)
    {
      *ppvObject = (ID3D11VideoContext2 *)this;
      AddRef();
      return S_OK;
    }
    else
    {
      *ppvObject = NULL;
      return E_NOINTERFACE;
    }
  }

  return m_pContext->QueryInterface(riid, ppvObject);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::GetDevice(ID3D11Device **ppDevice)
{
  m_pContext->GetDevice(ppDevice);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::GetDecoderBuffer(
    /* [annotation] */ _In_ ID3D11VideoDecoder *pDecoder, D3D11_VIDEO_DECODER_BUFFER_TYPE Type,
    /* [annotation] */ _Out_ UINT *pBufferSize,
    /* [annotation] */ _Outptr_result_bytebuffer_(*pBufferSize) void **ppBuffer)
{
  return m_pReal->GetDecoderBuffer(VIDEO_UNWRAP(WrappedID3D11VideoDecoder, pDecoder), Type,
                                   pBufferSize, ppBuffer);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::ReleaseDecoderBuffer(
    /* [annotation] */ _In_ ID3D11VideoDecoder *pDecoder,
    /* [annotation] */ _In_ D3D11_VIDEO_DECODER_BUFFER_TYPE Type)
{
  return m_pReal->ReleaseDecoderBuffer(VIDEO_UNWRAP(WrappedID3D11VideoDecoder, pDecoder), Type);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::DecoderBeginFrame(
    /* [annotation] */ _In_ ID3D11VideoDecoder *pDecoder,
    /* [annotation] */ _In_ ID3D11VideoDecoderOutputView *pView, UINT ContentKeySize,
    /* [annotation] */ _In_reads_bytes_opt_(ContentKeySize) const void *pContentKey)
{
  return m_pReal->DecoderBeginFrame(VIDEO_UNWRAP(WrappedID3D11VideoDecoder, pDecoder),
                                    VIDEO_UNWRAP(WrappedID3D11VideoDecoderOutputView, pView),
                                    ContentKeySize, pContentKey);
}

HRESULT STDMETHODCALLTYPE
WrappedID3D11VideoContext2::DecoderEndFrame(/* [annotation] */ _In_ ID3D11VideoDecoder *pDecoder)
{
  return m_pReal->DecoderEndFrame(VIDEO_UNWRAP(WrappedID3D11VideoDecoder, pDecoder));
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::SubmitDecoderBuffers(
    /* [annotation] */ _In_ ID3D11VideoDecoder *pDecoder, /* [annotation] */ _In_ UINT NumBuffers,
    /* [annotation] */ _In_reads_(NumBuffers) const D3D11_VIDEO_DECODER_BUFFER_DESC *pBufferDesc)
{
  return m_pReal->SubmitDecoderBuffers(VIDEO_UNWRAP(WrappedID3D11VideoDecoder, pDecoder),
                                       NumBuffers, pBufferDesc);
}

APP_DEPRECATED_HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::DecoderExtension(
    /* [annotation] */ _In_ ID3D11VideoDecoder *pDecoder,
    /* [annotation] */ _In_ const D3D11_VIDEO_DECODER_EXTENSION *pExtensionData)
{
  if(pExtensionData == NULL)
    return m_pReal->DecoderExtension(VIDEO_UNWRAP(WrappedID3D11VideoDecoder, pDecoder),
                                     pExtensionData);

  D3D11_VIDEO_DECODER_EXTENSION unwrappedExt = *pExtensionData;

  std::vector<ID3D11Resource *> unwrappedRes;

  unwrappedRes.resize(unwrappedExt.ResourceCount);
  for(UINT i = 0; i < unwrappedExt.ResourceCount; i++)
    unwrappedRes[i] = UnwrapD3D11Resource(unwrappedExt.ppResourceList[i]);

  unwrappedExt.ppResourceList = unwrappedRes.data();

  return m_pReal->DecoderExtension(VIDEO_UNWRAP(WrappedID3D11VideoDecoder, pDecoder), &unwrappedExt);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetOutputTargetRect(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ BOOL Enable, /* [annotation] */ _In_opt_ const RECT *pRect)
{
  return m_pReal->VideoProcessorSetOutputTargetRect(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), Enable, pRect);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetOutputBackgroundColor(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ BOOL YCbCr, /* [annotation] */ _In_ const D3D11_VIDEO_COLOR *pColor)
{
  return m_pReal->VideoProcessorSetOutputBackgroundColor(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), YCbCr, pColor);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetOutputColorSpace(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace)
{
  return m_pReal->VideoProcessorSetOutputColorSpace(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), pColorSpace);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetOutputAlphaFillMode(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE AlphaFillMode,
    /* [annotation] */ _In_ UINT StreamIndex)
{
  return m_pReal->VideoProcessorSetOutputAlphaFillMode(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), AlphaFillMode, StreamIndex);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetOutputConstriction(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ BOOL Enable, /* [annotation] */ _In_ SIZE Size)
{
  return m_pReal->VideoProcessorSetOutputConstriction(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), Enable, Size);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetOutputStereoMode(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor, /* [annotation] */ _In_ BOOL Enable)
{
  return m_pReal->VideoProcessorSetOutputStereoMode(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), Enable);
}

APP_DEPRECATED_HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetOutputExtension(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ const GUID *pExtensionGuid, /* [annotation] */ _In_ UINT DataSize,
    /* [annotation] */ _In_ void *pData)
{
  return m_pReal->VideoProcessorSetOutputExtension(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), pExtensionGuid, DataSize, pData);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetOutputTargetRect(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _Out_ BOOL *Enabled, /* [annotation] */ _Out_ RECT *pRect)
{
  return m_pReal->VideoProcessorGetOutputTargetRect(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), Enabled, pRect);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetOutputBackgroundColor(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _Out_ BOOL *pYCbCr, /* [annotation] */ _Out_ D3D11_VIDEO_COLOR *pColor)
{
  return m_pReal->VideoProcessorGetOutputBackgroundColor(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), pYCbCr, pColor);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetOutputColorSpace(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _Out_ D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace)
{
  return m_pReal->VideoProcessorGetOutputColorSpace(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), pColorSpace);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetOutputAlphaFillMode(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _Out_ D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE *pAlphaFillMode,
    /* [annotation] */ _Out_ UINT *pStreamIndex)
{
  return m_pReal->VideoProcessorGetOutputAlphaFillMode(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), pAlphaFillMode, pStreamIndex);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetOutputConstriction(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _Out_ BOOL *pEnabled, /* [annotation] */ _Out_ SIZE *pSize)
{
  return m_pReal->VideoProcessorGetOutputConstriction(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), pEnabled, pSize);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetOutputStereoMode(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _Out_ BOOL *pEnabled)
{
  return m_pReal->VideoProcessorGetOutputStereoMode(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), pEnabled);
}

APP_DEPRECATED_HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetOutputExtension(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ const GUID *pExtensionGuid, /* [annotation] */ _In_ UINT DataSize,
    /* [annotation] */ _Out_writes_bytes_(DataSize) void *pData)
{
  return m_pReal->VideoProcessorGetOutputExtension(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), pExtensionGuid, DataSize, pData);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamFrameFormat(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex,
    /* [annotation] */ _In_ D3D11_VIDEO_FRAME_FORMAT FrameFormat)
{
  return m_pReal->VideoProcessorSetStreamFrameFormat(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, FrameFormat);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamColorSpace(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex,
    /* [annotation] */ _In_ const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace)
{
  return m_pReal->VideoProcessorSetStreamColorSpace(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pColorSpace);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamOutputRate(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex,
    /* [annotation] */ _In_ D3D11_VIDEO_PROCESSOR_OUTPUT_RATE OutputRate,
    /* [annotation] */ _In_ BOOL RepeatFrame,
    /* [annotation] */ _In_opt_ const DXGI_RATIONAL *pCustomRate)
{
  return m_pReal->VideoProcessorSetStreamOutputRate(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, OutputRate,
      RepeatFrame, pCustomRate);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamSourceRect(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ BOOL Enable,
    /* [annotation] */ _In_opt_ const RECT *pRect)
{
  return m_pReal->VideoProcessorSetStreamSourceRect(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Enable, pRect);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamDestRect(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ BOOL Enable,
    /* [annotation] */ _In_opt_ const RECT *pRect)
{
  return m_pReal->VideoProcessorSetStreamDestRect(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Enable, pRect);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamAlpha(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ BOOL Enable,
    /* [annotation] */ _In_ FLOAT Alpha)
{
  return m_pReal->VideoProcessorSetStreamAlpha(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Enable, Alpha);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamPalette(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ UINT Count,
    /* [annotation] */ _In_reads_opt_(Count) const UINT *pEntries)
{
  return m_pReal->VideoProcessorSetStreamPalette(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Count, pEntries);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamPixelAspectRatio(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ BOOL Enable,
    /* [annotation] */ _In_opt_ const DXGI_RATIONAL *pSourceAspectRatio,
    /* [annotation] */ _In_opt_ const DXGI_RATIONAL *pDestinationAspectRatio)
{
  return m_pReal->VideoProcessorSetStreamPixelAspectRatio(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Enable,
      pSourceAspectRatio, pDestinationAspectRatio);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamLumaKey(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ BOOL Enable,
    /* [annotation] */ _In_ FLOAT Lower, /* [annotation] */ _In_ FLOAT Upper)
{
  return m_pReal->VideoProcessorSetStreamLumaKey(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Enable, Lower, Upper);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamStereoFormat(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ BOOL Enable,
    /* [annotation] */ _In_ D3D11_VIDEO_PROCESSOR_STEREO_FORMAT Format,
    /* [annotation] */ _In_ BOOL LeftViewFrame0, /* [annotation] */ _In_ BOOL BaseViewFrame0,
    /* [annotation] */ _In_ D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE FlipMode,
    /* [annotation] */ _In_ int MonoOffset)
{
  return m_pReal->VideoProcessorSetStreamStereoFormat(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Enable, Format,
      LeftViewFrame0, BaseViewFrame0, FlipMode, MonoOffset);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamAutoProcessingMode(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ BOOL Enable)
{
  return m_pReal->VideoProcessorSetStreamAutoProcessingMode(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Enable);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamFilter(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex,
    /* [annotation] */ _In_ D3D11_VIDEO_PROCESSOR_FILTER Filter,
    /* [annotation] */ _In_ BOOL Enable, /* [annotation] */ _In_ int Level)
{
  return m_pReal->VideoProcessorSetStreamFilter(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Filter, Enable, Level);
}

APP_DEPRECATED_HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamExtension(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ const GUID *pExtensionGuid,
    /* [annotation] */ _In_ UINT DataSize, /* [annotation] */ _In_ void *pData)
{
  return m_pReal->VideoProcessorSetStreamExtension(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pExtensionGuid,
      DataSize, pData);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamFrameFormat(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex,
    /* [annotation] */ _Out_ D3D11_VIDEO_FRAME_FORMAT *pFrameFormat)
{
  return m_pReal->VideoProcessorGetStreamFrameFormat(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pFrameFormat);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamColorSpace(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex,
    /* [annotation] */ _Out_ D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace)
{
  return m_pReal->VideoProcessorGetStreamColorSpace(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pColorSpace);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamOutputRate(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex,
    /* [annotation] */ _Out_ D3D11_VIDEO_PROCESSOR_OUTPUT_RATE *pOutputRate,
    /* [annotation] */ _Out_ BOOL *pRepeatFrame, /* [annotation] */ _Out_ DXGI_RATIONAL *pCustomRate)
{
  return m_pReal->VideoProcessorGetStreamOutputRate(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pOutputRate,
      pRepeatFrame, pCustomRate);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamSourceRect(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _Out_ BOOL *pEnabled,
    /* [annotation] */ _Out_ RECT *pRect)
{
  return m_pReal->VideoProcessorGetStreamSourceRect(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pEnabled, pRect);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamDestRect(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _Out_ BOOL *pEnabled,
    /* [annotation] */ _Out_ RECT *pRect)
{
  return m_pReal->VideoProcessorGetStreamDestRect(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pEnabled, pRect);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamAlpha(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _Out_ BOOL *pEnabled,
    /* [annotation] */ _Out_ FLOAT *pAlpha)
{
  return m_pReal->VideoProcessorGetStreamAlpha(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pEnabled, pAlpha);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamPalette(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ UINT Count,
    /* [annotation] */ _Out_writes_(Count) UINT *pEntries)
{
  return m_pReal->VideoProcessorGetStreamPalette(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Count, pEntries);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamPixelAspectRatio(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _Out_ BOOL *pEnabled,
    /* [annotation] */ _Out_ DXGI_RATIONAL *pSourceAspectRatio,
    /* [annotation] */ _Out_ DXGI_RATIONAL *pDestinationAspectRatio)
{
  return m_pReal->VideoProcessorGetStreamPixelAspectRatio(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pEnabled,
      pSourceAspectRatio, pDestinationAspectRatio);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamLumaKey(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _Out_ BOOL *pEnabled,
    /* [annotation] */ _Out_ FLOAT *pLower, /* [annotation] */ _Out_ FLOAT *pUpper)
{
  return m_pReal->VideoProcessorGetStreamLumaKey(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pEnabled, pLower,
      pUpper);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamStereoFormat(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _Out_ BOOL *pEnable,
    /* [annotation] */ _Out_ D3D11_VIDEO_PROCESSOR_STEREO_FORMAT *pFormat,
    /* [annotation] */ _Out_ BOOL *pLeftViewFrame0, /* [annotation] */ _Out_ BOOL *pBaseViewFrame0,
    /* [annotation] */ _Out_ D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE *pFlipMode,
    /* [annotation] */ _Out_ int *MonoOffset)
{
  return m_pReal->VideoProcessorGetStreamStereoFormat(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pEnable, pFormat,
      pLeftViewFrame0, pBaseViewFrame0, pFlipMode, MonoOffset);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamAutoProcessingMode(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _Out_ BOOL *pEnabled)
{
  return m_pReal->VideoProcessorGetStreamAutoProcessingMode(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pEnabled);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamFilter(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex,
    /* [annotation] */ _In_ D3D11_VIDEO_PROCESSOR_FILTER Filter,
    /* [annotation] */ _Out_ BOOL *pEnabled, /* [annotation] */ _Out_ int *pLevel)
{
  return m_pReal->VideoProcessorGetStreamFilter(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Filter, pEnabled,
      pLevel);
}

APP_DEPRECATED_HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamExtension(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ const GUID *pExtensionGuid,
    /* [annotation] */ _In_ UINT DataSize,
    /* [annotation] */ _Out_writes_bytes_(DataSize) void *pData)
{
  return m_pReal->VideoProcessorGetStreamExtension(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pExtensionGuid,
      DataSize, pData);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorBlt(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ ID3D11VideoProcessorOutputView *pView,
    /* [annotation] */ _In_ UINT OutputFrame, /* [annotation] */ _In_ UINT StreamCount,
    /* [annotation] */ _In_reads_(StreamCount) const D3D11_VIDEO_PROCESSOR_STREAM *pStreams)
{
  std::vector<D3D11_VIDEO_PROCESSOR_STREAM> unwrappedStreams;
  unwrappedStreams.insert(unwrappedStreams.begin(), pStreams, pStreams + StreamCount);

  size_t numFrames = 0;

  for(D3D11_VIDEO_PROCESSOR_STREAM &stream : unwrappedStreams)
  {
    if(stream.ppPastSurfaces)
      numFrames += stream.PastFrames;
    if(stream.ppPastSurfacesRight)
      numFrames += stream.PastFrames;

    if(stream.ppFutureSurfaces)
      numFrames += stream.FutureFrames;
    if(stream.ppFutureSurfacesRight)
      numFrames += stream.FutureFrames;
  }

  std::vector<ID3D11VideoProcessorInputView *> inputViews;

  inputViews.resize(numFrames);

  size_t offs = 0;

  for(D3D11_VIDEO_PROCESSOR_STREAM &stream : unwrappedStreams)
  {
    stream.pInputSurface = VIDEO_UNWRAP(WrappedID3D11VideoProcessorInputView, stream.pInputSurface);
    stream.pInputSurfaceRight =
        VIDEO_UNWRAP(WrappedID3D11VideoProcessorInputView, stream.pInputSurfaceRight);

    if(stream.ppPastSurfaces)
    {
      for(UINT i = 0; i < stream.PastFrames; i++)
        inputViews[offs + i] =
            VIDEO_UNWRAP(WrappedID3D11VideoProcessorInputView, stream.ppPastSurfaces[i]);

      stream.ppPastSurfaces = &inputViews[offs];
      offs += stream.PastFrames;
    }

    if(stream.ppPastSurfacesRight)
    {
      for(UINT i = 0; i < stream.PastFrames; i++)
        inputViews[offs + i] =
            VIDEO_UNWRAP(WrappedID3D11VideoProcessorInputView, stream.ppPastSurfacesRight[i]);

      stream.ppPastSurfacesRight = &inputViews[offs];
      offs += stream.PastFrames;
    }

    if(stream.ppFutureSurfaces)
    {
      for(UINT i = 0; i < stream.FutureFrames; i++)
        inputViews[offs + i] =
            VIDEO_UNWRAP(WrappedID3D11VideoProcessorInputView, stream.ppFutureSurfaces[i]);

      stream.ppFutureSurfaces = &inputViews[offs];
      offs += stream.FutureFrames;
    }

    if(stream.ppFutureSurfacesRight)
    {
      for(UINT i = 0; i < stream.FutureFrames; i++)
        inputViews[offs + i] =
            VIDEO_UNWRAP(WrappedID3D11VideoProcessorInputView, stream.ppFutureSurfacesRight[i]);

      stream.ppFutureSurfacesRight = &inputViews[offs];
      offs += stream.FutureFrames;
    }
  }

  return m_pReal->VideoProcessorBlt(VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor),
                                    VIDEO_UNWRAP(WrappedID3D11VideoProcessorOutputView, pView),
                                    OutputFrame, StreamCount, unwrappedStreams.data());
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::NegotiateCryptoSessionKeyExchange(
    /* [annotation] */ _In_ ID3D11CryptoSession *pCryptoSession,
    /* [annotation] */ _In_ UINT DataSize,
    /* [annotation] */ _Inout_updates_bytes_(DataSize) void *pData)
{
  return m_pReal->NegotiateCryptoSessionKeyExchange(
      VIDEO_UNWRAP(WrappedID3D11CryptoSession, pCryptoSession), DataSize, pData);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::EncryptionBlt(
    /* [annotation] */ _In_ ID3D11CryptoSession *pCryptoSession,
    /* [annotation] */ _In_ ID3D11Texture2D *pSrcSurface,
    /* [annotation] */ _In_ ID3D11Texture2D *pDstSurface, /* [annotation] */ _In_ UINT IVSize,
    /* [annotation] */ _Inout_opt_bytecount_(IVSize) void *pIV)
{
  return m_pReal->EncryptionBlt(VIDEO_UNWRAP(WrappedID3D11CryptoSession, pCryptoSession),
                                UNWRAP(WrappedID3D11Texture2D1, pSrcSurface),
                                UNWRAP(WrappedID3D11Texture2D1, pDstSurface), IVSize, pIV);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::DecryptionBlt(
    /* [annotation] */ _In_ ID3D11CryptoSession *pCryptoSession,
    /* [annotation] */ _In_ ID3D11Texture2D *pSrcSurface,
    /* [annotation] */ _In_ ID3D11Texture2D *pDstSurface,
    /* [annotation] */ _In_opt_ D3D11_ENCRYPTED_BLOCK_INFO *pEncryptedBlockInfo,
    /* [annotation] */ _In_ UINT ContentKeySize,
    /* [annotation] */ _In_reads_bytes_opt_(ContentKeySize) const void *pContentKey,
    /* [annotation] */ _In_ UINT IVSize, /* [annotation] */ _Inout_opt_bytecount_(IVSize) void *pIV)
{
  return m_pReal->DecryptionBlt(VIDEO_UNWRAP(WrappedID3D11CryptoSession, pCryptoSession),
                                UNWRAP(WrappedID3D11Texture2D1, pSrcSurface),
                                UNWRAP(WrappedID3D11Texture2D1, pDstSurface), pEncryptedBlockInfo,
                                ContentKeySize, pContentKey, IVSize, pIV);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::StartSessionKeyRefresh(
    /* [annotation] */ _In_ ID3D11CryptoSession *pCryptoSession,
    /* [annotation] */ _In_ UINT RandomNumberSize,
    /* [annotation] */ _Out_writes_bytes_(RandomNumberSize) void *pRandomNumber)
{
  return m_pReal->StartSessionKeyRefresh(VIDEO_UNWRAP(WrappedID3D11CryptoSession, pCryptoSession),
                                         RandomNumberSize, pRandomNumber);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::FinishSessionKeyRefresh(
    /* [annotation] */ _In_ ID3D11CryptoSession *pCryptoSession)
{
  return m_pReal->FinishSessionKeyRefresh(VIDEO_UNWRAP(WrappedID3D11CryptoSession, pCryptoSession));
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::GetEncryptionBltKey(
    /* [annotation] */ _In_ ID3D11CryptoSession *pCryptoSession, /* [annotation] */ _In_ UINT KeySize,
    /* [annotation] */ _Out_writes_bytes_(KeySize) void *pReadbackKey)
{
  return m_pReal->GetEncryptionBltKey(VIDEO_UNWRAP(WrappedID3D11CryptoSession, pCryptoSession),
                                      KeySize, pReadbackKey);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::NegotiateAuthenticatedChannelKeyExchange(
    /* [annotation] */ _In_ ID3D11AuthenticatedChannel *pChannel,
    /* [annotation] */ _In_ UINT DataSize,
    /* [annotation] */ _Inout_updates_bytes_(DataSize) void *pData)
{
  return m_pReal->NegotiateAuthenticatedChannelKeyExchange(
      VIDEO_UNWRAP(WrappedID3D11AuthenticatedChannel, pChannel), DataSize, pData);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::QueryAuthenticatedChannel(
    /* [annotation] */ _In_ ID3D11AuthenticatedChannel *pChannel,
    /* [annotation] */ _In_ UINT InputSize,
    /* [annotation] */ _In_reads_bytes_(InputSize) const void *pInput,
    /* [annotation] */ _In_ UINT OutputSize,
    /* [annotation] */ _Out_writes_bytes_(OutputSize) void *pOutput)
{
  return m_pReal->QueryAuthenticatedChannel(VIDEO_UNWRAP(WrappedID3D11AuthenticatedChannel, pChannel),
                                            InputSize, pInput, OutputSize, pOutput);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::ConfigureAuthenticatedChannel(
    /* [annotation] */ _In_ ID3D11AuthenticatedChannel *pChannel,
    /* [annotation] */ _In_ UINT InputSize,
    /* [annotation] */ _In_reads_bytes_(InputSize) const void *pInput,
    /* [annotation] */ _Out_ D3D11_AUTHENTICATED_CONFIGURE_OUTPUT *pOutput)
{
  return m_pReal->ConfigureAuthenticatedChannel(
      VIDEO_UNWRAP(WrappedID3D11AuthenticatedChannel, pChannel), InputSize, pInput, pOutput);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamRotation(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ BOOL Enable,
    /* [annotation] */ _In_ D3D11_VIDEO_PROCESSOR_ROTATION Rotation)
{
  return m_pReal->VideoProcessorSetStreamRotation(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Enable, Rotation);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamRotation(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _Out_ BOOL *pEnable,
    /* [annotation] */ _Out_ D3D11_VIDEO_PROCESSOR_ROTATION *pRotation)
{
  return m_pReal->VideoProcessorGetStreamRotation(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pEnable, pRotation);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::SubmitDecoderBuffers1(
    /* [annotation] */ _In_ ID3D11VideoDecoder *pDecoder, /* [annotation] */ _In_ UINT NumBuffers,
    /* [annotation] */ _In_reads_(NumBuffers) const D3D11_VIDEO_DECODER_BUFFER_DESC1 *pBufferDesc)
{
  if(!m_pReal1)
    return E_NOINTERFACE;
  return m_pReal1->SubmitDecoderBuffers1(VIDEO_UNWRAP(WrappedID3D11VideoDecoder, pDecoder),
                                         NumBuffers, pBufferDesc);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::GetDataForNewHardwareKey(
    /* [annotation] */ _In_ ID3D11CryptoSession *pCryptoSession,
    /* [annotation] */ _In_ UINT PrivateInputSize,
    /* [annotation] */ _In_reads_(PrivateInputSize) const void *pPrivatInputData,
    /* [annotation] */ _Out_ UINT64 *pPrivateOutputData)
{
  if(!m_pReal1)
    return E_NOINTERFACE;
  return m_pReal1->GetDataForNewHardwareKey(VIDEO_UNWRAP(WrappedID3D11CryptoSession, pCryptoSession),
                                            PrivateInputSize, pPrivatInputData, pPrivateOutputData);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::CheckCryptoSessionStatus(
    /* [annotation] */ _In_ ID3D11CryptoSession *pCryptoSession,
    /* [annotation] */ _Out_ D3D11_CRYPTO_SESSION_STATUS *pStatus)
{
  if(!m_pReal1)
    return E_NOINTERFACE;
  return m_pReal1->CheckCryptoSessionStatus(
      VIDEO_UNWRAP(WrappedID3D11CryptoSession, pCryptoSession), pStatus);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::DecoderEnableDownsampling(
    /* [annotation] */ _In_ ID3D11VideoDecoder *pDecoder,
    /* [annotation] */ _In_ DXGI_COLOR_SPACE_TYPE InputColorSpace,
    /* [annotation] */ _In_ const D3D11_VIDEO_SAMPLE_DESC *pOutputDesc,
    /* [annotation] */ _In_ UINT ReferenceFrameCount)
{
  if(!m_pReal1)
    return E_NOINTERFACE;
  return m_pReal1->DecoderEnableDownsampling(VIDEO_UNWRAP(WrappedID3D11VideoDecoder, pDecoder),
                                             InputColorSpace, pOutputDesc, ReferenceFrameCount);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::DecoderUpdateDownsampling(
    /* [annotation] */ _In_ ID3D11VideoDecoder *pDecoder,
    /* [annotation] */ _In_ const D3D11_VIDEO_SAMPLE_DESC *pOutputDesc)
{
  if(!m_pReal1)
    return E_NOINTERFACE;
  return m_pReal1->DecoderUpdateDownsampling(VIDEO_UNWRAP(WrappedID3D11VideoDecoder, pDecoder),
                                             pOutputDesc);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetOutputColorSpace1(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ DXGI_COLOR_SPACE_TYPE ColorSpace)
{
  if(!m_pReal1)
    return;
  return m_pReal1->VideoProcessorSetOutputColorSpace1(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), ColorSpace);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetOutputShaderUsage(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ BOOL ShaderUsage)
{
  if(!m_pReal1)
    return;
  return m_pReal1->VideoProcessorSetOutputShaderUsage(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), ShaderUsage);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetOutputColorSpace1(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _Out_ DXGI_COLOR_SPACE_TYPE *pColorSpace)
{
  if(!m_pReal1)
    return;
  return m_pReal1->VideoProcessorGetOutputColorSpace1(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), pColorSpace);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetOutputShaderUsage(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _Out_ BOOL *pShaderUsage)
{
  if(!m_pReal1)
    return;
  return m_pReal1->VideoProcessorGetOutputShaderUsage(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), pShaderUsage);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamColorSpace1(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex,
    /* [annotation] */ _In_ DXGI_COLOR_SPACE_TYPE ColorSpace)
{
  if(!m_pReal1)
    return;
  return m_pReal1->VideoProcessorSetStreamColorSpace1(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, ColorSpace);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamMirror(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ BOOL Enable,
    /* [annotation] */ _In_ BOOL FlipHorizontal, /* [annotation] */ _In_ BOOL FlipVertical)
{
  if(!m_pReal1)
    return;
  return m_pReal1->VideoProcessorSetStreamMirror(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Enable,
      FlipHorizontal, FlipVertical);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamColorSpace1(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex,
    /* [annotation] */ _Out_ DXGI_COLOR_SPACE_TYPE *pColorSpace)
{
  if(!m_pReal1)
    return;
  return m_pReal1->VideoProcessorGetStreamColorSpace1(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pColorSpace);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamMirror(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _Out_ BOOL *pEnable,
    /* [annotation] */ _Out_ BOOL *pFlipHorizontal, /* [annotation] */ _Out_ BOOL *pFlipVertical)
{
  if(!m_pReal1)
    return;
  return m_pReal1->VideoProcessorGetStreamMirror(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pEnable,
      pFlipHorizontal, pFlipVertical);
}

HRESULT STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetBehaviorHints(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT OutputWidth, /* [annotation] */ _In_ UINT OutputHeight,
    /* [annotation] */ _In_ DXGI_FORMAT OutputFormat, /* [annotation] */ _In_ UINT StreamCount,
    /* [annotation] */ _In_reads_(StreamCount)
        const D3D11_VIDEO_PROCESSOR_STREAM_BEHAVIOR_HINT *pStreams,
    /* [annotation] */ _Out_ UINT *pBehaviorHints)
{
  if(!m_pReal1)
    return E_NOINTERFACE;
  return m_pReal1->VideoProcessorGetBehaviorHints(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), OutputWidth, OutputHeight,
      OutputFormat, StreamCount, pStreams, pBehaviorHints);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetOutputHDRMetaData(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ DXGI_HDR_METADATA_TYPE Type, /* [annotation] */ _In_ UINT Size,
    /* [annotation] */ _In_reads_bytes_opt_(Size) const void *pHDRMetaData)
{
  if(!m_pReal2)
    return;
  return m_pReal2->VideoProcessorSetOutputHDRMetaData(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), Type, Size, pHDRMetaData);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetOutputHDRMetaData(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _Out_ DXGI_HDR_METADATA_TYPE *pType, /* [annotation] */ _In_ UINT Size,
    /* [annotation] */ _Out_writes_bytes_opt_(Size) void *pMetaData)
{
  if(!m_pReal2)
    return;
  return m_pReal2->VideoProcessorGetOutputHDRMetaData(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), pType, Size, pMetaData);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorSetStreamHDRMetaData(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex, /* [annotation] */ _In_ DXGI_HDR_METADATA_TYPE Type,
    /* [annotation] */ _In_ UINT Size,
    /* [annotation] */ _In_reads_bytes_opt_(Size) const void *pHDRMetaData)
{
  if(!m_pReal2)
    return;
  return m_pReal2->VideoProcessorSetStreamHDRMetaData(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, Type, Size,
      pHDRMetaData);
}

void STDMETHODCALLTYPE WrappedID3D11VideoContext2::VideoProcessorGetStreamHDRMetaData(
    /* [annotation] */ _In_ ID3D11VideoProcessor *pVideoProcessor,
    /* [annotation] */ _In_ UINT StreamIndex,
    /* [annotation] */ _Out_ DXGI_HDR_METADATA_TYPE *pType, /* [annotation] */ _In_ UINT Size,
    /* [annotation] */ _Out_writes_bytes_opt_(Size) void *pMetaData)
{
  if(!m_pReal2)
    return;
  return m_pReal2->VideoProcessorGetStreamHDRMetaData(
      VIDEO_UNWRAP(WrappedID3D11VideoProcessor, pVideoProcessor), StreamIndex, pType, Size,
      pMetaData);
}

void STDMETHODCALLTYPE WrappedID3D11VideoDecoderOutputView::GetResource(
    /* [annotation] */ _Outptr_ ID3D11Resource **ppResource)
{
  ID3D11Resource *res = NULL;
  m_pReal->GetResource(&res);

  *ppResource = (ID3D11Resource *)m_pDevice->GetResourceManager()->GetWrapper(res);
}

void STDMETHODCALLTYPE WrappedID3D11VideoProcessorInputView::GetResource(
    /* [annotation] */ _Outptr_ ID3D11Resource **ppResource)
{
  ID3D11Resource *res = NULL;
  m_pReal->GetResource(&res);

  *ppResource = (ID3D11Resource *)m_pDevice->GetResourceManager()->GetWrapper(res);
}

void STDMETHODCALLTYPE WrappedID3D11VideoProcessorOutputView::GetResource(
    /* [annotation] */ _Outptr_ ID3D11Resource **ppResource)
{
  ID3D11Resource *res = NULL;
  m_pReal->GetResource(&res);

  *ppResource = (ID3D11Resource *)m_pDevice->GetResourceManager()->GetWrapper(res);
}

template <typename NestedType, typename NestedType1>
Wrapped11VideoDeviceChild<NestedType, NestedType1>::Wrapped11VideoDeviceChild(
    NestedType *real, WrappedID3D11Device *device)
    : RefCounter(real), m_pDevice(device), m_pReal(real)
{
  m_pDevice->SoftRef();
}

template <typename NestedType, typename NestedType1>
Wrapped11VideoDeviceChild<NestedType, NestedType1>::~Wrapped11VideoDeviceChild()
{
  SAFE_RELEASE(m_pReal);
  m_pDevice = NULL;
}

template <typename NestedType, typename NestedType1>
ULONG STDMETHODCALLTYPE Wrapped11VideoDeviceChild<NestedType, NestedType1>::AddRef()
{
  return RefCounter::SoftRef(m_pDevice);
}

template <typename NestedType, typename NestedType1>
ULONG STDMETHODCALLTYPE Wrapped11VideoDeviceChild<NestedType, NestedType1>::Release()
{
  return RefCounter::SoftRelease(m_pDevice);
}

template <typename NestedType, typename NestedType1>
HRESULT STDMETHODCALLTYPE
Wrapped11VideoDeviceChild<NestedType, NestedType1>::QueryInterface(REFIID riid, void **ppvObject)
{
  if(riid == __uuidof(IUnknown))
  {
    *ppvObject = (IUnknown *)(NestedType *)this;
    AddRef();
    return S_OK;
  }
  if(riid == __uuidof(NestedType))
  {
    *ppvObject = (NestedType *)this;
    AddRef();
    return S_OK;
  }
  if(riid == __uuidof(NestedType1))
  {
    // check that the real interface supports this
    NestedType1 *dummy = NULL;
    HRESULT check = m_pReal->QueryInterface(riid, (void **)&dummy);

    SAFE_RELEASE(dummy);

    if(FAILED(check))
      return check;

    *ppvObject = (NestedType1 *)this;
    AddRef();
    return S_OK;
  }
  if(riid == __uuidof(ID3D11DeviceChild))
  {
    *ppvObject = (ID3D11DeviceChild *)this;
    AddRef();
    return S_OK;
  }
  if(riid == __uuidof(ID3D11Multithread))
  {
    // forward to the device as the lock is shared amongst all things
    return m_pDevice->QueryInterface(riid, ppvObject);
  }

  return RefCounter::QueryInterface(riid, ppvObject);
}

template <typename NestedType, typename NestedType1>
void STDMETHODCALLTYPE Wrapped11VideoDeviceChild<NestedType, NestedType1>::GetDevice(
    /* [annotation] */ __out ID3D11Device **ppDevice)
{
  if(ppDevice)
  {
    *ppDevice = m_pDevice;
    m_pDevice->AddRef();
  }
}