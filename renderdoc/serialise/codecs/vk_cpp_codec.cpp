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
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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
#include "vk_cpp_codec_tracker.h"
#include "vk_cpp_codec_writer.h"

namespace vk_cpp_codec
{
static ReplayStatus Structured2Code(CodeWriter &code, TraceTracker &tracker, const RDCFile &file,
                                    uint64_t version, const StructuredChunkList &chunks,
                                    RENDERDOC_ProgressCallback progress)
{
  code.Resolution(CodeWriter::ID_VAR);
  uint32_t pass = CodeWriter::ID_CREATE;

#ifndef CODE_ACQUIRE_NEXT_IMAGE
#define CODE_ACQUIRE_NEXT_IMAGE(ext, pass) \
  if(pass == CodeWriter::ID_CREATE)        \
  {                                        \
    pass = CodeWriter::ID_PRERESET;        \
    code.AcquireNextImage(ext, pass);      \
  }
#endif

#ifndef CODE_VULKAN_CASE
#define CODE_VULKAN_CASE(func, ext, pass)                               \
  {                                                                     \
    case(uint32_t)VulkanChunk::vk##func: code.func(ext, pass); break; \
  }
#endif

  for(size_t c = 0; c < chunks.size(); c++)
  {
    code.MultiPartSplit();
    tracker.CopiesClear();

    ExtObject *ext = as_ext(chunks[c]);
    if(ext->ChunkID() >= (uint32_t)VulkanChunk::vkEnumeratePhysicalDevices)
    {
      ext->name = code.shimPrefix + string(ext->name);
    }

    switch(ext->ChunkID())
    {
      case(uint32_t)SystemChunk::DriverInit: code.CreateInstance(ext, pass); break;
      case(uint32_t)SystemChunk::InitialContents:
        CODE_ACQUIRE_NEXT_IMAGE(ext, pass);
        code.InitialContents(ext);
        break;
      case(uint32_t)SystemChunk::InitialContentsList: CODE_ACQUIRE_NEXT_IMAGE(ext, pass); break;
      case(uint32_t)SystemChunk::CaptureScope: break;
      case(uint32_t)SystemChunk::CaptureBegin:
        CODE_ACQUIRE_NEXT_IMAGE(ext, pass);
        code.InitialLayouts(ext, pass);
        pass = CodeWriter::ID_RENDER;
        break;
      case(uint32_t)SystemChunk::CaptureEnd:
        code.EndFramePresent(ext, pass);
        code.EndFrameWaitIdle(ext, pass);
        break;

        CODE_VULKAN_CASE(EnumeratePhysicalDevices, ext, pass);
        CODE_VULKAN_CASE(GetDeviceQueue, ext, pass);
        CODE_VULKAN_CASE(GetSwapchainImagesKHR, ext, pass);

        CODE_VULKAN_CASE(AllocateCommandBuffers, ext, pass);
        CODE_VULKAN_CASE(AllocateDescriptorSets, ext, pass);

        CODE_VULKAN_CASE(CreateCommandPool, ext, pass);
        CODE_VULKAN_CASE(CreateDevice, ext, pass);
        CODE_VULKAN_CASE(CreateRenderPass, ext, pass);
        CODE_VULKAN_CASE(CreateDescriptorPool, ext, pass);
        CODE_VULKAN_CASE(CreateDescriptorSetLayout, ext, pass);
        CODE_VULKAN_CASE(CreateDescriptorUpdateTemplate, ext, pass);
        CODE_VULKAN_CASE(CreateBufferView, ext, pass);
        CODE_VULKAN_CASE(CreateSampler, ext, pass);
        CODE_VULKAN_CASE(CreateShaderModule, ext, pass);
        CODE_VULKAN_CASE(CreatePipelineLayout, ext, pass);
        CODE_VULKAN_CASE(CreatePipelineCache, ext, pass);
        CODE_VULKAN_CASE(CreateGraphicsPipelines, ext, pass);
        CODE_VULKAN_CASE(CreateComputePipelines, ext, pass);
        CODE_VULKAN_CASE(CreateSemaphore, ext, pass);
        CODE_VULKAN_CASE(CreateFence, ext, pass);
        CODE_VULKAN_CASE(CreateQueryPool, ext, pass);
        CODE_VULKAN_CASE(CreateEvent, ext, pass);
        CODE_VULKAN_CASE(CreateSwapchainKHR, ext, pass);

        CODE_VULKAN_CASE(UnmapMemory, ext, pass);
        CODE_VULKAN_CASE(FlushMappedMemoryRanges, ext, pass);
        CODE_VULKAN_CASE(GetFenceStatus, ext, pass);
        CODE_VULKAN_CASE(ResetFences, ext, pass);
        CODE_VULKAN_CASE(WaitForFences, ext, pass);
        CODE_VULKAN_CASE(GetEventStatus, ext, pass);
        CODE_VULKAN_CASE(SetEvent, ext, pass);
        CODE_VULKAN_CASE(ResetEvent, ext, pass);
        CODE_VULKAN_CASE(UpdateDescriptorSets, ext, pass);
        CODE_VULKAN_CASE(UpdateDescriptorSetWithTemplate, ext, pass);
        CODE_VULKAN_CASE(QueueWaitIdle, ext, pass);
        CODE_VULKAN_CASE(DeviceWaitIdle, ext, pass);

        CODE_VULKAN_CASE(CmdNextSubpass, ext, pass);
        CODE_VULKAN_CASE(CmdExecuteCommands, ext, pass);
        CODE_VULKAN_CASE(CmdEndRenderPass, ext, pass);
        CODE_VULKAN_CASE(CmdBindPipeline, ext, pass);
        CODE_VULKAN_CASE(CmdSetViewport, ext, pass);
        CODE_VULKAN_CASE(CmdSetScissor, ext, pass);
        CODE_VULKAN_CASE(CmdSetLineWidth, ext, pass);
        CODE_VULKAN_CASE(CmdSetDepthBias, ext, pass);
        CODE_VULKAN_CASE(CmdSetBlendConstants, ext, pass);
        CODE_VULKAN_CASE(CmdSetDepthBounds, ext, pass);
        CODE_VULKAN_CASE(CmdSetStencilCompareMask, ext, pass);
        CODE_VULKAN_CASE(CmdSetStencilWriteMask, ext, pass);
        CODE_VULKAN_CASE(CmdSetStencilReference, ext, pass);
        CODE_VULKAN_CASE(CmdBindDescriptorSets, ext, pass);
        CODE_VULKAN_CASE(CmdBindIndexBuffer, ext, pass);
        CODE_VULKAN_CASE(CmdBindVertexBuffers, ext, pass);
        CODE_VULKAN_CASE(CmdCopyBufferToImage, ext, pass);
        CODE_VULKAN_CASE(CmdCopyImageToBuffer, ext, pass);
        CODE_VULKAN_CASE(CmdCopyImage, ext, pass);
        CODE_VULKAN_CASE(CmdBlitImage, ext, pass);
        CODE_VULKAN_CASE(CmdResolveImage, ext, pass);
        CODE_VULKAN_CASE(CmdCopyBuffer, ext, pass);
        CODE_VULKAN_CASE(CmdUpdateBuffer, ext, pass);
        CODE_VULKAN_CASE(CmdFillBuffer, ext, pass);
        CODE_VULKAN_CASE(CmdPushConstants, ext, pass);
        CODE_VULKAN_CASE(CmdClearColorImage, ext, pass);
        CODE_VULKAN_CASE(CmdClearDepthStencilImage, ext, pass);
        CODE_VULKAN_CASE(CmdClearAttachments, ext, pass);
        CODE_VULKAN_CASE(CmdSetEvent, ext, pass);
        CODE_VULKAN_CASE(CmdResetEvent, ext, pass);
        CODE_VULKAN_CASE(CmdDraw, ext, pass);
        CODE_VULKAN_CASE(CmdDrawIndirect, ext, pass);
        CODE_VULKAN_CASE(CmdDrawIndexed, ext, pass);
        CODE_VULKAN_CASE(CmdDrawIndexedIndirect, ext, pass);
        CODE_VULKAN_CASE(CmdDispatch, ext, pass);
        CODE_VULKAN_CASE(CmdDispatchIndirect, ext, pass);
        CODE_VULKAN_CASE(CmdPipelineBarrier, ext, pass);
        CODE_VULKAN_CASE(EndCommandBuffer, ext, pass);

      // akharlamov: memory allocation, buffer and image creation and binding happens right after
      // device was created.
      case(uint32_t)VulkanChunk::vkAllocateMemory:      // Fallthrough
      case(uint32_t)VulkanChunk::vkCreateBuffer:        // Fallthrough
      case(uint32_t)VulkanChunk::vkCreateImage:         // Fallthrough
      case(uint32_t)VulkanChunk::vkBindBufferMemory:    // Fallthrough
      case(uint32_t)VulkanChunk::vkBindImageMemory:
        break;

      // akharlamov: VkImages aquired from swapchain are considered 'Presentable'. Any resource
      // such as VkImageView, VkFramebuffer etc that is created using a 'Presentable' resource
      // is also considered 'Presentable'. API calls used to deal with a 'Presentable' resources are
      // modified by the code generator.
      case(uint32_t)VulkanChunk::vkCreateFramebuffer:
        if(tracker.CreateFramebuffer(ext))
          code.CreatePresentFramebuffer(ext, pass);
        else
          code.CreateFramebuffer(ext, pass);
        break;
      case(uint32_t)VulkanChunk::vkCreateImageView:
        if(tracker.CreateImageView(ext))
          code.CreatePresentImageView(ext, pass);
        else
          code.CreateImageView(ext, pass);
        break;

      case(uint32_t)VulkanChunk::vkBeginCommandBuffer:
        tracker.BeginCommandBuffer(ext);
        code.BeginCommandBuffer(ext, pass);
        break;
      case(uint32_t)VulkanChunk::vkQueueSubmit:
        tracker.QueueSubmit(ext);
        code.QueueSubmit(ext, pass);
        break;
      case(uint32_t)VulkanChunk::vkCmdBeginRenderPass:
        tracker.CmdBeginRenderPass(ext);
        code.CmdBeginRenderPass(ext, pass);
        break;
      case(uint32_t)VulkanChunk::vkCmdWaitEvents:
        if(tracker.CmdWaitEvents(ext))
          code.CmdWaitEvents(ext, pass);
        break;

      // akharlamov: this is a reminder which Vk calls I'm aware of in RenderDoc, but that are
      // not yet implemented in code generator.
      case(uint32_t)VulkanChunk::vkQueueBindSparse:
      case(uint32_t)VulkanChunk::vkCmdWriteTimestamp:
      case(uint32_t)VulkanChunk::vkCmdCopyQueryPoolResults:
      case(uint32_t)VulkanChunk::vkCmdBeginQuery:
      case(uint32_t)VulkanChunk::vkCmdEndQuery:
      case(uint32_t)VulkanChunk::vkCmdResetQueryPool:
      case(uint32_t)VulkanChunk::vkCmdDebugMarkerBeginEXT:
      case(uint32_t)VulkanChunk::vkCmdDebugMarkerInsertEXT:
      case(uint32_t)VulkanChunk::vkCmdDebugMarkerEndEXT:
      case(uint32_t)VulkanChunk::vkDebugMarkerSetObjectNameEXT:
      case(uint32_t)VulkanChunk::vkRegisterDeviceEventEXT:
      case(uint32_t)VulkanChunk::vkRegisterDisplayEventEXT:
      case(uint32_t)VulkanChunk::SetShaderDebugPath:
      case(uint32_t)VulkanChunk::vkCmdIndirectSubCommand:
      default: RDCWARN("%s Vulkan call not implemented", ext->Name()); break;
    }
  }
  return ReplayStatus::Succeeded;
}

bool OptimizationDisabled(const char *name)
{
#ifdef _WIN32
  size_t len = 0;
  char var[8];
  errno_t err = getenv_s(&len, var, sizeof(var), name);
  if(err)
    return false;
#else
  const char *var = getenv(name);
  if(!var)
    return false;
#endif
  return strcmp(var, "false") == 0;
}

CodeGenOpts GetEnvOpts()
{
  CodeGenOpts optimizations = CODE_GEN_OPT_ALL_OPTS;
  if(OptimizationDisabled("RDOC_CODE_GEN_ALL_OPTS"))
  {
    optimizations = 0;
  }
  if(OptimizationDisabled("RDOC_CODE_GEN_OPT_BUFFER_INIT"))
  {
    optimizations &= ~CODE_GEN_OPT_BUFFER_INIT_BIT;
  }
  if(OptimizationDisabled("RDOC_CODE_GEN_OPT_BUFFER_RESET"))
  {
    optimizations &= ~CODE_GEN_OPT_BUFFER_RESET_BIT;
  }
  if(OptimizationDisabled("RDOC_CODE_GEN_OPT_IMAGE_INIT"))
  {
    optimizations &= ~CODE_GEN_OPT_IMAGE_INIT_BIT;
  }
  if (1) // OptimizationDisabled("RDOC_CODE_GEN_OPT_IMAGE_RESET")
  {
    optimizations &= ~CODE_GEN_OPT_IMAGE_RESET_BIT;
    RDCWARN("Optimization for VkImage resets is disabled.");
  }
  if(OptimizationDisabled("RDOC_CODE_GEN_OPT_IMAGE_MEMORY"))
  {
    optimizations &= ~CODE_GEN_OPT_IMAGE_MEMORY_BIT;
    optimizations &= ~CODE_GEN_OPT_IMAGE_INIT_BIT;
    optimizations &= ~CODE_GEN_OPT_IMAGE_RESET_BIT;
  }
  return optimizations;
}

}    // namespace vk_cpp_codec

ReplayStatus exportCPPZ(const char *filename, const RDCFile &rdc, const SDFile &structData,
                        RENDERDOC_ProgressCallback progress)
{
  std::string s_filename(filename);
  std::size_t found = s_filename.find_last_of(".");
  RDCASSERT(found != string::npos);

  vk_cpp_codec::CodeWriter code(s_filename.substr(0, found));
  vk_cpp_codec::TraceTracker tracker(s_filename.substr(0, found));
  tracker.SetOptimizations(vk_cpp_codec::GetEnvOpts());

  code.Set(&tracker);

  StructuredChunkList chunks;
  StructuredBufferList buffers;
  for(uint32_t i = 0; i < structData.chunks.size(); i++)
  {
    chunks.push_back(structData.chunks[i]);
  }
  for(uint32_t i = 0; i < structData.buffers.size(); i++)
  {
    buffers.push_back(structData.buffers[i]);
  }

  tracker.Scan(chunks, buffers);

  code.PrintReadBuffers(buffers);

  ReplayStatus status =
      vk_cpp_codec::Structured2Code(code, tracker, rdc, structData.version, chunks, progress);

  code.Close();

  return status;
}

#undef CODE_VULKAN_CASE
#undef CODE_ACQUIRE_NEXT_IMAGE

static ConversionRegistration CPPConversionRegistration(
    &exportCPPZ, {
                     "cpp", "CPP capture project",
                     R"(Stores the structured data in an cpp project, with large buffer data
 stored in indexed blobs in binary files. It cannot be reimported.)",
                     false,
                 });
