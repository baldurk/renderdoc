/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Google LLC
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
#include "vk_cpp_codec_tracker.h"
#include "vk_cpp_codec_writer.h"

namespace vk_cpp_codec
{
const char *VkImageLayoutStrings[15] = {
    "VK_IMAGE_LAYOUT_UNDEFINED",
    "VK_IMAGE_LAYOUT_GENERAL",
    "VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL",
    "VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL",
    "VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL",
    "VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL",
    "VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL",
    "VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL",
    "VK_IMAGE_LAYOUT_PREINITIALIZED",
    "VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL",
    "VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL",
    "VK_IMAGE_LAYOUT_PRESENT_SRC_KHR",
    "VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR",
    "VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL_KHR",
    "VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL_KHR",
};

// ----------------------------------------------------------------------------
// The family of functions below manages the variable maps in various way.
// ----------------------------------------------------------------------------

#ifdef TT_VK_CALL_INTERNAL_SWITCH
#undef TT_VK_CALL_INTERNAL_SWITCH
#endif
#define TT_VK_CALL_INTERNAL_SWITCH(call, arg) \
  case(uint32_t)VulkanChunk::vk##call:        \
    call##Internal(arg);                      \
    break;

#ifdef TT_VK_CALL_ANALYZE_SWITCH
#undef TT_VK_CALL_ANALYZE_SWITCH
#endif
#define TT_VK_CALL_ANALYZE_SWITCH(call, arg) \
  case(uint32_t)VulkanChunk::vk##call: call##Analyze(arg); break;

const char *TraceTracker::GetVarFromMap(VariableIDMap &m, uint64_t id, const char *type,
                                        const char *name)
{
  VariableIDMapIter it = m.find(id);
  if(it != m.end())
    return it->second.name.c_str();
  // If resource id isn't found that means it wasn't declared in variable file
  // either, so print that variable out.
  it = m.insert(VariableIDMapPair(id, Variable(type, name))).first;
  code->AddNamedVar(type, name);
  return it->second.name.c_str();
}

const char *TraceTracker::GetVarFromMap(VariableIDMap &m, const char *type, const char *name,
                                        uint64_t id)
{
  std::string full_name = std::string(name) + std::string("_") + std::to_string(id);
  return GetVarFromMap(m, id, type, full_name.c_str());
}

const char *TraceTracker::GetVarFromMap(VariableIDMap &m, uint64_t id, std::string map_name)
{
  VariableIDMapIter it = m.find(id);
  if(it != m.end())
    return it->second.name.c_str();
  else
  {
    if(id == 0)
      return "NULL";    // This is reasonable, a resource can be NULL sometimes.
    // The serialized frame references this resource, but it was never created.
    RDCLOG("%llu is not found in %s map", id, map_name.c_str());
    // Return 'nullptr' specifically to differentiate between valid NULL
    // resource and a missing resources.
    return "nullptr";
  }
}

void TraceTracker::TrackVarInMap(VariableIDMap &m, const char *type, const char *name, uint64_t id)
{
  RDCASSERT(m.find(id) == m.end());
  m.insert(VariableIDMapPair(id, Variable(type, name)));
}

VariableIDMapIter TraceTracker::GetResourceVarIt(uint64_t id)
{
  return resources.find(id);
}

// Get a resource name from a resource map using the ID.
const char *TraceTracker::GetResourceVar(uint64_t id)
{
  return GetVarFromMap(resources, id, "resource variables");
}

// Get a resource name from the resources Map using the ID if it was already added.
// If it's a new variable, add it to the map and also print it to VAR files.
// The resulting variable will look like this: type name;
const char *TraceTracker::GetResourceVar(uint64_t id, const char *type, const char *name)
{
  return GetVarFromMap(resources, type, name, id);
}

// Get a resource name from a resources Map using the ID if it was already added.
// If it's a new variable, add it to the map and also print it to VAR files.
// The resulting variable will look like this: type name_id;
const char *TraceTracker::GetResourceVar(const char *type, const char *name, uint64_t id)
{
  return GetVarFromMap(resources, type, name, id);
}

// Get a resource name from a resources Map using the ID if it was already added.
// If it's a new variable, add it to the map and also print it to VAR files.
// The resulting variable will look like this: type type_id;
const char *TraceTracker::GetResourceVar(const char *type, uint64_t id)
{
  return GetVarFromMap(resources, type, type, id);
}

// Get a VkMemoryAllocateInfo variable name from a memAllocInfoMap using the ID.
// If it's a new variable, add it to the map and also print it to VAR files.
// The resulting variable will look like this: VkMemoryAllocateInfo VkMemoryAllocateInfo_id;
const char *TraceTracker::GetMemAllocInfoVar(uint64_t id, bool create)
{
  if(create)
    return GetVarFromMap(memAllocInfos, "VkMemoryAllocateInfo", "VkMemoryAllocateInfo", id);
  else
    return GetVarFromMap(memAllocInfos, id, "memory allocations");
}

const char *TraceTracker::GetDataBlobVar(uint64_t id)
{
  return GetVarFromMap(dataBlobs, "std::vector<uint8_t>", "buffer", id);
}

// Get a MemoryRemapVec variable name from a remapMap using the ID.
// If it's a new variable, add it to the map and also print it to VAR files.
// The resulting variable will look like this: MemoryRemapVec Remap_id;
const char *TraceTracker::GetMemRemapVar(uint64_t id)
{
  return GetVarFromMap(remapMap, "MemoryRemapVec", "Remap", id);
}

// Get a VkDeviceSize variable name from a resetSizeMap using the ID.
// If it's a new variable, add it to the map and also print it to VAR files.
// The resulting variable will look like this: VkDeviceSize ResetSize_id;
const char *TraceTracker::GetMemResetSizeVar(uint64_t id)
{
  return GetVarFromMap(resetSizeMap, "VkDeviceSize", "ResetSize", id);
}

// Get a VkDeviceSize variable name from a resetSizeMap using the ID.
// If it's a new variable, add it to the map and also print it to VAR files.
// The resulting variable will look like this: VkDeviceSize InitSize_id;
const char *TraceTracker::GetMemInitSizeVar(uint64_t id)
{
  return GetVarFromMap(initSizeMap, "VkDeviceSize", "InitSize", id);
}

// Get a ReplayedMemoryBindOffset variable name from a replayMemBindOffsetMap using the ID.
// If it's a new variable, add it to the map and also print it to VAR files.
// The resulting variable will look like this: VkDeviceSize ReplayedMemoryBindOffset_id;
const char *TraceTracker::GetReplayBindOffsetVar(uint64_t id)
{
  return GetVarFromMap(replayMemBindOffsets, "VkDeviceSize", "ReplayedMemoryBindOffset", id);
}

// Get a CapturedMemoryBindOffset variable name from a captureMemBindOffsetMap using the ID.
// If it's a new variable, add it to the map and also print it to VAR files.
// The resulting variable will look like this: VkDeviceSize CapturedMemoryBindOffset_id;
const char *TraceTracker::GetCaptureBindOffsetVar(uint64_t id)
{
  return GetVarFromMap(captureMemBindOffsets, "VkDeviceSize", "CapturedMemoryBindOffset", id);
}

// Get a VkMemoryRequirements variable name from a memRequirementsMap using the ID.
// If it's a new variable, add it to the map and also print it to VAR files.
// The resulting variable will look like this: VkMemoryRequirements VkMemoryRequirements_id;
const char *TraceTracker::GetMemReqsVar(uint64_t id)
{
  return GetVarFromMap(memRequirements, "VkMemoryRequirements", "VkMemoryRequirements", id);
}

ExtObject *TraceTracker::DescSetInfosFindLayout(uint64_t descSet_id)
{
  DescriptorSetInfoMapIter it = descriptorSetInfos.find(descSet_id);
  RDCASSERT(it != descriptorSetInfos.end());
  ResourceWithViewsMapIter res_it = ResourceCreateFind(it->second.layout);
  RDCASSERT(res_it != ResourceCreateEnd());
  return res_it->second.sdobj;
}

// SDObject of an array type will have elements that all have the same name "$el".
// This is not informative for the code generation and also C/C++ doesn't allow
// names to start with $. To fix this, I create a duplicate and replace the name,
// with the parent's name + array index and I serialize the duplicate instead.
// The duplicates are stored in a 'copies' array and have to be manually cleaned up.
ExtObject *TraceTracker::CopiesAdd(ExtObject *o, uint64_t i, std::string &suffix)
{
  ExtObject *node = o->At(i);
  if(node->name == "$el")
  {
    node = as_ext(o->At(i)->Duplicate());
    suffix = std::string("_") + std::to_string(i);
    node->name = o->Name();    // +suffix;
    copies.push_back(node);
  }
  return node;
}
void TraceTracker::CopiesClear()
{
  for(uint32_t i = 0; i < copies.size(); i++)
    delete copies[i];
  copies.clear();
}

bool TraceTracker::IsValidNonNullResouce(uint64_t id)
{
  bool variable_found = resources.find(id) != resources.end();
  bool resource_created = createdResources.find(id) != createdResources.end();
  return (id != 0 && (variable_found || resource_created));
}

bool TraceTracker::IsPresentationResource(uint64_t id)
{
  return presentResources.find(id) != presentResources.end();
}

ExtObject *TraceTracker::FramebufferPresentView(ExtObject *o)
{
  ExtObject *ci = o->At(1);
  for(uint64_t i = 0; i < ci->At(5)->Size(); i++)
  {
    if(IsPresentationResource(ci->At(5)->At(i)->U64()))
    {
      return ci->At(5)->At(i);
    }
  }
  return NULL;
}

void TraceTracker::ScanBinaryData(StructuredBufferList &buffers)
{
  for(size_t i = 0; i < buffers.size(); i++)
  {
    if(buffers[i]->size() == 0)
      continue;

    const char *name = GetVarFromMap(dataBlobs, "std::vector<uint8_t>", "buffer", i);
    std::string full_name = file_dir + "/" + "sample_cpp_trace" + "/" + name;
    FileIO::CreateParentDirectory(full_name);
    FILE *fbin = FileIO::fopen(full_name.c_str(), "wb");
    RDCASSERT(fbin != NULL);
    FileIO::fwrite(buffers[i]->data(), 1, buffers[i]->size(), fbin);
    FileIO::fclose(fbin);
  }
}

bool TraceTracker::IsEntireResource(ExtObject *image, ExtObject *subres)
{
  ExtObject *image_ci = image->At(1);

  bool same_mips = subres->At(1)->U64() == 0 && (subres->At(2)->U64() == VK_REMAINING_MIP_LEVELS ||
                                                 subres->At(2)->U64() == image_ci->At(6)->U64());

  if(!same_mips)
    return false;

  bool same_layers = subres->At(3)->U64() == 0 && (subres->At(4)->U64() == VK_REMAINING_ARRAY_LAYERS ||
                                                   subres->At(4)->U64() == image_ci->At(7)->U64());

  if(!same_layers)
    return false;

  return true;
}

uint64_t TraceTracker::CurrentQueueFamily()
{
  ExtObjectIDMapIter queue_it = deviceQueues.find(cmdQueue);
  if(queue_it == deviceQueues.end())
  {
    return VK_QUEUE_FAMILY_IGNORED;
  }
  return queue_it->second->At("queueFamilyIndex")->U64();
}

// --------------------------------------------------------------------------
// Vulkan API specific tracking functions called on ScanSerializedInfo to
// track resource state across the frame
// --------------------------------------------------------------------------
void TraceTracker::ApplyMemoryUpdate(ExtObject *ext)
{
  RDCASSERT(ext->ChunkID() == (uint32_t)VulkanChunk::vkFlushMappedMemoryRanges);

  ExtObject *range = ext->At("MemRange");
  ExtObject *memory = range->At("memory");
  uint64_t offset = range->At("offset")->U64();
  uint64_t size = range->At("size")->U64();

  MemAllocWithResourcesMapIter it = MemAllocFind(memory->U64());
  it->second.Access(VK_QUEUE_FAMILY_IGNORED, VK_SHARING_MODE_CONCURRENT, ACCESS_ACTION_CLEAR,
                    offset, size);
}

void TraceTracker::ApplyDescSetUpdate(ExtObject *ext)
{
  ExtObject *descriptorWrites = NULL;
  if(ext->ChunkID() == (uint32_t)VulkanChunk::vkUpdateDescriptorSets)
    descriptorWrites = ext->At(2);
  if(ext->ChunkID() == (uint32_t)VulkanChunk::vkUpdateDescriptorSetWithTemplate)
    descriptorWrites = ext->At(3);
  RDCASSERT(descriptorWrites != NULL);
  for(uint64_t i = 0; i < descriptorWrites->Size(); i++)
  {
    WriteDescriptorSetInternal(descriptorWrites->At(i));
  }

  if(ext->ChunkID() == (uint32_t)VulkanChunk::vkUpdateDescriptorSets)
  {
    ExtObject *descriptorCopies = ext->At(4);
    for(uint64_t i = 0; i < descriptorCopies->Size(); i++)
    {
      CopyDescriptorSetInternal(descriptorCopies->At(i));
    }
  }
}

void TraceTracker::AddCommandBufferToFrameGraph(ExtObject *o)
{
  uint32_t i = fg.FindCmdBufferIndex(o->At(0));
  fg.records[i].cmds.push_back(o);
}

void TraceTracker::AnalyzeInitResources()
{
  uint64_t memory_updates = 0;
  uint64_t descset_updates = 0;
  for(QueueSubmitsIter qs = fg.submits.begin(); qs != fg.submits.end(); qs++)
  {
    cmdQueue = qs->q->U64();

    ExtObjectIDMapIter queue_it = deviceQueues.find(cmdQueue);
    RDCASSERT(queue_it != deviceQueues.end());
    cmdQueueFamily = queue_it->second->At("queueFamilyIndex")->U64();

    for(; memory_updates < qs->memory_updates; memory_updates++)
    {
      ApplyMemoryUpdate(fg.updates.memory[memory_updates]);
    }
    for(; descset_updates < qs->descset_updates; descset_updates++)
    {
      ApplyDescSetUpdate(fg.updates.descset[descset_updates]);
    }
    ExtObject *submitInfos = qs->sdobject->At(2);
    for(uint64_t i = 0; i < submitInfos->Size(); i++)
    {
      ExtObject *submitInfo = submitInfos->At(i);
      ExtObject *commandBuffers = submitInfo->At(6);
      for(uint64_t j = 0; j < commandBuffers->Size(); j++)
      {
        uint32_t recordIndex = fg.FindCmdBufferIndex(commandBuffers->At(j));
        CmdBufferRecord &record = fg.records[recordIndex];
        for(uint64_t k = 0; k < record.cmds.size(); k++)
        {
          AnalyzeCmd(record.cmds[k]);
        }
        // Reset the binding state at the end of the command buffer
        bindingState = BindingState();
      }
    }
  }
}

void TraceTracker::AnalyzeCmd(ExtObject *ext)
{
  switch(ext->ChunkID())
  {
    // Image related functions
    TT_VK_CALL_ANALYZE_SWITCH(CmdBeginRenderPass, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdNextSubpass, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdExecuteCommands, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdEndRenderPass, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdCopyImage, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdBlitImage, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdResolveImage, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdClearColorImage, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdClearDepthStencilImage, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdClearAttachments, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdCopyBufferToImage, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdCopyImageToBuffer, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdPipelineBarrier, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdWaitEvents, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdBindDescriptorSets, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdBindIndexBuffer, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdBindVertexBuffers, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdCopyBuffer, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdUpdateBuffer, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdFillBuffer, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdDispatch, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdDispatchIndirect, ext);
    // Draw functions
    TT_VK_CALL_ANALYZE_SWITCH(CmdDrawIndirect, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdDrawIndexedIndirect, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdDraw, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdDrawIndexed, ext);
    TT_VK_CALL_ANALYZE_SWITCH(CmdBindPipeline, ext);

    case(uint32_t)VulkanChunk::vkEndCommandBuffer:
      // TODO: do we actually need to do anything here?
      break;

    default:
      // Make sure we are actually handling all the commands that get sent here by
      // `AddCommandBufferToFrameGraph` in `ScanSerializeInfo`.
      RDCASSERT(0);
  }
}

void TraceTracker::AnalyzeMemoryAllocations()
{
  // ma_it: memory allocation iterator.
  for(MemAllocWithResourcesMapIter ma_it = MemAllocBegin(); ma_it != MemAllocEnd(); ma_it++)
  {
    // For each bound resource check if it's memory range overlaps with
    // any previously bound resources to determine if resource aliasing
    // takes place.
    // br_it: bound resource iterator.
    for(BoundResourcesIter br_it = ma_it->second.FirstBoundResource();
        br_it != ma_it->second.EndOfBoundResources(); br_it++)
    {
      MemRange range;
      range.MakeRange(br_it->offset, br_it->requirement);
      if(ma_it->second.CheckAliasedResources(range))
        break;
    }
  }
}

void TraceTracker::SaveInitialLayout(ExtObject *image, ExtObject *layouts)
{
  RDCASSERT(image != NULL && layouts != NULL);
  uint64_t imageID = image->U64();

  ImageStateMapIter imageState_it = imageStates.find(imageID);
  if(imageState_it == imageStates.end())
  {
    // Apparently, RenderDoc's "Beginning of Capture" chunk can include images
    // that don't have corresponding vkCreateImage or vkGetSwapchainImages chunks.
    return;
  }
  ImageState &imageState = imageState_it->second;

  ExtObject *subresources = layouts->At("subresourceStates");
  uint64_t queueFamily = layouts->Exists("queueFamilyIndex") ? layouts->At("queueFamilyIndex")->U64()
                                                             : VK_QUEUE_FAMILY_IGNORED;

  for(uint64_t i = 0; i < subresources->Size(); i++)
  {
    ExtObject *subres = subresources->At(i);
    ExtObject *range = subres->At("subresourceRange");
    uint64_t baseMip = range->At("baseMipLevel")->U64();
    uint64_t levelCount = range->At("levelCount")->U64();
    uint64_t baseLayer = range->At("baseArrayLayer")->U64();
    uint64_t layerCount = range->At("layerCount")->U64();
    VkImageAspectFlags aspectMask = (VkImageAspectFlags)range->At("aspectMask")->U64();
    VkImageLayout layout = (VkImageLayout)subres->At("newLayout")->U64();
    uint64_t dstQueueFamily = subres->Exists("dstQueueFamilyIndex")
                                  ? (VkImageLayout)subres->At("dstQueueFamilyIndex")->U64()
                                  : VK_QUEUE_FAMILY_IGNORED;
    ImageSubresourceRange imageRange =
        imageState.Range(aspectMask, baseMip, levelCount, baseLayer, layerCount);
    for(ImageSubresourceRangeIter res_it = imageRange.begin(); res_it != imageRange.end(); res_it++)
    {
      if(dstQueueFamily != VK_QUEUE_FAMILY_IGNORED)
      {
        // There are queue family indexes stored in both `layouts` (for the whole image) and in each
        // subresource.
        // So far, the queue family for subresources is always VK_QUEUE_FAMILY_IGNORED. If this is
        // ever not true, we need to understand what is happening.
        RDCWARN(
            "BeginCapture includes an image subresource with a dstQueueFamilyIndex. This is "
            "completely untested. Please let us know what breaks "
            "(with a capture that reproduces it, if possible).");
      }
      imageState.At(*res_it).Initialize(layout, queueFamily);
    }
  }
}

bool TraceTracker::ResourceNeedsReset(uint64_t resourceID, bool forInit, bool forReset)
{
  if(!(forInit || forReset))
  {
    return false;
  }
  InitResourceIDMapIter init_res_it = InitResourceFind(resourceID);
  if(init_res_it == InitResourceEnd())
  {
    // Nothing to reset the resource to. Assume we don't need to reset.
    return false;
  }

  switch(init_res_it->second.sdobj->At(0)->U64())
  {
    case VkResourceType::eResDeviceMemory:
    {
      MemAllocWithResourcesMapIter mem_it = MemAllocFind(resourceID);
      return (forInit && mem_it->second.NeedsInit()) || (forReset && mem_it->second.NeedsReset());
    }
    case VkResourceType::eResImage:
    {
      if(forInit && ((optimizations & CODE_GEN_OPT_IMAGE_INIT_BIT) == 0))
      {
        return true;
      }
      if(forReset && ((optimizations & CODE_GEN_OPT_IMAGE_RESET_BIT) == 0))
      {
        return true;
      }
      ResourceWithViewsMapIter create_it = ResourceCreateFind(resourceID);
      if(create_it == ResourceCreateEnd())
      {
        RDCASSERT(0);    // TODO: should this be happening?
        return true;
      }
      bool needsReset = false;
      bool needsInit = false;

      ImageStateMapIter imageState_it = imageStates.find(resourceID);
      RDCASSERT(imageState_it != imageStates.end());
      ImageState &imageState = imageState_it->second;

      for(ImageSubresourceStateMapIter it = imageState.begin(); it != imageState.end(); it++)
      {
        switch(it->second.AccessState())
        {
          case ACCESS_STATE_READ:
            // some part of the initial value could be read, so initialization is required
            needsInit = true;
            break;
          case ACCESS_STATE_RESET:
            // some part of the initial value could be read, and then written, so reset is required
            needsReset = true;
            break;
          default: break;
        }
      }

      // If the image is reset, it is redundant to also initialize
      needsInit = needsInit && (!needsReset);

      return ((forInit && needsInit) || (forReset && needsReset));
    }
    default: RDCASSERT(0); return true;
  }
}

void TraceTracker::ScanResourceCreation(StructuredChunkList &chunks, StructuredBufferList &buffers)
{
  for(size_t c = 0; c < chunks.size(); c++)
  {
    ExtObject *ext = as_ext(chunks[c]);
    switch(ext->ChunkID())
    {
      case(uint32_t)VulkanChunk::vkCreateBuffer:
      case(uint32_t)VulkanChunk::vkCreateImage: CreateResourceInternal(ext); break;
      case(uint32_t)VulkanChunk::vkCreateBufferView:
      case(uint32_t)VulkanChunk::vkCreateImageView:
        CreateResourceViewInternal(ext);
        break;
        TT_VK_CALL_INTERNAL_SWITCH(CreateDevice, ext);
        TT_VK_CALL_INTERNAL_SWITCH(GetDeviceQueue, ext);
        TT_VK_CALL_INTERNAL_SWITCH(AllocateMemory, ext);
        TT_VK_CALL_INTERNAL_SWITCH(CreateFramebuffer, ext);
        TT_VK_CALL_INTERNAL_SWITCH(BindBufferMemory, ext);
        TT_VK_CALL_INTERNAL_SWITCH(BindImageMemory, ext);
        TT_VK_CALL_INTERNAL_SWITCH(CreateSampler, ext);
        TT_VK_CALL_INTERNAL_SWITCH(CreateShaderModule, ext);
        TT_VK_CALL_INTERNAL_SWITCH(CreateSwapchainKHR, ext);
        TT_VK_CALL_INTERNAL_SWITCH(GetSwapchainImagesKHR, ext);
        TT_VK_CALL_INTERNAL_SWITCH(CreatePipelineCache, ext);
        TT_VK_CALL_INTERNAL_SWITCH(CreateRenderPass, ext);
        TT_VK_CALL_INTERNAL_SWITCH(CreateDescriptorSetLayout, ext);
        TT_VK_CALL_INTERNAL_SWITCH(AllocateDescriptorSets, ext);
        TT_VK_CALL_INTERNAL_SWITCH(CreateDescriptorPool, ext);
        TT_VK_CALL_INTERNAL_SWITCH(CreateDescriptorUpdateTemplate, ext);
        TT_VK_CALL_INTERNAL_SWITCH(CreateCommandPool, ext);
        TT_VK_CALL_INTERNAL_SWITCH(AllocateCommandBuffers, ext);
        TT_VK_CALL_INTERNAL_SWITCH(CreatePipelineLayout, ext);
        TT_VK_CALL_INTERNAL_SWITCH(CreateGraphicsPipelines, ext);
        TT_VK_CALL_INTERNAL_SWITCH(CreateComputePipelines, ext);
        TT_VK_CALL_INTERNAL_SWITCH(EnumeratePhysicalDevices, ext);
      default: break;
    }
  }
}

void TraceTracker::ScanQueueSubmits(StructuredChunkList &chunks)
{
  for(size_t c = 0; c < chunks.size(); c++)
  {
    ExtObject *ext = as_ext(chunks[c]);
    switch(ext->ChunkID())
    {
      TT_VK_CALL_INTERNAL_SWITCH(FlushMappedMemoryRanges, ext);
      TT_VK_CALL_INTERNAL_SWITCH(UpdateDescriptorSets, ext);
      TT_VK_CALL_INTERNAL_SWITCH(UpdateDescriptorSetWithTemplate, ext);
      TT_VK_CALL_INTERNAL_SWITCH(BeginCommandBuffer, ext);
      TT_VK_CALL_INTERNAL_SWITCH(EndCommandBuffer, ext);
      TT_VK_CALL_INTERNAL_SWITCH(QueueSubmit, ext);
      TT_VK_CALL_INTERNAL_SWITCH(WaitForFences, ext);
      case(uint32_t)VulkanChunk::vkCmdBeginRenderPass:
      case(uint32_t)VulkanChunk::vkCmdNextSubpass:
      case(uint32_t)VulkanChunk::vkCmdExecuteCommands:
      case(uint32_t)VulkanChunk::vkCmdEndRenderPass:
      case(uint32_t)VulkanChunk::vkCmdCopyImage:
      case(uint32_t)VulkanChunk::vkCmdBlitImage:
      case(uint32_t)VulkanChunk::vkCmdResolveImage:
      case(uint32_t)VulkanChunk::vkCmdClearColorImage:
      case(uint32_t)VulkanChunk::vkCmdClearDepthStencilImage:
      case(uint32_t)VulkanChunk::vkCmdClearAttachments:
      case(uint32_t)VulkanChunk::vkCmdCopyBufferToImage:
      case(uint32_t)VulkanChunk::vkCmdCopyImageToBuffer:
      case(uint32_t)VulkanChunk::vkCmdPipelineBarrier:
      case(uint32_t)VulkanChunk::vkCmdWaitEvents:
      case(uint32_t)VulkanChunk::vkCmdBindDescriptorSets:
      case(uint32_t)VulkanChunk::vkCmdBindIndexBuffer:
      case(uint32_t)VulkanChunk::vkCmdBindVertexBuffers:
      case(uint32_t)VulkanChunk::vkCmdCopyBuffer:
      case(uint32_t)VulkanChunk::vkCmdUpdateBuffer:
      case(uint32_t)VulkanChunk::vkCmdFillBuffer:
      case(uint32_t)VulkanChunk::vkCmdDispatch:
      case(uint32_t)VulkanChunk::vkCmdDispatchIndirect:
      case(uint32_t)VulkanChunk::vkCmdDrawIndirect:
      case(uint32_t)VulkanChunk::vkCmdDrawIndexedIndirect:
      case(uint32_t)VulkanChunk::vkCmdDraw:
      case(uint32_t)VulkanChunk::vkCmdDrawIndexed:
      case(uint32_t)VulkanChunk::vkCmdBindPipeline: AddCommandBufferToFrameGraph(ext); break;
      default: break;
    }
  }
}

void TraceTracker::ScanInitialContents(StructuredChunkList &chunks)
{
  for(size_t c = 0; c < chunks.size(); c++)
  {
    switch(chunks[c]->metadata.chunkID)
    {
      case(uint32_t)SystemChunk::CaptureBegin:
        // Initial Layouts are processed during ScanFilter because
        // it relies on processing InitialContents before initial layouts and
        // InitialContents is processed in this Scan function.
        InitialLayoutsInternal(as_ext(chunks[c]));
        break;
      case(uint32_t)SystemChunk::InitialContents: InitialContentsInternal(as_ext(chunks[c])); break;
      default: break;
    }
  }
}

void TraceTracker::ScanFilter(StructuredChunkList &chunks)
{
  for(size_t c = 0; c < chunks.size();)
  {
    ExtObject *ext = as_ext(chunks[c]);
    switch(ext->ChunkID())
    {
      case(uint32_t)SystemChunk::InitialContents:
      {
        if(ext->At(0)->U64() == VkResourceType::eResDescriptorSet && !FilterInitDescSet(ext))
        {
          chunks.removeOne(chunks[c]);
        }
        else
        {
          c++;
        }
      }
      break;
      case(uint32_t)VulkanChunk::vkUpdateDescriptorSets:
      {
        if(!FilterUpdateDescriptorSets(ext))
        {
          chunks.removeOne(chunks[c]);
        }
        else
        {
          c++;
        }
      }
      break;
      case(uint32_t)VulkanChunk::vkUpdateDescriptorSetWithTemplate:
        if(!FilterUpdateDescriptorSetWithTemplate(as_ext(chunks[c])))
          chunks.removeOne(chunks[c]);
        else
          c++;
        break;
      case(uint32_t)VulkanChunk::vkCreateImage: FilterCreateImage(as_ext(chunks[c++])); break;
      case(uint32_t)VulkanChunk::vkCreateGraphicsPipelines:
        FilterCreateGraphicsPipelines(as_ext(chunks[c++]));
        break;
      case(uint32_t)VulkanChunk::vkCreateComputePipelines:
        FilterCreateComputePipelines(as_ext(chunks[c++]));
        break;
      case(uint32_t)VulkanChunk::vkCmdCopyImageToBuffer:
        FilterCmdCopyImageToBuffer(as_ext(chunks[c++]));
        break;
      case(uint32_t)VulkanChunk::vkCmdCopyImage: FilterCmdCopyImage(as_ext(chunks[c++])); break;
      case(uint32_t)VulkanChunk::vkCmdBlitImage: FilterCmdBlitImage(as_ext(chunks[c++])); break;
      case(uint32_t)VulkanChunk::vkCmdResolveImage:
        FilterCmdResolveImage(as_ext(chunks[c++]));
        break;
      case(uint32_t)VulkanChunk::vkCreateDevice: FilterCreateDevice(as_ext(chunks[c++])); break;
      case (uint32_t) VulkanChunk::vkCmdPipelineBarrier:
        if (!FilterCmdPipelineBarrier(as_ext(chunks[c])))
          chunks.removeOne(chunks[c]);
        else
          c++;
        break;
      default: c++; break;
    }
  }
}

void TraceTracker::AnalyzeMemoryResetRequirements()
{
  for(MemAllocWithResourcesMapIter ma_it = MemAllocBegin(); ma_it != MemAllocEnd(); ma_it++)
  {
    MemoryAllocationWithBoundResources &mem = ma_it->second;

    for(BoundResourcesIter br_it = mem.FirstBoundResource(); br_it != mem.EndOfBoundResources();
        br_it++)
    {
      BoundResource &abr = *br_it;
      abr.reset = RESET_REQUIREMENT_NO_RESET;

      switch(abr.bindSDObj->ChunkID())
      {
        case(uint32_t)VulkanChunk::vkBindImageMemory:
        {
          ExtObject *image_ci = abr.createSDObj->At(1);
          VkImageLayout initialLayout = (VkImageLayout)image_ci->At(14)->U64();
          if(initialLayout == VK_IMAGE_LAYOUT_PREINITIALIZED)
          {
            abr.reset = RESET_REQUIREMENT_INIT;
          }
          if((optimizations & CODE_GEN_OPT_IMAGE_MEMORY_BIT) == 0)
          {
            abr.reset = RESET_REQUIREMENT_INIT;
          }
          break;
        }
        case(uint32_t)VulkanChunk::vkBindBufferMemory:
        {
          MemRange range;
          range.MakeRange(abr.offset, abr.requirement);
          for(IntervalsIter<MemoryState> it = mem.memoryState.find(range.start);
              it != mem.memoryState.end() && it.start() < range.end; it++)
          {
            switch(it.value().accessState)
            {
              case ACCESS_STATE_READ:
                abr.reset = std::min(RESET_REQUIREMENT_INIT, abr.reset);
                break;
              case ACCESS_STATE_RESET:
                abr.reset = std::min(RESET_REQUIREMENT_RESET, abr.reset);
                break;
              default: break;
            }
          }
          if((optimizations & CODE_GEN_OPT_BUFFER_INIT_BIT) == 0)
          {
            abr.reset = std::min(RESET_REQUIREMENT_INIT, abr.reset);
          }
          if((optimizations & CODE_GEN_OPT_BUFFER_RESET_BIT) == 0)
          {
            abr.reset = std::min(RESET_REQUIREMENT_RESET, abr.reset);
          }
          break;
        }
        default: RDCASSERT(0);
      }
    }
  }
}

// TODO(akharlamov): Should this also filter out the semaphores / fences
// that are never signaled / waited on in a frame? Should this filter out
// invalid desc set, clean resource references and such?
void TraceTracker::Scan(StructuredChunkList &chunks, StructuredBufferList &buffers)
{
  ScanResourceCreation(chunks, buffers);
  ScanFilter(chunks);
  ScanInitialContents(chunks);
  ScanQueueSubmits(chunks);
  ScanBinaryData(buffers);
  AnalyzeMemoryAllocations();
  AnalyzeInitResources();
  AnalyzeMemoryResetRequirements();
}

}    // namespace vk_cpp_codec
