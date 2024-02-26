/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <vector>

// nvidia uses its own version packing:
//   10 |  8  |        8       |       6
// major|minor|secondary_branch|tertiary_branch
#define VK_MAKE_VERSION_NV(major, minor, secondary_branch, tertiary_branch)        \
  (((major) << (8 + 8 + 6)) | ((minor) << (8 + 6)) | ((secondary_branch) << (6)) | \
   ((tertiary_branch) << (0)))

namespace vkh
{
const char *result_str(VkResult vkr);

template <typename ObjType>
VkFormat _FormatFromObj();

#define formatof(obj) _FormatFromObj<decltype(obj)>()

inline VkVertexInputAttributeDescription _VertexAttribute(uint32_t location, uint32_t binding,
                                                          VkFormat fmt, uint32_t offs)
{
  return {location, binding, fmt, offs};
}

#define vertexAttr(location, binding, baseStruct, member)                \
  _VertexAttribute(location, binding, vkh::formatof(baseStruct::member), \
                   offsetof(baseStruct, member))
#define vertexAttrFormatted(location, binding, baseStruct, member, format) \
  _VertexAttribute(location, binding, format, offsetof(baseStruct, member))

inline VkVertexInputBindingDescription _VertexBinding(uint32_t binding, uint32_t stride,
                                                      VkVertexInputRate rate)
{
  return {binding, stride, rate};
}

#define vertexBind(binding, baseStruct) \
  _VertexBinding(binding, sizeof(baseStruct), VK_VERTEX_INPUT_RATE_VERTEX)
#define instanceBind(binding, baseStruct) \
  _VertexBinding(binding, sizeof(baseStruct), VK_VERTEX_INPUT_RATE_INSTANCE)

// unfortunately we can't use this macro everywhere because it doesn't work for __VA_ARGS__ = 0
#define VKH_ENUMERATE(func, ...)   \
  uint32_t count = 0;              \
  func(__VA_ARGS__, &count, NULL); \
  vec.resize(count);               \
  return func(__VA_ARGS__, &count, &vec[0]);

inline VkResult enumerateInstanceLayerProperties(std::vector<VkLayerProperties> &vec)
{
  uint32_t count = 0;
  vkEnumerateInstanceLayerProperties(&count, NULL);
  vec.resize(count);
  return vkEnumerateInstanceLayerProperties(&count, &vec[0]);
}

inline VkResult enumerateInstanceExtensionProperties(std::vector<VkExtensionProperties> &vec,
                                                     const char *pLayerName)
{
  VKH_ENUMERATE(vkEnumerateInstanceExtensionProperties, pLayerName);
}

inline VkResult enumeratePhysicalDevices(std::vector<VkPhysicalDevice> &vec, VkInstance instance)
{
  VKH_ENUMERATE(vkEnumeratePhysicalDevices, instance);
}

inline VkResult enumerateDeviceLayerProperties(std::vector<VkLayerProperties> &vec,
                                               VkPhysicalDevice physDev)
{
  VKH_ENUMERATE(vkEnumerateDeviceLayerProperties, physDev);
}

inline VkResult enumerateDeviceExtensionProperties(std::vector<VkExtensionProperties> &vec,
                                                   VkPhysicalDevice physDev, const char *pLayerName)
{
  VKH_ENUMERATE(vkEnumerateDeviceExtensionProperties, physDev, pLayerName);
}

inline void getQueueFamilyProperties(std::vector<VkQueueFamilyProperties> &vec,
                                     VkPhysicalDevice physDev)
{
  VKH_ENUMERATE(vkGetPhysicalDeviceQueueFamilyProperties, physDev);
}

inline VkResult getSurfaceFormatsKHR(std::vector<VkSurfaceFormatKHR> &vec, VkPhysicalDevice physDev,
                                     VkSurfaceKHR surface)
{
  VKH_ENUMERATE(vkGetPhysicalDeviceSurfaceFormatsKHR, physDev, surface);
}

inline VkResult getSurfacePresentModesKHR(std::vector<VkPresentModeKHR> &vec,
                                          VkPhysicalDevice physDev, VkSurfaceKHR surface)
{
  VKH_ENUMERATE(vkGetPhysicalDeviceSurfacePresentModesKHR, physDev, surface);
}

inline VkResult getSwapchainImagesKHR(std::vector<VkImage> &vec, VkDevice device,
                                      VkSwapchainKHR swapchain)
{
  VKH_ENUMERATE(vkGetSwapchainImagesKHR, device, swapchain);
}

#undef VKH_ENUMERATE

inline VkPhysicalDeviceProperties getPhysicalDeviceProperties(VkPhysicalDevice physDev)
{
  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(physDev, &props);
  return props;
}

void updateDescriptorSets(VkDevice device, const std::vector<VkWriteDescriptorSet> &writes,
                          const std::vector<VkCopyDescriptorSet> &copies = {});

void cmdPipelineBarrier(VkCommandBuffer cmd, std::initializer_list<VkImageMemoryBarrier> img,
                        std::initializer_list<VkBufferMemoryBarrier> buf = {},
                        std::initializer_list<VkMemoryBarrier> mem = {},
                        VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VkDependencyFlags dependencyFlags = 0);

void cmdPipelineBarrier(VkCommandBuffer cmd, const std::vector<VkImageMemoryBarrier> &img,
                        const std::vector<VkBufferMemoryBarrier> &buf = {},
                        const std::vector<VkMemoryBarrier> &mem = {},
                        VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                        VkDependencyFlags dependencyFlags = 0);

struct ClearColorValue;
struct ClearDepthStencilValue;
void cmdClearImage(VkCommandBuffer cmd, VkImage img, const ClearColorValue &col,
                   VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);
void cmdClearImage(VkCommandBuffer cmd, VkImage img, const ClearDepthStencilValue &ds,
                   VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL);

void cmdBindVertexBuffers(VkCommandBuffer cmd, uint32_t firstBinding,
                          std::initializer_list<VkBuffer> bufs,
                          std::initializer_list<VkDeviceSize> offsets = {});

void cmdBindVertexBuffers(VkCommandBuffer cmd, std::initializer_list<VkBuffer> bufs);

void cmdBindDescriptorSets(VkCommandBuffer cmd, VkPipelineBindPoint pipelineBindPoint,
                           VkPipelineLayout layout, uint32_t firstSet,
                           std::vector<VkDescriptorSet> sets, std::vector<uint32_t> dynamicOffsets);

void cmdPushDescriptorSets(VkCommandBuffer cmd, VkPipelineBindPoint pipelineBindPoint,
                           VkPipelineLayout layout, uint32_t set,
                           std::vector<VkWriteDescriptorSet> writes);

struct ApplicationInfo : public VkApplicationInfo
{
  ApplicationInfo() : VkApplicationInfo() { sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; }
  ApplicationInfo(const char *pApplicationName, uint32_t applicationVersion,
                  const char *pEngineName, uint32_t engineVersion, uint32_t apiVersion)
      : VkApplicationInfo()
  {
    sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    pNext = NULL;
    this->pApplicationName = pApplicationName;
    this->applicationVersion = applicationVersion;
    this->pEngineName = pEngineName;
    this->engineVersion = engineVersion;
    this->apiVersion = apiVersion;
  }
};

struct InstanceCreateInfo : public VkInstanceCreateInfo
{
  InstanceCreateInfo(const VkApplicationInfo &appInfo, const std::vector<const char *> &layers,
                     const std::vector<const char *> &exts)
  {
    sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    pNext = NULL;
    flags = 0;
    pApplicationInfo = &appInfo;
    enabledLayerCount = uint32_t(layers.size());
    ppEnabledLayerNames = layers.data();
    enabledExtensionCount = uint32_t(exts.size());
    ppEnabledExtensionNames = exts.data();
  }

  InstanceCreateInfo &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }

  operator const VkInstanceCreateInfo *() const { return this; }
};

struct DebugReportCallbackCreateInfoEXT : public VkDebugReportCallbackCreateInfoEXT
{
  DebugReportCallbackCreateInfoEXT() : VkDebugReportCallbackCreateInfoEXT()
  {
    sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
  }
  DebugReportCallbackCreateInfoEXT(VkDebugReportFlagsEXT flags,
                                   PFN_vkDebugReportCallbackEXT callback, void *userData = NULL)
  {
    sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
    pNext = NULL;
    this->flags = flags;
    this->pfnCallback = callback;
    this->pUserData = userData;
  }

  operator const VkDebugReportCallbackCreateInfoEXT *() const { return this; }
};

struct DebugUtilsMessengerCreateInfoEXT : public VkDebugUtilsMessengerCreateInfoEXT
{
  DebugUtilsMessengerCreateInfoEXT(
      PFN_vkDebugUtilsMessengerCallbackEXT callback, void *userData = NULL,
      VkDebugUtilsMessageSeverityFlagsEXT severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT,
      VkDebugUtilsMessageTypeFlagsEXT type = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
      VkDebugUtilsMessengerCreateFlagsEXT flags = 0)
  {
    sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    pNext = NULL;
    this->flags = flags;
    this->messageSeverity = severity;
    this->messageType = type;
    this->pfnUserCallback = callback;
    this->pUserData = userData;
  }

  operator const VkDebugUtilsMessengerCreateInfoEXT *() const { return this; }
};

struct DeviceQueueCreateInfo : public VkDeviceQueueCreateInfo
{
  DeviceQueueCreateInfo(uint32_t queueFamilyIndex, uint32_t queueCount,
                        const std::vector<float> &priorities =
                            {
                                1.0f,
                                1.0f,
                                1.0f,
                                1.0f,
                                1.0f,
                                1.0f,
                                1.0f,
                                1.0f,
                                1.0f,
                                1.0f,
                                1.0f,
                                1.0f,
                                1.0f,
                                1.0f,
                                1.0f,
                                1.0f,
                            })
      : VkDeviceQueueCreateInfo()
  {
    sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    pNext = NULL;
    flags = 0;
    this->queueFamilyIndex = queueFamilyIndex;
    this->queueCount = queueCount;
    this->pQueuePriorities = priorities.data();
  }
};

struct DeviceCreateInfo : public VkDeviceCreateInfo
{
  DeviceCreateInfo(const std::vector<VkDeviceQueueCreateInfo> &queues,
                   const std::vector<const char *> &layers, const std::vector<const char *> &exts,
                   const VkPhysicalDeviceFeatures *features = NULL)
  {
    sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    pNext = NULL;
    flags = 0;
    queueCreateInfoCount = uint32_t(queues.size());
    pQueueCreateInfos = queues.data();
    enabledLayerCount = uint32_t(layers.size());
    ppEnabledLayerNames = layers.data();
    enabledExtensionCount = uint32_t(exts.size());
    ppEnabledExtensionNames = exts.data();
    pEnabledFeatures = features;
  }

  DeviceCreateInfo(const std::vector<VkDeviceQueueCreateInfo> &queues,
                   const std::vector<const char *> &layers, const std::vector<const char *> &exts,
                   const VkPhysicalDeviceFeatures &features)
      : DeviceCreateInfo(queues, layers, exts, &features)
  {
  }

  DeviceCreateInfo &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }

  operator const VkDeviceCreateInfo *() const { return this; }
};

struct PhysicalDeviceProperties2KHR : public VkPhysicalDeviceProperties2KHR
{
  PhysicalDeviceProperties2KHR()
  {
    sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
    pNext = NULL;
  }

  PhysicalDeviceProperties2KHR &next(void *next)
  {
    this->pNext = next;
    return *this;
  }

  operator VkPhysicalDeviceProperties2KHR *() { return this; }
};

struct PhysicalDeviceFeatures2KHR : public VkPhysicalDeviceFeatures2KHR
{
  PhysicalDeviceFeatures2KHR()
  {
    sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
    pNext = NULL;
  }

  PhysicalDeviceFeatures2KHR &next(void *next)
  {
    this->pNext = next;
    return *this;
  }

  operator VkPhysicalDeviceFeatures2KHR *() { return this; }
};

struct SemaphoreCreateInfo : public VkSemaphoreCreateInfo
{
  SemaphoreCreateInfo() : VkSemaphoreCreateInfo()
  {
    sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    pNext = NULL;
    flags = 0;
  }

  SemaphoreCreateInfo &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }

  operator const VkSemaphoreCreateInfo *() const { return this; }
};

struct FenceCreateInfo : public VkFenceCreateInfo
{
  FenceCreateInfo(VkFenceCreateFlags flags = 0) : VkFenceCreateInfo()
  {
    sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    pNext = NULL;
    this->flags = flags;
  }

  operator const VkFenceCreateInfo *() const { return this; }
};

struct EventCreateInfo : public VkEventCreateInfo
{
  EventCreateInfo(VkEventCreateFlags flags = 0) : VkEventCreateInfo()
  {
    sType = VK_STRUCTURE_TYPE_EVENT_CREATE_INFO;
    pNext = NULL;
    this->flags = flags;
  }

  operator const VkEventCreateInfo *() const { return this; }
};

struct CommandPoolCreateInfo : public VkCommandPoolCreateInfo
{
  CommandPoolCreateInfo(VkCommandPoolCreateFlags flags = 0, uint32_t queueFamilyIndex = 0)
  {
    sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pNext = NULL;
    this->flags = flags;
    this->queueFamilyIndex = queueFamilyIndex;
  }

  operator const VkCommandPoolCreateInfo *() const { return this; }
};

struct ImageSubresourceRange : public VkImageSubresourceRange
{
  ImageSubresourceRange(VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        uint32_t baseMipLevel = 0, uint32_t levelCount = VK_REMAINING_MIP_LEVELS,
                        uint32_t baseArrayLayer = 0, uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS)
  {
    this->aspectMask = aspectMask;
    this->baseMipLevel = baseMipLevel;
    this->levelCount = levelCount;
    this->baseArrayLayer = baseArrayLayer;
    this->layerCount = layerCount;
  }

  operator const VkImageSubresourceRange *() const { return this; }
};

struct ImageMemoryBarrier : public VkImageMemoryBarrier
{
  ImageMemoryBarrier(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                     VkImageLayout oldLayout, VkImageLayout newLayout, VkImage image,
                     VkImageSubresourceRange subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0,
                                                                 VK_REMAINING_MIP_LEVELS, 0,
                                                                 VK_REMAINING_ARRAY_LAYERS},
                     uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                     uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
  {
    sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    pNext = NULL;
    this->srcAccessMask = srcAccessMask;
    this->dstAccessMask = dstAccessMask;
    this->oldLayout = oldLayout;
    this->newLayout = newLayout;
    this->srcQueueFamilyIndex = srcQueueFamilyIndex;
    this->dstQueueFamilyIndex = dstQueueFamilyIndex;
    this->image = image;
    this->subresourceRange = subresourceRange;
  }
};

struct BufferMemoryBarrier : public VkBufferMemoryBarrier
{
  BufferMemoryBarrier(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, VkBuffer buffer,
                      VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE,
                      uint32_t srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                      uint32_t dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED)
  {
    sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    pNext = NULL;
    this->srcAccessMask = srcAccessMask;
    this->dstAccessMask = dstAccessMask;
    this->srcQueueFamilyIndex = srcQueueFamilyIndex;
    this->dstQueueFamilyIndex = dstQueueFamilyIndex;
    this->buffer = buffer;
    this->offset = offset;
    this->size = size;
  }
};

struct CommandBufferAllocateInfo : public VkCommandBufferAllocateInfo
{
  CommandBufferAllocateInfo(VkCommandPool commandPool, uint32_t commandBufferCount,
                            VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY)
  {
    sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    pNext = NULL;
    this->commandPool = commandPool;
    this->commandBufferCount = commandBufferCount;
    this->level = level;
  }

  operator const VkCommandBufferAllocateInfo *() const { return this; }
};

struct ShaderModuleCreateInfo : public VkShaderModuleCreateInfo
{
  ShaderModuleCreateInfo(const std::vector<uint32_t> &spirv)
  {
    sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    pNext = NULL;
    this->flags = 0;
    this->codeSize = spirv.size() * sizeof(uint32_t);
    this->pCode = spirv.data();
  }

  operator const VkShaderModuleCreateInfo *() const { return this; }
};

struct ImageCreateInfo : public VkImageCreateInfo
{
  ImageCreateInfo(uint32_t width, uint32_t height, uint32_t depth, VkFormat format,
                  VkImageUsageFlags usage, uint32_t mipLevels = 1, uint32_t arrayLayers = 1,
                  VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
                  VkImageCreateFlags flags = 0, VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                  uint32_t queueFamilyIndexCount = 0, const uint32_t *pQueueFamilyIndices = NULL,
                  VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
                  VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED)
  {
    sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    pNext = NULL;
    this->flags = flags;
    this->format = format;
    this->extent.width = std::max(1U, width);
    this->extent.height = std::max(1U, height);
    this->extent.depth = std::max(1U, depth);
    this->mipLevels = mipLevels;
    this->arrayLayers = arrayLayers;
    this->samples = samples;
    this->tiling = tiling;
    this->usage = usage;
    this->sharingMode = sharingMode;
    this->queueFamilyIndexCount = queueFamilyIndexCount;
    this->pQueueFamilyIndices = pQueueFamilyIndices;
    this->initialLayout = initialLayout;

    // derive imageType from dimensions
    if(depth > 0)
      imageType = VK_IMAGE_TYPE_3D;
    else if(height > 0)
      imageType = VK_IMAGE_TYPE_2D;
    else
      imageType = VK_IMAGE_TYPE_1D;
  }

  VkImageCreateInfo &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }

  operator const VkImageCreateInfo *() const { return this; }
};

struct SamplerCreateInfo : public VkSamplerCreateInfo
{
  // simplified constructor, filter and address mode identical in all directions
  SamplerCreateInfo(VkFilter filter,
                    VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    float maxAnisotropy = 0.0f,
                    VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                    float mipLodBias = 0.0f, float minLod = 0.0f, float maxLod = 0.0f,
                    VkCompareOp compareOp = VK_COMPARE_OP_NEVER,
                    VkBool32 unnormalizedCoordinates = VK_FALSE, VkSamplerCreateFlags flags = 0)
      : SamplerCreateInfo(filter, filter,
                          filter == VK_FILTER_NEAREST ? VK_SAMPLER_MIPMAP_MODE_NEAREST
                                                      : VK_SAMPLER_MIPMAP_MODE_LINEAR,
                          addressMode, addressMode, addressMode, maxAnisotropy, borderColor,
                          mipLodBias, minLod, maxLod, compareOp, unnormalizedCoordinates, flags)
  {
  }

  SamplerCreateInfo(VkFilter minFilter, VkFilter magFilter,
                    VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                    VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                    float maxAnisotropy = 0.0f,
                    VkBorderColor borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
                    float mipLodBias = 0.0f, float minLod = 0.0f, float maxLod = 0.0f,
                    VkCompareOp compareOp = VK_COMPARE_OP_NEVER,
                    VkBool32 unnormalizedCoordinates = VK_FALSE, VkSamplerCreateFlags flags = 0)
  {
    sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    pNext = NULL;

    this->flags = flags;
    this->magFilter = magFilter;
    this->minFilter = minFilter;
    this->mipmapMode = mipmapMode;
    this->addressModeU = addressModeU;
    this->addressModeV = addressModeV;
    this->addressModeW = addressModeW;

    this->mipLodBias = mipLodBias;

    this->anisotropyEnable = (maxAnisotropy > 1.0f);
    this->maxAnisotropy = maxAnisotropy;

    this->compareEnable = compareOp != VK_COMPARE_OP_NEVER;
    this->compareOp = compareOp;
    this->minLod = minLod;
    this->maxLod = maxLod;
    this->borderColor = borderColor;
    this->unnormalizedCoordinates = unnormalizedCoordinates;
  }

  SamplerCreateInfo &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }

  operator const VkSamplerCreateInfo *() const { return this; }
};

struct ImageViewCreateInfo : public VkImageViewCreateInfo
{
  ImageViewCreateInfo(
      VkImage image, VkImageViewType viewType, VkFormat format,
      VkComponentMapping components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                       VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
      VkImageSubresourceRange subresourceRange = {
          VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS})
  {
    sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    pNext = NULL;
    this->flags = 0;
    this->image = image;
    this->viewType = viewType;
    this->format = format;
    this->components = components;
    this->subresourceRange = subresourceRange;
  }

  operator const VkImageViewCreateInfo *() const { return this; }
};

struct BufferViewCreateInfo : public VkBufferViewCreateInfo
{
  BufferViewCreateInfo(VkBuffer buffer, VkFormat format, VkDeviceSize offset = 0,
                       VkDeviceSize range = VK_WHOLE_SIZE)
  {
    sType = VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO;
    pNext = NULL;
    this->flags = 0;
    this->buffer = buffer;
    this->format = format;
    this->offset = offset;
    this->range = range;
  }

  operator const VkBufferViewCreateInfo *() const { return this; }
};

struct BufferCreateInfo : public VkBufferCreateInfo
{
  BufferCreateInfo(VkDeviceSize size, VkBufferUsageFlags usage, VkBufferCreateFlags flags = 0,
                   VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                   uint32_t queueFamilyIndexCount = 0, const uint32_t *pQueueFamilyIndices = NULL)
  {
    sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    pNext = NULL;
    this->size = size;
    this->usage = usage;
    this->flags = flags;
    this->sharingMode = sharingMode;
    this->queueFamilyIndexCount = queueFamilyIndexCount;
    this->pQueueFamilyIndices = pQueueFamilyIndices;
  }

  operator const VkBufferCreateInfo *() const { return this; }
};

struct DescriptorSetLayoutCreateInfo : public VkDescriptorSetLayoutCreateInfo
{
  DescriptorSetLayoutCreateInfo(const std::vector<VkDescriptorSetLayoutBinding> &bindings,
                                VkDescriptorSetLayoutCreateFlags flags = 0)
  {
    sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    pNext = NULL;
    this->flags = flags;
    this->bindingCount = (uint32_t)bindings.size();
    this->pBindings = bindings.data();
  }

  DescriptorSetLayoutCreateInfo &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }

  operator const VkDescriptorSetLayoutCreateInfo *() const { return this; }
};

struct DescriptorPoolCreateInfo : public VkDescriptorPoolCreateInfo
{
  DescriptorPoolCreateInfo(uint32_t maxSets, const std::vector<VkDescriptorPoolSize> &poolSizes,
                           VkDescriptorPoolCreateFlags flags = 0)
  {
    sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pNext = NULL;
    this->flags = flags;
    this->maxSets = maxSets;
    this->poolSizeCount = (uint32_t)poolSizes.size();
    this->pPoolSizes = poolSizes.data();
  }

  DescriptorPoolCreateInfo &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }

  operator const VkDescriptorPoolCreateInfo *() const { return this; }
};

struct DescriptorSetAllocateInfo : public VkDescriptorSetAllocateInfo
{
  DescriptorSetAllocateInfo(VkDescriptorPool descriptorPool,
                            const std::vector<VkDescriptorSetLayout> &setLayouts)
  {
    sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    pNext = NULL;
    this->descriptorPool = descriptorPool;
    this->descriptorSetCount = (uint32_t)setLayouts.size();
    this->pSetLayouts = setLayouts.data();
  }

  operator const VkDescriptorSetAllocateInfo *() const { return this; }
};

struct PushConstantRange : public VkPushConstantRange
{
  PushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size)
  {
    this->stageFlags = stageFlags;
    this->offset = offset;
    this->size = size;
  }

  operator const VkPushConstantRange *() const { return this; }
};

struct PipelineLayoutCreateInfo : public VkPipelineLayoutCreateInfo
{
  PipelineLayoutCreateInfo(const std::vector<VkDescriptorSetLayout> &setLayouts = {},
                           const std::vector<VkPushConstantRange> &pushConstantRanges = {},
                           VkPipelineLayoutCreateFlags flags = 0)
  {
    sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pNext = NULL;
    this->flags = flags;
    this->setLayoutCount = (uint32_t)setLayouts.size();
    this->pSetLayouts = setLayouts.data();
    this->pushConstantRangeCount = (uint32_t)pushConstantRanges.size();
    this->pPushConstantRanges = pushConstantRanges.data();
  }

  operator const VkPipelineLayoutCreateInfo *() const { return this; }
};

struct AttachmentDescription : public VkAttachmentDescription
{
  AttachmentDescription(VkFormat format, VkImageLayout initialLayout, VkImageLayout finalLayout,
                        VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_MAX_ENUM,
                        VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_MAX_ENUM,
                        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
                        VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_MAX_ENUM,
                        VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_MAX_ENUM,
                        VkAttachmentDescriptionFlags flags = 0)
  {
    this->format = format;
    this->initialLayout = initialLayout;
    this->finalLayout = finalLayout;
    this->samples = samples;
    this->flags = flags;

    // if un-set, default loadOp/storeOp to load/store
    this->loadOp = loadOp != VK_ATTACHMENT_LOAD_OP_MAX_ENUM ? loadOp : VK_ATTACHMENT_LOAD_OP_LOAD;
    this->storeOp =
        storeOp != VK_ATTACHMENT_STORE_OP_MAX_ENUM ? storeOp : VK_ATTACHMENT_STORE_OP_STORE;

    // if un-set, default stencilLoadOp/StoreOp to the same as loadOp/storeOp
    this->stencilLoadOp =
        stencilLoadOp != VK_ATTACHMENT_LOAD_OP_MAX_ENUM ? stencilLoadOp : this->loadOp;
    this->stencilStoreOp =
        stencilStoreOp != VK_ATTACHMENT_STORE_OP_MAX_ENUM ? stencilStoreOp : this->storeOp;
  }
};

struct AttachmentDescription2KHR : public VkAttachmentDescription2KHR
{
  AttachmentDescription2KHR(VkFormat format, VkImageLayout initialLayout, VkImageLayout finalLayout,
                            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_MAX_ENUM,
                            VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_MAX_ENUM,
                            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
                            VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_MAX_ENUM,
                            VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_MAX_ENUM,
                            VkAttachmentDescriptionFlags flags = 0)
  {
    this->sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2_KHR;
    this->pNext = NULL;
    this->format = format;
    this->initialLayout = initialLayout;
    this->finalLayout = finalLayout;
    this->samples = samples;
    this->flags = flags;

    // if un-set, default loadOp/storeOp to load/store
    this->loadOp = loadOp != VK_ATTACHMENT_LOAD_OP_MAX_ENUM ? loadOp : VK_ATTACHMENT_LOAD_OP_LOAD;
    this->storeOp =
        storeOp != VK_ATTACHMENT_STORE_OP_MAX_ENUM ? storeOp : VK_ATTACHMENT_STORE_OP_STORE;

    // if un-set, default stencilLoadOp/StoreOp to the same as loadOp/storeOp
    this->stencilLoadOp =
        stencilLoadOp != VK_ATTACHMENT_LOAD_OP_MAX_ENUM ? stencilLoadOp : this->loadOp;
    this->stencilStoreOp =
        stencilStoreOp != VK_ATTACHMENT_STORE_OP_MAX_ENUM ? stencilStoreOp : this->storeOp;
  }

  AttachmentDescription2KHR &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }
};

struct AttachmentDescriptionStencilLayoutKHR : public VkAttachmentDescriptionStencilLayoutKHR
{
  AttachmentDescriptionStencilLayoutKHR(VkImageLayout stencilInitialLayout,
                                        VkImageLayout stencilFinalLayout)
  {
    this->sType = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_STENCIL_LAYOUT_KHR;
    this->pNext = NULL;
    this->stencilInitialLayout = stencilInitialLayout;
    this->stencilFinalLayout = stencilFinalLayout;
  }
  AttachmentDescriptionStencilLayoutKHR &next(void *next)
  {
    this->pNext = next;
    return *this;
  }
};

struct FramebufferCreateInfo : public VkFramebufferCreateInfo
{
  FramebufferCreateInfo(VkRenderPass renderPass, const std::vector<VkImageView> &attachments,
                        VkExtent2D extent, uint32_t layers = 1, VkFramebufferCreateFlags flags = 0)
  {
    sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    pNext = NULL;
    this->flags = flags;
    this->renderPass = renderPass;
    this->attachmentCount = (uint32_t)attachments.size();
    this->pAttachments = attachments.data();
    this->width = extent.width;
    this->height = extent.height;
    this->layers = layers;
  }

  FramebufferCreateInfo &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }

  operator const VkFramebufferCreateInfo *() const { return this; }
};

struct SwapchainCreateInfoKHR : public VkSwapchainCreateInfoKHR
{
  SwapchainCreateInfoKHR(
      VkSurfaceKHR surface, VkPresentModeKHR presentMode, VkSurfaceFormatKHR format,
      VkExtent2D imageExtent, VkImageUsageFlags imageUsage,
      VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE, uint32_t minImageCount = 2,
      uint32_t imageArrayLayers = 1,
      VkSurfaceTransformFlagBitsKHR preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      VkBool32 clipped = VK_FALSE, VkSharingMode imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      uint32_t queueFamilyIndexCount = 0, const uint32_t *pQueueFamilyIndices = NULL,
      VkSwapchainCreateFlagsKHR flags = 0)
  {
    sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    pNext = NULL;
    this->flags = flags;
    this->surface = surface;
    this->minImageCount = minImageCount;
    this->imageExtent = imageExtent;
    this->imageUsage = imageUsage;
    this->imageFormat = format.format;
    this->imageColorSpace = format.colorSpace;
    this->imageArrayLayers = imageArrayLayers;
    this->imageSharingMode = imageSharingMode;
    this->queueFamilyIndexCount = queueFamilyIndexCount;
    this->pQueueFamilyIndices = pQueueFamilyIndices;
    this->preTransform = preTransform;
    this->compositeAlpha = compositeAlpha;
    this->presentMode = presentMode;
    this->clipped = clipped;
    this->oldSwapchain = oldSwapchain;
  }

  operator const VkSwapchainCreateInfoKHR *() const { return this; }
};

struct DescriptorBufferInfo : public VkDescriptorBufferInfo
{
  DescriptorBufferInfo(VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE)
  {
    this->buffer = buffer;
    this->offset = offset;
    this->range = range;
  }
};

struct DescriptorImageInfo : public VkDescriptorImageInfo
{
  DescriptorImageInfo(VkImageView imageView,
                      VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VkSampler sampler = VK_NULL_HANDLE)
  {
    this->sampler = sampler;
    this->imageView = imageView;
    this->imageLayout = imageLayout;
  }
};

struct WriteDescriptorSet : public VkWriteDescriptorSet
{
  WriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t dstArrayElement,
                     VkDescriptorType descriptorType,
                     const std::vector<VkDescriptorBufferInfo> &bufferInfo)
  {
    sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pNext = NULL;
    this->dstSet = dstSet;
    this->dstBinding = dstBinding;
    this->dstArrayElement = dstArrayElement;
    this->descriptorCount = (uint32_t)bufferInfo.size();
    this->descriptorType = descriptorType;
    this->pImageInfo = NULL;
    this->pBufferInfo = bufferInfo.data();
    this->pTexelBufferView = NULL;
  }

  WriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding,
                     const VkWriteDescriptorSetInlineUniformBlockEXT &inlineWrite,
                     const VkDescriptorBufferInfo &bufferInfo)
  {
    sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pNext = &inlineWrite;
    this->dstSet = dstSet;
    this->dstBinding = dstBinding;
    this->dstArrayElement = 0;
    this->descriptorCount = inlineWrite.dataSize;
    this->descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK_EXT;
    this->pImageInfo = NULL;
    this->pBufferInfo = &bufferInfo;
    this->pTexelBufferView = NULL;
  }

  WriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding, VkDescriptorType descriptorType,
                     const std::vector<VkDescriptorBufferInfo> &bufferInfo)
      : WriteDescriptorSet(dstSet, dstBinding, 0, descriptorType, bufferInfo)
  {
  }

  WriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t dstArrayElement,
                     VkDescriptorType descriptorType,
                     const std::vector<VkDescriptorImageInfo> &imageInfo)
  {
    sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pNext = NULL;
    this->dstSet = dstSet;
    this->dstBinding = dstBinding;
    this->dstArrayElement = dstArrayElement;
    this->descriptorCount = (uint32_t)imageInfo.size();
    this->descriptorType = descriptorType;
    this->pImageInfo = imageInfo.data();
    this->pBufferInfo = NULL;
    this->pTexelBufferView = NULL;
  }

  WriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding, VkDescriptorType descriptorType,
                     const std::vector<VkDescriptorImageInfo> &imageInfo)
      : WriteDescriptorSet(dstSet, dstBinding, 0, descriptorType, imageInfo)
  {
  }

  WriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t dstArrayElement,
                     VkDescriptorType descriptorType,
                     const std::vector<VkBufferView> &texelBufferView)
  {
    sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    pNext = NULL;
    this->dstSet = dstSet;
    this->dstBinding = dstBinding;
    this->dstArrayElement = dstArrayElement;
    this->descriptorCount = (uint32_t)texelBufferView.size();
    this->descriptorType = descriptorType;
    this->pImageInfo = NULL;
    this->pBufferInfo = NULL;
    this->pTexelBufferView = texelBufferView.data();
  }

  WriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding, VkDescriptorType descriptorType,
                     const std::vector<VkBufferView> &texelBufferView)
      : WriteDescriptorSet(dstSet, dstBinding, 0, descriptorType, texelBufferView)
  {
  }

  WriteDescriptorSet &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }

  operator const VkWriteDescriptorSet *() const { return this; }
};

struct PresentInfoKHR : public VkPresentInfoKHR
{
  PresentInfoKHR(const VkSwapchainKHR &swap, const uint32_t &imageIndex, VkSemaphore *semaphore)
  {
    sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pNext = NULL;
    waitSemaphoreCount = semaphore ? 1 : 0;
    pWaitSemaphores = semaphore;
    swapchainCount = 1;
    pSwapchains = &swap;
    pImageIndices = &imageIndex;
    this->pResults = NULL;
  }

  operator const VkPresentInfoKHR *() const { return this; }
};

struct SubmitInfo : public VkSubmitInfo
{
  SubmitInfo(const std::vector<VkCommandBuffer> &cmds)
  {
    sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    pNext = NULL;
    this->commandBufferCount = (uint32_t)cmds.size();
    this->pCommandBuffers = cmds.data();

    this->waitSemaphoreCount = 0;
    this->pWaitSemaphores = NULL;
    this->pWaitDstStageMask = NULL;

    this->signalSemaphoreCount = 0;
    this->pSignalSemaphores = NULL;
  }

  operator const VkSubmitInfo *() const { return this; }
};

struct CommandBufferBeginInfo : public VkCommandBufferBeginInfo
{
  CommandBufferBeginInfo(VkCommandBufferUsageFlags flags = 0,
                         const VkCommandBufferInheritanceInfo *pInheritanceInfo = NULL)
  {
    sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    pNext = NULL;
    this->flags = flags;
    this->pInheritanceInfo = pInheritanceInfo;
  }

  operator const VkCommandBufferBeginInfo *() const { return this; }
};

struct CommandBufferInheritanceInfo : public VkCommandBufferInheritanceInfo
{
  CommandBufferInheritanceInfo(VkRenderPass renderPass, uint32_t subpass,
                               VkFramebuffer framebuffer = VK_NULL_HANDLE,
                               VkBool32 occlusionQueryEnable = VK_FALSE,
                               VkQueryControlFlags queryFlags = 0,
                               VkQueryPipelineStatisticFlags pipelineStatistics = 0)
  {
    sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    pNext = NULL;
    this->renderPass = renderPass;
    this->subpass = subpass;
    this->framebuffer = framebuffer;
    this->occlusionQueryEnable = occlusionQueryEnable;
    this->queryFlags = queryFlags;
    this->pipelineStatistics = pipelineStatistics;
  }

  operator const VkCommandBufferInheritanceInfo *() const { return this; }
};

struct RenderPassBeginInfo : public VkRenderPassBeginInfo
{
  RenderPassBeginInfo(VkRenderPass renderPass, VkFramebuffer framebuffer, VkRect2D renderArea,
                      const std::vector<VkClearValue> &clearVals = {})
  {
    sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pNext = NULL;
    this->renderPass = renderPass;
    this->framebuffer = framebuffer;
    this->renderArea = renderArea;
    this->clearValueCount = (uint32_t)clearVals.size();
    this->pClearValues = clearVals.data();
  }

  RenderPassBeginInfo &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }

  operator const VkRenderPassBeginInfo *() const { return this; }
};

struct SubpassDependency : public VkSubpassDependency
{
  SubpassDependency(uint32_t srcSubpass, uint32_t dstSubpass, VkPipelineStageFlags srcStageMask,
                    VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask,
                    VkAccessFlags dstAccessMask, VkDependencyFlags dependencyFlags = 0)
  {
    this->srcSubpass = srcSubpass;
    this->dstSubpass = dstSubpass;
    this->srcStageMask = srcStageMask;
    this->dstStageMask = dstStageMask;
    this->srcAccessMask = srcAccessMask;
    this->dstAccessMask = dstAccessMask;
    this->dependencyFlags = dependencyFlags;
  }

  operator const VkSubpassDependency *() const { return this; }
};

struct SubpassDependency2KHR : public VkSubpassDependency2KHR
{
  SubpassDependency2KHR(uint32_t srcSubpass, uint32_t dstSubpass, VkPipelineStageFlags srcStageMask,
                        VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask,
                        VkAccessFlags dstAccessMask, VkDependencyFlags dependencyFlags = 0,
                        uint32_t viewOffset = 0)
  {
    this->sType = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2_KHR;
    this->pNext = NULL;
    this->srcSubpass = srcSubpass;
    this->dstSubpass = dstSubpass;
    this->srcStageMask = srcStageMask;
    this->dstStageMask = dstStageMask;
    this->srcAccessMask = srcAccessMask;
    this->dstAccessMask = dstAccessMask;
    this->dependencyFlags = dependencyFlags;
    this->viewOffset = viewOffset;
  }

  SubpassDependency2KHR &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }

  operator const VkSubpassDependency2KHR *() const { return this; }
};

struct ClearColorValue
{
  ClearColorValue(float r, float g, float b, float a)
  {
    float32[0] = r;
    float32[1] = g;
    float32[2] = b;
    float32[3] = a;
  }

  ClearColorValue(uint32_t r, uint32_t g, uint32_t b, uint32_t a)
  {
    uint32[0] = r;
    uint32[1] = g;
    uint32[2] = b;
    uint32[3] = a;
  }

  union
  {
    float float32[4];
    int32_t int32[4];
    uint32_t uint32[4];
  };

  operator const VkClearColorValue *() const { return (VkClearColorValue *)this; }
};

struct ClearDepthStencilValue
{
  float depth;
  uint32_t stencil;

  operator const VkClearDepthStencilValue *() const { return (VkClearDepthStencilValue *)this; }
};

struct ClearValue
{
  ClearValue() {}
  ClearValue(float r, float g, float b, float a)
  {
    clear.color.float32[0] = r;
    clear.color.float32[1] = g;
    clear.color.float32[2] = b;
    clear.color.float32[3] = a;
  }

  ClearValue(float d, uint32_t s)
  {
    clear.depthStencil.depth = d;
    clear.depthStencil.stencil = s;
  }

  VkClearValue clear;

  operator const VkClearValue *() const { return (VkClearValue *)this; }
  operator const VkClearValue &() const { return (VkClearValue &)*this; }
};

struct Viewport : public VkViewport
{
  Viewport(float x, float y, float width, float height, float minDepth, float maxDepth)
  {
    this->x = x;
    this->y = y;
    this->width = width;
    this->height = height;
    this->minDepth = minDepth;
    this->maxDepth = maxDepth;
  }
};

struct Rect2D : public VkRect2D
{
  Rect2D(VkOffset2D offset, VkExtent2D extent)
  {
    this->offset = offset;
    this->extent = extent;
  }
};

struct PipelineShaderStageCreateInfo : public VkPipelineShaderStageCreateInfo
{
  PipelineShaderStageCreateInfo(VkShaderModule module, VkShaderStageFlagBits stage,
                                const char *entry = "main")
  {
    sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pNext = NULL;
    this->flags = 0;
    this->stage = stage;
    this->module = module;
    this->pName = entry;
    this->pSpecializationInfo = NULL;
  }

  operator const VkPipelineShaderStageCreateInfo *() const { return this; }
};

struct ComputePipelineCreateInfo : public VkComputePipelineCreateInfo
{
  ComputePipelineCreateInfo(VkPipelineLayout layout, VkPipelineShaderStageCreateInfo stage,
                            VkPipelineCreateFlags flags = 0,
                            VkPipeline basePipelineHandle = VK_NULL_HANDLE,
                            int32_t basePipelineIndex = -1)
  {
    sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pNext = NULL;
    this->flags = 0;
    this->stage = stage;
    this->layout = layout;
    this->basePipelineHandle = basePipelineHandle;
    this->basePipelineIndex = basePipelineIndex;
  }

  operator const VkComputePipelineCreateInfo *() const { return this; }
};

// we inherit privately and selectively make public the things we want to give direct access to
struct GraphicsPipelineCreateInfo : private VkGraphicsPipelineCreateInfo
{
  GraphicsPipelineCreateInfo();
  GraphicsPipelineCreateInfo(const GraphicsPipelineCreateInfo &other) { *this = other; }
  const GraphicsPipelineCreateInfo &operator=(const GraphicsPipelineCreateInfo &other);

  using VkGraphicsPipelineCreateInfo::pNext;
  using VkGraphicsPipelineCreateInfo::flags;
  using VkGraphicsPipelineCreateInfo::layout;
  using VkGraphicsPipelineCreateInfo::renderPass;
  using VkGraphicsPipelineCreateInfo::subpass;
  using VkGraphicsPipelineCreateInfo::basePipelineHandle;
  using VkGraphicsPipelineCreateInfo::basePipelineIndex;

  // now expose the nicer access that we store directly & connect up
  std::vector<VkPipelineShaderStageCreateInfo> stages;

  VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
  VkPipelineTessellationStateCreateInfo tessellationState = {};
  VkPipelineRasterizationStateCreateInfo rasterizationState = {};
  VkPipelineMultisampleStateCreateInfo multisampleState = {};
  VkPipelineDepthStencilStateCreateInfo depthStencilState = {};

  // these structs use the same hack as the parent struct
  struct VertexInputState : private VkPipelineVertexInputStateCreateInfo
  {
    // needs to be a friend so that bake() can poke our internals to point to the vectors
    friend struct GraphicsPipelineCreateInfo;

    VertexInputState()
    {
      sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
      pNext = NULL;
      flags = 0;
    }

    using VkPipelineVertexInputStateCreateInfo::pNext;
    using VkPipelineVertexInputStateCreateInfo::flags;

    std::vector<VkVertexInputBindingDescription> vertexBindingDescriptions;
    std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
  } vertexInputState;

  struct ViewportState : private VkPipelineViewportStateCreateInfo
  {
    // needs to be a friend so that bake() can poke our internals to point to the vectors
    friend struct GraphicsPipelineCreateInfo;

    ViewportState()
    {
      sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
      pNext = NULL;
      flags = 0;
    }

    using VkPipelineViewportStateCreateInfo::pNext;
    using VkPipelineViewportStateCreateInfo::flags;

    // we allow the counts to be public so they can be set to higher values when the viewport is
    // dynamic. In bake() we take max of the count and the size of the array (so setting an array
    // still works without needing to set size separately).
    using VkPipelineViewportStateCreateInfo::viewportCount;
    using VkPipelineViewportStateCreateInfo::scissorCount;

    std::vector<VkViewport> viewports;
    std::vector<VkRect2D> scissors;
  } viewportState;

  struct ColorBlendState : private VkPipelineColorBlendStateCreateInfo
  {
    // needs to be a friend so that bake() can poke our internals to point to the vector
    friend struct GraphicsPipelineCreateInfo;

    ColorBlendState()
    {
      sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
      pNext = NULL;
      flags = 0;
      logicOpEnable = VK_FALSE;
      logicOp = VK_LOGIC_OP_NO_OP;
      blendConstants[0] = blendConstants[1] = blendConstants[2] = blendConstants[3] = 1.0f;
    }

    using VkPipelineColorBlendStateCreateInfo::pNext;
    using VkPipelineColorBlendStateCreateInfo::flags;
    using VkPipelineColorBlendStateCreateInfo::logicOpEnable;
    using VkPipelineColorBlendStateCreateInfo::logicOp;
    using VkPipelineColorBlendStateCreateInfo::blendConstants;

    std::vector<VkPipelineColorBlendAttachmentState> attachments;
  } colorBlendState;

  struct DynamicState : private VkPipelineDynamicStateCreateInfo
  {
    // needs to be a friend so that bake() can poke our internals to point to the vector
    friend struct GraphicsPipelineCreateInfo;

    DynamicState()
    {
      sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
      pNext = NULL;
      flags = 0;
    }

    using VkPipelineDynamicStateCreateInfo::pNext;
    using VkPipelineDynamicStateCreateInfo::flags;

    std::vector<VkDynamicState> dynamicStates;
  };
  DynamicState dynamicState;

  operator const VkGraphicsPipelineCreateInfo *()
  {
    bake();
    return (const VkGraphicsPipelineCreateInfo *)this;
  }

  operator VkGraphicsPipelineCreateInfo *()
  {
    bake();
    return (VkGraphicsPipelineCreateInfo *)this;
  }

private:
  void bake();
};

struct RenderPassCreator : private VkRenderPassCreateInfo
{
  RenderPassCreator()
  {
    sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    pNext = NULL;
    flags = 0;
  }

  using VkRenderPassCreateInfo::pNext;
  using VkRenderPassCreateInfo::flags;

  std::vector<VkAttachmentDescription> attachments;
  std::vector<VkSubpassDescription> subpasses;
  std::vector<VkSubpassDependency> dependencies;

  void addSubpass(const std::vector<VkAttachmentReference> &colorAttachments,
                  uint32_t depthAttachment = VK_ATTACHMENT_UNUSED,
                  VkImageLayout depthLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                  const std::vector<VkAttachmentReference> &resolveAttachments = {},
                  const std::vector<VkAttachmentReference> &inputAttachments = {})
  {
    const VkAttachmentReference *color = NULL;
    if(!colorAttachments.empty())
    {
      color = MakeTempPtr();
      attrefs.insert(attrefs.end(), colorAttachments.begin(), colorAttachments.end());
    }

    const VkAttachmentReference *depth = NULL;
    if(depthAttachment != VK_ATTACHMENT_UNUSED)
    {
      depth = MakeTempPtr();
      attrefs.push_back(VkAttachmentReference({depthAttachment, depthLayout}));
    }

    const VkAttachmentReference *resolve = NULL;
    if(!resolveAttachments.empty())
    {
      resolve = MakeTempPtr();
      attrefs.insert(attrefs.end(), resolveAttachments.begin(), resolveAttachments.end());
    }

    const VkAttachmentReference *input = NULL;
    if(!inputAttachments.empty())
    {
      input = MakeTempPtr();
      attrefs.insert(attrefs.end(), inputAttachments.begin(), inputAttachments.end());
    }

    subpasses.push_back({
        // flags
        0,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        // input attachments
        (uint32_t)inputAttachments.size(),
        input,
        // color attachments
        (uint32_t)colorAttachments.size(),
        color,
        // resolve attachments
        resolve,
        // depth stencil attachment
        depth,
        // preserve attachments
        0,
        NULL,
    });
  }

  operator const VkRenderPassCreateInfo *()
  {
    bake();
    return (const VkRenderPassCreateInfo *)this;
  }

  RenderPassCreator &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }

private:
  void bake()
  {
    const VkAttachmentReference *invalid = MakeTempPtr();
    for(VkSubpassDescription &sub : subpasses)
    {
      if(sub.pInputAttachments && sub.pInputAttachments <= invalid)
        sub.pInputAttachments = MakeRealPtr(sub.pInputAttachments);
      if(sub.pColorAttachments && sub.pColorAttachments <= invalid)
        sub.pColorAttachments = MakeRealPtr(sub.pColorAttachments);
      if(sub.pResolveAttachments && sub.pResolveAttachments <= invalid)
        sub.pResolveAttachments = MakeRealPtr(sub.pResolveAttachments);
      if(sub.pDepthStencilAttachment && sub.pDepthStencilAttachment <= invalid)
        sub.pDepthStencilAttachment = MakeRealPtr(sub.pDepthStencilAttachment);
    }

    attachmentCount = (uint32_t)attachments.size();
    pAttachments = attachments.data();
    subpassCount = (uint32_t)subpasses.size();
    pSubpasses = subpasses.data();
    dependencyCount = (uint32_t)dependencies.size();
    pDependencies = dependencies.data();
  }

  const VkAttachmentReference *MakeTempPtr()
  {
    return (const VkAttachmentReference *)((attrefs.size() + 1) * sizeof(VkAttachmentReference));
  }
  const VkAttachmentReference *MakeRealPtr(const VkAttachmentReference *ptr)
  {
    return attrefs.data() + ptrdiff_t(ptr) / sizeof(VkAttachmentReference) - 1;
  }

  std::vector<VkAttachmentReference> attrefs;
};

struct AttachmentReference2KHR : public VkAttachmentReference2KHR
{
  AttachmentReference2KHR()
  {
    this->sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
    this->pNext = NULL;
    this->attachment = VK_ATTACHMENT_UNUSED;
    this->layout = VK_IMAGE_LAYOUT_UNDEFINED;
    this->aspectMask = 0;
  }
  AttachmentReference2KHR(uint32_t attachment, VkImageLayout layout, VkImageAspectFlags aspectMask = 0)
  {
    this->sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2_KHR;
    this->pNext = NULL;
    this->attachment = attachment;
    this->layout = layout;
    this->aspectMask = aspectMask;
  }

  AttachmentReference2KHR &next(const void *next)
  {
    this->pNext = next;
    return *this;
  }
};

struct AttachmentReferenceStencilLayoutKHR : public VkAttachmentReferenceStencilLayoutKHR
{
  AttachmentReferenceStencilLayoutKHR(VkImageLayout stencilLayout)
  {
    this->sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_STENCIL_LAYOUT_KHR;
    this->pNext = NULL;
    this->stencilLayout = stencilLayout;
  }

  AttachmentReferenceStencilLayoutKHR &next(void *next)
  {
    this->pNext = next;
    return *this;
  }
};

struct RenderPassCreator2 : private VkRenderPassCreateInfo2KHR
{
  RenderPassCreator2()
  {
    sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2_KHR;
    pNext = NULL;
    flags = 0;
  }

  using VkRenderPassCreateInfo2KHR::flags;
  using VkRenderPassCreateInfo2KHR::pNext;

  std::vector<VkAttachmentDescription2KHR> attachments;
  std::vector<VkSubpassDescription2KHR> subpasses;
  std::vector<VkSubpassDependency2KHR> dependencies;
  std::vector<uint32_t> correlatedViewMasks;

  void addSubpass(const std::vector<VkAttachmentReference2KHR> &colorAttachments,
                  const VkAttachmentReference2KHR &depthStencilAttachment,
                  const std::vector<VkAttachmentReference2KHR> &resolveAttachments = {},
                  const std::vector<VkAttachmentReference2KHR> &inputAttachments = {})
  {
    const VkAttachmentReference2KHR *color = NULL;
    if(!colorAttachments.empty())
    {
      color = MakeTempPtr();
      attrefs.insert(attrefs.end(), colorAttachments.begin(), colorAttachments.end());
    }

    const VkAttachmentReference2KHR *depth = NULL;
    depth = MakeTempPtr();
    attrefs.push_back(depthStencilAttachment);

    const VkAttachmentReference2KHR *resolve = NULL;
    if(!resolveAttachments.empty())
    {
      resolve = MakeTempPtr();
      attrefs.insert(attrefs.end(), resolveAttachments.begin(), resolveAttachments.end());
    }

    const VkAttachmentReference2KHR *input = NULL;
    if(!inputAttachments.empty())
    {
      input = MakeTempPtr();
      attrefs.insert(attrefs.end(), inputAttachments.begin(), inputAttachments.end());
    }

    subpasses.push_back({
        VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2_KHR,
        NULL,
        // flags
        0,
        // pipelineBindPoint
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        // viewMask
        0,
        // input attachments
        (uint32_t)inputAttachments.size(),
        input,
        // color attachments
        (uint32_t)colorAttachments.size(),
        color,
        // resolve attachments
        resolve,
        // depth stencil attachment
        depth,
        // preserve attachments
        0,
        NULL,
    });
  }

  operator const VkRenderPassCreateInfo2KHR *()
  {
    bake();
    return (const VkRenderPassCreateInfo2KHR *)this;
  }

private:
  void bake()
  {
    const VkAttachmentReference2KHR *invalid = MakeTempPtr();
    for(VkSubpassDescription2KHR &sub : subpasses)
    {
      if(sub.pInputAttachments && sub.pInputAttachments <= invalid)
        sub.pInputAttachments = MakeRealPtr(sub.pInputAttachments);
      if(sub.pColorAttachments && sub.pColorAttachments <= invalid)
        sub.pColorAttachments = MakeRealPtr(sub.pColorAttachments);
      if(sub.pResolveAttachments && sub.pResolveAttachments <= invalid)
        sub.pResolveAttachments = MakeRealPtr(sub.pResolveAttachments);
      if(sub.pDepthStencilAttachment && sub.pDepthStencilAttachment <= invalid)
        sub.pDepthStencilAttachment = MakeRealPtr(sub.pDepthStencilAttachment);
    }

    attachmentCount = (uint32_t)attachments.size();
    pAttachments = attachments.data();
    subpassCount = (uint32_t)subpasses.size();
    pSubpasses = subpasses.data();
    dependencyCount = (uint32_t)dependencies.size();
    pDependencies = dependencies.data();
    correlatedViewMaskCount = (uint32_t)correlatedViewMasks.size();
    pCorrelatedViewMasks = correlatedViewMasks.data();
  }

  const VkAttachmentReference2KHR *MakeTempPtr()
  {
    return (const VkAttachmentReference2KHR *)((attrefs.size() + 1) *
                                               sizeof(VkAttachmentReference2KHR));
  }
  const VkAttachmentReference2KHR *MakeRealPtr(const VkAttachmentReference2KHR *ptr)
  {
    return attrefs.data() + ptrdiff_t(ptr) / sizeof(VkAttachmentReference2KHR) - 1;
  }

  std::vector<VkAttachmentReference2KHR> attrefs;
};
};    // namespace vkh
