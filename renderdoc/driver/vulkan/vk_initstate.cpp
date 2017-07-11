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

// VKTODOLOW for depth-stencil images we are only save/restoring the depth, not the stencil

// VKTODOLOW there's a lot of duplicated code in this file for creating a buffer to do
// a memory copy and saving to disk.
// VKTODOLOW SerialiseComplexArray not having the ability to serialise into an in-memory
// array means some redundant copies.
// VKTODOLOW The code pattern for creating a few contiguous arrays all in one
// AllocAlignedBuffer for the initial contents buffer is ugly.

// VKTODOLOW in general we do a lot of "create buffer, use it, flush/sync then destroy".
// I don't know what the exact cost is, but it would be nice to batch up the buffers/etc
// used across init state use, and only do a single flush. Also we could then get some
// nice command buffer reuse (although need to be careful we don't create too large a
// command buffer that stalls the GPU).
// See INITSTATEBATCH

struct MemIDOffset
{
  ResourceId memId;
  VkDeviceSize memOffs;
};

template <>
void Serialiser::Serialise(const char *name, MemIDOffset &el)
{
  Serialise("memId", el.memId);
  Serialise("memOffs", el.memOffs);
}

struct SparseBufferInitState
{
  uint32_t numBinds;
  VkSparseMemoryBind *binds;

  uint32_t numUniqueMems;
  MemIDOffset *memDataOffs;

  VkDeviceSize totalSize;
};

struct SparseImageInitState
{
  uint32_t opaqueCount;
  VkSparseMemoryBind *opaque;

  VkExtent3D imgdim;    // in pages
  VkExtent3D pagedim;
  uint32_t pageCount[NUM_VK_IMAGE_ASPECTS];

  // available on capture - filled out in Prepare_SparseInitialState and serialised to disk
  MemIDOffset *pages[NUM_VK_IMAGE_ASPECTS];

  // available on replay - filled out in the READING path of Serialise_SparseInitialState
  VkSparseImageMemoryBind *pageBinds[NUM_VK_IMAGE_ASPECTS];

  uint32_t numUniqueMems;
  MemIDOffset *memDataOffs;

  VkDeviceSize totalSize;
};

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

  SparseBufferInitState *info = (SparseBufferInitState *)Serialiser::AllocAlignedBuffer(
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

    memDataOffs[memidx].memId = GetResID(it->first);
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
      id, VulkanResourceManager::InitialContentData(GetWrapped(readbackmem), 0, (byte *)info));

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

  byte *blob = Serialiser::AllocAlignedBuffer(
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
        state->pages[a][i].memId = GetResID(sparse->pages[a][i].first);
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

    memDataOffs[memidx].memId = GetResID(it->first);
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
      id, VulkanResourceManager::InitialContentData(GetWrapped(readbackmem), 0, (byte *)blob));

  return true;
}

bool WrappedVulkan::Serialise_SparseBufferInitialState(
    ResourceId id, VulkanResourceManager::InitialContentData contents)
{
  if(m_State >= WRITING)
  {
    SparseBufferInitState *info = (SparseBufferInitState *)contents.blob;

    m_pSerialiser->Serialise("numBinds", info->numBinds);
    m_pSerialiser->Serialise("numUniqueMems", info->numUniqueMems);

    if(info->numBinds > 0)
      m_pSerialiser->SerialiseComplexArray("binds", info->binds, info->numBinds);

    if(info->numUniqueMems > 0)
      m_pSerialiser->SerialiseComplexArray("mems", info->memDataOffs, info->numUniqueMems);

    VkDevice d = GetDev();

    byte *ptr = NULL;
    ObjDisp(d)->MapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(contents.resource), 0, VK_WHOLE_SIZE,
                          0, (void **)&ptr);

    size_t dataSize = (size_t)info->totalSize;

    m_pSerialiser->Serialise("totalSize", info->totalSize);
    m_pSerialiser->SerialiseBuffer("data", ptr, dataSize);

    ObjDisp(d)->UnmapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(contents.resource));
  }
  else
  {
    uint32_t numBinds = 0;
    uint32_t numUniqueMems = 0;

    m_pSerialiser->Serialise("numBinds", numBinds);
    m_pSerialiser->Serialise("numUniqueMems", numUniqueMems);

    SparseBufferInitState *info = (SparseBufferInitState *)Serialiser::AllocAlignedBuffer(
        sizeof(SparseBufferInitState) + sizeof(VkSparseMemoryBind) * numBinds +
        sizeof(MemIDOffset) * numUniqueMems);

    VkSparseMemoryBind *binds = (VkSparseMemoryBind *)(info + 1);
    MemIDOffset *memDataOffs = (MemIDOffset *)(binds + numBinds);

    info->numBinds = numBinds;
    info->numUniqueMems = numUniqueMems;
    info->binds = binds;
    info->memDataOffs = memDataOffs;

    if(info->numBinds > 0)
    {
      VkSparseMemoryBind *b = NULL;
      m_pSerialiser->SerialiseComplexArray("binds", b, numBinds);
      memcpy(info->binds, b, sizeof(VkSparseMemoryBind) * numBinds);
      delete[] b;
    }
    else
    {
      info->binds = NULL;
    }

    if(info->numUniqueMems > 0)
    {
      MemIDOffset *m = NULL;
      m_pSerialiser->SerialiseComplexArray("mems", m, numUniqueMems);
      memcpy(info->memDataOffs, m, sizeof(MemIDOffset) * numUniqueMems);
      delete[] m;
    }
    else
    {
      info->memDataOffs = NULL;
    }

    m_pSerialiser->Serialise("totalSize", info->totalSize);

    VkResult vkr = VK_SUCCESS;

    VkDevice d = GetDev();

    VkDeviceMemory mem = VK_NULL_HANDLE;

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        info->totalSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE,
        0,
        NULL,
    };

    VkBuffer buf;

    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &buf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), buf);

    VkMemoryRequirements mrq = {0};

    ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), Unwrap(buf), &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        GetUploadMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &mem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), mem);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(buf), Unwrap(mem), 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    byte *ptr = NULL;
    ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mem), 0, VK_WHOLE_SIZE, 0, (void **)&ptr);

    size_t dummy = 0;
    m_pSerialiser->SerialiseBuffer("data", ptr, dummy);

    ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mem));

    m_CleanupMems.push_back(mem);

    GetResourceManager()->SetInitialContents(
        id, VulkanResourceManager::InitialContentData(GetWrapped(buf), 0, (byte *)info));
  }

  return true;
}

bool WrappedVulkan::Serialise_SparseImageInitialState(ResourceId id,
                                                      VulkanResourceManager::InitialContentData contents)
{
  if(m_State >= WRITING)
  {
    SparseImageInitState *state = (SparseImageInitState *)contents.blob;

    uint32_t totalPageCount = 0;
    for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
      totalPageCount += state->pageCount[a];

    m_pSerialiser->Serialise("opaqueCount", state->opaqueCount);
    m_pSerialiser->Serialise("totalPageCount", totalPageCount);
    m_pSerialiser->Serialise("imgdim", state->imgdim);
    m_pSerialiser->Serialise("pagedim", state->pagedim);
    m_pSerialiser->Serialise("numUniqueMems", state->numUniqueMems);

    if(state->opaqueCount > 0)
      m_pSerialiser->SerialiseComplexArray("opaque", state->opaque, state->opaqueCount);

    if(totalPageCount > 0)
    {
      for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
      {
        m_pSerialiser->Serialise("aspectPageCount", state->pageCount[a]);

        if(state->pageCount[a] > 0)
          m_pSerialiser->SerialiseComplexArray("pages", state->pages[a], state->pageCount[a]);
      }
    }

    if(state->numUniqueMems > 0)
      m_pSerialiser->SerialiseComplexArray("mems", state->memDataOffs, state->numUniqueMems);

    VkDevice d = GetDev();

    byte *ptr = NULL;
    ObjDisp(d)->MapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(contents.resource), 0, VK_WHOLE_SIZE,
                          0, (void **)&ptr);

    size_t dataSize = (size_t)state->totalSize;

    m_pSerialiser->Serialise("totalSize", state->totalSize);
    m_pSerialiser->SerialiseBuffer("data", ptr, dataSize);

    ObjDisp(d)->UnmapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(contents.resource));
  }
  else
  {
    uint32_t opaqueCount = 0;
    uint32_t pageCount = 0;
    uint32_t numUniqueMems = 0;
    VkExtent3D imgdim = {};
    VkExtent3D pagedim = {};

    m_pSerialiser->Serialise("opaqueCount", opaqueCount);
    m_pSerialiser->Serialise("pageCount", pageCount);
    m_pSerialiser->Serialise("imgdim", imgdim);
    m_pSerialiser->Serialise("pagedim", pagedim);
    m_pSerialiser->Serialise("numUniqueMems", numUniqueMems);

    byte *blob = Serialiser::AllocAlignedBuffer(
        sizeof(SparseImageInitState) + sizeof(VkSparseMemoryBind) * opaqueCount +
        sizeof(VkSparseImageMemoryBind) * pageCount + sizeof(MemIDOffset) * numUniqueMems);

    SparseImageInitState *state = (SparseImageInitState *)blob;
    VkSparseMemoryBind *opaque = (VkSparseMemoryBind *)(state + 1);
    VkSparseImageMemoryBind *pageBinds = (VkSparseImageMemoryBind *)(opaque + opaqueCount);
    MemIDOffset *memDataOffs = (MemIDOffset *)(pageBinds + pageCount);

    RDCEraseEl(state->pageBinds);

    state->opaqueCount = opaqueCount;
    state->opaque = opaque;
    state->imgdim = imgdim;
    state->pagedim = pagedim;
    state->numUniqueMems = numUniqueMems;
    state->memDataOffs = memDataOffs;

    if(opaqueCount > 0)
    {
      VkSparseMemoryBind *o = NULL;
      m_pSerialiser->SerialiseComplexArray("opaque", o, opaqueCount);
      memcpy(opaque, o, sizeof(VkSparseMemoryBind) * opaqueCount);
      delete[] o;
    }
    else
    {
      state->opaque = NULL;
    }

    if(pageCount > 0)
    {
      for(uint32_t a = 0; a < NUM_VK_IMAGE_ASPECTS; a++)
      {
        m_pSerialiser->Serialise("aspectPageCount", state->pageCount[a]);

        if(state->pageCount[a] == 0)
        {
          state->pageBinds[a] = NULL;
        }
        else
        {
          state->pageBinds[a] = pageBinds;
          pageBinds += state->pageCount[a];

          MemIDOffset *pages = NULL;
          m_pSerialiser->SerialiseComplexArray("pages", pages, state->pageCount[a]);

          uint32_t i = 0;

          for(uint32_t z = 0; z < imgdim.depth; z++)
          {
            for(uint32_t y = 0; y < imgdim.height; y++)
            {
              for(uint32_t x = 0; x < imgdim.width; x++)
              {
                VkSparseImageMemoryBind &p = state->pageBinds[a][i];

                p.memory =
                    Unwrap(GetResourceManager()->GetLiveHandle<VkDeviceMemory>(pages[i].memId));
                p.memoryOffset = pages[i].memOffs;
                p.extent = pagedim;
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

          delete[] pages;
        }
      }
    }

    if(state->numUniqueMems > 0)
    {
      MemIDOffset *m = NULL;
      m_pSerialiser->SerialiseComplexArray("opaque", m, numUniqueMems);
      memcpy(state->memDataOffs, m, sizeof(MemIDOffset) * numUniqueMems);
      delete[] m;
    }
    else
    {
      state->memDataOffs = NULL;
    }

    m_pSerialiser->Serialise("totalSize", state->totalSize);

    VkResult vkr = VK_SUCCESS;

    VkDevice d = GetDev();

    VkDeviceMemory mem = VK_NULL_HANDLE;

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        state->totalSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    VkBuffer buf;

    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &buf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), buf);

    VkMemoryRequirements mrq = {0};

    ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), Unwrap(buf), &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        GetUploadMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &mem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), mem);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(buf), Unwrap(mem), 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    byte *ptr = NULL;
    ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mem), 0, VK_WHOLE_SIZE, 0, (void **)&ptr);

    size_t dummy = 0;
    m_pSerialiser->SerialiseBuffer("data", ptr, dummy);

    ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mem));

    m_CleanupMems.push_back(mem);

    GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(
                                                     GetWrapped(buf), eInitialContents_Sparse, blob));
  }

  return true;
}

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
        GetResourceManager()->GetLiveHandle<VkDeviceMemory>(info->memDataOffs[i].memId);

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
        GetResourceManager()->GetLiveHandle<VkDeviceMemory>(info->memDataOffs[i].memId);

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

bool WrappedVulkan::Prepare_InitialState(WrappedVkRes *res)
{
  ResourceId id = GetResourceManager()->GetID(res);

  VkResourceType type = IdentifyTypeByPtr(res);

  if(type == eResDescriptorSet)
  {
    VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
    RDCASSERT(record->descInfo && record->descInfo->layout);
    const DescSetLayout &layout = *record->descInfo->layout;

    uint32_t numElems = 0;
    for(size_t i = 0; i < layout.bindings.size(); i++)
      numElems += layout.bindings[i].descriptorCount;

    DescriptorSetSlot *info =
        (DescriptorSetSlot *)Serialiser::AllocAlignedBuffer(sizeof(DescriptorSetSlot) * numElems);
    RDCEraseMem(info, sizeof(DescriptorSetSlot) * numElems);

    uint32_t e = 0;
    for(size_t i = 0; i < layout.bindings.size(); i++)
      for(uint32_t b = 0; b < layout.bindings[i].descriptorCount; b++)
        info[e++] = record->descInfo->descBindings[i][b];

    GetResourceManager()->SetInitialContents(
        id, VulkanResourceManager::InitialContentData(NULL, 0, (byte *)info));
    return true;
  }
  else if(type == eResBuffer)
  {
    WrappedVkBuffer *buffer = (WrappedVkBuffer *)res;

    // buffers are only dirty if they are sparse
    RDCASSERT(buffer->record->sparseInfo);

    return Prepare_SparseInitialState(buffer);
  }
  else if(type == eResImage)
  {
    VkResult vkr = VK_SUCCESS;

    WrappedVkImage *im = (WrappedVkImage *)res;

    if(im->record->sparseInfo)
    {
      // if the image is sparse we have to do a different kind of initial state prepare,
      // to serialise out the page mapping. The fetching of memory is also different
      return Prepare_SparseInitialState((WrappedVkImage *)res);
    }

    VkDevice d = GetDev();
    // INITSTATEBATCH
    VkCommandBuffer cmd = GetNextCmd();

    ImageLayouts *layout = NULL;
    {
      SCOPED_LOCK(m_ImageLayoutsLock);
      layout = &m_ImageLayouts[im->id];
    }

    // must ensure offset remains valid. Must be multiple of block size, or 4, depending on format
    VkDeviceSize bufAlignment = 4;
    if(IsBlockFormat(layout->format))
      bufAlignment = (VkDeviceSize)GetByteSize(1, 1, 1, layout->format, 0);

    VkDeviceMemory readbackmem = VK_NULL_HANDLE;

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        0,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    VkImage arrayIm = VK_NULL_HANDLE;
    VkDeviceMemory arrayMem = VK_NULL_HANDLE;

    VkImage realim = im->real.As<VkImage>();
    int numLayers = layout->layerCount;

    if(layout->sampleCount > 1)
    {
      // first decompose to array
      numLayers *= layout->sampleCount;

      VkImageCreateInfo arrayInfo = {
          VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, NULL, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
          VK_IMAGE_TYPE_2D, layout->format, layout->extent, (uint32_t)layout->levelCount,
          (uint32_t)numLayers, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
          VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
              VK_IMAGE_USAGE_TRANSFER_DST_BIT,
          VK_SHARING_MODE_EXCLUSIVE, 0, NULL, VK_IMAGE_LAYOUT_UNDEFINED,
      };

      if(IsDepthOrStencilFormat(layout->format))
        arrayInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      else
        arrayInfo.usage |= VK_IMAGE_USAGE_STORAGE_BIT;

      vkr = ObjDisp(d)->CreateImage(Unwrap(d), &arrayInfo, NULL, &arrayIm);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkMemoryRequirements mrq = {0};

      ObjDisp(d)->GetImageMemoryRequirements(Unwrap(d), arrayIm, &mrq);

      VkMemoryAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
          GetGPULocalMemoryIndex(mrq.memoryTypeBits),
      };

      vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &arrayMem);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      vkr = ObjDisp(d)->BindImageMemory(Unwrap(d), arrayIm, arrayMem, 0);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);
    }

    VkFormat sizeFormat = GetDepthOnlyFormat(layout->format);

    for(int a = 0; a < numLayers; a++)
    {
      for(int m = 0; m < layout->levelCount; m++)
      {
        bufInfo.size = AlignUp(bufInfo.size, bufAlignment);

        bufInfo.size += GetByteSize(layout->extent.width, layout->extent.height,
                                    layout->extent.depth, sizeFormat, m);

        if(sizeFormat != layout->format)
        {
          // if there's stencil and depth, allocate space for stencil
          bufInfo.size = AlignUp(bufInfo.size, bufAlignment);

          bufInfo.size += GetByteSize(layout->extent.width, layout->extent.height,
                                      layout->extent.depth, VK_FORMAT_S8_UINT, m);
        }
      }
    }

    // since this is very short lived, it is not wrapped
    VkBuffer dstBuf;

    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};

    ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), dstBuf, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
        GetReadbackMemoryIndex(mrq.memoryTypeBits),
    };

    vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &readbackmem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), readbackmem);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(readbackmem), 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
    if(IsStencilOnlyFormat(layout->format))
      aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
    else if(IsDepthOrStencilFormat(layout->format))
      aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

    VkImageMemoryBarrier srcimBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        0,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        realim,
        {aspectFlags, 0, (uint32_t)layout->levelCount, 0, (uint32_t)numLayers}};

    if(aspectFlags == VK_IMAGE_ASPECT_DEPTH_BIT && !IsDepthOnlyFormat(layout->format))
      srcimBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    // update the real image layout into transfer-source
    srcimBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    if(arrayIm != VK_NULL_HANDLE)
      srcimBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // ensure all previous writes have completed
    srcimBarrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
    // before we go reading
    srcimBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT;

    for(size_t si = 0; si < layout->subresourceStates.size(); si++)
    {
      srcimBarrier.subresourceRange = layout->subresourceStates[si].subresourceRange;
      srcimBarrier.oldLayout = layout->subresourceStates[si].newLayout;
      DoPipelineBarrier(cmd, 1, &srcimBarrier);
    }

    if(arrayIm != VK_NULL_HANDLE)
    {
      VkImageMemoryBarrier arrayimBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                             NULL,
                                             0,
                                             0,
                                             VK_IMAGE_LAYOUT_UNDEFINED,
                                             VK_IMAGE_LAYOUT_GENERAL,
                                             VK_QUEUE_FAMILY_IGNORED,
                                             VK_QUEUE_FAMILY_IGNORED,
                                             arrayIm,
                                             {srcimBarrier.subresourceRange.aspectMask, 0,
                                              VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

      DoPipelineBarrier(cmd, 1, &arrayimBarrier);

      vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetDebugManager()->CopyTex2DMSToArray(arrayIm, realim, layout->extent, layout->layerCount,
                                            layout->sampleCount, layout->format);

      cmd = GetNextCmd();

      vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      arrayimBarrier.srcAccessMask =
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      arrayimBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      arrayimBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
      arrayimBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

      DoPipelineBarrier(cmd, 1, &arrayimBarrier);

      realim = arrayIm;
    }

    VkDeviceSize bufOffset = 0;

    // loop over every slice/mip, copying it to the appropriate point in the buffer
    for(int a = 0; a < numLayers; a++)
    {
      VkExtent3D extent = layout->extent;

      for(int m = 0; m < layout->levelCount; m++)
      {
        VkBufferImageCopy region = {
            0,
            0,
            0,
            {aspectFlags, (uint32_t)m, (uint32_t)a, 1},
            {
                0, 0, 0,
            },
            extent,
        };

        bufOffset = AlignUp(bufOffset, bufAlignment);

        region.bufferOffset = bufOffset;

        bufOffset += GetByteSize(layout->extent.width, layout->extent.height, layout->extent.depth,
                                 sizeFormat, m);

        ObjDisp(d)->CmdCopyImageToBuffer(Unwrap(cmd), realim, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                         dstBuf, 1, &region);

        if(sizeFormat != layout->format)
        {
          // if we removed stencil from the format, copy that separately now.
          bufOffset = AlignUp(bufOffset, bufAlignment);

          region.bufferOffset = bufOffset;
          region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

          bufOffset += GetByteSize(layout->extent.width, layout->extent.height,
                                   layout->extent.depth, VK_FORMAT_S8_UINT, m);

          ObjDisp(d)->CmdCopyImageToBuffer(
              Unwrap(cmd), realim, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstBuf, 1, &region);
        }

        // update the extent for the next mip
        extent.width = RDCMAX(extent.width >> 1, 1U);
        extent.height = RDCMAX(extent.height >> 1, 1U);
        extent.depth = RDCMAX(extent.depth >> 1, 1U);
      }
    }

    RDCASSERTMSG("buffer wasn't sized sufficiently!", bufOffset <= bufInfo.size, bufOffset,
                 mrq.size, layout->extent, layout->format, numLayers, layout->levelCount);

    // transfer back to whatever it was
    srcimBarrier.oldLayout = srcimBarrier.newLayout;

    srcimBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    srcimBarrier.dstAccessMask = 0;

    for(size_t si = 0; si < layout->subresourceStates.size(); si++)
    {
      srcimBarrier.subresourceRange = layout->subresourceStates[si].subresourceRange;
      srcimBarrier.newLayout = layout->subresourceStates[si].newLayout;
      srcimBarrier.dstAccessMask = MakeAccessMask(srcimBarrier.newLayout);
      DoPipelineBarrier(cmd, 1, &srcimBarrier);
    }

    vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // INITSTATEBATCH
    SubmitCmds();
    FlushQ();

    ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf, NULL);

    if(arrayIm != VK_NULL_HANDLE)
    {
      ObjDisp(d)->DestroyImage(Unwrap(d), arrayIm, NULL);
      ObjDisp(d)->FreeMemory(Unwrap(d), arrayMem, NULL);
    }

    GetResourceManager()->SetInitialContents(
        id, VulkanResourceManager::InitialContentData(GetWrapped(readbackmem), (uint32_t)mrq.size,
                                                      NULL));

    return true;
  }
  else if(type == eResDeviceMemory)
  {
    VkResult vkr = VK_SUCCESS;

    VkDevice d = GetDev();
    // INITSTATEBATCH
    VkCommandBuffer cmd = GetNextCmd();

    VkResourceRecord *record = GetResourceManager()->GetResourceRecord(id);
    VkDeviceSize dataoffs = 0;
    VkDeviceMemory datamem = ToHandle<VkDeviceMemory>(res);
    VkDeviceSize datasize = record->Length;

    RDCASSERT(datamem != VK_NULL_HANDLE);

    RDCASSERT(record->Length > 0);
    VkDeviceSize memsize = record->Length;

    VkDeviceMemory readbackmem = VK_NULL_HANDLE;

    VkBufferCreateInfo bufInfo = {
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        NULL,
        0,
        0,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    // since these are very short lived, they are not wrapped
    VkBuffer srcBuf, dstBuf;

    // dstBuf is just over the allocated memory, so only the image's size
    bufInfo.size = datasize;
    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &dstBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // srcBuf spans the entire memory, then we copy out the sub-region we're interested in
    bufInfo.size = memsize;
    vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &srcBuf);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkMemoryRequirements mrq = {0};

    ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), srcBuf, &mrq);

    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, datasize,
        GetReadbackMemoryIndex(mrq.memoryTypeBits),
    };

    allocInfo.allocationSize = AlignUp(allocInfo.allocationSize, mrq.alignment);

    vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &readbackmem);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    GetResourceManager()->WrapResource(Unwrap(d), readbackmem);

    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), srcBuf, datamem, 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);
    vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), dstBuf, Unwrap(readbackmem), 0);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkr = ObjDisp(d)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkBufferCopy region = {dataoffs, 0, datasize};

    ObjDisp(d)->CmdCopyBuffer(Unwrap(cmd), srcBuf, dstBuf, 1, &region);

    vkr = ObjDisp(d)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    // INITSTATEBATCH
    SubmitCmds();
    FlushQ();

    ObjDisp(d)->DestroyBuffer(Unwrap(d), srcBuf, NULL);
    ObjDisp(d)->DestroyBuffer(Unwrap(d), dstBuf, NULL);

    GetResourceManager()->SetInitialContents(
        id, VulkanResourceManager::InitialContentData(GetWrapped(readbackmem), (uint32_t)datasize,
                                                      NULL));

    return true;
  }
  else
  {
    RDCERR("Unhandled resource type %d", type);
  }

  return false;
}

// second parameter isn't used, as we might be serialising init state for a deleted resource
bool WrappedVulkan::Serialise_InitialState(ResourceId resid, WrappedVkRes *)
{
  // use same serialiser as resource manager
  Serialiser *localSerialiser = GetMainSerialiser();

  VkResourceRecord *record = NULL;
  if(m_State >= WRITING)
    record = GetResourceManager()->GetResourceRecord(resid);

  // use the record's resource, not the one passed in, because the passed in one
  // might be null if it was deleted
  SERIALISE_ELEMENT(VkResourceType, type, IdentifyTypeByPtr(record->Resource));
  SERIALISE_ELEMENT(ResourceId, id, resid);

  if(m_State >= WRITING)
  {
    VulkanResourceManager::InitialContentData initContents =
        GetResourceManager()->GetInitialContents(id);

    if(type == eResDescriptorSet)
    {
      RDCASSERT(record->descInfo && record->descInfo->layout);
      const DescSetLayout &layout = *record->descInfo->layout;

      DescriptorSetSlot *info = (DescriptorSetSlot *)initContents.blob;

      uint32_t numElems = 0;
      for(size_t i = 0; i < layout.bindings.size(); i++)
        numElems += layout.bindings[i].descriptorCount;

      m_pSerialiser->SerialiseComplexArray("Bindings", info, numElems);
    }
    else if(type == eResBuffer)
    {
      return Serialise_SparseBufferInitialState(id, initContents);
    }
    else if(type == eResDeviceMemory || type == eResImage)
    {
      // both image and memory are serialised as a whole hunk of data
      VkDevice d = GetDev();

      bool isSparse = (initContents.blob != NULL);
      m_pSerialiser->Serialise("isSparse", isSparse);

      if(isSparse)
      {
        // contains page mapping
        RDCASSERT(type == eResImage);
        return Serialise_SparseImageInitialState(id, initContents);
      }

      byte *ptr = NULL;
      ObjDisp(d)->MapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(initContents.resource), 0,
                            VK_WHOLE_SIZE, 0, (void **)&ptr);

      size_t dataSize = (size_t)initContents.num;

      m_pSerialiser->Serialise("dataSize", initContents.num);
      m_pSerialiser->SerialiseBuffer("data", ptr, dataSize);

      ObjDisp(d)->UnmapMemory(Unwrap(d), ToHandle<VkDeviceMemory>(initContents.resource));
    }
    else
    {
      RDCERR("Unhandled resource type %d", type);
    }
  }
  else
  {
    WrappedVkRes *res = GetResourceManager()->GetLiveResource(id);

    RDCASSERT(res != NULL);

    ResourceId liveid = GetResourceManager()->GetLiveID(id);

    if(type == eResDescriptorSet)
    {
      uint32_t numElems;
      DescriptorSetSlot *bindings = NULL;

      m_pSerialiser->SerialiseComplexArray("Bindings", bindings, numElems);

      const DescSetLayout &layout =
          m_CreationInfo.m_DescSetLayout[m_DescriptorSetState[liveid].layout];

      uint32_t numBinds = (uint32_t)layout.bindings.size();

      // allocate memory to keep the element structures around, as well as a WriteDescriptorSet
      // array
      byte *blob = Serialiser::AllocAlignedBuffer(sizeof(VkDescriptorBufferInfo) * numElems +
                                                  sizeof(VkWriteDescriptorSet) * numBinds);

      RDCCOMPILE_ASSERT(sizeof(VkDescriptorBufferInfo) >= sizeof(VkDescriptorImageInfo),
                        "Descriptor structs sizes are unexpected, ensure largest size is used");

      VkWriteDescriptorSet *writes = (VkWriteDescriptorSet *)blob;
      VkDescriptorBufferInfo *dstData = (VkDescriptorBufferInfo *)(writes + numBinds);
      DescriptorSetSlot *srcData = bindings;

      uint32_t validBinds = numBinds;

      // i is the writedescriptor that we're updating, could be
      // lower than j if a writedescriptor ended up being no-op and
      // was skipped. j is the actual index.
      for(uint32_t i = 0, j = 0; j < numBinds; j++)
      {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].pNext = NULL;

        // update whole element (array or single)
        writes[i].dstSet = ToHandle<VkDescriptorSet>(res);
        writes[i].dstBinding = j;
        writes[i].dstArrayElement = 0;
        writes[i].descriptorCount = layout.bindings[j].descriptorCount;
        writes[i].descriptorType = layout.bindings[j].descriptorType;

        DescriptorSetSlot *src = srcData;
        srcData += layout.bindings[j].descriptorCount;

        // will be cast to the appropriate type, we just need to increment
        // the dstData pointer by worst case size
        VkDescriptorBufferInfo *dstBuffer = dstData;
        VkDescriptorImageInfo *dstImage = (VkDescriptorImageInfo *)dstData;
        VkBufferView *dstTexelBuffer = (VkBufferView *)dstData;
        dstData += layout.bindings[j].descriptorCount;

        // the correct one will be set below
        writes[i].pBufferInfo = NULL;
        writes[i].pImageInfo = NULL;
        writes[i].pTexelBufferView = NULL;

        // check that the resources we need for this write are present,
        // as some might have been skipped due to stale descriptor set
        // slots or otherwise unreferenced objects (the descriptor set
        // initial contents do not cause a frame reference for their
        // resources
        //
        // While we go, we copy from the DescriptorSetSlot structures to
        // the appropriate array in the VkWriteDescriptorSet for the
        // descriptor type
        bool valid = true;

        // quick check for slots that were completely uninitialised
        // and so don't have valid data
        if(src->texelBufferView == VK_NULL_HANDLE && src->imageInfo.sampler == VK_NULL_HANDLE &&
           src->imageInfo.imageView == VK_NULL_HANDLE && src->bufferInfo.buffer == VK_NULL_HANDLE)
        {
          valid = false;
        }
        else
        {
          switch(writes[i].descriptorType)
          {
            case VK_DESCRIPTOR_TYPE_SAMPLER:
            {
              for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
              {
                dstImage[d] = src[d].imageInfo;
                valid &= (src[d].imageInfo.sampler != VK_NULL_HANDLE);
              }
              writes[i].pImageInfo = dstImage;
              break;
            }
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            {
              for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
              {
                dstImage[d] = src[d].imageInfo;
                valid &= (src[d].imageInfo.sampler != VK_NULL_HANDLE) ||
                         (layout.bindings[j].immutableSampler &&
                          layout.bindings[j].immutableSampler[d] != ResourceId());
                valid &= (src[d].imageInfo.imageView != VK_NULL_HANDLE);
              }
              writes[i].pImageInfo = dstImage;
              break;
            }
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
            {
              for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
              {
                dstImage[d] = src[d].imageInfo;
                valid &= (src[d].imageInfo.imageView != VK_NULL_HANDLE);
              }
              writes[i].pImageInfo = dstImage;
              break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
            {
              for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
              {
                dstTexelBuffer[d] = src[d].texelBufferView;
                valid &= (src[d].texelBufferView != VK_NULL_HANDLE);
              }
              writes[i].pTexelBufferView = dstTexelBuffer;
              break;
            }
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
            {
              for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
              {
                dstBuffer[d] = src[d].bufferInfo;
                valid &= (src[d].bufferInfo.buffer != VK_NULL_HANDLE);
              }
              writes[i].pBufferInfo = dstBuffer;
              break;
            }
            default: RDCERR("Unexpected descriptor type %d", writes[i].descriptorType);
          }
        }

        // if this write is not valid, skip it
        // and start writing the next one in here
        if(!valid)
          validBinds--;
        else
          i++;
      }

      SAFE_DELETE_ARRAY(bindings);

      GetResourceManager()->SetInitialContents(
          id, VulkanResourceManager::InitialContentData(NULL, validBinds, blob));
    }
    else if(type == eResBuffer)
    {
      return Serialise_SparseBufferInitialState(id, VulkanResourceManager::InitialContentData());
    }
    else if(type == eResImage)
    {
      bool isSparse = false;
      m_pSerialiser->Serialise("isSparse", isSparse);

      if(isSparse)
      {
        return Serialise_SparseImageInitialState(id, VulkanResourceManager::InitialContentData());
      }

      uint32_t dataSize = 0;
      m_pSerialiser->Serialise("dataSize", dataSize);

      VkResult vkr = VK_SUCCESS;

      VkDevice d = GetDev();

      VkBufferCreateInfo bufInfo = {
          VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          NULL,
          0,
          dataSize,
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      };

      VkBuffer buf;
      VkDeviceMemory uploadmem = VK_NULL_HANDLE;

      vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &buf);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetResourceManager()->WrapResource(Unwrap(d), buf);

      VkMemoryRequirements mrq = {0};

      ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), Unwrap(buf), &mrq);

      VkMemoryAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, dataSize,
          GetUploadMemoryIndex(mrq.memoryTypeBits),
      };

      // first we upload the data into a single buffer, then we do
      // a copy per-mip from that buffer to a new image
      vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &uploadmem);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetResourceManager()->WrapResource(Unwrap(d), uploadmem);

      vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(buf), Unwrap(uploadmem), 0);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      byte *ptr = NULL;
      ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(uploadmem), 0, VK_WHOLE_SIZE, 0, (void **)&ptr);

      size_t dummy = 0;
      m_pSerialiser->SerialiseBuffer("data", ptr, dummy);

      ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(uploadmem));

      VulkanResourceManager::InitialContentData initial(GetWrapped(buf), 0, NULL);

      VulkanCreationInfo::Image &c = m_CreationInfo.m_Image[liveid];

      if(c.samples == VK_SAMPLE_COUNT_1_BIT)
      {
        // remember to free this memory on shutdown
        m_CleanupMems.push_back(uploadmem);
      }
      else
      {
        int numLayers = c.arrayLayers * (int)c.samples;

        VkImageCreateInfo arrayInfo = {
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            NULL,
            VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
            VK_IMAGE_TYPE_2D,
            c.format,
            c.extent,
            (uint32_t)c.mipLevels,
            (uint32_t)numLayers,
            VK_SAMPLE_COUNT_1_BIT,
            VK_IMAGE_TILING_OPTIMAL,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_SHARING_MODE_EXCLUSIVE,
            0,
            NULL,
            VK_IMAGE_LAYOUT_UNDEFINED,
        };

        VkImage arrayIm;

        vkr = ObjDisp(d)->CreateImage(Unwrap(d), &arrayInfo, NULL, &arrayIm);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        GetResourceManager()->WrapResource(Unwrap(d), arrayIm);

        ObjDisp(d)->GetImageMemoryRequirements(Unwrap(d), Unwrap(arrayIm), &mrq);

        allocInfo.allocationSize = mrq.size;
        allocInfo.memoryTypeIndex = GetGPULocalMemoryIndex(mrq.memoryTypeBits);

        VkDeviceMemory arrayMem;

        vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &arrayMem);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        GetResourceManager()->WrapResource(Unwrap(d), arrayMem);

        vkr = ObjDisp(d)->BindImageMemory(Unwrap(d), Unwrap(arrayIm), Unwrap(arrayMem), 0);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        VkCommandBuffer cmd = GetNextCmd();

        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                              VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

        vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        VkExtent3D extent = c.extent;

        VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

        VkFormat fmt = c.format;
        if(IsStencilOnlyFormat(fmt))
          aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
        else if(IsDepthOrStencilFormat(fmt))
          aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

        VkImageMemoryBarrier dstimBarrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            NULL,
            0,
            0,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            Unwrap(arrayIm),
            {aspectFlags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS}};

        if(aspectFlags == VK_IMAGE_ASPECT_DEPTH_BIT && !IsDepthOnlyFormat(fmt))
          dstimBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

        DoPipelineBarrier(cmd, 1, &dstimBarrier);

        VkDeviceSize bufOffset = 0;

        // must ensure offset remains valid. Must be multiple of block size, or 4, depending on
        // format
        VkDeviceSize bufAlignment = 4;
        if(IsBlockFormat(fmt))
          bufAlignment = (VkDeviceSize)GetByteSize(1, 1, 1, fmt, 0);

        std::vector<VkBufferImageCopy> mainCopies, stencilCopies;

        // copy each slice/mip individually
        for(int a = 0; a < numLayers; a++)
        {
          extent = c.extent;

          for(int m = 0; m < c.mipLevels; m++)
          {
            VkBufferImageCopy region = {
                0,
                0,
                0,
                {aspectFlags, (uint32_t)m, (uint32_t)a, 1},
                {
                    0, 0, 0,
                },
                extent,
            };

            bufOffset = AlignUp(bufOffset, bufAlignment);

            region.bufferOffset = bufOffset;

            VkFormat sizeFormat = GetDepthOnlyFormat(fmt);

            // pass 0 for mip since we've already pre-downscaled extent
            bufOffset += GetByteSize(extent.width, extent.height, extent.depth, sizeFormat, 0);

            mainCopies.push_back(region);

            if(sizeFormat != fmt)
            {
              // if we removed stencil from the format, copy that separately now.
              bufOffset = AlignUp(bufOffset, bufAlignment);

              region.bufferOffset = bufOffset;
              region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

              bufOffset +=
                  GetByteSize(extent.width, extent.height, extent.depth, VK_FORMAT_S8_UINT, 0);

              stencilCopies.push_back(region);
            }

            // update the extent for the next mip
            extent.width = RDCMAX(extent.width >> 1, 1U);
            extent.height = RDCMAX(extent.height >> 1, 1U);
            extent.depth = RDCMAX(extent.depth >> 1, 1U);
          }
        }

        ObjDisp(cmd)->CmdCopyBufferToImage(Unwrap(cmd), Unwrap(buf), Unwrap(arrayIm),
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                           (uint32_t)mainCopies.size(), &mainCopies[0]);

        if(!stencilCopies.empty())
          ObjDisp(cmd)->CmdCopyBufferToImage(Unwrap(cmd), Unwrap(buf), Unwrap(arrayIm),
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                             (uint32_t)stencilCopies.size(), &stencilCopies[0]);

        // once transfers complete, get ready for copy array->ms
        dstimBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        dstimBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        dstimBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstimBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        DoPipelineBarrier(cmd, 1, &dstimBarrier);

        vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        // INITSTATEBATCH
        SubmitCmds();
        FlushQ();

        vkDestroyBuffer(d, buf, NULL);
        vkFreeMemory(d, uploadmem, NULL);

        m_CleanupMems.push_back(arrayMem);
        initial.resource = GetWrapped(arrayIm);
      }

      GetResourceManager()->SetInitialContents(id, initial);
    }
    else if(type == eResDeviceMemory)
    {
      // dummy since we share a serialise-write for devicememory and image. This will always be
      // false
      bool isSparse = false;
      m_pSerialiser->Serialise("isSparse", isSparse);

      (void)isSparse;
      RDCASSERT(!isSparse);

      uint32_t dataSize = 0;
      m_pSerialiser->Serialise("dataSize", dataSize);

      VkResult vkr = VK_SUCCESS;

      VkDevice d = GetDev();

      VkDeviceMemory mem = VK_NULL_HANDLE;

      VkBufferCreateInfo bufInfo = {
          VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
          NULL,
          0,
          dataSize,
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      };

      VkBuffer buf;

      vkr = ObjDisp(d)->CreateBuffer(Unwrap(d), &bufInfo, NULL, &buf);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetResourceManager()->WrapResource(Unwrap(d), buf);

      VkMemoryRequirements mrq = {0};

      ObjDisp(d)->GetBufferMemoryRequirements(Unwrap(d), Unwrap(buf), &mrq);

      VkMemoryAllocateInfo allocInfo = {
          VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, mrq.size,
          GetUploadMemoryIndex(mrq.memoryTypeBits),
      };

      vkr = ObjDisp(d)->AllocateMemory(Unwrap(d), &allocInfo, NULL, &mem);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetResourceManager()->WrapResource(Unwrap(d), mem);

      vkr = ObjDisp(d)->BindBufferMemory(Unwrap(d), Unwrap(buf), Unwrap(mem), 0);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      byte *ptr = NULL;
      ObjDisp(d)->MapMemory(Unwrap(d), Unwrap(mem), 0, VK_WHOLE_SIZE, 0, (void **)&ptr);

      size_t dummy = 0;
      m_pSerialiser->SerialiseBuffer("data", ptr, dummy);

      ObjDisp(d)->UnmapMemory(Unwrap(d), Unwrap(mem));

      m_CleanupMems.push_back(mem);

      GetResourceManager()->SetInitialContents(
          id, VulkanResourceManager::InitialContentData(GetWrapped(buf), (uint32_t)dataSize, NULL));
    }
    else
    {
      RDCERR("Unhandled resource type %d", type);
    }
  }

  return true;
}

void WrappedVulkan::Create_InitialState(ResourceId id, WrappedVkRes *live, bool hasData)
{
  VkResourceType type = IdentifyTypeByPtr(live);

  if(type == eResDescriptorSet)
  {
    // There is no sensible default for a descriptor set to create. The contents are
    // undefined until written to. This means if a descriptor set was alloc'd within a
    // frame (the only time we won't have initial contents tracked for it) then the
    // contents are undefined, so using whatever is currently in the set is fine. Reading
    // from it (and thus getting data from later in the frame potentially) is an error.
    //
    // Note the same kind of problem applies if a descriptor set is alloc'd before the
    // frame and then say slot 5 is never written to until the middle of the frame, then
    // used. The initial states we have prepared won't have anything valid for 5 so when
    // we apply we won't even write anything into slot 5 - the same case as if we had
    // no initial states at all for that descriptor set
  }
  else if(type == eResImage)
  {
    ResourceId liveid = GetResourceManager()->GetLiveID(id);

    if(m_ImageLayouts.find(liveid) == m_ImageLayouts.end())
    {
      RDCERR("Couldn't find image info for %llu", id);
      GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(
                                                       NULL, eInitialContents_ClearColorImage, NULL));
      return;
    }

    ImageLayouts &layouts = m_ImageLayouts[liveid];

    if(layouts.subresourceStates[0].subresourceRange.aspectMask == VK_IMAGE_ASPECT_COLOR_BIT)
      GetResourceManager()->SetInitialContents(id, VulkanResourceManager::InitialContentData(
                                                       NULL, eInitialContents_ClearColorImage, NULL));
    else
      GetResourceManager()->SetInitialContents(
          id, VulkanResourceManager::InitialContentData(
                  NULL, eInitialContents_ClearDepthStencilImage, NULL));
  }
  else if(type == eResDeviceMemory)
  {
    // ignore, it was probably dirty but not referenced in the frame
  }
  else
  {
    RDCERR("Unhandled resource type %d", type);
  }
}

void WrappedVulkan::Apply_InitialState(WrappedVkRes *live,
                                       VulkanResourceManager::InitialContentData initial)
{
  VkResourceType type = IdentifyTypeByPtr(live);

  ResourceId id = GetResourceManager()->GetID(live);

  if(type == eResDescriptorSet)
  {
    VkWriteDescriptorSet *writes = (VkWriteDescriptorSet *)initial.blob;

    // if it ended up that no descriptors were valid, just skip
    if(initial.num == 0)
      return;

    ObjDisp(GetDev())->UpdateDescriptorSets(Unwrap(GetDev()), initial.num, writes, 0, NULL);

    // need to blat over the current descriptor set contents, so these are available
    // when we want to fetch pipeline state
    vector<DescriptorSetSlot *> &bindings = m_DescriptorSetState[id].currentBindings;

    for(uint32_t i = 0; i < initial.num; i++)
    {
      RDCASSERT(writes[i].dstBinding < bindings.size());
      RDCASSERT(writes[i].dstArrayElement == 0);

      DescriptorSetSlot *bind = bindings[writes[i].dstBinding];

      for(uint32_t d = 0; d < writes[i].descriptorCount; d++)
      {
        if(writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER ||
           writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER)
        {
          bind[d].texelBufferView = writes[i].pTexelBufferView[d];
        }
        else if(writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                writes[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
        {
          bind[d].bufferInfo = writes[i].pBufferInfo[d];
        }
        else
        {
          bind[d].imageInfo = writes[i].pImageInfo[d];
        }
      }
    }
  }
  else if(type == eResBuffer)
  {
    Apply_SparseInitialState((WrappedVkBuffer *)live, initial);
  }
  else if(type == eResImage)
  {
    VkResult vkr = VK_SUCCESS;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    if(initial.blob != NULL)
    {
      RDCASSERT(initial.num == eInitialContents_Sparse);
      Apply_SparseInitialState((WrappedVkImage *)live, initial);
      return;
    }

    // handle any 'created' initial states, without an actual image with contents
    if(initial.resource == NULL)
    {
      RDCASSERT(initial.num != eInitialContents_Sparse);
      if(initial.num == eInitialContents_ClearColorImage)
      {
        if(IsBlockFormat(m_ImageLayouts[id].format))
        {
          RDCWARN(
              "Trying to clear a compressed image %llu - should have initial states or be "
              "stripped.",
              id);
          return;
        }

        VkCommandBuffer cmd = GetNextCmd();

        vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        VkImageMemoryBarrier barrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            NULL,
            0,
            0,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            ToHandle<VkImage>(live),
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
        };

        // finish any pending work before clear
        barrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
        // clear completes before subsequent operations
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          barrier.oldLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
          DoPipelineBarrier(cmd, 1, &barrier);
        }

        VkClearColorValue clearval = {};
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0,
                                         VK_REMAINING_ARRAY_LAYERS};

        ObjDisp(cmd)->CmdClearColorImage(Unwrap(cmd), ToHandle<VkImage>(live),
                                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearval, 1, &range);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        // complete clear before any other work
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_ALL_READ_BITS;

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          barrier.newLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
          barrier.dstAccessMask |= MakeAccessMask(barrier.newLayout);
          DoPipelineBarrier(cmd, 1, &barrier);
        }

        vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
        SubmitCmds();
#endif
      }
      else if(initial.num == eInitialContents_ClearDepthStencilImage)
      {
        VkCommandBuffer cmd = GetNextCmd();

        vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

        VkImageMemoryBarrier barrier = {
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            NULL,
            0,
            0,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            ToHandle<VkImage>(live),
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
        };

        // finish any pending work before clear
        barrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
        // clear completes before subsequent operations
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          barrier.oldLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
          DoPipelineBarrier(cmd, 1, &barrier);
        }

        VkClearDepthStencilValue clearval = {1.0f, 0};
        VkImageSubresourceRange range = {barrier.subresourceRange.aspectMask, 0,
                                         VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS};

        ObjDisp(cmd)->CmdClearDepthStencilImage(Unwrap(cmd), ToHandle<VkImage>(live),
                                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearval, 1,
                                                &range);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

        // complete clear before any other work
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_ALL_READ_BITS;

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          barrier.newLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
          DoPipelineBarrier(cmd, 1, &barrier);
        }

        vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
        RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
        SubmitCmds();
#endif
      }
      else
      {
        RDCERR("Unexpected initial state type %u with NULL resource", initial.num);
      }

      return;
    }

    if(m_CreationInfo.m_Image[id].samples != VK_SAMPLE_COUNT_1_BIT)
    {
      VkCommandBuffer cmd = GetNextCmd();

      vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

      VulkanCreationInfo::Image &c = m_CreationInfo.m_Image[id];

      VkFormat fmt = c.format;
      if(IsStencilOnlyFormat(fmt))
        aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
      else if(IsDepthOrStencilFormat(fmt))
        aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

      if(aspectFlags == VK_IMAGE_ASPECT_DEPTH_BIT && !IsDepthOnlyFormat(fmt))
        aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;

      VkImageMemoryBarrier barrier = {
          VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          NULL,
          0,
          0,
          VK_IMAGE_LAYOUT_UNDEFINED,
          VK_IMAGE_LAYOUT_GENERAL,
          VK_QUEUE_FAMILY_IGNORED,
          VK_QUEUE_FAMILY_IGNORED,
          ToHandle<VkImage>(live),
          {aspectFlags, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS},
      };

      barrier.srcAccessMask = VK_ACCESS_ALL_WRITE_BITS;
      barrier.dstAccessMask =
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

      for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
      {
        barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
        barrier.oldLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
        DoPipelineBarrier(cmd, 1, &barrier);
      }

      WrappedVkImage *arrayIm = (WrappedVkImage *)initial.resource;

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      GetDebugManager()->CopyArrayToTex2DMS(ToHandle<VkImage>(live), arrayIm->real.As<VkImage>(),
                                            c.extent, (uint32_t)c.arrayLayers, (uint32_t)c.samples,
                                            fmt);

      cmd = GetNextCmd();

      vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

      barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;

      // complete copy before any other work
      barrier.srcAccessMask =
          VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_ALL_READ_BITS;

      for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
      {
        barrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
        barrier.newLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
        barrier.dstAccessMask |= MakeAccessMask(barrier.newLayout);
        DoPipelineBarrier(cmd, 1, &barrier);
      }

      vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
      RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
      SubmitCmds();
#endif
      return;
    }

    WrappedVkBuffer *buf = (WrappedVkBuffer *)initial.resource;

    VkCommandBuffer cmd = GetNextCmd();

    vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkExtent3D extent = m_CreationInfo.m_Image[id].extent;

    VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;

    VkFormat fmt = m_CreationInfo.m_Image[id].format;
    if(IsStencilOnlyFormat(fmt))
      aspectFlags = VK_IMAGE_ASPECT_STENCIL_BIT;
    else if(IsDepthOrStencilFormat(fmt))
      aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;

    VkImageMemoryBarrier dstimBarrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        NULL,
        0,
        0,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_QUEUE_FAMILY_IGNORED,
        VK_QUEUE_FAMILY_IGNORED,
        ToHandle<VkImage>(live),
        {aspectFlags, 0, 1, 0, (uint32_t)m_CreationInfo.m_Image[id].arrayLayers}};

    if(aspectFlags == VK_IMAGE_ASPECT_DEPTH_BIT && !IsDepthOnlyFormat(fmt))
      dstimBarrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

    VkDeviceSize bufOffset = 0;

    // must ensure offset remains valid. Must be multiple of block size, or 4, depending on format
    VkDeviceSize bufAlignment = 4;
    if(IsBlockFormat(fmt))
      bufAlignment = (VkDeviceSize)GetByteSize(1, 1, 1, fmt, 0);

    // copy each slice/mip individually
    for(int a = 0; a < m_CreationInfo.m_Image[id].arrayLayers; a++)
    {
      extent = m_CreationInfo.m_Image[id].extent;

      for(int m = 0; m < m_CreationInfo.m_Image[id].mipLevels; m++)
      {
        VkBufferImageCopy region = {
            0,
            0,
            0,
            {aspectFlags, (uint32_t)m, (uint32_t)a, 1},
            {
                0, 0, 0,
            },
            extent,
        };

        bufOffset = AlignUp(bufOffset, bufAlignment);

        region.bufferOffset = bufOffset;

        VkFormat sizeFormat = GetDepthOnlyFormat(fmt);

        // pass 0 for mip since we've already pre-downscaled extent
        bufOffset += GetByteSize(extent.width, extent.height, extent.depth, sizeFormat, 0);

        dstimBarrier.subresourceRange.baseArrayLayer = a;
        dstimBarrier.subresourceRange.baseMipLevel = m;

        // first update the live image layout into destination optimal (the initial state
        // image is always and permanently in source optimal already).
        dstimBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        dstimBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          dstimBarrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          dstimBarrier.oldLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
          dstimBarrier.srcAccessMask =
              VK_ACCESS_ALL_WRITE_BITS | MakeAccessMask(dstimBarrier.oldLayout);
          DoPipelineBarrier(cmd, 1, &dstimBarrier);
        }

        ObjDisp(cmd)->CmdCopyBufferToImage(Unwrap(cmd), buf->real.As<VkBuffer>(),
                                           ToHandle<VkImage>(live),
                                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        if(sizeFormat != fmt)
        {
          // if we removed stencil from the format, copy that separately now.
          bufOffset = AlignUp(bufOffset, bufAlignment);

          region.bufferOffset = bufOffset;
          region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;

          bufOffset += GetByteSize(extent.width, extent.height, extent.depth, VK_FORMAT_S8_UINT, 0);

          ObjDisp(cmd)->CmdCopyBufferToImage(Unwrap(cmd), buf->real.As<VkBuffer>(),
                                             ToHandle<VkImage>(live),
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        }

        // update the live image layout back
        dstimBarrier.oldLayout = dstimBarrier.newLayout;

        // make sure the apply completes before any further work
        dstimBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstimBarrier.dstAccessMask = VK_ACCESS_ALL_READ_BITS;

        for(size_t si = 0; si < m_ImageLayouts[id].subresourceStates.size(); si++)
        {
          dstimBarrier.subresourceRange = m_ImageLayouts[id].subresourceStates[si].subresourceRange;
          dstimBarrier.newLayout = m_ImageLayouts[id].subresourceStates[si].newLayout;
          dstimBarrier.dstAccessMask |= MakeAccessMask(dstimBarrier.newLayout);
          DoPipelineBarrier(cmd, 1, &dstimBarrier);
        }

        // update the extent for the next mip
        extent.width = RDCMAX(extent.width >> 1, 1U);
        extent.height = RDCMAX(extent.height >> 1, 1U);
        extent.depth = RDCMAX(extent.depth >> 1, 1U);
      }
    }

    vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
    SubmitCmds();
#endif
  }
  else if(type == eResDeviceMemory)
  {
    VkResult vkr = VK_SUCCESS;

    VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    VkBuffer srcBuf = (VkBuffer)(uint64_t)initial.resource;
    VkDeviceSize datasize = (VkDeviceSize)initial.num;
    VkDeviceSize dstMemOffs = 0;

    VkCommandBuffer cmd = GetNextCmd();

    vkr = ObjDisp(cmd)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

    VkBuffer dstBuf = m_CreationInfo.m_Memory[id].wholeMemBuf;

    VkBufferCopy region = {0, dstMemOffs, datasize};

    ObjDisp(cmd)->CmdCopyBuffer(Unwrap(cmd), Unwrap(srcBuf), Unwrap(dstBuf), 1, &region);

    vkr = ObjDisp(cmd)->EndCommandBuffer(Unwrap(cmd));
    RDCASSERTEQUAL(vkr, VK_SUCCESS);

#if ENABLED(SINGLE_FLUSH_VALIDATE)
    SubmitCmds();
#endif
  }
  else
  {
    RDCERR("Unhandled resource type %d", type);
  }
}
