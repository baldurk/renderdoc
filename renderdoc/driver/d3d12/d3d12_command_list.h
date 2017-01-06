/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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
#include "d3d12_commands.h"
#include "d3d12_common.h"
#include "d3d12_device.h"
#include "d3d12_resources.h"

class WrappedID3D12GraphicsCommandList;

struct WrappedID3D12DebugCommandList : public ID3D12DebugCommandList
{
  WrappedID3D12GraphicsCommandList *m_pList;
  ID3D12DebugCommandList *m_pReal;

  WrappedID3D12DebugCommandList() : m_pList(NULL), m_pReal(NULL) {}
  //////////////////////////////
  // implement IUnknown
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject)
  {
    if(riid == __uuidof(ID3D12DebugCommandList))
    {
      *ppvObject = (ID3D12DebugCommandList *)this;
      AddRef();
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();

  //////////////////////////////
  // implement ID3D12DebugCommandList

  virtual BOOL STDMETHODCALLTYPE AssertResourceState(ID3D12Resource *pResource, UINT Subresource,
                                                     UINT State)
  {
    if(m_pReal)
      m_pReal->AssertResourceState(pResource, Subresource, State);
    return TRUE;
  }

  virtual HRESULT STDMETHODCALLTYPE SetFeatureMask(D3D12_DEBUG_FEATURE Mask)
  {
    if(m_pReal)
      m_pReal->SetFeatureMask(Mask);
    return S_OK;
  }

  virtual D3D12_DEBUG_FEATURE STDMETHODCALLTYPE GetFeatureMask()
  {
    if(m_pReal)
      return m_pReal->GetFeatureMask();
    return D3D12_DEBUG_FEATURE_NONE;
  }
};

class WrappedID3D12GraphicsCommandList : public RefCounter12<ID3D12GraphicsCommandList>,
                                         public ID3D12GraphicsCommandList
{
  WrappedID3D12Device *m_pDevice;

  // command recording/replay data shared between queues and lists
  D3D12CommandData *m_Cmd;

  WrappedID3D12RootSignature *m_CurGfxRootSig, *m_CurCompRootSig;

  ResourceId m_ResourceID;
  D3D12ResourceRecord *m_ListRecord;

  Serialiser *m_pSerialiser;
  LogState &m_State;

  WrappedID3D12DebugCommandList m_WrappedDebug;

  struct
  {
    IID riid;
    UINT nodeMask;
    D3D12_COMMAND_LIST_TYPE type;
  } m_Init;

  const char *GetChunkName(uint32_t idx) { return m_pDevice->GetChunkName(idx); }
  D3D12ResourceManager *GetResourceManager() { return m_pDevice->GetResourceManager(); }
public:
  static const int AllocPoolCount = 8192;
  static const int AllocMaxByteSize = 2 * 1024 * 1024;
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12GraphicsCommandList, AllocPoolCount, AllocMaxByteSize);

  WrappedID3D12GraphicsCommandList(ID3D12GraphicsCommandList *real, WrappedID3D12Device *device,
                                   Serialiser *serialiser, LogState &state);
  virtual ~WrappedID3D12GraphicsCommandList();

  Serialiser *GetSerialiser() { return m_pSerialiser; }
  ResourceId GetResourceID() { return m_ResourceID; }
  ID3D12GraphicsCommandList *GetReal() { return m_pReal; }
  WrappedID3D12Device *GetWrappedDevice() { return m_pDevice; }
  D3D12ResourceRecord *GetResourceRecord() { return m_ListRecord; }
  ID3D12GraphicsCommandList *GetList(ResourceId id);
  ID3D12GraphicsCommandList *GetCrackedList(ResourceId id);

  void SetCommandData(D3D12CommandData *cmd) { m_Cmd = cmd; }
  void SetInitParams(REFIID riid, UINT nodeMask, D3D12_COMMAND_LIST_TYPE type)
  {
    m_Init.riid = riid;
    m_Init.nodeMask = nodeMask;
    m_Init.type = type;
  }

  bool ValidateRootGPUVA(ResourceId buffer);

  //////////////////////////////
  // implement IUnknown

  ULONG STDMETHODCALLTYPE AddRef() { return RefCounter12::SoftRef(m_pDevice); }
  ULONG STDMETHODCALLTYPE Release() { return RefCounter12::SoftRelease(m_pDevice); }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  //////////////////////////////
  // implement ID3D12Object

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
  {
    return m_pReal->GetPrivateData(guid, pDataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
  {
    if(guid == WKPDID_D3DDebugObjectName)
      m_pDevice->SetResourceName(this, (const char *)pData);

    return m_pReal->SetPrivateData(guid, DataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
  {
    return m_pReal->SetPrivateDataInterface(guid, pData);
  }

  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name)
  {
    string utf8 = StringFormat::Wide2UTF8(Name);
    m_pDevice->SetResourceName(this, utf8.c_str());

    return m_pReal->SetName(Name);
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
  // implement ID3D12CommandList

  virtual D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE GetType() { return m_pReal->GetType(); }
  //////////////////////////////
  // implement ID3D12GraphicsCommandList

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, Close());

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE,
                                Reset(ID3D12CommandAllocator *pAllocator,
                                      ID3D12PipelineState *pInitialState));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ClearState(ID3D12PipelineState *pPipelineState));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount,
                                              UINT StartVertexLocation, UINT StartInstanceLocation));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount,
                                                     UINT StartIndexLocation, INT BaseVertexLocation,
                                                     UINT StartInstanceLocation));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY,
                                         UINT ThreadGroupCountZ));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CopyBufferRegion(ID3D12Resource *pDstBuffer, UINT64 DstOffset,
                                                 ID3D12Resource *pSrcBuffer, UINT64 SrcOffset,
                                                 UINT64 NumBytes));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION *pDst,
                                                  UINT DstX, UINT DstY, UINT DstZ,
                                                  const D3D12_TEXTURE_COPY_LOCATION *pSrc,
                                                  const D3D12_BOX *pSrcBox));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                CopyResource(ID3D12Resource *pDstResource,
                                             ID3D12Resource *pSrcResource));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      CopyTiles(ID3D12Resource *pTiledResource,
                const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate,
                const D3D12_TILE_REGION_SIZE *pTileRegionSize, ID3D12Resource *pBuffer,
                UINT64 BufferStartOffsetInBytes, D3D12_TILE_COPY_FLAGS Flags));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ResolveSubresource(ID3D12Resource *pDstResource,
                                                   UINT DstSubresource, ID3D12Resource *pSrcResource,
                                                   UINT SrcSubresource, DXGI_FORMAT Format));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                IASetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT *pViewports));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                RSSetScissorRects(UINT NumRects, const D3D12_RECT *pRects));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                OMSetBlendFactor(const FLOAT BlendFactor[4]));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMSetStencilRef(UINT StencilRef));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetPipelineState(ID3D12PipelineState *pPipelineState));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ResourceBarrier(UINT NumBarriers,
                                                const D3D12_RESOURCE_BARRIER *pBarriers));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ExecuteBundle(ID3D12GraphicsCommandList *pCommandList));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetDescriptorHeaps(UINT NumDescriptorHeaps,
                                                   ID3D12DescriptorHeap *const *ppDescriptorHeaps));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetComputeRootSignature(ID3D12RootSignature *pRootSignature));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetGraphicsRootSignature(ID3D12RootSignature *pRootSignature));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      SetComputeRootDescriptorTable(UINT RootParameterIndex,
                                    D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      SetGraphicsRootDescriptorTable(UINT RootParameterIndex,
                                     D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData,
                                                            UINT DestOffsetIn32BitValues));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData,
                                                             UINT DestOffsetIn32BitValues));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetComputeRoot32BitConstants(UINT RootParameterIndex,
                                                             UINT Num32BitValuesToSet,
                                                             const void *pSrcData,
                                                             UINT DestOffsetIn32BitValues));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetGraphicsRoot32BitConstants(UINT RootParameterIndex,
                                                              UINT Num32BitValuesToSet,
                                                              const void *pSrcData,
                                                              UINT DestOffsetIn32BitValues));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      SetComputeRootConstantBufferView(UINT RootParameterIndex,
                                       D3D12_GPU_VIRTUAL_ADDRESS BufferLocation));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      SetGraphicsRootConstantBufferView(UINT RootParameterIndex,
                                        D3D12_GPU_VIRTUAL_ADDRESS BufferLocation));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      SetComputeRootShaderResourceView(UINT RootParameterIndex,
                                       D3D12_GPU_VIRTUAL_ADDRESS BufferLocation));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      SetGraphicsRootShaderResourceView(UINT RootParameterIndex,
                                        D3D12_GPU_VIRTUAL_ADDRESS BufferLocation));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      SetComputeRootUnorderedAccessView(UINT RootParameterIndex,
                                        D3D12_GPU_VIRTUAL_ADDRESS BufferLocation));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      SetGraphicsRootUnorderedAccessView(UINT RootParameterIndex,
                                         D3D12_GPU_VIRTUAL_ADDRESS BufferLocation));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                IASetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW *pView));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                IASetVertexBuffers(UINT StartSlot, UINT NumViews,
                                                   const D3D12_VERTEX_BUFFER_VIEW *pViews));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SOSetTargets(UINT StartSlot, UINT NumViews,
                                             const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      OMSetRenderTargets(UINT NumRenderTargetDescriptors,
                         const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors,
                         BOOL RTsSingleHandleToDescriptorRange,
                         const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView,
                                                      D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth,
                                                      UINT8 Stencil, UINT NumRects,
                                                      const D3D12_RECT *pRects));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView,
                                                      const FLOAT ColorRGBA[4], UINT NumRects,
                                                      const D3D12_RECT *pRects));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      ClearUnorderedAccessViewUint(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
                                   D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
                                   ID3D12Resource *pResource, const UINT Values[4], UINT NumRects,
                                   const D3D12_RECT *pRects));

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE,
      ClearUnorderedAccessViewFloat(D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
                                    D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle,
                                    ID3D12Resource *pResource, const FLOAT Values[4], UINT NumRects,
                                    const D3D12_RECT *pRects));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                DiscardResource(ID3D12Resource *pResource,
                                                const D3D12_DISCARD_REGION *pRegion));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                BeginQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type,
                                           UINT Index));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                EndQuery(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type,
                                         UINT Index));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ResolveQueryData(ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type,
                                                 UINT StartIndex, UINT NumQueries,
                                                 ID3D12Resource *pDestinationBuffer,
                                                 UINT64 AlignedDestinationBufferOffset));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetPredication(ID3D12Resource *pBuffer, UINT64 AlignedBufferOffset,
                                               D3D12_PREDICATION_OP Operation));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                SetMarker(UINT Metadata, const void *pData, UINT Size));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                BeginEvent(UINT Metadata, const void *pData, UINT Size));

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, EndEvent());

  void ReserveExecuteIndirect(ID3D12GraphicsCommandList *list, ResourceId sig, UINT maxCount);
  void PatchExecuteIndirect(BakedCmdListInfo &info, uint32_t executeIndex);
  void ReplayExecuteIndirect(ID3D12GraphicsCommandList *list, BakedCmdListInfo &info);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE,
                                ExecuteIndirect(ID3D12CommandSignature *pCommandSignature,
                                                UINT MaxCommandCount, ID3D12Resource *pArgumentBuffer,
                                                UINT64 ArgumentBufferOffset,
                                                ID3D12Resource *pCountBuffer,
                                                UINT64 CountBufferOffset));
};

template <>
ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList *obj);
template <>
ID3D12CommandList *Unwrap(ID3D12CommandList *obj);

template <>
ResourceId GetResID(ID3D12GraphicsCommandList *obj);
template <>
ResourceId GetResID(ID3D12CommandList *obj);

template <>
D3D12ResourceRecord *GetRecord(ID3D12GraphicsCommandList *obj);
template <>
D3D12ResourceRecord *GetRecord(ID3D12CommandList *obj);