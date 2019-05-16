/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2016-2019 Baldur Karlsson
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

#include "d3d9_device.h"
#include "core/core.h"
#include "driver/dxgi/dxgi_common.h"
#include "serialise/serialiser.h"
#include "d3d9_debug.h"

WrappedD3DDevice9::WrappedD3DDevice9(IDirect3DDevice9 *device, HWND wnd)
    : m_RefCounter(device, false),
      m_SoftRefCounter(NULL, false),
      m_device(device),
      m_DebugManager(NULL)
{
  m_FrameCounter = 0;

  // refcounters implicitly construct with one reference, but we don't start with any soft
  // references.
  m_SoftRefCounter.Release();
  m_InternalRefcount = 0;
  m_Alive = true;

  if(!RenderDoc::Inst().IsReplayApp())
  {
    RenderDoc::Inst().AddDeviceFrameCapturer((IDirect3DDevice9 *)this, this);

    m_Wnd = wnd;

    if(wnd != NULL)
      RenderDoc::Inst().AddFrameCapturer((IDirect3DDevice9 *)this, wnd, this);
  }
  else
  {
    m_Wnd = NULL;
  }
}

void WrappedD3DDevice9::CheckForDeath()
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

WrappedD3DDevice9::~WrappedD3DDevice9()
{
  RenderDoc::Inst().RemoveDeviceFrameCapturer((IDirect3DDevice9 *)this);

  if(m_Wnd != NULL)
    RenderDoc::Inst().RemoveFrameCapturer((IDirect3DDevice9 *)this, m_Wnd);

  SAFE_DELETE(m_DebugManager);

  SAFE_RELEASE(m_device);
}

HRESULT WrappedD3DDevice9::QueryInterface(REFIID riid, void **ppvObject)
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
    WarnUnknownGUID("IDirect3DDevice9", riid);
  }

  return m_device->QueryInterface(riid, ppvObject);
}

void WrappedD3DDevice9::LazyInit()
{
  m_DebugManager = new D3D9DebugManager(this);
}

void WrappedD3DDevice9::StartFrameCapture(void *dev, void *wnd)
{
  RDCERR("Capture not supported on D3D9");
}

bool WrappedD3DDevice9::EndFrameCapture(void *dev, void *wnd)
{
  RDCERR("Capture not supported on D3D9");
  return false;
}

bool WrappedD3DDevice9::DiscardFrameCapture(void *dev, void *wnd)
{
  RDCERR("Capture not supported on D3D9");
  return false;
}

HRESULT __stdcall WrappedD3DDevice9::TestCooperativeLevel()
{
  return m_device->TestCooperativeLevel();
}

UINT __stdcall WrappedD3DDevice9::GetAvailableTextureMem()
{
  return m_device->GetAvailableTextureMem();
}

HRESULT __stdcall WrappedD3DDevice9::EvictManagedResources()
{
  return m_device->EvictManagedResources();
}

HRESULT __stdcall WrappedD3DDevice9::GetDirect3D(IDirect3D9 **ppD3D9)
{
  return m_device->GetDirect3D(ppD3D9);
}

HRESULT __stdcall WrappedD3DDevice9::GetDeviceCaps(D3DCAPS9 *pCaps)
{
  return m_device->GetDeviceCaps(pCaps);
}

HRESULT __stdcall WrappedD3DDevice9::GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE *pMode)
{
  return m_device->GetDisplayMode(iSwapChain, pMode);
}

HRESULT __stdcall WrappedD3DDevice9::GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
  return m_device->GetCreationParameters(pParameters);
}

HRESULT __stdcall WrappedD3DDevice9::SetCursorProperties(UINT XHotSpot, UINT YHotSpot,
                                                         IDirect3DSurface9 *pCursorBitmap)
{
  return m_device->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
}

void __stdcall WrappedD3DDevice9::SetCursorPosition(int X, int Y, DWORD Flags)
{
  m_device->SetCursorPosition(X, Y, Flags);
}

BOOL __stdcall WrappedD3DDevice9::ShowCursor(BOOL bShow)
{
  return m_device->ShowCursor(bShow);
}

HRESULT __stdcall WrappedD3DDevice9::CreateAdditionalSwapChain(
    D3DPRESENT_PARAMETERS *pPresentationParameters, IDirect3DSwapChain9 **pSwapChain)
{
  return m_device->CreateAdditionalSwapChain(pPresentationParameters, pSwapChain);
}

HRESULT __stdcall WrappedD3DDevice9::GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9 **pSwapChain)
{
  return m_device->GetSwapChain(iSwapChain, pSwapChain);
}

UINT __stdcall WrappedD3DDevice9::GetNumberOfSwapChains()
{
  return m_device->GetNumberOfSwapChains();
}

HRESULT __stdcall WrappedD3DDevice9::Reset(D3DPRESENT_PARAMETERS *pPresentationParameters)
{
  return m_device->Reset(pPresentationParameters);
}

HRESULT __stdcall WrappedD3DDevice9::Present(CONST RECT *pSourceRect, CONST RECT *pDestRect,
                                             HWND hDestWindowOverride, CONST RGNDATA *pDirtyRegion)
{
  // if(m_State == WRITING_IDLE)
  RenderDoc::Inst().Tick();

  IDirect3DSwapChain9 *swapChain = NULL;
  m_device->GetSwapChain(0, &swapChain);
  D3DPRESENT_PARAMETERS presentParams = {};
  if(swapChain)
    swapChain->GetPresentParameters(&presentParams);

  HWND wnd = presentParams.hDeviceWindow;
  if(hDestWindowOverride != NULL)
    wnd = hDestWindowOverride;

  bool activeWindow = RenderDoc::Inst().IsActiveWindow((IDirect3DDevice9 *)this, wnd);

  m_FrameCounter++;

  // if (m_State == WRITING_IDLE)
  if(wnd != NULL)
  {
    uint32_t overlay = RenderDoc::Inst().GetOverlayBits();

    if(overlay & eRENDERDOC_Overlay_Enabled)
    {
      HRESULT res = S_OK;
      res = m_device->BeginScene();
      IDirect3DStateBlock9 *stateBlock;
      HRESULT stateBlockRes = m_device->CreateStateBlock(D3DSBT_ALL, &stateBlock);

      IDirect3DSurface9 *backBuffer;
      res |= m_device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
      res |= m_device->SetRenderTarget(0, backBuffer);

      D3DSURFACE_DESC bbDesc;
      backBuffer->GetDesc(&bbDesc);

      //
      D3DVIEWPORT9 viewport = {0, 0, bbDesc.Width, bbDesc.Height, 0.f, 1.f};
      res |= m_device->SetViewport(&viewport);

      GetDebugManager()->SetOutputDimensions(bbDesc.Width, bbDesc.Height);
      GetDebugManager()->SetOutputWindow(presentParams.hDeviceWindow);

      int flags = activeWindow ? RenderDoc::eOverlay_ActiveWindow : 0;
      flags |= RenderDoc::eOverlay_CaptureDisabled;

      std::string overlayText =
          RenderDoc::Inst().GetOverlayText(RDCDriver::D3D9, m_FrameCounter, flags);

      overlayText += "Captures not supported with D3D9\n";

      if(!overlayText.empty())
        GetDebugManager()->RenderText(0.0f, 0.0f, overlayText.c_str());

      stateBlockRes = stateBlock->Apply();
      res |= m_device->EndScene();
    }
  }

  RenderDoc::Inst().AddActiveDriver(RDCDriver::D3D9, true);

  return m_device->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

HRESULT __stdcall WrappedD3DDevice9::GetBackBuffer(UINT iSwapChain, UINT iBackBuffer,
                                                   D3DBACKBUFFER_TYPE Type,
                                                   IDirect3DSurface9 **ppBackBuffer)
{
  return m_device->GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer);
}

HRESULT __stdcall WrappedD3DDevice9::GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS *pRasterStatus)
{
  return m_device->GetRasterStatus(iSwapChain, pRasterStatus);
}

HRESULT __stdcall WrappedD3DDevice9::SetDialogBoxMode(BOOL bEnableDialogs)
{
  return m_device->SetDialogBoxMode(bEnableDialogs);
}

void __stdcall WrappedD3DDevice9::SetGammaRamp(UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP *pRamp)
{
  m_device->SetGammaRamp(iSwapChain, Flags, pRamp);
}

void __stdcall WrappedD3DDevice9::GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP *pRamp)
{
  m_device->GetGammaRamp(iSwapChain, pRamp);
}

HRESULT __stdcall WrappedD3DDevice9::CreateTexture(UINT Width, UINT Height, UINT Levels,
                                                   DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                                   IDirect3DTexture9 **ppTexture,
                                                   HANDLE *pSharedHandle)
{
  return m_device->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture,
                                 pSharedHandle);
}

HRESULT __stdcall WrappedD3DDevice9::CreateVolumeTexture(UINT Width, UINT Height, UINT Depth,
                                                         UINT Levels, DWORD Usage, D3DFORMAT Format,
                                                         D3DPOOL Pool,
                                                         IDirect3DVolumeTexture9 **ppVolumeTexture,
                                                         HANDLE *pSharedHandle)
{
  return m_device->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool,
                                       ppVolumeTexture, pSharedHandle);
}

HRESULT __stdcall WrappedD3DDevice9::CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage,
                                                       D3DFORMAT Format, D3DPOOL Pool,
                                                       IDirect3DCubeTexture9 **ppCubeTexture,
                                                       HANDLE *pSharedHandle)
{
  return m_device->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture,
                                     pSharedHandle);
}

HRESULT __stdcall WrappedD3DDevice9::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF,
                                                        D3DPOOL Pool,
                                                        IDirect3DVertexBuffer9 **ppVertexBuffer,
                                                        HANDLE *pSharedHandle)
{
  return m_device->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
}

HRESULT __stdcall WrappedD3DDevice9::CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format,
                                                       D3DPOOL Pool,
                                                       IDirect3DIndexBuffer9 **ppIndexBuffer,
                                                       HANDLE *pSharedHandle)
{
  return m_device->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
}

HRESULT __stdcall WrappedD3DDevice9::CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format,
                                                        D3DMULTISAMPLE_TYPE MultiSample,
                                                        DWORD MultisampleQuality, BOOL Lockable,
                                                        IDirect3DSurface9 **ppSurface,
                                                        HANDLE *pSharedHandle)
{
  return m_device->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality,
                                      Lockable, ppSurface, pSharedHandle);
}

HRESULT __stdcall WrappedD3DDevice9::CreateDepthStencilSurface(
    UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample,
    DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle)
{
  return m_device->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality,
                                             Discard, ppSurface, pSharedHandle);
}

HRESULT __stdcall WrappedD3DDevice9::UpdateSurface(IDirect3DSurface9 *pSourceSurface,
                                                   CONST RECT *pSourceRect,
                                                   IDirect3DSurface9 *pDestinationSurface,
                                                   CONST POINT *pDestPoint)
{
  return m_device->UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);
}

HRESULT __stdcall WrappedD3DDevice9::UpdateTexture(IDirect3DBaseTexture9 *pSourceTexture,
                                                   IDirect3DBaseTexture9 *pDestinationTexture)
{
  return m_device->UpdateTexture(pSourceTexture, pDestinationTexture);
}

HRESULT __stdcall WrappedD3DDevice9::GetRenderTargetData(IDirect3DSurface9 *pRenderTarget,
                                                         IDirect3DSurface9 *pDestSurface)
{
  return m_device->GetRenderTargetData(pRenderTarget, pDestSurface);
}

HRESULT __stdcall WrappedD3DDevice9::GetFrontBufferData(UINT iSwapChain,
                                                        IDirect3DSurface9 *pDestSurface)
{
  return m_device->GetFrontBufferData(iSwapChain, pDestSurface);
}

HRESULT __stdcall WrappedD3DDevice9::StretchRect(IDirect3DSurface9 *pSourceSurface,
                                                 CONST RECT *pSourceRect,
                                                 IDirect3DSurface9 *pDestSurface,
                                                 CONST RECT *pDestRect, D3DTEXTUREFILTERTYPE Filter)
{
  return m_device->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);
}

HRESULT __stdcall WrappedD3DDevice9::ColorFill(IDirect3DSurface9 *pSurface, CONST RECT *pRect,
                                               D3DCOLOR color)
{
  return m_device->ColorFill(pSurface, pRect, color);
}

HRESULT __stdcall WrappedD3DDevice9::CreateOffscreenPlainSurface(UINT Width, UINT Height,
                                                                 D3DFORMAT Format, D3DPOOL Pool,
                                                                 IDirect3DSurface9 **ppSurface,
                                                                 HANDLE *pSharedHandle)
{
  return m_device->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle);
}

HRESULT __stdcall WrappedD3DDevice9::SetRenderTarget(DWORD RenderTargetIndex,
                                                     IDirect3DSurface9 *pRenderTarget)
{
  return m_device->SetRenderTarget(RenderTargetIndex, pRenderTarget);
}

HRESULT __stdcall WrappedD3DDevice9::GetRenderTarget(DWORD RenderTargetIndex,
                                                     IDirect3DSurface9 **ppRenderTarget)
{
  return m_device->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
}

HRESULT __stdcall WrappedD3DDevice9::SetDepthStencilSurface(IDirect3DSurface9 *pNewZStencil)
{
  return m_device->SetDepthStencilSurface(pNewZStencil);
}

HRESULT __stdcall WrappedD3DDevice9::GetDepthStencilSurface(IDirect3DSurface9 **ppZStencilSurface)
{
  return m_device->GetDepthStencilSurface(ppZStencilSurface);
}

HRESULT __stdcall WrappedD3DDevice9::BeginScene()
{
  return m_device->BeginScene();
}

HRESULT __stdcall WrappedD3DDevice9::EndScene()
{
  return m_device->EndScene();
}

HRESULT __stdcall WrappedD3DDevice9::Clear(DWORD Count, CONST D3DRECT *pRects, DWORD Flags,
                                           D3DCOLOR Color, float Z, DWORD Stencil)
{
  return m_device->Clear(Count, pRects, Flags, Color, Z, Stencil);
}

HRESULT __stdcall WrappedD3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State,
                                                  CONST D3DMATRIX *pMatrix)
{
  return m_device->SetTransform(State, pMatrix);
}

HRESULT __stdcall WrappedD3DDevice9::GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix)
{
  return m_device->GetTransform(State, pMatrix);
}

HRESULT __stdcall WrappedD3DDevice9::MultiplyTransform(D3DTRANSFORMSTATETYPE _arg1,
                                                       CONST D3DMATRIX *_arg2)
{
  return m_device->MultiplyTransform(_arg1, _arg2);
}

HRESULT __stdcall WrappedD3DDevice9::SetViewport(CONST D3DVIEWPORT9 *pViewport)
{
  return m_device->SetViewport(pViewport);
}

HRESULT __stdcall WrappedD3DDevice9::GetViewport(D3DVIEWPORT9 *pViewport)
{
  return m_device->GetViewport(pViewport);
}

HRESULT __stdcall WrappedD3DDevice9::SetMaterial(CONST D3DMATERIAL9 *pMaterial)
{
  return m_device->SetMaterial(pMaterial);
}

HRESULT __stdcall WrappedD3DDevice9::GetMaterial(D3DMATERIAL9 *pMaterial)
{
  return m_device->GetMaterial(pMaterial);
}

HRESULT __stdcall WrappedD3DDevice9::SetLight(DWORD Index, CONST D3DLIGHT9 *_arg2)
{
  return m_device->SetLight(Index, _arg2);
}

HRESULT __stdcall WrappedD3DDevice9::GetLight(DWORD Index, D3DLIGHT9 *_arg2)
{
  return m_device->GetLight(Index, _arg2);
}

HRESULT __stdcall WrappedD3DDevice9::LightEnable(DWORD Index, BOOL Enable)
{
  return m_device->LightEnable(Index, Enable);
}

HRESULT __stdcall WrappedD3DDevice9::GetLightEnable(DWORD Index, BOOL *pEnable)
{
  return m_device->GetLightEnable(Index, pEnable);
}

HRESULT __stdcall WrappedD3DDevice9::SetClipPlane(DWORD Index, CONST float *pPlane)
{
  return m_device->SetClipPlane(Index, pPlane);
}

HRESULT __stdcall WrappedD3DDevice9::GetClipPlane(DWORD Index, float *pPlane)
{
  return m_device->GetClipPlane(Index, pPlane);
}

HRESULT __stdcall WrappedD3DDevice9::SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
{
  return m_device->SetRenderState(State, Value);
}

HRESULT __stdcall WrappedD3DDevice9::GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue)
{
  return m_device->GetRenderState(State, pValue);
}

HRESULT __stdcall WrappedD3DDevice9::CreateStateBlock(D3DSTATEBLOCKTYPE Type,
                                                      IDirect3DStateBlock9 **ppSB)
{
  return m_device->CreateStateBlock(Type, ppSB);
}

HRESULT __stdcall WrappedD3DDevice9::BeginStateBlock()
{
  return m_device->BeginStateBlock();
}

HRESULT __stdcall WrappedD3DDevice9::EndStateBlock(IDirect3DStateBlock9 **ppSB)
{
  return m_device->EndStateBlock(ppSB);
}

HRESULT __stdcall WrappedD3DDevice9::SetClipStatus(CONST D3DCLIPSTATUS9 *pClipStatus)
{
  return m_device->SetClipStatus(pClipStatus);
}

HRESULT __stdcall WrappedD3DDevice9::GetClipStatus(D3DCLIPSTATUS9 *pClipStatus)
{
  return m_device->GetClipStatus(pClipStatus);
}

HRESULT __stdcall WrappedD3DDevice9::GetTexture(DWORD Stage, IDirect3DBaseTexture9 **ppTexture)
{
  return m_device->GetTexture(Stage, ppTexture);
}

HRESULT __stdcall WrappedD3DDevice9::SetTexture(DWORD Stage, IDirect3DBaseTexture9 *pTexture)
{
  return m_device->SetTexture(Stage, pTexture);
}

HRESULT __stdcall WrappedD3DDevice9::GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type,
                                                          DWORD *pValue)
{
  return m_device->GetTextureStageState(Stage, Type, pValue);
}

HRESULT __stdcall WrappedD3DDevice9::SetTextureStageState(DWORD Stage,
                                                          D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
  return m_device->SetTextureStageState(Stage, Type, Value);
}

HRESULT __stdcall WrappedD3DDevice9::GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type,
                                                     DWORD *pValue)
{
  return m_device->GetSamplerState(Sampler, Type, pValue);
}

HRESULT __stdcall WrappedD3DDevice9::SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type,
                                                     DWORD Value)
{
  return m_device->SetSamplerState(Sampler, Type, Value);
}

HRESULT __stdcall WrappedD3DDevice9::ValidateDevice(DWORD *pNumPasses)
{
  return m_device->ValidateDevice(pNumPasses);
}

HRESULT __stdcall WrappedD3DDevice9::SetPaletteEntries(UINT PaletteNumber,
                                                       CONST PALETTEENTRY *pEntries)
{
  return m_device->SetPaletteEntries(PaletteNumber, pEntries);
}

HRESULT __stdcall WrappedD3DDevice9::GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY *pEntries)
{
  return m_device->GetPaletteEntries(PaletteNumber, pEntries);
}

HRESULT __stdcall WrappedD3DDevice9::SetCurrentTexturePalette(UINT PaletteNumber)
{
  return m_device->SetCurrentTexturePalette(PaletteNumber);
}

HRESULT __stdcall WrappedD3DDevice9::GetCurrentTexturePalette(UINT *PaletteNumber)
{
  return m_device->GetCurrentTexturePalette(PaletteNumber);
}

HRESULT __stdcall WrappedD3DDevice9::SetScissorRect(CONST RECT *pRect)
{
  return m_device->SetScissorRect(pRect);
}

HRESULT __stdcall WrappedD3DDevice9::GetScissorRect(RECT *pRect)
{
  return m_device->GetScissorRect(pRect);
}

HRESULT __stdcall WrappedD3DDevice9::SetSoftwareVertexProcessing(BOOL bSoftware)
{
  return m_device->SetSoftwareVertexProcessing(bSoftware);
}

BOOL __stdcall WrappedD3DDevice9::GetSoftwareVertexProcessing()
{
  return m_device->GetSoftwareVertexProcessing();
}

HRESULT __stdcall WrappedD3DDevice9::SetNPatchMode(float nSegments)
{
  return m_device->SetNPatchMode(nSegments);
}

float __stdcall WrappedD3DDevice9::GetNPatchMode()
{
  return m_device->GetNPatchMode();
}

HRESULT __stdcall WrappedD3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex,
                                                   UINT PrimitiveCount)
{
  return m_device->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
}

HRESULT __stdcall WrappedD3DDevice9::DrawIndexedPrimitive(D3DPRIMITIVETYPE _arg1, INT BaseVertexIndex,
                                                          UINT MinVertexIndex, UINT NumVertices,
                                                          UINT startIndex, UINT primCount)
{
  return m_device->DrawIndexedPrimitive(_arg1, BaseVertexIndex, MinVertexIndex, NumVertices,
                                        startIndex, primCount);
}

HRESULT __stdcall WrappedD3DDevice9::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,
                                                     UINT PrimitiveCount,
                                                     CONST void *pVertexStreamZeroData,
                                                     UINT VertexStreamZeroStride)
{
  return m_device->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData,
                                   VertexStreamZeroStride);
}

HRESULT __stdcall WrappedD3DDevice9::DrawIndexedPrimitiveUP(
    D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount,
    CONST void *pIndexData, D3DFORMAT IndexDataFormat, CONST void *pVertexStreamZeroData,
    UINT VertexStreamZeroStride)
{
  return m_device->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices,
                                          PrimitiveCount, pIndexData, IndexDataFormat,
                                          pVertexStreamZeroData, VertexStreamZeroStride);
}

HRESULT __stdcall WrappedD3DDevice9::ProcessVertices(UINT SrcStartIndex, UINT DestIndex,
                                                     UINT VertexCount,
                                                     IDirect3DVertexBuffer9 *pDestBuffer,
                                                     IDirect3DVertexDeclaration9 *pVertexDecl,
                                                     DWORD Flags)
{
  return m_device->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl,
                                   Flags);
}

HRESULT __stdcall WrappedD3DDevice9::CreateVertexDeclaration(CONST D3DVERTEXELEMENT9 *pVertexElements,
                                                             IDirect3DVertexDeclaration9 **ppDecl)
{
  return m_device->CreateVertexDeclaration(pVertexElements, ppDecl);
}

HRESULT __stdcall WrappedD3DDevice9::SetVertexDeclaration(IDirect3DVertexDeclaration9 *pDecl)
{
  return m_device->SetVertexDeclaration(pDecl);
}

HRESULT __stdcall WrappedD3DDevice9::GetVertexDeclaration(IDirect3DVertexDeclaration9 **ppDecl)
{
  return m_device->GetVertexDeclaration(ppDecl);
}

HRESULT __stdcall WrappedD3DDevice9::SetFVF(DWORD FVF)
{
  return m_device->SetFVF(FVF);
}

HRESULT __stdcall WrappedD3DDevice9::GetFVF(DWORD *pFVF)
{
  return m_device->GetFVF(pFVF);
}

HRESULT __stdcall WrappedD3DDevice9::CreateVertexShader(CONST DWORD *pFunction,
                                                        IDirect3DVertexShader9 **ppShader)
{
  return m_device->CreateVertexShader(pFunction, ppShader);
}

HRESULT __stdcall WrappedD3DDevice9::SetVertexShader(IDirect3DVertexShader9 *pShader)
{
  return m_device->SetVertexShader(pShader);
}

HRESULT __stdcall WrappedD3DDevice9::GetVertexShader(IDirect3DVertexShader9 **ppShader)
{
  return m_device->GetVertexShader(ppShader);
}

HRESULT __stdcall WrappedD3DDevice9::SetVertexShaderConstantF(UINT StartRegister,
                                                              CONST float *pConstantData,
                                                              UINT Vector4fCount)
{
  return m_device->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall WrappedD3DDevice9::GetVertexShaderConstantF(UINT StartRegister,
                                                              float *pConstantData,
                                                              UINT Vector4fCount)
{
  return m_device->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall WrappedD3DDevice9::SetVertexShaderConstantI(UINT StartRegister,
                                                              CONST int *pConstantData,
                                                              UINT Vector4iCount)
{
  return m_device->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall WrappedD3DDevice9::GetVertexShaderConstantI(UINT StartRegister,
                                                              int *pConstantData, UINT Vector4iCount)
{
  return m_device->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall WrappedD3DDevice9::SetVertexShaderConstantB(UINT StartRegister,
                                                              CONST BOOL *pConstantData,
                                                              UINT BoolCount)
{
  return m_device->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall WrappedD3DDevice9::GetVertexShaderConstantB(UINT StartRegister,
                                                              BOOL *pConstantData, UINT BoolCount)
{
  return m_device->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall WrappedD3DDevice9::SetStreamSource(UINT StreamNumber,
                                                     IDirect3DVertexBuffer9 *pStreamData,
                                                     UINT OffsetInBytes, UINT Stride)
{
  return m_device->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
}

HRESULT __stdcall WrappedD3DDevice9::GetStreamSource(UINT StreamNumber,
                                                     IDirect3DVertexBuffer9 **ppStreamData,
                                                     UINT *pOffsetInBytes, UINT *pStride)
{
  return m_device->GetStreamSource(StreamNumber, ppStreamData, pOffsetInBytes, pStride);
}

HRESULT __stdcall WrappedD3DDevice9::SetStreamSourceFreq(UINT StreamNumber, UINT Setting)
{
  return m_device->SetStreamSourceFreq(StreamNumber, Setting);
}

HRESULT __stdcall WrappedD3DDevice9::GetStreamSourceFreq(UINT StreamNumber, UINT *pSetting)
{
  return m_device->GetStreamSourceFreq(StreamNumber, pSetting);
}

HRESULT __stdcall WrappedD3DDevice9::SetIndices(IDirect3DIndexBuffer9 *pIndexData)
{
  return m_device->SetIndices(pIndexData);
}

HRESULT __stdcall WrappedD3DDevice9::GetIndices(IDirect3DIndexBuffer9 **ppIndexData)
{
  return m_device->GetIndices(ppIndexData);
}

HRESULT __stdcall WrappedD3DDevice9::CreatePixelShader(CONST DWORD *pFunction,
                                                       IDirect3DPixelShader9 **ppShader)
{
  return m_device->CreatePixelShader(pFunction, ppShader);
}

HRESULT __stdcall WrappedD3DDevice9::SetPixelShader(IDirect3DPixelShader9 *pShader)
{
  return m_device->SetPixelShader(pShader);
}

HRESULT __stdcall WrappedD3DDevice9::GetPixelShader(IDirect3DPixelShader9 **ppShader)
{
  return m_device->GetPixelShader(ppShader);
}

HRESULT __stdcall WrappedD3DDevice9::SetPixelShaderConstantF(UINT StartRegister,
                                                             CONST float *pConstantData,
                                                             UINT Vector4fCount)
{
  return m_device->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall WrappedD3DDevice9::GetPixelShaderConstantF(UINT StartRegister,
                                                             float *pConstantData, UINT Vector4fCount)
{
  return m_device->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

HRESULT __stdcall WrappedD3DDevice9::SetPixelShaderConstantI(UINT StartRegister,
                                                             CONST int *pConstantData,
                                                             UINT Vector4iCount)
{
  return m_device->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall WrappedD3DDevice9::GetPixelShaderConstantI(UINT StartRegister, int *pConstantData,
                                                             UINT Vector4iCount)
{
  return m_device->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

HRESULT __stdcall WrappedD3DDevice9::SetPixelShaderConstantB(UINT StartRegister,
                                                             CONST BOOL *pConstantData,
                                                             UINT BoolCount)
{
  return m_device->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall WrappedD3DDevice9::GetPixelShaderConstantB(UINT StartRegister,
                                                             BOOL *pConstantData, UINT BoolCount)
{
  return m_device->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}

HRESULT __stdcall WrappedD3DDevice9::DrawRectPatch(UINT Handle, CONST float *pNumSegs,
                                                   CONST D3DRECTPATCH_INFO *pRectPatchInfo)
{
  return m_device->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
}

HRESULT __stdcall WrappedD3DDevice9::DrawTriPatch(UINT Handle, CONST float *pNumSegs,
                                                  CONST D3DTRIPATCH_INFO *pTriPatchInfo)
{
  return m_device->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
}

HRESULT __stdcall WrappedD3DDevice9::DeletePatch(UINT Handle)
{
  return m_device->DeletePatch(Handle);
}

HRESULT __stdcall WrappedD3DDevice9::CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9 **ppQuery)
{
  return m_device->CreateQuery(Type, ppQuery);
}

HRESULT __stdcall WrappedD3D9::QueryInterface(REFIID riid, void **ppvObj)
{
  return m_direct3D->QueryInterface(riid, ppvObj);
}

ULONG __stdcall WrappedD3D9::AddRef()
{
  ULONG refCount;
  refCount = m_direct3D->AddRef();
  return refCount;
}

ULONG __stdcall WrappedD3D9::Release()
{
  ULONG refCount = m_direct3D->Release();
  if(refCount == 0)
  {
    delete this;
  }
  return refCount;
}

HRESULT __stdcall WrappedD3D9::RegisterSoftwareDevice(void *pInitializeFunction)
{
  return m_direct3D->RegisterSoftwareDevice(pInitializeFunction);
}

UINT __stdcall WrappedD3D9::GetAdapterCount()
{
  return m_direct3D->GetAdapterCount();
}

HRESULT __stdcall WrappedD3D9::GetAdapterIdentifier(UINT Adapter, DWORD Flags,
                                                    D3DADAPTER_IDENTIFIER9 *pIdentifier)
{
  return m_direct3D->GetAdapterIdentifier(Adapter, Flags, pIdentifier);
}

UINT __stdcall WrappedD3D9::GetAdapterModeCount(UINT Adapter, D3DFORMAT Format)
{
  return m_direct3D->GetAdapterModeCount(Adapter, Format);
}

HRESULT __stdcall WrappedD3D9::EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode,
                                                D3DDISPLAYMODE *pMode)
{
  return m_direct3D->EnumAdapterModes(Adapter, Format, Mode, pMode);
}

HRESULT __stdcall WrappedD3D9::GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE *pMode)
{
  return m_direct3D->GetAdapterDisplayMode(Adapter, pMode);
}

HRESULT __stdcall WrappedD3D9::CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType,
                                               D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat,
                                               BOOL bWindowed)
{
  return m_direct3D->CheckDeviceType(Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed);
}

HRESULT __stdcall WrappedD3D9::CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType,
                                                 D3DFORMAT AdapterFormat, DWORD Usage,
                                                 D3DRESOURCETYPE RType, D3DFORMAT CheckFormat)
{
  return m_direct3D->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
}

HRESULT __stdcall WrappedD3D9::CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType,
                                                          D3DFORMAT SurfaceFormat, BOOL Windowed,
                                                          D3DMULTISAMPLE_TYPE MultiSampleType,
                                                          DWORD *pQualityLevels)
{
  return m_direct3D->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed,
                                                MultiSampleType, pQualityLevels);
}

HRESULT __stdcall WrappedD3D9::CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType,
                                                      D3DFORMAT AdapterFormat,
                                                      D3DFORMAT RenderTargetFormat,
                                                      D3DFORMAT DepthStencilFormat)
{
  return m_direct3D->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat,
                                            DepthStencilFormat);
}

HRESULT __stdcall WrappedD3D9::CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType,
                                                           D3DFORMAT SourceFormat,
                                                           D3DFORMAT TargetFormat)
{
  return m_direct3D->CheckDeviceFormatConversion(Adapter, DeviceType, SourceFormat, TargetFormat);
}

HRESULT __stdcall WrappedD3D9::GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9 *pCaps)
{
  return m_direct3D->GetDeviceCaps(Adapter, DeviceType, pCaps);
}

HMONITOR __stdcall WrappedD3D9::GetAdapterMonitor(UINT Adapter)
{
  return m_direct3D->GetAdapterMonitor(Adapter);
}

HRESULT __stdcall WrappedD3D9::CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
                                            DWORD BehaviorFlags,
                                            D3DPRESENT_PARAMETERS *pPresentationParameters,
                                            IDirect3DDevice9 **ppReturnedDeviceInterface)
{
  IDirect3DDevice9 *device = NULL;
  HRESULT res = m_direct3D->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags,
                                         pPresentationParameters, &device);
  if(res == S_OK)
  {
    RDCLOG("App creating d3d9 device");

    HWND wnd = pPresentationParameters->hDeviceWindow;
    if(wnd == NULL)
      wnd = hFocusWindow;

    if(!wnd)
      RDCWARN("Couldn't find valid non-NULL window at CreateDevice time");

    WrappedD3DDevice9 *wrappedDevice = new WrappedD3DDevice9(device, wnd);
    wrappedDevice->LazyInit();    // TODO this can be moved later probably
    *ppReturnedDeviceInterface = wrappedDevice;
  }
  else
  {
    *ppReturnedDeviceInterface = NULL;
  }
  return res;
}
