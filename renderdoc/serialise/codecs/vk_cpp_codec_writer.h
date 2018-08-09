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
#include "vk_cpp_codec_file.h"
#include "ext_object.h"
#include "vk_cpp_codec_state.h"
#include "vk_cpp_codec_tracker.h"

namespace vk_cpp_codec
{
class TraceTracker;

class CodeWriter
{
  friend TraceTracker;

  static std::string rootCMakeLists;
  static std::string projectCMakeLists;
  static std::string helperCMakeLists;
  static std::string mainWinCpp;
  static std::string mainXlibCpp;
  static std::string commonH;
  static std::string helperH;
  static std::string helperCppP1;
  static std::string helperCppP2;
  static std::string genScriptWin;
  static std::string genScriptLinux;
  static std::string genScriptWinNinja;
public:
  enum IDs
  {
    ID_MAIN,
    ID_VAR,
    ID_RENDER,
    ID_CREATE,
    ID_RELEASE,
    ID_INIT,
    ID_PRERESET,
    ID_POSTRESET,

    ID_COUNT,
  };
  const char *shimPrefix = "";

protected:
  typedef std::array<std::string, ID_COUNT> func_array;
  func_array funcs = {{"main",    "variables", "render",   "create",
                      "release", "init",      "prereset", "postreset"}};

  std::string rootDirectory;
  typedef std::array<CodeFile *, ID_COUNT> file_array;
  file_array files;

  TraceTracker *tracker = NULL;

  // Code project doesn't allow multiple calls to 'Open'.
  // Once you create a code project you get all the files you need.
  void Open();

  void WriteTemplateFile(std::string subdir, std::string file, const char * str);

  const uint32_t kLinearizeMemory = 0;

  void RemapMemAlloc(uint32_t pass, MemAllocWithResourcesMapIter alloc_it);

  void EarlyCreateResource(uint32_t pass);
  void EarlyAllocateMemory(uint32_t pass);
  void EarlyBindResourceMemory(uint32_t pass);

  // This vulkan functions are not allowed to be called directly by
  // codec.cpp.
  void CreateImage(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateBuffer(ExtObject *o, uint32_t pass, bool global_ci = false);
  void AllocateMemory(ExtObject *o, uint32_t pass);
  void BindResourceMemory(ExtObject *o, uint32_t pass);

  void GenericCreatePipelines(ExtObject *o, uint32_t pass, bool global_ci = false);
  void GenericEvent(ExtObject *o, uint32_t pass);
  void GenericVkCreate(ExtObject *o, uint32_t pass, bool global_ci = false);
  void GenericWaitIdle(ExtObject *o, uint32_t pass);
  void GenericCmdSetRectTest(ExtObject *o, uint32_t pass);
  void GenericCmdSetStencilParam(ExtObject *o, uint32_t pass);
  void GenericCmdEvent(ExtObject *o, uint32_t pass);
  void GenericCmdDrawIndirect(ExtObject *o, uint32_t pass);

  void CreateAuxResources(ExtObject *o, uint32_t pass, bool global_ci = false);
  void HandleMemoryAllocationAndResourceCreation(uint32_t pass);
  void BufferOrImageMemoryReqs(ExtObject *o, const char *get_mem_req_func, uint32_t pass);

  void InlineVariable(ExtObject *o, uint32_t pass);
  void LocalVariable(ExtObject *o, std::string suffix, uint32_t pass);

  void InitSrcBuffer(ExtObject *o, uint32_t pass);
  void InitDstBuffer(ExtObject *o, uint32_t pass);
  void InitDescSet(ExtObject *o);
  void CopyResetImage(ExtObject *o, uint32_t pass);
  void CopyResetBuffer(ExtObject *o, uint32_t pass);
  void ImageLayoutTransition(uint64_t image_id, ExtObject *subres, const char *old_layout,
                             uint32_t pass);
  void ClearBufferData();

public:
  CodeWriter(std::string path) : rootDirectory{path}
  {
    if(strcmp(RENDERDOC_GetConfigSetting("shim"), "true") == 0)
    {
      shimPrefix = "shim_";
    }
    Open();
  }
  ~CodeWriter() { Close(); }
  // Closing the code project also closes all of the code files. In
  // case of a 'MAIN' code files, it get's fully created at the very
  // end of code generation.
  void Close();

  void Set(TraceTracker *ptr)
  {
    tracker = ptr;
    RDCASSERT(tracker != NULL);
    tracker->Set(this);
  }

  void MultiPartSplit()
  {
    for(uint32_t i = ID_RENDER; i < ID_COUNT; i++)
    {
      static_cast<MultiPartCodeFile *>(files[i])->MultiPartSplit();
    }
  }

  std::string MakeVarName(const char *name1, const char *name2)
  {
    std::string full_name = std::string(name1) + std::string("_") + std::string(name2);
    return full_name;
  }

  std::string MakeVarName(const char *name, uint64_t id)
  {
    std::string full_name = std::string(name) + std::string("_") + std::to_string(id);
    return full_name;
  }

  // Add a global variable of a given type into the VAR files. Just use the
  // provided name as the full name for the variable.
  std::string AddNamedVar(const char *type, const char *name)
  {
    files[ID_VAR]->PrintLnH("extern %s %s;", type, name).PrintLn("%s %s;", type, name);
    return std::string(name);
  }

  // Add a global variable of a given type into the VAR files. Concatenate the 'type'
  // and the 'name' to get a 'full' variable name. For example 'VkDevice' and
  // 'captured' will produce 'VkDevice_captured'.
  std::string AddVar(const char *type, const char *name)
  {
    std::string full_name = MakeVarName(type, name);
    return AddNamedVar(type, full_name.c_str());
  }

  // Add a global variable of a given type into the VAR files.
  // This call is used for more complicated variable declarations, such as
  // std::vector<VkDevice> VkDevice_1;
  std::string AddVar(const char *type, const char *name, uint64_t id)
  {
    std::string full_name = std::string(name) + std::string("_") + std::to_string(id);
    return AddNamedVar(type, full_name.c_str());
  }

  // Add a global variable of a given type into the VAR files.
  // For simple variable declarations, such as VkDevice VkDevice_1;
  std::string AddVar(const char *type, uint64_t id) { return AddVar(type, type, id); }
  // --------------------------------------------------------------------------
  void Resolution(uint32_t pass);

  void EnumeratePhysicalDevices(ExtObject *o, uint32_t pass);
  void GetDeviceQueue(ExtObject *o, uint32_t pass);
  void GetSwapchainImagesKHR(ExtObject *o, uint32_t pass);

  void CreateInstance(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreatePresentFramebuffer(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreatePresentImageView(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateDescriptorPool(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateCommandPool(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateFramebuffer(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateRenderPass(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateSemaphore(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateFence(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateEvent(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateQueryPool(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateDescriptorSetLayout(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateDescriptorUpdateTemplate(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateImageView(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateSampler(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateShaderModule(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreatePipelineLayout(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreatePipelineCache(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateBufferView(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateSwapchainKHR(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateGraphicsPipelines(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateComputePipelines(ExtObject *o, uint32_t pass, bool global_ci = false);
  void CreateDevice(ExtObject *o, uint32_t pass, bool global_ci = false);

  void AllocateCommandBuffers(ExtObject *o, uint32_t pass);
  void AllocateDescriptorSets(ExtObject *o, uint32_t pass);

  void FlushMappedMemoryRanges(ExtObject *o, uint32_t pass);
  void UnmapMemory(ExtObject *o, uint32_t pass);
  void AcquireNextImage(ExtObject *o, uint32_t pass);
  void BeginCommandBuffer(ExtObject *o, uint32_t pass);
  void EndCommandBuffer(ExtObject *o, uint32_t pass);
  void WaitForFences(ExtObject *o, uint32_t pass);
  void GetFenceStatus(ExtObject *o, uint32_t pass);
  void ResetFences(ExtObject *o, uint32_t pass);
  void GetEventStatus(ExtObject *o, uint32_t pass);
  void SetEvent(ExtObject *o, uint32_t pass);
  void ResetEvent(ExtObject *o, uint32_t pass);
  void QueueSubmit(ExtObject *o, uint32_t pass);
  void QueueWaitIdle(ExtObject *o, uint32_t pass);
  void DeviceWaitIdle(ExtObject *o, uint32_t pass);
  void UpdateDescriptorSets(ExtObject *o, uint32_t pass);
  void UpdateDescriptorSetWithTemplate(ExtObject *o, uint32_t pass);

  // Command recording API calls
  void CmdBeginRenderPass(ExtObject *o, uint32_t pass);
  void CmdNextSubpass(ExtObject *o, uint32_t pass);
  void CmdExecuteCommands(ExtObject *o, uint32_t pass);
  void CmdEndRenderPass(ExtObject *o, uint32_t pass);
  void CmdSetViewport(ExtObject *o, uint32_t pass);
  void CmdSetScissor(ExtObject *o, uint32_t pass);
  void CmdBindDescriptorSets(ExtObject *o, uint32_t pass);
  void CmdBindPipeline(ExtObject *o, uint32_t pass);
  void CmdBindVertexBuffers(ExtObject *o, uint32_t pass);
  void CmdBindIndexBuffer(ExtObject *o, uint32_t pass);
  void CmdDraw(ExtObject *o, uint32_t pass);
  void CmdDrawIndirect(ExtObject *o, uint32_t pass);
  void CmdDrawIndexed(ExtObject *o, uint32_t pass);
  void CmdDrawIndexedIndirect(ExtObject *o, uint32_t pass);
  void CmdDispatch(ExtObject *o, uint32_t pass);
  void CmdDispatchIndirect(ExtObject *o, uint32_t pass);
  void CmdSetEvent(ExtObject *o, uint32_t pass);
  void CmdResetEvent(ExtObject *o, uint32_t pass);
  void CmdWaitEvents(ExtObject *o, uint32_t pass);
  void CmdPipelineBarrier(ExtObject *o, uint32_t pass);
  void CmdPushConstants(ExtObject *o, uint32_t pass);
  void CmdSetDepthBias(ExtObject *o, uint32_t pass);
  void CmdSetDepthBounds(ExtObject *o, uint32_t pass);
  void CmdSetStencilCompareMask(ExtObject *o, uint32_t pass);
  void CmdSetStencilWriteMask(ExtObject *o, uint32_t pass);
  void CmdSetStencilReference(ExtObject *o, uint32_t pass);
  void CmdSetLineWidth(ExtObject *o, uint32_t pass);
  void CmdCopyBuffer(ExtObject *o, uint32_t pass);
  void CmdUpdateBuffer(ExtObject *o, uint32_t pass);
  void CmdFillBuffer(ExtObject *o, uint32_t pass);
  void CmdCopyImage(ExtObject *o, uint32_t pass);
  void CmdBlitImage(ExtObject *o, uint32_t pass);
  void CmdResolveImage(ExtObject *o, uint32_t pass);
  void CmdSetBlendConstants(ExtObject *o, uint32_t pass);
  void CmdCopyBufferToImage(ExtObject *o, uint32_t pass);
  void CmdCopyImageToBuffer(ExtObject *o, uint32_t pass);
  void CmdClearAttachments(ExtObject *o, uint32_t pass);
  void CmdClearDepthStencilImage(ExtObject *o, uint32_t pass);
  void CmdClearColorImage(ExtObject *o, uint32_t pass);

  void EndFramePresent(ExtObject *o, uint32_t pass);
  void EndFrameWaitIdle(ExtObject *o, uint32_t pass);
  void InitialContents(ExtObject *o);
  void InitialLayouts(ExtObject *o, uint32_t pass);

  void PrintReadBuffers(StructuredBufferList &buffers);
};

}    // namespace vk_cpp_codec