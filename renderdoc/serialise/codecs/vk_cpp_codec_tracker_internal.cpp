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
void TraceTracker::EnumeratePhysicalDevicesInternal(ExtObject *o)
{
  RDCASSERT(o->Size() == 9 && queueFamilyCount == 0);
  queueFamilyCount = o->At("queueCount")->U64();
  ExtObject *queueFamilyProps = o->At("queueProps");
  queueUsed.resize(queueFamilyCount);
  RDCASSERT(queueFamilyCount <= queueFamilyProps->Size());
  for(size_t i = 0; i < queueFamilyCount; i++)
  {
    uint64_t queueCount = queueFamilyProps->At(i)->At("queueCount")->U64();
    queueUsed[i].resize((size_t)queueCount);
  }
}

void TraceTracker::CreateDeviceInternal(ExtObject *o)
{
  // Only allow this once.
  RDCASSERT(PhysDevID() == 0 && DeviceID() == 0);
  PhysDevID(o->At(0)->U64());
  DeviceID(o->At(3)->U64());
  ExtObject *ci = o->At(1);
  ExtObject *extensionCount = ci->At(7);
  ExtObject *extensions = ci->At(8);
  std::string debug_marker("VK_EXT_debug_marker");
  for(uint64_t i = 0; i < extensions->Size(); i++)
  {
    if(debug_marker == extensions->At(i)->Str())
    {
      extensions->RemoveOne(i);
      break;
    }
  }
  extensionCount->U64() = extensions->Size();
  TrackVarInMap(resources, "VkSemaphore", "aux.semaphore", ACQUIRE_SEMAPHORE_VAR_ID);

  queueFamilyPropertiesStr = code->MakeVarName("VkQueueFamilyProperties", PhysDevID());
}

void TraceTracker::GetDeviceQueueInternal(ExtObject *o)
{
  uint64_t queueFamilyIndex = o->At("queueFamilyIndex")->U64();
  uint64_t queueIndex = o->At("queueIndex")->U64();
  RDCASSERT(queueFamilyIndex < queueUsed.size());
  RDCASSERT(queueIndex < queueUsed[queueFamilyIndex].size());
  queueUsed[queueFamilyIndex][queueIndex] = true;
  uint64_t queue = o->At("Queue")->U64();
  bool inserted = deviceQueues.insert(std::make_pair(queue, o)).second;
  RDCASSERT(inserted);
}

void TraceTracker::AllocateMemoryInternal(ExtObject *o)
{
  MemoryAllocationWithBoundResources mawbr(o);
  MemAllocAdd(o->At(3)->U64(), mawbr);
}

ResourceWithViewsMapIter TraceTracker::GenericCreateResourceInternal(ExtObject *o)
{
  // Using At(3) here because many Vulkan functions that create resources have the same
  // signature, where a Vulkan resource is the 4th argument in the function call.
  RDCASSERT(o->Size() >= 4 && o->At(3)->IsResource());
  uint64_t resource_id = o->At(3)->U64();
  ResourceWithViews rwv = {o};
  ResourceWithViewsMapIter it =
      createdResources.insert(ResourceWithViewsMapPair(resource_id, rwv)).first;

  if(o->ChunkID() == (uint32_t)VulkanChunk::vkCreateImage)
  {
    imageStates.insert(
        ImageStateMapPair(resource_id, ImageState(resource_id, o->At("CreateInfo"))));
  }
  return it;
}

void TraceTracker::CreateResourceInternal(ExtObject *o)
{
  GenericCreateResourceInternal(o);
  ExtObject *mem_reqs = o->At(4);
  mem_reqs->name = code->MakeVarName("VkMemoryRequirements", o->At(3)->U64());
}

void TraceTracker::CreateResourceViewInternal(ExtObject *o)
{
  ExtObject *ci = o->At(1);
  ExtObject *resource = ci->At(3);
  ExtObject *view = o->At(3);
  ResourceWithViewsMapIter res_it = ResourceCreateFind(resource->U64());
  if(res_it != ResourceCreateEnd())
  {    // this can fail, for example for swapchain images
    res_it->second.views.insert(ExtObjectIDMapPair(view->U64(), o));
    GenericCreateResourceInternal(o);
    return;
  }
  ExtObjectIDMapIter present_it = presentResources.find(resource->U64());
  if(present_it != presentResources.end())
  {
    GenericCreateResourceInternal(o);
    presentResources.insert(ExtObjectIDMapPair(view->U64(), o));
    presentResources.insert(ExtObjectIDMapPair(view->U64() + PRESENT_VARIABLE_OFFSET, o));
    return;
  }

  {
    RDCWARN("Resource wasn't found in createdResource or presentResources");
  }
}

void TraceTracker::BindResourceMemoryHelper(ExtObject *o)
{
  MemAllocWithResourcesMapIter mem_it = MemAllocFind(o->At(2)->U64());
  ResourceWithViewsMapIter create_it = ResourceCreateFind(o->At(1)->U64());
  RDCASSERT(create_it != ResourceCreateEnd());
  BoundResource br = {/* create call SDObject    */ create_it->second.sdobj,
                      /* bind call SDObject      */ o,
                      /* serialized resource ID  */ o->At(1),
                      /* serialized requirements */ ResourceCreateFindMemReqs(o->At(1)->U64()),
                      /* serialized offset       */ o->At(3)};
  mem_it->second.Add(br);    // Add buffer or image to the list of bound resources
}

void TraceTracker::BindBufferMemoryInternal(ExtObject *o)
{
  BindResourceMemoryHelper(o);
  uint64_t buf_id = o->At(1)->U64();
  uint64_t mem_id = o->At(2)->U64();
  ResourceWithViewsMapIter r_it = ResourceCreateFind(buf_id);
  if(r_it != ResourceCreateEnd())
  {
    r_it->second.views.insert(ExtObjectIDMapPair(mem_id, o));
  }
}

void TraceTracker::BindImageMemoryInternal(ExtObject *o)
{
  BindResourceMemoryHelper(o);
}

void TraceTracker::CreateRenderPassInternal(ExtObject *o)
{
  GenericCreateResourceInternal(o);
  // Is this render pass presenting?
  ExtObject *attachments = o->At(1)->At(4);
  for(uint32_t a = 0; a < attachments->Size(); a++)
  {
    if(attachments->At(a)->At(8)->U64() == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
      ExtObject *renderpass = o->At(3);
      presentResources.insert(ExtObjectIDMapPair(renderpass->U64(), o));
      break;
    }
  }
}

void TraceTracker::CreatePipelineLayoutInternal(ExtObject *o)
{
  GenericCreateResourceInternal(o);
}

void TraceTracker::CreateGraphicsPipelinesInternal(ExtObject *o)
{
  uint64_t createInfoCount = o->At(2)->U64();
  ExtObject *pipeline = o->At(5);
  RDCASSERT(createInfoCount == 1);    // `createInfo` and `pipeline` are not serialized as arrays;
                                      // if this fails, figure out how renderdoc is handling that
                                      // case.
  createdPipelines.insert(ExtObjectIDMapPair(pipeline->U64(), o));
}

void TraceTracker::CreateComputePipelinesInternal(ExtObject *o)
{
  uint64_t createInfoCount = o->At(2)->U64();
  ExtObject *pipeline = o->At(5);
  RDCASSERT(createInfoCount == 1);    // `createInfo` and `pipeline` are not serialized as arrays;
                                      // if this fails, figure out how renderdoc is handling that
                                      // case.
  createdPipelines.insert(ExtObjectIDMapPair(pipeline->U64(), o));
}

void TraceTracker::CreateFramebufferInternal(ExtObject *o)
{
  ExtObject *ci = o->At(1);
  // Add create framebuffer call to createResource map.
  ResourceWithViewsMapIter fb_it = GenericCreateResourceInternal(o);

  // add renderpass here to link framebuffer with renderpass.
  ExtObject *renderpass = ci->At(3);
  ResourceWithViewsMapIter rp_it = ResourceCreateFind(renderpass->U64());
  RDCASSERT(rp_it != ResourceCreateEnd());
  fb_it->second.views.insert(ExtObjectIDMapPair(renderpass->U64(), rp_it->second.sdobj));

  // look at all the attachments, find view IDs, and link fb with image views
  for(uint64_t i = 0; i < ci->At(5)->Size(); i++)
  {
    ExtObject *attach = ci->At(5)->At(i);
    ResourceWithViewsMapIter view_it = ResourceCreateFind(attach->U64());
    if(view_it != ResourceCreateEnd())
    {    // this can fail, for example swapchain image view isn't in the map
         // add image views that are used as attachments.
      fb_it->second.views.insert(ExtObjectIDMapPair(attach->U64(), view_it->second.sdobj));
    }
  }
}

void TraceTracker::CreateSamplerInternal(ExtObject *o)
{
  GenericCreateResourceInternal(o);
}

void TraceTracker::CreateShaderModuleInternal(ExtObject *o)
{
  ExtObject *ci = o->At(1);
  ExtObject *buffer = ci->At(4);
  buffer->data.str = GetVarFromMap(dataBlobs, "std::vector<uint8_t>", "shader", buffer->U64());
}

void TraceTracker::CreateSwapchainKHRInternal(ExtObject *o)
{
  RDCASSERT(swapchainID == 0);    // this should only happen once.
  ExtObject *ci = o->At("CreateInfo");
  swapchainCreateInfo = ci;
  ExtObject *swapchain = o->At("SwapChain");
  swapchainID = swapchain->U64();
  swapchainCount = ci->At("minImageCount")->U64();
  ExtObject *extent = ci->At("imageExtent");
  swapchainWidth = extent->At("width")->U64();
  swapchainHeight = extent->At("height")->U64();
  presentImageIndex.resize(swapchainCount);
  swapchainCountStr = std::string("PresentImageCount_") + std::to_string(swapchain->U64());
  presentImagesStr = std::string("PresentImages_") + std::to_string(swapchain->U64());

  TrackVarInMap(resources, "VkImage", (presentImagesStr + std::string("[acquired_frame]")).c_str(),
                PRESENT_IMAGE_OFFSET);
}

void TraceTracker::GetSwapchainImagesKHRInternal(ExtObject *o)
{
  static uint32_t count = 0;
  ExtObject *swapchainIdx = o->At("SwapchainImageIndex");
  ExtObject *image = o->At("SwapchainImage");
  uint64_t imageID = image->U64();

  RDCASSERT(swapchainCount > 0 && count < swapchainCount && swapchainIdx->U64() < swapchainCount);
  // Keep track that this image with ID is at swapchain_index location
  presentImageIndex[swapchainIdx->U64()] = image;

  imageStates.insert(ImageStateMapPair(imageID, ImageState(imageID, swapchainCreateInfo)));

  // Add the image to the list of swapchain images, We'll be looking for these resources
  // in framebuffer attachments so that we can figure out which one needs to be presented.
  presentResources.insert(ExtObjectIDMapPair(imageID, o));
  count++;
}

void TraceTracker::CreatePipelineCacheInternal(ExtObject *o)
{
  ExtObject *ci = o->At(1);
  ExtObject *buffer = ci->At(4);
  buffer->data.str =
      GetVarFromMap(dataBlobs, "std::vector<uint8_t>", "pipeline_cache", buffer->U64());
}

void TraceTracker::FlushMappedMemoryRangesInternal(ExtObject *o)
{
  fg.updates.memory.push_back(o);
}

void TraceTracker::UpdateDescriptorSetWithTemplateInternal(ExtObject *o)
{
  fg.updates.descset.push_back(o);
}

void TraceTracker::UpdateDescriptorSetsInternal(ExtObject *o)
{
  fg.updates.descset.push_back(o);
}

void TraceTracker::InitDescriptorSetInternal(ExtObject *o)
{
  uint64_t descriptorSet_id = o->At(1)->U64();
  InitResourceIDMapIter it = InitResourceFind(descriptorSet_id);

  DescriptorSetInfoMapIter descriptorSet_it = descriptorSetInfos.find(descriptorSet_id);
  RDCASSERT(descriptorSet_it != descriptorSetInfos.end());
  DescriptorSetInfo &descriptorSet = descriptorSet_it->second;

  ExtObject *initBindings = o->At(2);
  if(initBindings->Size() == 0)
    return;

  for(uint32_t i = 0; i < initBindings->Size(); i++)
  {
    ExtObject *initBinding = initBindings->At(i);
    RDCASSERT(initBinding->Size() == 6);
    uint64_t binding = initBinding->At(3)->U64();
    uint64_t type = initBinding->At(4)->U64();
    uint64_t element = initBinding->At(5)->U64();

    switch(type)
    {
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
      case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
      case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        descriptorSet.bindings[binding].SetBindingObj(element, initBinding->At(0), true);
        break;

      case VK_DESCRIPTOR_TYPE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
      case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
      case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
      case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        if(IsPresentationResource(initBinding->At(1)->At(1)->U64()))
        {
          // Desc sets that include presentation resources always have to be reset
          // because they rely on correctly setting an '[acquired_frame]' imageview.
          descriptorSet.bindings[binding].updated[element] = true;
        }
        descriptorSet.bindings[binding].SetBindingObj(element, initBinding->At(1), true);
        break;

      case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
      case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        descriptorSet.bindings[binding].SetBindingObj(element, initBinding->At(2), true);
        break;

      default: RDCASSERT(0);
    }
  }
}

void TraceTracker::InitialLayoutsInternal(ExtObject *o)
{
  RDCASSERT(o->ChunkID() == (uint32_t)SystemChunk::CaptureBegin);
  RDCASSERT(o->At(0)->U64() > 0);
  for(uint64_t i = 0; i < o->At("NumImages")->U64(); i++)
  {
    SaveInitialLayout(o->At(i * 2 + 1), o->At(i * 2 + 2));
  }
}

void TraceTracker::WriteDescriptorSetInternal(ExtObject *wds)
{
  uint64_t descSet = wds->At(2)->U64();
  uint64_t descSetBinding = wds->At(3)->U64();

  DescriptorSetInfoMapIter descriptorSet_it = descriptorSetInfos.find(descSet);
  RDCASSERT(descriptorSet_it != descriptorSetInfos.end());
  DescriptorSetInfo &descriptorSet = descriptorSet_it->second;
  DescriptorBindingMapIter binding_it = descriptorSet.bindings.find(descSetBinding);
  uint64_t dstArrayElement = wds->At(4)->U64();
  uint64_t srcArrayElement = 0;
  uint64_t descriptorCount = wds->At(5)->U64();

  ExtObject *srcObjs = NULL;

  switch(binding_it->second.type)
  {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: srcObjs = wds->At(8); break;

    case VK_DESCRIPTOR_TYPE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
    case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
    case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
    case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: srcObjs = wds->At(7); break;

    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: srcObjs = wds->At(9); break;
    default: RDCASSERT(0);
  }
  RDCASSERT(srcObjs->Size() == descriptorCount);

  while(srcArrayElement < descriptorCount)
  {
    RDCASSERT(binding_it != descriptorSet.bindings.end());
    RDCASSERT(binding_it->second.type == (VkDescriptorType)wds->At(6)->U64());
    for(; srcArrayElement < descriptorCount && dstArrayElement < binding_it->second.Size();
        srcArrayElement++, dstArrayElement++)
    {
      binding_it->second.SetBindingObj(dstArrayElement, srcObjs->At(srcArrayElement), false);
    }
    dstArrayElement = 0;
    binding_it++;
  }
}

void TraceTracker::CopyDescriptorSetInternal(ExtObject *cds)
{
  uint64_t srcSetID = cds->At(2)->U64();
  uint64_t srcBinding = cds->At(3)->U64();
  uint64_t dstSetID = cds->At(5)->U64();
  uint64_t dstBinding = cds->At(6)->U64();

  DescriptorSetInfoMapIter srcSet_it = descriptorSetInfos.find(srcSetID);
  RDCASSERT(srcSet_it != descriptorSetInfos.end());
  DescriptorSetInfo &srcSet = srcSet_it->second;
  DescriptorBindingMapIter srcBinding_it = srcSet.bindings.find(srcBinding);

  DescriptorSetInfoMapIter dstSet_it = descriptorSetInfos.find(dstSetID);
  RDCASSERT(dstSet_it != descriptorSetInfos.end());
  DescriptorSetInfo &dstSet = dstSet_it->second;
  DescriptorBindingMapIter dstBinding_it = dstSet.bindings.find(dstBinding);
  uint64_t srcArrayElement = cds->At(4)->U64();
  uint64_t dstArrayElement = cds->At(7)->U64();
  uint64_t descriptorCount = cds->At(8)->U64();
  while(descriptorCount > 0)
  {
    RDCASSERT(srcBinding_it != srcSet.bindings.end());
    RDCASSERT(dstBinding_it != dstSet.bindings.end());
    RDCASSERT(srcBinding_it->second.type == dstBinding_it->second.type);
    uint64_t srcSize = srcBinding_it->second.Size();
    uint64_t dstSize = dstBinding_it->second.Size();
    for(; srcArrayElement < srcSize && dstArrayElement < dstSize;
        srcArrayElement++, dstArrayElement++, descriptorCount--)
    {
      dstBinding_it->second.CopyBinding(dstArrayElement, srcBinding_it->second, srcArrayElement);
    }
    if(srcArrayElement == srcSize)
    {
      srcArrayElement = 0;
      srcBinding_it++;
    }
    if(dstArrayElement == dstSize)
    {
      dstArrayElement = 0;
      dstBinding_it++;
    }
  }
}

void TraceTracker::BeginCommandBufferInternal(ExtObject *o)
{
  CmdBufferRecord cbr = {o, o->At(0)};
  fg.records.push_back(cbr);
}

void TraceTracker::EndCommandBufferInternal(ExtObject *o)
{
  uint32_t i = fg.FindCmdBufferIndex(o->At(0));
  fg.records[i].cmds.push_back(o);
}

void TraceTracker::WaitForFencesInternal(ExtObject *o)
{
}

void TraceTracker::QueueSubmitInternal(ExtObject *o)
{
  // On queue submit:
  // - look at all the command buffers that in this submit info.
  // - look at all the memory updates and descset that have been done
  // - add a new element to the fg.submits list:
  // -- pointers to all command buffer records from fg.records
  // -- memory_updates and descset_updates
  // -- which queue the submit happened on
  ExtObject *si = o->At(2);
  for(uint64_t j = 0; j < si->Size(); j++)
  {
    ExtObject *cb = si->At(j)->At(6);
    vk_cpp_codec::QueueSubmit qs = {o, o->At(0)};
    uint64_t mem_updates = fg.updates.memory.size();
    uint64_t ds_updates = fg.updates.descset.size();
    for(uint64_t i = 0; i < cb->Size(); i++)
    {
      qs.memory_updates = mem_updates;
      qs.descset_updates = ds_updates;
    }
    fg.AddUnorderedSubmit(qs);
  }
}

void TraceTracker::CreateDescriptorPoolInternal(ExtObject *o)
{
  GenericCreateResourceInternal(o);
}

void TraceTracker::CreateDescriptorUpdateTemplateInternal(ExtObject *o)
{
  GenericCreateResourceInternal(o);
}

void TraceTracker::CreateDescriptorSetLayoutInternal(ExtObject *o)
{
  GenericCreateResourceInternal(o);
  RDCASSERT(descSetLayouts.find(o->At(3)->U64()) == descSetLayouts.end());
  descSetLayouts.insert(ExtObjectIDMapPair(o->At(3)->U64(), o));
}

void TraceTracker::AllocateDescriptorSetsInternal(ExtObject *o)
{
  RDCASSERT(o->Size() == 3);
  ExtObject *ai = o->At(1);
  ExtObject *ds = o->At(2);
  // DescriptorSetAllocateInfo.descriptorSetCount must always be equal to '1'.
  // Descriptor set allocation can allocate multiple descriptor sets at the
  // same time, but RenderDoc splits these calls into multiple calls, one per
  // each descriptor set object that is still alive at the time of capture.
  RDCASSERT(ai->At(3)->U64() == 1);
  uint64_t layout_id = ai->At(4)->At(0)->U64();
  ExtObjectIDMapIter layout_it = descSetLayouts.find(layout_id);
  RDCASSERT(layout_it != descSetLayouts.end());
  ExtObject *layout_ci = layout_it->second->At(1);

  DescriptorSetInfo info = {layout_id};
  uint64_t bindingCount = layout_ci->At(3)->U64();
  ExtObject *bindings = layout_ci->At(4);
  for(uint64_t i = 0; i < bindingCount; i++)
  {
    ExtObject *bindingLayout = bindings->At(i);
    uint64_t bindingNum = bindingLayout->At(0)->U64();
    VkDescriptorType type = (VkDescriptorType)bindingLayout->At(1)->U64();
    uint64_t descriptorCount = bindingLayout->At(2)->U64();
    DescriptorBinding binding(type, descriptorCount);

    info.bindings[bindingNum] = binding;
  }
  RDCASSERT(descriptorSetInfos.insert(DescriptorSetInfoMapPair(ds->U64(), info)).second);

  ResourceWithViews rwv = {o};
  createdResources.insert(ResourceWithViewsMapPair(ds->U64(), rwv));
}

void TraceTracker::CreateCommandPoolInternal(ExtObject *o)
{
  GenericCreateResourceInternal(o);
}

void TraceTracker::AllocateCommandBuffersInternal(ExtObject *o)
{
  uint64_t commandBufferCount = o->At("AllocateInfo")->At("commandBufferCount")->U64();
  if(commandBufferCount != 1)
    RDCWARN("%s has AllocateInfo.commandBufferCount equal to %llu, expected '1'", o->Name(),
            commandBufferCount);
  uint64_t cmdBufferPoolID = o->At("AllocateInfo")->At("commandPool")->U64();
  uint64_t cmdBufferID = o->At("CommandBuffer")->U64();
  ResourceWithViewsMapIter cmdBufferPool = ResourceCreateFind(cmdBufferPoolID);
  RDCASSERT(cmdBufferPool != ResourceCreateEnd());
  ResourceCreateAdd(cmdBufferID, o);
  ResourceCreateAddAssociation(cmdBufferPoolID, cmdBufferID, o);
  ResourceCreateAddAssociation(cmdBufferID, cmdBufferPoolID, o);
}

void TraceTracker::InitialContentsInternal(ExtObject *o)
{
  InitResourceAdd(o->At(1)->U64(), o, true);

  if(o->At(0)->U64() == VkResourceType::eResDescriptorSet)
  {
    InitDescriptorSetInternal(o);
  }
}

}    // namespace vk_cpp_codec
