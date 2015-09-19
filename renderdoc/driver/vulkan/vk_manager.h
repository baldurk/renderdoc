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

// VKTODOLOW maybe make this a bit nicer? I'm not sure.
#define VKMGR() VulkanResourceManager::GetInstance()

class VulkanResourceManager : public ResourceManager<WrappedVkRes*, RealVkRes, VkResourceRecord>
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
		
		template<typename realtype>
		void AddLiveResource(ResourceId id, realtype obj)
		{
			ResourceManager::AddLiveResource(id, GetWrapped(obj));
		}

		using ResourceManager::AddResourceRecord;
		
		template<typename realtype>
		VkResourceRecord *AddResourceRecord(realtype &obj)
		{
			typename UnwrapHelper<realtype>::Outer *wrapped = GetWrapped(obj);
			VkResourceRecord *ret = wrapped->record = ResourceManager::AddResourceRecord(wrapped->id);

			return ret;
		}
		
		// easy path for getting the unwrapped handle cast to the
		// write type. Saves a lot of work casting to either WrappedVkNonDispRes
		// or WrappedVkDispRes depending on the type, then ->real, then casting
		// when this is all we want to do in most cases
		template<typename realtype>
		realtype GetLiveHandle(ResourceId origid)
		{
			RealVkRes &res = ((typename UnwrapHelper<realtype>::ParentType *)ResourceManager::GetLiveResource(origid))->real;
			return res.As<realtype>();
		}

		template<typename realtype>
		realtype GetCurrentHandle(ResourceId id)
		{
			RealVkRes &res = ((typename UnwrapHelper<realtype>::ParentType *)ResourceManager::GetCurrentResource(id))->real;
			return res.As<realtype>();
		}
		
		// handling memory & image transitions
		void RecordTransitions(vector< pair<ResourceId, ImageRegionState> > &trans, map<ResourceId, ImgState> &states,
			                   uint32_t numTransitions, const VkImageMemoryBarrier *transitions);

		void ApplyTransitions(vector< pair<ResourceId, ImageRegionState> > &trans, map<ResourceId, ImgState> &states);

		void SerialiseImageStates(Serialiser *ser, map<ResourceId, ImgState> &states, vector<VkImageMemoryBarrier> &transitions);

		ResourceId GetID(WrappedVkRes *res)
		{
			if(res == NULL) return ResourceId();

			if(IsDispatchableRes(res))
				return ((WrappedVkDispRes *)res)->id;

			return ((WrappedVkNonDispRes *)res)->id;
		}
		
		template<typename parenttype, typename realtype>
		ResourceId WrapResource(parenttype parentObj, realtype &obj)
		{
			RDCASSERT(obj != VK_NULL_HANDLE);

			ResourceId id = ResourceIDGen::GetNewUniqueID();
			typename UnwrapHelper<realtype>::Outer *wrapped = new typename UnwrapHelper<realtype>::Outer(obj, id);
			
			SetTableIfDispatchable(m_State >= WRITING, parentObj, wrapped);

			AddCurrentResource(id, wrapped);

			AddWrapper(wrapped, UnwrapHelper<realtype>::ToRealRes(obj));

			obj = realtype((uint64_t)wrapped);

			return id;
		}
		
		template<typename realtype>
		void ReleaseWrappedResource(realtype obj)
		{
			ResourceId id = GetResID(obj);
			ResourceManager::MarkCleanResource(id);
			ResourceManager::RemoveWrapper(UnwrapHelper<realtype>::ToRealRes(Unwrap(obj)));
			ResourceManager::ReleaseCurrentResource(id);
			if(GetRecord(obj)) GetRecord(obj)->Delete(this);
			delete GetWrapped(obj);
		}
			
	private:
		bool SerialisableResource(ResourceId id, VkResourceRecord *record);

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
