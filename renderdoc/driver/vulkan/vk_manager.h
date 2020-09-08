/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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
    SAFE_DELETE_ARRAY(inlineInfo);
    FreeAlignedBuffer(inlineData);

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
  VkWriteDescriptorSetInlineUniformBlockEXT *inlineInfo;
  byte *inlineData;
  size_t inlineByteSize;
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
  VulkanResourceManager(CaptureState &state, WrappedVulkan *core)
      : ResourceManager(state), m_Core(core)
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
    using WrappedType = typename UnwrapHelper<realtype>::Outer;
    WrappedType *wrapped = GetWrapped(obj);
    VkResourceRecord *ret = wrapped->record = ResourceManager::AddResourceRecord(wrapped->id);

    ret->Resource = (WrappedVkRes *)wrapped;
    ret->resType = (VkResourceType)WrappedType::TypeEnum;

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
  void RecordSingleBarrier(rdcarray<rdcpair<ResourceId, ImageRegionState> > &states, ResourceId id,
                           const SrcBarrierType &t, uint32_t nummips, uint32_t numslices);

  void RecordBarriers(rdcarray<rdcpair<ResourceId, ImageRegionState> > &states,
                      const std::map<ResourceId, ImageLayouts> &layouts, uint32_t numBarriers,
                      const VkImageMemoryBarrier *barriers);

  void MergeBarriers(rdcarray<rdcpair<ResourceId, ImageRegionState> > &dststates,
                     rdcarray<rdcpair<ResourceId, ImageRegionState> > &srcstates);

  void ApplyBarriers(uint32_t queueFamilyIndex,
                     rdcarray<rdcpair<ResourceId, ImageRegionState> > &states,
                     std::map<ResourceId, ImageLayouts> &layouts);

  void RecordBarriers(rdcflatmap<ResourceId, ImageState> &states, uint32_t queueFamilyIndex,
                      uint32_t numBarriers, const VkImageMemoryBarrier *barriers);

  template <typename SerialiserType>
  void SerialiseImageStates(SerialiserType &ser, std::map<ResourceId, LockingImageState> &states);

  template <typename SerialiserType>
  bool Serialise_DeviceMemoryRefs(SerialiserType &ser, rdcarray<MemRefInterval> &data);

  bool Serialise_ImageRefs(ReadSerialiser &ser, std::map<ResourceId, LockingImageState> &states);

  void InsertDeviceMemoryRefs(WriteSerialiser &ser);

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

  template <typename realtype>
  WrappedVkDispRes *GetDispWrapper(realtype real)
  {
    return (WrappedVkDispRes *)GetWrapper(ToTypedHandle(real));
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
  ResourceId WrapReusedResource(VkResourceRecord *record, realtype &obj)
  {
    RDCASSERT(obj != VK_NULL_HANDLE);

    typename UnwrapHelper<realtype>::Outer *wrapped =
        (typename UnwrapHelper<realtype>::Outer *)record->Resource;
    wrapped->real = ToTypedHandle(obj).real;

    obj = realtype((uint64_t)wrapped);

    return wrapped->id;
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
        // here we lock against concurrent alloc/delete and remove it from our pool so we don't try
        // and destroy it
        record->pool->LockChunks();
        record->pool->pooledChildren.removeOne(record);
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

  void MarkMemoryFrameReferenced(ResourceId mem, VkDeviceSize start, VkDeviceSize end,
                                 FrameRefType refType);
  void AddMemoryFrameRefs(ResourceId mem);
  void AddDeviceMemory(ResourceId mem);
  void RemoveDeviceMemory(ResourceId mem);

  void MergeReferencedMemory(rdcflatmap<ResourceId, MemRefs> &memRefs);
  void ClearReferencedMemory();
  MemRefs *FindMemRefs(ResourceId mem);
  ImgRefs *FindImgRefs(ResourceId img);

  inline InitPolicy GetInitPolicy() { return m_InitPolicy; }
  void SetOptimisationLevel(ReplayOptimisationLevel level)
  {
    switch(level)
    {
      case ReplayOptimisationLevel::Count:
        RDCERR("Invalid optimisation level specified");
        m_InitPolicy = eInitPolicy_NoOpt;
        break;
      case ReplayOptimisationLevel::NoOptimisation: m_InitPolicy = eInitPolicy_NoOpt; break;
      case ReplayOptimisationLevel::Conservative: m_InitPolicy = eInitPolicy_CopyAll; break;
      case ReplayOptimisationLevel::Balanced: m_InitPolicy = eInitPolicy_ClearUnread; break;
      case ReplayOptimisationLevel::Fastest: m_InitPolicy = eInitPolicy_Fastest; break;
    }
  }

  bool IsResourceTrackedForPersistency(WrappedVkRes *const &res);

private:
  bool ResourceTypeRelease(WrappedVkRes *res);

  bool Prepare_InitialState(WrappedVkRes *res);
  uint64_t GetSize_InitialState(ResourceId id, const VkInitialContents &initial);
  bool Serialise_InitialState(WriteSerialiser &ser, ResourceId id, VkResourceRecord *record,
                              const VkInitialContents *initial);
  void Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData);
  void Apply_InitialState(WrappedVkRes *live, const VkInitialContents &initial);
  rdcarray<ResourceId> InitialContentResources();

  WrappedVulkan *m_Core;
  rdcflatmap<ResourceId, MemRefs> m_MemFrameRefs;
  std::set<ResourceId> m_DeviceMemories;
  InitPolicy m_InitPolicy = eInitPolicy_CopyAll;
};
