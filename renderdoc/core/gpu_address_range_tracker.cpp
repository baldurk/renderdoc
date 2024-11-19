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

#include "core/gpu_address_range_tracker.h"

void GPUAddressRangeTracker::AddTo(const GPUAddressRange &range)
{
  SCOPED_WRITELOCK(addressLock);
  auto it = std::lower_bound(addresses.begin(), addresses.end(), range.start);

  addresses.insert(it - addresses.begin(), range);
}

void GPUAddressRangeTracker::RemoveFrom(const GPUAddressRange &range)
{
  {
    SCOPED_WRITELOCK(addressLock);
    size_t i = std::lower_bound(addresses.begin(), addresses.end(), range.start) - addresses.begin();

    // there might be multiple buffers with the same range start, find the exact range for this
    // buffer
    while(i < addresses.size() && addresses[i].start == range.start)
    {
      if(addresses[i].id == range.id)
      {
        addresses.erase(i);
        return;
      }

      ++i;
    }
  }

  RDCERR("Couldn't find matching range to remove for %s", ToStr(range.id).c_str());
}

void GPUAddressRangeTracker::GetResIDFromAddr(GPUAddressRange::Address addr, ResourceId &id,
                                              uint64_t &offs)
{
  id = ResourceId();
  offs = 0;

  if(addr == 0)
    return;

  GPUAddressRange range;

  {
    SCOPED_READLOCK(addressLock);

    auto it = std::lower_bound(addresses.begin(), addresses.end(), addr);
    if(it == addresses.end())
      return;

    range = *it;

    // find the largest resource containing this address - not perfect but helps with trivially bad
    // aliases where a tiny resource and a large resource are co-situated and the larger resource
    // needs to be used for validity
    while((it + 1) != addresses.end() && (it + 1)->start <= addr && (it + 1)->realEnd > range.realEnd)
    {
      it++;
      range = *it;
    }
  }

  if(addr < range.start || addr >= range.realEnd)
    return;

  id = range.id;
  offs = addr - range.start;
}

void GPUAddressRangeTracker::GetResIDFromAddrAllowOutOfBounds(GPUAddressRange::Address addr,
                                                              ResourceId &id, uint64_t &offs)
{
  id = ResourceId();
  offs = 0;

  if(addr == 0)
    return;

  GPUAddressRange range;

  {
    SCOPED_READLOCK(addressLock);

    auto it = std::lower_bound(addresses.begin(), addresses.end(), addr);
    if(it == addresses.end())
      return;

    range = *it;

    // find the largest resource containing this address - not perfect but helps with trivially bad
    // aliases where a tiny resource and a large resource are co-situated and the larger resource
    // needs to be used for validity
    while((it + 1) != addresses.end() && (it + 1)->start <= addr && (it + 1)->realEnd > range.realEnd)
    {
      it++;
      range = *it;
    }
  }

  if(addr < range.start)
    return;

  // still enforce the OOB end on ranges - which is the remaining range in the backing store.
  // Otherwise we could end up passing through invalid addresses stored in stale descriptors
  if(addr >= range.oobEnd)
    return;

  id = range.id;
  offs = addr - range.start;
}

void GPUAddressRangeTracker::GetResIDBoundForAddr(GPUAddressRange::Address addr, ResourceId &lower,
                                                  GPUAddressRange::Address &lowerVA,
                                                  ResourceId &upper,
                                                  GPUAddressRange::Address &upperVA)
{
  lower = upper = ResourceId();
  lowerVA = upperVA = 0;

  if(addr == 0)
    return;

  if(addresses.empty())
    return;

  {
    SCOPED_READLOCK(addressLock);

    auto it = std::lower_bound(addresses.begin(), addresses.end(), addr);
    if(it == addresses.end())
    {
      --it;

      lower = it->id;
      lowerVA = it->start;
      return;
    }

    // find the last resource containing this address if there are multiple overlapping
    while((it + 1) != addresses.end() && (it + 1)->start <= addr && (it + 1)->realEnd > addr)
    {
      it++;
    }

    lower = it->id;
    lowerVA = it->start;

    // if this range contains the address exactly, return it as a tight bound
    if(it->realEnd > addr)
    {
      upper = it->id;
      upperVA = it->realEnd;
    }

    // otherwise the address is past its end but before the next. Move one allocation along - we
    // already know that we picked the largest allocation that covers this address
    ++it;

    // if this wasn't the end, return the upper bound
    if(it != addresses.end())
    {
      upper = it->id;
      upperVA = it->realEnd;
    }
  }
}
