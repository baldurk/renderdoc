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

#include "vk_core.h"
#include "vk_debug.h"

struct MemIDOffset
{
  ResourceId memory;
  VkDeviceSize memOffs;
};

DECLARE_REFLECTION_STRUCT(MemIDOffset);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MemIDOffset &el)
{
  SERIALISE_MEMBER(memory);
  SERIALISE_MEMBER(memOffs);
}

struct SparseBufferInitState
{
  uint32_t numBinds;
  VkSparseMemoryBind *binds;

  uint32_t numUniqueMems;
  MemIDOffset *memDataOffs;

  VkDeviceSize totalSize;
};

DECLARE_REFLECTION_STRUCT(SparseBufferInitState);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, SparseBufferInitState &el)
{
  SERIALISE_MEMBER_ARRAY(binds, numBinds);
  SERIALISE_MEMBER_ARRAY(memDataOffs, numUniqueMems);
  SERIALISE_MEMBER(totalSize);
}

template <>
void Deserialise(const SparseBufferInitState &el)
{
  delete[] el.binds;
  delete[] el.memDataOffs;
}

struct SparseImageInitState
{
  uint32_t opaqueCount;
  VkSparseMemoryBind *opaque;

  VkExtent3D imgdim;    // in pages
  VkExtent3D pagedim;
  uint32_t pageCount[NUM_VK_IMAGE_ASPECTS];

  // available on capture - filled out in Prepare_SparseInitialState and serialised to disk
  MemIDOffset *pages[NUM_VK_IMAGE_ASPECTS];

  // available on replay - filled out in the read path of Serialise_SparseInitialState
  VkSparseImageMemoryBind *pageBinds[NUM_VK_IMAGE_ASPECTS];

  uint32_t numUniqueMems;
  MemIDOffset *memDataOffs;

  VkDeviceSize totalSize;
};

DECLARE_REFLECTION_STRUCT(SparseImageInitState);

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, SparseImageInitState &el)
{
  SERIALISE_MEMBER_ARRAY(opaque, opaqueCount);
  SERIALISE_MEMBER(imgdim);
  SERIALISE_MEMBER(pagedim);
  SERIALISE_MEMBER_ARRAY(memDataOffs, numUniqueMems);
  SERIALISE_MEMBER(totalSize);

  for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
    SERIALISE_MEMBER_ARRAY(pages[a], pageCount[a]);
}

template <>
void Deserialise(const SparseImageInitState &el)
{
  delete[] el.opaque;
  delete[] el.memDataOffs;
  for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
    delete[] el.pages[a];
}

bool WrappedVulkan::Prepare_SparseInitialState(WrappedVkBuffer *buf)
{
  ResourceId id = buf->id;

  // VKTODOLOW this is a bit conservative, as we save the whole memory object rather than just the
  // bound range.
  map<VkDeviceMemory, VkDeviceSize> boundMems;

  // value will be filled out later once all memories are added
  for(size_t i = 0; i < buf->record->sparseInfo->opaquemappings.size(); i++)
    boundMems[buf->record->sparseInfo->opaquemappings[i].memory] = 0;

  uint32_t numElems = (uint32_t)buf->record->sparseInfo->opaquemappings.size();

  SparseBufferInitState *info = (SparseBufferInitState *)AllocAlignedBuffer(
      sizeof(SparseBufferInitState) + sizeof(VkSparseMemoryBind) * numElems +
      sizeof(MemIDOffset) * boundMems.size());

  VkSparseMemoryBind *binds = (VkSparseMemoryBind *)(info + 1);
  MemIDOffset *memDataOffs = (MemIDOffset *)(binds + numElems);

  info->numBinds = numElems;
  info->numUniqueMems = (uint32_t)boundMems.size();
  info->memDataOffs = memDataOffs;
  info->binds = binds;

  memcpy(binds, &buf->record->sparseInfo->opaquemappings[0], sizeof(VkSparseMemoryBind) * numElems);

  VkDevice d = GetDev();
  // INITSTATEBATCH
  VkCommandBuffer cmd = GetNextCmd();

  VkBufferCreateInfo bufInfo = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      NULL,
      0,
      0,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
  };

  uint32_t memidx = 0;
  for(auto it = boundMems.begin(); it != boundMems.end(); ++it)
  {
    // store offset
    it->second = bufInfo.size;

    memDataOffs[memidx].memory = GetResID(it->first);
    memDataOffs[memidx].memOffs = bufInfo.size;

    // increase size
    bufInfo.size += GetRecord(it->first)->Length;
    memidx++;
  }

  info->totalSize = bufInfo.size;

  VkDeviceMemory readbackmem = VK_NULL_HANDLE;

  // since these are very short lived, they are not wrapped
  VkBuffer dstBuf;

  VkResult vkr = VK_SUCCESS;

  vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMemoryRequirements mrq = {0};

  ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), dstBuf, &mrq);

  VkMemoryAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, bufInfo.size,
      GetReadbackMemoryIndex(mrq.memoryTypeBits),
  };

  allocInfo.allocationSize = AlignUp(allocInfo.allocationSize, mrq.alignment);

  vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &readbackmem);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  GetResourceManager()->WrapResource(Unwrap(d), readbackmem);

  vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(readbackmem), 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vector<VkBuffer> bufdeletes;
  bufdeletes.push_back(dstBuf);

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // copy all of the bound memory objects
  for(auto it = boundMems.begin(); it != boundMems.end(); ++it)
  {
    VkBuffer srcBuf;

    bufInfo.size = GetRecord(it->first)->Length;
    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &srcBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), srcBuf, Unwrap(it->first), 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // copy srcbuf into its area in dstbuf
    VkBufferCopy region = {0, it->second, bufInfo.size};

    ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), srcBuf, dstBuf, 1, &region);

    bufdeletes.push_back(srcBuf);
  }

  vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // INITSTATEBATCH
  SubmitCmds();
  FlushQ();

  for(size_t i = 0; i < bufdeletes.size(); i++)
    ObjDisp(d)->DestroyBuffer(Unwrap(d), bufdeletes[i], NULL);

  GetResourceManager()->SetInitialContents(
      id, VulkanResourceManager::InitialContentData(eResBuffer, GetWrapped(readbackmem), 0,
                                                    (byte *)info));

  return true;
}

bool WrappedVulkan::Prepare_SparseInitialState(WrappedVkImage *im)
{
  ResourceId id = im->id;

  SparseMapping *sparse = im->record->sparseInfo;

  // VKTODOLOW this is a bit conservative, as we save the whole memory object rather than just the
  // bound range.
  map<VkDeviceMemory, VkDeviceSize> boundMems;

  // value will be filled out later once all memories are added
  for(size_t i = 0; i < sparse->opaquemappings.size(); i++)
    boundMems[sparse->opaquemappings[i].memory] = 0;

  uint32_t pagePerAspect = sparse->imgdim.width * sparse->imgdim.height * sparse->imgdim.depth;

  for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
  {
    if(sparse->pages[a])
    {
      for(uint32_t i = 0; i < pagePerAspect; i++)
        if(sparse->pages[a][i].first != VK_NULL_HANDLE)
          boundMems[sparse->pages[a][i].first] = 0;
    }
  }

  uint32_t totalPageCount = 0;
  for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
    totalPageCount += sparse->pages[a] ? pagePerAspect : 0;

  uint32_t opaqueCount = (uint32_t)sparse->opaquemappings.size();

  byte *blob = AllocAlignedBuffer(
      sizeof(SparseImageInitState) + sizeof(VkSparseMemoryBind) * opaqueCount +
      sizeof(MemIDOffset) * totalPageCount + sizeof(MemIDOffset) * boundMems.size());

  SparseImageInitState *state = (SparseImageInitState *)blob;
  VkSparseMemoryBind *opaque = (VkSparseMemoryBind *)(state + 1);
  MemIDOffset *pages = (MemIDOffset *)(opaque + opaqueCount);
  MemIDOffset *memDataOffs = (MemIDOffset *)(pages + totalPageCount);

  state->opaque = opaque;
  state->opaqueCount = opaqueCount;
  state->pagedim = sparse->pagedim;
  state->imgdim = sparse->imgdim;
  state->numUniqueMems = (uint32_t)boundMems.size();
  state->memDataOffs = memDataOffs;

  if(opaqueCount > 0)
    memcpy(opaque, &sparse->opaquemappings[0], sizeof(VkSparseMemoryBind) * opaqueCount);

  for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
  {
    state->pageCount[a] = (sparse->pages[a] ? pagePerAspect : 0);

    if(state->pageCount[a] != 0)
    {
      state->pages[a] = pages;

      for(uint32_t i = 0; i < pagePerAspect; i++)
      {
        state->pages[a][i].memory = GetResID(sparse->pages[a][i].first);
        state->pages[a][i].memOffs = sparse->pages[a][i].second;
      }

      pages += pagePerAspect;
    }
    else
    {
      state->pages[a] = NULL;
    }
  }

  VkDevice d = GetDev();
  // INITSTATEBATCH
  VkCommandBuffer cmd = GetNextCmd();

  VkBufferCreateInfo bufInfo = {
      VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      NULL,
      0,
      0,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
  };

  uint32_t memidx = 0;
  for(auto it = boundMems.begin(); it != boundMems.end(); ++it)
  {
    // store offset
    it->second = bufInfo.size;

    memDataOffs[memidx].memory = GetResID(it->first);
    memDataOffs[memidx].memOffs = bufInfo.size;

    // increase size
    bufInfo.size += GetRecord(it->first)->Length;
    memidx++;
  }

  state->totalSize = bufInfo.size;

  VkDeviceMemory readbackmem = VK_NULL_HANDLE;

  // since these are very short lived, they are not wrapped
  VkBuffer dstBuf;

  VkResult vkr = VK_SUCCESS;

  vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMemoryRequirements mrq = {0};

  ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), dstBuf, &mrq);

  VkMemoryAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, bufInfo.size,
      GetReadbackMemoryIndex(mrq.memoryTypeBits),
  };

  allocInfo.allocationSize = AlignUp(allocInfo.allocationSize, mrq.alignment);

  vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &readbackmem);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  GetResourceManager()->WrapResource(Unwrap(d), readbackmem);

  vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(readbackmem), 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vector<VkBuffer> bufdeletes;
  bufdeletes.push_back(dstBuf);

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // copy all of the bound memory objects
  for(auto it = boundMems.begin(); it != boundMems.end(); ++it)
  {
    VkBuffer srcBuf;

    bufInfo.size = GetRecord(it->first)->Length;
    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &srcBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), srcBuf, Unwrap(it->first), 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // copy srcbuf into its area in dstbuf
    VkBufferCopy region = {0, it->second, bufInfo.size};

    ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), srcBuf, dstBuf, 1, &region);

    bufdeletes.push_back(srcBuf);
  }

  vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // INITSTATEBATCH
  SubmitCmds();
  FlushQ();

  for(size_t i = 0; i < bufdeletes.size(); i++)
    ObjDisp(d)->DestroyBuffer(Unwrap(d), bufdeletes[i], NULL);

  GetResourceManager()->SetInitialContents(
      id, VulkanResourceManager::InitialContentData(eResImage, GetWrapped(readbackmem), 0,
                                                    (byte *)blob));

  return true;
}

uint32_t WrappedVulkan::GetSize_SparseInitialState(ResourceId id, WrappedVkRes *res)
{
  VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
  VkResourceType type = IdentifyTypeByPtr(record->Resource);
  VulkanResourceManager::InitialContentData contents = GetResourceManager()->GetInitialContents(id);

  if(type == eResBuffer)
  {
    SparseBufferInitState *info = (SparseBufferInitState *)contents.blob;

    // some bytes just to cover overheads etc.
    uint32_t ret = 128;

    // the list of memory objects bound
    ret += 8 + sizeof(VkSparseMemoryBind) * info->numBinds;

    // the list of memory regions to copy
    ret += 8 + sizeof(MemIDOffset) * info->numUniqueMems;

    // the actual data
    ret += uint32_t(info->totalSize + WriteSerialiser::GetChunkAlignment());

    return ret;
  }
  else if(type == eResImage)
  {
    SparseImageInitState *info = (SparseImageInitState *)contents.blob;

    // some bytes just to cover overheads etc.
    uint32_t ret = 128;

    // the meta-data structure
    ret += sizeof(SparseImageInitState);

    // the list of memory objects bound
    ret += sizeof(VkSparseMemoryBind) * info->opaqueCount;

    // the page tables
    for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
      ret += 8 + sizeof(MemIDOffset) * info->pageCount[a];

    // the list of memory regions to copy
    ret += sizeof(MemIDOffset) * info->numUniqueMems;

    // the actual data
    ret += uint32_t(info->totalSize + WriteSerialiser::GetChunkAlignment());

    return ret;
  }

  RDCERR("Unhandled resource type %s", ToStr(type).c_str());
  return 128;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_SparseBufferInitialState(
    SerialiserType &ser, ResourceId id, VulkanResourceManager::InitialContentData contents)
{
  VkDevice d = !IsStructuredExporting(m_State) ? GetDev() : VK_NULL_HANDLE;
  VkResult vkr = VK_SUCCESS;

  SparseBufferInitState *info = (SparseBufferInitState *)contents.blob;

  SERIALISE_ELEMENT_LOCAL(SparseState, *info);

  VkDeviceMemory mappedMem = VK_NULL_HANDLE;
  byte *Contents = NULL;
  uint64_t ContentsSize = (uint64_t)SparseState.totalSize;

  // Serialise this separately so that it can be used on reading to prepare the upload memory
  SERIALISE_ELEMENT(ContentsSize);

  // the memory/buffer that we allocated on read, to upload the initial contents.
  VkDeviceMemory uploadMemory = VK_NULL_HANDLE;
  VkBuffer uploadBuf = VK_NULL_HANDLE;

  // during writing, we already have the memory copied off - we just need to map it.
  if(ser.IsWriting())
  {
    // the memory was created not wrapped.
    mappedMem = (VkDeviceMemory)(uint64_t)contents.resource;
    vkr =
        ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mappedMem), 0, VK_WHOLE_SIZE, 0, (void **)&Contents);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }
  else if(IsReplayingAndReading() && !ser.IsErrored())
  {
    // create a buffer with memory attached, which we will fill with the initial contents
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        ContentsSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &uploadBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), uploadBuf);

    VkMemoryRequirements mrq = {0};

    ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), Unwrap(uploadBuf), &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        GetUploadMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &uploadMemory);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), uploadMemory);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(uploadBuf), Unwrap(uploadMemory), 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    mappedMem = uploadMemory;

    ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(uploadMemory), 0, VK_WHOLE_SIZE, 0, (void **)&Contents);
  }

  // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
  // directly into upload memory
  ser.Serialise("Contents", Contents, ContentsSize, SerialiserFlags::NoFlags);

  // unmap the resource we mapped before - we need to do this on read and on write.
  if(!IsStructuredExporting(m_State) && mappedMem)
    ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mappedMem));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    // allocate a persistent set of metadata info
    info = (SparseBufferInitState *)AllocAlignedBuffer(
        sizeof(SparseBufferInitState) + sizeof(VkSparseMemoryBind) * SparseState.numBinds +
        sizeof(MemIDOffset) * SparseState.numUniqueMems);

    info->numBinds = SparseState.numBinds;
    info->numUniqueMems = SparseState.numUniqueMems;

    info->binds = (VkSparseMemoryBind *)(info + 1);
    info->memDataOffs = (MemIDOffset *)(info->binds + SparseState.numBinds);

    memcpy(info->binds, SparseState.binds, sizeof(VkSparseMemoryBind) * info->numBinds);
    memcpy(info->memDataOffs, SparseState.memDataOffs, sizeof(MemIDOffset) * info->numUniqueMems);

    m_CleanupMems.push_back(uploadMemory);

    GetResourceManager()->SetInitialContents(
        id, VulkanResourceManager::InitialContentData(eResBuffer, GetWrapped(uploadBuf), 0,
                                                      (byte *)info));
  }

  return true;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_SparseImageInitialState(SerialiserType &ser, ResourceId id,
                                                      VulkanResourceManager::InitialContentData contents)
{
  VkDevice d = !IsStructuredExporting(m_State) ? GetDev() : VK_NULL_HANDLE;
  VkResult vkr = VK_SUCCESS;

  SparseImageInitState *info = (SparseImageInitState *)contents.blob;

  SERIALISE_ELEMENT_LOCAL(SparseState, *info);

  VkDeviceMemory mappedMem = VK_NULL_HANDLE;
  byte *Contents = NULL;
  uint64_t ContentsSize = (uint64_t)SparseState.totalSize;

  // Serialise this separately so that it can be used on reading to prepare the upload memory
  SERIALISE_ELEMENT(ContentsSize);

  // the memory/buffer that we allocated on read, to upload the initial contents.
  VkDeviceMemory uploadMemory = VK_NULL_HANDLE;
  VkBuffer uploadBuf = VK_NULL_HANDLE;

  // during writing, we already have the memory copied off - we just need to map it.
  if(ser.IsWriting())
  {
    // the memory was created not wrapped.
    mappedMem = (VkDeviceMemory)(uint64_t)contents.resource;
    vkr =
        ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mappedMem), 0, VK_WHOLE_SIZE, 0, (void **)&Contents);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
  }
  else if(IsReplayingAndReading() && !ser.IsErrored())
  {
    // create a buffer with memory attached, which we will fill with the initial contents
    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        ContentsSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &uploadBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), uploadBuf);

    VkMemoryRequirements mrq = {0};

    ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), Unwrap(uploadBuf), &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        GetUploadMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &uploadMemory);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), uploadMemory);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(uploadBuf), Unwrap(uploadMemory), 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    mappedMem = uploadMemory;

    ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(uploadMemory), 0, VK_WHOLE_SIZE, 0, (void **)&Contents);
  }

  // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
  // directly into upload memory
  ser.Serialise("Contents", Contents, ContentsSize, SerialiserFlags::NoFlags);

  // unmap the resource we mapped before - we need to do this on read and on write.
  if(!IsStructuredExporting(m_State) && mappedMem)
    ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mappedMem));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    uint32_t totalPageCount = 0;

    for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
      totalPageCount += SparseState.pageCount[a];

    info = (SparseImageInitState *)AllocAlignedBuffer(
        sizeof(SparseImageInitState) + sizeof(VkSparseMemoryBind) * SparseState.opaqueCount +
        sizeof(VkSparseImageMemoryBind) * totalPageCount +
        sizeof(MemIDOffset) * SparseState.numUniqueMems);

    RDCEraseEl(info->pages);
    RDCEraseEl(info->pageBinds);

    info->opaque = (VkSparseMemoryBind *)(info + 1);
    VkSparseImageMemoryBind *pageBinds =
        (VkSparseImageMemoryBind *)(info->opaque + SparseState.opaqueCount);
    info->memDataOffs = (MemIDOffset *)(pageBinds + totalPageCount);

    info->opaqueCount = SparseState.opaqueCount;
    info->numUniqueMems = SparseState.numUniqueMems;
    info->memDataOffs = SparseState.memDataOffs;
    info->imgdim = SparseState.imgdim;
    info->pagedim = SparseState.pagedim;

    memcpy(info->opaque, SparseState.opaque, sizeof(VkSparseMemoryBind) * info->opaqueCount);
    memcpy(info->memDataOffs, SparseState.memDataOffs, sizeof(MemIDOffset) * info->numUniqueMems);

    for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
    {
      if(SparseState.pageCount[a] == 0)
      {
        info->pageBinds[a] = NULL;
      }
      else
      {
        info->pageBinds[a] = pageBinds;
        pageBinds += SparseState.pageCount[a];

        uint32_t i = 0;

        for(uint32_t z = 0; z < SparseState.imgdim.depth; z++)
        {
          for(uint32_t y = 0; y < SparseState.imgdim.height; y++)
          {
            for(uint32_t x = 0; x < SparseState.imgdim.width; x++)
            {
              VkSparseImageMemoryBind &p = info->pageBinds[a][i];

              p.memory = Unwrap(GetResourceManager()->GetLiveHandle<VkDeviceMemory>(
                  SparseState.pages[a][i].memory));
              p.memoryOffset = SparseState.pages[a][i].memOffs;
              p.extent = SparseState.pagedim;
              p.subresource.aspectMask = (VkImageAspectFlags)(1 << a);
              p.subresource.arrayLayer = 0;
              p.subresource.mipLevel = 0;
              p.offset.x = x * p.extent.width;
              p.offset.y = y * p.extent.height;
              p.offset.z = z * p.extent.depth;

              i++;
            }
          }
        }
      }
    }

    m_CleanupMems.push_back(uploadMemory);

    GetResourceManager()->SetInitialContents(
        id, VulkanResourceManager::InitialContentData(eResImage, GetWrapped(uploadBuf),
                                                      eInitialContents_Sparse, (byte *)info));
  }

  return true;
}

template bool WrappedVulkan::Serialise_SparseBufferInitialState(
    ReadSerialiser &ser, ResourceId id, VulkanResourceManager::InitialContentData contents);
template bool WrappedVulkan::Serialise_SparseBufferInitialState(
    WriteSerialiser &ser, ResourceId id, VulkanResourceManager::InitialContentData contents);
template bool WrappedVulkan::Serialise_SparseImageInitialState(
    ReadSerialiser &ser, ResourceId id, VulkanResourceManager::InitialContentData contents);
template bool WrappedVulkan::Serialise_SparseImageInitialState(
    WriteSerialiser &ser, ResourceId id, VulkanResourceManager::InitialContentData contents);

bool WrappedVulkan::Apply_SparseInitialState(WrappedVkBuffer *buf,
                                             VulkanResourceManager::InitialContentData contents)
{
  SparseBufferInitState *info = (SparseBufferInitState *)contents.blob;

  // unbind the entire buffer so that any new areas that are bound are unbound again

  VkQueue q = GetQ();

  VkMemoryRequirements mrq = {};
  ObjDisp(q)->GetBufferMemoryRequirements(Unwrap(GetDev()), buf->real.As<VkBuffer>(), &mrq);

  VkSparseMemoryBind unbind = {0, RDCMAX(mrq.size, m_CreationInfo.m_Buffer[buf->id].size),
                               VK_NULL_HANDLE, 0, 0};

  VkSparseBufferMemoryBindInfo bufBind = {buf->real.As<VkBuffer>(), 1, &unbind};

  // this semaphore separates the unbind and bind, as there isn't an ordering guarantee
  // for two adjacent batches that bind the same resource.
  VkSemaphore sem = GetNextSemaphore();

  VkBindSparseInfo bindsparse = {
      VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,
      NULL,
      0,
      NULL,    // wait semaphores
      1,
      &bufBind,
      0,
      NULL,    // image opaque
      0,
      NULL,    // image bind
      1,
      UnwrapPtr(sem),    // signal semaphores
  };

  // first unbind all
  ObjDisp(q)->QueueBindSparse(Unwrap(q), 1, &bindsparse, VK_NULL_HANDLE);

  // then make any bindings
  if(info->numBinds > 0)
  {
    bufBind.bindCount = info->numBinds;
    bufBind.pBinds = info->binds;

    // wait for unbind semaphore
    bindsparse.waitSemaphoreCount = 1;
    bindsparse.pWaitSemaphores = bindsparse.pSignalSemaphores;

    bindsparse.signalSemaphoreCount = 0;
    bindsparse.pSignalSemaphores = NULL;

    ObjDisp(q)->QueueBindSparse(Unwrap(q), 1, &bindsparse, VK_NULL_HANDLE);
  }

  // marks that the above semaphore has been used, so next time we
  // flush it will be moved back to the pool
  SubmitSemaphores();

  VkResult vkr = VK_SUCCESS;

  VkBuffer srcBuf = (VkBuffer)(uint64_t)contents.resource;

  VkCommandBuffer cmd = GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  for(uint32_t i = 0; i < info->numUniqueMems; i++)
  {
    VkDeviceMemory dstMem =
        GetResourceManager()->GetLiveHandle<VkDeviceMemory>(info->memDataOffs[i].memory);

    VkBuffer dstBuf = m_CreationInfo.m_Memory[GetResID(dstMem)].wholeMemBuf;

    VkDeviceSize size = m_CreationInfo.m_Memory[GetResID(dstMem)].size;

    // fill the whole memory from the given offset
    VkBufferCopy region = {info->memDataOffs[i].memOffs, 0, size};

    ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), Unwrap(dstBuf), 1, &region);
  }

  vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  FlushQ();

  return true;
}

bool WrappedVulkan::Apply_SparseInitialState(WrappedVkImage *im,
                                             VulkanResourceManager::InitialContentData contents)
{
  SparseImageInitState *info = (SparseImageInitState *)contents.blob;

  VkQueue q = GetQ();

  if(info->opaque)
  {
    // unbind the entire image so that any new areas that are bound are unbound again

    // VKTODOLOW not sure if this is the right size for opaque portion of partial resident
    // sparse image? how is that determined?
    VkSparseMemoryBind unbind = {0, 0, VK_NULL_HANDLE, 0, 0};

    VkMemoryRequirements mrq = {0};
    ObjDisp(q)->GetImageMemoryRequirements(Unwrap(GetDev()), im->real.As<VkImage>(), &mrq);
    unbind.size = mrq.size;

    VkSparseImageOpaqueMemoryBindInfo opaqueBind = {im->real.As<VkImage>(), 1, &unbind};

    VkSemaphore sem = GetNextSemaphore();

    VkBindSparseInfo bindsparse = {
        VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,
        NULL,
        0,
        NULL,    // wait semaphores
        0,
        NULL,    // buffer bind
        1,
        &opaqueBind,
        0,
        NULL,    // image bind
        1,
        UnwrapPtr(sem),    // signal semaphores
    };

    // first unbind all
    ObjDisp(q)->QueueBindSparse(Unwrap(q), 1, &bindsparse, VK_NULL_HANDLE);

    // then make any bindings
    if(info->opaqueCount > 0)
    {
      opaqueBind.bindCount = info->opaqueCount;
      opaqueBind.pBinds = info->opaque;

      // wait for unbind semaphore
      bindsparse.waitSemaphoreCount = 1;
      bindsparse.pWaitSemaphores = bindsparse.pSignalSemaphores;

      bindsparse.signalSemaphoreCount = 0;
      bindsparse.pSignalSemaphores = NULL;

      ObjDisp(q)->QueueBindSparse(Unwrap(q), 1, &bindsparse, VK_NULL_HANDLE);
    }

    // marks that the above semaphore has been used, so next time we
    // flush it will be moved back to the pool
    SubmitSemaphores();
  }

  {
    VkSparseImageMemoryBindInfo imgBinds[NUM_VK_IMAGE_ASPECTS];
    RDCEraseEl(imgBinds);

    VkBindSparseInfo bindsparse = {
        VK_STRUCTURE_TYPE_BIND_SPARSE_INFO,
        NULL,
        0,
        NULL,    // wait semaphores
        0,
        NULL,    // buffer bind
        0,
        NULL,    // opaque bind
        0,
        imgBinds,
        0,
        NULL,    // signal semaphores
    };

    // blat the page tables
    for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
    {
      if(!info->pageBinds[a])
        continue;

      imgBinds[bindsparse.imageBindCount].image = im->real.As<VkImage>(),
      imgBinds[bindsparse.imageBindCount].bindCount = info->pageCount[a];
      imgBinds[bindsparse.imageBindCount].pBinds = info->pageBinds[a];

      bindsparse.imageBindCount++;
    }

    ObjDisp(q)->QueueBindSparse(Unwrap(q), 1, &bindsparse, VK_NULL_HANDLE);
  }

  VkResult vkr = VK_SUCCESS;

  VkBuffer srcBuf = (VkBuffer)(uint64_t)contents.resource;

  VkCommandBuffer cmd = GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  for(uint32_t i = 0; i < info->numUniqueMems; i++)
  {
    VkDeviceMemory dstMem =
        GetResourceManager()->GetLiveHandle<VkDeviceMemory>(info->memDataOffs[i].memory);

    // since this is short lived it isn't wrapped. Note that we want
    // to cache this up front, so it will then be wrapped
    VkBuffer dstBuf = m_CreationInfo.m_Memory[GetResID(dstMem)].wholeMemBuf;
    VkDeviceSize size = m_CreationInfo.m_Memory[GetResID(dstMem)].size;

    // fill the whole memory from the given offset
    VkBufferCopy region = {info->memDataOffs[i].memOffs, 0, size};

    ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), Unwrap(dstBuf), 1, &region);
  }

  vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  return true;
}
