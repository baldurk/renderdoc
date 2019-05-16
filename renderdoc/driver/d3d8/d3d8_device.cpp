/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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

#include "d3d8_device.h"
#include "core/core.h"
#include "driver/dxgi/dxgi_common.h"
#include "serialise/serialiser.h"
#include "d3d8_debug.h"
#include "d3d8_resources.h"

WrappedD3DDevice8::WrappedD3DDevice8(IDirect3DDevice8 *device, HWND wnd,
                                     D3DPRESENT_PARAMETERS *pPresentationParameters)
    : m_RefCounter(device, false),
      m_SoftRefCounter(NULL, false),
      m_device(device),
      m_DebugManager(NULL),
      m_PresentParameters(*pPresentationParameters)
{
  m_FrameCounter = 0;

  // refcounters implicitly construct with one reference, but we don't start with any soft
  // references.
  m_SoftRefCounter.Release();
  m_InternalRefcount = 0;
  m_Alive = true;

#if ENABLED(RDOC_RELEASE)
  const bool debugSerialiser = false;
#else
  const bool debugSerialiser = true;
#endif

  if(!RenderDoc::Inst().IsReplayApp())
  {
    m_State = CaptureState::BackgroundCapturing;

    RenderDoc::Inst().AddDeviceFrameCapturer((IDirect3DDevice8 *)this, this);

    m_Wnd = wnd;

    if(wnd != NULL)
      RenderDoc::Inst().AddFrameCapturer((IDirect3DDevice8 *)this, wnd, this);
  }
  else
  {
    m_State = CaptureState::LoadingReplaying;

    m_Wnd = NULL;
  }

  m_ResourceManager = new D3D8ResourceManager(this);
}

void WrappedD3DDevice8::CheckForDeath()
{
  if(!m_Alive)
    return;

  if(m_RefCounter.GetRefCount() == 0)
  {
    RDCASSERT(m_SoftRefCounter.GetRefCount() >= m_InternalRefcount);

    if(m_SoftRefCounter.GetRefCount() <= m_InternalRefcount)
    {
      m_Alive = false;
      delete this;
    }
  }
}

WrappedD3DDevice8::~WrappedD3DDevice8()
{
  RenderDoc::Inst().RemoveDeviceFrameCapturer((IDirect3DDevice8 *)this);

  if(m_Wnd != NULL)
    RenderDoc::Inst().RemoveFrameCapturer((IDirect3DDevice8 *)this, m_Wnd);

  SAFE_DELETE(m_DebugManager);

  m_ResourceManager->Shutdown();

  SAFE_DELETE(m_ResourceManager);

  SAFE_RELEASE(m_device);

  RDCASSERT(WrappedIDirect3DVertexBuffer8::m_BufferList.empty());
  RDCASSERT(WrappedIDirect3DIndexBuffer8::m_BufferList.empty());
}

bool WrappedD3DDevice8::Serialise_ReleaseResource(IDirect3DResource8 *res)
{
  return true;
}

void WrappedD3DDevice8::ReleaseResource(IDirect3DResource8 *res)
{
  ResourceId id = GetResID((IUnknown *)res);

  D3D8ResourceRecord *record = GetResourceManager()->GetResourceRecord(id);

  if(record)
    record->Delete(GetResourceManager());

  // wrapped resources get released all the time, we don't want to
  // try and slerp in a resource release. Just the explicit ones
  if(IsReplayMode(m_State))
  {
    if(GetResourceManager()->HasLiveResource(id))
      GetResourceManager()->EraseLiveResource(id);
    return;
  }
}

HRESULT WrappedD3DDevice8::QueryInterface(REFIID riid, void **ppvObject)
{
  // RenderDoc UUID {A7AA6116-9C8D-4BBA-9083-B4D816B71B78}
  static const GUID IRenderDoc_uuid = {
      0xa7aa6116, 0x9c8d, 0x4bba, {0x90, 0x83, 0xb4, 0xd8, 0x16, 0xb7, 0x1b, 0x78}};

  if(riid == IRenderDoc_uuid)
  {
    AddRef();
    *ppvObject = (IUnknown *)this;
    return S_OK;
  }
  else
  {
    WarnUnknownGUID("IDirect3DDevice8", riid);
  }

  return m_device->QueryInterface(riid, ppvObject);
}

void WrappedD3DDevice8::LazyInit()
{
  m_DebugManager = new D3D8DebugManager(this);
}

void WrappedD3DDevice8::StartFrameCapture(void *dev, void *wnd)
{
  RDCERR("Capture not supported on D3D8");
}

bool WrappedD3DDevice8::EndFrameCapture(void *dev, void *wnd)
{
  RDCERR("Capture not supported on D3D8");
  return false;
}

bool WrappedD3DDevice8::DiscardFrameCapture(void *dev, void *wnd)
{
  RDCERR("Capture not supported on D3D8");
  return false;
}

HRESULT __stdcall WrappedD3DDevice8::TestCooperativeLevel()
{
  return m_device->TestCooperativeLevel();
}

UINT __stdcall WrappedD3DDevice8::GetAvailableTextureMem()
{
  return m_device->GetAvailableTextureMem();
}

HRESULT __stdcall WrappedD3DDevice8::ResourceManagerDiscardBytes(DWORD Bytes)
{
  return m_device->ResourceManagerDiscardBytes(Bytes);
}

HRESULT __stdcall WrappedD3DDevice8::GetDirect3D(IDirect3D8 **ppD3D8)
{
  return m_device->GetDirect3D(ppD3D8);
}

HRESULT __stdcall WrappedD3DDevice8::GetDeviceCaps(D3DCAPS8 *pCaps)
{
  return m_device->GetDeviceCaps(pCaps);
}

HRESULT __stdcall WrappedD3DDevice8::GetDisplayMode(D3DDISPLAYMODE *pMode)
{
  return m_device->GetDisplayMode(pMode);
}

HRESULT __stdcall WrappedD3DDevice8::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
  return m_device->GetCreationParameters(pParameters);
}

HRESULT __stdcall WrappedD3DDevice8::SetCursorProperties(UINT XHotSpot, UINT YHotSpot,
                                                         IDirect3DSurface8 *pCursorBitmap)
{
  return m_device->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
}

void __stdcall WrappedD3DDevice8::SetCursorPosition(int X, int Y, DWORD Flags)
{
  m_device->SetCursorPosition(X, Y, Flags);
}

BOOL __stdcall WrappedD3DDevice8::ShowCursor(BOOL bShow)
{
  return m_device->ShowCursor(bShow);
}

HRESULT __stdcall WrappedD3DDevice8::CreateAdditionalSwapChain(
    D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DSwapChain8 **pSwapChain)
{
  return m_device->CreateAdditionalSwapChain(pPresentationParameters, pSwapChain);
}

HRESULT __stdcall WrappedD3DDevice8::Reset(D3DPRESENT_PARAMETERS *pPresentationParameters)
{
  m_PresentParameters = *pPresentationParameters;
  return m_device->Reset(pPresentationParameters);
}

HRESULT __stdcall WrappedD3DDevice8::Present(CONST RECT *pSourceRect, CONST RECT *pDestRect,
                                             HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion)
{
  // if(m_State == WRITING_IDLE)
  RenderDoc::Inst().Tick();

  HWND wnd = m_PresentParameters.hDeviceWindow;
  if(hDestWindowOverride != NULL)
    wnd = hDestWindowOverride;

  bool activeWindow = RenderDoc::Inst().IsActiveWindow((IDirect3DDevice8 *)this, wnd);

  m_FrameCounter++;

  // if (m_State == WRITING_IDLE)
  {
    uint32_t overlay = RenderDoc::Inst().GetOverlayBits();

    static bool debugRenderOverlay = true;

    if(overlay & eRENDERDOC_Overlay_Enabled && debugRenderOverlay)
    {
      HRESULT res = S_OK;
      res = m_device->BeginScene();
      DWORD stateBlock;
      HRESULT stateBlockRes = m_device->CreateStateBlock(D3DSBT_ALL, &stateBlock);

      IDirect3DSurface8 *backBuffer;
      res |= m_device->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
      res |= m_device->SetRenderTarget(0, backBuffer);

      D3DSURFACE_DESC bbDesc;
      backBuffer->GetDesc(&bbDesc);

      //
      D3DVIEWPORT8 viewport = {0, 0, bbDesc.Width, bbDesc.Height, 0.f, 1.f};
      res |= m_device->SetViewport(&viewport);

      GetDebugManager()->SetOutputDimensions(bbDesc.Width, bbDesc.Height);
      GetDebugManager()->SetOutputWindow(m_PresentParameters.hDeviceWindow);

      int flags = activeWindow ? RenderDoc::eOverlay_ActiveWindow : 0;
      flags |= RenderDoc::eOverlay_CaptureDisabled;

      std::string overlayText =
          RenderDoc::Inst().GetOverlayText(RDCDriver::D3D8, m_FrameCounter, flags);

      overlayText += "Captures not supported with D3D8\n";

      if(!overlayText.empty())
        GetDebugManager()->RenderText(0.0f, 0.0f, overlayText.c_str());

      stateBlockRes = m_device->ApplyStateBlock(stateBlock);
      res |= m_device->EndScene();
    }
  }

  RenderDoc::Inst().AddActiveDriver(RDCDriver::D3D8, true);

  return m_device->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT __stdcall WrappedD3DDevice8::GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type,
                                                   IDirect3DSurface8 **ppBackBuffer)
{
  return m_device->GetBackBuffer(iBackBuffer, Type, ppBackBuffer);
}

HRESULT __stdcall WrappedD3DDevice8::GetRasterStatus(D3DRASTER_STATUS *pRasterStatus)
{
  return m_device->GetRasterStatus(pRasterStatus);
}

void __stdcall WrappedD3DDevice8::SetGammaRamp(DWORD Flags, CONST D3DGAMMARAMP *pRamp)
{
  m_device->SetGammaRamp(Flags, pRamp);
}

void __stdcall WrappedD3DDevice8::GetGammaRamp(D3DGAMMARAMP *pRamp)
{
  m_device->GetGammaRamp(pRamp);
}

HRESULT __stdcall WrappedD3DDevice8::CreateTexture(UINT Width, UINT Height, UINT Levels,
                                                   DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                                   IDirect3DTexture8 **ppTexture)
{
  return m_device->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture);
}

HRESULT __stdcall WrappedD3DDevice8::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth,
                                                         UINT Levels, DWORD Usage, D3DFORMAT Format,
                                                         D3DPOOL Pool,
                                                         IDirect3DVolumeTexture8 **ppVolumeTexture)
{
  return m_device->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool,
                                       ppVolumeTexture);
}

HRESULT __stdcall WrappedD3DDevice8::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage,
                                                       D3DFORMAT Format, D3DPOOL Pool,
                                                       IDirect3DCubeTexture8 **ppCubeTexture)
{
  return m_device->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture);
}

HRESULT __stdcall WrappedD3DDevice8::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF,
                                                        D3DPOOL Pool,
                                                        IDirect3DVertexBuffer8 **ppVertexBuffer)
{
  IDirect3DVertexBuffer8 *real = NULL;
  IDirect3DVertexBuffer8 *wrapped = NULL;
  HRESULT ret = m_device->CreateVertexBuffer(Length, Usage, FVF, Pool, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedIDirect3DVertexBuffer8(real, Length, this);

    if(IsCaptureMode(m_State))
    {
      // TODO: Serialise
    }
    else
    {
      WrappedIDirect3DVertexBuffer8 *w = (WrappedIDirect3DVertexBuffer8 *)wrapped;

      m_ResourceManager->AddLiveResource(w->GetResourceID(), wrapped);
    }

    *ppVertexBuffer = wrapped;
  }

  return ret;
}

HRESULT __stdcall WrappedD3DDevice8::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format,
                                                       D3DPOOL Pool,
                                                       IDirect3DIndexBuffer8 **ppIndexBuffer)
{
  IDirect3DIndexBuffer8 *real = NULL;
  IDirect3DIndexBuffer8 *wrapped = NULL;
  HRESULT ret = m_device->CreateIndexBuffer(Length, Usage, Format, Pool, &real);

  if(SUCCEEDED(ret))
  {
    SCOPED_LOCK(m_D3DLock);

    wrapped = new WrappedIDirect3DIndexBuffer8(real, Length, this);

    if(IsCaptureMode(m_State))
    {
      // TODO: Serialise
    }
    else
    {
      WrappedIDirect3DIndexBuffer8 *w = (WrappedIDirect3DIndexBuffer8 *)wrapped;

      m_ResourceManager->AddLiveResource(w->GetResourceID(), wrapped);
    }

    *ppIndexBuffer = wrapped;
  }

  return ret;
}

HRESULT __stdcall WrappedD3DDevice8::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format,
                                                        D3DMULTISAMPLE_TYPE MultiSample,
                                                        BOOL Lockable, IDirect3DSurface8 **ppSurface)
{
  return m_device->CreateRenderTarget(Width, Height, Format, MultiSample, Lockable, ppSurface);
}

HRESULT __stdcall WrappedD3DDevice8::CreateDepthStencilSurface(UINT Width, UINT Height,
                                                               D3DFORMAT Format,
                                                               D3DMULTISAMPLE_TYPE MultiSample,
                                                               IDirect3DSurface8 **ppSurface)
{
  return m_device->CreateDepthStencilSurface(Width, Height, Format, MultiSample, ppSurface);
}

HRESULT __stdcall WrappedD3DDevice8::CreateImageSurface(UINT Width, UINT Height, D3DFORMAT Format,
                                                        IDirect3DSurface8 **ppSurface)
{
  return m_device->CreateImageSurface(Width, Height, Format, ppSurface);
}

HRESULT __stdcall WrappedD3DDevice8::CopyRects(IDirect3DSurface8 *pSourceSurface,
                                               CONST RECT *pSourceRectsArray, UINT NumRects,
                                               IDirect3DSurface8 *pDestinationSurface,
                                               CONST POINT *pDestPointsArray)
{
  return m_device->CopyRects(pSourceSurface, pSourceRectsArray, NumRects, pDestinationSurface,
                             pDestPointsArray);
}

HRESULT __stdcall WrappedD3DDevice8::UpdateTexture(IDirect3DBaseTexture8 *pSourceTexture,
                                                   IDirect3DBaseTexture8 *pDestinationTexture)
{
  return m_device->UpdateTexture(pSourceTexture, pDestinationTexture);
}

HRESULT __stdcall WrappedD3DDevice8::GetFrontBuffer(IDirect3DSurface8 *pDestSurface)
{
  return m_device->GetFrontBuffer(pDestSurface);
}

HRESULT __stdcall WrappedD3DDevice8::SetRenderTarget(IDirect3DSurface8 *pRenderTarget,
                                                     IDirect3DSurface8 *pNewZStencil)
{
  return m_device->SetRenderTarget(pRenderTarget, pNewZStencil);
}

HRESULT __stdcall WrappedD3DDevice8::GetRenderTarget(IDirect3DSurface8 **ppRenderTarget)
{
  return m_device->GetRenderTarget(ppRenderTarget);
}

HRESULT __stdcall WrappedD3DDevice8::GetDepthStencilSurface(IDirect3DSurface8 **ppZStencilSurface)
{
  return m_device->GetDepthStencilSurface(ppZStencilSurface);
}

HRESULT __stdcall WrappedD3DDevice8::BeginScene()
{
  return m_device->BeginScene();
}

HRESULT __stdcall WrappedD3DDevice8::EndScene()
{
  return m_device->EndScene();
}

HRESULT __stdcall WrappedD3DDevice8::Clear(DWORD Count, CONST D3DRECT *pRects, DWORD Flags,
                                           D3DCOLOR Color, float Z, DWORD Stencil)
{
  return m_device->Clear(Count, pRects, Flags, Color, Z, Stencil);
}

HRESULT __stdcall WrappedD3DDevice8::SetTransform(D3DTRANSFORMSTATETYPE State,
                                                  CONST D3DMATRIX *pMatrix)
{
  return m_device->SetTransform(State, pMatrix);
}

HRESULT __stdcall WrappedD3DDevice8::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix)
{
  return m_device->GetTransform(State, pMatrix);
}

HRESULT __stdcall WrappedD3DDevice8::MultiplyTransform(D3DTRANSFORMSTATETYPE _arg1,
                                                       CONST D3DMATRIX *_arg2)
{
  return m_device->MultiplyTransform(_arg1, _arg2);
}

HRESULT __stdcall WrappedD3DDevice8::SetViewport(CONST D3DVIEWPORT8 *pViewport)
{
  return m_device->SetViewport(pViewport);
}

HRESULT __stdcall WrappedD3DDevice8::GetViewport(D3DVIEWPORT8 *pViewport)
{
  return m_device->GetViewport(pViewport);
}

HRESULT __stdcall WrappedD3DDevice8::SetMaterial(CONST D3DMATERIAL8 *pMaterial)
{
  return m_device->SetMaterial(pMaterial);
}

HRESULT __stdcall WrappedD3DDevice8::GetMaterial(D3DMATERIAL8 *pMaterial)
{
  return m_device->GetMaterial(pMaterial);
}

HRESULT __stdcall WrappedD3DDevice8::SetLight(DWORD Index, CONST D3DLIGHT8 *_arg2)
{
  return m_device->SetLight(Index, _arg2);
}

HRESULT __stdcall WrappedD3DDevice8::GetLight(DWORD Index, D3DLIGHT8 *_arg2)
{
  return m_device->GetLight(Index, _arg2);
}

HRESULT __stdcall WrappedD3DDevice8::LightEnable(DWORD Index, BOOL Enable)
{
  return m_device->LightEnable(Index, Enable);
}

HRESULT __stdcall WrappedD3DDevice8::GetLightEnable(DWORD Index, BOOL *pEnable)
{
  return m_device->GetLightEnable(Index, pEnable);
}

HRESULT __stdcall WrappedD3DDevice8::SetClipPlane(DWORD Index, CONST float *pPlane)
{
  return m_device->SetClipPlane(Index, pPlane);
}

HRESULT __stdcall WrappedD3DDevice8::GetClipPlane(DWORD Index, float *pPlane)
{
  return m_device->GetClipPlane(Index, pPlane);
}

HRESULT __stdcall WrappedD3DDevice8::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{
  return m_device->SetRenderState(State, Value);
}

HRESULT __stdcall WrappedD3DDevice8::GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue)
{
  return m_device->GetRenderState(State, pValue);
}

HRESULT __stdcall WrappedD3DDevice8::BeginStateBlock()
{
  return m_device->BeginStateBlock();
}

HRESULT __stdcall WrappedD3DDevice8::EndStateBlock(DWORD *pToken)
{
  return m_device->EndStateBlock(pToken);
}

HRESULT __stdcall WrappedD3DDevice8::ApplyStateBlock(DWORD Token)
{
  return m_device->ApplyStateBlock(Token);
}

HRESULT __stdcall WrappedD3DDevice8::CaptureStateBlock(DWORD Token)
{
  return m_device->CaptureStateBlock(Token);
}

HRESULT __stdcall WrappedD3DDevice8::DeleteStateBlock(DWORD Token)
{
  return m_device->DeleteStateBlock(Token);
}

HRESULT __stdcall WrappedD3DDevice8::CreateStateBlock(D3DSTATEBLOCKTYPE Type, DWORD *pToken)
{
  return m_device->CreateStateBlock(Type, pToken);
}

HRESULT __stdcall WrappedD3DDevice8::SetClipStatus(CONST D3DCLIPSTATUS8 *pClipStatus)
{
  return m_device->SetClipStatus(pClipStatus);
}

HRESULT __stdcall WrappedD3DDevice8::GetClipStatus(D3DCLIPSTATUS8 *pClipStatus)
{
  return m_device->GetClipStatus(pClipStatus);
}

HRESULT __stdcall WrappedD3DDevice8::GetTexture(DWORD Stage, IDirect3DBaseTexture8 **ppTexture)
{
  return m_device->GetTexture(Stage, ppTexture);
}

HRESULT __stdcall WrappedD3DDevice8::SetTexture(DWORD Stage, IDirect3DBaseTexture8 *pTexture)
{
  return m_device->SetTexture(Stage, pTexture);
}

HRESULT __stdcall WrappedD3DDevice8::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type,
                                                          DWORD *pValue)
{
  return m_device->GetTextureStageState(Stage, Type, pValue);
}

HRESULT __stdcall WrappedD3DDevice8::SetTextureStageState(DWORD Stage,
                                                          D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
  return m_device->SetTextureStageState(Stage, Type, Value);
}

HRESULT __stdcall WrappedD3DDevice8::ValidateDevice(DWORD *pNumPasses)
{
  return m_device->ValidateDevice(pNumPasses);
}

HRESULT __stdcall WrappedD3DDevice8::GetInfo(DWORD DevInfoID, void *pDevInfoStruct,
                                             DWORD DevInfoStructSize)
{
  return m_device->GetInfo(DevInfoID, pDevInfoStruct, DevInfoStructSize);
}

HRESULT __stdcall WrappedD3DDevice8::SetPaletteEntries(UINT PaletteNumber,
                                                       CONST PALETTEENTRY *pEntries)
{
  return m_device->SetPaletteEntries(PaletteNumber, pEntries);
}

HRESULT __stdcall WrappedD3DDevice8::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY *pEntries)
{
  return m_device->GetPaletteEntries(PaletteNumber, pEntries);
}

HRESULT __stdcall WrappedD3DDevice8::SetCurrentTexturePalette(UINT PaletteNumber)
{
  return m_device->SetCurrentTexturePalette(PaletteNumber);
}

HRESULT __stdcall WrappedD3DDevice8::GetCurrentTexturePalette(UINT *PaletteNumber)
{
  return m_device->GetCurrentTexturePalette(PaletteNumber);
}

HRESULT __stdcall WrappedD3DDevice8::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex,
                                                   UINT PrimitiveCount)
{
  return m_device->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}

HRESULT __stdcall WrappedD3DDevice8::DrawIndexedPrimitive(D3DPRIMITIVETYPE _arg1,
                                                          UINT MinVertexIndex, UINT NumVertices,
                                                          UINT startIndex, UINT primCount)
{
  return m_device->DrawIndexedPrimitive(_arg1, MinVertexIndex, NumVertices, startIndex, primCount);
}

HRESULT __stdcall WrappedD3DDevice8::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,
                                                     UINT PrimitiveCount,
                                                     CONST void *pVertexStreamZeroData,
                                                     UINT VertexStreamZeroStride)
{
  return m_device->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData,
                                   VertexStreamZeroStride);
}

HRESULT __stdcall WrappedD3DDevice8::DrawIndexedPrimitiveUP(
    D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount,
    CONST void *pIndexData, D3DFORMAT IndexDataFormat, CONST void *pVertexStreamZeroData,
    UINT VertexStreamZeroStride)
{
  return m_device->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices,
                                          PrimitiveCount, pIndexData, IndexDataFormat,
                                          pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT __stdcall WrappedD3DDevice8::ProcessVertices(UINT SrcStartIndex, UINT DestIndex,
                                                     UINT VertexCount,
                                                     IDirect3DVertexBuffer8 *pDestBuffer, DWORD Flags)
{
  return m_device->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, Flags);
}

HRESULT __stdcall WrappedD3DDevice8::CreateVertexShader(CONST DWORD *pDeclaration,
                                                        CONST DWORD *pFunction, DWORD *pHandle,
                                                        DWORD Usage)
{
  return m_device->CreateVertexShader(pDeclaration, pFunction, pHandle, Usage);
}

HRESULT __stdcall WrappedD3DDevice8::SetVertexShader(DWORD Handle)
{
  return m_device->SetVertexShader(Handle);
}

HRESULT __stdcall WrappedD3DDevice8::GetVertexShader(DWORD *pHandle)
{
  return m_device->GetVertexShader(pHandle);
}

HRESULT __stdcall WrappedD3DDevice8::DeleteVertexShader(DWORD Handle)
{
  return m_device->DeleteVertexShader(Handle);
}

HRESULT __stdcall WrappedD3DDevice8::SetVertexShaderConstant(DWORD Register,
                                                             CONST void *pConstantData,
                                                             DWORD ConstantCount)
{
  return m_device->SetVertexShaderConstant(Register, pConstantData, ConstantCount);
}

HRESULT __stdcall WrappedD3DDevice8::GetVertexShaderConstant(DWORD Register, void *pConstantData,
                                                             DWORD ConstantCount)
{
  return m_device->GetVertexShaderConstant(Register, pConstantData, ConstantCount);
}

HRESULT __stdcall WrappedD3DDevice8::GetVertexShaderDeclaration(DWORD Handle, void *pData,
                                                                DWORD *pSizeOfData)
{
  return m_device->GetVertexShaderDeclaration(Handle, pData, pSizeOfData);
}

HRESULT __stdcall WrappedD3DDevice8::GetVertexShaderFunction(DWORD Handle, void *pData,
                                                             DWORD *pSizeOfData)
{
  return m_device->GetVertexShaderFunction(Handle, pData, pSizeOfData);
}

HRESULT __stdcall WrappedD3DDevice8::SetStreamSource(UINT StreamNumber,
                                                     IDirect3DVertexBuffer8 *pStreamData, UINT Stride)
{
  return m_device->SetStreamSource(StreamNumber, Unwrap(pStreamData), Stride);
}

HRESULT __stdcall WrappedD3DDevice8::GetStreamSource(UINT StreamNumber,
                                                     IDirect3DVertexBuffer8 **ppStreamData,
                                                     UINT *pStride)
{
  IDirect3DVertexBuffer8 *real;
  HRESULT ret = m_device->GetStreamSource(StreamNumber, &real, pStride);

  if(SUCCEEDED(ret))
  {
    SAFE_RELEASE_NOCLEAR(real);
    *ppStreamData = (IDirect3DVertexBuffer8 *)GetResourceManager()->GetWrapper(real);
    SAFE_ADDREF(*ppStreamData);
  }

  return ret;
}

HRESULT __stdcall WrappedD3DDevice8::SetIndices(IDirect3DIndexBuffer8 *pIndexData,
                                                UINT BaseVertexIndex)
{
  return m_device->SetIndices(Unwrap(pIndexData), BaseVertexIndex);
}

HRESULT __stdcall WrappedD3DDevice8::GetIndices(IDirect3DIndexBuffer8 **ppIndexData,
                                                UINT *pBaseVertexIndex)
{
  IDirect3DIndexBuffer8 *real;
  HRESULT ret = m_device->GetIndices(&real, pBaseVertexIndex);

  if(SUCCEEDED(ret))
  {
    SAFE_RELEASE_NOCLEAR(real);
    *ppIndexData = (IDirect3DIndexBuffer8 *)GetResourceManager()->GetWrapper(real);
    SAFE_ADDREF(*ppIndexData);
  }

  return ret;
}

HRESULT __stdcall WrappedD3DDevice8::CreatePixelShader(CONST DWORD *pFunction, DWORD *pHandle)
{
  return m_device->CreatePixelShader(pFunction, pHandle);
}

HRESULT __stdcall WrappedD3DDevice8::SetPixelShader(DWORD Handle)
{
  return m_device->SetPixelShader(Handle);
}

HRESULT __stdcall WrappedD3DDevice8::GetPixelShader(DWORD *pHandle)
{
  return m_device->GetPixelShader(pHandle);
}

HRESULT __stdcall WrappedD3DDevice8::DeletePixelShader(DWORD Handle)
{
  return m_device->DeletePixelShader(Handle);
}

HRESULT __stdcall WrappedD3DDevice8::SetPixelShaderConstant(DWORD Register, CONST void *pConstantData,
                                                            DWORD ConstantCount)
{
  return m_device->SetPixelShaderConstant(Register, pConstantData, ConstantCount);
}

HRESULT __stdcall WrappedD3DDevice8::GetPixelShaderConstant(DWORD Register, void *pConstantData,
                                                            DWORD ConstantCount)
{
  return m_device->GetPixelShaderConstant(Register, pConstantData, ConstantCount);
}

HRESULT __stdcall WrappedD3DDevice8::GetPixelShaderFunction(DWORD Handle, void *pData,
                                                            DWORD *pSizeOfData)
{
  return m_device->GetPixelShaderFunction(Handle, pData, pSizeOfData);
}

HRESULT __stdcall WrappedD3DDevice8::DrawRectPatch(UINT Handle, CONST float *pNumSegs,
                                                   CONST D3DRECTPATCH_INFO *pRectPatchInfo)
{
  return m_device->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
}

HRESULT __stdcall WrappedD3DDevice8::DrawTriPatch(UINT Handle, CONST float *pNumSegs,
                                                  CONST D3DTRIPATCH_INFO *pTriPatchInfo)
{
  return m_device->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
}

HRESULT __stdcall WrappedD3DDevice8::DeletePatch(UINT Handle)
{
  return m_device->DeletePatch(Handle);
}

HRESULT __stdcall WrappedD3D8::QueryInterface(REFIID riid, void **ppvObj)
{
  return m_direct3D->QueryInterface(riid, ppvObj);
}

ULONG __stdcall WrappedD3D8::AddRef()
{
  ULONG refCount;
  refCount = m_direct3D->AddRef();
  return refCount;
}

ULONG __stdcall WrappedD3D8::Release()
{
  ULONG refCount = m_direct3D->Release();
  if(refCount == 0)
  {
    delete this;
  }
  return refCount;
}

HRESULT __stdcall WrappedD3D8::RegisterSoftwareDevice(void *pInitializeFunction)
{
  return m_direct3D->RegisterSoftwareDevice(pInitializeFunction);
}

UINT __stdcall WrappedD3D8::GetAdapterCount()
{
  return m_direct3D->GetAdapterCount();
}

HRESULT __stdcall WrappedD3D8::GetAdapterIdentifier(UINT Adapter, DWORD Flags,
                                                    D3DADAPTER_IDENTIFIER8 *pIdentifier)
{
  return m_direct3D->GetAdapterIdentifier(Adapter, Flags, pIdentifier);
}

UINT __stdcall WrappedD3D8::GetAdapterModeCount(UINT Adapter)
{
  return m_direct3D->GetAdapterModeCount(Adapter);
}

HRESULT __stdcall WrappedD3D8::EnumAdapterModes(UINT Adapter, UINT Mode, D3DDISPLAYMODE *pMode)
{
  return m_direct3D->EnumAdapterModes(Adapter, Mode, pMode);
}

HRESULT __stdcall WrappedD3D8::GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE *pMode)
{
  return m_direct3D->GetAdapterDisplayMode(Adapter, pMode);
}

HRESULT __stdcall WrappedD3D8::CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType,
                                               D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                               BOOL bWindowed)
{
  return m_direct3D->CheckDeviceType(Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed);
}

HRESULT __stdcall WrappedD3D8::CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType,
                                                 D3DFORMAT AdapterFormat, DWORD Usage,
                                                 D3DRESOURCETYPE RType, D3DFORMAT CheckFormat)
{
  return m_direct3D->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
}

HRESULT __stdcall WrappedD3D8::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType,
                                                          D3DFORMAT SurfaceFormat, BOOL Windowed,
                                                          D3DMULTISAMPLE_TYPE MultiSampleType)
{
  return m_direct3D->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed,
                                                MultiSampleType);
}

HRESULT __stdcall WrappedD3D8::CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType,
                                                      D3DFORMAT AdapterFormat,
                                                      D3DFORMAT RenderTargetFormat,
                                                      D3DFORMAT DepthStencilFormat)
{
  return m_direct3D->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat,
                                            DepthStencilFormat);
}

HRESULT __stdcall WrappedD3D8::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS8 *pCaps)
{
  return m_direct3D->GetDeviceCaps(Adapter, DeviceType, pCaps);
}

HMONITOR __stdcall WrappedD3D8::GetAdapterMonitor(UINT Adapter)
{
  return m_direct3D->GetAdapterMonitor(Adapter);
}

HRESULT __stdcall WrappedD3D8::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
                                            DWORD BehaviorFlags,
                                            D3DPRESENT_PARAMETERS *pPresentationParameters,
                                            IDirect3DDevice8 **ppReturnedDeviceInterface)
{
  IDirect3DDevice8 *device = NULL;
  HRESULT res = m_direct3D->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags,
                                         pPresentationParameters, &device);
  if(res == S_OK)
  {
    RDCLOG("App creating d3d8 device");

    HWND wnd = pPresentationParameters->hDeviceWindow;
    if(wnd == NULL)
      wnd = hFocusWindow;

    if(!wnd)
      RDCWARN("Couldn't find valid non-NULL window at CreateDevice time");

    WrappedD3DDevice8 *wrappedDevice = new WrappedD3DDevice8(device, wnd, pPresentationParameters);
    wrappedDevice->LazyInit();    // TODO this can be moved later probably
    *ppReturnedDeviceInterface = wrappedDevice;
  }
  else
  {
    *ppReturnedDeviceInterface = NULL;
  }
  return res;
}
