/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include "vk_manager.h"
#include <algorithm>
#include "vk_core.h"

// debugging logging for barriers
#if 0
#define TRDBG(...) RDCLOG(__VA_ARGS__)
#else
#define TRDBG(...)
#endif

template <typename SrcBarrierType>
void VulkanResourceManager::RecordSingleBarrier(
    rdcarray<rdcpair<ResourceId, ImageRegionState>> &dststates, ResourceId id,
    const SrcBarrierType &t, uint32_t nummips, uint32_t numslices)
{
  // if this is a single barrier for depth and stencil, and we are handling separate depth/stencil,
  // split it to ease processing
  if(m_Core->SeparateDepthStencil() &&
     t.subresourceRange.aspectMask == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
  {
    SrcBarrierType tmp = t;
    tmp.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    RecordSingleBarrier(dststates, id, tmp, nummips, numslices);
    tmp.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
    RecordSingleBarrier(dststates, id, tmp, nummips, numslices);
    return;
  }

  bool done = false;

  size_t i = 0;
  for(; i < dststates.size(); i++)
  {
    rdcpair<ResourceId, ImageRegionState> &state = dststates[i];

    // image barriers are handled by initially inserting one subresource range for each aspect,
    // and whenever we need more fine-grained detail we split it immediately for one range for
    // each subresource in that aspect. Thereafter if a barrier comes in that covers multiple
    // subresources, we update all matching ranges.

    // find the states matching this id
    if(state.first < id)
      continue;
    if(state.first != id)
      break;

    // skip states that don't match aspect mask when handling separate aspects for depth/stencil
    if(m_Core->SeparateDepthStencil() &&
       state.second.subresourceRange.aspectMask != t.subresourceRange.aspectMask)
      continue;

    state.second.dstQueueFamilyIndex = t.dstQueueFamilyIndex;

    {
      // we've found a range that completely matches our region, doesn't matter if that's
      // a whole image and the barrier is the whole image, or it's one subresource.
      // note that for images with only one array/mip slice (e.g. render targets) we'll never
      // really have to worry about the else{} branch
      if(state.second.subresourceRange.baseMipLevel == t.subresourceRange.baseMipLevel &&
         state.second.subresourceRange.levelCount == nummips &&
         state.second.subresourceRange.baseArrayLayer == t.subresourceRange.baseArrayLayer &&
         state.second.subresourceRange.layerCount == numslices)
      {
        // verify
        // RDCASSERT(it->second.newLayout == t.oldLayout);

        // apply it (prevstate is from the start of all barriers accumulated, so only set once)
        if(state.second.oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
          state.second.oldLayout = t.oldLayout;
        state.second.newLayout = t.newLayout;

        done = true;
        break;
      }
      else
      {
        // this handles the case where the barrier covers a number of subresources and we need
        // to update each matching subresource. If the barrier was only one mip & array slice
        // it would have hit the case above. Find each subresource within the range, update it,
        // and continue (marking as done so whenever we stop finding matching ranges, we are
        // satisfied.
        //
        // note that regardless of how we lay out our subresources (slice-major or mip-major) the
        // new range could be sparse, but that's OK as we only break out of the loop once we go past
        // the whole aspect. Any subresources that don't match the range, after the split, will fail
        // to meet any of the handled cases, so we'll just continue processing.
        if(state.second.subresourceRange.levelCount == 1 &&
           state.second.subresourceRange.layerCount == 1 &&
           state.second.subresourceRange.baseMipLevel >= t.subresourceRange.baseMipLevel &&
           state.second.subresourceRange.baseMipLevel < t.subresourceRange.baseMipLevel + nummips &&
           state.second.subresourceRange.baseArrayLayer >= t.subresourceRange.baseArrayLayer &&
           state.second.subresourceRange.baseArrayLayer <
               t.subresourceRange.baseArrayLayer + numslices)
        {
          // apply it (prevstate is from the start of all barriers accumulated, so only set once)
          if(state.second.oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
            state.second.oldLayout = t.oldLayout;
          state.second.newLayout = t.newLayout;

          // continue as there might be more, but we're done
          done = true;
          continue;
        }
        // finally handle the case where we have a range that covers a whole image but we need to
        // split it. If the barrier covered the whole image too it would have hit the very first
        // case, so we know that the barrier doesn't cover the whole range.
        // Also, if we've already done the split this case won't be hit and we'll either fall into
        // the case above, or we'll finish as we've covered the whole barrier.
        else if(state.second.subresourceRange.levelCount > 1 ||
                state.second.subresourceRange.layerCount > 1)
        {
          const uint32_t levelCount = state.second.subresourceRange.levelCount;
          const uint32_t layerCount = state.second.subresourceRange.layerCount;

          size_t count = levelCount * layerCount;

          // reset layer/level count
          state.second.subresourceRange.levelCount = 1;
          state.second.subresourceRange.layerCount = 1;

          rdcpair<ResourceId, ImageRegionState> existing = state;

          // insert new copies of the current state to expand out the subresources. Only insert
          // count-1 as we want count entries total - one per subresource
          for(size_t sub = 0; sub < count - 1; sub++)
            dststates.insert(i, existing);

          for(size_t sub = 0; sub < count; sub++)
          {
            rdcpair<ResourceId, ImageRegionState> &subState = dststates[i + sub];

            // slice-major, update base of each subresource
            subState.second.subresourceRange.baseArrayLayer = uint32_t(sub / levelCount);
            subState.second.subresourceRange.baseMipLevel = uint32_t(sub % levelCount);
          }

          // can't use state here, as it may no longer be valid if the inserts above resized the
          // array
          rdcpair<ResourceId, ImageRegionState> &firstState = dststates[i];

          // the loop will continue after this point and look at the next subresources
          // so we need to check to see if the first subresource lies in the range here
          if(firstState.second.subresourceRange.baseMipLevel >= t.subresourceRange.baseMipLevel &&
             firstState.second.subresourceRange.baseMipLevel <
                 t.subresourceRange.baseMipLevel + nummips &&
             firstState.second.subresourceRange.baseArrayLayer >= t.subresourceRange.baseArrayLayer &&
             firstState.second.subresourceRange.baseArrayLayer <
                 t.subresourceRange.baseArrayLayer + numslices)
          {
            // apply it (prevstate is from the start of all barriers accumulated, so only set
            // once)
            if(firstState.second.oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
              firstState.second.oldLayout = t.oldLayout;
            firstState.second.newLayout = t.newLayout;

            // continue as there might be more, but we're done
            done = true;
          }

          // continue processing from here
          continue;
        }
      }
    }

    // otherwise continue to try and find the subresource range
  }

  if(done)
    return;

  // we don't have an existing barrier for this memory region, insert into place. it points to
  // where it should be inserted
  VkImageSubresourceRange subRange = t.subresourceRange;
  subRange.levelCount = nummips;
  subRange.layerCount = numslices;
  dststates.insert(i, make_rdcpair(id, ImageRegionState(VK_QUEUE_FAMILY_IGNORED, subRange,
                                                        t.oldLayout, t.newLayout)));
}

void VulkanResourceManager::RecordBarriers(rdcarray<rdcpair<ResourceId, ImageRegionState>> &states,
                                           const std::map<ResourceId, ImageLayouts> &layouts,
                                           uint32_t numBarriers, const VkImageMemoryBarrier *barriers)
{
  TRDBG("Recording %u barriers", numBarriers);

  for(uint32_t ti = 0; ti < numBarriers; ti++)
  {
    const VkImageMemoryBarrier &t = barriers[ti];

    // ignore barriers that are do-nothing. Best case this doesn't change our tracking at all and
    // worst case this is a KHR_synchronization2 barrier that should not change the layout.
    if(t.oldLayout == t.newLayout)
      continue;

    ResourceId id = IsReplayMode(m_State) ? GetNonDispWrapper(t.image)->id : GetResID(t.image);

    if(id == ResourceId())
    {
      RDCERR("Couldn't get ID for image %p in barrier", GetWrapped(t.image));
      continue;
    }

    uint32_t nummips = t.subresourceRange.levelCount;
    uint32_t numslices = t.subresourceRange.layerCount;

    auto it = layouts.find(id);

    if(nummips == VK_REMAINING_MIP_LEVELS)
    {
      if(it != layouts.end())
        nummips = it->second.imageInfo.levelCount - t.subresourceRange.baseMipLevel;
      else
        nummips = 1;
    }

    if(numslices == VK_REMAINING_ARRAY_LAYERS)
    {
      if(it != layouts.end())
        numslices = it->second.imageInfo.layerCount - t.subresourceRange.baseArrayLayer;
      else
        numslices = 1;
    }

    RecordSingleBarrier(states, id, t, nummips, numslices);
  }

  TRDBG("Post-record, there are %u states", (uint32_t)states.size());
}

void VulkanResourceManager::RecordBarriers(rdcflatmap<ResourceId, ImageState> &states,
                                           uint32_t queueFamilyIndex, uint32_t numBarriers,
                                           const VkImageMemoryBarrier2 *barriers)
{
  rdcarray<VkImageMemoryBarrier> downcast;
  downcast.reserve(numBarriers);
  VkImageMemoryBarrier b = {};
  b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  for(uint32_t i = 0; i < numBarriers; i++)
  {
    // just truncate, the lower bits all match
    b.srcAccessMask = uint32_t(barriers[i].srcAccessMask);
    b.dstAccessMask = uint32_t(barriers[i].dstAccessMask);
    b.oldLayout = barriers[i].oldLayout;
    b.newLayout = barriers[i].newLayout;
    b.srcQueueFamilyIndex = barriers[i].srcQueueFamilyIndex;
    b.dstQueueFamilyIndex = barriers[i].dstQueueFamilyIndex;
    b.image = barriers[i].image;
    b.subresourceRange = barriers[i].subresourceRange;
    downcast.push_back(b);
  }
  RecordBarriers(states, queueFamilyIndex, (uint32_t)downcast.size(), downcast.data());
}

void VulkanResourceManager::MergeBarriers(rdcarray<rdcpair<ResourceId, ImageRegionState>> &dststates,
                                          rdcarray<rdcpair<ResourceId, ImageRegionState>> &srcstates)
{
  TRDBG("Merging %u states", (uint32_t)srcstates.size());

  for(size_t ti = 0; ti < srcstates.size(); ti++)
  {
    const ImageRegionState &t = srcstates[ti].second;
    RecordSingleBarrier(dststates, srcstates[ti].first, t, t.subresourceRange.levelCount,
                        t.subresourceRange.layerCount);
  }

  TRDBG("Post-merge, there are %u states", (uint32_t)dststates.size());
}

template <typename SerialiserType>
void VulkanResourceManager::SerialiseImageStates(SerialiserType &ser,
                                                 std::map<ResourceId, LockingImageState> &states)
{
  SERIALISE_ELEMENT_LOCAL(NumImages, (uint32_t)states.size()).Important();

  auto srcit = states.begin();

  for(uint32_t i = 0; i < NumImages; i++)
  {
    SERIALISE_ELEMENT_LOCAL(Image, (ResourceId)(srcit->first)).TypedAs("VkImage"_lit);
    if(ser.IsWriting())
    {
      LockedImageStateRef lockedState = srcit->second.LockWrite();
      ::ImageState &ImageState = *lockedState;
      SERIALISE_ELEMENT(ImageState);
      ++srcit;
    }
    else
    {
      bool hasLiveRes = HasLiveResource(Image);

      ImageState imageState;

      if(ser.VersionLess(0x11))
      {
        ImageLayouts imageLayouts;
        {
          ImageLayouts &ImageState = imageLayouts;
          SERIALISE_ELEMENT(ImageState);
        }
        if(IsReplayingAndReading() && hasLiveRes)
        {
          if(imageLayouts.imageInfo.extent.depth > 1)
            imageLayouts.imageInfo.imageType = VK_IMAGE_TYPE_3D;

          imageState = ImageState(VK_NULL_HANDLE, imageLayouts.imageInfo, eFrameRef_Unknown);

          rdcarray<ImageSubresourceStateForRange> subresourceStates;
          subresourceStates.reserve(imageLayouts.subresourceStates.size());

          for(ImageRegionState &st : imageLayouts.subresourceStates)
          {
            ImageSubresourceStateForRange p;
            p.range = st.subresourceRange;
            p.range.sliceCount = imageLayouts.imageInfo.extent.depth;
            p.state.oldQueueFamilyIndex = m_Core->RemapQueue(st.dstQueueFamilyIndex);
            p.state.newQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            p.state.oldLayout = st.newLayout;
            p.state.newLayout = imageState.GetImageInfo().initialLayout;
            p.state.refType = eFrameRef_Unknown;
            subresourceStates.push_back(p);
          }

          if(!subresourceStates.empty())
          {
            std::sort(subresourceStates.begin(), subresourceStates.end(),
                      ImageSubresourceStateForRange::CompareRangeBegin);
            imageState.subresourceStates.FromArray(subresourceStates);
          }
          imageState.maxRefType = eFrameRef_Unknown;
        }
      }
      else
      {
        {
          ::ImageState &ImageState = imageState;
          SERIALISE_ELEMENT(ImageState);
        }
        if(IsReplayingAndReading() && hasLiveRes)
        {
          imageState.newQueueFamilyTransfers.clear();
          for(auto it = imageState.subresourceStates.begin();
              it != imageState.subresourceStates.end(); ++it)
          {
            // Set the current image state (`newLayout`, `newQueueFamilyIndex`, `refType`) to the
            // initial image state, so that calling `ResetToOldState` will move the image from the
            // initial state to the state it was in at the beginning of the capture.
            ImageSubresourceState &state = it->state();
            state.newLayout = imageState.GetImageInfo().initialLayout;
            state.oldQueueFamilyIndex = m_Core->RemapQueue(state.oldQueueFamilyIndex);
            state.newQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
          }
        }
      }
      if(hasLiveRes)
      {
        ResourceId liveid = GetLiveID(Image);

        if(IsLoading(m_State))
        {
          auto stit = states.find(liveid);
          if(stit == states.end())
          {
            imageState.subresourceStates.Unsplit();
            states.insert({liveid, LockingImageState(imageState)});
          }
          else
          {
            auto st = stit->second.LockWrite();
            st->MergeCaptureBeginState(imageState);
            st->subresourceStates.Unsplit();
          }
        }
        else if(IsActiveReplaying(m_State))
        {
          auto current = states.find(liveid)->second.LockRead();
          auto stit = states.find(liveid);
          for(auto subit = imageState.subresourceStates.begin();
              subit != imageState.subresourceStates.end(); ++subit)
          {
            uint32_t aspectIndex = 0;
            for(auto it = ImageAspectFlagIter::begin(imageState.GetImageInfo().Aspects());
                it != ImageAspectFlagIter::end() && ((*it) & subit->range().aspectMask) == 0;
                ++it, ++aspectIndex)
            {
            }
            auto currentSub = current->subresourceStates.SubresourceIndexValue(
                aspectIndex, subit->range().baseMipLevel, subit->range().baseArrayLayer,
                subit->range().baseDepthSlice);
            FrameRefType expectedRef = subit->state().refType;
            if(current->m_Storage)
              expectedRef = eFrameRef_ReadBeforeWrite;
            RDCASSERT(currentSub.refType == expectedRef || expectedRef == eFrameRef_Unknown);
            RDCASSERT(currentSub.oldLayout == subit->state().oldLayout ||
                      subit->state().oldLayout == UNKNOWN_PREV_IMG_LAYOUT);
            RDCASSERT(currentSub.oldQueueFamilyIndex == subit->state().oldQueueFamilyIndex ||
                      subit->state().oldQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED);
          }
        }
      }
    }
  }
}
template void VulkanResourceManager::SerialiseImageStates(
    WriteSerialiser &ser, std::map<ResourceId, LockingImageState> &states);
template void VulkanResourceManager::SerialiseImageStates(
    ReadSerialiser &ser, std::map<ResourceId, LockingImageState> &states);

template <class SerialiserType>
void DoSerialise(SerialiserType &ser, MemRefInterval &el)
{
  SERIALISE_MEMBER(memory);
  SERIALISE_MEMBER(start);
  SERIALISE_MEMBER(refType);
}

template <typename SerialiserType>
bool VulkanResourceManager::Serialise_DeviceMemoryRefs(SerialiserType &ser,
                                                       rdcarray<MemRefInterval> &data)
{
  SERIALISE_ELEMENT(data);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // unpack data into m_MemFrameRefs
    auto it_data = data.begin();
    while(it_data != data.end())
    {
      ResourceId mem = it_data->memory;

      auto res = m_MemFrameRefs.insert(std::pair<ResourceId, MemRefs>(mem, MemRefs()));
      RDCASSERTMSG("MemRefIntervals for each memory resource must be contiguous", res.second);
      Intervals<FrameRefType> &rangeRefs = res.first->second.rangeRefs;

      auto it_ints = rangeRefs.begin();
      uint64_t last = 0;
      FrameRefType lastRef = eFrameRef_None;
      while(it_data != data.end() && it_data->memory == mem)
      {
        uint64_t start = it_data->start;
        if(start & 0x3)
        {
          // start is not a multiple of 4. We need to shift start to a multiple of 4 to satisfy the
          // alignment requirements of `vkCmdFillBuffer`.

          uint64_t nextDWord = AlignUp4(start);

          // Compute the overall ref type for the dword, including all the ref types of intervals
          // intersecting the dword
          FrameRefType overlapRef = lastRef;
          for(; it_data != data.end() && it_data->start < nextDWord && it_data->memory == mem;
              ++it_data)
            overlapRef = ComposeFrameRefsDisjoint(overlapRef, it_data->refType);

          --it_data;
          // it_data now points to the last interval intersecting the dword.

          if(overlapRef == lastRef)
          {
            // The ref type for the overlap dword is the same as the ref type of the previous
            // interval; move the entire overlap dword into the previous interval, which means the
            // start of this interval moves up to the the next higher dword.
            start = nextDWord;
          }
          else if(overlapRef == it_data->refType)
          {
            // The ref type for the overlap dword is the same as for this interval; move the entire
            // overlap dword into this interval, which means the start of this interval moves down
            // to the next lower dword.
            start = nextDWord - 4;
          }
          else
          {
            // The ref type of the overlap dword matches neither the previous interval nor this
            // interval; insert a new interval for the overlap.
            if(last < nextDWord - 4)
              it_ints->split(nextDWord - 4);
            it_ints->setValue(overlapRef);
            last = nextDWord - 4;
            start = nextDWord;
          }
        }
        RDCASSERTMSG("MemRefInterval starts must be increasing", start >= last);

        if(last < start)
          it_ints->split(start);
        it_ints->setValue(it_data->refType);
        last = start;
        lastRef = it_data->refType;
        it_data++;
      }
    }
  }

  return true;
}

template bool VulkanResourceManager::Serialise_DeviceMemoryRefs(ReadSerialiser &ser,
                                                                rdcarray<MemRefInterval> &data);
template bool VulkanResourceManager::Serialise_DeviceMemoryRefs(WriteSerialiser &ser,
                                                                rdcarray<MemRefInterval> &data);

bool VulkanResourceManager::Serialise_ImageRefs(ReadSerialiser &ser,
                                                std::map<ResourceId, LockingImageState> &states)
{
  rdcarray<ImgRefsPair> data;
  SERIALISE_ELEMENT(data);

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // unpack data into states
    for(auto it = data.begin(); it != data.end(); ++it)
    {
      if(!HasLiveResource(it->image))
        continue;
      ResourceId liveid = GetLiveID(it->image);

      auto stit = states.find(liveid);
      if(stit == states.end())
      {
        RDCWARN("Found ImgRefs for unknown image");
      }
      else
      {
        LockedImageStateRef imst = stit->second.LockWrite();
        imst->subresourceStates.FromImgRefs(it->imgRefs);
        FrameRefType maxRefType = eFrameRef_None;
        for(auto subit = imst->subresourceStates.begin(); subit != imst->subresourceStates.end();
            ++subit)
        {
          maxRefType = ComposeFrameRefsDisjoint(maxRefType, subit->state().refType);
        }
        imst->maxRefType = maxRefType;
      }
    }
  }

  return true;
}

void VulkanResourceManager::InsertDeviceMemoryRefs(WriteSerialiser &ser)
{
  rdcarray<MemRefInterval> data;
  MemRefs emptyMemRefs;
  for(auto memIt = m_DeviceMemories.begin(); memIt != m_DeviceMemories.end(); memIt++)
  {
    MemRefs *memRefs = NULL;
    auto it = m_MemFrameRefs.find(*memIt);
    if(it == m_MemFrameRefs.end())
      memRefs = &emptyMemRefs;
    else
      memRefs = &it->second;
    Intervals<FrameRefType> &rangeRefs = memRefs->rangeRefs;
    for(auto jt = rangeRefs.begin(); jt != rangeRefs.end(); jt++)
      data.push_back({*memIt, jt->start(), jt->value()});
  }

  for(ResourceId dead : m_DeadDeviceMemories)
    m_DeviceMemories.erase(dead);
  m_DeadDeviceMemories.clear();

  uint64_t sizeEstimate = data.size() * sizeof(MemRefInterval) + 32;

  {
    SCOPED_SERIALISE_CHUNK(VulkanChunk::DeviceMemoryRefs, sizeEstimate);
    Serialise_DeviceMemoryRefs(ser, data);
  }
}

void VulkanResourceManager::MarkSparseMapReferenced(const ResourceInfo *sparse)
{
  if(sparse == NULL)
  {
    RDCERR("Unexpected NULL sparse mapping");
    return;
  }

  for(size_t a = 0; a <= sparse->altSparseAspects.size(); a++)
  {
    const Sparse::PageTable &table = a < sparse->altSparseAspects.size()
                                         ? sparse->altSparseAspects[a].table
                                         : sparse->sparseTable;

    uint32_t numSubs = table.getNumSubresources();
    const Sparse::MipTail &mipTail = table.getMipTail();
    for(uint32_t s = 0; s < numSubs + mipTail.mappings.size(); s++)
    {
      const Sparse::PageRangeMapping &mapping =
          s < numSubs ? table.getSubresource(s) : table.getMipTail().mappings[s - numSubs];

      if(s < numSubs && table.isSubresourceInMipTail(s))
        continue;

      if(mapping.hasSingleMapping())
      {
        MarkMemoryFrameReferenced(
            mapping.singleMapping.memory, mapping.singleMapping.offset,
            mapping.singlePageReused ? table.getPageByteSize() : table.getSubresourceByteSize(s),
            eFrameRef_Read);
      }
      else
      {
        // this is a huge perf cliff as we've lost any batching and we perform as badly as if every
        // page was mapped to a different resource, so we hope applications don't hit this often.
        for(const Sparse::Page &page : mapping.pages)
        {
          MarkMemoryFrameReferenced(page.memory, page.offset, table.getPageByteSize(),
                                    eFrameRef_Read);
        }
      }
    }
  }
}

void VulkanResourceManager::SetInternalResource(ResourceId id)
{
  if(!RenderDoc::Inst().IsReplayApp())
  {
    VkResourceRecord *record = GetResourceRecord(id);
    if(record)
      record->InternalResource = true;
  }
}

void VulkanResourceManager::ApplyBarriers(uint32_t queueFamilyIndex,
                                          rdcarray<rdcpair<ResourceId, ImageRegionState>> &states,
                                          std::map<ResourceId, ImageLayouts> &layouts)
{
  TRDBG("Applying %u barriers", (uint32_t)states.size());

  for(size_t ti = 0; ti < states.size(); ti++)
  {
    ResourceId id = states[ti].first;
    ImageRegionState &t = states[ti].second;

    TRDBG("Applying barrier to %s", ToStr(GetOriginalID(id)).c_str());

    auto stit = layouts.find(id);

    if(stit == layouts.end())
    {
      TRDBG("Didn't find ID in image layouts");
      continue;
    }

    // apply any ownership transfer
    stit->second.queueFamilyIndex = t.dstQueueFamilyIndex;

    // if there's no ownership transfer, it's implicitly owned by the current queue
    if(t.dstQueueFamilyIndex == VK_QUEUE_FAMILY_IGNORED)
      stit->second.queueFamilyIndex = queueFamilyIndex;

    const ImageInfo &imageInfo = stit->second.imageInfo;

    uint32_t nummips = t.subresourceRange.levelCount;
    uint32_t numslices = t.subresourceRange.layerCount;
    if(nummips == VK_REMAINING_MIP_LEVELS)
      nummips = imageInfo.levelCount;
    if(numslices == VK_REMAINING_ARRAY_LAYERS)
      numslices = imageInfo.layerCount;

    if(nummips == 0)
      nummips = 1;
    if(numslices == 0)
      numslices = 1;

    if(t.oldLayout == t.newLayout)
      continue;

    TRDBG("Barrier of %s (%u->%u, %u->%u) from %s to %s", ToStr(t.subresourceRange.aspect).c_str(),
          t.subresourceRange.baseMipLevel, t.subresourceRange.levelCount,
          t.subresourceRange.baseArrayLayer, t.subresourceRange.layerCount,
          ToStr(t.oldLayout).c_str(), ToStr(t.newLayout).c_str());

    bool done = false;

    TRDBG("Matching image has %u subresource states", stit->second.subresourceStates.size());

    for(size_t i = 0; i < stit->second.subresourceStates.size(); i++)
    {
      ImageRegionState &state = stit->second.subresourceStates[i];
      TRDBG(".. state %s (%u->%u, %u->%u) from %s to %s",
            ToStr(state.subresourceRange.aspect).c_str(), state.range.baseMipLevel,
            state.range.levelCount, state.range.baseArrayLayer, state.range.layerCount,
            ToStr(state.oldLayout).c_str(), ToStr(state.newLayout).c_str());

      // image barriers are handled by initially inserting one subresource range for the whole
      // object,
      // and whenever we need more fine-grained detail we split it immediately.
      // Thereafter if a barrier comes in that covers multiple subresources, we update all matching
      // ranges.

      // ignore states of different aspects when depth/stencil aspects are split
      if(m_Core->SeparateDepthStencil() &&
         state.subresourceRange.aspectMask != t.subresourceRange.aspectMask)
        continue;

      {
        // we've found a range that completely matches our region, doesn't matter if that's
        // a whole image and the barrier is the whole image, or it's one subresource.
        // note that for images with only one array/mip slice (e.g. render targets) we'll never
        // really have to worry about the else{} branch
        if(state.subresourceRange.baseMipLevel == t.subresourceRange.baseMipLevel &&
           state.subresourceRange.levelCount == nummips &&
           state.subresourceRange.baseArrayLayer == t.subresourceRange.baseArrayLayer &&
           state.subresourceRange.layerCount == numslices)
        {
          if(state.oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
            state.oldLayout = t.oldLayout;
          t.oldLayout = state.newLayout;
          state.newLayout = t.newLayout;

          done = true;
          break;
        }
        else
        {
          // this handles the case where the barrier covers a number of subresources and we need
          // to update each matching subresource. If the barrier was only one mip & array slice
          // it would have hit the case above. Find each subresource within the range, update it,
          // and continue (marking as done so whenever we stop finding matching ranges, we are
          // satisfied.
          //
          // note that regardless of how we lay out our subresources (slice-major or mip-major) the
          // new range could be sparse, but that's OK as we only break out of the loop once we go
          // past the whole aspect. Any subresources that don't match the range, after the split,
          // will fail to meet any of the handled cases, so we'll just continue processing.
          if(state.subresourceRange.levelCount == 1 && state.subresourceRange.layerCount == 1 &&
             state.subresourceRange.baseMipLevel >= t.subresourceRange.baseMipLevel &&
             state.subresourceRange.baseMipLevel < t.subresourceRange.baseMipLevel + nummips &&
             state.subresourceRange.baseArrayLayer >= t.subresourceRange.baseArrayLayer &&
             state.subresourceRange.baseArrayLayer < t.subresourceRange.baseArrayLayer + numslices)
          {
            // apply it (prevstate is from the start of all barriers accumulated, so only set once)
            if(state.oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
              state.oldLayout = t.oldLayout;
            t.oldLayout = state.newLayout;
            state.newLayout = t.newLayout;

            // continue as there might be more, but we're done
            done = true;
            continue;
          }
          // finally handle the case where we have a range that covers a whole image but we need to
          // split it. If the barrier covered the whole image too it would have hit the very first
          // case, so we know that the barrier doesn't cover the whole range.
          // Also, if we've already done the split this case won't be hit and we'll either fall into
          // the case above, or we'll finish as we've covered the whole barrier.
          else if(state.subresourceRange.levelCount > 1 || state.subresourceRange.layerCount > 1)
          {
            const uint32_t levelCount = state.subresourceRange.levelCount;
            const uint32_t layerCount = state.subresourceRange.layerCount;

            size_t count = levelCount * layerCount;

            // reset layer/level count
            state.subresourceRange.levelCount = 1;
            state.subresourceRange.layerCount = 1;

            // copy now, state will no longer be valid after inserting below
            ImageRegionState existing = state;

            // insert new copies of the current state to expand out the subresources. Only insert
            // count-1 as we want count entries total - one per subresource
            for(size_t sub = 0; sub < count - 1; sub++)
              stit->second.subresourceStates.insert(i, existing);

            for(size_t sub = 0; sub < count; sub++)
            {
              ImageRegionState &subState = stit->second.subresourceStates[i + sub];

              // slice-major, update base of each subresource
              subState.subresourceRange.baseArrayLayer = uint32_t(sub / levelCount);
              subState.subresourceRange.baseMipLevel = uint32_t(sub % levelCount);
            }

            // can't use state here, as it may no longer be valid if the inserts above resized the
            // array
            ImageRegionState &firstState = stit->second.subresourceStates[i];

            // the loop will continue after this point and look at the next subresources
            // so we need to check to see if the first subresource lies in the range here
            if(firstState.subresourceRange.baseMipLevel >= t.subresourceRange.baseMipLevel &&
               firstState.subresourceRange.baseMipLevel < t.subresourceRange.baseMipLevel + nummips &&
               firstState.subresourceRange.baseArrayLayer >= t.subresourceRange.baseArrayLayer &&
               firstState.subresourceRange.baseArrayLayer <
                   t.subresourceRange.baseArrayLayer + numslices)
            {
              // apply it (prevstate is from the start of all barriers accumulated, so only set
              // once)
              if(firstState.oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
                firstState.oldLayout = t.oldLayout;
              t.oldLayout = firstState.newLayout;
              firstState.newLayout = t.newLayout;

              // continue as there might be more, but we're done
              done = true;
            }

            // continue processing from here
            continue;
          }
        }
      }

      // otherwise continue to try and find the subresource range
    }

    if(!done)
      RDCERR("Couldn't find subresource range to apply barrier to - invalid!");
  }
}

void VulkanResourceManager::RecordBarriers(rdcflatmap<ResourceId, ImageState> &states,
                                           uint32_t queueFamilyIndex, uint32_t numBarriers,
                                           const VkImageMemoryBarrier *barriers)
{
  TRDBG("Recording %u barriers", numBarriers);

  for(uint32_t ti = 0; ti < numBarriers; ti++)
  {
    const VkImageMemoryBarrier &t = barriers[ti];

    // ignore barriers that are do-nothing. Best case this doesn't change our tracking at all and
    // worst case this is a KHR_synchronization2 barrier that should not change the layout.
    if(t.oldLayout == t.newLayout)
      continue;

    ResourceId id = IsReplayMode(m_State) ? GetNonDispWrapper(t.image)->id : GetResID(t.image);

    if(id == ResourceId())
    {
      RDCERR("Couldn't get ID for image in barrier");
      continue;
    }

    auto stateIt = states.find(id);
    if(stateIt == states.end())
    {
      LockedConstImageStateRef globalState = m_Core->FindConstImageState(id);
      if(!globalState)
      {
        RDCERR("Recording barrier for unknown image: %s", ToStr(id).c_str());
        continue;
      }
      stateIt = states.insert({id, globalState->CommandBufferInitialState()}).first;
    }

    ImageState &state = stateIt->second;
    state.RecordBarrier(t, queueFamilyIndex, m_Core->GetImageTransitionInfo());
  }

  TRDBG("Post-record, there are %u states", (uint32_t)states.size());
}

ResourceId VulkanResourceManager::GetFirstIDForHandle(uint64_t handle)
{
  for(auto it = m_CurrentResourceMap.begin(); it != m_CurrentResourceMap.end(); ++it)
  {
    WrappedVkRes *res = it->second;

    if(!res)
      continue;

    if(IsDispatchableRes(res))
    {
      WrappedVkDispRes *disp = (WrappedVkDispRes *)res;
      if(disp->real.handle == handle)
        return IsReplayMode(m_State) ? GetOriginalID(disp->id) : disp->id;
    }
    else
    {
      WrappedVkNonDispRes *nondisp = (WrappedVkNonDispRes *)res;
      if(nondisp->real.handle == handle)
        return IsReplayMode(m_State) ? GetOriginalID(nondisp->id) : nondisp->id;
    }
  }

  return ResourceId();
}

void VulkanResourceManager::MarkMemoryFrameReferenced(ResourceId mem, VkDeviceSize offset,
                                                      VkDeviceSize size, FrameRefType refType)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  FrameRefType maxRef = MarkMemoryReferenced(m_MemFrameRefs, mem, offset, size, refType);
  if(IsCompleteWriteFrameRef(maxRef))
  {
    // check and make sure this is really a CompleteWrite
    VkResourceRecord *record = GetResourceRecord(mem);
    // if we are not writing the entire memory, degrade it to a partial write
    if(offset != 0 || size != record->Length)
      maxRef = eFrameRef_PartialWrite;
  }
  MarkResourceFrameReferenced(mem, maxRef, ComposeFrameRefsDisjoint);
}

void VulkanResourceManager::AddMemoryFrameRefs(ResourceId mem)
{
  m_MemFrameRefs[mem] = MemRefs();
}

void VulkanResourceManager::AddDeviceMemory(ResourceId mem)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  m_DeviceMemories.insert(mem);
}

void VulkanResourceManager::RemoveDeviceMemory(ResourceId mem)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  if(IsActiveCapturing(m_State))
    m_DeadDeviceMemories.push_back(mem);
  else
    m_DeviceMemories.erase(mem);
}

void VulkanResourceManager::MergeReferencedMemory(std::unordered_map<ResourceId, MemRefs> &memRefs)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  for(auto j = memRefs.begin(); j != memRefs.end(); j++)
  {
    auto i = m_MemFrameRefs.find(j->first);
    if(i == m_MemFrameRefs.end())
      m_MemFrameRefs[j->first] = j->second;
    else
      i->second.Merge(j->second);
  }
}

void VulkanResourceManager::FixupStorageBufferMemory(
    const std::unordered_set<VkResourceRecord *> &storageBuffers)
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  for(VkResourceRecord *buf : storageBuffers)
  {
    ResourceId id = buf->GetResourceID();
    MemRefs ref;
    ref.Update(buf->memOffset, buf->memSize, eFrameRef_ReadBeforeWrite);

    auto i = m_MemFrameRefs.find(id);
    if(i == m_MemFrameRefs.end())
      m_MemFrameRefs[id] = ref;
    else
      i->second.Merge(ref, ComposeFrameRefsUnordered);
  }
}

void VulkanResourceManager::ClearReferencedMemory()
{
  SCOPED_LOCK_OPTIONAL(m_Lock, m_Capturing);

  m_MemFrameRefs.clear();
}

MemRefs *VulkanResourceManager::FindMemRefs(ResourceId mem)
{
  auto it = m_MemFrameRefs.find(mem);
  if(it != m_MemFrameRefs.end())
    return &it->second;
  else
    return NULL;
}

void VulkanResourceManager::Begin_PrepareInitialBatch()
{
  return m_Core->Begin_PrepareInitialBatch();
}

void VulkanResourceManager::End_PrepareInitialBatch()
{
  return m_Core->End_PrepareInitialBatch();
}

bool VulkanResourceManager::Prepare_InitialState(WrappedVkRes *res)
{
  return m_Core->Prepare_InitialState(res);
}

uint64_t VulkanResourceManager::GetSize_InitialState(ResourceId id, const VkInitialContents &initial)
{
  return m_Core->GetSize_InitialState(id, initial);
}

bool VulkanResourceManager::Serialise_InitialState(WriteSerialiser &ser, ResourceId id,
                                                   VkResourceRecord *record,
                                                   const VkInitialContents *initial)
{
  return m_Core->Serialise_InitialState(ser, id, record, initial);
}

void VulkanResourceManager::Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData)
{
  return m_Core->Create_InitialState(id, live, hasData);
}

void VulkanResourceManager::Apply_InitialState(WrappedVkRes *live, const VkInitialContents &initial)
{
  return m_Core->Apply_InitialState(live, initial);
}

rdcarray<ResourceId> VulkanResourceManager::InitialContentResources()
{
  rdcarray<ResourceId> resources =
      ResourceManager<VulkanResourceManagerConfiguration>::InitialContentResources();
  std::sort(resources.begin(), resources.end(), [this](ResourceId a, ResourceId b) {
    return m_InitialContents[a].data.type < m_InitialContents[b].data.type;
  });
  return resources;
}

bool VulkanResourceManager::ResourceTypeRelease(WrappedVkRes *res)
{
  return m_Core->ReleaseResource(res);
}

bool VulkanResourceManager::IsResourceTrackedForPersistency(WrappedVkRes *const &res)
{
  return IsPostponableRes(res);
}
