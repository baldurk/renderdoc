/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
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

HRESULT WrappedID3D12Device::OpenExistingHeapFromAddress1(const void *pAddress, SIZE_T size,
                                                          REFIID riid, void **ppvHeap)
{
  if(ppvHeap == NULL)
    return m_pDevice13->OpenExistingHeapFromAddress1(pAddress, size, riid, ppvHeap);

  if(riid != __uuidof(ID3D12Heap))
    return E_NOINTERFACE;

  ID3D12Heap *real = NULL;
  HRESULT ret;
  SERIALISE_TIME_CALL(
      ret = m_pDevice13->OpenExistingHeapFromAddress1(pAddress, size, riid, (void **)&real));

  if(SUCCEEDED(ret))
  {
    WrappedID3D12Heap *wrapped = new WrappedID3D12Heap(real, this);

    if(IsCaptureMode(m_State))
    {
      CACHE_THREAD_SERIALISER();

      D3D12_HEAP_DESC heapDesc = wrapped->GetDesc();

      // remove SHARED flags that are not allowed on real heaps
      heapDesc.Flags &= ~(D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER);

      D3D12_FEATURE_DATA_D3D12_OPTIONS opts = {};
      if(SUCCEEDED(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &opts, sizeof(opts))))
      {
        if(opts.ResourceHeapTier == D3D12_RESOURCE_HEAP_TIER_1)
        {
          // tier 1 devices don't support heaps with no DENY flags, but the heap we get from here
          // will likely have no DENY flags set. Artifically add one that should be safe for this
          // kind of heap.
          if((heapDesc.Flags & (D3D12_HEAP_FLAG_DENY_BUFFERS | D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES |
                                D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES)) == 0)
          {
            RDCWARN(
                "Adding DENY_RT_DS_TEXTURES|DENY_NON_RT_DS_TEXTURES to OpenExistingHeap heap for "
                "tier 1 compatibility");
            heapDesc.Flags |=
                D3D12_HEAP_FLAG_DENY_RT_DS_TEXTURES | D3D12_HEAP_FLAG_DENY_NON_RT_DS_TEXTURES;
          }
        }
      }

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateHeapFromAddress1);
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
  else
  {
    CHECK_HR(this, ret);
  }

  return ret;
}
