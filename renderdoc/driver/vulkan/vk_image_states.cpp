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

#include "vk_resources.h"

template <typename Barrier>
void BarrierSequence<Barrier>::AddWrapped(uint32_t batchIndex, uint32_t queueFamilyIndex,
                                          const Barrier &barrier)
{
  RDCASSERT(batchIndex < MAX_BATCH_COUNT);
  RDCASSERT(queueFamilyIndex < MAX_QUEUE_FAMILY_COUNT);
  batches[batchIndex][queueFamilyIndex].push_back(barrier);
  ++barrierCount;
}
template void BarrierSequence<VkImageMemoryBarrier>::AddWrapped(uint32_t batchIndex,
                                                                uint32_t queueFamilyIndex,
                                                                const VkImageMemoryBarrier &barrier);

template <typename Barrier>
void BarrierSequence<Barrier>::Merge(const BarrierSequence<Barrier> &other)
{
  for(uint32_t batchIndex = 0; batchIndex < MAX_BATCH_COUNT; ++batchIndex)
  {
    rdcarray<Barrier> *batch = batches[batchIndex];
    const rdcarray<Barrier> *otherBatch = other.batches[batchIndex];
    for(uint32_t queueFamilyIndex = 0; queueFamilyIndex < MAX_QUEUE_FAMILY_COUNT; ++queueFamilyIndex)
    {
      rdcarray<Barrier> &barriers = batch[queueFamilyIndex];
      const rdcarray<Barrier> &otherBarriers = otherBatch[queueFamilyIndex];
      barriers.insert(barriers.size(), otherBarriers.begin(), otherBarriers.size());
      barrierCount += otherBarriers.size();
    }
  }
}
template void BarrierSequence<VkImageMemoryBarrier>::Merge(
    const BarrierSequence<VkImageMemoryBarrier> &other);

template <typename Barrier>
bool BarrierSequence<Barrier>::IsBatchEmpty(uint32_t batchIndex) const
{
  if(batchIndex > MAX_BATCH_COUNT)
    return true;
  for(uint32_t queueFamilyIndex = 0; queueFamilyIndex < MAX_QUEUE_FAMILY_COUNT; ++queueFamilyIndex)
  {
    if(!batches[batchIndex][queueFamilyIndex].empty())
      return false;
  }
  return true;
}
template bool BarrierSequence<VkImageMemoryBarrier>::IsBatchEmpty(uint32_t batchIndex) const;

template <>
void BarrierSequence<VkImageMemoryBarrier>::UnwrapBarriers(rdcarray<VkImageMemoryBarrier> &barriers)
{
  for(auto it = barriers.begin(); it != barriers.end(); ++it)
  {
    it->image = ::Unwrap(it->image);
  }
}

template <typename Barrier>
void BarrierSequence<Barrier>::ExtractUnwrappedBatch(uint32_t batchIndex, uint32_t queueFamilyIndex,
                                                     rdcarray<Barrier> &result)
{
  if(batchIndex >= MAX_BATCH_COUNT || queueFamilyIndex >= MAX_QUEUE_FAMILY_COUNT)
    return;
  rdcarray<Barrier> &batch = batches[batchIndex][queueFamilyIndex];
  batch.swap(result);
  batch.clear();
  barrierCount -= result.size();
  UnwrapBarriers(result);
}
template void BarrierSequence<VkImageMemoryBarrier>::ExtractUnwrappedBatch(
    uint32_t batchIndex, uint32_t queueFamilyIndex, rdcarray<VkImageMemoryBarrier> &result);

template <typename Barrier>
void BarrierSequence<Barrier>::ExtractFirstUnwrappedBatchForQueue(uint32_t queueFamilyIndex,
                                                                  rdcarray<Barrier> &result)
{
  for(uint32_t batchIndex = 0; batchIndex < MAX_BATCH_COUNT; ++batchIndex)
  {
    if(!IsBatchEmpty(batchIndex))
    {
      batches[batchIndex][queueFamilyIndex].swap(result);
      batches[batchIndex][queueFamilyIndex].clear();
      barrierCount -= result.size();
      UnwrapBarriers(result);
      return;
    }
  }
}
template void BarrierSequence<VkImageMemoryBarrier>::ExtractFirstUnwrappedBatchForQueue(
    uint32_t queueFamilyIndex, rdcarray<VkImageMemoryBarrier> &result);

template <typename Barrier>
void BarrierSequence<Barrier>::ExtractLastUnwrappedBatchForQueue(uint32_t queueFamilyIndex,
                                                                 rdcarray<Barrier> &result)
{
  for(uint32_t batchIndex = MAX_BATCH_COUNT; batchIndex > 0;)
  {
    --batchIndex;
    if(!IsBatchEmpty(batchIndex))
    {
      batches[batchIndex][queueFamilyIndex].swap(result);
      batches[batchIndex][queueFamilyIndex].clear();
      barrierCount -= result.size();
      UnwrapBarriers(result);
      return;
    }
  }
}
template void BarrierSequence<VkImageMemoryBarrier>::ExtractLastUnwrappedBatchForQueue(
    uint32_t queueFamilyIndex, rdcarray<VkImageMemoryBarrier> &result);
