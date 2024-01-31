/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

#include "d3d12_device.h"
#include "driver/dxgi/dxgi_common.h"
#include "d3d12_resources.h"

D3D12_RESOURCE_ALLOCATION_INFO STDMETHODCALLTYPE WrappedID3D12Device::GetResourceAllocationInfo2(
    UINT visibleMask, UINT numResourceDescs,
    _In_reads_(numResourceDescs) const D3D12_RESOURCE_DESC1 *pResourceDescs,
    _Out_writes_opt_(numResourceDescs) D3D12_RESOURCE_ALLOCATION_INFO1 *pResourceAllocationInfo1)
{
  return m_pDevice8->GetResourceAllocationInfo2(visibleMask, numResourceDescs, pResourceDescs,
                                                pResourceAllocationInfo1);
}

void STDMETHODCALLTYPE WrappedID3D12Device::CreateSamplerFeedbackUnorderedAccessView(
    _In_opt_ ID3D12Resource *pTargetedResource, _In_opt_ ID3D12Resource *pFeedbackResource,
    _In_ D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  RDCERR("CreateSamplerFeedbackUnorderedAccessView called but sampler feedback is not supported!");
}

void STDMETHODCALLTYPE WrappedID3D12Device::GetCopyableFootprints1(
    _In_ const D3D12_RESOURCE_DESC1 *pResourceDesc,
    _In_range_(0, D3D12_REQ_SUBRESOURCES) UINT FirstSubresource,
    _In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource) UINT NumSubresources, UINT64 BaseOffset,
    _Out_writes_opt_(NumSubresources) D3D12_PLACED_SUBRESOURCE_FOOTPRINT *pLayouts,
    _Out_writes_opt_(NumSubresources) UINT *pNumRows,
    _Out_writes_opt_(NumSubresources) UINT64 *pRowSizeInBytes, _Out_opt_ UINT64 *pTotalBytes)
{
  return m_pDevice8->GetCopyableFootprints1(pResourceDesc, FirstSubresource, NumSubresources,
                                            BaseOffset, pLayouts, pNumRows, pRowSizeInBytes,
                                            pTotalBytes);
}
