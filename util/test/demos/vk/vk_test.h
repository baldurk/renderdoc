/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2019-2021 Baldur Karlsson
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
#include <mutex>
#include <set>
#include <vector>
#include "../test_common.h"
#include "vk_headers.h"
#include "vk_helpers.h"
#include "vk_test.h"

struct VulkanGraphicsTest;

struct AllocatedBuffer
{
  VulkanGraphicsTest *test = NULL;
  VmaAllocator allocator = NULL;
  VkBuffer buffer = VK_NULL_HANDLE;
  VmaAllocation alloc = {};

  AllocatedBuffer() {}
  AllocatedBuffer(VulkanGraphicsTest *test, const VkBufferCreateInfo &bufInfo,
                  const VmaAllocationCreateInfo &allocInfo);

  void free();

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
  VulkanGraphicsTest *test = NULL;
  VmaAllocator allocator = NULL;
  VkImage image = VK_NULL_HANDLE;
  VmaAllocation alloc = {};
  VkImageCreateInfo createInfo;

  AllocatedImage() {}
  AllocatedImage(VulkanGraphicsTest *test, const VkImageCreateInfo &imgInfo,
                 const VmaAllocationCreateInfo &allocInfo);

  void free();
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

struct VulkanGraphicsTest;

struct VulkanWindow : public GraphicsWindow
{
  VkFormat format;
  uint32_t imgIndex = 0;
  VkRenderPass rp = VK_NULL_HANDLE;
  VkViewport viewport;
  VkRect2D scissor;

  VulkanWindow(VulkanGraphicsTest *test, GraphicsWindow *win);
  virtual ~VulkanWindow();
  void Shutdown();

  size_t GetCount() { return imgs.size(); }
  VkImage GetImage(size_t idx = ~0U)
  {
    if(idx == ~0U)
      idx = imgIndex;
    return imgs[idx];
  }
  VkImageView GetView(size_t idx = ~0U)
  {
    if(idx == ~0U)
      idx = imgIndex;
    return imgviews[idx];
  }
  VkFramebuffer GetFB(size_t idx = ~0U)
  {
    if(idx == ~0U)
      idx = imgIndex;
    return fbs[idx];
  }
  bool Initialised() { return swap != VK_NULL_HANDLE; }
  VkCommandBuffer GetCommandBuffer(VkCommandBufferLevel level);
  void Submit(int index, int totalSubmits, const std::vector<VkCommandBuffer> &cmds,
              const std::vector<VkCommandBuffer> &seccmds, VkQueue q, bool sync2);
  void Present(VkQueue q);
  void Acquire();

  // forward GraphicsWindow functions to internal window
  void Resize(int width, int height) { m_Win->Resize(width, height); }
  bool Update() { return m_Win->Update(); }
private:
  bool CreateSwapchain();
  void DestroySwapchain();

  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkSwapchainKHR swap = VK_NULL_HANDLE;
  std::vector<VkImage> imgs;
  std::vector<VkImageView> imgviews;
  VkSemaphore renderStartSemaphore = VK_NULL_HANDLE, renderEndSemaphore = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> fbs;

  VkCommandPool cmdPool;
  std::set<VkFence> fences;

  std::vector<VkCommandBuffer> freeCommandBuffers[2];
  std::vector<std::pair<VkCommandBuffer, VkFence>> pendingCommandBuffers[2];

  GraphicsWindow *m_Win;
  VulkanGraphicsTest *m_Test;
};

struct VulkanGraphicsTest : public GraphicsTest
{
  static const TestAPI API = TestAPI::Vulkan;

  VulkanGraphicsTest();

  void Prepare(int argc, char **argv);
  bool Init();
  void Shutdown();
  VulkanWindow *MakeWindow(int width, int height, const char *title);

  bool Running();
  VkImage StartUsingBackbuffer(VkCommandBuffer cmd, VkAccessFlags nextUse, VkImageLayout layout,
                               VulkanWindow *window = NULL);
  void FinishUsingBackbuffer(VkCommandBuffer cmd, VkAccessFlags prevUse, VkImageLayout layout,
                             VulkanWindow *window = NULL);
  void Submit(int index, int totalSubmits, const std::vector<VkCommandBuffer> &cmds,
              const std::vector<VkCommandBuffer> &seccmds = {}, VulkanWindow *window = NULL,
              VkQueue q = VK_NULL_HANDLE, bool sync2 = false);
  void Present(VulkanWindow *window = NULL, VkQueue q = VK_NULL_HANDLE);

  VkPipelineShaderStageCreateInfo CompileShaderModule(
      const std::string &source_text, ShaderLang lang, ShaderStage stage,
      const char *entry_point = "main", const std::map<std::string, std::string> &macros = {},
      SPIRVTarget target = SPIRVTarget::vulkan);
  VkCommandBuffer GetCommandBuffer(VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                   VulkanWindow *window = NULL);

  void setName(VkObjectType objType, uint64_t obj, const std::string &name);
  void pushMarker(VkCommandBuffer cmd, const std::string &name);
  void setMarker(VkCommandBuffer cmd, const std::string &name);
  void popMarker(VkCommandBuffer cmd);

  void pushMarker(VkQueue queue, const std::string &name);
  void setMarker(VkQueue queue, const std::string &name);
  void popMarker(VkQueue queue);

  void blitToSwap(VkCommandBuffer cmd, VkImage src, VkImageLayout srcLayout, VkImage dst,
                  VkImageLayout dstLayout);

  void uploadBufferToImage(VkImage destImage, VkExtent3D destExtent, VkBuffer srcBuffer,
                           VkImageLayout finalLayout);

  template <typename T>
  void setName(T obj, const std::string &name);

  VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout setLayout);
  VkPipeline createGraphicsPipeline(const VkGraphicsPipelineCreateInfo *info);
  VkPipeline createComputePipeline(const VkComputePipelineCreateInfo *info);
  VkFramebuffer createFramebuffer(const VkFramebufferCreateInfo *info);
  VkRenderPass createRenderPass(const VkRenderPassCreateInfo *info);
  VkImageView createImageView(const VkImageViewCreateInfo *info);
  VkBufferView createBufferView(const VkBufferViewCreateInfo *info);
  VkPipelineLayout createPipelineLayout(const VkPipelineLayoutCreateInfo *info);
  VkDescriptorSetLayout createDescriptorSetLayout(const VkDescriptorSetLayoutCreateInfo *info);
  VkSampler createSampler(const VkSamplerCreateInfo *info);

  void getPhysFeatures2(void *nextStruct);
  void getPhysProperties2(void *nextStruct);

  std::mutex mutex;

  // instance version
  uint32_t instVersion;
  // device version
  uint32_t devVersion;

  // a custom struct to pass to vkInstanceCreateInfo::pNext
  const void *instInfoNext = NULL;

  // requested features
  VkPhysicalDeviceFeatures features = {};
  VkPhysicalDeviceFeatures optFeatures = {};

  // enabled instance extensions
  std::vector<const char *> instExts;
  std::vector<const char *> instLayers;

  // required extensions before Init(), enabled extensions after Init()
  std::vector<const char *> devExts;

  // optional extensions, will be added to devExts if supported (allows fallback paths)
  std::vector<const char *> optDevExts;

  bool hasExt(const char *ext);

  // a custom struct to pass to vkDeviceCreateInfo::pNext
  const void *devInfoNext = NULL;

  // core objects
  VkInstance instance;
  VkPhysicalDevice phys;
  VkDevice device;
  uint32_t queueFamilyIndex = ~0U;
  uint32_t queueCount;
  VkQueue queue;
  VkPhysicalDeviceProperties physProperties;

  // utilities
  VkDebugUtilsMessengerEXT debugUtilsMessenger;

  // tracking object lifetimes
  std::vector<VkShaderModule> shaders;
  std::vector<VkDescriptorPool> descPools;
  std::vector<VkPipeline> pipes;
  std::vector<VkFramebuffer> framebuffers;
  std::vector<VkRenderPass> renderpasses;
  std::vector<VkImageView> imageviews;
  std::vector<VkBufferView> bufferviews;
  std::vector<VkPipelineLayout> pipelayouts;
  std::vector<VkDescriptorSetLayout> setlayouts;
  std::vector<VkSampler> samplers;

  std::map<VkImage, VmaAllocation> imageAllocs;
  std::map<VkBuffer, VmaAllocation> bufferAllocs;

  VulkanWindow *mainWindow = NULL;

  // VMA
  bool vmaDedicated = false;
  VmaAllocator allocator = VK_NULL_HANDLE;

private:
  static bool prepared_vk;

  GraphicsWindow *MakePlatformWindow(int width, int height, const char *title);
};

extern std::string VKDefaultVertex;
extern std::string VKDefaultPixel;
