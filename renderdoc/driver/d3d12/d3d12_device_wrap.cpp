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

#include "driver/d3d12/d3d12_device.h"

HRESULT WrappedID3D12Device::CreateCommandQueue(const D3D12_COMMAND_QUEUE_DESC *pDesc, REFIID riid,
                                                void **ppCommandQueue)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type, REFIID riid,
                                                    void **ppCommandAllocator)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateGraphicsPipelineState(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc,
                                                         REFIID riid, void **ppPipelineState)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateComputePipelineState(const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc,
                                                        REFIID riid, void **ppPipelineState)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateCommandList(UINT nodeMask, D3D12_COMMAND_LIST_TYPE type,
                                               ID3D12CommandAllocator *pCommandAllocator,
                                               ID3D12PipelineState *pInitialState, REFIID riid,
                                               void **ppCommandList)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateDescriptorHeap(const D3D12_DESCRIPTOR_HEAP_DESC *pDescriptorHeapDesc,
                                                  REFIID riid, void **ppvHeap)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateRootSignature(UINT nodeMask, const void *pBlobWithRootSignature,
                                                 SIZE_T blobLengthInBytes, REFIID riid,
                                                 void **ppvRootSignature)
{
  return E_NOTIMPL;
}

void WrappedID3D12Device::CreateConstantBufferView(const D3D12_CONSTANT_BUFFER_VIEW_DESC *pDesc,
                                                   D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
}

void WrappedID3D12Device::CreateShaderResourceView(ID3D12Resource *pResource,
                                                   const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc,
                                                   D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
}

void WrappedID3D12Device::CreateUnorderedAccessView(ID3D12Resource *pResource,
                                                    ID3D12Resource *pCounterResource,
                                                    const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc,
                                                    D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
}

void WrappedID3D12Device::CreateRenderTargetView(ID3D12Resource *pResource,
                                                 const D3D12_RENDER_TARGET_VIEW_DESC *pDesc,
                                                 D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
}

void WrappedID3D12Device::CreateDepthStencilView(ID3D12Resource *pResource,
                                                 const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc,
                                                 D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
}

void WrappedID3D12Device::CreateSampler(const D3D12_SAMPLER_DESC *pDesc,
                                        D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
}

HRESULT WrappedID3D12Device::CreateCommittedResource(const D3D12_HEAP_PROPERTIES *pHeapProperties,
                                                     D3D12_HEAP_FLAGS HeapFlags,
                                                     const D3D12_RESOURCE_DESC *pResourceDesc,
                                                     D3D12_RESOURCE_STATES InitialResourceState,
                                                     const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                     REFIID riidResource, void **ppvResource)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateHeap(const D3D12_HEAP_DESC *pDesc, REFIID riid, void **ppvHeap)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreatePlacedResource(ID3D12Heap *pHeap, UINT64 HeapOffset,
                                                  const D3D12_RESOURCE_DESC *pDesc,
                                                  D3D12_RESOURCE_STATES InitialState,
                                                  const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                  REFIID riid, void **ppvResource)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateReservedResource(const D3D12_RESOURCE_DESC *pDesc,
                                                    D3D12_RESOURCE_STATES InitialState,
                                                    const D3D12_CLEAR_VALUE *pOptimizedClearValue,
                                                    REFIID riid, void **ppvResource)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateFence(UINT64 InitialValue, D3D12_FENCE_FLAGS Flags, REFIID riid,
                                         void **ppFence)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateQueryHeap(const D3D12_QUERY_HEAP_DESC *pDesc, REFIID riid,
                                             void **ppvHeap)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateCommandSignature(const D3D12_COMMAND_SIGNATURE_DESC *pDesc,
                                                    ID3D12RootSignature *pRootSignature,
                                                    REFIID riid, void **ppvCommandSignature)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateSharedHandle(ID3D12DeviceChild *pObject,
                                                const SECURITY_ATTRIBUTES *pAttributes,
                                                DWORD Access, LPCWSTR Name, HANDLE *pHandle)
{
  return E_NOTIMPL;
}

void WrappedID3D12Device::CopyDescriptors(
    UINT NumDestDescriptorRanges, const D3D12_CPU_DESCRIPTOR_HANDLE *pDestDescriptorRangeStarts,
    const UINT *pDestDescriptorRangeSizes, UINT NumSrcDescriptorRanges,
    const D3D12_CPU_DESCRIPTOR_HANDLE *pSrcDescriptorRangeStarts,
    const UINT *pSrcDescriptorRangeSizes, D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
}

void WrappedID3D12Device::CopyDescriptorsSimple(UINT NumDescriptors,
                                                D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptorRangeStart,
                                                D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptorRangeStart,
                                                D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapsType)
{
}

HRESULT WrappedID3D12Device::OpenSharedHandle(HANDLE NTHandle, REFIID riid, void **ppvObj)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::OpenSharedHandleByName(LPCWSTR Name, DWORD Access, HANDLE *pNTHandle)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::MakeResident(UINT NumObjects, ID3D12Pageable *const *ppObjects)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::Evict(UINT NumObjects, ID3D12Pageable *const *ppObjects)
{
  return E_NOTIMPL;
}

// we don't need to wrap any of these
HRESULT WrappedID3D12Device::GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData)
{
  return m_pDevice->GetPrivateData(guid, pDataSize, pData);
}

HRESULT WrappedID3D12Device::SetPrivateData(REFGUID guid, UINT DataSize, const void *pData)
{
  return m_pDevice->SetPrivateData(guid, DataSize, pData);
}

HRESULT WrappedID3D12Device::SetPrivateDataInterface(REFGUID guid, const IUnknown *pData)
{
  return m_pDevice->SetPrivateDataInterface(guid, pData);
}

HRESULT WrappedID3D12Device::SetName(LPCWSTR Name)
{
  return m_pDevice->SetName(Name);
}

UINT WrappedID3D12Device::GetNodeCount()
{
  return m_pDevice->GetNodeCount();
}

LUID WrappedID3D12Device::GetAdapterLuid()
{
  return m_pDevice->GetAdapterLuid();
}

void WrappedID3D12Device::GetResourceTiling(
    ID3D12Resource *pTiledResource, UINT *pNumTilesForEntireResource,
    D3D12_PACKED_MIP_INFO *pPackedMipDesc, D3D12_TILE_SHAPE *pStandardTileShapeForNonPackedMips,
    UINT *pNumSubresourceTilings, UINT FirstSubresourceTilingToGet,
    D3D12_SUBRESOURCE_TILING *pSubresourceTilingsForNonPackedMips)
{
  return m_pDevice->GetResourceTiling(
      pTiledResource, pNumTilesForEntireResource, pPackedMipDesc, pStandardTileShapeForNonPackedMips,
      pNumSubresourceTilings, FirstSubresourceTilingToGet, pSubresourceTilingsForNonPackedMips);
}

HRESULT WrappedID3D12Device::SetStablePowerState(BOOL Enable)
{
  return m_pDevice->SetStablePowerState(Enable);
}

HRESULT WrappedID3D12Device::CheckFeatureSupport(D3D12_FEATURE Feature, void *pFeatureSupportData,
                                                 UINT FeatureSupportDataSize)
{
  return m_pDevice->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}

UINT WrappedID3D12Device::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType)
{
  return m_pDevice->GetDescriptorHandleIncrementSize(DescriptorHeapType);
}

D3D12_RESOURCE_ALLOCATION_INFO WrappedID3D12Device::GetResourceAllocationInfo(
    UINT visibleMask, UINT numResourceDescs, const D3D12_RESOURCE_DESC *pResourceDescs)
{
  return m_pDevice->GetResourceAllocationInfo(visibleMask, numResourceDescs, pResourceDescs);
}

D3D12_HEAP_PROPERTIES WrappedID3D12Device::GetCustomHeapProperties(UINT nodeMask,
                                                                   D3D12_HEAP_TYPE heapType)
{
  return m_pDevice->GetCustomHeapProperties(nodeMask, heapType);
}

HRESULT WrappedID3D12Device::GetDeviceRemovedReason()
{
  return m_pDevice->GetDeviceRemovedReason();
}

void WrappedID3D12Device::GetCopyableFootprints(const D3D12_RESOURCE_DESC *pResourceDesc,
                                                UINT FirstSubresource, UINT NumSubresources,
                                                UINT64 BaseOffset,
                                                D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pLayouts,
                                                UINT *pNumRows, UINT64 *pRowSizeInBytes,
                                                UINT64 *pTotalBytes)
{
  return m_pDevice->GetCopyableFootprints(pResourceDesc, FirstSubresource, NumSubresources,
                                          BaseOffset, pLayouts, pNumRows, pRowSizeInBytes,
                                          pTotalBytes);
}
