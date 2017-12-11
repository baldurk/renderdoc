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

#pragma once

#include <algorithm>
#include <utility>
#include "core/resource_manager.h"
#include "vk_resources.h"

using std::pair;

class WrappedVulkan;

class VulkanResourceManager
    : public ResourceManager<WrappedVkRes *, TypedRealHandle, VkResourceRecord>
{
public:
  VulkanResourceManager(CaptureState state, WrappedVulkan *core)
      : ResourceManager(), m_State(state), m_Core(core)
  {
  }
  ~VulkanResourceManager() {}
  void ClearWithoutReleasing()
  {
    // if any objects leaked past, it's no longer safe to delete them as we would
    // be calling Shutdown() after the device that owns them is destroyed. Instead
    // we just have to leak ourselves.
    RDCASSERT(m_LiveResourceMap.empty());
    RDCASSERT(m_InitialContents.empty());
    RDCASSERT(m_ResourceRecords.empty());
    RDCASSERT(m_CurrentResourceMap.empty());
    RDCASSERT(m_WrapperMap.empty());

    m_LiveResourceMap.clear();
    m_InitialContents.clear();
    m_ResourceRecords.clear();
    m_CurrentResourceMap.clear();
    m_WrapperMap.clear();
  }

  template <typename realtype>
  void AddLiveResource(ResourceId id, realtype obj)
  {
    ResourceManager::AddLiveResource(id, GetWrapped(obj));
  }

  using ResourceManager::AddResourceRecord;

  template <typename realtype>
  VkResourceRecord *AddResourceRecord(realtype &obj)
  {
    typename UnwrapHelper<realtype>::Outer *wrapped = GetWrapped(obj);
    VkResourceRecord *ret = wrapped->record = ResourceManager::AddResourceRecord(wrapped->id);

    ret->Resource = (WrappedVkRes *)wrapped;

    return ret;
  }

  // easy path for getting the unwrapped handle cast to the
  // write type. Saves a lot of work casting to either WrappedVkNonDispRes
  // or WrappedVkDispRes depending on the type, then ->real, then casting
  // when this is all we want to do in most cases
  template <typename realtype>
  realtype GetLiveHandle(ResourceId origid)
  {
    return realtype((uint64_t)(
        (typename UnwrapHelper<realtype>::ParentType *)ResourceManager::GetLiveResource(origid)));
  }

  template <typename realtype>
  realtype GetCurrentHandle(ResourceId id)
  {
    return realtype((uint64_t)(
        (typename UnwrapHelper<realtype>::ParentType *)ResourceManager::GetCurrentResource(id)));
  }

  // handling memory & image layouts
  template <typename SrcBarrierType>
  void RecordSingleBarrier(vector<pair<ResourceId, ImageRegionState> > &states, ResourceId id,
                           const SrcBarrierType &t, uint32_t nummips, uint32_t numslices);

  void RecordBarriers(vector<pair<ResourceId, ImageRegionState> > &states,
                      const map<ResourceId, ImageLayouts> &layouts, uint32_t numBarriers,
                      const VkImageMemoryBarrier *barriers);

  void MergeBarriers(vector<pair<ResourceId, ImageRegionState> > &dststates,
                     vector<pair<ResourceId, ImageRegionState> > &srcstates);

  void ApplyBarriers(vector<pair<ResourceId, ImageRegionState> > &states,
                     map<ResourceId, ImageLayouts> &layouts);

  template <typename SerialiserType>
  void SerialiseImageStates(SerialiserType &ser, std::map<ResourceId, ImageLayouts> &states,
                            std::vector<VkImageMemoryBarrier> &barriers);

  ResourceId GetID(WrappedVkRes *res)
  {
    if(res == NULL)
      return ResourceId();

    if(IsDispatchableRes(res))
      return ((WrappedVkDispRes *)res)->id;

    return ((WrappedVkNonDispRes *)res)->id;
  }

  template <typename realtype>
  WrappedVkNonDispRes *GetNonDispWrapper(realtype real)
  {
    return (WrappedVkNonDispRes *)GetWrapper(ToTypedHandle(real));
  }

  template <typename parenttype, typename realtype>
  ResourceId WrapResource(parenttype parentObj, realtype &obj)
  {
    RDCASSERT(obj != VK_NULL_HANDLE);

    ResourceId id = ResourceIDGen::GetNewUniqueID();
    typename UnwrapHelper<realtype>::Outer *wrapped =
        new typename UnwrapHelper<realtype>::Outer(obj, id);

    SetTableIfDispatchable(IsCaptureMode(m_State), parentObj, m_Core, wrapped);

    AddCurrentResource(id, wrapped);

    if(IsReplayMode(m_State))
      AddWrapper(wrapped, ToTypedHandle(obj));

    obj = realtype((uint64_t)wrapped);

    return id;
  }

  template <typename realtype>
  void ReleaseWrappedResource(realtype obj, bool clearID = false)
  {
    ResourceId id = GetResID(obj);

    auto origit = m_OriginalIDs.find(id);
    if(origit != m_OriginalIDs.end())
      EraseLiveResource(origit->second);

    if(IsReplayMode(m_State))
      ResourceManager::RemoveWrapper(ToTypedHandle(Unwrap(obj)));

    ResourceManager::ReleaseCurrentResource(id);
    VkResourceRecord *record = GetRecord(obj);
    if(record)
    {
      // we need to lock here because the app could be creating
      // and deleting from this pool at the same time. We do know
      // though that the pool isn't going to be destroyed while
      // either allocation or freeing happens, so we only need to
      // lock against concurrent allocs or deletes of children.

      if(ToTypedHandle(obj).type == eResCommandBuffer && record->cmdInfo &&
         record->cmdInfo->allocRecord)
      {
        record->cmdInfo->allocRecord->Delete(this);
        record->cmdInfo->allocRecord = NULL;
      }

      if(record->bakedCommands)
      {
        record->bakedCommands->Delete(this);
        record->bakedCommands = NULL;
      }

      if(record->pool)
      {
        // here we lock against concurrent alloc/delete
        record->pool->LockChunks();
        for(auto it = record->pool->pooledChildren.begin();
            it != record->pool->pooledChildren.end(); ++it)
        {
          if(*it == record)
          {
            // remove it from our pool so we don't try and destroy it
            record->pool->pooledChildren.erase(it);
            break;
          }
        }
        record->pool->UnlockChunks();
      }
      else if(record->pooledChildren.size())
      {
        // delete all of our children
        for(auto it = record->pooledChildren.begin(); it != record->pooledChildren.end(); ++it)
        {
          // unset record->pool so we don't recurse
          (*it)->pool = NULL;
          VkResourceType restype = IdentifyTypeByPtr((*it)->Resource);
          if(restype == eResDescriptorSet)
            ReleaseWrappedResource((VkDescriptorSet)(uint64_t)(*it)->Resource, true);
          else if(restype == eResCommandBuffer)
            ReleaseWrappedResource((VkCommandBuffer)(*it)->Resource, true);
          else if(restype == eResQueue)
            ReleaseWrappedResource((VkQueue)(*it)->Resource, true);
          else if(restype == eResPhysicalDevice)
            ReleaseWrappedResource((VkPhysicalDevice)(*it)->Resource, true);
          else
            RDCERR("Unexpected resource type %d as pooled child!", restype);
        }
        record->pooledChildren.clear();
      }

      record->Delete(this);
    }
    if(clearID)
    {
      // note the nulling of the wrapped object's ID here is rather unpleasant,
      // but the lesser of two evils to ensure that stale descriptor set slots
      // referencing the object behave safely. To do this correctly we would need
      // to maintain a list of back-references to every descriptor set that has
      // this object bound, and invalidate them. Instead we just make sure the ID
      // is always something sensible, since we know the deallocation doesn't
      // free the memory - the object is pool-allocated.
      // If a new object is allocated in that pool slot, it will still be a valid
      // ID and if the resource isn't ever referenced elsewhere, it will just be
      // a non-live ID to be ignored.

      if(IsDispatchable(obj))
      {
        WrappedVkDispRes *res = (WrappedVkDispRes *)GetWrapped(obj);
        res->id = ResourceId();
        res->record = NULL;
      }
      else
      {
        WrappedVkNonDispRes *res = (WrappedVkNonDispRes *)GetWrapped(obj);
        res->id = ResourceId();
        res->record = NULL;
      }
    }
    delete GetWrapped(obj);
  }

  // helper for sparse mappings
  void MarkSparseMapReferenced(SparseMapping *sparse);

private:
  bool SerialisableResource(ResourceId id, VkResourceRecord *record);

  bool ResourceTypeRelease(WrappedVkRes *res);

  bool Force_InitialState(WrappedVkRes *res, bool prepare);
  bool AllowDeletedResource_InitialState() { return true; }
  bool Need_InitialStateChunk(WrappedVkRes *res);
  bool Prepare_InitialState(WrappedVkRes *res);
  uint32_t GetSize_InitialState(ResourceId id, WrappedVkRes *res);
  bool Serialise_InitialState(WriteSerialiser &ser, ResourceId resid, WrappedVkRes *res);
  void Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData);
  void Apply_InitialState(WrappedVkRes *live, InitialContentData initial);

  CaptureState m_State;
  WrappedVulkan *m_Core;
};
