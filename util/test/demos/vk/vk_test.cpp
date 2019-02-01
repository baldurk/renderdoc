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

#include "../test_common.h"

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0

#define VMA_ASSERT(expr) TEST_ASSERT(expr, "VMA assertion failed");

#pragma warning(push)
#pragma warning(disable : 4127)

#include "vk_headers.h"

#pragma warning(pop)

#include "vk_test.h"

#if defined(WIN32)
#include "../win32/win32_window.h"
#else
#include "../linux/linux_window.h"
#endif

static VkBool32 VKAPI_PTR vulkanCallback(VkDebugReportFlagsEXT flags,
                                         VkDebugReportObjectTypeEXT objectType, uint64_t object,
                                         size_t location, int32_t messageCode,
                                         const char *pLayerPrefix, const char *pMessage,
                                         void *pUserData)
{
  TEST_WARN("Vulkan message: [%s] %s", pLayerPrefix, pMessage);

  return false;
}

VulkanGraphicsTest::VulkanGraphicsTest()
{
  features.depthClamp = true;
}

bool VulkanGraphicsTest::Init(int argc, char **argv)
{
  // parse parameters here to override parameters
  GraphicsTest::Init(argc, argv);

  if(volkInitialize() != VK_SUCCESS)
  {
    TEST_ERROR("Couldn't init vulkan");
    return false;
  }

  if(!SpvCompilationSupported())
  {
    TEST_ERROR("glslc must be in PATH to run vulkan tests");
    return false;
  }

  instExts.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(WIN32)
  instExts.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#else
  instExts.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);

  X11Window::Init();
#endif

  if(debugDevice)
    instExts.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

  optInstExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  std::vector<const char *> layers;

  std::vector<VkLayerProperties> supportedLayers;
  CHECK_VKR(vkh::enumerateInstanceLayerProperties(supportedLayers));

  if(debugDevice)
  {
    for(const VkLayerProperties &layer : supportedLayers)
    {
      if(!strcmp(layer.layerName, "VK_LAYER_LUNARG_standard_validation"))
      {
        layers.push_back(layer.layerName);
        break;
      }
    }
  }

  std::vector<VkExtensionProperties> supportedExts;
  CHECK_VKR(vkh::enumerateInstanceExtensionProperties(supportedExts, NULL));

  for(const char *search : instExts)
  {
    bool found = false;
    for(VkExtensionProperties &ext : supportedExts)
    {
      if(!strcmp(ext.extensionName, search))
      {
        found = true;
        break;
      }
    }

    if(!found)
    {
      TEST_ERROR("Required instance extension '%s' missing", search);
      return false;
    }
  }

  // add any optional extensions that are supported
  for(const char *search : optInstExts)
  {
    bool found = false;
    for(VkExtensionProperties &ext : supportedExts)
    {
      if(!strcmp(ext.extensionName, search))
      {
        found = true;
        break;
      }
    }

    if(found)
      instExts.push_back(search);
  }

  vkh::ApplicationInfo app("RenderDoc autotesting", VK_MAKE_VERSION(1, 0, 0),
                           "RenderDoc autotesting", VK_MAKE_VERSION(1, 0, 0), VK_API_VERSION_1_0);

  CHECK_VKR(vkCreateInstance(vkh::InstanceCreateInfo(app, layers, instExts), NULL, &instance));

  volkLoadInstance((VkInstance)instance);

  if(debugDevice)
  {
    CHECK_VKR(vkCreateDebugReportCallbackEXT(
        instance,
        vkh::DebugReportCallbackCreateInfoEXT(
            VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT, &vulkanCallback),
        NULL, &debugReportCallback));
  }

  std::vector<VkPhysicalDevice> physDevices;
  CHECK_VKR(vkh::enumeratePhysicalDevices(physDevices, instance));

  if(physDevices.empty())
  {
    TEST_ERROR("No vulkan devices available");
    return false;
  }

  std::vector<VkPhysicalDeviceProperties> physProps;
  for(VkPhysicalDevice p : physDevices)
  {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(p, &props);
    physProps.push_back(props);
  }

  // default to the first discrete card
  for(size_t i = 0; i < physDevices.size(); i++)
  {
    if(physProps[i].deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
    {
      phys = physDevices[i];
      break;
    }
  }

  // if none found, default to first
  if(phys == VK_NULL_HANDLE)
    phys = physDevices[0];

  // allow command line override
  for(int i = 0; i < argc; i++)
  {
    if(!strcmp(argv[i], "--gpu") && i + 1 < argc)
    {
      std::string needle = strlower(argv[i + 1]);

      const bool nv = (needle == "nv" || needle == "nvidia");
      const bool amd = (needle == "amd");
      const bool intel = (needle == "intel");

      for(size_t p = 0; p < physDevices.size(); p++)
      {
        std::string haystack = strlower(physProps[p].deviceName);

        if(haystack.find(needle) != std::string::npos || (nv && physProps[p].vendorID == 0x10DE) ||
           (amd && physProps[p].vendorID == 0x1002) || (intel && physProps[p].vendorID == 0x8086))
        {
          phys = physDevices[p];
          break;
        }
      }

      break;
    }
  }

  std::vector<VkQueueFamilyProperties> queueProps;
  vkh::getQueueFamilyProperties(queueProps, phys);

  VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

  for(uint32_t q = 0; q < queueProps.size(); q++)
  {
    VkQueueFlags flags = queueProps[q].queueFlags;
    if((flags & required) == required)
    {
      queueFamilyIndex = q;
      break;
    }
  }

  if(queueFamilyIndex == ~0U)
  {
    TEST_ERROR("No graphics/compute queues available");
    return false;
  }

  mainWindow = MakeWindow(screenWidth, screenHeight, "Autotesting");

  VkResult vkr = (VkResult)CreateSurface(mainWindow, &surface);

  if(vkr != VK_SUCCESS)
  {
    TEST_ERROR("Error creating surface: %s", vkh::result_str(vkr));
    return false;
  };

  devExts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  VkPhysicalDeviceFeatures supported;
  vkGetPhysicalDeviceFeatures(phys, &supported);

  const VkBool32 *enabledBegin = (VkBool32 *)&features;
  const VkBool32 *enabledEnd = enabledBegin + sizeof(features);

  const VkBool32 *supportedBegin = (VkBool32 *)&features;

  for(; enabledBegin != enabledEnd; ++enabledBegin, ++supportedBegin)
  {
    if(*enabledBegin && !*supportedBegin)
    {
      TEST_ERROR("Feature enabled that isn't supported");
      return false;
    }
  }

  supportedExts.clear();
  CHECK_VKR(vkh::enumerateDeviceExtensionProperties(supportedExts, phys, NULL));

  for(const char *search : devExts)
  {
    bool found = false;
    for(VkExtensionProperties &ext : supportedExts)
    {
      if(!strcmp(ext.extensionName, search))
      {
        found = true;
        break;
      }
    }

    if(!found)
    {
      // try the layers we're enabling
      for(const char *layer : layers)
      {
        std::vector<VkExtensionProperties> layerExts;
        CHECK_VKR(vkh::enumerateDeviceExtensionProperties(layerExts, phys, layer));

        for(VkExtensionProperties &ext : layerExts)
        {
          if(!strcmp(ext.extensionName, search))
          {
            found = true;
            break;
          }
        }

        if(found)
          break;
      }

      if(!found)
      {
        TEST_ERROR("Required device extension '%s' missing", search);
        return false;
      }
    }
  }

  // add any optional extensions that are supported
  for(const char *search : optDevExts)
  {
    bool found = false;
    for(VkExtensionProperties &ext : supportedExts)
    {
      if(!strcmp(ext.extensionName, search))
      {
        found = true;
        break;
      }
    }

    if(found)
      devExts.push_back(search);
  }

  CHECK_VKR(vkCreateDevice(phys,
                           vkh::DeviceCreateInfo({vkh::DeviceQueueCreateInfo(queueFamilyIndex, 1)},
                                                 layers, devExts, features)
                               .next(devInfoNext),
                           NULL, &device));

  volkLoadDevice(device);

  vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

  CHECK_VKR(vkCreateSemaphore(device, vkh::SemaphoreCreateInfo(), NULL, &renderStartSemaphore));
  CHECK_VKR(vkCreateSemaphore(device, vkh::SemaphoreCreateInfo(), NULL, &renderEndSemaphore));

  CHECK_VKR(vkCreateCommandPool(
      device, vkh::CommandPoolCreateInfo(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT), NULL,
      &cmdPool));

  createSwap();

  acquireImage();

  VmaVulkanFunctions funcs = {
      vkGetPhysicalDeviceProperties,
      vkGetPhysicalDeviceMemoryProperties,
      vkAllocateMemory,
      vkFreeMemory,
      vkMapMemory,
      vkUnmapMemory,
      vkFlushMappedMemoryRanges,
      vkInvalidateMappedMemoryRanges,
      vkBindBufferMemory,
      vkBindImageMemory,
      vkGetBufferMemoryRequirements,
      vkGetImageMemoryRequirements,
      vkCreateBuffer,
      vkDestroyBuffer,
      vkCreateImage,
      vkDestroyImage,
      vkGetBufferMemoryRequirements2KHR,
      vkGetImageMemoryRequirements2KHR,
  };

  VmaAllocatorCreateInfo allocInfo = {};
  allocInfo.physicalDevice = phys;
  allocInfo.device = device;
  allocInfo.frameInUseCount = uint32_t(swapImages.size() - 1);
  allocInfo.pVulkanFunctions = &funcs;

  vmaCreateAllocator(&allocInfo, &allocator);

  return true;
}

bool VulkanGraphicsTest::IsSupported()
{
  if(volkGetInstanceVersion() > 0)
    return true;

  static bool glslcChecked = false;

  if(!glslcChecked)
  {
    static bool glslcSupported = SpvCompilationSupported();

    if(!glslcSupported)
      return false;
  }

  return volkInitialize() == VK_SUCCESS;
}

GraphicsWindow *VulkanGraphicsTest::MakeWindow(int width, int height, const char *title)
{
#if defined(WIN32)
  return new Win32Window(width, height, title);
#else
  return new X11Window(width, height, title);
#endif
}

VulkanGraphicsTest::~VulkanGraphicsTest()
{
  if(volkGetInstanceVersion() == 0)
    return;

  vmaDestroyAllocator(allocator);

  if(device)
  {
    vkDeviceWaitIdle(device);

    for(VkFence fence : fences)
      vkDestroyFence(device, fence, NULL);

    for(VkShaderModule shader : shaders)
      vkDestroyShaderModule(device, shader, NULL);

    for(VkDescriptorPool pool : descPools)
      vkDestroyDescriptorPool(device, pool, NULL);

    for(VkPipeline pipe : pipes)
      vkDestroyPipeline(device, pipe, NULL);

    for(VkFramebuffer fb : framebuffers)
      vkDestroyFramebuffer(device, fb, NULL);

    for(VkRenderPass rp : renderpasses)
      vkDestroyRenderPass(device, rp, NULL);

    for(VkImageView view : imageviews)
      vkDestroyImageView(device, view, NULL);

    for(VkPipelineLayout layout : pipelayouts)
      vkDestroyPipelineLayout(device, layout, NULL);

    for(VkDescriptorSetLayout layout : setlayouts)
      vkDestroyDescriptorSetLayout(device, layout, NULL);

    vkDestroyCommandPool(device, cmdPool, NULL);
    vkDestroySemaphore(device, renderStartSemaphore, NULL);
    vkDestroySemaphore(device, renderEndSemaphore, NULL);

    destroySwap();

    vkDestroyDevice(device, NULL);
  }

  if(debugReportCallback)
    vkDestroyDebugReportCallbackEXT(instance, debugReportCallback, NULL);
  if(surface)
    vkDestroySurfaceKHR(instance, surface, NULL);

  if(instance)
    vkDestroyInstance(instance, NULL);

  delete mainWindow;
}

bool VulkanGraphicsTest::Running()
{
  if(!FrameLimit())
    return false;

  return mainWindow->Update();
}

VkImage VulkanGraphicsTest::StartUsingBackbuffer(VkCommandBuffer cmd, VkAccessFlags nextUse,
                                                 VkImageLayout layout)
{
  VkImage img = swapImages[swapIndex];

  vkh::cmdPipelineBarrier(
      cmd, {
               vkh::ImageMemoryBarrier(0, nextUse, VK_IMAGE_LAYOUT_UNDEFINED, layout, img),
           });

  return img;
}

void VulkanGraphicsTest::FinishUsingBackbuffer(VkCommandBuffer cmd, VkAccessFlags prevUse,
                                               VkImageLayout layout)
{
  VkImage img = swapImages[swapIndex];

  vkh::cmdPipelineBarrier(cmd, {
                                   vkh::ImageMemoryBarrier(prevUse, VK_ACCESS_MEMORY_READ_BIT, layout,
                                                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, img),
                               });
}

void VulkanGraphicsTest::Submit(int index, int totalSubmits, const std::vector<VkCommandBuffer> &cmds,
                                const std::vector<VkCommandBuffer> &seccmds)
{
  VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

  VkSubmitInfo submit = vkh::SubmitInfo(cmds);

  if(index == 0)
  {
    submit.waitSemaphoreCount = 1;
    submit.pWaitDstStageMask = &waitStage;
    submit.pWaitSemaphores = &renderStartSemaphore;
  }

  if(index == totalSubmits - 1)
  {
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderEndSemaphore;
  }

  VkFence fence;
  CHECK_VKR(vkCreateFence(device, vkh::FenceCreateInfo(), NULL, &fence));

  fences.insert(fence);

  for(const VkCommandBuffer &cmd : cmds)
    pendingCommandBuffers[0].push_back(std::make_pair(cmd, fence));

  for(const VkCommandBuffer &cmd : seccmds)
    pendingCommandBuffers[1].push_back(std::make_pair(cmd, fence));

  vkQueueSubmit(queue, 1, &submit, fence);
}

void VulkanGraphicsTest::Present()
{
  VkResult vkr = vkQueuePresentKHR(queue, vkh::PresentInfoKHR(swap, swapIndex, &renderEndSemaphore));

  if(vkr == VK_SUBOPTIMAL_KHR || vkr == VK_ERROR_OUT_OF_DATE_KHR)
    resize();

  vkQueueWaitIdle(queue);

  std::set<VkFence> doneFences;

  for(int level = 0; level < VK_COMMAND_BUFFER_LEVEL_RANGE_SIZE; level++)
  {
    for(auto it = pendingCommandBuffers[level].begin(); it != pendingCommandBuffers[level].end();)
    {
      if(vkGetFenceStatus(device, it->second) == VK_SUCCESS)
      {
        freeCommandBuffers[level].push_back(it->first);
        doneFences.insert(it->second);
        it = pendingCommandBuffers[level].erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

  for(auto it = doneFences.begin(); it != doneFences.end(); ++it)
  {
    vkDestroyFence(device, *it, NULL);
    fences.erase(*it);
  }

  acquireImage();
}

VkPipelineShaderStageCreateInfo VulkanGraphicsTest::CompileShaderModule(
    const std::string &source_text, ShaderLang lang, ShaderStage stage, const char *entry_point)
{
  VkShaderModule ret = VK_NULL_HANDLE;

  std::vector<uint32_t> spirv =
      ::CompileShaderToSpv(source_text, SPIRVTarget::vulkan, lang, stage, entry_point);

  if(spirv.empty())
    return {};

  CHECK_VKR(vkCreateShaderModule(device, vkh::ShaderModuleCreateInfo(spirv), NULL, &ret));

  shaders.push_back(ret);

  VkShaderStageFlagBits vkstage[] = {
      VK_SHADER_STAGE_VERTEX_BIT,
      VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
      VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
      VK_SHADER_STAGE_GEOMETRY_BIT,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      VK_SHADER_STAGE_COMPUTE_BIT,
  };

  return vkh::PipelineShaderStageCreateInfo(ret, vkstage[(int)stage], entry_point);
}

VkCommandBuffer VulkanGraphicsTest::GetCommandBuffer(VkCommandBufferLevel level)
{
  std::vector<VkCommandBuffer> &buflist = freeCommandBuffers[level];

  if(buflist.empty())
  {
    buflist.resize(4);
    CHECK_VKR(vkAllocateCommandBuffers(device, vkh::CommandBufferAllocateInfo(cmdPool, 4, level),
                                       &buflist[0]));
  }

  VkCommandBuffer ret = buflist.back();
  buflist.pop_back();

  return ret;
}

template <>
void VulkanGraphicsTest::setName(VkPipeline obj, const std::string &name)
{
  setName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)(uintptr_t)obj, name);
}

void VulkanGraphicsTest::setName(VkObjectType objType, uint64_t obj, const std::string &name)
{
  if(vkSetDebugUtilsObjectNameEXT)
  {
    VkDebugUtilsObjectNameInfoEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
    info.objectType = objType;
    info.objectHandle = obj;
    info.pObjectName = name.c_str();
    vkSetDebugUtilsObjectNameEXT(device, &info);
  }
}

void VulkanGraphicsTest::pushMarker(VkCommandBuffer cmd, const std::string &name)
{
  if(vkCmdBeginDebugUtilsLabelEXT)
  {
    VkDebugUtilsLabelEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pLabelName = name.c_str();
    vkCmdBeginDebugUtilsLabelEXT(cmd, &info);
  }
}

void VulkanGraphicsTest::setMarker(VkCommandBuffer cmd, const std::string &name)
{
  if(vkCmdInsertDebugUtilsLabelEXT)
  {
    VkDebugUtilsLabelEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pLabelName = name.c_str();
    vkCmdInsertDebugUtilsLabelEXT(cmd, &info);
  }
}

void VulkanGraphicsTest::popMarker(VkCommandBuffer cmd)
{
  if(vkCmdEndDebugUtilsLabelEXT)
  {
    vkCmdEndDebugUtilsLabelEXT(cmd);
  }
}

VkDescriptorSet VulkanGraphicsTest::allocateDescriptorSet(VkDescriptorSetLayout setLayout)
{
  VkDescriptorSet ret = VK_NULL_HANDLE;

  if(!descPools.empty())
  {
    VkDescriptorPool pool = descPools.back();
    VkResult vkr =
        vkAllocateDescriptorSets(device, vkh::DescriptorSetAllocateInfo(pool, {setLayout}), &ret);
    if(vkr == VK_SUCCESS)
      return ret;
  }

  // failed to allocate, create a new pool and push it
  {
    VkDescriptorPool pool = VK_NULL_HANDLE;
    CHECK_VKR(vkCreateDescriptorPool(
        device, vkh::DescriptorPoolCreateInfo(128,
                                              {
                                                  {VK_DESCRIPTOR_TYPE_SAMPLER, 1024},
                                                  {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024},
                                                  {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024},
                                                  {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1024},
                                                  {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1024},
                                                  {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1024},
                                                  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1024},
                                                  {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1024},
                                                  {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1024},
                                                  {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1024},
                                                  {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1024},
                                              }),
        NULL, &pool));
    descPools.push_back(pool);

    // this must succeed or we can't continue.
    CHECK_VKR(
        vkAllocateDescriptorSets(device, vkh::DescriptorSetAllocateInfo(pool, {setLayout}), &ret));
    return ret;
  }
}

VkPipeline VulkanGraphicsTest::createGraphicsPipeline(const VkGraphicsPipelineCreateInfo *info)
{
  VkPipeline ret;
  CHECK_VKR(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, info, NULL, &ret));
  pipes.push_back(ret);
  return ret;
}

VkPipeline VulkanGraphicsTest::createComputePipeline(const VkComputePipelineCreateInfo *info)
{
  VkPipeline ret;
  CHECK_VKR(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, info, NULL, &ret));
  pipes.push_back(ret);
  return ret;
}

VkFramebuffer VulkanGraphicsTest::createFramebuffer(const VkFramebufferCreateInfo *info)
{
  VkFramebuffer ret;
  CHECK_VKR(vkCreateFramebuffer(device, info, NULL, &ret));
  framebuffers.push_back(ret);
  return ret;
}

VkRenderPass VulkanGraphicsTest::createRenderPass(const VkRenderPassCreateInfo *info)
{
  VkRenderPass ret;
  CHECK_VKR(vkCreateRenderPass(device, info, NULL, &ret));
  renderpasses.push_back(ret);
  return ret;
}

VkImageView VulkanGraphicsTest::createImageView(const VkImageViewCreateInfo *info)
{
  VkImageView ret;
  CHECK_VKR(vkCreateImageView(device, info, NULL, &ret));
  imageviews.push_back(ret);
  return ret;
}

VkPipelineLayout VulkanGraphicsTest::createPipelineLayout(const VkPipelineLayoutCreateInfo *info)
{
  VkPipelineLayout ret;
  CHECK_VKR(vkCreatePipelineLayout(device, info, NULL, &ret));
  pipelayouts.push_back(ret);
  return ret;
}

VkDescriptorSetLayout VulkanGraphicsTest::createDescriptorSetLayout(
    const VkDescriptorSetLayoutCreateInfo *info)
{
  VkDescriptorSetLayout ret;
  CHECK_VKR(vkCreateDescriptorSetLayout(device, info, NULL, &ret));
  setlayouts.push_back(ret);
  return ret;
}

void VulkanGraphicsTest::resize()
{
  destroySwap();

  createSwap();

  for(const std::function<void()> &cb : resizeCallbacks)
    cb();
}

bool VulkanGraphicsTest::createSwap()
{
  VkResult vkr = VK_SUCCESS;

  VkSurfaceFormatKHR format = {};

  std::vector<VkSurfaceFormatKHR> formats;
  CHECK_VKR(vkh::getSurfaceFormatsKHR(formats, phys, surface));

  VkBool32 support = VK_FALSE;
  CHECK_VKR(vkGetPhysicalDeviceSurfaceSupportKHR(phys, queueFamilyIndex, surface, &support));
  TEST_ASSERT(support, "Presentation is not supported on surface");

  if(vkr != VK_SUCCESS || formats.empty())
  {
    TEST_ERROR("Error getting surface formats: %s", vkh::result_str(vkr));
    return false;
  }

  format = formats[0];

  for(const VkSurfaceFormatKHR &f : formats)
  {
    if(f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
      format = f;
      break;
    }
  }

  if(format.format == VK_FORMAT_UNDEFINED)
  {
    format.format = VK_FORMAT_B8G8R8A8_SRGB;
    format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  }

  swapFormat = format.format;

  std::vector<VkPresentModeKHR> modes;
  CHECK_VKR(vkh::getSurfacePresentModesKHR(modes, phys, surface));

  VkPresentModeKHR mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

  if(std::find(modes.begin(), modes.end(), mode) == modes.end())
    mode = VK_PRESENT_MODE_FIFO_KHR;

  uint32_t width = 1, height = 1;

  VkSurfaceCapabilitiesKHR capabilities;
  CHECK_VKR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys, surface, &capabilities));

  width = capabilities.currentExtent.width;

  width = std::min(width, capabilities.maxImageExtent.width);
  width = std::max(width, capabilities.minImageExtent.width);

  height = capabilities.currentExtent.height;

  height = std::min(height, capabilities.maxImageExtent.height);
  height = std::max(height, capabilities.minImageExtent.height);

  viewport = vkh::Viewport(0, 0, (float)width, (float)height, 0.0f, 1.0f);
  scissor = vkh::Rect2D({0, 0}, {width, height});

  CHECK_VKR(vkCreateSwapchainKHR(
      device, vkh::SwapchainCreateInfoKHR(
                  surface, mode, format, {width, height},
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
      NULL, &swap));

  CHECK_VKR(vkh::getSwapchainImagesKHR(swapImages, device, swap));

  if(swapRenderPass == VK_NULL_HANDLE)
  {
    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(
        vkh::AttachmentDescription(swapFormat, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})});

    swapRenderPass = createRenderPass(renderPassCreateInfo);
  }

  swapImageViews.resize(swapImages.size());
  for(size_t i = 0; i < swapImages.size(); i++)
  {
    CHECK_VKR(vkCreateImageView(
        device, vkh::ImageViewCreateInfo(swapImages[i], VK_IMAGE_VIEW_TYPE_2D, format.format), NULL,
        &swapImageViews[i]));
  }
  swapFramebuffers.resize(swapImages.size());
  for(size_t i = 0; i < swapImageViews.size(); i++)
    swapFramebuffers[i] = createFramebuffer(
        vkh::FramebufferCreateInfo(swapRenderPass, {swapImageViews[i]}, scissor.extent));

  return true;
}

void VulkanGraphicsTest::destroySwap()
{
  vkDeviceWaitIdle(device);

  for(size_t i = 0; i < swapImages.size(); i++)
    vkDestroyImageView(device, swapImageViews[i], NULL);

  vkDestroySwapchainKHR(device, swap, NULL);
}

void VulkanGraphicsTest::acquireImage()
{
  VkResult vkr = vkAcquireNextImageKHR(device, swap, UINT64_MAX, renderStartSemaphore,
                                       VK_NULL_HANDLE, &swapIndex);

  if(vkr == VK_SUBOPTIMAL_KHR || vkr == VK_ERROR_OUT_OF_DATE_KHR)
  {
    resize();

    vkr = vkAcquireNextImageKHR(device, swap, UINT64_MAX, renderStartSemaphore, VK_NULL_HANDLE,
                                &swapIndex);
  }
}

VkResult VulkanGraphicsTest::CreateSurface(GraphicsWindow *win, VkSurfaceKHR *outSurf)
{
#if defined(WIN32)
  VkWin32SurfaceCreateInfoKHR createInfo;

  createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  createInfo.pNext = NULL;
  createInfo.flags = 0;
  createInfo.hwnd = ((Win32Window *)win)->wnd;
  createInfo.hinstance = GetModuleHandleA(NULL);

  return vkCreateWin32SurfaceKHR(instance, &createInfo, NULL, outSurf);
#else
  VkXcbSurfaceCreateInfoKHR createInfo;

  createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
  createInfo.pNext = NULL;
  createInfo.flags = 0;
  createInfo.connection = ((X11Window *)win)->xcb.connection;
  createInfo.window = ((X11Window *)win)->xcb.window;

  return vkCreateXcbSurfaceKHR(instance, &createInfo, NULL, outSurf);
#endif
}

template <>
VkFormat vkh::_FormatFromObj<Vec4f>()
{
  return VK_FORMAT_R32G32B32A32_SFLOAT;
}
template <>
VkFormat vkh::_FormatFromObj<Vec3f>()
{
  return VK_FORMAT_R32G32B32_SFLOAT;
}
template <>
VkFormat vkh::_FormatFromObj<Vec2f>()
{
  return VK_FORMAT_R32G32_SFLOAT;
}