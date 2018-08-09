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
#pragma once

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "common/common.h"
#include "core/core.h"
#include "driver/vulkan/vk_common.h"
#include "driver/vulkan/vk_resources.h"
#include "serialise/rdcfile.h"
#include "ext_object.h"
#include "vk_cpp_codec_state.h"
#if defined _MSC_VER
#include <direct.h>
#endif
#define VARIABLE_OFFSET 0x000FFFFFF
// There is only one acquire semaphore variable
#define ACQUIRE_SEMAPHORE_VAR_ID (VARIABLE_OFFSET)
#define ACQUIRE_SEMAPHORE_VAR_MAX_COUNT (1)
// There is only one PresentImage variable
#define PRESENT_IMAGE_OFFSET (ACQUIRE_SEMAPHORE_VAR_ID + ACQUIRE_SEMAPHORE_VAR_MAX_COUNT)
#define PRESENT_IMAGE_MAX_COUNT 1
// All other presentable objects (views and framebuffers) get a
// single presentable variable for the frame render: VkType_id[acquired_frame]
#define PRESENT_VARIABLE_OFFSET (PRESENT_IMAGE_OFFSET + PRESENT_IMAGE_MAX_COUNT)

namespace vk_cpp_codec
{
class CodeWriter;

struct Variable
{
  std::string type = "";
  std::string name = "";
  Variable(const char *t, const char *n) : type(t), name(n) {}
};

// The 'VariableIDMap' type correlates an SDObject ID with a variable name used
// by the code generator.
typedef std::map<uint64_t, Variable> VariableIDMap;
typedef VariableIDMap::iterator VariableIDMapIter;
typedef std::pair<uint64_t, Variable> VariableIDMapPair;

extern const char *VkImageLayoutStrings[15];

// Bit flags for the various code gen optimizations.
enum CodeGenOptBits
{
  // Allow reordering of memory bindings.
  // Necessary for the BUFFER_INIT, BUFFER_RESET, IMAGE_INIT, IMAGE_RESET, and
  // IMAGE_MEMORY optimizations.
  CODE_GEN_OPT_REORDER_MEMORY_BINDINGS_BIT = 0x1,

  // Optimize buffer initialization by tracking buffer reads/writes.
  // If the buffer has a region that was read without first being written, then an initialization is
  // required.
  CODE_GEN_OPT_BUFFER_INIT_BIT = 0x2,

  // Optimize buffer resets by tracking buffer reads/writes.
  // If the buffer has a region that was first read, and then written, then a reset is required.
  CODE_GEN_OPT_BUFFER_RESET_BIT = 0x4,

  // Optimize image initialization by tracking image reads/writes.
  // If the image has a subresource that was read without first being written, then an
  // initialization is required.
  CODE_GEN_OPT_IMAGE_INIT_BIT = 0x8,

  // Optimize image ]resets by tracking image reads/writes.
  // If the image has a subresource that was first read, and then written, then a reset is required.
  CODE_GEN_OPT_IMAGE_RESET_BIT = 0x10,

  // Do not reset or initialize image memory.
  // The images themselves should be initialized and/or reset.
  CODE_GEN_OPT_IMAGE_MEMORY_BIT = 0x20,

  // Enable all optimizations.
  CODE_GEN_OPT_ALL_OPTS = 0x3f,
};

typedef uint32_t CodeGenOpts;

class TraceTracker
{
private:
  std::string file_dir;
  // Each piece of binary data represents a shader, a pipeline cache or texture/
  // buffer data.
  VariableIDMap dataBlobs;
  // Each captured Vulkan resource has a unique resource ID and this map
  // correlates this ID with the resource type and the variable name used by
  // the code gen.
  VariableIDMap resources;
  VariableIDMap captureMemBindOffsets;
  VariableIDMap replayMemBindOffsets;
  VariableIDMap memAllocInfos;
  VariableIDMap remapMap;
  VariableIDMap resetSizeMap;
  VariableIDMap initSizeMap;
  // Allocated resource, like an image or a buffer, will have a
  // structure of memory requirements associated with it.
  VariableIDMap memRequirements;

  // Occasionally tracker needs to create copies of an ExtObject instance.
  // This is done via ExtObject::Duplicate() call and the caller is responsible
  // for clean up. Such duplicates are stored in this vector and cleaned up.
  ExtObjectVec copies;

  // This map will store everything related to presenting the frame, which includes:
  // 1. The IDs of all the Images that were retrieved from a surface.
  // 2. The IDs of all ImageViews that were created of the images.
  // 3. The IDs of all RenderPasses that have a VK_IMAGE_LAYOUT_PRESENT_SRC_KHR attachment.
  // 4. The IDs of all Framebuffers that have a presenting renderpass attachment.
  // 5. The IDs of command buffers that use the presenting image.
  ExtObjectIDMap presentResources;
  // This map correlates image ID to an the image index in the swapchain.
  ExtObjectVec presentImageIndex;

  std::string swapchainCountStr;
  std::string presentImagesStr;
  std::string queueFamilyPropertiesStr;

  // Descriptor sets are created with a specific descriptor set layout.
  // When descriptor sets are updated, much of the descriptor set layout info
  // is needed. The descSetLayouts is a list of created descriptor set layouts
  // which corresponds to a
  ExtObjectIDMap descSetLayouts;    // a list of all CreateDescriptorSetLayout calls

  // map of memory allocations combined with the list of resources bound to memory allocation
  MemAllocWithResourcesMap memoryAllocations;
  // list of resources that need to be reset with initial data
  InitResourceIDMap initResources;
  // map of all created resources combined with their list of resource views
  ResourceWithViewsMap createdResources;
  // map from VkDescriptorSet to the layout and binding info for that descriptor set.
  DescriptorSetInfoMap descriptorSetInfos;
  // map from pipelineID to the VkGraphicsPipelineCreateInfo or VkComputePipelineCreateInfo
  ExtObjectIDMap createdPipelines;

  // This map collects the IDs of the queues that were submitted during the frame.
  U64Map submittedQueues;

  // State of current bindings (pipelines, vertex/index buffers, etc).
  BindingState bindingState;

  // State of images (layout, access). This state is tracked separately for each aspect, layer, and
  // level.
  ImageStateMap imageStates;

  // Globally accessible resource ID. Some resources need to be accessible
  // at random points in time and may not be available in serialized chunks.
  uint64_t instanceID = 0;
  uint64_t physicalDeviceID = 0;
  uint64_t deviceID = 0;
  uint64_t swapchainID = 0;
  uint64_t swapchainCount = 0;
  uint64_t presentQueueID = 0;
  uint64_t swapchainWidth = 0;
  uint64_t swapchainHeight = 0;
  ExtObject *swapchainCreateInfo = NULL;
  uint64_t queueFamilyCount = 0;

  // queueUsed[family][index] is true if the queue at the specified index in the specified family
  // is used. Currently, "used" just means that "vkGetDeviceQueue" was called for that
  // queue/family.
  std::vector<std::vector<bool>> queueUsed;

  // This map keeps track of semaphore usage in a trace, checking that for
  // every 'wait' semaphore, there was a 'signal' issued before. It also collects
  // the issued 'signals' that haven't been waited on and adds them to the wait
  // list in FramePresent() call.
  U64Map signalSemaphoreIDs;

  // For each vkCmdBeginRenderpass this map stores the ID of the command buffer
  // to the corresponding vkCmdBeginRenderpass chunk to allow matching it on
  // vkCmdEndRenderpass.
  ExtObjectIDMap beginRenderPassCmdBuffer;

  // deviceQueues is a map from queue IDs to the vkGetDeviceQueue chunk which created them.
  ExtObjectIDMap deviceQueues;

  // cmdQueue is the queue on which analyzed commands are to be executed.
  // This will be set durring any call to AnalyzeCmd and the Cmd*Analyze methods.
  uint64_t cmdQueue;
  uint64_t cmdQueueFamily;
  uint64_t CurrentQueueFamily();

  CodeWriter *code = NULL;

  // FrameRender represents the frame render graph
  FrameGraph fg;

  CodeGenOpts optimizations;

  const char *GetVarFromMap(VariableIDMap &m, uint64_t id, const char *type, const char *full_name);
  const char *GetVarFromMap(VariableIDMap &m, const char *type, const char *name, uint64_t id);
  const char *GetVarFromMap(VariableIDMap &m, uint64_t id, std::string map_name);
  void TrackVarInMap(VariableIDMap &m, const char *type, const char *name, uint64_t id);

  // Private 'Internal' functions that are used throughout the 'Scan'ing process.
  void EnumeratePhysicalDevicesInternal(ExtObject *o);
  void CreateDeviceInternal(ExtObject *o);
  void GetDeviceQueueInternal(ExtObject *o);
  void AllocateMemoryInternal(ExtObject *o);
  void CreateResourceInternal(ExtObject *o);
  void CreateResourceViewInternal(ExtObject *o);
  void BindBufferMemoryInternal(ExtObject *o);
  void BindImageMemoryInternal(ExtObject *o);
  void CreateFramebufferInternal(ExtObject *o);
  void CreateRenderPassInternal(ExtObject *o);
  void CreateDescriptorPoolInternal(ExtObject *o);
  void CreateDescriptorUpdateTemplateInternal(ExtObject *o);
  void CreateDescriptorSetLayoutInternal(ExtObject *o);
  void AllocateDescriptorSetsInternal(ExtObject *o);
  void CreateCommandPoolInternal(ExtObject *o);
  void AllocateCommandBuffersInternal(ExtObject *o);
  void CreatePipelineLayoutInternal(ExtObject *o);
  void CreateGraphicsPipelinesInternal(ExtObject *o);
  void CreateComputePipelinesInternal(ExtObject *o);
  void CreateSamplerInternal(ExtObject *ext);
  void CreateShaderModuleInternal(ExtObject *o);
  void CreateSwapchainKHRInternal(ExtObject *o);
  void GetSwapchainImagesKHRInternal(ExtObject *o);
  void CreatePipelineCacheInternal(ExtObject *o);
  void InitialContentsInternal(ExtObject *o);
  void FlushMappedMemoryRangesInternal(ExtObject *o);
  void UpdateDescriptorSetWithTemplateInternal(ExtObject *o);
  void UpdateDescriptorSetsInternal(ExtObject *o);
  void BeginCommandBufferInternal(ExtObject *o);
  void EndCommandBufferInternal(ExtObject *o);
  void QueueSubmitInternal(ExtObject *o);
  void WaitForFencesInternal(ExtObject *o);
  void InitDescriptorSetInternal(ExtObject *o);
  void WriteDescriptorSetInternal(ExtObject *o);
  void CopyDescriptorSetInternal(ExtObject *o);

  void AddCommandBufferToFrameGraph(ExtObject *o);
  ResourceWithViewsMapIter GenericCreateResourceInternal(ExtObject *o);

  // Functions called by AnalyzeCmds for analysis of vkCmd* calls
  void CmdBeginRenderPassAnalyze(ExtObject *o);
  void CmdNextSubpassAnalyze(ExtObject *o);
  void CmdEndRenderPassAnalyze(ExtObject *o);
  void CmdExecuteCommandsAnalyze(ExtObject *o);
  void CmdBindPipelineAnalyze(ExtObject *o);
  void CmdBindDescriptorSetsAnalyze(ExtObject *o);
  void CmdBindIndexBufferAnalyze(ExtObject *o);
  void CmdBindVertexBuffersAnalyze(ExtObject *o);
  void CmdCopyBufferToImageAnalyze(ExtObject *o);
  void CmdCopyImageToBufferAnalyze(ExtObject *o);
  void CmdCopyImageAnalyze(ExtObject *o);
  void CmdBlitImageAnalyze(ExtObject *o);
  void CmdResolveImageAnalyze(ExtObject *o);
  void CmdCopyBufferAnalyze(ExtObject *o);
  void CmdUpdateBufferAnalyze(ExtObject *o);
  void CmdFillBufferAnalyze(ExtObject *o);
  void CmdClearColorImageAnalyze(ExtObject *o);
  void CmdClearDepthStencilImageAnalyze(ExtObject *o);
  void CmdClearAttachmentsAnalyze(ExtObject *o);
  void CmdPipelineBarrierAnalyze(ExtObject *o);
  void CmdWaitEventsAnalyze(ExtObject *o);
  void CmdDispatchAnalyze(ExtObject *o);
  void CmdDispatchIndirectAnalyze(ExtObject *o);
  void CmdDrawIndirectAnalyze(ExtObject *o);
  void CmdDrawIndexedIndirectAnalyze(ExtObject *o);
  void CmdDrawAnalyze(ExtObject *o);
  void CmdDrawIndexedAnalyze(ExtObject *o);

  // Private filtering functions that are used throughout 'Scan'ing process
  bool FilterUpdateDescriptorSets(ExtObject *o);
  bool FilterWriteDescriptorSet(ExtObject *o);    // handles one writedescset.
  bool FilterImageInfoDescSet(uint64_t type, uint64_t image_id, uint64_t sampler_id,
                              uint64_t immut_sampler_id, ExtObject *layout, ExtObject *descImageInfo);
  bool FilterBufferInfoDescSet(uint64_t buffer_id, uint64_t offset, ExtObject *range);
  bool FilterTexelBufferViewDescSet(uint64_t texelview_id);
  bool FilterInitDescSet(ExtObject *o);
  bool FilterUpdateDescriptorSetWithTemplate(ExtObject *o);
  bool FilterCreateImage(ExtObject *o);
  bool FilterCreateGraphicsPipelines(ExtObject *o);
  bool FilterCreateComputePipelines(ExtObject *o);
  void FilterCmdCopyImageToBuffer(ExtObject *o);
  void FilterCmdCopyImage(ExtObject *o);
  void FilterCmdBlitImage(ExtObject *o);
  void FilterCmdResolveImage(ExtObject *o);
  void FilterCreateDevice(ExtObject *o);
  bool FilterCmdPipelineBarrier(ExtObject *o);
  // --------------------------------------------------------------------------
  // Trace tracker 'Scans' the trace entirely several times to understand
  // what was happening in it to resources.
  // --------------------------------------------------------------------------
  void ScanBinaryData(StructuredBufferList &buffers);
  void ScanResourceCreation(StructuredChunkList &chunks, StructuredBufferList &buffers);
  void ScanFilter(StructuredChunkList &chunks);
  void ScanInitialContents(StructuredChunkList &chunks);
  void ScanQueueSubmits(StructuredChunkList &chunks);
  // TODO(akharlamov, bjoeris): This function seems poorly named. It actually analyzes the entire
  // frame graph and tries to understand initial resource data. Proposal to rename it?
  void AnalyzeInitResources();
  void AnalyzeMemoryAllocations();
  void AnalyzeMemoryResetRequirements();
  void AnalyzeImageAndLayout(uint64_t id, uint64_t layout);
  void AnalyzeCmd(ExtObject *ext);

  bool IsEntireResource(ExtObject *image, ExtObject *subres);

  // Helper functions called by the Internal tracking functions.
  void BindResourceMemoryHelper(ExtObject *ext);

  // Helper functions for the Cmd*Analyze() methods
  void AccessBufferMemory(uint64_t buf_id, uint64_t offset, uint64_t size, AccessAction action);
  void TransitionBufferQueueFamily(uint64_t buf_id, uint64_t srcQueueFamily,
                                   uint64_t dstQueueFamily, uint64_t offset, uint64_t size);
  void ReadBoundVertexBuffers(uint64_t vertexCount, uint64_t instanceCount, uint64_t firstVertex,
                              uint64_t firstInstance);
  void AccessMemoryInBoundDescriptorSets(BoundPipeline &boundPipeline);
  void AccessMemoryInDescriptorSet(uint64_t descriptorSet, uint64_t setLayout);
  void AccessImage(uint64_t image, VkImageAspectFlags aspectMask, uint64_t baseMipLevel,
                   uint64_t levelCount, uint64_t baseArrayLayer, uint64_t layerCount, bool is2DView,
                   VkImageLayout layout, AccessAction action);
  void AccessImage(uint64_t image, ExtObject *subresource, VkImageLayout layout, AccessAction action);
  void AccessImage(uint64_t image, ExtObject *subresource, ExtObject *offset, ExtObject *extent,
                   VkImageLayout layout, AccessAction action);
  void AccessImageView(uint64_t view, VkImageLayout layout, AccessAction action,
                       VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM,
                       uint64_t baseArrayLayer = 0, uint64_t layerCount = VK_REMAINING_ARRAY_LAYERS);
  void AccessAttachment(uint64_t attachment, AccessAction action,
                        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_FLAG_BITS_MAX_ENUM,
                        uint64_t baseArrayLayer = 0, uint64_t layerCount = VK_REMAINING_ARRAY_LAYERS);
  void TransitionImageLayout(uint64_t image, ExtObject *subresource, VkImageLayout oldlayout,
                             VkImageLayout newLayout, uint64_t srcQueueFamily,
                             uint64_t dstQueueFamily);
  void TransitionImageViewLayout(uint64_t view, VkImageLayout oldLayout, VkImageLayout newLayout,
                                 uint64_t srcQueueFamily, uint64_t dstQueueFamily);
  void TransitionAttachmentLayout(uint64_t attachment, VkImageLayout layout);
  void LoadSubpassAttachment(ExtObject *attachmentRef);
  void BeginSubpass();
  void EndSubpass();
  ExtObject *FindBufferMemBinding(uint64_t buf_id);
  void BufferImageCopyHelper(uint64_t buf_id, uint64_t img_id, ExtObject *regions,
                             VkImageLayout imageLayout, AccessAction buffeAction,
                             AccessAction imageAction);

  void ApplyMemoryUpdate(ExtObject *ext);
  void ApplyDescSetUpdate(ExtObject *ext);
  void SaveInitialLayout(ExtObject *image, ExtObject *layouts);
  void InitialLayoutsInternal(ExtObject *o);

public:
  TraceTracker(std::string path) : file_dir{path} {}
  void Set(CodeWriter *ptr)
  {
    code = ptr;
    RDCASSERT(code != NULL);
  }

  void InstanceID(uint64_t id) { instanceID = id; }
  uint64_t InstanceID() { return instanceID; }
  void PhysDevID(uint64_t id) { physicalDeviceID = id; }
  uint64_t PhysDevID() { return physicalDeviceID; }
  void DeviceID(uint64_t id) { deviceID = id; }
  uint64_t DeviceID() { return deviceID; }
  void SwapchainID(uint64_t id) { swapchainID = id; }
  uint64_t SwapchainID() { return swapchainID; }
  uint64_t SwapchainWidth() const { return swapchainWidth; }
  uint64_t SwapchainHeight() const { return swapchainHeight; }
  void PresentQueueID(uint64_t id) { presentQueueID = id; }
  uint64_t PresentQueueID() { return presentQueueID; }
  uint64_t SwapchainCount() { return swapchainCount; }
  const char *SwapchainCountStr() { return swapchainCountStr.c_str(); }
  const char *PresentImagesStr() { return presentImagesStr.c_str(); }
  const char *GetInstanceVar() { return GetResourceVar(instanceID); }
  const char *GetPhysDeviceVar() { return GetResourceVar(physicalDeviceID); }
  const char *GetDeviceVar() { return GetResourceVar(deviceID); }
  const char *GetSwapchainVar() { return GetResourceVar(swapchainID); }
  const char *GetPresentQueueVar() { return GetResourceVar(presentQueueID); }
  const char *GetQueueFamilyPropertiesVar() const { return queueFamilyPropertiesStr.c_str(); }
  uint64_t QueueFamilyCount() const { return queueFamilyCount; }
  bool IsQueueFamilyUsed(uint64_t queueFamilyIndex) const
  {
    if(queueFamilyIndex > queueUsed.size())
    {
      return false;
    }
    for(size_t i = 0; i < queueUsed[queueFamilyIndex].size(); i++)
    {
      if(queueUsed[queueFamilyIndex][i])
      {
        return true;
      }
    }
    return false;
  }
  // All the Get***Var call below return variable iterator using the ID.
  VariableIDMapIter GetResourceVarIt(uint64_t id);
  // Get a variable name from a resource map using the ID.
  const char *GetResourceVar(uint64_t id);
  // Get a variable name from a resource map using the ID if it was already added.
  // If it's a new variable, add it to the map and also print it to VAR files.
  // Variable type is 'type' and name is 'name_id'
  const char *GetResourceVar(const char *type, const char *name, uint64_t id);
  // Get a variable name from a resource map using the ID if it was already added.
  // If it's a new variable, add it to the map and also print it to VAR files.
  // Variable type is 'type' and name is 'type_id'
  const char *GetResourceVar(const char *type, uint64_t id);
  // Get a variable name from resource map using the ID if it was already added.
  // If it's a new variable, add it to map and also print it to VAR files.
  // Variable type is 'type' and name is 'name'
  const char *GetResourceVar(uint64_t id, const char *type, const char *name);
  // Get a variable of VkMemoryAllocateInfo type from the map if it was already added.
  // If it's a new variable, add it to the map and also print it to VAR files.
  const char *GetMemAllocInfoVar(uint64_t id, bool create = false);
  // Get a variable name for the binary data blob from the map if it was already added.
  // If it's a new variable, add it to the map and also print it to VAR files.
  const char *GetDataBlobVar(uint64_t id);
  // Get a variable of VkMemoryAllocateInfo type from the map if it was already added.
  // If it's a new variable, add it to the map and also print it to VAR files.
  const char *GetMemRemapVar(uint64_t id);
  // Get a variable of VkDeviceSize type from the map if it was already added.
  // If it's a new variable, add it to the map and also print it to VAR files.
  const char *GetMemResetSizeVar(uint64_t id);
  // Get a variable of VkDeviceSize type from the map if it was already added.
  // If it's a new variable, add it to the map and also print it to VAR files.
  const char *GetMemInitSizeVar(uint64_t id);
  // Get a variable of VkDeviceSize type from the map.
  const char *GetReplayBindOffsetVar(uint64_t id);
  // Get a variable of VkDeviceSize type from the map.
  const char *GetCaptureBindOffsetVar(uint64_t id);
  // Get a variable of VkMemoryRequirements type from the map.
  const char *GetMemReqsVar(uint64_t id);

  // Get the descriptor set layout that was used for a descriptor set.
  ExtObject *DescSetInfosFindLayout(uint64_t id);
  DescriptorSetInfoMapIter DescSetInfosFind(uint64_t id) { return descriptorSetInfos.find(id); }
  // SDObject of an array type will have elements that all have the same name "$el".
  // This is not informative for the code generation and also C/C++ doesn't allow
  // names to start with $. To fix this, I create a duplicate and replace the name,
  // with the parent's name + array index and I serialize the duplicate instead.
  // The duplicates are stored in a 'copies' array and have to be manually cleaned up.
  ExtObject *CopiesAdd(ExtObject *o, uint64_t i, std::string &suffix);
  void CopiesClear();

  VariableIDMapIter DataBlobBegin() { return dataBlobs.begin(); }
  VariableIDMapIter DataBlobEnd() { return dataBlobs.end(); }
  InitResourceIDMapIter InitResourceAdd(uint64_t id, ExtObject *o, bool u)
  {
    return initResources.insert(InitResourceIDMapPair(id, InitResourceDesc(o, u))).first;
  }
  InitResourceIDMapIter InitResourceFind(uint64_t id) { return initResources.find(id); }
  InitResourceIDMapIter InitResourceBegin() { return initResources.begin(); }
  InitResourceIDMapIter InitResourceEnd() { return initResources.end(); }
  // Tracker holds the lists of memory allocations that need to be created out of order.
  // Below is the API to access those allocations from the CodeWriter class.
  int64_t MemAllocTypeIndex(uint64_t id)
  {
    return memoryAllocations.find(id)->second.allocateSDObj->At(1)->At(3)->U64();
  }
  void MemAllocAdd(uint64_t id, MemoryAllocationWithBoundResources &mawbr)
  {
    memoryAllocations.insert(MemAllocWithResourcesMapPair(id, mawbr));
  }
  MemAllocWithResourcesMapIter MemAllocFind(uint64_t id) { return memoryAllocations.find(id); }
  MemAllocWithResourcesMapIter MemAllocBegin() { return memoryAllocations.begin(); }
  MemAllocWithResourcesMapIter MemAllocEnd() { return memoryAllocations.end(); }
  // Tracker holds the lists of resource that need to be created out of order.
  // Below is the API to access those resource from the CodeWriter class.
  ResourceWithViewsMapIter ResourceCreateAdd(uint64_t id, ExtObject *o)
  {
    ResourceWithViews rwv = {o};
    ResourceWithViewsMapIter it = createdResources.insert(ResourceWithViewsMapPair(id, rwv)).first;
    return it;
  }
  void ResourceCreateAddAssociation(uint64_t resourceId, uint64_t associationID, ExtObject *o)
  {
    RDCASSERT(createdResources.find(resourceId) != createdResources.end());
    RDCASSERT(createdResources[resourceId].views.find(associationID) ==
              createdResources[resourceId].views.end());
    createdResources[resourceId].views.emplace(associationID, o);
    return;
  }
  ResourceWithViewsMapIter ResourceCreateFind(uint64_t id) { return createdResources.find(id); }
  ExtObject *ResourceCreateFindMemReqs(uint64_t id)
  {
    return createdResources.find(id)->second.sdobj->At(4);
  }
  ResourceWithViewsMapIter ResourceCreateBegin() { return createdResources.begin(); }
  ResourceWithViewsMapIter ResourceCreateEnd() { return createdResources.end(); }
  ImageStateMapIter ImageStateFind(uint64_t id) { return imageStates.find(id); }
  ImageStateMapIter ImageStateEnd() { return imageStates.end(); }
  U64MapIter SubmittedQueuesBegin() { return submittedQueues.begin(); }
  U64MapIter SubmittedQueuesEnd() { return submittedQueues.end(); }
  U64MapIter SignalSemaphoreBegin() { return signalSemaphoreIDs.begin(); }
  U64MapIter SignalSemaphoreEnd() { return signalSemaphoreIDs.end(); }
  ExtObjectVecIter PresentImageBegin() { return presentImageIndex.begin(); }
  ExtObjectVecIter PresentImageEnd() { return presentImageIndex.end(); }
  // Return pointer to an ExtObject in the pAttachment array that is presentable.
  ExtObject *FramebufferPresentView(ExtObject *o);
  // Check if a resource has been properly created and that it's not NULL
  bool IsValidNonNullResouce(uint64_t id);
  // Check if a resource is involved in presenting the final image
  bool IsPresentationResource(uint64_t id);
  // Check if an image or device memory needs a reset or init
  bool ResourceNeedsReset(uint64_t resource_id, bool forInit, bool forReset);

  // --------------------------------------------------------------------------
  // Vulkan API specific tracking functions
  // --------------------------------------------------------------------------
  // Use initResource to check if a resource has initial data
  // and if it does add a TRANSFER_DST flag to createInfo.usage.
  void CreateResource(ExtObject *o);
  // Use presentResources to check if framebuffer deals with
  // presentation, if it does add it to presentResources. This also
  // creates special 'acquired_frame' names to use in render functions.
  bool CreateFramebuffer(ExtObject *o);
  // Use presentResources to check if an imageview is created for a
  // swapchain image, if it is add it to presentResources. This also
  // creates special 'acquired_frame' names to use in render functions.
  bool CreateImageView(ExtObject *o);
  // Check of command buffer inherits presentation framebuffer or
  // renderpass and save command buffer in presentResources.
  void BeginCommandBuffer(ExtObject *o);
  // Check if the objects in the event wait are valid and not NULL. If they
  // aren't, remove them. Also check if there are presentation resources in the
  // barrier, and if there are, add the command buffer to presentResources.
  bool CmdWaitEvents(ExtObject *o);
  // Check if the framebuffer is presenting, and if it is, use special
  // 'acquired_frame' name and add command buffer to presentResource.
  void CmdBeginRenderPass(ExtObject *o);

  // The purpose of this function is to track what's happening on queue submit.
  // We are interested in a few things here:
  // 1. If the queue submitting any command buffer that has transfered an image
  // to a presentation layout. If yes, use this queue as a present queue.
  // 2. Accumulate semaphore from p[Wait|Signal]Semaphores arrays. We need to\
  // make sure that there are no 'waits' that are never signalled and also to
  // make sure Present() waits on all signalled semaphores later.
  // 3. Any Queue that submits anything needs to do a WaitIdle at the end of the
  // frame in order to avoid synchronization problems. This can be optimized later.
  void QueueSubmit(ExtObject *o);

  // Scan() looks at all of the trace and tries to build the
  // necessary data structures to facilitate the subsequent code generation.
  void Scan(StructuredChunkList &chunks, StructuredBufferList &buffers);

  inline CodeGenOpts Optimizations() { return optimizations; }
  inline void SetOptimizations(CodeGenOpts opts) { optimizations = opts; }
};

}    // namespace vk_cpp_codec
