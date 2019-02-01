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

#pragma once

#include "common/wrapped_pool.h"
#include "d3d11_common.h"

class WrappedID3D11Device;
class WrappedID3D11DeviceContext;

struct WrappedID3D11VideoDevice2 : public ID3D11VideoDevice2
{
  WrappedID3D11Device *m_pDevice = NULL;
  ID3D11VideoDevice *m_pReal = NULL;
  ID3D11VideoDevice1 *m_pReal1 = NULL;
  ID3D11VideoDevice2 *m_pReal2 = NULL;

  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D11VideoDevice
  virtual HRESULT STDMETHODCALLTYPE CreateVideoDecoder(
      /* [annotation] */
      _In_ const D3D11_VIDEO_DECODER_DESC *pVideoDesc,
      /* [annotation] */
      _In_ const D3D11_VIDEO_DECODER_CONFIG *pConfig,
      /* [annotation] */
      _COM_Outptr_ ID3D11VideoDecoder **ppDecoder);

  virtual HRESULT STDMETHODCALLTYPE CreateVideoProcessor(
      /* [annotation] */
      _In_ ID3D11VideoProcessorEnumerator *pEnum,
      /* [annotation] */
      _In_ UINT RateConversionIndex,
      /* [annotation] */
      _COM_Outptr_ ID3D11VideoProcessor **ppVideoProcessor);

  virtual HRESULT STDMETHODCALLTYPE CreateAuthenticatedChannel(
      /* [annotation] */
      _In_ D3D11_AUTHENTICATED_CHANNEL_TYPE ChannelType,
      /* [annotation] */
      _COM_Outptr_ ID3D11AuthenticatedChannel **ppAuthenticatedChannel);

  virtual HRESULT STDMETHODCALLTYPE CreateCryptoSession(
      /* [annotation] */
      _In_ const GUID *pCryptoType,
      /* [annotation] */
      _In_opt_ const GUID *pDecoderProfile,
      /* [annotation] */
      _In_ const GUID *pKeyExchangeType,
      /* [annotation] */
      _COM_Outptr_ ID3D11CryptoSession **ppCryptoSession);

  virtual HRESULT STDMETHODCALLTYPE CreateVideoDecoderOutputView(
      /* [annotation] */
      _In_ ID3D11Resource *pResource,
      /* [annotation] */
      _In_ const D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC *pDesc,
      /* [annotation] */
      _COM_Outptr_opt_ ID3D11VideoDecoderOutputView **ppVDOVView);

  virtual HRESULT STDMETHODCALLTYPE CreateVideoProcessorInputView(
      /* [annotation] */
      _In_ ID3D11Resource *pResource,
      /* [annotation] */
      _In_ ID3D11VideoProcessorEnumerator *pEnum,
      /* [annotation] */
      _In_ const D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC *pDesc,
      /* [annotation] */
      _COM_Outptr_opt_ ID3D11VideoProcessorInputView **ppVPIView);

  virtual HRESULT STDMETHODCALLTYPE CreateVideoProcessorOutputView(
      /* [annotation] */
      _In_ ID3D11Resource *pResource,
      /* [annotation] */
      _In_ ID3D11VideoProcessorEnumerator *pEnum,
      /* [annotation] */
      _In_ const D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC *pDesc,
      /* [annotation] */
      _COM_Outptr_opt_ ID3D11VideoProcessorOutputView **ppVPOView);

  virtual HRESULT STDMETHODCALLTYPE CreateVideoProcessorEnumerator(
      /* [annotation] */
      _In_ const D3D11_VIDEO_PROCESSOR_CONTENT_DESC *pDesc,
      /* [annotation] */
      _COM_Outptr_ ID3D11VideoProcessorEnumerator **ppEnum);

  virtual UINT STDMETHODCALLTYPE GetVideoDecoderProfileCount(void);

  virtual HRESULT STDMETHODCALLTYPE GetVideoDecoderProfile(
      /* [annotation] */
      _In_ UINT Index,
      /* [annotation] */
      _Out_ GUID *pDecoderProfile);

  virtual HRESULT STDMETHODCALLTYPE CheckVideoDecoderFormat(
      /* [annotation] */
      _In_ const GUID *pDecoderProfile,
      /* [annotation] */
      _In_ DXGI_FORMAT Format,
      /* [annotation] */
      _Out_ BOOL *pSupported);

  virtual HRESULT STDMETHODCALLTYPE GetVideoDecoderConfigCount(
      /* [annotation] */
      _In_ const D3D11_VIDEO_DECODER_DESC *pDesc,
      /* [annotation] */
      _Out_ UINT *pCount);

  virtual HRESULT STDMETHODCALLTYPE GetVideoDecoderConfig(
      /* [annotation] */
      _In_ const D3D11_VIDEO_DECODER_DESC *pDesc,
      /* [annotation] */
      _In_ UINT Index,
      /* [annotation] */
      _Out_ D3D11_VIDEO_DECODER_CONFIG *pConfig);

  virtual HRESULT STDMETHODCALLTYPE GetContentProtectionCaps(
      /* [annotation] */
      _In_opt_ const GUID *pCryptoType,
      /* [annotation] */
      _In_opt_ const GUID *pDecoderProfile,
      /* [annotation] */
      _Out_ D3D11_VIDEO_CONTENT_PROTECTION_CAPS *pCaps);

  virtual HRESULT STDMETHODCALLTYPE CheckCryptoKeyExchange(
      /* [annotation] */
      _In_ const GUID *pCryptoType,
      /* [annotation] */
      _In_opt_ const GUID *pDecoderProfile,
      /* [annotation] */
      _In_ UINT Index,
      /* [annotation] */
      _Out_ GUID *pKeyExchangeType);

  virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
      /* [annotation] */
      _In_ REFGUID guid,
      /* [annotation] */
      _In_ UINT DataSize,
      /* [annotation] */
      _In_reads_bytes_opt_(DataSize) const void *pData)
  {
    return m_pReal->SetPrivateData(guid, DataSize, pData);
  }

  virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
      /* [annotation] */
      _In_ REFGUID guid,
      /* [annotation] */
      _In_opt_ const IUnknown *pData)
  {
    return m_pReal->SetPrivateDataInterface(guid, pData);
  }

  //////////////////////////////
  // implement ID3D11VideoDevice1
  virtual HRESULT STDMETHODCALLTYPE GetCryptoSessionPrivateDataSize(
      /* [annotation] */
      _In_ const GUID *pCryptoType,
      /* [annotation] */
      _In_opt_ const GUID *pDecoderProfile,
      /* [annotation] */
      _In_ const GUID *pKeyExchangeType,
      /* [annotation] */
      _Out_ UINT *pPrivateInputSize,
      /* [annotation] */
      _Out_ UINT *pPrivateOutputSize);

  virtual HRESULT STDMETHODCALLTYPE GetVideoDecoderCaps(
      /* [annotation] */
      _In_ const GUID *pDecoderProfile,
      /* [annotation] */
      _In_ UINT SampleWidth,
      /* [annotation] */
      _In_ UINT SampleHeight,
      /* [annotation] */
      _In_ const DXGI_RATIONAL *pFrameRate,
      /* [annotation] */
      _In_ UINT BitRate,
      /* [annotation] */
      _In_opt_ const GUID *pCryptoType,
      /* [annotation] */
      _Out_ UINT *pDecoderCaps);

  virtual HRESULT STDMETHODCALLTYPE CheckVideoDecoderDownsampling(
      /* [annotation] */
      _In_ const D3D11_VIDEO_DECODER_DESC *pInputDesc,
      /* [annotation] */
      _In_ DXGI_COLOR_SPACE_TYPE InputColorSpace,
      /* [annotation] */
      _In_ const D3D11_VIDEO_DECODER_CONFIG *pInputConfig,
      /* [annotation] */
      _In_ const DXGI_RATIONAL *pFrameRate,
      /* [annotation] */
      _In_ const D3D11_VIDEO_SAMPLE_DESC *pOutputDesc,
      /* [annotation] */
      _Out_ BOOL *pSupported,
      /* [annotation] */
      _Out_ BOOL *pRealTimeHint);

  virtual HRESULT STDMETHODCALLTYPE RecommendVideoDecoderDownsampleParameters(
      /* [annotation] */
      _In_ const D3D11_VIDEO_DECODER_DESC *pInputDesc,
      /* [annotation] */
      _In_ DXGI_COLOR_SPACE_TYPE InputColorSpace,
      /* [annotation] */
      _In_ const D3D11_VIDEO_DECODER_CONFIG *pInputConfig,
      /* [annotation] */
      _In_ const DXGI_RATIONAL *pFrameRate,
      /* [annotation] */
      _Out_ D3D11_VIDEO_SAMPLE_DESC *pRecommendedOutputDesc);

  //////////////////////////////
  // implement ID3D11VideoDevice2

  virtual HRESULT STDMETHODCALLTYPE
  CheckFeatureSupport(D3D11_FEATURE_VIDEO Feature,
                      /* [annotation] */
                      _Out_writes_bytes_(FeatureSupportDataSize) void *pFeatureSupportData,
                      UINT FeatureSupportDataSize);

  virtual HRESULT STDMETHODCALLTYPE NegotiateCryptoSessionKeyExchangeMT(
      /* [annotation] */
      _In_ ID3D11CryptoSession *pCryptoSession,
      /* [annotation] */
      _In_ D3D11_CRYPTO_SESSION_KEY_EXCHANGE_FLAGS flags,
      /* [annotation] */
      _In_ UINT DataSize,
      /* [annotation] */
      _Inout_updates_bytes_(DataSize) void *pData);
};

struct WrappedID3D11VideoContext2 : public ID3D11VideoContext2
{
  WrappedID3D11DeviceContext *m_pContext = NULL;
  ID3D11VideoContext *m_pReal = NULL;
  ID3D11VideoContext1 *m_pReal1 = NULL;
  ID3D11VideoContext2 *m_pReal2 = NULL;

  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D11DeviceChild
  virtual void STDMETHODCALLTYPE GetDevice(
      /* [annotation] */
      _Outptr_ ID3D11Device **ppDevice);

  virtual HRESULT STDMETHODCALLTYPE GetPrivateData(
      /* [annotation] */
      _In_ REFGUID guid,
      /* [annotation] */
      _Inout_ UINT *pDataSize,
      /* [annotation] */
      _Out_writes_bytes_opt_(*pDataSize) void *pData)
  {
    return m_pReal->GetPrivateData(guid, pDataSize, pData);
  }

  virtual HRESULT STDMETHODCALLTYPE SetPrivateData(
      /* [annotation] */
      _In_ REFGUID guid,
      /* [annotation] */
      _In_ UINT DataSize,
      /* [annotation] */
      _In_reads_bytes_opt_(DataSize) const void *pData)
  {
    return m_pReal->SetPrivateData(guid, DataSize, pData);
  }

  virtual HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
      /* [annotation] */
      _In_ REFGUID guid,
      /* [annotation] */
      _In_opt_ const IUnknown *pData)
  {
    return m_pReal->SetPrivateDataInterface(guid, pData);
  }

  //////////////////////////////
  // implement ID3D11VideoContext
  virtual HRESULT STDMETHODCALLTYPE GetDecoderBuffer(
      /* [annotation] */
      _In_ ID3D11VideoDecoder *pDecoder, D3D11_VIDEO_DECODER_BUFFER_TYPE Type,
      /* [annotation] */
      _Out_ UINT *pBufferSize,
      /* [annotation] */
      _Outptr_result_bytebuffer_(*pBufferSize) void **ppBuffer);

  virtual HRESULT STDMETHODCALLTYPE ReleaseDecoderBuffer(
      /* [annotation] */
      _In_ ID3D11VideoDecoder *pDecoder,
      /* [annotation] */
      _In_ D3D11_VIDEO_DECODER_BUFFER_TYPE Type);

  virtual HRESULT STDMETHODCALLTYPE DecoderBeginFrame(
      /* [annotation] */
      _In_ ID3D11VideoDecoder *pDecoder,
      /* [annotation] */
      _In_ ID3D11VideoDecoderOutputView *pView, UINT ContentKeySize,
      /* [annotation] */
      _In_reads_bytes_opt_(ContentKeySize) const void *pContentKey);

  virtual HRESULT STDMETHODCALLTYPE DecoderEndFrame(
      /* [annotation] */
      _In_ ID3D11VideoDecoder *pDecoder);

  virtual HRESULT STDMETHODCALLTYPE SubmitDecoderBuffers(
      /* [annotation] */
      _In_ ID3D11VideoDecoder *pDecoder,
      /* [annotation] */
      _In_ UINT NumBuffers,
      /* [annotation] */
      _In_reads_(NumBuffers) const D3D11_VIDEO_DECODER_BUFFER_DESC *pBufferDesc);

  virtual APP_DEPRECATED_HRESULT STDMETHODCALLTYPE DecoderExtension(
      /* [annotation] */
      _In_ ID3D11VideoDecoder *pDecoder,
      /* [annotation] */
      _In_ const D3D11_VIDEO_DECODER_EXTENSION *pExtensionData);

  virtual void STDMETHODCALLTYPE VideoProcessorSetOutputTargetRect(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ BOOL Enable,
      /* [annotation] */
      _In_opt_ const RECT *pRect);

  virtual void STDMETHODCALLTYPE VideoProcessorSetOutputBackgroundColor(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ BOOL YCbCr,
      /* [annotation] */
      _In_ const D3D11_VIDEO_COLOR *pColor);

  virtual void STDMETHODCALLTYPE VideoProcessorSetOutputColorSpace(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace);

  virtual void STDMETHODCALLTYPE VideoProcessorSetOutputAlphaFillMode(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE AlphaFillMode,
      /* [annotation] */
      _In_ UINT StreamIndex);

  virtual void STDMETHODCALLTYPE VideoProcessorSetOutputConstriction(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ BOOL Enable,
      /* [annotation] */
      _In_ SIZE Size);

  virtual void STDMETHODCALLTYPE VideoProcessorSetOutputStereoMode(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ BOOL Enable);

  virtual APP_DEPRECATED_HRESULT STDMETHODCALLTYPE VideoProcessorSetOutputExtension(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ const GUID *pExtensionGuid,
      /* [annotation] */
      _In_ UINT DataSize,
      /* [annotation] */
      _In_ void *pData);

  virtual void STDMETHODCALLTYPE VideoProcessorGetOutputTargetRect(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _Out_ BOOL *Enabled,
      /* [annotation] */
      _Out_ RECT *pRect);

  virtual void STDMETHODCALLTYPE VideoProcessorGetOutputBackgroundColor(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _Out_ BOOL *pYCbCr,
      /* [annotation] */
      _Out_ D3D11_VIDEO_COLOR *pColor);

  virtual void STDMETHODCALLTYPE VideoProcessorGetOutputColorSpace(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace);

  virtual void STDMETHODCALLTYPE VideoProcessorGetOutputAlphaFillMode(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_ALPHA_FILL_MODE *pAlphaFillMode,
      /* [annotation] */
      _Out_ UINT *pStreamIndex);

  virtual void STDMETHODCALLTYPE VideoProcessorGetOutputConstriction(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _Out_ BOOL *pEnabled,
      /* [annotation] */
      _Out_ SIZE *pSize);

  virtual void STDMETHODCALLTYPE VideoProcessorGetOutputStereoMode(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _Out_ BOOL *pEnabled);

  virtual APP_DEPRECATED_HRESULT STDMETHODCALLTYPE VideoProcessorGetOutputExtension(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ const GUID *pExtensionGuid,
      /* [annotation] */
      _In_ UINT DataSize,
      /* [annotation] */
      _Out_writes_bytes_(DataSize) void *pData);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamFrameFormat(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ D3D11_VIDEO_FRAME_FORMAT FrameFormat);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamColorSpace(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ const D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamOutputRate(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ D3D11_VIDEO_PROCESSOR_OUTPUT_RATE OutputRate,
      /* [annotation] */
      _In_ BOOL RepeatFrame,
      /* [annotation] */
      _In_opt_ const DXGI_RATIONAL *pCustomRate);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamSourceRect(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ BOOL Enable,
      /* [annotation] */
      _In_opt_ const RECT *pRect);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamDestRect(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ BOOL Enable,
      /* [annotation] */
      _In_opt_ const RECT *pRect);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamAlpha(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ BOOL Enable,
      /* [annotation] */
      _In_ FLOAT Alpha);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamPalette(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ UINT Count,
      /* [annotation] */
      _In_reads_opt_(Count) const UINT *pEntries);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamPixelAspectRatio(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ BOOL Enable,
      /* [annotation] */
      _In_opt_ const DXGI_RATIONAL *pSourceAspectRatio,
      /* [annotation] */
      _In_opt_ const DXGI_RATIONAL *pDestinationAspectRatio);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamLumaKey(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ BOOL Enable,
      /* [annotation] */
      _In_ FLOAT Lower,
      /* [annotation] */
      _In_ FLOAT Upper);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamStereoFormat(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ BOOL Enable,
      /* [annotation] */
      _In_ D3D11_VIDEO_PROCESSOR_STEREO_FORMAT Format,
      /* [annotation] */
      _In_ BOOL LeftViewFrame0,
      /* [annotation] */
      _In_ BOOL BaseViewFrame0,
      /* [annotation] */
      _In_ D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE FlipMode,
      /* [annotation] */
      _In_ int MonoOffset);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamAutoProcessingMode(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ BOOL Enable);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamFilter(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ D3D11_VIDEO_PROCESSOR_FILTER Filter,
      /* [annotation] */
      _In_ BOOL Enable,
      /* [annotation] */
      _In_ int Level);

  virtual APP_DEPRECATED_HRESULT STDMETHODCALLTYPE VideoProcessorSetStreamExtension(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ const GUID *pExtensionGuid,
      /* [annotation] */
      _In_ UINT DataSize,
      /* [annotation] */
      _In_ void *pData);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamFrameFormat(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ D3D11_VIDEO_FRAME_FORMAT *pFrameFormat);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamColorSpace(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_COLOR_SPACE *pColorSpace);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamOutputRate(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_OUTPUT_RATE *pOutputRate,
      /* [annotation] */
      _Out_ BOOL *pRepeatFrame,
      /* [annotation] */
      _Out_ DXGI_RATIONAL *pCustomRate);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamSourceRect(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ BOOL *pEnabled,
      /* [annotation] */
      _Out_ RECT *pRect);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamDestRect(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ BOOL *pEnabled,
      /* [annotation] */
      _Out_ RECT *pRect);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamAlpha(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ BOOL *pEnabled,
      /* [annotation] */
      _Out_ FLOAT *pAlpha);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamPalette(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ UINT Count,
      /* [annotation] */
      _Out_writes_(Count) UINT *pEntries);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamPixelAspectRatio(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ BOOL *pEnabled,
      /* [annotation] */
      _Out_ DXGI_RATIONAL *pSourceAspectRatio,
      /* [annotation] */
      _Out_ DXGI_RATIONAL *pDestinationAspectRatio);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamLumaKey(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ BOOL *pEnabled,
      /* [annotation] */
      _Out_ FLOAT *pLower,
      /* [annotation] */
      _Out_ FLOAT *pUpper);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamStereoFormat(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ BOOL *pEnable,
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_STEREO_FORMAT *pFormat,
      /* [annotation] */
      _Out_ BOOL *pLeftViewFrame0,
      /* [annotation] */
      _Out_ BOOL *pBaseViewFrame0,
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_STEREO_FLIP_MODE *pFlipMode,
      /* [annotation] */
      _Out_ int *MonoOffset);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamAutoProcessingMode(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ BOOL *pEnabled);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamFilter(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ D3D11_VIDEO_PROCESSOR_FILTER Filter,
      /* [annotation] */
      _Out_ BOOL *pEnabled,
      /* [annotation] */
      _Out_ int *pLevel);

  virtual APP_DEPRECATED_HRESULT STDMETHODCALLTYPE VideoProcessorGetStreamExtension(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ const GUID *pExtensionGuid,
      /* [annotation] */
      _In_ UINT DataSize,
      /* [annotation] */
      _Out_writes_bytes_(DataSize) void *pData);

  virtual HRESULT STDMETHODCALLTYPE VideoProcessorBlt(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ ID3D11VideoProcessorOutputView *pView,
      /* [annotation] */
      _In_ UINT OutputFrame,
      /* [annotation] */
      _In_ UINT StreamCount,
      /* [annotation] */
      _In_reads_(StreamCount) const D3D11_VIDEO_PROCESSOR_STREAM *pStreams);

  virtual HRESULT STDMETHODCALLTYPE NegotiateCryptoSessionKeyExchange(
      /* [annotation] */
      _In_ ID3D11CryptoSession *pCryptoSession,
      /* [annotation] */
      _In_ UINT DataSize,
      /* [annotation] */
      _Inout_updates_bytes_(DataSize) void *pData);

  virtual void STDMETHODCALLTYPE EncryptionBlt(
      /* [annotation] */
      _In_ ID3D11CryptoSession *pCryptoSession,
      /* [annotation] */
      _In_ ID3D11Texture2D *pSrcSurface,
      /* [annotation] */
      _In_ ID3D11Texture2D *pDstSurface,
      /* [annotation] */
      _In_ UINT IVSize,
      /* [annotation] */
      _Inout_opt_bytecount_(IVSize) void *pIV);

  virtual void STDMETHODCALLTYPE DecryptionBlt(
      /* [annotation] */
      _In_ ID3D11CryptoSession *pCryptoSession,
      /* [annotation] */
      _In_ ID3D11Texture2D *pSrcSurface,
      /* [annotation] */
      _In_ ID3D11Texture2D *pDstSurface,
      /* [annotation] */
      _In_opt_ D3D11_ENCRYPTED_BLOCK_INFO *pEncryptedBlockInfo,
      /* [annotation] */
      _In_ UINT ContentKeySize,
      /* [annotation] */
      _In_reads_bytes_opt_(ContentKeySize) const void *pContentKey,
      /* [annotation] */
      _In_ UINT IVSize,
      /* [annotation] */
      _Inout_opt_bytecount_(IVSize) void *pIV);

  virtual void STDMETHODCALLTYPE StartSessionKeyRefresh(
      /* [annotation] */
      _In_ ID3D11CryptoSession *pCryptoSession,
      /* [annotation] */
      _In_ UINT RandomNumberSize,
      /* [annotation] */
      _Out_writes_bytes_(RandomNumberSize) void *pRandomNumber);

  virtual void STDMETHODCALLTYPE FinishSessionKeyRefresh(
      /* [annotation] */
      _In_ ID3D11CryptoSession *pCryptoSession);

  virtual HRESULT STDMETHODCALLTYPE GetEncryptionBltKey(
      /* [annotation] */
      _In_ ID3D11CryptoSession *pCryptoSession,
      /* [annotation] */
      _In_ UINT KeySize,
      /* [annotation] */
      _Out_writes_bytes_(KeySize) void *pReadbackKey);

  virtual HRESULT STDMETHODCALLTYPE NegotiateAuthenticatedChannelKeyExchange(
      /* [annotation] */
      _In_ ID3D11AuthenticatedChannel *pChannel,
      /* [annotation] */
      _In_ UINT DataSize,
      /* [annotation] */
      _Inout_updates_bytes_(DataSize) void *pData);

  virtual HRESULT STDMETHODCALLTYPE QueryAuthenticatedChannel(
      /* [annotation] */
      _In_ ID3D11AuthenticatedChannel *pChannel,
      /* [annotation] */
      _In_ UINT InputSize,
      /* [annotation] */
      _In_reads_bytes_(InputSize) const void *pInput,
      /* [annotation] */
      _In_ UINT OutputSize,
      /* [annotation] */
      _Out_writes_bytes_(OutputSize) void *pOutput);

  virtual HRESULT STDMETHODCALLTYPE ConfigureAuthenticatedChannel(
      /* [annotation] */
      _In_ ID3D11AuthenticatedChannel *pChannel,
      /* [annotation] */
      _In_ UINT InputSize,
      /* [annotation] */
      _In_reads_bytes_(InputSize) const void *pInput,
      /* [annotation] */
      _Out_ D3D11_AUTHENTICATED_CONFIGURE_OUTPUT *pOutput);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamRotation(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ BOOL Enable,
      /* [annotation] */
      _In_ D3D11_VIDEO_PROCESSOR_ROTATION Rotation);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamRotation(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ BOOL *pEnable,
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_ROTATION *pRotation);

  //////////////////////////////
  // implement ID3D11VideoContext1
  virtual HRESULT STDMETHODCALLTYPE SubmitDecoderBuffers1(
      /* [annotation] */
      _In_ ID3D11VideoDecoder *pDecoder,
      /* [annotation] */
      _In_ UINT NumBuffers,
      /* [annotation] */
      _In_reads_(NumBuffers) const D3D11_VIDEO_DECODER_BUFFER_DESC1 *pBufferDesc);

  virtual HRESULT STDMETHODCALLTYPE GetDataForNewHardwareKey(
      /* [annotation] */
      _In_ ID3D11CryptoSession *pCryptoSession,
      /* [annotation] */
      _In_ UINT PrivateInputSize,
      /* [annotation] */
      _In_reads_(PrivateInputSize) const void *pPrivatInputData,
      /* [annotation] */
      _Out_ UINT64 *pPrivateOutputData);

  virtual HRESULT STDMETHODCALLTYPE CheckCryptoSessionStatus(
      /* [annotation] */
      _In_ ID3D11CryptoSession *pCryptoSession,
      /* [annotation] */
      _Out_ D3D11_CRYPTO_SESSION_STATUS *pStatus);

  virtual HRESULT STDMETHODCALLTYPE DecoderEnableDownsampling(
      /* [annotation] */
      _In_ ID3D11VideoDecoder *pDecoder,
      /* [annotation] */
      _In_ DXGI_COLOR_SPACE_TYPE InputColorSpace,
      /* [annotation] */
      _In_ const D3D11_VIDEO_SAMPLE_DESC *pOutputDesc,
      /* [annotation] */
      _In_ UINT ReferenceFrameCount);

  virtual HRESULT STDMETHODCALLTYPE DecoderUpdateDownsampling(
      /* [annotation] */
      _In_ ID3D11VideoDecoder *pDecoder,
      /* [annotation] */
      _In_ const D3D11_VIDEO_SAMPLE_DESC *pOutputDesc);

  virtual void STDMETHODCALLTYPE VideoProcessorSetOutputColorSpace1(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ DXGI_COLOR_SPACE_TYPE ColorSpace);

  virtual void STDMETHODCALLTYPE VideoProcessorSetOutputShaderUsage(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ BOOL ShaderUsage);

  virtual void STDMETHODCALLTYPE VideoProcessorGetOutputColorSpace1(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _Out_ DXGI_COLOR_SPACE_TYPE *pColorSpace);

  virtual void STDMETHODCALLTYPE VideoProcessorGetOutputShaderUsage(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _Out_ BOOL *pShaderUsage);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamColorSpace1(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ DXGI_COLOR_SPACE_TYPE ColorSpace);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamMirror(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ BOOL Enable,
      /* [annotation] */
      _In_ BOOL FlipHorizontal,
      /* [annotation] */
      _In_ BOOL FlipVertical);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamColorSpace1(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ DXGI_COLOR_SPACE_TYPE *pColorSpace);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamMirror(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ BOOL *pEnable,
      /* [annotation] */
      _Out_ BOOL *pFlipHorizontal,
      /* [annotation] */
      _Out_ BOOL *pFlipVertical);

  virtual HRESULT STDMETHODCALLTYPE VideoProcessorGetBehaviorHints(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT OutputWidth,
      /* [annotation] */
      _In_ UINT OutputHeight,
      /* [annotation] */
      _In_ DXGI_FORMAT OutputFormat,
      /* [annotation] */
      _In_ UINT StreamCount,
      /* [annotation] */
      _In_reads_(StreamCount) const D3D11_VIDEO_PROCESSOR_STREAM_BEHAVIOR_HINT *pStreams,
      /* [annotation] */
      _Out_ UINT *pBehaviorHints);

  //////////////////////////////
  // implement ID3D11VideoContext2
  virtual void STDMETHODCALLTYPE VideoProcessorSetOutputHDRMetaData(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ DXGI_HDR_METADATA_TYPE Type,
      /* [annotation] */
      _In_ UINT Size,
      /* [annotation] */
      _In_reads_bytes_opt_(Size) const void *pHDRMetaData);

  virtual void STDMETHODCALLTYPE VideoProcessorGetOutputHDRMetaData(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _Out_ DXGI_HDR_METADATA_TYPE *pType,
      /* [annotation] */
      _In_ UINT Size,
      /* [annotation] */
      _Out_writes_bytes_opt_(Size) void *pMetaData);

  virtual void STDMETHODCALLTYPE VideoProcessorSetStreamHDRMetaData(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _In_ DXGI_HDR_METADATA_TYPE Type,
      /* [annotation] */
      _In_ UINT Size,
      /* [annotation] */
      _In_reads_bytes_opt_(Size) const void *pHDRMetaData);

  virtual void STDMETHODCALLTYPE VideoProcessorGetStreamHDRMetaData(
      /* [annotation] */
      _In_ ID3D11VideoProcessor *pVideoProcessor,
      /* [annotation] */
      _In_ UINT StreamIndex,
      /* [annotation] */
      _Out_ DXGI_HDR_METADATA_TYPE *pType,
      /* [annotation] */
      _In_ UINT Size,
      /* [annotation] */
      _Out_writes_bytes_opt_(Size) void *pMetaData);
};

// we don't want to depend on d3d11_resources.h in this header, so replicate
// WrappedID3D11DeviceChild here (and it's simpler because these are pure pass-through wrappers)
template <typename NestedType, typename NestedType1 = NestedType>
struct Wrapped11VideoDeviceChild : public RefCounter, public NestedType1
{
protected:
  WrappedID3D11Device *m_pDevice;
  NestedType *m_pReal;

  Wrapped11VideoDeviceChild(NestedType *real, WrappedID3D11Device *device);
  virtual ~Wrapped11VideoDeviceChild();

public:
  typedef NestedType InnerType;

  NestedType *GetReal() { return m_pReal; }
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  //////////////////////////////
  // implement ID3D11DeviceChild

  void STDMETHODCALLTYPE GetDevice(
      /* [annotation] */
      __out ID3D11Device **ppDevice);

  HRESULT STDMETHODCALLTYPE GetPrivateData(
      /* [annotation] */
      __in REFGUID guid,
      /* [annotation] */
      __inout UINT *pDataSize,
      /* [annotation] */
      __out_bcount_opt(*pDataSize) void *pData)
  {
    return m_pReal->GetPrivateData(guid, pDataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateData(
      /* [annotation] */
      __in REFGUID guid,
      /* [annotation] */
      __in UINT DataSize,
      /* [annotation] */
      __in_bcount_opt(DataSize) const void *pData)
  {
    return m_pReal->SetPrivateData(guid, DataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
      /* [annotation] */
      __in REFGUID guid,
      /* [annotation] */
      __in_opt const IUnknown *pData)
  {
    return m_pReal->SetPrivateDataInterface(guid, pData);
  }
};

class WrappedID3D11AuthenticatedChannel : public Wrapped11VideoDeviceChild<ID3D11AuthenticatedChannel>
{
public:
  WrappedID3D11AuthenticatedChannel(ID3D11AuthenticatedChannel *real, WrappedID3D11Device *device)
      : Wrapped11VideoDeviceChild(real, device)
  {
  }
  //////////////////////////////
  // implement ID3D11AuthenticatedChannel
  virtual HRESULT STDMETHODCALLTYPE GetCertificateSize(
      /* [annotation] */
      _Out_ UINT *pCertificateSize)
  {
    return m_pReal->GetCertificateSize(pCertificateSize);
  }

  virtual HRESULT STDMETHODCALLTYPE GetCertificate(
      /* [annotation] */
      _In_ UINT CertificateSize,
      /* [annotation] */
      _Out_writes_bytes_(CertificateSize) BYTE *pCertificate)
  {
    return m_pReal->GetCertificate(CertificateSize, pCertificate);
  }

  virtual void STDMETHODCALLTYPE GetChannelHandle(
      /* [annotation] */
      _Out_ HANDLE *pChannelHandle)
  {
    return m_pReal->GetChannelHandle(pChannelHandle);
  }
};

class WrappedID3D11CryptoSession : public Wrapped11VideoDeviceChild<ID3D11CryptoSession>
{
public:
  WrappedID3D11CryptoSession(ID3D11CryptoSession *real, WrappedID3D11Device *device)
      : Wrapped11VideoDeviceChild(real, device)
  {
  }
  //////////////////////////////
  // implement ID3D11CryptoSession
  virtual void STDMETHODCALLTYPE GetCryptoType(
      /* [annotation] */
      _Out_ GUID *pCryptoType)
  {
    return m_pReal->GetCryptoType(pCryptoType);
  }

  virtual void STDMETHODCALLTYPE GetDecoderProfile(
      /* [annotation] */
      _Out_ GUID *pDecoderProfile)
  {
    return m_pReal->GetDecoderProfile(pDecoderProfile);
  }

  virtual HRESULT STDMETHODCALLTYPE GetCertificateSize(
      /* [annotation] */
      _Out_ UINT *pCertificateSize)
  {
    return m_pReal->GetCertificateSize(pCertificateSize);
  }

  virtual HRESULT STDMETHODCALLTYPE GetCertificate(
      /* [annotation] */
      _In_ UINT CertificateSize,
      /* [annotation] */
      _Out_writes_bytes_(CertificateSize) BYTE *pCertificate)
  {
    return m_pReal->GetCertificate(CertificateSize, pCertificate);
  }

  virtual void STDMETHODCALLTYPE GetCryptoSessionHandle(
      /* [annotation] */
      _Out_ HANDLE *pCryptoSessionHandle)
  {
    return m_pReal->GetCryptoSessionHandle(pCryptoSessionHandle);
  }
};

class WrappedID3D11VideoDecoder : public Wrapped11VideoDeviceChild<ID3D11VideoDecoder>
{
public:
  WrappedID3D11VideoDecoder(ID3D11VideoDecoder *real, WrappedID3D11Device *device)
      : Wrapped11VideoDeviceChild(real, device)
  {
  }
  //////////////////////////////
  // implement ID3D11VideoDecoder
  virtual HRESULT STDMETHODCALLTYPE GetCreationParameters(
      /* [annotation] */
      _Out_ D3D11_VIDEO_DECODER_DESC *pVideoDesc,
      /* [annotation] */
      _Out_ D3D11_VIDEO_DECODER_CONFIG *pConfig)
  {
    return m_pReal->GetCreationParameters(pVideoDesc, pConfig);
  }

  virtual HRESULT STDMETHODCALLTYPE GetDriverHandle(
      /* [annotation] */
      _Out_ HANDLE *pDriverHandle)
  {
    return m_pReal->GetDriverHandle(pDriverHandle);
  }
};

class WrappedID3D11VideoDecoderOutputView
    : public Wrapped11VideoDeviceChild<ID3D11VideoDecoderOutputView>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11VideoDecoderOutputView);

  WrappedID3D11VideoDecoderOutputView(ID3D11VideoDecoderOutputView *real, WrappedID3D11Device *device)
      : Wrapped11VideoDeviceChild(real, device)
  {
  }
  //////////////////////////////
  // implement ID3D11View
  virtual void STDMETHODCALLTYPE GetResource(
      /* [annotation] */
      _Outptr_ ID3D11Resource **ppResource);

  //////////////////////////////
  // implement ID3D11VideoDecoderOutputView
  virtual void STDMETHODCALLTYPE GetDesc(
      /* [annotation] */
      _Out_ D3D11_VIDEO_DECODER_OUTPUT_VIEW_DESC *pDesc)
  {
    return m_pReal->GetDesc(pDesc);
  }
};

class WrappedID3D11VideoProcessor : public Wrapped11VideoDeviceChild<ID3D11VideoProcessor>
{
public:
  WrappedID3D11VideoProcessor(ID3D11VideoProcessor *real, WrappedID3D11Device *device)
      : Wrapped11VideoDeviceChild(real, device)
  {
  }
  //////////////////////////////
  // implement ID3D11VideoProcessor
  virtual void STDMETHODCALLTYPE GetContentDesc(
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_CONTENT_DESC *pDesc)
  {
    return m_pReal->GetContentDesc(pDesc);
  }

  virtual void STDMETHODCALLTYPE GetRateConversionCaps(
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS *pCaps)
  {
    return m_pReal->GetRateConversionCaps(pCaps);
  }
};

class WrappedID3D11VideoProcessorEnumerator1
    : public Wrapped11VideoDeviceChild<ID3D11VideoProcessorEnumerator, ID3D11VideoProcessorEnumerator1>
{
public:
  WrappedID3D11VideoProcessorEnumerator1(ID3D11VideoProcessorEnumerator *real,
                                         WrappedID3D11Device *device)
      : Wrapped11VideoDeviceChild(real, device)
  {
  }
  //////////////////////////////
  // implement ID3D11VideoProcessorEnumerator
  virtual HRESULT STDMETHODCALLTYPE GetVideoProcessorContentDesc(
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_CONTENT_DESC *pContentDesc)
  {
    return m_pReal->GetVideoProcessorContentDesc(pContentDesc);
  }

  virtual HRESULT STDMETHODCALLTYPE CheckVideoProcessorFormat(
      /* [annotation] */
      _In_ DXGI_FORMAT Format,
      /* [annotation] */
      _Out_ UINT *pFlags)
  {
    return m_pReal->CheckVideoProcessorFormat(Format, pFlags);
  }

  virtual HRESULT STDMETHODCALLTYPE GetVideoProcessorCaps(
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_CAPS *pCaps)
  {
    return m_pReal->GetVideoProcessorCaps(pCaps);
  }

  virtual HRESULT STDMETHODCALLTYPE GetVideoProcessorRateConversionCaps(
      /* [annotation] */
      _In_ UINT TypeIndex,
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_RATE_CONVERSION_CAPS *pCaps)
  {
    return m_pReal->GetVideoProcessorRateConversionCaps(TypeIndex, pCaps);
  }

  virtual HRESULT STDMETHODCALLTYPE GetVideoProcessorCustomRate(
      /* [annotation] */
      _In_ UINT TypeIndex,
      /* [annotation] */
      _In_ UINT CustomRateIndex,
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_CUSTOM_RATE *pRate)
  {
    return m_pReal->GetVideoProcessorCustomRate(TypeIndex, CustomRateIndex, pRate);
  }

  virtual HRESULT STDMETHODCALLTYPE GetVideoProcessorFilterRange(
      /* [annotation] */
      _In_ D3D11_VIDEO_PROCESSOR_FILTER Filter,
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_FILTER_RANGE *pRange)
  {
    return m_pReal->GetVideoProcessorFilterRange(Filter, pRange);
  }

  //////////////////////////////
  // implement ID3D11VideoProcessorEnumerator1
  virtual HRESULT STDMETHODCALLTYPE CheckVideoProcessorFormatConversion(
      /* [annotation] */
      _In_ DXGI_FORMAT InputFormat,
      /* [annotation] */
      _In_ DXGI_COLOR_SPACE_TYPE InputColorSpace,
      /* [annotation] */
      _In_ DXGI_FORMAT OutputFormat,
      /* [annotation] */
      _In_ DXGI_COLOR_SPACE_TYPE OutputColorSpace,
      /* [annotation] */
      _Out_ BOOL *pSupported)
  {
    ID3D11VideoProcessorEnumerator1 *real1 = NULL;
    HRESULT check =
        m_pReal->QueryInterface(__uuidof(ID3D11VideoProcessorEnumerator1), (void **)&real1);

    HRESULT ret = E_NOINTERFACE;

    if(SUCCEEDED(check) && real1)
      ret = real1->CheckVideoProcessorFormatConversion(InputFormat, InputColorSpace, OutputFormat,
                                                       OutputColorSpace, pSupported);

    SAFE_RELEASE(real1);
    return ret;
  }
};

class WrappedID3D11VideoProcessorInputView
    : public Wrapped11VideoDeviceChild<ID3D11VideoProcessorInputView>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11VideoProcessorInputView);

  WrappedID3D11VideoProcessorInputView(ID3D11VideoProcessorInputView *real,
                                       WrappedID3D11Device *device)
      : Wrapped11VideoDeviceChild(real, device)
  {
  }
  //////////////////////////////
  // implement ID3D11View
  virtual void STDMETHODCALLTYPE GetResource(
      /* [annotation] */
      _Outptr_ ID3D11Resource **ppResource);

  //////////////////////////////
  // implement ID3D11VideoProcessorInputView
  virtual void STDMETHODCALLTYPE GetDesc(
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC *pDesc)
  {
    return m_pReal->GetDesc(pDesc);
  }
};

class WrappedID3D11VideoProcessorOutputView
    : public Wrapped11VideoDeviceChild<ID3D11VideoProcessorOutputView>
{
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D11VideoProcessorOutputView);

  WrappedID3D11VideoProcessorOutputView(ID3D11VideoProcessorOutputView *real,
                                        WrappedID3D11Device *device)
      : Wrapped11VideoDeviceChild(real, device)
  {
  }
  //////////////////////////////
  // implement ID3D11View
  virtual void STDMETHODCALLTYPE GetResource(
      /* [annotation] */
      _Outptr_ ID3D11Resource **ppResource);

  //////////////////////////////
  // implement ID3D11VideoProcessorOutputView
  virtual void STDMETHODCALLTYPE GetDesc(
      /* [annotation] */
      _Out_ D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC *pDesc)
  {
    return m_pReal->GetDesc(pDesc);
  }
};

// differs from Unwrap because it doesn't check IsAlloc in development
template <typename Dest>
typename Dest::InnerType *VideoUnwrap(typename Dest::InnerType *obj)
{
  return obj == NULL ? NULL : (Dest::InnerType *)((Dest *)obj)->GetReal();
}

#define VIDEO_UNWRAP(type, obj) VideoUnwrap<type>(obj)
