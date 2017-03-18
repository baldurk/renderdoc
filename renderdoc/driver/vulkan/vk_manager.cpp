/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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
#include "vk_core.h"

template <>
void Serialiser::Serialise(const char *name, ImageRegionState &el)
{
  ScopedContext scope(this, name, "ImageRegionState", 0, true);

  Serialise("range", el.subresourceRange);
  Serialise("prevstate", el.oldLayout);
  Serialise("state", el.newLayout);
}

bool VulkanResourceManager::SerialisableResource(ResourceId id, VkResourceRecord *record)
{
  if(record->SpecialResource || id == m_Core->GetContextResourceID())
    return false;
  return true;
}

// debugging logging for barriers
#if 0
#define TRDBG(...) RDCLOG(__VA_ARGS__)
#else
#define TRDBG(...)
#endif

template <typename SrcBarrierType>
void VulkanResourceManager::RecordSingleBarrier(vector<pair<ResourceId, ImageRegionState> > &dststates,
                                                ResourceId id, const SrcBarrierType &t,
                                                uint32_t nummips, uint32_t numslices)
{
  bool done = false;

  auto it = dststates.begin();
  for(; it != dststates.end(); ++it)
  {
    // image barriers are handled by initially inserting one subresource range for each aspect,
    // and whenever we need more fine-grained detail we split it immediately for one range for
    // each subresource in that aspect. Thereafter if a barrier comes in that covers multiple
    // subresources, we update all matching ranges.

    // find the states matching this id
    if(it->first < id)
      continue;
    if(it->first != id)
      break;

    {
      // we've found a range that completely matches our region, doesn't matter if that's
      // a whole image and the barrier is the whole image, or it's one subresource.
      // note that for images with only one array/mip slice (e.g. render targets) we'll never
      // really have to worry about the else{} branch
      if(it->second.subresourceRange.baseMipLevel == t.subresourceRange.baseMipLevel &&
         it->second.subresourceRange.levelCount == nummips &&
         it->second.subresourceRange.baseArrayLayer == t.subresourceRange.baseArrayLayer &&
         it->second.subresourceRange.layerCount == numslices)
      {
        // verify
        // RDCASSERT(it->second.newLayout == t.oldLayout);

        // apply it (prevstate is from the start of all barriers accumulated, so only set once)
        if(it->second.oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
          it->second.oldLayout = t.oldLayout;
        it->second.newLayout = t.newLayout;

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
        // new
        // range could be sparse, but that's OK as we only break out of the loop once we go past the
        // whole
        // aspect. Any subresources that don't match the range, after the split, will fail to meet
        // any
        // of the handled cases, so we'll just continue processing.
        if(it->second.subresourceRange.levelCount == 1 &&
           it->second.subresourceRange.layerCount == 1 &&
           it->second.subresourceRange.baseMipLevel >= t.subresourceRange.baseMipLevel &&
           it->second.subresourceRange.baseMipLevel < t.subresourceRange.baseMipLevel + nummips &&
           it->second.subresourceRange.baseArrayLayer >= t.subresourceRange.baseArrayLayer &&
           it->second.subresourceRange.baseArrayLayer < t.subresourceRange.baseArrayLayer + numslices)
        {
          // apply it (prevstate is from the start of all barriers accumulated, so only set once)
          if(it->second.oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
            it->second.oldLayout = t.oldLayout;
          it->second.newLayout = t.newLayout;

          // continue as there might be more, but we're done
          done = true;
          continue;
        }
        // finally handle the case where we have a range that covers a whole image but we need to
        // split it. If the barrier covered the whole image too it would have hit the very first
        // case, so we know that the barrier doesn't cover the whole range.
        // Also, if we've already done the split this case won't be hit and we'll either fall into
        // the case above, or we'll finish as we've covered the whole barrier.
        else if(it->second.subresourceRange.levelCount > 1 ||
                it->second.subresourceRange.layerCount > 1)
        {
          pair<ResourceId, ImageRegionState> existing = *it;

          // remember where we were in the array, as after this iterators will be
          // invalidated.
          size_t offs = it - dststates.begin();
          size_t count =
              it->second.subresourceRange.levelCount * it->second.subresourceRange.layerCount;

          // only insert count-1 as we want count entries total - one per subresource
          dststates.insert(it, count - 1, existing);

          // it now points at the first subresource, but we need to modify the ranges
          // to be valid
          it = dststates.begin() + offs;

          for(size_t i = 0; i < count; i++)
          {
            it->second.subresourceRange.levelCount = 1;
            it->second.subresourceRange.layerCount = 1;

            // slice-major
            it->second.subresourceRange.baseArrayLayer =
                uint32_t(i / existing.second.subresourceRange.levelCount);
            it->second.subresourceRange.baseMipLevel =
                uint32_t(i % existing.second.subresourceRange.levelCount);
            it++;
          }

          // reset the iterator to point to the first subresource
          it = dststates.begin() + offs;

          // the loop will continue after this point and look at the next subresources
          // so we need to check to see if the first subresource lies in the range here
          if(it->second.subresourceRange.baseMipLevel >= t.subresourceRange.baseMipLevel &&
             it->second.subresourceRange.baseMipLevel < t.subresourceRange.baseMipLevel + nummips &&
             it->second.subresourceRange.baseArrayLayer >= t.subresourceRange.baseArrayLayer &&
             it->second.subresourceRange.baseArrayLayer <
                 t.subresourceRange.baseArrayLayer + numslices)
          {
            // apply it (prevstate is from the start of all barriers accumulated, so only set once)
            if(it->second.oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
              it->second.oldLayout = t.oldLayout;
            it->second.newLayout = t.newLayout;

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
  dststates.insert(it, std::make_pair(id, ImageRegionState(subRange, t.oldLayout, t.newLayout)));
}

void VulkanResourceManager::RecordBarriers(vector<pair<ResourceId, ImageRegionState> > &states,
                                           const map<ResourceId, ImageLayouts> &layouts,
                                           uint32_t numBarriers, const VkImageMemoryBarrier *barriers)
{
  TRDBG("Recording %u barriers", numBarriers);

  for(uint32_t ti = 0; ti < numBarriers; ti++)
  {
    const VkImageMemoryBarrier &t = barriers[ti];

    ResourceId id = m_State < WRITING ? GetNonDispWrapper(t.image)->id : GetResID(t.image);

    if(id == ResourceId())
    {
      RDCERR("Couldn't get ID for image %p in barrier", t.image);
      continue;
    }

    uint32_t nummips = t.subresourceRange.levelCount;
    uint32_t numslices = t.subresourceRange.layerCount;

    auto it = layouts.find(id);

    if(nummips == VK_REMAINING_MIP_LEVELS)
    {
      if(it != layouts.end())
        nummips = it->second.levelCount - t.subresourceRange.baseMipLevel;
      else
        nummips = 1;
    }

    if(numslices == VK_REMAINING_ARRAY_LAYERS)
    {
      if(it != layouts.end())
        numslices = it->second.layerCount - t.subresourceRange.baseArrayLayer;
      else
        numslices = 1;
    }

    RecordSingleBarrier(states, id, t, nummips, numslices);
  }

  TRDBG("Post-record, there are %u states", (uint32_t)states.size());
}

void VulkanResourceManager::MergeBarriers(vector<pair<ResourceId, ImageRegionState> > &dststates,
                                          vector<pair<ResourceId, ImageRegionState> > &srcstates)
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

void VulkanResourceManager::SerialiseImageStates(map<ResourceId, ImageLayouts> &states,
                                                 vector<VkImageMemoryBarrier> &barriers)
{
  Serialiser *localSerialiser = m_pSerialiser;

  SERIALISE_ELEMENT(uint32_t, NumMems, (uint32_t)states.size());

  auto srcit = states.begin();

  vector<pair<ResourceId, ImageRegionState> > vec;

  for(uint32_t i = 0; i < NumMems; i++)
  {
    SERIALISE_ELEMENT(ResourceId, id, srcit->first);
    SERIALISE_ELEMENT(uint32_t, NumStates, (uint32_t)srcit->second.subresourceStates.size());

    ResourceId liveid;
    if(m_State < WRITING && HasLiveResource(id))
      liveid = GetLiveID(id);

    for(uint32_t m = 0; m < NumStates; m++)
    {
      SERIALISE_ELEMENT(ImageRegionState, state, srcit->second.subresourceStates[m]);

      if(m_State < WRITING && liveid != ResourceId() && srcit != states.end())
      {
        VkImageMemoryBarrier t;
        t.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        t.pNext = NULL;
        // these access masks aren't used, we need to apply a global memory barrier
        // to memory each time we restart log replaying. These barriers are just
        // to get images into the right layout
        t.srcAccessMask = 0;
        t.dstAccessMask = 0;
        // MULTIDEVICE need to handle multiple queues
        t.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        t.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        t.image = Unwrap(GetCurrentHandle<VkImage>(liveid));
        t.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ReplacePresentableImageLayout(state.newLayout);
        t.newLayout = state.newLayout;
        t.subresourceRange = state.subresourceRange;
        barriers.push_back(t);
        vec.push_back(std::make_pair(liveid, state));
      }
    }

    if(m_State >= WRITING)
      srcit++;
  }

  ApplyBarriers(vec, states);

  for(size_t i = 0; i < vec.size(); i++)
    barriers[i].oldLayout = vec[i].second.oldLayout;

  // erase any do-nothing barriers
  for(auto it = barriers.begin(); it != barriers.end();)
  {
    if(it->oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
      it->oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if(it->oldLayout == it->newLayout)
      it = barriers.erase(it);
    else
      ++it;
  }

  // try to merge images that have been split up by subresource but are now all in the same state
  // again.
  for(auto it = states.begin(); it != states.end(); ++it)
  {
    ImageLayouts &layouts = it->second;

    if(layouts.subresourceStates.size() > 1 &&
       layouts.subresourceStates.size() == size_t(layouts.layerCount * layouts.levelCount))
    {
      VkImageLayout layout = layouts.subresourceStates[0].newLayout;

      bool allIdentical = true;

      for(size_t i = 0; i < layouts.subresourceStates.size(); i++)
      {
        if(layouts.subresourceStates[i].newLayout != layout)
        {
          allIdentical = false;
          break;
        }
      }

      if(allIdentical)
      {
        layouts.subresourceStates.erase(layouts.subresourceStates.begin() + 1,
                                        layouts.subresourceStates.end());
        layouts.subresourceStates[0].subresourceRange.baseArrayLayer = 0;
        layouts.subresourceStates[0].subresourceRange.baseMipLevel = 0;
        layouts.subresourceStates[0].subresourceRange.layerCount = layouts.layerCount;
        layouts.subresourceStates[0].subresourceRange.levelCount = layouts.levelCount;
      }
    }
  }
}

void VulkanResourceManager::MarkSparseMapReferenced(SparseMapping *sparse)
{
  if(sparse == NULL)
  {
    RDCERR("Unexpected NULL sparse mapping");
    return;
  }

  for(size_t i = 0; i < sparse->opaquemappings.size(); i++)
    MarkResourceFrameReferenced(GetResID(sparse->opaquemappings[i].memory), eFrameRef_Read);

  for(int a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
    for(VkDeviceSize i = 0;
        sparse->pages[a] &&
        i < VkDeviceSize(sparse->imgdim.width * sparse->imgdim.height * sparse->imgdim.depth);
        i++)
      MarkResourceFrameReferenced(GetResID(sparse->pages[a][i].first), eFrameRef_Read);
}

void VulkanResourceManager::ApplyBarriers(vector<pair<ResourceId, ImageRegionState> > &states,
                                          map<ResourceId, ImageLayouts> &layouts)
{
  TRDBG("Applying %u barriers", (uint32_t)states.size());

  for(size_t ti = 0; ti < states.size(); ti++)
  {
    ResourceId id = states[ti].first;
    ImageRegionState &t = states[ti].second;

    TRDBG("Applying barrier to %llu", GetOriginalID(id));

    auto stit = layouts.find(id);

    if(stit == layouts.end())
    {
      TRDBG("Didn't find ID in image layouts");
      continue;
    }

    uint32_t nummips = t.subresourceRange.levelCount;
    uint32_t numslices = t.subresourceRange.layerCount;
    if(nummips == VK_REMAINING_MIP_LEVELS)
      nummips = layouts[id].levelCount;
    if(numslices == VK_REMAINING_ARRAY_LAYERS)
      numslices = layouts[id].layerCount;

    if(nummips == 0)
      nummips = 1;
    if(numslices == 0)
      numslices = 1;

    if(t.oldLayout == t.newLayout)
      continue;

    TRDBG("Barrier of %s (%u->%u, %u->%u) from %s to %s",
          ToStr::Get(t.subresourceRange.aspect).c_str(), t.subresourceRange.baseMipLevel,
          t.subresourceRange.levelCount, t.subresourceRange.baseArrayLayer,
          t.subresourceRange.layerCount, ToStr::Get(t.oldLayout).c_str(),
          ToStr::Get(t.newLayout).c_str());

    bool done = false;

    TRDBG("Matching image has %u subresource states", stit->second.subresourceStates.size());

    auto it = stit->second.subresourceStates.begin();
    for(; it != stit->second.subresourceStates.end(); ++it)
    {
      TRDBG(".. state %s (%u->%u, %u->%u) from %s to %s",
            ToStr::Get(it->subresourceRange.aspect).c_str(), it->range.baseMipLevel,
            it->range.levelCount, it->range.baseArrayLayer, it->range.layerCount,
            ToStr::Get(it->oldLayout).c_str(), ToStr::Get(it->newLayout).c_str());

      // image barriers are handled by initially inserting one subresource range for the whole
      // object,
      // and whenever we need more fine-grained detail we split it immediately.
      // Thereafter if a barrier comes in that covers multiple subresources, we update all matching
      // ranges.
      // NOTE: Depth-stencil images must always be trasnsitioned together for both aspects, so we
      // don't
      // have to worry about different aspects being in different states and can in fact ignore the
      // aspect
      // for the purpose of this case.

      {
        // we've found a range that completely matches our region, doesn't matter if that's
        // a whole image and the barrier is the whole image, or it's one subresource.
        // note that for images with only one array/mip slice (e.g. render targets) we'll never
        // really have to worry about the else{} branch
        if(it->subresourceRange.baseMipLevel == t.subresourceRange.baseMipLevel &&
           it->subresourceRange.levelCount == nummips &&
           it->subresourceRange.baseArrayLayer == t.subresourceRange.baseArrayLayer &&
           it->subresourceRange.layerCount == numslices)
        {
          /*
          RDCASSERT(t.oldLayout == UNKNOWN_PREV_IMG_LAYOUT || it->newLayout ==
          UNKNOWN_PREV_IMG_LAYOUT || // renderdoc untracked/ignored
                    it->newLayout == t.oldLayout || // valid barrier
                    t.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED); // can barrier from UNDEFINED to any
          state
          */
          if(it->oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
            it->oldLayout = t.oldLayout;
          t.oldLayout = it->newLayout;
          it->newLayout = t.newLayout;

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
          // new
          // range could be sparse, but that's OK as we only break out of the loop once we go past
          // the whole
          // aspect. Any subresources that don't match the range, after the split, will fail to meet
          // any
          // of the handled cases, so we'll just continue processing.
          if(it->subresourceRange.levelCount == 1 && it->subresourceRange.layerCount == 1 &&
             it->subresourceRange.baseMipLevel >= t.subresourceRange.baseMipLevel &&
             it->subresourceRange.baseMipLevel < t.subresourceRange.baseMipLevel + nummips &&
             it->subresourceRange.baseArrayLayer >= t.subresourceRange.baseArrayLayer &&
             it->subresourceRange.baseArrayLayer < t.subresourceRange.baseArrayLayer + numslices)
          {
            // apply it (prevstate is from the start of all barriers accumulated, so only set once)
            if(it->oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
              it->oldLayout = t.oldLayout;
            it->newLayout = t.newLayout;

            // continue as there might be more, but we're done
            done = true;
            continue;
          }
          // finally handle the case where we have a range that covers a whole image but we need to
          // split it. If the barrier covered the whole image too it would have hit the very first
          // case, so we know that the barrier doesn't cover the whole range.
          // Also, if we've already done the split this case won't be hit and we'll either fall into
          // the case above, or we'll finish as we've covered the whole barrier.
          else if(it->subresourceRange.levelCount > 1 || it->subresourceRange.layerCount > 1)
          {
            ImageRegionState existing = *it;

            // remember where we were in the array, as after this iterators will be
            // invalidated.
            size_t offs = it - stit->second.subresourceStates.begin();
            size_t count = it->subresourceRange.levelCount * it->subresourceRange.layerCount;

            // only insert count-1 as we want count entries total - one per subresource
            stit->second.subresourceStates.insert(it, count - 1, existing);

            // it now points at the first subresource, but we need to modify the ranges
            // to be valid
            it = stit->second.subresourceStates.begin() + offs;

            for(size_t i = 0; i < count; i++)
            {
              it->subresourceRange.levelCount = 1;
              it->subresourceRange.layerCount = 1;

              // slice-major
              it->subresourceRange.baseArrayLayer =
                  uint32_t(i / existing.subresourceRange.levelCount);
              it->subresourceRange.baseMipLevel = uint32_t(i % existing.subresourceRange.levelCount);
              it++;
            }

            // reset the iterator to point to the first subresource
            it = stit->second.subresourceStates.begin() + offs;

            // the loop will continue after this point and look at the next subresources
            // so we need to check to see if the first subresource lies in the range here
            if(it->subresourceRange.baseMipLevel >= t.subresourceRange.baseMipLevel &&
               it->subresourceRange.baseMipLevel < t.subresourceRange.baseMipLevel + nummips &&
               it->subresourceRange.baseArrayLayer >= t.subresourceRange.baseArrayLayer &&
               it->subresourceRange.baseArrayLayer < t.subresourceRange.baseArrayLayer + numslices)
            {
              // apply it (prevstate is from the start of all barriers accumulated, so only set
              // once)
              if(it->oldLayout == UNKNOWN_PREV_IMG_LAYOUT)
                it->oldLayout = t.oldLayout;
              it->newLayout = t.newLayout;

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

bool VulkanResourceManager::Force_InitialState(WrappedVkRes *res, bool prepare)
{
  return false;
}

bool VulkanResourceManager::Need_InitialStateChunk(WrappedVkRes *res)
{
  return true;
}

bool VulkanResourceManager::Prepare_InitialState(WrappedVkRes *res)
{
  return m_Core->Prepare_InitialState(res);
}

bool VulkanResourceManager::Serialise_InitialState(ResourceId resid, WrappedVkRes *res)
{
  return m_Core->Serialise_InitialState(resid, res);
}

void VulkanResourceManager::Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData)
{
  return m_Core->Create_InitialState(id, live, hasData);
}

void VulkanResourceManager::Apply_InitialState(WrappedVkRes *live, InitialContentData initial)
{
  return m_Core->Apply_InitialState(live, initial);
}

bool VulkanResourceManager::ResourceTypeRelease(WrappedVkRes *res)
{
  return m_Core->ReleaseResource(res);
}
