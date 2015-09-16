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

#pragma once

#include "core/resource_manager.h"

#include "vk_resources.h"

#include <algorithm>
#include <utility>

using std::pair;

class WrappedVulkan;

class VulkanResourceManager : public ResourceManager<WrappedVkRes*, VkResourceRecord>
{
	public: 
		VulkanResourceManager(LogState s, Serialiser *ser, WrappedVulkan *core)
			: ResourceManager(s, ser), m_Core(core)
		{
			if(m_Inst) RDCFATAL("Multiple resource managers");
			m_Inst = this;
		}
		~VulkanResourceManager() { m_Inst = NULL; }
		
		static VulkanResourceManager *GetInstance() { return m_Inst; }
		
		// handling memory & image transitions
		void RecordTransitions(vector< pair<ResourceId, ImageRegionState> > &trans, map<ResourceId, ImgState> &states,
			                   uint32_t numTransitions, const VkImageMemoryBarrier *transitions);

		void ApplyTransitions(vector< pair<ResourceId, ImageRegionState> > &trans, map<ResourceId, ImgState> &states);

		void SerialiseImageStates(Serialiser *ser, map<ResourceId, ImgState> &states, vector<VkImageMemoryBarrier> &transitions);
		
	private:
		bool SerialisableResource(ResourceId id, VkResourceRecord *record);

		ResourceId GetID(WrappedVkRes *res)
		{
			return res ? res->id : ResourceId();
		}
		
		bool ResourceTypeRelease(WrappedVkRes *res);

		bool Force_InitialState(WrappedVkRes *res);
		bool Need_InitialStateChunk(WrappedVkRes *res);
		bool Prepare_InitialState(WrappedVkRes *res);
		bool Serialise_InitialState(WrappedVkRes *res);
		void Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData);
		void Apply_InitialState(WrappedVkRes *live, InitialContentData initial);

		static VulkanResourceManager *m_Inst;

		WrappedVulkan *m_Core;
};
