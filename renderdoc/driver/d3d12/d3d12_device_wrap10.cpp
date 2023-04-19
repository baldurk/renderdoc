/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2023 Baldur Karlsson
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
#include "d3d12_command_queue.h"
#include "d3d12_resources.h"

HRESULT WrappedID3D12Device::CreateCommittedResource3(
    _In_ const D3D12_HEAP_PROPERTIES *pHeapProperties, D3D12_HEAP_FLAGS HeapFlags,
    _In_ const D3D12_RESOURCE_DESC1 *pDesc, D3D12_BARRIER_LAYOUT InitialLayout,
    _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    _In_opt_ ID3D12ProtectedResourceSession *pProtectedSession, UINT32 NumCastableFormats,
    _In_opt_count_(NumCastableFormats) const DXGI_FORMAT *pCastableFormats, REFIID riidResource,
    _COM_Outptr_opt_ void **ppvResource)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreatePlacedResource2(
    _In_ ID3D12Heap *pHeap, UINT64 HeapOffset, _In_ const D3D12_RESOURCE_DESC1 *pDesc,
    D3D12_BARRIER_LAYOUT InitialLayout, _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    UINT32 NumCastableFormats, _In_opt_count_(NumCastableFormats) const DXGI_FORMAT *pCastableFormats,
    REFIID riid, _COM_Outptr_opt_ void **ppvResource)
{
  return E_NOTIMPL;
}

HRESULT WrappedID3D12Device::CreateReservedResource2(
    _In_ const D3D12_RESOURCE_DESC *pDesc, D3D12_BARRIER_LAYOUT InitialLayout,
    _In_opt_ const D3D12_CLEAR_VALUE *pOptimizedClearValue,
    _In_opt_ ID3D12ProtectedResourceSession *pProtectedSession, UINT32 NumCastableFormats,
    _In_opt_count_(NumCastableFormats) const DXGI_FORMAT *pCastableFormats, REFIID riid,
    _COM_Outptr_opt_ void **ppvResource)
{
  return E_NOTIMPL;
}
