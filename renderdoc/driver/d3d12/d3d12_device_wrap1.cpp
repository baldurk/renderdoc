/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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
#include "d3d12_resources.h"

HRESULT WrappedID3D12Device::CreatePipelineLibrary(_In_reads_(BlobLength) const void *pLibraryBlob,
                                                   SIZE_T BlobLength, REFIID riid,
                                                   _COM_Outptr_ void **ppPipelineLibrary)
{
// we don't want to ever use pipeline libraries since then we can't get the
// bytecode and pipeline config. So instead we always return that a blob is
// non-matching and return a dummy interface that does nothing when stored.
// This might cause the application to clear its previous cache but that's
// not the end of the world.
#ifndef D3D12_ERROR_DRIVER_VERSION_MISMATCH
#define D3D12_ERROR_DRIVER_VERSION_MISMATCH _HRESULT_TYPEDEF_(0x887E0002L)
#endif

  if(BlobLength > 0)
    return D3D12_ERROR_DRIVER_VERSION_MISMATCH;

  WrappedID3D12PipelineLibrary1 *pipeLibrary = new WrappedID3D12PipelineLibrary1(this);

  if(riid == __uuidof(ID3D12PipelineLibrary))
  {
    *ppPipelineLibrary = (ID3D12PipelineLibrary *)pipeLibrary;
  }
  else if(riid == __uuidof(ID3D12PipelineLibrary1))
  {
    *ppPipelineLibrary = (ID3D12PipelineLibrary1 *)pipeLibrary;
  }
  else
  {
    RDCERR("Unexpected interface type %s", ToStr(riid).c_str());
    pipeLibrary->Release();
    *ppPipelineLibrary = NULL;
    return E_NOINTERFACE;
  }

  return S_OK;
}

HRESULT WrappedID3D12Device::SetEventOnMultipleFenceCompletion(
    _In_reads_(NumFences) ID3D12Fence *const *ppFences,
    _In_reads_(NumFences) const UINT64 *pFenceValues, UINT NumFences,
    D3D12_MULTIPLE_FENCE_WAIT_FLAGS Flags, HANDLE hEvent)
{
  ID3D12Fence **unwrapped = GetTempArray<ID3D12Fence *>(NumFences);

  for(UINT i = 0; i < NumFences; i++)
    unwrapped[i] = Unwrap(ppFences[i]);

  return m_pDevice1->SetEventOnMultipleFenceCompletion(unwrapped, pFenceValues, NumFences, Flags,
                                                       hEvent);
}

HRESULT WrappedID3D12Device::SetResidencyPriority(UINT NumObjects,
                                                  _In_reads_(NumObjects)
                                                      ID3D12Pageable *const *ppObjects,
                                                  _In_reads_(NumObjects)
                                                      const D3D12_RESIDENCY_PRIORITY *pPriorities)
{
  ID3D12Pageable **unwrapped = GetTempArray<ID3D12Pageable *>(NumObjects);

  for(UINT i = 0; i < NumObjects; i++)
    unwrapped[i] = (ID3D12Pageable *)Unwrap((ID3D12DeviceChild *)ppObjects[i]);

  return m_pDevice1->SetResidencyPriority(NumObjects, unwrapped, pPriorities);
}
