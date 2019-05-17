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

#include "vk_core.h"
#include "vk_debug.h"

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, MemIDOffset &el)
{
  SERIALISE_MEMBER(memory);
  SERIALISE_MEMBER(memOffs);
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, SparseBufferInitState &el)
{
  SERIALISE_MEMBER_ARRAY(binds, numBinds);
  SERIALISE_MEMBER(numBinds);
  SERIALISE_MEMBER_ARRAY(memDataOffs, numUniqueMems);
  SERIALISE_MEMBER(numUniqueMems);
  SERIALISE_MEMBER(totalSize);
}

template <>
void Deserialise(const SparseBufferInitState &el)
{
  delete[] el.binds;
  delete[] el.memDataOffs;
}

template <typename SerialiserType>
void DoSerialise(SerialiserType &ser, SparseImageInitState &el)
{
  SERIALISE_MEMBER_ARRAY(opaque, opaqueCount);
  SERIALISE_MEMBER(opaqueCount);
  SERIALISE_MEMBER(imgdim);
  SERIALISE_MEMBER(pagedim);
  for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
    SERIALISE_MEMBER_ARRAY(pages[a], pageCount[a]);
  SERIALISE_MEMBER(pageCount);
  SERIALISE_MEMBER_ARRAY(memDataOffs, numUniqueMems);
  SERIALISE_MEMBER(numUniqueMems);
  SERIALISE_MEMBER(totalSize);
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
  std::map<VkDeviceMemory, VkDeviceSize> boundMems;

  // value will be filled out later once all memories are added
  for(size_t i = 0; i < buf->record->resInfo->opaquemappings.size(); i++)
    boundMems[buf->record->resInfo->opaquemappings[i].memory] = 0;

  uint32_t numElems = (uint32_t)buf->record->resInfo->opaquemappings.size();

  VkInitialContents initContents;

  initContents.tag = VkInitialContents::Sparse;
  initContents.type = eResBuffer;

  initContents.sparseBuffer.numBinds = numElems;
  initContents.sparseBuffer.binds = new VkSparseMemoryBind[numElems];
  initContents.sparseBuffer.numUniqueMems = (uint32_t)boundMems.size();
  initContents.sparseBuffer.memDataOffs = new MemIDOffset[boundMems.size()];

  memcpy(initContents.sparseBuffer.binds, &buf->record->resInfo->opaquemappings[0],
         sizeof(VkSparseMemoryBind) * numElems);

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

    initContents.sparseBuffer.memDataOffs[memidx].memory = GetResID(it->first);
    initContents.sparseBuffer.memDataOffs[memidx].memOffs = bufInfo.size;

    // increase size
    bufInfo.size += GetRecord(it->first)->Length;
    memidx++;
  }

  initContents.sparseBuffer.totalSize = bufInfo.size;

  // since this happens during capture, we don't want to start serialising extra buffer creates, so
  // we manually create & then just wrap.
  VkBuffer dstBuf;

  VkResult vkr = VK_SUCCESS;

  vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  GetResourceManager()->WrapResource(Unwrap(d), dstBuf);

  MemoryAllocation readbackmem =
      AllocateMemoryForResource(dstBuf, MemoryScope::InitialContents, MemoryType::Readback);

  initContents.mem = readbackmem;

  vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(dstBuf), Unwrap(readbackmem.mem),
                                     readbackmem.offs);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  std::vector<VkBuffer> bufdeletes;
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

    GetResourceManager()->WrapResource(Unwrap(d), srcBuf);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(srcBuf), Unwrap(it->first), 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // copy srcbuf into its area in dstbuf
    VkBufferCopy region = {0, it->second, bufInfo.size};

    ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), Unwrap(dstBuf), 1, &region);

    bufdeletes.push_back(srcBuf);
  }

  vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // INITSTATEBATCH
  SubmitCmds();
  FlushQ();

  for(size_t i = 0; i < bufdeletes.size(); i++)
  {
    ObjDisp(d)->DestroyBuffer(Unwrap(d), Unwrap(bufdeletes[i]), NULL);
    GetResourceManager()->ReleaseWrappedResource(bufdeletes[i]);
  }

  GetResourceManager()->SetInitialContents(id, initContents);

  return true;
}

bool WrappedVulkan::Prepare_SparseInitialState(WrappedVkImage *im)
{
  ResourceId id = im->id;

  ResourceInfo *sparse = im->record->resInfo;

  // VKTODOLOW this is a bit conservative, as we save the whole memory object rather than just the
  // bound range.
  std::map<VkDeviceMemory, VkDeviceSize> boundMems;

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

  uint32_t opaqueCount = (uint32_t)sparse->opaquemappings.size();

  VkInitialContents initContents;

  initContents.tag = VkInitialContents::Sparse;
  initContents.type = eResImage;

  SparseImageInitState &sparseInit = initContents.sparseImage;

  sparseInit.opaqueCount = opaqueCount;
  sparseInit.opaque = new VkSparseMemoryBind[opaqueCount];
  sparseInit.imgdim = sparse->imgdim;
  sparseInit.pagedim = sparse->pagedim;
  sparseInit.numUniqueMems = (uint32_t)boundMems.size();
  sparseInit.memDataOffs = new MemIDOffset[boundMems.size()];

  if(opaqueCount > 0)
    memcpy(sparseInit.opaque, &sparse->opaquemappings[0], sizeof(VkSparseMemoryBind) * opaqueCount);

  for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
  {
    sparseInit.pageCount[a] = (sparse->pages[a] ? pagePerAspect : 0);

    if(sparseInit.pageCount[a] != 0)
    {
      sparseInit.pages[a] = new MemIDOffset[pagePerAspect];

      for(uint32_t i = 0; i < pagePerAspect; i++)
      {
        sparseInit.pages[a][i].memory = GetResID(sparse->pages[a][i].first);
        sparseInit.pages[a][i].memOffs = sparse->pages[a][i].second;
      }
    }
    else
    {
      sparseInit.pages[a] = NULL;
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

    sparseInit.memDataOffs[memidx].memory = GetResID(it->first);
    sparseInit.memDataOffs[memidx].memOffs = bufInfo.size;

    // increase size
    bufInfo.size += GetRecord(it->first)->Length;
    memidx++;
  }

  sparseInit.totalSize = bufInfo.size;

  // since this happens during capture, we don't want to start serialising extra buffer creates, so
  // we manually create & then just wrap.
  VkBuffer dstBuf;

  VkResult vkr = VK_SUCCESS;

  vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  GetResourceManager()->WrapResource(Unwrap(d), dstBuf);

  MemoryAllocation readbackmem =
      AllocateMemoryForResource(dstBuf, MemoryScope::InitialContents, MemoryType::Readback);

  initContents.mem = readbackmem;

  vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(dstBuf), Unwrap(readbackmem.mem),
                                     readbackmem.offs);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  std::vector<VkBuffer> bufdeletes;
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

    GetResourceManager()->WrapResource(Unwrap(d), srcBuf);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(srcBuf), Unwrap(it->first), 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // copy srcbuf into its area in dstbuf
    VkBufferCopy region = {0, it->second, bufInfo.size};

    ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), Unwrap(dstBuf), 1, &region);

    bufdeletes.push_back(srcBuf);
  }

  vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // INITSTATEBATCH
  SubmitCmds();
  FlushQ();

  for(size_t i = 0; i < bufdeletes.size(); i++)
  {
    ObjDisp(d)->DestroyBuffer(Unwrap(d), Unwrap(bufdeletes[i]), NULL);
    GetResourceManager()->ReleaseWrappedResource(bufdeletes[i]);
  }

  GetResourceManager()->SetInitialContents(id, initContents);

  return true;
}

uint64_t WrappedVulkan::GetSize_SparseInitialState(ResourceId id, const VkInitialContents &initial)
{
  if(initial.type == eResBuffer)
  {
    const SparseBufferInitState &info = initial.sparseBuffer;

    // some bytes just to cover overheads etc.
    uint64_t ret = 128;

    // the list of memory objects bound
    ret += 8 + sizeof(VkSparseMemoryBind) * info.numBinds;

    // the list of memory regions to copy
    ret += 8 + sizeof(MemIDOffset) * info.numUniqueMems;

    // the actual data
    ret += uint64_t(info.totalSize + WriteSerialiser::GetChunkAlignment());

    return ret;
  }
  else if(initial.type == eResImage)
  {
    const SparseImageInitState &info = initial.sparseImage;

    // some bytes just to cover overheads etc.
    uint64_t ret = 128;

    // the meta-data structure
    ret += sizeof(SparseImageInitState);

    // the list of memory objects bound
    ret += sizeof(VkSparseMemoryBind) * info.opaqueCount;

    // the page tables
    for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
      ret += 8 + sizeof(MemIDOffset) * info.pageCount[a];

    // the list of memory regions to copy
    ret += sizeof(MemIDOffset) * info.numUniqueMems;

    // the actual data
    ret += uint64_t(info.totalSize + WriteSerialiser::GetChunkAlignment());

    return ret;
  }

  RDCERR("Unhandled resource type %s", ToStr(initial.type).c_str());
  return 128;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_SparseBufferInitialState(SerialiserType &ser, ResourceId id,
                                                       const VkInitialContents *contents)
{
  VkDevice d = !IsStructuredExporting(m_State) ? GetDev() : VK_NULL_HANDLE;
  VkResult vkr = VK_SUCCESS;

  SERIALISE_ELEMENT_LOCAL(SparseState, contents->sparseBuffer);

  MemoryAllocation mappedMem;
  byte *Contents = NULL;
  uint64_t ContentsSize = (uint64_t)SparseState.totalSize;

  // Serialise this separately so that it can be used on reading to prepare the upload memory
  SERIALISE_ELEMENT(ContentsSize);

  // the memory/buffer that we allocated on read, to upload the initial contents.
  MemoryAllocation uploadMemory;
  VkBuffer uploadBuf = VK_NULL_HANDLE;

  // during writing, we already have the memory copied off - we just need to map it.
  if(ser.IsWriting())
  {
    // the memory was created not wrapped.
    mappedMem = contents->mem;
    vkr = ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mappedMem.mem), mappedMem.offs, mappedMem.size, 0,
                                (void **)&Contents);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // invalidate the cpu cache for this memory range to avoid reading stale data
    VkMappedMemoryRange range = {
        VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
        NULL,
        Unwrap(mappedMem.mem),
        mappedMem.offs,
        mappedMem.size,
    };

    vkr = ObjDisp(d)->InvalidateMappedMemoryRanges(Unwrap(d), 1, &range);
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

    uploadMemory =
        AllocateMemoryForResource(uploadBuf, MemoryScope::InitialContents, MemoryType::Upload);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(uploadBuf), Unwrap(uploadMemory.mem),
                                       uploadMemory.offs);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    mappedMem = uploadMemory;

    ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(uploadMemory.mem), uploadMemory.offs, uploadMemory.size,
                          0, (void **)&Contents);
  }

  // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
  // directly into upload memory
  ser.Serialise("Contents"_lit, Contents, ContentsSize, SerialiserFlags::NoFlags);

  // unmap the resource we mapped before - we need to do this on read and on write.
  if(!IsStructuredExporting(m_State) && mappedMem.mem != VK_NULL_HANDLE)
  {
    if(IsReplayingAndReading())
    {
      // first ensure we flush the writes from the cpu to gpu memory
      VkMappedMemoryRange range = {
          VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
          NULL,
          Unwrap(mappedMem.mem),
          mappedMem.offs,
          mappedMem.size,
      };

      vkr = ObjDisp(d)->FlushMappedMemoryRanges(Unwrap(d), 1, &range);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mappedMem.mem));
  }

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkInitialContents initContents;
    initContents.type = eResBuffer;
    initContents.buf = uploadBuf;
    initContents.mem = uploadMemory;
    initContents.tag = VkInitialContents::Sparse;
    initContents.sparseBuffer = SparseState;

    // we steal the serialised arrays here by resetting the struct, then the serialisation won't
    // deallocate them. VkInitialContents::Free() will deallocate them in the same way.
    SparseState = SparseBufferInitState();

    GetResourceManager()->SetInitialContents(id, initContents);
  }

  return true;
}

template <typename SerialiserType>
bool WrappedVulkan::Serialise_SparseImageInitialState(SerialiserType &ser, ResourceId id,
                                                      const VkInitialContents *contents)
{
  VkDevice d = !IsStructuredExporting(m_State) ? GetDev() : VK_NULL_HANDLE;
  VkResult vkr = VK_SUCCESS;

  SERIALISE_ELEMENT_LOCAL(SparseState, contents->sparseImage);

  MemoryAllocation mappedMem;
  byte *Contents = NULL;
  uint64_t ContentsSize = (uint64_t)SparseState.totalSize;

  // Serialise this separately so that it can be used on reading to prepare the upload memory
  SERIALISE_ELEMENT(ContentsSize);

  // the memory/buffer that we allocated on read, to upload the initial contents.
  MemoryAllocation uploadMemory;
  VkBuffer uploadBuf = VK_NULL_HANDLE;

  // during writing, we already have the memory copied off - we just need to map it.
  if(ser.IsWriting())
  {
    mappedMem = contents->mem;
    vkr = ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mappedMem.mem), mappedMem.offs, mappedMem.size, 0,
                                (void **)&Contents);
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

    uploadMemory =
        AllocateMemoryForResource(uploadBuf, MemoryScope::InitialContents, MemoryType::Upload);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(uploadBuf), Unwrap(uploadMemory.mem),
                                       uploadMemory.offs);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    mappedMem = uploadMemory;

    ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(uploadMemory.mem), uploadMemory.offs, uploadMemory.size,
                          0, (void **)&Contents);
  }

  // not using SERIALISE_ELEMENT_ARRAY so we can deliberately avoid allocation - we serialise
  // directly into upload memory
  ser.Serialise("Contents"_lit, Contents, ContentsSize, SerialiserFlags::NoFlags);

  // unmap the resource we mapped before - we need to do this on read and on write.
  if(!IsStructuredExporting(m_State) && mappedMem.mem != VK_NULL_HANDLE)
    ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mappedMem.mem));

  SERIALISE_CHECK_READ_ERRORS();

  if(IsReplayingAndReading())
  {
    VkInitialContents initContents;
    initContents.type = eResImage;
    initContents.buf = uploadBuf;
    initContents.mem = uploadMemory;
    initContents.tag = VkInitialContents::Sparse;
    initContents.sparseImage = SparseState;

    for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
    {
      if(SparseState.pageCount[a] == 0)
      {
        initContents.sparseImage.pageBinds[a] = NULL;
      }
      else
      {
        initContents.sparseImage.pageBinds[a] = new VkSparseImageMemoryBind[SparseState.pageCount[a]];

        uint32_t i = 0;

        for(uint32_t z = 0; z < SparseState.imgdim.depth; z++)
        {
          for(uint32_t y = 0; y < SparseState.imgdim.height; y++)
          {
            for(uint32_t x = 0; x < SparseState.imgdim.width; x++)
            {
              VkSparseImageMemoryBind &p = initContents.sparseImage.pageBinds[a][i];

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

    // delete and free the pages array, we no longer need it.
    for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
      SAFE_DELETE_ARRAY(SparseState.pages[a]);

    // we steal the serialised arrays here by resetting the struct, then the serialisation won't
    // deallocate them. VkInitialContents::Free() will deallocate them in the same way.
    SparseState = SparseImageInitState();

    GetResourceManager()->SetInitialContents(id, initContents);
  }

  return true;
}

template bool WrappedVulkan::Serialise_SparseBufferInitialState(ReadSerialiser &ser, ResourceId id,
                                                                const VkInitialContents *contents);
template bool WrappedVulkan::Serialise_SparseBufferInitialState(WriteSerialiser &ser, ResourceId id,
                                                                const VkInitialContents *contents);
template bool WrappedVulkan::Serialise_SparseImageInitialState(ReadSerialiser &ser, ResourceId id,
                                                               const VkInitialContents *contents);
template bool WrappedVulkan::Serialise_SparseImageInitialState(WriteSerialiser &ser, ResourceId id,
                                                               const VkInitialContents *contents);

bool WrappedVulkan::Apply_SparseInitialState(WrappedVkBuffer *buf, const VkInitialContents &contents)
{
  const SparseBufferInitState &info = contents.sparseBuffer;

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
  if(info.numBinds > 0)
  {
    bufBind.bindCount = info.numBinds;
    bufBind.pBinds = info.binds;

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

  VkBuffer srcBuf = contents.buf;

  VkCommandBuffer cmd = GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  for(uint32_t i = 0; i < info.numUniqueMems; i++)
  {
    VkDeviceMemory dstMem =
        GetResourceManager()->GetLiveHandle<VkDeviceMemory>(info.memDataOffs[i].memory);

    ResourceId id = GetResID(dstMem);

    VkBuffer dstBuf = m_CreationInfo.m_Memory[id].wholeMemBuf;

    VkDeviceSize size = m_CreationInfo.m_Memory[id].size;

    // fill the whole memory from the given offset
    VkBufferCopy region = {info.memDataOffs[i].memOffs, 0, size};

    if(dstBuf != VK_NULL_HANDLE)
      ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), Unwrap(dstBuf), 1, &region);
    else
      RDCERR("Whole memory buffer not present for %llu", id);
  }

  vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  FlushQ();

  return true;
}

bool WrappedVulkan::Apply_SparseInitialState(WrappedVkImage *im, const VkInitialContents &contents)
{
  const SparseImageInitState &info = contents.sparseImage;

  VkQueue q = GetQ();

  if(info.opaque)
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
    if(info.opaqueCount > 0)
    {
      opaqueBind.bindCount = info.opaqueCount;
      opaqueBind.pBinds = info.opaque;

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
      if(!info.pageBinds[a])
        continue;

      imgBinds[bindsparse.imageBindCount].image = im->real.As<VkImage>(),
      imgBinds[bindsparse.imageBindCount].bindCount = info.pageCount[a];
      imgBinds[bindsparse.imageBindCount].pBinds = info.pageBinds[a];

      bindsparse.imageBindCount++;
    }

    ObjDisp(q)->QueueBindSparse(Unwrap(q), 1, &bindsparse, VK_NULL_HANDLE);
  }

  VkResult vkr = VK_SUCCESS;

  VkBuffer srcBuf = contents.buf;

  VkCommandBuffer cmd = GetNextCmd();

  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  for(uint32_t i = 0; i < info.numUniqueMems; i++)
  {
    VkDeviceMemory dstMem =
        GetResourceManager()->GetLiveHandle<VkDeviceMemory>(info.memDataOffs[i].memory);

    ResourceId id = GetResID(dstMem);

    // since this is short lived it isn't wrapped. Note that we want
    // to cache this up front, so it will then be wrapped
    VkBuffer dstBuf = m_CreationInfo.m_Memory[id].wholeMemBuf;
    VkDeviceSize size = m_CreationInfo.m_Memory[id].size;

    // fill the whole memory from the given offset
    VkBufferCopy region = {info.memDataOffs[i].memOffs, 0, size};

    if(dstBuf != VK_NULL_HANDLE)
      ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), Unwrap(dstBuf), 1, &region);
    else
      RDCERR("Whole memory buffer not present for %llu", id);
  }

  vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  return true;
}
