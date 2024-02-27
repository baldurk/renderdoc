/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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

void WrappedID3D12Device::CreateSampler2(const D3D12_SAMPLER_DESC2 *pDesc,
                                         D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor)
{
  bool capframe = false;

  {
    SCOPED_READLOCK(m_CapTransitionLock);
    capframe = IsActiveCapturing(m_State);
  }

  SERIALISE_TIME_CALL(m_pDevice11->CreateSampler2(pDesc, Unwrap(DestDescriptor)));

  // assume descriptors are volatile
  if(capframe)
  {
    DynamicDescriptorWrite write;
    write.desc.Init(pDesc);
    write.dest = GetWrapped(DestDescriptor);

    {
      CACHE_THREAD_SERIALISER();

      SCOPED_SERIALISE_CHUNK(D3D12Chunk::Device_CreateSampler2);
      Serialise_DynamicDescriptorWrite(ser, &write);

      m_FrameCaptureRecord->AddChunk(scope.Get());
    }
  }

  GetWrapped(DestDescriptor)->Init(pDesc);
}
