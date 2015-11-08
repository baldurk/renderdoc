/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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

template<>
void Serialiser::Serialise(const char *name, ImageRegionState &el)
{
	ScopedContext scope(this, name, "ImageRegionState", 0, true);
	
	Serialise("range", el.subresourceRange);
	Serialise("prevstate", el.oldLayout);
	Serialise("state", el.newLayout);
}

bool VulkanResourceManager::SerialisableResource(ResourceId id, VkResourceRecord *record)
{
	if(id == m_Core->GetContextResourceID())
		return false;
	return true;
}

// debugging logging for transitions
#if 0
#define TRDBG(...) RDCLOG(__VA_ARGS__)
#else
#define TRDBG(...)
#endif

template<typename SrcTransType>
void VulkanResourceManager::RecordSingleTransition(vector< pair<ResourceId, ImageRegionState> > &dsttrans, ResourceId id, const SrcTransType &t, uint32_t nummips, uint32_t numslices)
{
	bool done = false;

	auto it = dsttrans.begin();
	for(; it != dsttrans.end(); ++it)
	{
		// image transitions are handled by initially inserting one subresource range for each aspect,
		// and whenever we need more fine-grained detail we split it immediately for one range for
		// each subresource in that aspect. Thereafter if a transition comes in that covers multiple
		// subresources, we transition all matching ranges.

		// find the transitions matching this id
		if(it->first < id) continue;
		if(it->first != id) break;

		if(it->second.subresourceRange.aspectMask & t.subresourceRange.aspectMask)
		{
			// we've found a range that completely matches our region, doesn't matter if that's
			// a whole image and the transition is the whole image, or it's one subresource.
			// note that for images with only one array/mip slice (e.g. render targets) we'll never
			// really have to worry about the else{} branch
			if(it->second.subresourceRange.baseMipLevel == t.subresourceRange.baseMipLevel &&
				it->second.subresourceRange.mipLevels == nummips &&
				it->second.subresourceRange.baseArrayLayer == t.subresourceRange.baseArrayLayer &&
				it->second.subresourceRange.arraySize == numslices)
			{
				// verify
				//RDCASSERT(it->second.state == t.oldLayout);

				// apply it (prevstate is from the start of all transitions, so only set once)
				if(it->second.oldLayout == UNTRANSITIONED_IMG_STATE)
					it->second.oldLayout = t.oldLayout;
				it->second.newLayout = t.newLayout;

				done = true;
				break;
			}
			else
			{
				// this handles the case where the transition covers a number of subresources and we need
				// to transition each matching subresource. If the transition was only one mip & array slice
				// it would have hit the case above. Find each subresource within the range, transition it,
				// and continue (marking as done so whenever we stop finding matching ranges, we are
				// satisfied.
				//
				// note that regardless of how we lay out our subresources (slice-major or mip-major) the new
				// range could be sparse, but that's OK as we only break out of the loop once we go past the whole
				// aspect. Any subresources that don't match the range, after the split, will fail to meet any
				// of the handled cases, so we'll just continue processing.
				if(it->second.subresourceRange.mipLevels == 1 &&
					it->second.subresourceRange.arraySize == 1 &&
					it->second.subresourceRange.baseMipLevel >= t.subresourceRange.baseMipLevel &&
					it->second.subresourceRange.baseMipLevel < t.subresourceRange.baseMipLevel+nummips &&
					it->second.subresourceRange.baseArrayLayer >= t.subresourceRange.baseArrayLayer &&
					it->second.subresourceRange.baseArrayLayer < t.subresourceRange.baseArrayLayer+numslices)
				{
					// apply it (prevstate is from the start of all transitions, so only set once)
					if(it->second.oldLayout == UNTRANSITIONED_IMG_STATE)
						it->second.oldLayout = t.oldLayout;
					it->second.newLayout = t.newLayout;

					// continue as there might be more, but we're done
					done = true;
					continue;
				}
				// finally handle the case where we have a range that covers a whole image but we need to
				// split it. If the transition covered the whole image too it would have hit the very first
				// case, so we know that the transition doesn't cover the whole range.
				// Also, if we've already done the split this case won't be hit and we'll either fall into
				// the case above, or we'll finish as we've covered the whole transition.
				else if(it->second.subresourceRange.mipLevels > 1 || it->second.subresourceRange.arraySize > 1)
				{
					pair<ResourceId, ImageRegionState> existing = *it;

					// remember where we were in the array, as after this iterators will be
					// invalidated.
					size_t offs = it - dsttrans.begin();
					size_t count = it->second.subresourceRange.mipLevels * it->second.subresourceRange.arraySize;

					// only insert count-1 as we want count entries total - one per subresource
					dsttrans.insert(it, count-1, existing);

					// it now points at the first subresource, but we need to modify the ranges
					// to be valid
					it = dsttrans.begin()+offs;

					for(size_t i=0; i < count; i++)
					{
						it->second.subresourceRange.mipLevels = 1;
						it->second.subresourceRange.arraySize = 1;

						// slice-major
						it->second.subresourceRange.baseArrayLayer = uint32_t(i / existing.second.subresourceRange.mipLevels);
						it->second.subresourceRange.baseMipLevel = uint32_t(i % existing.second.subresourceRange.mipLevels);
						it++;
					}

					// reset the iterator to point to the first subresource
					it = dsttrans.begin()+offs;

					// the loop will continue after this point and look at the next subresources
					// so we need to check to see if the first subresource lies in the range here
					if(it->second.subresourceRange.baseMipLevel >= t.subresourceRange.baseMipLevel &&
						it->second.subresourceRange.baseMipLevel < t.subresourceRange.baseMipLevel+nummips &&
						it->second.subresourceRange.baseArrayLayer >= t.subresourceRange.baseArrayLayer &&
						it->second.subresourceRange.baseArrayLayer < t.subresourceRange.baseArrayLayer+numslices)
					{
						// apply it (prevstate is from the start of all transitions, so only set once)
						if(it->second.oldLayout == UNTRANSITIONED_IMG_STATE)
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

		// if we've gone past where the new subresource range would sit
		if(it->second.subresourceRange.aspectMask > t.subresourceRange.aspectMask)
			break;

		// otherwise continue to try and find the subresource range
	}

	if(done) return;

	// we don't have an existing transition for this memory region, insert into place. it points to
	// where it should be inserted
	dsttrans.insert(it, std::make_pair(id, ImageRegionState(t.subresourceRange, t.oldLayout, t.newLayout)));
}

void VulkanResourceManager::RecordTransitions(vector< pair<ResourceId, ImageRegionState> > &trans, map<ResourceId, ImageLayouts> &states,
											  uint32_t numTransitions, const VkImageMemoryBarrier *transitions)
{
	TRDBG("Recording %u transitions", numTransitions);

	for(uint32_t ti=0; ti < numTransitions; ti++)
	{
		const VkImageMemoryBarrier &t = transitions[ti];
		
		ResourceId id = m_State < WRITING ? GetNonDispWrapper(t.image)->id : GetResID(t.image);
		
		uint32_t nummips = t.subresourceRange.mipLevels;
		uint32_t numslices = t.subresourceRange.arraySize;
		if(nummips == VK_REMAINING_MIP_LEVELS) nummips = states[id].mipLevels - t.subresourceRange.baseMipLevel;
		if(numslices == VK_REMAINING_ARRAY_LAYERS) numslices = states[id].arraySize - t.subresourceRange.baseArrayLayer;

		RecordSingleTransition(trans, id, t, nummips, numslices);
	}

	TRDBG("Post-record, there are %u transitions", (uint32_t)trans.size());
}

void VulkanResourceManager::MergeTransitions(vector< pair<ResourceId, ImageRegionState> > &dsttrans,
                                             vector< pair<ResourceId, ImageRegionState> > &srctrans)
{
	TRDBG("Merging %u transitions", (uint32_t)srctrans.size());

	for(size_t ti=0; ti < srctrans.size(); ti++)
	{
		const ImageRegionState &t = srctrans[ti].second;
		RecordSingleTransition(dsttrans, srctrans[ti].first, t, t.subresourceRange.mipLevels, t.subresourceRange.arraySize);
	}

	TRDBG("Post-merge, there are %u transitions", (uint32_t)dsttrans.size());
}

void VulkanResourceManager::SerialiseImageStates(map<ResourceId, ImageLayouts> &states, vector<VkImageMemoryBarrier> &transitions)
{
	Serialiser *localSerialiser = m_pSerialiser;

	SERIALISE_ELEMENT(uint32_t, NumMems, (uint32_t)states.size());

	auto srcit = states.begin();

	vector< pair<ResourceId, ImageRegionState> > vec;

	for(uint32_t i=0; i < NumMems; i++)
	{
		SERIALISE_ELEMENT(ResourceId, id, srcit->first);
		SERIALISE_ELEMENT(uint32_t, NumStates, (uint32_t)srcit->second.subresourceStates.size());

		ResourceId liveid;
		if(m_State < WRITING && HasLiveResource(id))
			liveid = GetLiveID(id);
		auto dstit = states.find(id);

		for(uint32_t m=0; m < NumStates; m++)
		{
			SERIALISE_ELEMENT(ImageRegionState, state, srcit->second.subresourceStates[m]);

			if(m_State < WRITING && liveid != ResourceId() && srcit != states.end())
			{
				VkImageMemoryBarrier t;
				t.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				t.pNext = NULL;
				// these input masks aren't used, we need to apply a global memory barrier
				// to memory each time we restart log replaying. These transitions are just
				// to get images into the right layout
				t.inputMask = 0;
				t.outputMask = 0;
				// VKTODOLOW need to handle multiple queues better than this maybe
				t.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				t.destQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				t.image = Unwrap(GetCurrentHandle<VkImage>(liveid));
				t.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				t.newLayout = state.newLayout;
				t.subresourceRange = state.subresourceRange;
				transitions.push_back(t);
				vec.push_back(std::make_pair(liveid, state));
			}
		}

		if(m_State >= WRITING) srcit++;
	}

	ApplyTransitions(vec, states);

	for(size_t i=0; i < vec.size(); i++)
		transitions[i].oldLayout = vec[i].second.oldLayout;

	// erase any do-nothing transitions
	for(auto it=transitions.begin(); it != transitions.end();)
	{
		if(it->oldLayout == it->newLayout)
			it = transitions.erase(it);
		else
			++it;
	}
}

void VulkanResourceManager::ApplyTransitions(vector< pair<ResourceId, ImageRegionState> > &trans, map<ResourceId, ImageLayouts> &states)
{
	TRDBG("Applying %u transitions", (uint32_t)trans.size());

	for(size_t ti=0; ti < trans.size(); ti++)
	{
		ResourceId id = trans[ti].first;
		ImageRegionState &t = trans[ti].second;

		TRDBG("Applying transition to %llu", GetOriginalID(id));

		auto stit = states.find(id);

		if(stit == states.end())
		{
			TRDBG("Didn't find ID in image states");
			continue;
		}

		uint32_t nummips = t.subresourceRange.mipLevels;
		uint32_t numslices = t.subresourceRange.arraySize;
		if(nummips == VK_REMAINING_MIP_LEVELS) nummips = states[id].mipLevels;
		if(numslices == VK_REMAINING_ARRAY_LAYERS) numslices = states[id].arraySize;

		// VKTODOLOW check, does this mean the sensible thing?
		if(nummips == 0) nummips = 1;
		if(numslices == 0) numslices = 1;

		if(t.oldLayout == t.newLayout) continue;

		TRDBG("Transition of %s (%u->%u, %u->%u) from %s to %s",
				ToStr::Get(t.subresourceRange.aspect).c_str(),
				t.subresourceRange.baseMipLevel, t.subresourceRange.mipLevels,
				t.subresourceRange.baseArrayLayer, t.subresourceRange.arraySize,
				ToStr::Get(t.oldLayout).c_str(), ToStr::Get(t.newLayout).c_str());

		bool done = false;

		TRDBG("Matching image has %u subresource states", stit->second.subresourceStates.size());

		auto it = stit->second.subresourceStates.begin();
		for(; it != stit->second.subresourceStates.end(); ++it)
		{
			TRDBG(".. state %s (%u->%u, %u->%u) from %s to %s",
				ToStr::Get(it->subresourceRange.aspect).c_str(),
				it->range.baseMipLevel, it->range.mipLevels,
				it->range.baseArrayLayer, it->range.arraySize,
				ToStr::Get(it->oldLayout).c_str(), ToStr::Get(it->newLayout).c_str());

			// image transitions are handled by initially inserting one subresource range for each aspect,
			// and whenever we need more fine-grained detail we split it immediately for one range for
			// each subresource in that aspect. Thereafter if a transition comes in that covers multiple
			// subresources, we transition all matching ranges.

			if(it->subresourceRange.aspectMask & t.subresourceRange.aspectMask)
			{
				// we've found a range that completely matches our region, doesn't matter if that's
				// a whole image and the transition is the whole image, or it's one subresource.
				// note that for images with only one array/mip slice (e.g. render targets) we'll never
				// really have to worry about the else{} branch
				if(it->subresourceRange.baseMipLevel == t.subresourceRange.baseMipLevel &&
				   it->subresourceRange.mipLevels == nummips &&
				   it->subresourceRange.baseArrayLayer == t.subresourceRange.baseArrayLayer &&
				   it->subresourceRange.arraySize == numslices)
				{
					/*
					RDCASSERT(t.prevstate == UNTRANSITIONED_IMG_STATE || it->state == UNTRANSITIONED_IMG_STATE || // renderdoc untracked/ignored
					          it->state == t.prevstate || // valid transition
										t.prevstate == VK_IMAGE_LAYOUT_UNDEFINED); // can transition from UNDEFINED to any state
					*/
					t.oldLayout = it->newLayout;
					it->newLayout = t.newLayout;

					done = true;
					break;
				}
				else
				{
					// this handles the case where the transition covers a number of subresources and we need
					// to transition each matching subresource. If the transition was only one mip & array slice
					// it would have hit the case above. Find each subresource within the range, transition it,
					// and continue (marking as done so whenever we stop finding matching ranges, we are
					// satisfied.
					//
					// note that regardless of how we lay out our subresources (slice-major or mip-major) the new
					// range could be sparse, but that's OK as we only break out of the loop once we go past the whole
					// aspect. Any subresources that don't match the range, after the split, will fail to meet any
					// of the handled cases, so we'll just continue processing.
					if(it->subresourceRange.mipLevels == 1 &&
					   it->subresourceRange.arraySize == 1 &&
					   it->subresourceRange.baseMipLevel >= t.subresourceRange.baseMipLevel &&
					   it->subresourceRange.baseMipLevel < t.subresourceRange.baseMipLevel+nummips &&
					   it->subresourceRange.baseArrayLayer >= t.subresourceRange.baseArrayLayer &&
					   it->subresourceRange.baseArrayLayer < t.subresourceRange.baseArrayLayer+numslices)
					{
						// apply it (prevstate is from the start of all transitions, so only set once)
						if(it->oldLayout == UNTRANSITIONED_IMG_STATE)
							it->oldLayout = t.oldLayout;
						it->newLayout = t.newLayout;

						// continue as there might be more, but we're done
						done = true;
						continue;
					}
					// finally handle the case where we have a range that covers a whole image but we need to
					// split it. If the transition covered the whole image too it would have hit the very first
					// case, so we know that the transition doesn't cover the whole range.
					// Also, if we've already done the split this case won't be hit and we'll either fall into
					// the case above, or we'll finish as we've covered the whole transition.
					else if(it->subresourceRange.mipLevels > 1 || it->subresourceRange.arraySize > 1)
					{
						ImageRegionState existing = *it;

						// remember where we were in the array, as after this iterators will be
						// invalidated.
						size_t offs = it - stit->second.subresourceStates.begin();
						size_t count = it->subresourceRange.mipLevels * it->subresourceRange.arraySize;

						// only insert count-1 as we want count entries total - one per subresource
						stit->second.subresourceStates.insert(it, count-1, existing);

						// it now points at the first subresource, but we need to modify the ranges
						// to be valid
						it = stit->second.subresourceStates.begin()+offs;

						for(size_t i=0; i < count; i++)
						{
							it->subresourceRange.mipLevels = 1;
							it->subresourceRange.arraySize = 1;

							// slice-major
							it->subresourceRange.baseArrayLayer = uint32_t(i / existing.subresourceRange.mipLevels);
							it->subresourceRange.baseMipLevel = uint32_t(i % existing.subresourceRange.mipLevels);
							it++;
						}
						
						// reset the iterator to point to the first subresource
						it = stit->second.subresourceStates.begin()+offs;

						// the loop will continue after this point and look at the next subresources
						// so we need to check to see if the first subresource lies in the range here
						if(it->subresourceRange.baseMipLevel >= t.subresourceRange.baseMipLevel &&
						   it->subresourceRange.baseMipLevel < t.subresourceRange.baseMipLevel+nummips &&
						   it->subresourceRange.baseArrayLayer >= t.subresourceRange.baseArrayLayer &&
						   it->subresourceRange.baseArrayLayer < t.subresourceRange.baseArrayLayer+numslices)
						{
							// apply it (prevstate is from the start of all transitions, so only set once)
							if(it->oldLayout == UNTRANSITIONED_IMG_STATE)
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

			// if we've gone past where the new subresource range would sit
			if(it->subresourceRange.aspectMask > t.subresourceRange.aspectMask)
				break;

			// otherwise continue to try and find the subresource range
		}

		if(!done)
			RDCERR("Couldn't find subresource range to apply transition to - invalid!");
	}
}

bool VulkanResourceManager::Force_InitialState(WrappedVkRes *res)
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

bool VulkanResourceManager::Serialise_InitialState(WrappedVkRes *res)
{
	return m_Core->Serialise_InitialState(res);
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
