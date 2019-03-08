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

HRESULT WrappedID3D12Device::OpenExistingHeapFromAddress(const void *pAddress, REFIID riid,
                                                         void **ppvHeap)
{
  if(ppvHeap == NULL)
    return m_pDevice3->OpenExistingHeapFromAddress(pAddress, riid, ppvHeap);

  if(riid != __uuidof(ID3D12Heap))
    return E_NOINTERFACE;

  ID3D12Heap *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(ret = m_pDevice3->OpenExistingHeapFromAddress(pAddress, riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12Heap1 *wrapped = new WrappedID3D12Heap1(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      D3D12_HEAP_DESC heapDesc = wrapped->GetDesc();

      // remove SHARED flags that are not allowed on real heaps
      heapDesc.Flags &= ~(D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER);

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateHeapFromAddress);
      Serialise_CreateHeap(ser, &heapDesc, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Heap;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppvHeap = (ID3D12Heap *)wrapped;
  }

  return ret;
}

HRESULT WrappedID3D12Device::OpenExistingHeapFromFileMapping(HANDLE hFileMapping, REFIID riid,
                                                             void **ppvHeap)
{
  if(ppvHeap == NULL)
    return m_pDevice3->OpenExistingHeapFromFileMapping(hFileMapping, riid, ppvHeap);

  if(riid != __uuidof(ID3D12Heap))
    return E_NOINTERFACE;

  ID3D12Heap *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(
      ret = m_pDevice3->OpenExistingHeapFromFileMapping(hFileMapping, riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12Heap1 *wrapped = new WrappedID3D12Heap1(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      D3D12_HEAP_DESC heapDesc = wrapped->GetDesc();

      // remove SHARED flags that are not allowed on real heaps
      heapDesc.Flags &= ~(D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER);

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateHeapFromFileMapping);
      Serialise_CreateHeap(ser, &heapDesc, riid, (void **)&wrapped);

      D3D12ResourceRecord *record = GetResourceManager()->AddResourceRecord(wrapped->GetResourceID());
      record->type = Resource_Heap;
      record->Length = 0;
      wrapped->SetResourceRecord(record);

      record->AddChunk(scope.Get());
    }
    else
    {
      GetResourceManager()->AddLiveResource(wrapped->GetResourceID(), wrapped);
    }

    *ppvHeap = (ID3D12Heap *)wrapped;
  }

  return ret;
}

HRESULT WrappedID3D12Device::EnqueueMakeResident(D3D12_RESIDENCY_FLAGS Flags, UINT NumObjects,
                                                 ID3D12Pageable *const *ppObjects,
                                                 ID3D12Fence *pFenceToSignal,
                                                 UINT64 FenceValueToSignal)
{
  ID3D12Pageable **unwrapped = GetTempArray<ID3D12Pageable *>(NumObjects);

  // assume objects are immediately resident for the purposes of our tracking
  for(UINT i = 0; i < NumObjects; i++)
  {
    if(WrappedID3D12DescriptorHeap::IsAlloc(ppObjects[i]))
    {
      WrappedID3D12DescriptorHeap *heap = (WrappedID3D12DescriptorHeap *)ppObjects[i];
      heap->SetResident(true);
      unwrapped[i] = heap->GetReal();
    }
    else if(WrappedID3D12Resource1::IsAlloc(ppObjects[i]))
    {
      WrappedID3D12Resource1 *res = (WrappedID3D12Resource1 *)ppObjects[i];
      res->SetResident(true);
      unwrapped[i] = res->GetReal();
    }
    else
    {
      unwrapped[i] = (ID3D12Pageable *)Unwrap((ID3D12DeviceChild *)ppObjects[i]);
    }
  }

  bool capframe = false;

  {
    SCOPED_READLOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  if(capframe)
  {
    // serialise
  }

  return m_pDevice3->EnqueueMakeResident(Flags, NumObjects, unwrapped, Unwrap(pFenceToSignal),
                                         FenceValueToSignal);
}
