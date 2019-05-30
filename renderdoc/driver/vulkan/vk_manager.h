/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

class WrappedVulkan;

struct MemIDOffset
{
  ResourceId memory;
  VkDeviceSize memOffs;
};

DECLARE_REFLECTION_STRUCT(MemIDOffset);

struct SparseBufferInitState
{
  VkSparseMemoryBind *binds;
  uint32_t numBinds;

  MemIDOffset *memDataOffs;
  uint32_t numUniqueMems;

  VkDeviceSize totalSize;
};

DECLARE_REFLECTION_STRUCT(SparseBufferInitState);

struct SparseImageInitState
{
  VkSparseMemoryBind *opaque;
  uint32_t opaqueCount;

  VkExtent3D imgdim;    // in pages
  VkExtent3D pagedim;

  // available on capture - filled out in Prepare_SparseInitialState and serialised to disk
  MemIDOffset *pages[NUM_VK_IMAGE_ASPECTS];

  uint32_t pageCount[NUM_VK_IMAGE_ASPECTS];

  // available on replay - filled out in the read path of Serialise_SparseInitialState
  VkSparseImageMemoryBind *pageBinds[NUM_VK_IMAGE_ASPECTS];

  MemIDOffset *memDataOffs;
  uint32_t numUniqueMems;

  VkDeviceSize totalSize;
};

DECLARE_REFLECTION_STRUCT(SparseImageInitState);

// this struct is copied around and for that reason we explicitly keep it simple and POD. The
// lifetime of the memory allocated is controlled by the resource manager - when preparing or
// serialising, we explicitly set the initial contents, then when the whole system is done with them
// we free them again.
struct VkInitialContents
{
  enum Tag
  {
    BufferCopy = 0,
    ClearColorImage = 1,
    ClearDepthStencilImage,
    Sparse,
    DescriptorSet,
  };

  VkInitialContents()
  {
    RDCCOMPILE_ASSERT(std::is_standard_layout<VkInitialContents>::value,
                      "VkInitialContents must be POD");
    memset(this, 0, sizeof(*this));
  }

  VkInitialContents(VkResourceType t, Tag tg)
  {
    memset(this, 0, sizeof(*this));
    type = t;
    tag = tg;
  }

  VkInitialContents(VkResourceType t, MemoryAllocation m)
  {
    memset(this, 0, sizeof(*this));
    type = t;
    mem = m;
  }

  template <typename Configuration>
  void Free(ResourceManager<Configuration> *rm)
  {
    // any of these will be NULL if unused
    SAFE_DELETE_ARRAY(descriptorSlots);
    SAFE_DELETE_ARRAY(descriptorWrites);
    SAFE_DELETE_ARRAY(descriptorInfo);

    rm->ResourceTypeRelease(GetWrapped(buf));
    rm->ResourceTypeRelease(GetWrapped(img));

    // memory is not free'd here

    if(tag == Sparse)
    {
      if(type == eResImage)
      {
        SAFE_DELETE_ARRAY(sparseImage.opaque);
        for(uint32_t i = 0; i < NUM_VK_IMAGE_ASPECTS; i++)
        {
          SAFE_DELETE_ARRAY(sparseImage.pages[i]);
          SAFE_DELETE_ARRAY(sparseImage.pageBinds[i]);
        }
        SAFE_DELETE_ARRAY(sparseImage.memDataOffs);
      }
      else if(type == eResBuffer)
      {
        SAFE_DELETE_ARRAY(sparseBuffer.binds);
        SAFE_DELETE_ARRAY(sparseBuffer.memDataOffs);
      }
    }
  }

  // for descriptor heaps, when capturing we save the slots, when replaying we store direct writes
  DescriptorSetSlot *descriptorSlots;
  VkWriteDescriptorSet *descriptorWrites;
  VkDescriptorBufferInfo *descriptorInfo;
  uint32_t numDescriptors;

  // for plain resources, we store the resource type and memory allocation details of the contents
  VkResourceType type;
  VkBuffer buf;
  VkImage img;
  MemoryAllocation mem;
  Tag tag;

  // sparse resources need extra information. Which one is valid, depends on the value of type above
  union
  {
    SparseBufferInitState sparseBuffer;
    SparseImageInitState sparseImage;
  };
};

struct VulkanResourceManagerConfiguration
{
  typedef WrappedVkRes *WrappedResourceType;
  typedef TypedRealHandle RealResourceType;
  typedef VkResourceRecord RecordType;
  typedef VkInitialContents InitialContentData;
};

struct MemRefInterval
{
  ResourceId memory;
  uint64_t start;
  FrameRefType refType;
};

DECLARE_REFLECTION_STRUCT(MemRefInterval);

class VulkanResourceManager : public ResourceManager<VulkanResourceManagerConfiguration>
{
public:
  VulkanResourceManager(CaptureState state, WrappedVulkan *core)
      : ResourceManager(), m_State(state), m_Core(core)
  {
  }
  void SetState(CaptureState state) { m_State = state; }
  CaptureState GetState() { return m_State; }
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

  ResourceId GetFirstIDForHandle(uint64_t handle);

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
  void RecordSingleBarrier(std::vector<rdcpair<ResourceId, ImageRegionState> > &states, ResourceId id,
                           const SrcBarrierType &t, uint32_t nummips, uint32_t numslices);

  void RecordBarriers(std::vector<rdcpair<ResourceId, ImageRegionState> > &states,
                      const std::map<ResourceId, ImageLayouts> &layouts, uint32_t numBarriers,
                      const VkImageMemoryBarrier *barriers);

  void MergeBarriers(std::vector<rdcpair<ResourceId, ImageRegionState> > &dststates,
                     std::vector<rdcpair<ResourceId, ImageRegionState> > &srcstates);

  void ApplyBarriers(uint32_t queueFamilyIndex,
                     std::vector<rdcpair<ResourceId, ImageRegionState> > &states,
                     std::map<ResourceId, ImageLayouts> &layouts);

  template <typename SerialiserType>
  void SerialiseImageStates(SerialiserType &ser, std::map<ResourceId, ImageLayouts> &states,
                            std::vector<VkImageMemoryBarrier> &barriers);

  template <typename SerialiserType>
  bool Serialise_DeviceMemoryRefs(SerialiserType &ser, std::vector<MemRefInterval> &data);

  template <typename SerialiserType>
  bool Serialise_ImageRefs(SerialiserType &ser, std::vector<ImgRefsPair> &data);

  void InsertDeviceMemoryRefs(WriteSerialiser &ser);
  void InsertImageRefs(WriteSerialiser &ser);

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
  void MarkSparseMapReferenced(ResourceInfo *sparse);

  void SetInternalResource(ResourceId id);

  void MarkImageFrameReferenced(const VkResourceRecord *img, const ImageRange &range,
                                FrameRefType refType);
  void MarkImageFrameReferenced(ResourceId img, const ImageInfo &imageInfo, const ImageRange &range,
                                FrameRefType refType);
  void MarkMemoryFrameReferenced(ResourceId mem, VkDeviceSize start, VkDeviceSize end,
                                 FrameRefType refType);

  void MergeReferencedMemory(std::map<ResourceId, MemRefs> &memRefs);
  void MergeReferencedImages(std::map<ResourceId, ImgRefs> &imgRefs);
  void ClearReferencedImages();
  void ClearReferencedMemory();
  MemRefs *FindMemRefs(ResourceId mem);
  ImgRefs *FindImgRefs(ResourceId img);

  inline bool OptimizeInitialState() { return m_OptimizeInitialState; }
private:
  bool ResourceTypeRelease(WrappedVkRes *res);

  bool Prepare_InitialState(WrappedVkRes *res);
  uint64_t GetSize_InitialState(ResourceId id, const VkInitialContents &initial);
  bool Serialise_InitialState(WriteSerialiser &ser, ResourceId id, VkResourceRecord *record,
                              const VkInitialContents *initial);
  void Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData);
  void Apply_InitialState(WrappedVkRes *live, const VkInitialContents &initial);
  std::vector<ResourceId> InitialContentResources();

  CaptureState m_State;
  WrappedVulkan *m_Core;
  std::map<ResourceId, MemRefs> m_MemFrameRefs;
  std::map<ResourceId, ImgRefs> m_ImgFrameRefs;
  bool m_OptimizeInitialState = false;
};
