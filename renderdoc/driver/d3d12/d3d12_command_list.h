/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

struct IAmdExtD3DCommandListMarker;
class WrappedID3D12GraphicsCommandList;

// The inheritance is awful for these. See WrappedID3D12DebugDevice for why there are multiple
// parent classes
struct WrappedID3D12DebugCommandList : public ID3D12DebugCommandList3, public ID3D12DebugCommandList1
{
  WrappedID3D12GraphicsCommandList *m_pList = NULL;
  ID3D12DebugCommandList *m_pReal = NULL;
  ID3D12DebugCommandList1 *m_pReal1 = NULL;
  ID3D12DebugCommandList2 *m_pReal2 = NULL;
  ID3D12DebugCommandList3 *m_pReal3 = NULL;

  WrappedID3D12DebugCommandList() {}
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
    else if(riid == __uuidof(ID3D12DebugCommandList1))
    {
      *ppvObject = (ID3D12DebugCommandList1 *)this;
      AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12DebugCommandList2))
    {
      *ppvObject = (ID3D12DebugCommandList2 *)this;
      AddRef();
      return S_OK;
    }
    else if(riid == __uuidof(ID3D12DebugCommandList3))
    {
      *ppvObject = (ID3D12DebugCommandList3 *)this;
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
      return m_pReal->AssertResourceState(Unwrap(pResource), Subresource, State);
    return TRUE;
  }

  virtual HRESULT STDMETHODCALLTYPE SetFeatureMask(D3D12_DEBUG_FEATURE Mask)
  {
    if(m_pReal)
      return m_pReal->SetFeatureMask(Mask);
    return S_OK;
  }

  virtual D3D12_DEBUG_FEATURE STDMETHODCALLTYPE GetFeatureMask()
  {
    if(m_pReal)
      return m_pReal->GetFeatureMask();
    return D3D12_DEBUG_FEATURE_NONE;
  }

  //////////////////////////////
  // implement ID3D12DebugCommandList1 / ID3D12DebugCommandList2

  virtual HRESULT STDMETHODCALLTYPE SetDebugParameter(D3D12_DEBUG_COMMAND_LIST_PARAMETER_TYPE Type,
                                                      const void *pData, UINT DataSize)
  {
    if(m_pReal1)
      return m_pReal1->SetDebugParameter(Type, pData, DataSize);
    if(m_pReal2)
      return m_pReal2->SetDebugParameter(Type, pData, DataSize);
    return S_OK;
  }

  virtual HRESULT STDMETHODCALLTYPE GetDebugParameter(D3D12_DEBUG_COMMAND_LIST_PARAMETER_TYPE Type,
                                                      void *pData, UINT DataSize)
  {
    if(m_pReal1)
      return m_pReal1->GetDebugParameter(Type, pData, DataSize);
    if(m_pReal2)
      return m_pReal2->GetDebugParameter(Type, pData, DataSize);
    return S_OK;
  }

  //////////////////////////////
  // implement ID3D12DebugCommandList3

  virtual void STDMETHODCALLTYPE AssertResourceAccess(ID3D12Resource *pResource, UINT Subresource,
                                                      D3D12_BARRIER_ACCESS Access)
  {
    if(m_pReal3)
      m_pReal3->AssertResourceAccess(Unwrap(pResource), Subresource, Access);
  }

  virtual void STDMETHODCALLTYPE AssertTextureLayout(ID3D12Resource *pResource, UINT Subresource,
                                                     D3D12_BARRIER_LAYOUT Layout)
  {
    if(m_pReal3)
      m_pReal3->AssertTextureLayout(Unwrap(pResource), Subresource, Layout);
  }
};

class WrappedID3D12GraphicsCommandList : public ID3D12GraphicsCommandListX
{
private:
  ID3D12GraphicsCommandList *m_pList = NULL;
  ID3D12GraphicsCommandList1 *m_pList1 = NULL;
  ID3D12GraphicsCommandList2 *m_pList2 = NULL;
  ID3D12GraphicsCommandList3 *m_pList3 = NULL;
  ID3D12GraphicsCommandList4 *m_pList4 = NULL;
  ID3D12GraphicsCommandList5 *m_pList5 = NULL;
  ID3D12GraphicsCommandList6 *m_pList6 = NULL;
  ID3D12GraphicsCommandList7 *m_pList7 = NULL;
  ID3D12GraphicsCommandList8 *m_pList8 = NULL;
  ID3D12GraphicsCommandList9 *m_pList9 = NULL;

  RefCounter12<ID3D12GraphicsCommandList> m_RefCounter;

  WrappedID3D12Device *m_pDevice;

  WriteSerialiser &GetThreadSerialiser();

  // command recording/replay data shared between queues and lists
  D3D12CommandData *m_Cmd;

  IAmdExtD3DCommandListMarker *m_AMDMarkers = NULL;

  WrappedID3D12RootSignature *m_CurGfxRootSig, *m_CurCompRootSig;

  ResourceId m_ResourceID;
  D3D12ResourceRecord *m_ListRecord;
  D3D12ResourceRecord *m_CreationRecord;

  // for ray dispatching we need to patch the tables on the GPU which requires pushing & popping
  // compute pipeline and root signature, so we track it here during capture
  D3D12RenderState m_CaptureComputeState;

  // the ray dispatches which have happened on this list, this keeps a reference on the buffers until
  // the list is reset, and each time the list is submitted the queue takes an additional reference
  rdcarray<PatchedRayDispatch::Resources> m_RayDispatches;

  CaptureState &m_State;

  WrappedID3D12DebugCommandList m_WrappedDebug;

  struct
  {
    IID riid;
    UINT nodeMask;
    D3D12_COMMAND_LIST_TYPE type;
  } m_Init;

  bool m_FakeCreationReset = false;

  static rdcstr GetChunkName(uint32_t idx);
  D3D12ResourceManager *GetResourceManager() { return m_pDevice->GetResourceManager(); }

  rdcarray<std::function<bool()>> m_ImmediateASCallbacks;
  rdcarray<std::function<bool()>> m_PendingASCallbacks;
public:
  ALLOCATE_WITH_WRAPPED_POOL(WrappedID3D12GraphicsCommandList);

  WrappedID3D12GraphicsCommandList(ID3D12GraphicsCommandList *real, WrappedID3D12Device *device,
                                   CaptureState &state);
  virtual ~WrappedID3D12GraphicsCommandList();

  ResourceId GetResourceID() { return m_ResourceID; }
  ID3D12GraphicsCommandList *GetReal() { return m_pList; }
  ID3D12GraphicsCommandList1 *GetReal1() { return m_pList1; }
  ID3D12GraphicsCommandList2 *GetReal2() { return m_pList2; }
  ID3D12GraphicsCommandList3 *GetReal3() { return m_pList3; }
  ID3D12GraphicsCommandList4 *GetReal4() { return m_pList4; }
  ID3D12GraphicsCommandList5 *GetReal5() { return m_pList5; }
  ID3D12GraphicsCommandList6 *GetReal6() { return m_pList6; }
  ID3D12GraphicsCommandList7 *GetReal7() { return m_pList7; }
  ID3D12GraphicsCommandList8 *GetReal8() { return m_pList8; }
  ID3D12GraphicsCommandList9 *GetReal9() { return m_pList9; }
  WrappedID3D12Device *GetWrappedDevice() { return m_pDevice; }
  D3D12ResourceRecord *GetResourceRecord() { return m_ListRecord; }
  D3D12ResourceRecord *GetCreationRecord() { return m_CreationRecord; }

  void FinaliseExecuteIndirectEvents(BakedCmdListInfo &info, BakedCmdListInfo::ExecuteData &exec);
  void SaveExecuteIndirectParameters(ID3D12GraphicsCommandListX *list,
                                     ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount,
                                     ID3D12Resource *pArgumentBuffer, UINT64 ArgumentBufferOffset,
                                     ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset);
  void ResetAndRecordExecuteIndirectStates(ID3D12GraphicsCommandListX *list, uint32_t baseEventID,
                                           uint32_t execCount,
                                           ID3D12CommandSignature *pCommandSignature,
                                           ID3D12Resource *pArgumentBuffer,
                                           UINT64 ArgumentBufferOffset, uint32_t argumentsReplayed);

  void AddRayDispatches(rdcarray<PatchedRayDispatch::Resources> &dispatches);

  void SetAMDMarkerInterface(IAmdExtD3DCommandListMarker *marker) { m_AMDMarkers = marker; }
  void SetCommandData(D3D12CommandData *cmd) { m_Cmd = cmd; }
  void SetInitParams(REFIID riid, UINT nodeMask, D3D12_COMMAND_LIST_TYPE type)
  {
    m_Init.riid = riid;
    m_Init.nodeMask = nodeMask;
    m_Init.type = type;
  }
  HRESULT ResetInternal(ID3D12CommandAllocator *pAllocator, ID3D12PipelineState *pInitialState,
                        bool fakeCreationReset);

  bool ValidateRootGPUVA(D3D12_GPU_VIRTUAL_ADDRESS buffer);

  void AddSubmissionASBuildCallback(bool waitForSubmission, const std::function<bool()> &postBldExec)
  {
    if(waitForSubmission)
      m_PendingASCallbacks.push_back(postBldExec);
    else
      m_ImmediateASCallbacks.push_back(postBldExec);
  }

  bool ExecuteImmediateASBuildCallbacks()
  {
    bool success = true;

    for(std::function<bool()> &func : m_ImmediateASCallbacks)
    {
      success &= func();
    }

    m_ImmediateASCallbacks.clear();
    return success;
  }

  void TakeWaitingASBuildCallbacks(rdcarray<std::function<bool()>> &callbacks)
  {
    callbacks.append(std::move(m_PendingASCallbacks));
    m_PendingASCallbacks.clear();
  }

  //////////////////////////////
  // implement IUnknown
  ULONG STDMETHODCALLTYPE AddRef() { return m_RefCounter.SoftRef(m_pDevice); }
  ULONG STDMETHODCALLTYPE Release()
  {
    unsigned int ret = m_RefCounter.SoftRelease(m_pDevice);
    if(ret == 0)
      delete this;
    return ret;
  }
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  //////////////////////////////
  // implement ID3D12Object

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
  {
    return m_pList->GetPrivateData(guid, pDataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
  {
    if(guid == WKPDID_D3DDebugObjectName)
    {
      m_pDevice->SetName(this, (const char *)pData);
    }
    else if(guid == WKPDID_D3DDebugObjectNameW)
    {
      rdcwstr wName((const wchar_t *)pData, DataSize / 2);
      rdcstr sName = StringFormat::Wide2UTF8(wName);
      m_pDevice->SetName(this, sName.c_str());
    }

    return m_pList->SetPrivateData(guid, DataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
  {
    return m_pList->SetPrivateDataInterface(guid, pData);
  }

  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR Name)
  {
    rdcstr utf8 = StringFormat::Wide2UTF8(Name);
    m_pDevice->SetName(this, utf8.c_str());

    return m_pList->SetName(Name);
  }

  //////////////////////////////
  // implement ID3D12DeviceChild

  virtual HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, _COM_Outptr_opt_ void **ppvDevice)
  {
    return m_pDevice->GetDevice(riid, ppvDevice);
  }

  //////////////////////////////
  // implement ID3D12CommandList

  virtual D3D12_COMMAND_LIST_TYPE STDMETHODCALLTYPE GetType() { return m_pList->GetType(); }
  //////////////////////////////
  // implement ID3D12GraphicsCommandList

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, Close);

  IMPLEMENT_FUNCTION_SERIALISED(virtual HRESULT STDMETHODCALLTYPE, Reset,
                                ID3D12CommandAllocator *pAllocator,
                                ID3D12PipelineState *pInitialState);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ClearState,
                                ID3D12PipelineState *pPipelineState);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DrawInstanced,
                                UINT VertexCountPerInstance, UINT InstanceCount,
                                UINT StartVertexLocation, UINT StartInstanceLocation);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DrawIndexedInstanced,
                                UINT IndexCountPerInstance, UINT InstanceCount,
                                UINT StartIndexLocation, INT BaseVertexLocation,
                                UINT StartInstanceLocation);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, Dispatch, UINT ThreadGroupCountX,
                                UINT ThreadGroupCountY, UINT ThreadGroupCountZ);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CopyBufferRegion,
                                ID3D12Resource *pDstBuffer, UINT64 DstOffset,
                                ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CopyTextureRegion,
                                const D3D12_TEXTURE_COPY_LOCATION *pDst, UINT DstX, UINT DstY,
                                UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION *pSrc,
                                const D3D12_BOX *pSrcBox);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CopyResource,
                                ID3D12Resource *pDstResource, ID3D12Resource *pSrcResource);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CopyTiles,
                                ID3D12Resource *pTiledResource,
                                const D3D12_TILED_RESOURCE_COORDINATE *pTileRegionStartCoordinate,
                                const D3D12_TILE_REGION_SIZE *pTileRegionSize,
                                ID3D12Resource *pBuffer, UINT64 BufferStartOffsetInBytes,
                                D3D12_TILE_COPY_FLAGS Flags);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ResolveSubresource,
                                ID3D12Resource *pDstResource, UINT DstSubresource,
                                ID3D12Resource *pSrcResource, UINT SrcSubresource,
                                DXGI_FORMAT Format);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, IASetPrimitiveTopology,
                                D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, RSSetViewports, UINT NumViewports,
                                const D3D12_VIEWPORT *pViewports);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, RSSetScissorRects, UINT NumRects,
                                const D3D12_RECT *pRects);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMSetBlendFactor,
                                const FLOAT BlendFactor[4]);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMSetStencilRef, UINT StencilRef);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetPipelineState,
                                ID3D12PipelineState *pPipelineState);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ResourceBarrier, UINT NumBarriers,
                                const D3D12_RESOURCE_BARRIER *pBarriers);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ExecuteBundle,
                                ID3D12GraphicsCommandList *pCommandList);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetDescriptorHeaps,
                                UINT NumDescriptorHeaps,
                                ID3D12DescriptorHeap *const *ppDescriptorHeaps);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetComputeRootSignature,
                                ID3D12RootSignature *pRootSignature);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetGraphicsRootSignature,
                                ID3D12RootSignature *pRootSignature);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetComputeRootDescriptorTable,
                                UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetGraphicsRootDescriptorTable,
                                UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetComputeRoot32BitConstant,
                                UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetGraphicsRoot32BitConstant,
                                UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetComputeRoot32BitConstants,
                                UINT RootParameterIndex, UINT Num32BitValuesToSet,
                                const void *pSrcData, UINT DestOffsetIn32BitValues);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetGraphicsRoot32BitConstants,
                                UINT RootParameterIndex, UINT Num32BitValuesToSet,
                                const void *pSrcData, UINT DestOffsetIn32BitValues);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetComputeRootConstantBufferView,
                                UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetGraphicsRootConstantBufferView,
                                UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetComputeRootShaderResourceView,
                                UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetGraphicsRootShaderResourceView,
                                UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetComputeRootUnorderedAccessView,
                                UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetGraphicsRootUnorderedAccessView,
                                UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, IASetIndexBuffer,
                                const D3D12_INDEX_BUFFER_VIEW *pView);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, IASetVertexBuffers, UINT StartSlot,
                                UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW *pViews);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SOSetTargets, UINT StartSlot,
                                UINT NumViews, const D3D12_STREAM_OUTPUT_BUFFER_VIEW *pViews);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMSetRenderTargets,
                                UINT NumRenderTargetDescriptors,
                                const D3D12_CPU_DESCRIPTOR_HANDLE *pRenderTargetDescriptors,
                                BOOL RTsSingleHandleToDescriptorRange,
                                const D3D12_CPU_DESCRIPTOR_HANDLE *pDepthStencilDescriptor);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ClearDepthStencilView,
                                D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView,
                                D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil,
                                UINT NumRects, const D3D12_RECT *pRects);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ClearRenderTargetView,
                                D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView,
                                const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT *pRects);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ClearUnorderedAccessViewUint,
                                D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
                                D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource,
                                const UINT Values[4], UINT NumRects, const D3D12_RECT *pRects);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ClearUnorderedAccessViewFloat,
                                D3D12_GPU_DESCRIPTOR_HANDLE ViewGPUHandleInCurrentHeap,
                                D3D12_CPU_DESCRIPTOR_HANDLE ViewCPUHandle, ID3D12Resource *pResource,
                                const FLOAT Values[4], UINT NumRects, const D3D12_RECT *pRects);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DiscardResource,
                                ID3D12Resource *pResource, const D3D12_DISCARD_REGION *pRegion);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, BeginQuery,
                                ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, EndQuery,
                                ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT Index);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ResolveQueryData,
                                ID3D12QueryHeap *pQueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex,
                                UINT NumQueries, ID3D12Resource *pDestinationBuffer,
                                UINT64 AlignedDestinationBufferOffset);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetPredication,
                                ID3D12Resource *pBuffer, UINT64 AlignedBufferOffset,
                                D3D12_PREDICATION_OP Operation);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetMarker, UINT Metadata,
                                const void *pData, UINT Size);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, BeginEvent, UINT Metadata,
                                const void *pData, UINT Size);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, EndEvent, );

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ExecuteIndirect,
                                ID3D12CommandSignature *pCommandSignature, UINT MaxCommandCount,
                                ID3D12Resource *pArgumentBuffer, UINT64 ArgumentBufferOffset,
                                ID3D12Resource *pCountBuffer, UINT64 CountBufferOffset);

  //////////////////////////////
  // implement ID3D12GraphicsCommandList1

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, AtomicCopyBufferUINT,
                                ID3D12Resource *pDstBuffer, UINT64 DstOffset,
                                ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT Dependencies,
                                ID3D12Resource *const *ppDependentResources,
                                const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, AtomicCopyBufferUINT64,
                                ID3D12Resource *pDstBuffer, UINT64 DstOffset,
                                ID3D12Resource *pSrcBuffer, UINT64 SrcOffset, UINT Dependencies,
                                ID3D12Resource *const *ppDependentResources,
                                const D3D12_SUBRESOURCE_RANGE_UINT64 *pDependentSubresourceRanges);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMSetDepthBounds, FLOAT Min,
                                FLOAT Max);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetSamplePositions,
                                UINT NumSamplesPerPixel, UINT NumPixels,
                                D3D12_SAMPLE_POSITION *pSamplePositions);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ResolveSubresourceRegion,
                                ID3D12Resource *pDstResource, UINT DstSubresource, UINT DstX,
                                UINT DstY, ID3D12Resource *pSrcResource, UINT SrcSubresource,
                                D3D12_RECT *pSrcRect, DXGI_FORMAT Format,
                                D3D12_RESOLVE_MODE ResolveMode);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetViewInstanceMask, UINT Mask);

  //////////////////////////////
  // implement ID3D12GraphicsCommandList2

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, WriteBufferImmediate, UINT Count,
                                const D3D12_WRITEBUFFERIMMEDIATE_PARAMETER *pParams,
                                const D3D12_WRITEBUFFERIMMEDIATE_MODE *pModes);

  //////////////////////////////
  // implement ID3D12GraphicsCommandList3

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetProtectedResourceSession,
                                _In_opt_ ID3D12ProtectedResourceSession *pProtectedResourceSession);

  //////////////////////////////
  // implement ID3D12GraphicsCommandList4
  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, BeginRenderPass,
                                _In_ UINT NumRenderTargets,
                                _In_reads_opt_(NumRenderTargets)
                                    const D3D12_RENDER_PASS_RENDER_TARGET_DESC *pRenderTargets,
                                _In_opt_ const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC *pDepthStencil,
                                D3D12_RENDER_PASS_FLAGS Flags);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, EndRenderPass);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, InitializeMetaCommand,
                                _In_ ID3D12MetaCommand *pMetaCommand,
                                _In_reads_bytes_opt_(InitializationParametersDataSizeInBytes)
                                    const void *pInitializationParametersData,
                                _In_ SIZE_T InitializationParametersDataSizeInBytes);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, ExecuteMetaCommand,
                                _In_ ID3D12MetaCommand *pMetaCommand,
                                _In_reads_bytes_opt_(ExecutionParametersDataSizeInBytes)
                                    const void *pExecutionParametersData,
                                _In_ SIZE_T ExecutionParametersDataSizeInBytes);

  bool PatchAccStructBlasAddress(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *accStructInput,
                                 ID3D12GraphicsCommandList4 *list,
                                 BakedCmdListInfo::PatchRaytracing *patchRaytracing);

  bool ProcessASBuildAfterSubmission(ResourceId asbWrappedResourceId,
                                     D3D12BufferOffset asbWrappedResourceBufferOffset,
                                     UINT64 byteSize);

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE, BuildRaytracingAccelerationStructure,
      _In_ const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC *pDesc,
      _In_ UINT NumPostbuildInfoDescs,
      _In_reads_opt_(NumPostbuildInfoDescs)
          const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pPostbuildInfoDescs);

  IMPLEMENT_FUNCTION_SERIALISED(
      virtual void STDMETHODCALLTYPE, EmitRaytracingAccelerationStructurePostbuildInfo,
      _In_ const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC *pDesc,
      _In_ UINT NumSourceAccelerationStructures,
      _In_reads_(NumSourceAccelerationStructures)
          const D3D12_GPU_VIRTUAL_ADDRESS *pSourceAccelerationStructureData);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, CopyRaytracingAccelerationStructure,
                                _In_ D3D12_GPU_VIRTUAL_ADDRESS DestAccelerationStructureData,
                                _In_ D3D12_GPU_VIRTUAL_ADDRESS SourceAccelerationStructureData,
                                _In_ D3D12_RAYTRACING_ACCELERATION_STRUCTURE_COPY_MODE Mode);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, SetPipelineState1,
                                _In_ ID3D12StateObject *pStateObject);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DispatchRays,
                                _In_ const D3D12_DISPATCH_RAYS_DESC *pDesc);

  //////////////////////////////
  // implement ID3D12GraphicsCommandList5
  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, RSSetShadingRate,
                                D3D12_SHADING_RATE baseShadingRate,
                                const D3D12_SHADING_RATE_COMBINER *combiners);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, RSSetShadingRateImage,
                                ID3D12Resource *shadingRateImage);

  //////////////////////////////
  // implement ID3D12GraphicsCommandList6
  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, DispatchMesh,
                                _In_ UINT ThreadGroupCountX, _In_ UINT ThreadGroupCountY,
                                _In_ UINT ThreadGroupCountZ);

  //////////////////////////////
  // implement ID3D12GraphicsCommandList7
  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, Barrier, UINT32 NumBarrierGroups,
                                _In_reads_(NumBarrierGroups)
                                    const D3D12_BARRIER_GROUP *pBarrierGroups);

  //////////////////////////////
  // implement ID3D12GraphicsCommandList8
  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, OMSetFrontAndBackStencilRef,
                                _In_ UINT FrontStencilRef, _In_ UINT BackStencilRef);

  //////////////////////////////
  // implement ID3D12GraphicsCommandList9
  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, RSSetDepthBias, _In_ FLOAT DepthBias,
                                _In_ FLOAT DepthBiasClamp, _In_ FLOAT SlopeScaledDepthBias);

  IMPLEMENT_FUNCTION_SERIALISED(virtual void STDMETHODCALLTYPE, IASetIndexBufferStripCutValue,
                                _In_ D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue);
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

template <>
ResourceId GetResID(ID3D12GraphicsCommandList1 *obj);
template <>
ResourceId GetResID(ID3D12GraphicsCommandList2 *obj);
template <>
ResourceId GetResID(ID3D12GraphicsCommandList3 *obj);
template <>
ResourceId GetResID(ID3D12GraphicsCommandList4 *obj);
template <>
ResourceId GetResID(ID3D12GraphicsCommandList5 *obj);
template <>
ResourceId GetResID(ID3D12GraphicsCommandList6 *obj);
template <>
ResourceId GetResID(ID3D12GraphicsCommandList7 *obj);
template <>
ResourceId GetResID(ID3D12GraphicsCommandList8 *obj);
template <>
ResourceId GetResID(ID3D12GraphicsCommandList9 *obj);

ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList1 *obj);
ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList2 *obj);
ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList3 *obj);
ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList4 *obj);
ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList5 *obj);
ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList6 *obj);
ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList7 *obj);
ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList8 *obj);
ID3D12GraphicsCommandList *Unwrap(ID3D12GraphicsCommandList9 *obj);

ID3D12GraphicsCommandList1 *Unwrap1(ID3D12GraphicsCommandList1 *obj);
ID3D12GraphicsCommandList2 *Unwrap2(ID3D12GraphicsCommandList2 *obj);
ID3D12GraphicsCommandList3 *Unwrap3(ID3D12GraphicsCommandList3 *obj);
ID3D12GraphicsCommandList4 *Unwrap4(ID3D12GraphicsCommandList4 *obj);
ID3D12GraphicsCommandList5 *Unwrap5(ID3D12GraphicsCommandList5 *obj);
ID3D12GraphicsCommandList6 *Unwrap6(ID3D12GraphicsCommandList6 *obj);
ID3D12GraphicsCommandList7 *Unwrap7(ID3D12GraphicsCommandList7 *obj);
ID3D12GraphicsCommandList8 *Unwrap8(ID3D12GraphicsCommandList8 *obj);
ID3D12GraphicsCommandList9 *Unwrap9(ID3D12GraphicsCommandList9 *obj);

WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList1 *obj);
WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList2 *obj);
WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList3 *obj);
WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList4 *obj);
WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList5 *obj);
WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList6 *obj);
WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList7 *obj);
WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList8 *obj);
WrappedID3D12GraphicsCommandList *GetWrapped(ID3D12GraphicsCommandList9 *obj);
