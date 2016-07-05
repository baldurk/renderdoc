/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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
#include "d3d12_common.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"

class WrappedID3D12CommandQueue;

struct DummyID3D12DebugCommandQueue : public ID3D12DebugCommandQueue
{
  WrappedID3D12CommandQueue *m_pQueue;
  ID3D12DebugCommandQueue *m_pReal;

  DummyID3D12DebugCommandQueue() {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(ID3D12DebugCommandQueue))
    {
      *ppvObject = (ID3D12DebugCommandQueue *)this;
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D12DebugCommandQueue

  virtual BOOL STDMETHODCALLTYPE AssertResourceState(ID3D12Resource *pResource, UINT Subresource,
                                                     UINT State)
  {
    if(m_pReal)
      m_pReal->AssertResourceState(pResource, Subresource, State);
    return TRUE;
  }
};

class WrappedID3D12CommandQueue : public ID3D12CommandQueue,
                                  public RefCounter12<ID3D12CommandQueue>,
                                  public ID3DDevice
{
  ID3D12CommandQueue *m_pQueue;
  WrappedID3D12Device *m_pDevice;

  ResourceId m_ResourceID;
  D3D12ResourceRecord *m_QueueRecord;

  Serialiser *m_pSerialiser;
  LogState m_State;

  DummyID3D12DebugCommandQueue m_DummyDebug;

public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12CommandQueue);

  WrappedID3D12CommandQueue(ID3D12CommandQueue *real, WrappedID3D12Device *device,
                            Serialiser *serialiser);
  virtual ~WrappedID3D12CommandQueue();

  Serialiser *GetSerialiser() { return m_pSerialiser; }
  ResourceId GetResourceID() { return m_ResourceID; }
  ID3D12CommandQueue *GetReal() { return m_pQueue; }
  WrappedID3D12Device *GetWrappedDevice() { return m_pDevice; }
  // interface for DXGI
  virtual IUnknown *GetRealIUnknown() { return GetReal(); }
  virtual IID GetBackbufferUUID() { return __uuidof(ID3D12Resource); }
  virtual IID GetDeviceUUID() { return __uuidof(ID3D12CommandQueue); }
  virtual IUnknown *GetDeviceInterface() { return (ID3D12CommandQueue *)this; }
  // the rest forward to the device
  virtual void FirstFrame(WrappedIDXGISwapChain3 *swapChain) { m_pDevice->FirstFrame(swapChain); }
  virtual void ShutdownSwapchain(WrappedIDXGISwapChain3 *swapChain)
  {
    m_pDevice->FirstFrame(swapChain);
  }

  virtual void NewSwapchainBuffer(IUnknown *backbuffer)
  {
    m_pDevice->NewSwapchainBuffer(backbuffer);
  }
  virtual void ReleaseSwapchainResources(WrappedIDXGISwapChain3 *swapChain)
  {
    m_pDevice->ReleaseSwapchainResources(swapChain);
  }
  virtual IUnknown *WrapSwapchainBuffer(WrappedIDXGISwapChain3 *swap, DXGI_SWAP_CHAIN_DESC *swapDesc,
                                        UINT buffer, IUnknown *realSurface)
  {
    return m_pDevice->WrapSwapchainBuffer(swap, swapDesc, buffer, realSurface);
  }

  virtual HRESULT Present(WrappedIDXGISwapChain3 *swapChain, UINT SyncInterval, UINT Flags)
  {
    return m_pDevice->Present(swapChain, SyncInterval, Flags);
  }

  //////////////////////////////
  // implement IUnknown

  ULONG STDMETHODCALLTYPE AddRef() { return RefCounter12::SoftRef(m_pDevice); }
  ULONG STDMETHODCALLTYPE Release() { return RefCounter12::SoftRelease(m_pDevice); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  //////////////////////////////
  // implement ID3D12Object

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
  {
    return m_pQueue->GetPrivateData(guid, pDataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
  {
    if(guid == WKPDID_D3DDebugObjectName)
      m_pDevice->SetResourceName(this, (const char *)pData);

    return m_pQueue->SetPrivateData(guid, DataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
  {
    return m_pQueue->SetPrivateDataInterface(guid, pData);
  }

  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name)
  {
    string utf8 = StringFormat::Wide2UTF8(Name);
    m_pDevice->SetResourceName(this, utf8.c_str());

    return m_pQueue->SetName(Name);
  }

  //////////////////////////////
  // implement ID3D12DeviceChild

  virtual HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, _COM_Outptr_opt_ void **ppvDevice)
  {
    if(riid == __uuidof(ID3D12Device) && ppvDevice)
    {
      *ppvDevice = (ID3D12Device *)m_pDevice;
      m_pDevice->AddRef();
    }
    else if(riid != __uuidof(ID3D12Device))
    {
      return E_NOINTERFACE;
    }

    return E_INVALIDARG;
  }

  //////////////////////////////
  // implement ID3D12CommandQueue

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      UpdateTileMappings(ID3D12Resource *pResource, UINT NumResourceRegions,
                         const D3D12_TILED_RESOURCE_COORDINATE *pResourceRegionStartCoordinates,
                         const D3D12_TILE_REGION_SIZE *pResourceRegionSizes, ID3D12Heap *pHeap,
                         UINT NumRanges, const D3D12_TILE_RANGE_FLAGS *pRangeFlags,
                         const UINT *pHeapRangeStartOffsets, const UINT *pRangeTileCounts,
                         D3D12_TILE_MAPPING_FLAGS Flags));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      CopyTileMappings(ID3D12Resource *pDstResource,
                       const D3D12_TILED_RESOURCE_COORDINATE *pDstRegionStartCoordinate,
                       ID3D12Resource *pSrcResource,
                       const D3D12_TILED_RESOURCE_COORDINATE *pSrcRegionStartCoordinate,
                       const D3D12_TILE_REGION_SIZE *pRegionSize, D3D12_TILE_MAPPING_FLAGS Flags));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ExecuteCommandLists(UINT NumCommandLists,
                                                    ID3D12CommandList *const *ppCommandLists));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetMarker(UINT Metadata, const void *pData, UINT Size));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                BeginEvent(UINT Metadata, const void *pData, UINT Size));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, EndEvent());

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                Signal(ID3D12Fence *pFence, UINT64 Value));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                Wait(ID3D12Fence *pFence, UINT64 Value));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                GetTimestampFrequency(UINT64 *pFrequency));

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                GetClockCalibration(UINT64 *pGpuTimestamp, UINT64 *pCpuTimestamp));

  virtual D3D12_COMMAND_QUEUE_DESC STDMETHODCALLTYPE GetDesc() { return m_pReal->GetDesc(); }
};

template <>
ID3D12CommandQueue *Unwrap(ID3D12CommandQueue *obj);