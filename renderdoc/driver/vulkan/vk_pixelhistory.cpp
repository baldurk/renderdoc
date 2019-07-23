#include <stdio.h>
#include <sys/types.h>
#include <vector>
#include "driver/vulkan/vk_debug.h"
#include "driver/vulkan/vk_replay.h"
#include "vk_shader_cache.h"

bool isUsageUAV(ResourceUsage usage)
{
  return ((usage >= ResourceUsage::VS_RWResource && usage <= ResourceUsage::CS_RWResource) ||
          usage == ResourceUsage::CopyDst || usage == ResourceUsage::Copy ||
          usage == ResourceUsage::Resolve || usage == ResourceUsage::ResolveDst ||
          usage == ResourceUsage::GenMips);
}

struct CopyPixelParams
{
  bool multisampled;
  bool floatTex;
  bool uintTex;
  bool intTex;

  bool depthcopy;
  VkImage srcImage;
  VkFormat srcImageFormat;
  VkImageLayout srcImageLayout;
  VkOffset3D imageOffset;

  VkBuffer dstBuffer;
};

struct PixelHistoryResources
{
  VkBuffer dstBuffer;
  VkDeviceMemory bufferMemory;

  // Used for offscreen rendering for draw call events.
  VkImage colorImage;
  VkImageView colorImageView;
  VkImage stencilImage;
  VkImageView stencilImageView;
  VkDeviceMemory gpuMem;
};

struct PixelHistoryValue
{
  uint8_t color[16];
  uint8_t depth[8];
};

struct EventInfo
{
  PixelHistoryValue premod;
  PixelHistoryValue postmod;
  uint8_t shadout[16];
  uint8_t fixed_stencil[8];
  uint8_t original_stencil[8];
};

struct VulkanPixelHistoryCallback : public VulkanDrawcallCallback
{
  VulkanPixelHistoryCallback(WrappedVulkan *vk, uint32_t x, uint32_t y, VkImage image,
                             VkFormat format, uint32_t sampleMask, VkQueryPool occlusionPool,
                             VkImageView colorImageView, VkImageView stencilImageView,
                             VkImage colorImage, VkImage stencilImage, VkBuffer dstBuffer,
                             const std::vector<EventUsage> &events)
      : m_pDriver(vk),
        m_X(x),
        m_Y(y),
        m_Image(image),
        m_Format(format),
        m_DstBuffer(dstBuffer),
        m_SampleMask(sampleMask),
        m_OcclusionPool(occlusionPool),
        m_ColorImageView(colorImageView),
        m_StencilImageView(stencilImageView),
        m_ColorImage(colorImage),
        m_StencilImage(stencilImage),
        m_PrevState(vk, NULL)
  {
    m_pDriver->SetDrawcallCB(this);
    for(size_t i = 0; i < events.size(); i++)
      m_Events.insert(std::make_pair(events[i].eventId, events[i]));
  }
  ~VulkanPixelHistoryCallback() { m_pDriver->SetDrawcallCB(NULL); }
  void CopyPixel(uint32_t eid, VkCommandBuffer cmd, size_t offset)
  {
    CopyPixelParams colourCopyParams = {};
    colourCopyParams.multisampled = false;    // TODO: multisampled
    colourCopyParams.srcImage = m_Image;
    colourCopyParams.srcImageLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;    // TODO: image layout
    colourCopyParams.srcImageFormat = m_Format;
    colourCopyParams.imageOffset = VkOffset3D{int32_t(m_X), int32_t(m_Y), 0};
    colourCopyParams.dstBuffer = m_DstBuffer;

    m_pDriver->GetDebugManager()->PixelHistoryCopyPixel(cmd, colourCopyParams, offset);

    const DrawcallDescription *draw = m_pDriver->GetDrawcall(eid);
    if(draw && draw->depthOut != ResourceId())
    {
      // The draw call had a depth image attachment.
      ResourceId depthImage = m_pDriver->GetResourceManager()->GetLiveID(draw->depthOut);
      VulkanCreationInfo::Image imginfo = m_pDriver->GetDebugManager()->GetImageInfo(depthImage);
      CopyPixelParams depthCopyParams = colourCopyParams;
      depthCopyParams.depthcopy = true;
      depthCopyParams.srcImage =
          (VkImage)m_pDriver->GetResourceManager()->GetCurrentResource(depthImage);
      depthCopyParams.srcImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      depthCopyParams.srcImageFormat = imginfo.format;
      m_pDriver->GetDebugManager()->PixelHistoryCopyPixel(
          cmd, depthCopyParams, offset + offsetof(struct PixelHistoryValue, depth));
    }
  }

  void PreDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    auto it = m_Events.find(eid);
    if(it == m_Events.end())
      return;
    EventUsage event = it->second;
    m_PrevState = m_pDriver->GetRenderState();

    // TODO: handle secondary command buffers.
    m_pDriver->GetRenderState().EndRenderPass(cmd);
    // Get pre-modification values
    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(eid, cmd, storeOffset);

    // TODO: add all logic for draw calls.

    if(m_PrevState.graphics.pipeline != ResourceId())
      m_pDriver->GetRenderState().BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);
  }

  bool PostDraw(uint32_t eid, VkCommandBuffer cmd)
  {
    if(m_Events.find(eid) == m_Events.end())
      return false;

    m_pDriver->GetRenderState().EndRenderPass(cmd);

    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(eid, cmd, storeOffset + offsetof(struct EventInfo, postmod));

    m_pDriver->GetRenderState().BeginRenderPassAndApplyState(cmd, VulkanRenderState::BindGraphics);

    // Get post-modification values
    m_EventIndices.insert(std::make_pair(eid, m_EventIndices.size()));
    return false;
  }

  void PostRedraw(uint32_t eid, VkCommandBuffer cmd)
  {
    // nothing to do
  }

  void PreDispatch(uint32_t eid, VkCommandBuffer cmd)
  {
    if(m_Events.find(eid) == m_Events.end())
      return;

    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(eid, cmd, storeOffset);
  }
  bool PostDispatch(uint32_t eid, VkCommandBuffer cmd)
  {
    if(m_Events.find(eid) == m_Events.end())
      return false;

    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(eid, cmd, storeOffset + offsetof(struct EventInfo, postmod));
    m_EventIndices.insert(std::make_pair(eid, m_EventIndices.size()));
    return false;
  }
  void PostRedispatch(uint32_t eid, VkCommandBuffer cmd) {}
  void PreMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd)
  {
    if(m_Events.find(eid) == m_Events.end())
      return;

    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(eid, cmd, storeOffset);
  }
  bool PostMisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd)
  {
    if(m_Events.find(eid) == m_Events.end())
      return false;

    size_t storeOffset = m_EventIndices.size() * sizeof(EventInfo);
    CopyPixel(eid, cmd, storeOffset + offsetof(struct EventInfo, postmod));
    m_EventIndices.insert(std::make_pair(eid, m_EventIndices.size()));
    return false;
  }

  void PostRemisc(uint32_t eid, DrawFlags flags, VkCommandBuffer cmd) {}
  void PreEndCommandBuffer(VkCommandBuffer cmd) {}
  void AliasEvent(uint32_t primary, uint32_t alias)
  {
    // TODO: handled aliased events.
  }

  WrappedVulkan *m_pDriver;
  VkImage m_Image;
  VkFormat m_Format;
  std::map<uint32_t, EventUsage> m_Events;
  std::map<uint32_t, size_t> m_EventIndices;
  VkBuffer m_DstBuffer;
  uint32_t m_X, m_Y;

  uint32_t m_SampleMask;
  VkQueryPool m_OcclusionPool;
  VkImageView m_ColorImageView;
  VkImageView m_StencilImageView;
  VkImage m_ColorImage;
  VkImage m_StencilImage;

  VulkanRenderState m_PrevState;
};

bool VulkanDebugManager::PixelHistorySetupResources(PixelHistoryResources &resources,
                                                    VkExtent3D extent, VkFormat format,
                                                    uint32_t numEvents)
{
  VkImage colorImage;
  VkImageView colorImageView;
  VkImage stencilImage;
  VkImageView stencilImageView;
  VkDeviceMemory gpuMem;

  VkBuffer dstBuffer;
  VkDeviceMemory bufferMemory;

  VkResult vkr;
  VkDevice dev = m_pDriver->GetDev();

  // Create Images
  VkImageCreateInfo imgInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  imgInfo.imageType = VK_IMAGE_TYPE_2D;
  imgInfo.mipLevels = 1;
  imgInfo.arrayLayers = 1;
  imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  // Device local resources:
  imgInfo.format = format;
  imgInfo.extent = {extent.width, extent.height, 1};
  imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  vkr = m_pDriver->vkCreateImage(dev, &imgInfo, NULL, &colorImage);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMemoryRequirements colorImageMrq = {0};
  m_pDriver->vkGetImageMemoryRequirements(dev, colorImage, &colorImageMrq);

  imgInfo.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
  imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

  vkr = m_pDriver->vkCreateImage(dev, &imgInfo, NULL, &stencilImage);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkMemoryRequirements stencilImageMrq = {0};
  m_pDriver->vkGetImageMemoryRequirements(dev, stencilImage, &stencilImageMrq);
  VkDeviceSize offset = AlignUp(colorImageMrq.size, stencilImageMrq.alignment);

  VkMemoryAllocateInfo allocInfo = {
      VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, NULL, offset + stencilImageMrq.size,
      m_pDriver->GetGPULocalMemoryIndex(colorImageMrq.memoryTypeBits),
  };
  vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &gpuMem);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = m_pDriver->vkBindImageMemory(m_Device, colorImage, gpuMem, 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = m_pDriver->vkBindImageMemory(m_Device, stencilImage, gpuMem, offset);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkImageViewCreateInfo viewInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
  viewInfo.image = colorImage;
  viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
  viewInfo.format = format;
  viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &colorImageView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  viewInfo.image = stencilImage;
  viewInfo.format = VK_FORMAT_D32_SFLOAT_S8_UINT;
  viewInfo.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};

  vkr = m_pDriver->vkCreateImageView(m_Device, &viewInfo, NULL, &stencilImageView);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
  bufferInfo.size = AlignUp((uint32_t)(numEvents * sizeof(EventInfo)), 512U);
  bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;

  vkr = m_pDriver->vkCreateBuffer(m_Device, &bufferInfo, NULL, &dstBuffer);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  // Allocate memory
  VkMemoryRequirements mrq = {};
  m_pDriver->vkGetBufferMemoryRequirements(m_Device, dstBuffer, &mrq);
  allocInfo.allocationSize = mrq.size;
  allocInfo.memoryTypeIndex = m_pDriver->GetReadbackMemoryIndex(mrq.memoryTypeBits);
  vkr = m_pDriver->vkAllocateMemory(m_Device, &allocInfo, NULL, &bufferMemory);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  vkr = m_pDriver->vkBindBufferMemory(m_Device, dstBuffer, bufferMemory, 0);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkCommandBuffer cmd = m_pDriver->GetNextCmd();
  VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, NULL,
                                        VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

  vkr = ObjDisp(dev)->BeginCommandBuffer(Unwrap(cmd), &beginInfo);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  VkImageMemoryBarrier barriers[2] = {};
  barriers[0] = {};
  barriers[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barriers[0].srcAccessMask = 0;
  barriers[0].oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  barriers[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barriers[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barriers[0].newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barriers[0].image = Unwrap(colorImage);

  barriers[1] = barriers[0];
  barriers[1].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  barriers[1].newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  barriers[1].subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0,
                                  1};
  barriers[1].image = Unwrap(stencilImage);

  DoPipelineBarrier(cmd, 2, barriers);

  {
    SCOPED_LOCK(m_pDriver->m_ImageLayoutsLock);
    m_pDriver->m_ImageLayouts[GetResID(colorImage)].subresourceStates[0].newLayout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    m_pDriver->m_ImageLayouts[GetResID(stencilImage)].subresourceStates[0].newLayout =
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  }
  vkr = ObjDisp(dev)->EndCommandBuffer(Unwrap(cmd));
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  resources.colorImage = colorImage;
  resources.colorImageView = colorImageView;
  resources.stencilImage = stencilImage;
  resources.stencilImageView = stencilImageView;
  resources.gpuMem = gpuMem;

  resources.bufferMemory = bufferMemory;
  resources.dstBuffer = dstBuffer;

  return true;
}

bool VulkanDebugManager::PixelHistoryDestroyResources(const PixelHistoryResources &r)
{
  VkDevice dev = m_pDriver->GetDev();
  if(r.gpuMem != VK_NULL_HANDLE)
    m_pDriver->vkFreeMemory(dev, r.gpuMem, NULL);
  if(r.colorImage != VK_NULL_HANDLE)
    m_pDriver->vkDestroyImage(dev, r.colorImage, NULL);
  if(r.colorImageView != VK_NULL_HANDLE)
    m_pDriver->vkDestroyImageView(dev, r.colorImageView, NULL);
  if(r.stencilImage != VK_NULL_HANDLE)
    m_pDriver->vkDestroyImage(dev, r.stencilImage, NULL);
  if(r.stencilImageView != VK_NULL_HANDLE)
    m_pDriver->vkDestroyImageView(dev, r.stencilImageView, NULL);
  if(r.dstBuffer != VK_NULL_HANDLE)
    m_pDriver->vkDestroyBuffer(dev, r.dstBuffer, NULL);
  if(r.bufferMemory != VK_NULL_HANDLE)
    m_pDriver->vkFreeMemory(dev, r.bufferMemory, NULL);
  return true;
}

void VulkanDebugManager::PixelHistoryCopyPixel(VkCommandBuffer cmd, CopyPixelParams &p, size_t offset)
{
  std::vector<VkBufferImageCopy> regions;
  // Check if depth image includes depth and stencil
  VkImageAspectFlags aspectFlags = 0;
  VkBufferImageCopy region = {};
  region.bufferOffset = (uint64_t)offset;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageOffset = p.imageOffset;
  region.imageExtent = VkExtent3D{1U, 1U, 1U};
  if(!p.depthcopy)
  {
    region.imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    regions.push_back(region);
    aspectFlags = VkImageAspectFlags(VK_IMAGE_ASPECT_COLOR_BIT);
  }
  else
  {
    region.imageSubresource = VkImageSubresourceLayers{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
    if(IsDepthOnlyFormat(p.srcImageFormat) || IsDepthAndStencilFormat(p.srcImageFormat))
    {
      regions.push_back(region);
      aspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    if(IsStencilFormat(p.srcImageFormat))
    {
      region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
      region.bufferOffset = offset + 4;
      regions.push_back(region);
      aspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  }

  VkImageMemoryBarrier barrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                                  NULL,
                                  VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                      VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                                      VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
                                  VK_ACCESS_TRANSFER_READ_BIT,
                                  p.srcImageLayout,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                  VK_QUEUE_FAMILY_IGNORED,
                                  VK_QUEUE_FAMILY_IGNORED,
                                  Unwrap(p.srcImage),
                                  {aspectFlags, 0, 1, 0, 1}};

  DoPipelineBarrier(cmd, 1, &barrier);

  ObjDisp(cmd)->CmdCopyImageToBuffer(Unwrap(cmd), Unwrap(p.srcImage),
                                     VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Unwrap(p.dstBuffer),
                                     (uint32_t)regions.size(), regions.data());

  barrier.image = Unwrap(p.srcImage);
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
  barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  barrier.newLayout = p.srcImageLayout;
  DoPipelineBarrier(cmd, 1, &barrier);
}

void CreateOcclusionPool(VkDevice dev, uint32_t poolSize, VkQueryPool *pQueryPool)
{
  VkQueryPoolCreateInfo occlusionPoolCreateInfo = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
  occlusionPoolCreateInfo.queryType = VK_QUERY_TYPE_OCCLUSION;
  occlusionPoolCreateInfo.queryCount = poolSize;
  // TODO: check that occlusion feature is available
  VkResult vkr =
      ObjDisp(dev)->CreateQueryPool(Unwrap(dev), &occlusionPoolCreateInfo, NULL, pQueryPool);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);
}

VkImageLayout VulkanDebugManager::GetImageLayout(ResourceId image, VkImageAspectFlags aspect,
                                                 uint32_t mip)
{
  VkImageLayout imgLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  const ImageLayouts &imgLayouts = m_pDriver->m_ImageLayouts[image];
  for(const auto &resState : imgLayouts.subresourceStates)
  {
    VkImageSubresourceRange range = resState.subresourceRange;

    if((range.aspectMask & aspect) &&
       (mip >= range.baseMipLevel && mip < range.baseMipLevel + range.levelCount))
    {
      // Consider using the layer count
      imgLayout = resState.newLayout;
    }
  }
  return imgLayout;
}

std::vector<PixelModification> VulkanReplay::PixelHistory(std::vector<EventUsage> events,
                                                          ResourceId target, uint32_t x, uint32_t y,
                                                          uint32_t slice, uint32_t mip,
                                                          uint32_t sampleIdx, CompType typeHint)
{
  std::vector<PixelModification> history;
  VkResult vkr;
  VkDevice dev = m_pDriver->GetDev();

  if(events.empty())
    return history;

  VulkanCreationInfo::Image imginfo = GetDebugManager()->GetImageInfo(target);
  if(imginfo.format == VK_FORMAT_UNDEFINED)
    return history;

  // TODO: figure out correct aspect.
  VkImageLayout imgLayout = GetDebugManager()->GetImageLayout(target, VK_IMAGE_ASPECT_COLOR_BIT, mip);
  RDCASSERTNOTEQUAL(imgLayout, VK_IMAGE_LAYOUT_UNDEFINED);

  // TODO: use the given type hint for typeless textures
  SCOPED_TIMER("VkDebugManager::PixelHistory");

  if(sampleIdx > (uint32_t)imginfo.samples)
    sampleIdx = 0;

  uint32_t sampleMask = ~0U;
  if(sampleIdx < 32)
    sampleMask = 1U << sampleIdx;

  bool multisampled = (imginfo.samples > 1);

  if(sampleIdx == ~0U || !multisampled)
    sampleIdx = 0;

  RDCASSERT(m_pDriver->GetDeviceFeatures().occlusionQueryPrecise);
  VkPhysicalDeviceProperties props = m_pDriver->GetDeviceProps();
  VkPhysicalDeviceFeatures features = m_pDriver->GetDeviceFeatures();

  VkQueryPool occlusionPool;
  CreateOcclusionPool(dev, (uint32_t)events.size(), &occlusionPool);

  PixelHistoryResources resources = {};
  GetDebugManager()->PixelHistorySetupResources(resources, imginfo.extent, imginfo.format,
                                                (uint32_t)events.size());

  VulkanPixelHistoryCallback cb(
      m_pDriver, x, y, (VkImage)GetResourceManager()->GetCurrentResource(target), imginfo.format,
      sampleMask, occlusionPool, resources.colorImageView, resources.stencilImageView,
      resources.colorImage, resources.stencilImage, resources.dstBuffer, events);
  m_pDriver->ReplayLog(0, events.back().eventId, eReplay_Full);
  m_pDriver->SubmitCmds();
  m_pDriver->FlushQ();

  for(size_t ev = 0; ev < events.size(); ev++)
  {
    const DrawcallDescription *draw = m_pDriver->GetDrawcall(events[ev].eventId);
    bool clear = bool(draw->flags & DrawFlags::Clear);
    bool uavWrite = isUsageUAV(events[ev].usage);
    // TODO: actually do occlusion query.
    uint64_t occlData = 1;

    if(events[ev].view != ResourceId())
    {
      // TODO
    }

    if(occlData > 0 || clear || uavWrite)
    {
      PixelModification mod;
      RDCEraseEl(mod);

      mod.eventId = events[ev].eventId;
      mod.directShaderWrite = uavWrite;
      mod.unboundPS = false;

      if(!clear && !uavWrite)
      {
        // TODO: fill in flags for the modification.
      }
      history.push_back(mod);
    }
  }

  // Try to read memory back

  void *bufPtr = NULL;
  vkr = m_pDriver->vkMapMemory(dev, resources.bufferMemory, 0, VK_WHOLE_SIZE, 0, (void **)&bufPtr);
  RDCASSERTEQUAL(vkr, VK_SUCCESS);

  EventInfo *eventsInfo = (EventInfo *)bufPtr;

  for(size_t h = 0; h < history.size(); h++)
  {
    PixelModification &mod = history[h];

    ModificationValue preMod, postMod, shadout;
    const EventInfo &ei = eventsInfo[cb.m_EventIndices[mod.eventId]];
    preMod.col.floatValue[0] = (float)(ei.premod.color[2] / 255.0);
    preMod.col.floatValue[1] = (float)(ei.premod.color[1] / 255.0);
    preMod.col.floatValue[2] = (float)(ei.premod.color[0] / 255.0);
    preMod.col.floatValue[3] = (float)(ei.premod.color[3] / 255.0);
    postMod.col.floatValue[0] = (float)(ei.postmod.color[2] / 255.0);
    postMod.col.floatValue[1] = (float)(ei.postmod.color[1] / 255.0);
    postMod.col.floatValue[2] = (float)(ei.postmod.color[0] / 255.0);
    postMod.col.floatValue[3] = (float)(ei.shadout[3] / 255.0);
    shadout.col.floatValue[0] = (float)(ei.shadout[2] / 255.0);
    shadout.col.floatValue[1] = (float)(ei.shadout[1] / 255.0);
    shadout.col.floatValue[2] = (float)(ei.shadout[0] / 255.0);
    shadout.col.floatValue[3] = (float)(ei.shadout[3] / 255.0);
    preMod.depth = *((float *)ei.premod.depth);
    preMod.stencil = ei.premod.depth[4];
    postMod.depth = *((float *)ei.postmod.depth);
    postMod.stencil = ei.postmod.depth[4];

    mod.preMod = preMod;
    mod.shaderOut = shadout;
    mod.postMod = postMod;
  }

  m_pDriver->vkUnmapMemory(dev, resources.bufferMemory);
  GetDebugManager()->PixelHistoryDestroyResources(resources);
  ObjDisp(dev)->DestroyQueryPool(Unwrap(dev), occlusionPool, NULL);

  return history;
}
