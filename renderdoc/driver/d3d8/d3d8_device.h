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

#pragma once
#include "common/timing.h"
#include "d3d8_common.h"
#include "d3d8_manager.h"

class D3D8DebugManager;

class WrappedD3DDevice8 : public IDirect3DDevice8, public IFrameCapturer
{
public:
  WrappedD3DDevice8(IDirect3DDevice8 *device, HWND wnd,
                    D3DPRESENT_PARAMETERS *pPresentationParameters);
  ~WrappedD3DDevice8();

  void LazyInit();

  void StartFrameCapture(void *dev, void *wnd);
  bool EndFrameCapture(void *dev, void *wnd);
  bool DiscardFrameCapture(void *dev, void *wnd);

  void InternalRef() { InterlockedIncrement(&m_InternalRefcount); }
  void InternalRelease() { InterlockedDecrement(&m_InternalRefcount); }
  void SoftRef() { m_SoftRefCounter.AddRef(); }
  void SoftRelease()
  {
    m_SoftRefCounter.Release();
    CheckForDeath();
  }

  D3D8DebugManager *GetDebugManager() { return m_DebugManager; }
  D3D8ResourceManager *GetResourceManager() { return m_ResourceManager; }
  Threading::CriticalSection &D3DLock() { return m_D3DLock; }
  IMPLEMENT_FUNCTION_SERIALISED(void, ReleaseResource(IDirect3DResource8 *res));

  /*** IUnknown methods ***/
  ULONG STDMETHODCALLTYPE AddRef() { return m_RefCounter.AddRef(); }
  ULONG STDMETHODCALLTYPE Release()
  {
    unsigned int ret = m_RefCounter.Release();
    CheckForDeath();
    return ret;
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  /*** IDirect3DDevice8 methods ***/
  virtual HRESULT __stdcall TestCooperativeLevel();
  virtual UINT __stdcall GetAvailableTextureMem();
  virtual HRESULT __stdcall ResourceManagerDiscardBytes(DWORD Bytes);
  virtual HRESULT __stdcall GetDirect3D(IDirect3D8 **ppD3D8);
  virtual HRESULT __stdcall GetDeviceCaps(D3DCAPS8 *pCaps);
  virtual HRESULT __stdcall GetDisplayMode(D3DDISPLAYMODE *pMode);
  virtual HRESULT __stdcall GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS *pParameters);
  virtual HRESULT __stdcall SetCursorProperties(UINT XHotSpot, UINT YHotSpot,
                                                IDirect3DSurface8 *pCursorBitmap);
  virtual void __stdcall SetCursorPosition(int X, int Y, DWORD Flags);
  virtual BOOL __stdcall ShowCursor(BOOL bShow);
  virtual HRESULT __stdcall CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters,
                                                      IDirect3DSwapChain8 **pSwapChain);
  virtual HRESULT __stdcall Reset(D3DPRESENT_PARAMETERS *pPresentationParameters);
  virtual HRESULT __stdcall Present(CONST RECT *pSourceRect, CONST RECT *pDestRect,
                                    HWND hDestWindow, CONST RGNDATA *pDirtyRegion);
  virtual HRESULT __stdcall GetBackBuffer(UINT iBackBuffer, D3DBACKBUFFER_TYPE Type,
                                          IDirect3DSurface8 **ppBackBuffer);
  virtual HRESULT __stdcall GetRasterStatus(D3DRASTER_STATUS *pRasterStatus);
  virtual void __stdcall SetGammaRamp(DWORD Flags, CONST D3DGAMMARAMP *pRamp);
  virtual void __stdcall GetGammaRamp(D3DGAMMARAMP *pRamp);
  virtual HRESULT __stdcall CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage,
                                          D3DFORMAT Format, D3DPOOL Pool,
                                          IDirect3DTexture8 **ppTexture);
  virtual HRESULT __stdcall CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels,
                                                DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                                IDirect3DVolumeTexture8 **ppVolumeTexture);
  virtual HRESULT __stdcall CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage,
                                              D3DFORMAT Format, D3DPOOL Pool,
                                              IDirect3DCubeTexture8 **ppCubeTexture);
  virtual HRESULT __stdcall CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool,
                                               IDirect3DVertexBuffer8 **ppVertexBuffer);
  virtual HRESULT __stdcall CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format,
                                              D3DPOOL Pool, IDirect3DIndexBuffer8 **ppIndexBuffer);
  virtual HRESULT __stdcall CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format,
                                               D3DMULTISAMPLE_TYPE MultiSample, BOOL Lockable,
                                               IDirect3DSurface8 **ppSurface);
  virtual HRESULT __stdcall CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format,
                                                      D3DMULTISAMPLE_TYPE MultiSample,
                                                      IDirect3DSurface8 **ppSurface);
  virtual HRESULT __stdcall CreateImageSurface(UINT Width, UINT Height, D3DFORMAT Format,
                                               IDirect3DSurface8 **ppSurface);
  virtual HRESULT __stdcall CopyRects(IDirect3DSurface8 *pSourceSurface,
                                      CONST RECT *pSourceRectsArray, UINT NumRects,
                                      IDirect3DSurface8 *pDestinationSurface,
                                      CONST POINT *pDestPointsArray);
  virtual HRESULT __stdcall UpdateTexture(IDirect3DBaseTexture8 *pSourceTexture,
                                          IDirect3DBaseTexture8 *pDestinationTexture);
  virtual HRESULT __stdcall GetFrontBuffer(IDirect3DSurface8 *pDestSurface);
  virtual HRESULT __stdcall SetRenderTarget(IDirect3DSurface8 *pRenderTarget,
                                            IDirect3DSurface8 *pNewZStencil);
  virtual HRESULT __stdcall GetRenderTarget(IDirect3DSurface8 **ppRenderTarget);
  virtual HRESULT __stdcall GetDepthStencilSurface(IDirect3DSurface8 **ppZStencilSurface);
  virtual HRESULT __stdcall BeginScene();
  virtual HRESULT __stdcall EndScene();
  virtual HRESULT __stdcall Clear(DWORD Count, CONST D3DRECT *pRects, DWORD Flags, D3DCOLOR Color,
                                  float Z, DWORD Stencil);
  virtual HRESULT __stdcall SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX *pMatrix);
  virtual HRESULT __stdcall GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX *pMatrix);
  virtual HRESULT __stdcall MultiplyTransform(D3DTRANSFORMSTATETYPE, CONST D3DMATRIX *);
  virtual HRESULT __stdcall SetViewport(CONST D3DVIEWPORT8 *pViewport);
  virtual HRESULT __stdcall GetViewport(D3DVIEWPORT8 *pViewport);
  virtual HRESULT __stdcall SetMaterial(CONST D3DMATERIAL8 *pMaterial);
  virtual HRESULT __stdcall GetMaterial(D3DMATERIAL8 *pMaterial);
  virtual HRESULT __stdcall SetLight(DWORD Index, CONST D3DLIGHT8 *);
  virtual HRESULT __stdcall GetLight(DWORD Index, D3DLIGHT8 *);
  virtual HRESULT __stdcall LightEnable(DWORD Index, BOOL Enable);
  virtual HRESULT __stdcall GetLightEnable(DWORD Index, BOOL *pEnable);
  virtual HRESULT __stdcall SetClipPlane(DWORD Index, CONST float *pPlane);
  virtual HRESULT __stdcall GetClipPlane(DWORD Index, float *pPlane);
  virtual HRESULT __stdcall SetRenderState(D3DRENDERSTATETYPE State, DWORD Value);
  virtual HRESULT __stdcall GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue);
  virtual HRESULT __stdcall BeginStateBlock();
  virtual HRESULT __stdcall EndStateBlock(DWORD *pToken);
  virtual HRESULT __stdcall ApplyStateBlock(DWORD Token);
  virtual HRESULT __stdcall CaptureStateBlock(DWORD Token);
  virtual HRESULT __stdcall DeleteStateBlock(DWORD Token);
  virtual HRESULT __stdcall CreateStateBlock(D3DSTATEBLOCKTYPE Type, DWORD *pToken);
  virtual HRESULT __stdcall SetClipStatus(CONST D3DCLIPSTATUS8 *pClipStatus);
  virtual HRESULT __stdcall GetClipStatus(D3DCLIPSTATUS8 *pClipStatus);
  virtual HRESULT __stdcall GetTexture(DWORD Stage, IDirect3DBaseTexture8 **ppTexture);
  virtual HRESULT __stdcall SetTexture(DWORD Stage, IDirect3DBaseTexture8 *pTexture);
  virtual HRESULT __stdcall GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type,
                                                 DWORD *pValue);
  virtual HRESULT __stdcall SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type,
                                                 DWORD Value);
  virtual HRESULT __stdcall ValidateDevice(DWORD *pNumPasses);
  virtual HRESULT __stdcall GetInfo(DWORD DevInfoID, void *pDevInfoStruct, DWORD DevInfoStructSize);
  virtual HRESULT __stdcall SetPaletteEntries(UINT PaletteNumber, CONST PALETTEENTRY *pEntries);
  virtual HRESULT __stdcall GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY *pEntries);
  virtual HRESULT __stdcall SetCurrentTexturePalette(UINT PaletteNumber);
  virtual HRESULT __stdcall GetCurrentTexturePalette(UINT *PaletteNumber);
  virtual HRESULT __stdcall DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex,
                                          UINT PrimitiveCount);
  virtual HRESULT __stdcall DrawIndexedPrimitive(D3DPRIMITIVETYPE, UINT MinVertexIndex,
                                                 UINT NumVertices, UINT startIndex, UINT primCount);
  virtual HRESULT __stdcall DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount,
                                            CONST void *pVertexStreamZeroData,
                                            UINT VertexStreamZeroStride);
  virtual HRESULT __stdcall DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType,
                                                   UINT MinVertexIndex, UINT NumVertices,
                                                   UINT PrimitiveCount, CONST void *pIndexData,
                                                   D3DFORMAT IndexDataFormat,
                                                   CONST void *pVertexStreamZeroData,
                                                   UINT VertexStreamZeroStride);
  virtual HRESULT __stdcall ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount,
                                            IDirect3DVertexBuffer8 *pDestBuffer, DWORD Flags);
  virtual HRESULT __stdcall CreateVertexShader(CONST DWORD *pDeclaration, CONST DWORD *pFunction,
                                               DWORD *pHandle, DWORD Usage);
  virtual HRESULT __stdcall SetVertexShader(DWORD Handle);
  virtual HRESULT __stdcall GetVertexShader(DWORD *pHandle);
  virtual HRESULT __stdcall DeleteVertexShader(DWORD HHandle);
  virtual HRESULT __stdcall SetVertexShaderConstant(DWORD Register, CONST void *pConstantData,
                                                    DWORD ConstantCount);
  virtual HRESULT __stdcall GetVertexShaderConstant(DWORD Register, void *pConstantData,
                                                    DWORD ConstantCount);
  virtual HRESULT __stdcall GetVertexShaderDeclaration(DWORD Handle, void *pData, DWORD *pSizeOfData);
  virtual HRESULT __stdcall GetVertexShaderFunction(DWORD Handle, void *pData, DWORD *pSizeOfData);
  virtual HRESULT __stdcall SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer8 *pStreamData,
                                            UINT Stride);
  virtual HRESULT __stdcall GetStreamSource(UINT StreamNumber,
                                            IDirect3DVertexBuffer8 **ppStreamData, UINT *pStride);
  virtual HRESULT __stdcall SetIndices(IDirect3DIndexBuffer8 *pIndexData, UINT BaseVertexIndex);
  virtual HRESULT __stdcall GetIndices(IDirect3DIndexBuffer8 **ppIndexData, UINT *pBaseVertexIndex);
  virtual HRESULT __stdcall CreatePixelShader(CONST DWORD *pFunction, DWORD *pHandle);
  virtual HRESULT __stdcall SetPixelShader(DWORD Handle);
  virtual HRESULT __stdcall GetPixelShader(DWORD *pHandle);
  virtual HRESULT __stdcall DeletePixelShader(DWORD Handle);
  virtual HRESULT __stdcall SetPixelShaderConstant(DWORD Register, CONST void *pConstantData,
                                                   DWORD ConstantCount);
  virtual HRESULT __stdcall GetPixelShaderConstant(DWORD Register, void *pConstantData,
                                                   DWORD ConstantCount);
  virtual HRESULT __stdcall GetPixelShaderFunction(DWORD Handle, void *pData, DWORD *pSizeOfData);
  virtual HRESULT __stdcall DrawRectPatch(UINT Handle, CONST float *pNumSegs,
                                          CONST D3DRECTPATCH_INFO *pRectPatchInfo);
  virtual HRESULT __stdcall DrawTriPatch(UINT Handle, CONST float *pNumSegs,
                                         CONST D3DTRIPATCH_INFO *pTriPatchInfo);
  virtual HRESULT __stdcall DeletePatch(UINT Handle);

private:
  void CheckForDeath();

  IDirect3DDevice8 *m_device;
  D3D8DebugManager *m_DebugManager;
  D3D8ResourceManager *m_ResourceManager;

  D3DPRESENT_PARAMETERS m_PresentParameters;

  HWND m_Wnd;

  // ensure all calls in via the D3D wrapped interface are thread safe
  // protects wrapped resource creation and serialiser access
  Threading::CriticalSection m_D3DLock;

  CaptureState m_State;

  unsigned int m_InternalRefcount;
  RefCounter8 m_RefCounter;
  RefCounter8 m_SoftRefCounter;
  bool m_Alive;

  uint32_t m_FrameCounter;
};

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
// WrappedD3D8

class WrappedD3D8 : public IDirect3D8
{
public:
  WrappedD3D8(IDirect3D8 *direct3D8) : m_direct3D(direct3D8) {}
  /*** IUnknown methods ***/
  virtual HRESULT __stdcall QueryInterface(REFIID riid, void **ppvObj);
  virtual ULONG __stdcall AddRef();
  virtual ULONG __stdcall Release();

  /*** IDirect3D8 methods ***/
  virtual HRESULT __stdcall RegisterSoftwareDevice(void *pInitializeFunction);
  virtual UINT __stdcall GetAdapterCount();
  virtual HRESULT __stdcall GetAdapterIdentifier(UINT Adapter, DWORD Flags,
                                                 D3DADAPTER_IDENTIFIER8 *pIdentifier);
  virtual UINT __stdcall GetAdapterModeCount(UINT Adapter);
  virtual HRESULT __stdcall EnumAdapterModes(UINT Adapter, UINT Mode, D3DDISPLAYMODE *pMode);
  virtual HRESULT __stdcall GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE *pMode);
  virtual HRESULT __stdcall CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat,
                                            D3DFORMAT BackBufferFormat, BOOL bWindowed);
  virtual HRESULT __stdcall CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType,
                                              D3DFORMAT AdapterFormat, DWORD Usage,
                                              D3DRESOURCETYPE RType, D3DFORMAT CheckFormat);
  virtual HRESULT __stdcall CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType,
                                                       D3DFORMAT SurfaceFormat, BOOL Windowed,
                                                       D3DMULTISAMPLE_TYPE MultiSampleType);
  virtual HRESULT __stdcall CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType,
                                                   D3DFORMAT AdapterFormat,
                                                   D3DFORMAT RenderTargetFormat,
                                                   D3DFORMAT DepthStencilFormat);
  virtual HRESULT __stdcall GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS8 *pCaps);
  virtual HMONITOR __stdcall GetAdapterMonitor(UINT Adapter);
  virtual HRESULT __stdcall CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow,
                                         DWORD BehaviorFlags,
                                         D3DPRESENT_PARAMETERS *pPresentationParameters,
                                         IDirect3DDevice8 **ppReturnedDeviceInterface);

private:
  IDirect3D8 *m_direct3D;
};