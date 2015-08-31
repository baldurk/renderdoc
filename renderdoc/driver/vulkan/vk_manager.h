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

class VulkanResourceManager : public ResourceManager<VkResource, VkResourceRecord>
{
	public: 
		VulkanResourceManager(LogState s, Serialiser *ser, WrappedVulkan *core)
			: ResourceManager(s, ser), m_Core(core)
		{
			if(m_Inst) RDCFATAL("Multiple resource managers");
			m_Inst = this;
		}
		~VulkanResourceManager() {}
		
		static VulkanResourceManager *GetInstance() { return m_Inst; }

		void Shutdown()
		{
			while(!m_VkResourceRecords.empty())
			{
				auto it = m_VkResourceRecords.begin();
				ResourceId id = it->second->GetResourceID();
				it->second->Delete(this);

				if(!m_VkResourceRecords.empty() && m_VkResourceRecords.begin()->second->GetResourceID() == id)
					m_VkResourceRecords.erase(m_VkResourceRecords.begin());
			}

			m_CurrentResourceIds.clear();

			ResourceManager::Shutdown();
		}
		
		inline void RemoveResourceRecord(ResourceId id)
		{
			for(auto it = m_VkResourceRecords.begin(); it != m_VkResourceRecords.end(); it++)
			{
				if(it->second->GetResourceID() == id)
				{
					m_VkResourceRecords.erase(it);
					break;
				}
			}
			
			ResourceManager::RemoveResourceRecord(id);
		}

		// this should take the VkDevice associated too, for release
		ResourceId RegisterResource(VkResource res)
		{
			ResourceId id = ResourceIDGen::GetNewUniqueID();
			m_CurrentResourceIds[res] = id;
			AddCurrentResource(id, res);
			return id;
		}

		void UnregisterResource(VkResource res)
		{
			auto it = m_CurrentResourceIds.find(res);
			if(it != m_CurrentResourceIds.end())
			{
				ReleaseCurrentResource(it->second);
				m_CurrentResourceIds.erase(res);
			}
		}

		ResourceId GetID(VkResource res)
		{
			auto it = m_CurrentResourceIds.find(res);
			if(it != m_CurrentResourceIds.end())
				return it->second;
			return ResourceId();
		}
		
		ResourceId GetID(uint64_t obj)
		{
			struct obj_compare
			{
				obj_compare(const uint64_t o) : obj(o) {}
				bool operator() (const std::pair<VkResource,ResourceId> &p) { return (p.first.handle == obj); }
				uint64_t obj;
			};

			auto it = std::find_if(m_CurrentResourceIds.begin(), m_CurrentResourceIds.end(), obj_compare(obj));
			if(it != m_CurrentResourceIds.end())
				return it->second;

			return ResourceId();
		}
		
		using ResourceManager::GetCurrentResource;

		VkResource GetCurrentResource(VkResource obj)
		{
			return ResourceManager::GetCurrentResource(GetID(obj));
		}
		
		using ResourceManager::HasCurrentResource;

		bool HasCurrentResource(VkResource obj)
		{
			return ResourceManager::HasCurrentResource(GetID(obj));
		}

		VkResourceRecord *AddResourceRecord(ResourceId id)
		{
			VkResourceRecord *ret = ResourceManager::AddResourceRecord(id);
			VkResource res = GetCurrentResource(id);

			m_VkResourceRecords[res] = ret;

			return ret;
		}

		VkResourceRecord *AddResourceRecord(VkResource res, ResourceId id)
		{
			VkResourceRecord *ret = ResourceManager::AddResourceRecord(id);
			m_VkResourceRecords[res] = ret;
			return ret;
		}
		
		using ResourceManager::HasResourceRecord;

		bool HasResourceRecord(VkResource res)
		{
			auto it = m_VkResourceRecords.find(res);
			if(it != m_VkResourceRecords.end())
				return true;

			return ResourceManager::HasResourceRecord(GetID(res));
		}
		
		using ResourceManager::GetResourceRecord;

		VkResourceRecord *GetResourceRecord(VkResource res)
		{
			auto it = m_VkResourceRecords.find(res);
			if(it != m_VkResourceRecords.end())
				return it->second;

			return ResourceManager::GetResourceRecord(GetID(res));
		}
		
		VkResourceRecord *GetResourceRecord(uint64_t obj)
		{
			return ResourceManager::GetResourceRecord(GetID(obj));
		}
		
		using ResourceManager::MarkResourceFrameReferenced;

		void MarkResourceFrameReferenced(VkResource res, FrameRefType refType)
		{
			if(res.handle == 0) return;
			ResourceManager::MarkResourceFrameReferenced(GetID(res), refType);
		}

		// handling memory & image transitions
		void RecordTransitions(vector< pair<ResourceId, ImageRegionState> > &trans, map<ResourceId, ImgState> &states,
			                   uint32_t numTransitions, const VkImageMemoryBarrier *transitions);

		void ApplyTransitions(vector< pair<ResourceId, ImageRegionState> > &trans, map<ResourceId, ImgState> &states);

		void SerialiseImageStates(Serialiser *ser, map<ResourceId, ImgState> &states, vector<VkImageMemoryBarrier> &transitions);
		
	private:
		bool SerialisableResource(ResourceId id, VkResourceRecord *record);
		
		bool ResourceTypeRelease(VkResource res);

		bool Force_InitialState(VkResource res);
		bool Need_InitialStateChunk(VkResource res);
		bool Prepare_InitialState(VkResource res);
		bool Serialise_InitialState(VkResource res);
		void Create_InitialState(ResourceId id, VkResource live, bool hasData);
		void Apply_InitialState(VkResource live, InitialContentData initial);

		static VulkanResourceManager *m_Inst;

		map<VkResource, VkResourceRecord*> m_VkResourceRecords;

		map<VkResource, ResourceId> m_CurrentResourceIds;

		WrappedVulkan *m_Core;
};
