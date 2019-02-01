/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2018-2019 Baldur Karlsson
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

#include <functional>
#include <set>
#include <vector>
#include "../test_common.h"
#include "vk_headers.h"
#include "vk_helpers.h"
#include "vk_test.h"

struct AllocatedBuffer
{
  VmaAllocator allocator;
  VkBuffer buffer;
  VmaAllocation alloc;

  AllocatedBuffer() {}
  AllocatedBuffer(VmaAllocator allocator, const VkBufferCreateInfo &bufInfo,
                  const VmaAllocationCreateInfo &allocInfo)
  {
    create(allocator, bufInfo, allocInfo);
  }
  AllocatedBuffer(const AllocatedBuffer &) = delete;
  AllocatedBuffer &operator=(const AllocatedBuffer &) = delete;

  void create(VmaAllocator vma, const VkBufferCreateInfo &bufInfo,
              const VmaAllocationCreateInfo &allocInfo)
  {
    allocator = vma;
    VkBuffer buf;
    vmaCreateBuffer(allocator, &bufInfo, &allocInfo, &buf, &alloc, NULL);
    buffer = VkBuffer(buf);
  }

  ~AllocatedBuffer()
  {
    if(buffer != VK_NULL_HANDLE)
      vmaDestroyBuffer(allocator, (VkBuffer)buffer, alloc);
  }
  template <typename T, size_t N>
  void upload(const T (&data)[N])
  {
    upload(data, sizeof(T) * N);
  }

  void upload(const void *data, size_t size)
  {
    byte *ptr = map();
    if(ptr)
      memcpy(ptr, data, size);
    unmap();
  }

  byte *map()
  {
    byte *ret = NULL;
    VkResult vkr = vmaMapMemory(allocator, alloc, (void **)&ret);

    if(vkr != VK_SUCCESS)
      return NULL;

    return ret;
  }

  void unmap() { vmaUnmapMemory(allocator, alloc); }
};

struct AllocatedImage
{
  VmaAllocator allocator;
  VkImage image;
  VmaAllocation alloc;

  AllocatedImage() {}
  AllocatedImage(VmaAllocator allocator, const VkImageCreateInfo &imgInfo,
                 const VmaAllocationCreateInfo &allocInfo)
  {
    create(allocator, imgInfo, allocInfo);
  }
  AllocatedImage(const AllocatedImage &) = delete;
  AllocatedImage &operator=(const AllocatedImage &) = delete;

  void create(VmaAllocator vma, const VkImageCreateInfo &imgInfo,
              const VmaAllocationCreateInfo &allocInfo)
  {
    allocator = vma;
    VkImage img;
    vmaCreateImage(allocator, &imgInfo, &allocInfo, &img, &alloc, NULL);
    image = VkImage(img);
  }

  ~AllocatedImage()
  {
    if(image != VK_NULL_HANDLE)
      vmaDestroyImage(allocator, (VkImage)image, alloc);
  }
};

#define CHECK_VKR(cmd)                                                               \
  do                                                                                 \
  {                                                                                  \
    VkResult _vkr = cmd;                                                             \
    if(_vkr != VK_SUCCESS)                                                           \
    {                                                                                \
      fprintf(stdout, "%s:%d Vulkan Error: %s executing:\n%s\n", __FILE__, __LINE__, \
              vkh::result_str(_vkr), #cmd);                                          \
      fflush(stdout);                                                                \
      DEBUG_BREAK();                                                                 \
      exit(1);                                                                       \
    }                                                                                \
  } while(0);

struct VulkanGraphicsTest : public GraphicsTest
{
  static const TestAPI API = TestAPI::Vulkan;

  VulkanGraphicsTest();
  ~VulkanGraphicsTest();

  bool Init(int argc, char **argv);
  bool IsSupported();
  GraphicsWindow *MakeWindow(int width, int height, const char *title);
  VkResult CreateSurface(GraphicsWindow *win, VkSurfaceKHR *outSurf);

  bool Running();
  VkImage StartUsingBackbuffer(VkCommandBuffer cmd, VkAccessFlags nextUse, VkImageLayout layout);
  void FinishUsingBackbuffer(VkCommandBuffer cmd, VkAccessFlags prevUse, VkImageLayout layout);
  void Submit(int index, int totalSubmits, const std::vector<VkCommandBuffer> &cmds,
              const std::vector<VkCommandBuffer> &seccmds = {});
  void Present();

  VkPipelineShaderStageCreateInfo CompileShaderModule(const std::string &source_text,
                                                      ShaderLang lang, ShaderStage stage,
                                                      const char *entry_point = "main");
  VkCommandBuffer GetCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

  void setName(VkObjectType objType, uint64_t obj, const std::string &name);
  void pushMarker(VkCommandBuffer cmd, const std::string &name);
  void setMarker(VkCommandBuffer cmd, const std::string &name);
  void popMarker(VkCommandBuffer cmd);

  template <typename T>
  void setName(T obj, const std::string &name);

  VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout setLayout);
  VkPipeline createGraphicsPipeline(const VkGraphicsPipelineCreateInfo *info);
  VkPipeline createComputePipeline(const VkComputePipelineCreateInfo *info);
  VkFramebuffer createFramebuffer(const VkFramebufferCreateInfo *info);
  VkRenderPass createRenderPass(const VkRenderPassCreateInfo *info);
  VkImageView createImageView(const VkImageViewCreateInfo *info);
  VkPipelineLayout createPipelineLayout(const VkPipelineLayoutCreateInfo *info);
  VkDescriptorSetLayout createDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo *info);

  void resize();
  void onResize(std::function<void()> callback) { resizeCallbacks.push_back(callback); }
  bool createSwap();
  void destroySwap();
  void acquireImage();

  // requested features
  VkPhysicalDeviceFeatures features = {};

  // required extensions before Init(), enabled extensions after Init()
  std::vector<const char *> instExts;
  std::vector<const char *> devExts;

  // a custom struct to pass to vkDeviceCreateInfo::pNext
  const void *devInfoNext = NULL;

  // optional extensions
  std::vector<const char *> optInstExts;
  std::vector<const char *> optDevExts;

  // core objects
  VkInstance instance;
  VkPhysicalDevice phys;
  VkDevice device;
  uint32_t queueFamilyIndex;
  VkQueue queue;

  // swapchain stuff
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkSwapchainKHR swap;
  VkFormat swapFormat;
  std::vector<VkImage> swapImages;
  std::vector<VkImageView> swapImageViews;
  uint32_t swapIndex = 0;
  VkSemaphore renderStartSemaphore, renderEndSemaphore;
  std::vector<std::function<void()>> resizeCallbacks;
  VkRenderPass swapRenderPass;
  std::vector<VkFramebuffer> swapFramebuffers;

  // utilities
  VkDebugReportCallbackEXT debugReportCallback;
  VkCommandPool cmdPool;

  // tracking object lifetimes
  std::vector<VkShaderModule> shaders;
  std::vector<VkDescriptorPool> descPools;
  std::vector<VkPipeline> pipes;
  std::vector<VkFramebuffer> framebuffers;
  std::vector<VkRenderPass> renderpasses;
  std::vector<VkImageView> imageviews;
  std::vector<VkPipelineLayout> pipelayouts;
  std::vector<VkDescriptorSetLayout> setlayouts;

  VkViewport viewport;
  VkRect2D scissor;

  GraphicsWindow *mainWindow = NULL;

  // internal bookkeeping
  std::set<VkFence> fences;

  std::vector<VkCommandBuffer> freeCommandBuffers[VK_COMMAND_BUFFER_LEVEL_RANGE_SIZE];
  std::vector<std::pair<VkCommandBuffer, VkFence>>
      pendingCommandBuffers[VK_COMMAND_BUFFER_LEVEL_RANGE_SIZE];

  // VMA
  VmaAllocator allocator = VK_NULL_HANDLE;
};