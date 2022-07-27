/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Baldur Karlsson
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

#ifndef SHADER_MODEL_MIN_6_0_REQUIRED
#define SHADER_MODEL_MIN_6_0_REQUIRED
#endif
#include "hlsl_cbuffers.h"

RWStructuredBuffer<InstanceDesc> instanceDescs : register(u0, space0);
StructuredBuffer<BlasAddressPair> oldNewAddressesPair : register(t0, space0);

bool InRange(BlasAddressRange addressRange, GPUAddress address)
{
  if(addressRange.start <= address && address <= addressRange.end)
  {
    return true;
  }

  return false;
}

// Each SV_GroupId corresponds to each of the BLAS (instance) in TLAS
[numthreads(1, 1, 1)] void RENDERDOC_PatchAccStructAddressCS(uint3 dispatchGroup
                                                             : SV_GroupId) {
  GPUAddress instanceBlasAddress = instanceDescs[dispatchGroup.x].blasAddress;

  for(uint i = 0; i < addressCount; i++)
  {
    if(InRange(oldNewAddressesPair[i].oldAddress, instanceBlasAddress))
    {
      uint64_t offset = instanceBlasAddress - oldNewAddressesPair[i].oldAddress.start;
      instanceDescs[dispatchGroup.x].blasAddress = oldNewAddressesPair[i].newAddress.start + offset;
      return;
    }
  }

  // This  might cause device hang but at least we won't access incorrect addresses
  instanceDescs[dispatchGroup.x].blasAddress = 0;
}
