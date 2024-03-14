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

#include "../test_common.h"

std::string VKFullscreenQuadVertex = R"EOSHADER(

#version 460 core

void main()
{
	vec2 positions[] = {
		vec2(-1.0f,  1.0f),
		vec2( 1.0f,  1.0f),
		vec2(-1.0f, -1.0f),
		vec2( 1.0f, -1.0f),
	};

	gl_Position = vec4(positions[gl_VertexIndex], 0, 1);
}

)EOSHADER";

static std::string common = R"EOSHADER(

#version 460 core

#define v2f v2f_block \
{                     \
	vec4 pos;           \
	vec4 col;           \
	vec4 uv;            \
}

)EOSHADER";

std::string VKDefaultVertex = common + R"EOSHADER(

layout(location = 0) in vec3 Position;
layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 UV;

layout(location = 0) out v2f vertOut;

void main()
{
	vertOut.pos = vec4(Position.xyz*vec3(1,-1,1), 1);
	gl_Position = vertOut.pos;
	vertOut.col = Color;
	vertOut.uv = vec4(UV.xy, 0, 1);
}

)EOSHADER";

std::string VKDefaultPixel = common + R"EOSHADER(

layout(location = 0) in v2f vertIn;

layout(location = 0, index = 0) out vec4 Color;

void main()
{
	Color = vertIn.col;
}

)EOSHADER";

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
#elif defined(ANDROID)
#include "../android/android_window.h"
#elif defined(__linux__)
#include "../linux/linux_window.h"
#elif defined(__APPLE__)
#include "../apple/apple_window.h"
#else
#error UNKNOWN PLATFORM
#endif

static VkBool32 VKAPI_PTR vulkanCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                         VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                                         const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                         void *pUserData)
{
  TEST_WARN("Vulkan message: [%s] %s", pCallbackData->pMessageIdName, pCallbackData->pMessage);

  return false;
}

VulkanGraphicsTest::VulkanGraphicsTest()
{
}

namespace
{
bool volk = false;
bool spv = false;
uint32_t vulkanVersion = 0;
VkInstance inst = VK_NULL_HANDLE;
VkPhysicalDevice selectedPhys = VK_NULL_HANDLE;
std::vector<const char *> enabledInstExts;
std::vector<const char *> enabledLayers;
};

void VulkanGraphicsTest::Prepare(int argc, char **argv)
{
  GraphicsTest::Prepare(argc, argv);

  static bool prepared = false;

  std::vector<VkLayerProperties> availInstLayers;
  std::vector<VkExtensionProperties> availInstExts;

  if(!prepared)
  {
    prepared = true;

    volk = (volkInitialize() == VK_SUCCESS);

    spv = SpvCompilationSupported();

    if(volk && spv)
    {
      enabledInstExts = instExts;
      enabledLayers = instLayers;

      enabledInstExts.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

#if defined(WIN32)
      enabledInstExts.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(ANDROID)
      enabledInstExts.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
      enabledInstExts.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);

      X11Window::Init();
#elif defined(__APPLE__)
      enabledInstExts.push_back(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);

      AppleWindow::Init();
#else
#error UNKNOWN PLATFORM
#endif

      std::vector<const char *> optInstExts;

      // this is used by so many sub extensions, initialise it if we can.
      optInstExts.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

      // enable debug utils when possible
      optInstExts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

      CHECK_VKR(vkh::enumerateInstanceLayerProperties(availInstLayers));

      if(debugDevice)
      {
        bool found = false;

        for(const VkLayerProperties &layer : availInstLayers)
        {
          if(!strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation"))
          {
            enabledLayers.push_back("VK_LAYER_KHRONOS_validation");
            found = true;
            break;
          }
        }

        if(!found)
        {
          for(const VkLayerProperties &layer : availInstLayers)
          {
            if(!strcmp(layer.layerName, "VK_LAYER_LUNARG_standard_validation"))
            {
              enabledLayers.push_back("VK_LAYER_LUNARG_standard_validation");
              found = true;
              break;
            }
          }
        }
      }

      CHECK_VKR(vkh::enumerateInstanceExtensionProperties(availInstExts, NULL));

      for(const char *l : enabledLayers)
      {
        bool supported = false;
        for(const VkLayerProperties &layer : availInstLayers)
        {
          if(!strcmp(layer.layerName, l))
          {
            supported = true;
            break;
          }
        }

        if(!supported)
        {
          Avail = "Vulkan layer '";
          Avail += l;
          Avail += "' is not available";
          return;
        }

        std::vector<VkExtensionProperties> tmp;
        CHECK_VKR(vkh::enumerateInstanceExtensionProperties(tmp, l));

        for(const VkExtensionProperties &t : tmp)
          availInstExts.push_back(t);
      }

      // strip any extensions that are not supported
      for(auto it = enabledInstExts.begin(); it != enabledInstExts.end();)
      {
        bool found = false;
        for(VkExtensionProperties &ext : availInstExts)
        {
          if(!strcmp(ext.extensionName, *it))
          {
            found = true;
            break;
          }
        }

        if(found)
        {
          ++it;
        }
        else
        {
          DEBUG_BREAK();
          it = enabledInstExts.erase(it);
        }
      }

      // add any optional extensions that are supported
      for(const char *search : optInstExts)
      {
        bool found = false;
        for(VkExtensionProperties &ext : availInstExts)
        {
          if(!strcmp(ext.extensionName, search))
          {
            found = true;
            break;
          }
        }

        if(found)
          enabledInstExts.push_back(search);
      }

      vulkanVersion = volkGetInstanceVersion();

      vkh::ApplicationInfo app("RenderDoc autotesting", VK_MAKE_VERSION(1, 0, 0),
                               "RenderDoc autotesting", VK_MAKE_VERSION(1, 0, 0), vulkanVersion);

      TEST_LOG("Initialising Vulkan at VK%u.%u", VK_VERSION_MAJOR(vulkanVersion),
               VK_VERSION_MINOR(vulkanVersion));

      VkResult vkr = vkCreateInstance(
          vkh::InstanceCreateInfo(app, enabledLayers, enabledInstExts).next(instInfoNext), NULL,
          &inst);

      if(vkr != VK_SUCCESS)
      {
        TEST_ERROR("Error initialising vulkan instance: %d", vkr);
      }
      else
      {
        volkLoadInstance((VkInstance)inst);

        std::vector<VkPhysicalDevice> physDevices;
        CHECK_VKR(vkh::enumeratePhysicalDevices(physDevices, inst));

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
            selectedPhys = physDevices[i];
            break;
          }
        }

        // if none found, default to first
        if(selectedPhys == VK_NULL_HANDLE && !physDevices.empty())
          selectedPhys = physDevices[0];

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

              if(haystack.find(needle) != std::string::npos ||
                 (nv && physProps[p].vendorID == PCI_VENDOR_NV) ||
                 (amd && physProps[p].vendorID == PCI_VENDOR_AMD) ||
                 (intel && physProps[p].vendorID == PCI_VENDOR_INTEL))
              {
                selectedPhys = physDevices[p];
                break;
              }
            }

            break;
          }
        }
      }
    }
  }

  instance = inst;
  phys = selectedPhys;

  if(!volk)
    Avail = "volk did not initialise - vulkan library is not available";
  else if(!spv)
    Avail = InternalSpvCompiler() ? "Internal SPIR-V compiler did not initialise"
                                  : "Couldn't find 'glslc' or 'glslangValidator' in PATH - "
                                    "required for SPIR-V compilation";
  else if(instance == VK_NULL_HANDLE)
    Avail = "Vulkan instance did not initialise";
  else if(phys == VK_NULL_HANDLE)
    Avail = "Couldn't find vulkan physical device";

  if(!Avail.empty())
    return;

  devExts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

  VkPhysicalDeviceFeatures supported;
  vkGetPhysicalDeviceFeatures(phys, &supported);

#define CHECK_FEATURE(a)                                                  \
  if(features.a && !supported.a)                                          \
  {                                                                       \
    Avail = "Required physical device feature '" #a "' is not supported"; \
    return;                                                               \
  }                                                                       \
  if(optFeatures.a && supported.a)                                        \
  {                                                                       \
    features.a = VK_TRUE;                                                 \
  }

  CHECK_FEATURE(robustBufferAccess);
  CHECK_FEATURE(fullDrawIndexUint32);
  CHECK_FEATURE(imageCubeArray);
  CHECK_FEATURE(independentBlend);
  CHECK_FEATURE(geometryShader);
  CHECK_FEATURE(tessellationShader);
  CHECK_FEATURE(sampleRateShading);
  CHECK_FEATURE(dualSrcBlend);
  CHECK_FEATURE(logicOp);
  CHECK_FEATURE(multiDrawIndirect);
  CHECK_FEATURE(drawIndirectFirstInstance);
  CHECK_FEATURE(depthClamp);
  CHECK_FEATURE(depthBiasClamp);
  CHECK_FEATURE(fillModeNonSolid);
  CHECK_FEATURE(depthBounds);
  CHECK_FEATURE(wideLines);
  CHECK_FEATURE(largePoints);
  CHECK_FEATURE(alphaToOne);
  CHECK_FEATURE(multiViewport);
  CHECK_FEATURE(samplerAnisotropy);
  CHECK_FEATURE(textureCompressionETC2);
  CHECK_FEATURE(textureCompressionASTC_LDR);
  CHECK_FEATURE(textureCompressionBC);
  CHECK_FEATURE(occlusionQueryPrecise);
  CHECK_FEATURE(pipelineStatisticsQuery);
  CHECK_FEATURE(vertexPipelineStoresAndAtomics);
  CHECK_FEATURE(fragmentStoresAndAtomics);
  CHECK_FEATURE(shaderTessellationAndGeometryPointSize);
  CHECK_FEATURE(shaderImageGatherExtended);
  CHECK_FEATURE(shaderStorageImageExtendedFormats);
  CHECK_FEATURE(shaderStorageImageMultisample);
  CHECK_FEATURE(shaderStorageImageReadWithoutFormat);
  CHECK_FEATURE(shaderStorageImageWriteWithoutFormat);
  CHECK_FEATURE(shaderUniformBufferArrayDynamicIndexing);
  CHECK_FEATURE(shaderSampledImageArrayDynamicIndexing);
  CHECK_FEATURE(shaderStorageBufferArrayDynamicIndexing);
  CHECK_FEATURE(shaderStorageImageArrayDynamicIndexing);
  CHECK_FEATURE(shaderClipDistance);
  CHECK_FEATURE(shaderCullDistance);
  CHECK_FEATURE(shaderFloat64);
  CHECK_FEATURE(shaderInt64);
  CHECK_FEATURE(shaderInt16);
  CHECK_FEATURE(shaderResourceResidency);
  CHECK_FEATURE(shaderResourceMinLod);
  CHECK_FEATURE(sparseBinding);
  CHECK_FEATURE(sparseResidencyBuffer);
  CHECK_FEATURE(sparseResidencyImage2D);
  CHECK_FEATURE(sparseResidencyImage3D);
  CHECK_FEATURE(sparseResidency2Samples);
  CHECK_FEATURE(sparseResidency4Samples);
  CHECK_FEATURE(sparseResidency8Samples);
  CHECK_FEATURE(sparseResidency16Samples);
  CHECK_FEATURE(sparseResidencyAliased);
  CHECK_FEATURE(variableMultisampleRate);
  CHECK_FEATURE(inheritedQueries);

  CHECK_VKR(vkh::enumerateInstanceLayerProperties(availInstLayers));
  CHECK_VKR(vkh::enumerateInstanceExtensionProperties(availInstExts, NULL));

  instExts = enabledInstExts;
  instLayers = enabledLayers;

  for(const char *l : instLayers)
  {
    bool layerSupported = false;
    for(const VkLayerProperties &layer : availInstLayers)
    {
      if(!strcmp(layer.layerName, l))
      {
        layerSupported = true;
        break;
      }
    }

    if(!layerSupported)
    {
      Avail = "Vulkan layer '";
      Avail += l;
      Avail += "' is not available";
      return;
    }

    std::vector<VkExtensionProperties> tmp;
    CHECK_VKR(vkh::enumerateInstanceExtensionProperties(tmp, l));

    for(const VkExtensionProperties &t : tmp)
      availInstExts.push_back(t);
  }

  for(const char *search : instExts)
  {
    bool extSupported = false;
    for(const VkExtensionProperties &e : availInstExts)
    {
      if(!strcmp(e.extensionName, search))
      {
        extSupported = true;
        break;
      }
    }

    if(!extSupported)
    {
      Avail = "instance extension '";
      Avail += search;
      Avail += "' is not available";
      return;
    }
  }

  std::vector<VkExtensionProperties> supportedExts;
  CHECK_VKR(vkh::enumerateDeviceExtensionProperties(supportedExts, phys, NULL));

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

  vkGetPhysicalDeviceProperties(phys, &physProperties);

  instVersion = vulkanVersion;
  devVersion = physProperties.apiVersion;

  if(std::find(enabledInstExts.begin(), enabledInstExts.end(),
               VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) != enabledInstExts.end())
  {
    vkh::PhysicalDeviceProperties2KHR props2;
    vkGetPhysicalDeviceProperties2KHR(phys, props2);
    devVersion = props2.properties.apiVersion;
  }

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
      for(const char *layer : enabledLayers)
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
        Avail = "Required device extension '";
        Avail += search;
        Avail += "' is not supported";
        return;
      }
    }
  }

  std::vector<VkQueueFamilyProperties> queueProps;
  vkh::getQueueFamilyProperties(queueProps, phys);

  for(uint32_t q = 0; q < queueProps.size(); q++)
  {
    if(queueProps[q].queueFlags & VK_QUEUE_GRAPHICS_BIT)
    {
      if(graphicsQueueFamilyIndex == ~0U)
        graphicsQueueFamilyIndex = q;
    }
    else if(queueProps[q].queueFlags & VK_QUEUE_COMPUTE_BIT)
    {
      if(computeQueueFamilyIndex == ~0U)
        computeQueueFamilyIndex = q;
    }
    else if(queueProps[q].queueFlags & VK_QUEUE_TRANSFER_BIT)
    {
      if(transferQueueFamilyIndex == ~0U)
        transferQueueFamilyIndex = q;
    }
  }

  // if no queue has been selected, find it now
  if(queueFamilyIndex == ~0U)
  {
    // try to find an exact match first
    for(uint32_t q = 0; q < queueProps.size(); q++)
    {
      VkQueueFlags flags = queueProps[q].queueFlags;

      if(flags == queueFlagsRequired)
      {
        queueFamilyIndex = q;
        queueCount = 1;
        break;
      }
    }
  }

  if(queueFamilyIndex == ~0U)
  {
    // if we didn't find an exact match, look for any that does satisfy what we want
    for(uint32_t q = 0; q < queueProps.size(); q++)
    {
      VkQueueFlags flags = queueProps[q].queueFlags;

      if(((flags & queueFlagsRequired) == queueFlagsRequired) && ((flags & queueFlagsBanned) == 0))
      {
        queueFamilyIndex = q;
        queueCount = 1;
        break;
      }
    }
  }

  if(queueFamilyIndex == ~0U)
    Avail = "No satisfactory queue family available";
}

bool VulkanGraphicsTest::Init()
{
  // parse parameters here to override parameters
  if(!GraphicsTest::Init())
    return false;

  if(debugDevice)
  {
    CHECK_VKR(vkCreateDebugUtilsMessengerEXT(
        instance,
        vkh::DebugUtilsMessengerCreateInfoEXT(&vulkanCallback, NULL,
                                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT),
        NULL, &debugUtilsMessenger));
  }

  std::vector<VkExtensionProperties> supportedExts;
  CHECK_VKR(vkh::enumerateDeviceExtensionProperties(supportedExts, phys, NULL));

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

  const std::vector<float> priorities = {
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
      1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
  };

  std::vector<VkDeviceQueueCreateInfo> queueCreates = {
      vkh::DeviceQueueCreateInfo(queueFamilyIndex, queueCount, priorities),
  };

  if(queueFamilyIndex != graphicsQueueFamilyIndex && forceGraphicsQueue)
    queueCreates.push_back(vkh::DeviceQueueCreateInfo(graphicsQueueFamilyIndex, 1, priorities));
  if(queueFamilyIndex != computeQueueFamilyIndex &&
     (graphicsQueueFamilyIndex != computeQueueFamilyIndex || !forceGraphicsQueue) &&
     computeQueueFamilyIndex != ~0U && forceComputeQueue)
    queueCreates.push_back(vkh::DeviceQueueCreateInfo(computeQueueFamilyIndex, 1, priorities));
  if(queueFamilyIndex != transferQueueFamilyIndex &&
     graphicsQueueFamilyIndex != transferQueueFamilyIndex &&
     computeQueueFamilyIndex != transferQueueFamilyIndex && transferQueueFamilyIndex != ~0U &&
     forceTransferQueue)
    queueCreates.push_back(vkh::DeviceQueueCreateInfo(transferQueueFamilyIndex, 1, priorities));

  CHECK_VKR(vkCreateDevice(
      phys, vkh::DeviceCreateInfo(queueCreates, enabledLayers, devExts, features).next(devInfoNext),
      NULL, &device));

  volkLoadDevice(device);

  vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

  if(!headless)
  {
    mainWindow = MakeWindow(screenWidth, screenHeight, "Autotesting");

    if(!mainWindow->Initialised())
    {
      TEST_ERROR("Error creating surface");
      return false;
    }
  }

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
  allocInfo.frameInUseCount = 4;
  allocInfo.pVulkanFunctions = &funcs;
  if(hasExt(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME) && vmaDedicated)
    allocInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;

  vmaCreateAllocator(&allocInfo, &allocator);

  TEST_LOG("Running Vulkan test on %s (version %d.%d)", physProperties.deviceName,
           VK_VERSION_MAJOR(physProperties.apiVersion), VK_VERSION_MINOR(physProperties.apiVersion));

  headlessCmds = new VulkanCommands(this);

  if(!headless)
  {
    VkPipelineLayout layout = createPipelineLayout(vkh::PipelineLayoutCreateInfo());

    vkh::GraphicsPipelineCreateInfo pipeCreateInfo;

    pipeCreateInfo.layout = layout;
    pipeCreateInfo.renderPass = mainWindow->rp;

    pipeCreateInfo.vertexInputState.vertexBindingDescriptions = {vkh::vertexBind(0, DefaultA2V)};
    pipeCreateInfo.vertexInputState.vertexAttributeDescriptions = {
        vkh::vertexAttr(0, 0, DefaultA2V, pos),
        vkh::vertexAttr(1, 0, DefaultA2V, col),
        vkh::vertexAttr(2, 0, DefaultA2V, uv),
    };

    pipeCreateInfo.stages = {
        CompileShaderModule(VKDefaultVertex, ShaderLang::glsl, ShaderStage::vert, "main"),
        CompileShaderModule(VKDefaultPixel, ShaderLang::glsl, ShaderStage::frag, "main"),
    };

    DefaultTriPipe = createGraphicsPipeline(pipeCreateInfo);

    DefaultTriVB = AllocatedBuffer(
        this,
        vkh::BufferCreateInfo(sizeof(DefaultTri),
                              VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT),
        VmaAllocationCreateInfo({0, VMA_MEMORY_USAGE_CPU_TO_GPU}));

    DefaultTriVB.upload(DefaultTri);
  }

  return true;
}

VulkanWindow *VulkanGraphicsTest::MakeWindow(int width, int height, const char *title)
{
#if defined(WIN32)
  GraphicsWindow *platWin = new Win32Window(width, height, title);
#elif defined(ANDROID)
  GraphicsWindow *platWin = new AndroidWindow(width, height, title);
#elif defined(__linux__)
  GraphicsWindow *platWin = new X11Window(width, height, 0, title);
#elif defined(__APPLE__)
  GraphicsWindow *platWin = new AppleWindow(width, height, title);
#else
#error UNKNOWN PLATFORM
#endif

  return new VulkanWindow(this, platWin);
}

void VulkanGraphicsTest::Shutdown()
{
  if(device)
  {
    vkDeviceWaitIdle(device);

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

    for(VkBufferView view : bufferviews)
      vkDestroyBufferView(device, view, NULL);

    for(VkPipelineLayout layout : pipelayouts)
      vkDestroyPipelineLayout(device, layout, NULL);

    for(VkDescriptorSetLayout layout : setlayouts)
      vkDestroyDescriptorSetLayout(device, layout, NULL);

    for(VkSampler sampler : samplers)
      vkDestroySampler(device, sampler, NULL);

    for(auto it : imageAllocs)
      vmaDestroyImage(allocator, it.first, it.second);

    for(auto it : bufferAllocs)
      vmaDestroyBuffer(allocator, it.first, it.second);

    vmaDestroyAllocator(allocator);

    if(headlessCmds)
      delete headlessCmds;

    delete mainWindow;

    vkDestroyDevice(device, NULL);
  }

  if(debugUtilsMessenger)
    vkDestroyDebugUtilsMessengerEXT(instance, debugUtilsMessenger, NULL);

  if(instance)
    vkDestroyInstance(instance, NULL);
}

bool VulkanGraphicsTest::Running()
{
  if(!FrameLimit())
    return false;

  return mainWindow->Update();
}

VkImage VulkanGraphicsTest::StartUsingBackbuffer(VkCommandBuffer cmd, VkAccessFlags nextUse,
                                                 VkImageLayout layout, VulkanWindow *window)
{
  if(window == NULL)
    window = mainWindow;

  VkImage img = window->GetImage();

  vkh::cmdPipelineBarrier(
      cmd, {
               vkh::ImageMemoryBarrier(0, nextUse, VK_IMAGE_LAYOUT_UNDEFINED, layout, img),
           });

  return img;
}

void VulkanGraphicsTest::FinishUsingBackbuffer(VkCommandBuffer cmd, VkAccessFlags prevUse,
                                               VkImageLayout layout, VulkanWindow *window)
{
  if(window == NULL)
    window = mainWindow;

  VkImage img = window->GetImage();

  vkh::cmdPipelineBarrier(cmd, {
                                   vkh::ImageMemoryBarrier(prevUse, VK_ACCESS_MEMORY_READ_BIT, layout,
                                                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, img),
                               });
}

void VulkanGraphicsTest::Submit(int index, int totalSubmits, const std::vector<VkCommandBuffer> &cmds,
                                const std::vector<VkCommandBuffer> &seccmds)
{
  if(mainWindow)
    mainWindow->Submit(index, totalSubmits, cmds, seccmds, queue);
  else
    headlessCmds->Submit(cmds, seccmds, queue, VK_NULL_HANDLE, VK_NULL_HANDLE);
}

void VulkanGraphicsTest::SubmitAndPresent(const std::vector<VkCommandBuffer> &cmds)
{
  Submit(0, 1, cmds, {});
  Present();
}

void VulkanGraphicsTest::Present()
{
  mainWindow->Present(queue);
}

VkPipelineShaderStageCreateInfo VulkanGraphicsTest::CompileShaderModule(
    const std::string &source_text, ShaderLang lang, ShaderStage stage, const char *entry_point,
    const std::map<std::string, std::string> &macros, SPIRVTarget target)
{
  VkShaderModule ret = VK_NULL_HANDLE;

  std::vector<uint32_t> spirv =
      ::CompileShaderToSpv(source_text, target, lang, stage, entry_point, macros);

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

VkCommandBuffer VulkanGraphicsTest::GetCommandBuffer(VkCommandBufferLevel level, VulkanWindow *window)
{
  if(window == NULL)
    window = mainWindow;

  if(window)
    return window->GetCommandBuffer(level);

  return headlessCmds->GetCommandBuffer(level);
}

template <>
void VulkanGraphicsTest::setName(VkPipeline obj, const std::string &name)
{
  setName(VK_OBJECT_TYPE_PIPELINE, (uint64_t)obj, name);
}

template <>
void VulkanGraphicsTest::setName(VkFramebuffer obj, const std::string &name)
{
  setName(VK_OBJECT_TYPE_FRAMEBUFFER, (uint64_t)obj, name);
}

template <>
void VulkanGraphicsTest::setName(VkImage obj, const std::string &name)
{
  setName(VK_OBJECT_TYPE_IMAGE, (uint64_t)obj, name);
}

template <>
void VulkanGraphicsTest::setName(VkSampler obj, const std::string &name)
{
  setName(VK_OBJECT_TYPE_SAMPLER, (uint64_t)obj, name);
}

template <>
void VulkanGraphicsTest::setName(VkBuffer obj, const std::string &name)
{
  setName(VK_OBJECT_TYPE_BUFFER, (uint64_t)obj, name);
}

template <>
void VulkanGraphicsTest::setName(VkSemaphore obj, const std::string &name)
{
  setName(VK_OBJECT_TYPE_SEMAPHORE, (uint64_t)obj, name);
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
    vkCmdEndDebugUtilsLabelEXT(cmd);
}

void VulkanGraphicsTest::blitToSwap(VkCommandBuffer cmd, VkImage src, VkImageLayout srcLayout,
                                    VkImage dst, VkImageLayout dstLayout)
{
  VkImageBlit region = {};
  region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.srcSubresource.layerCount = 1;
  region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.dstSubresource.layerCount = 1;
  region.srcOffsets[1].x = mainWindow->scissor.extent.width;
  region.srcOffsets[1].y = mainWindow->scissor.extent.height;
  region.srcOffsets[1].z = 1;
  region.dstOffsets[1].x = mainWindow->scissor.extent.width;
  region.dstOffsets[1].y = mainWindow->scissor.extent.height;
  region.dstOffsets[1].z = 1;

  vkCmdBlitImage(cmd, src, srcLayout, dst, dstLayout, 1, &region, VK_FILTER_LINEAR);
}

void VulkanGraphicsTest::uploadBufferToImage(VkImage destImage, VkExtent3D destExtent,
                                             VkBuffer srcBuffer, VkImageLayout finalLayout)
{
  VkCommandBuffer cmd = GetCommandBuffer();

  vkBeginCommandBuffer(cmd, vkh::CommandBufferBeginInfo());

  vkh::cmdPipelineBarrier(
      cmd, {
               vkh::ImageMemoryBarrier(0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, destImage),
           });

  VkBufferImageCopy copy = {};
  copy.imageExtent = destExtent;
  copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy.imageSubresource.layerCount = 1;

  vkCmdCopyBufferToImage(cmd, srcBuffer, destImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

  vkh::cmdPipelineBarrier(
      cmd, {
               vkh::ImageMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, finalLayout, destImage),
           });

  vkEndCommandBuffer(cmd);

  Submit(99, 99, {cmd});

  vkDeviceWaitIdle(device);
}

void VulkanGraphicsTest::pushMarker(VkQueue q, const std::string &name)
{
  if(vkQueueBeginDebugUtilsLabelEXT)
  {
    VkDebugUtilsLabelEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pLabelName = name.c_str();
    vkQueueBeginDebugUtilsLabelEXT(q, &info);
  }
}

void VulkanGraphicsTest::setMarker(VkQueue q, const std::string &name)
{
  if(vkQueueInsertDebugUtilsLabelEXT)
  {
    VkDebugUtilsLabelEXT info = {};
    info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    info.pLabelName = name.c_str();
    vkQueueInsertDebugUtilsLabelEXT(q, &info);
  }
}

void VulkanGraphicsTest::popMarker(VkQueue q)
{
  if(vkQueueEndDebugUtilsLabelEXT)
    vkQueueEndDebugUtilsLabelEXT(q);
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

    std::vector<VkDescriptorPoolSize> poolSizes = {
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
    };

    VkDescriptorPoolInlineUniformBlockCreateInfo inlineCreateInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_INLINE_UNIFORM_BLOCK_CREATE_INFO,
    };
    void *next = NULL;

    if(hasExt(VK_EXT_INLINE_UNIFORM_BLOCK_EXTENSION_NAME) || devVersion >= VK_MAKE_VERSION(1, 3, 0))
    {
      poolSizes.push_back({VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, 128 * 4096});

      inlineCreateInfo.maxInlineUniformBlockBindings = 1024;
      next = &inlineCreateInfo;
    }

    CHECK_VKR(vkCreateDescriptorPool(
        device, vkh::DescriptorPoolCreateInfo(128, poolSizes).next(next), NULL, &pool));
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

VkBufferView VulkanGraphicsTest::createBufferView(const VkBufferViewCreateInfo *info)
{
  VkBufferView ret;
  CHECK_VKR(vkCreateBufferView(device, info, NULL, &ret));
  bufferviews.push_back(ret);
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

VkSampler VulkanGraphicsTest::createSampler(const VkSamplerCreateInfo *info)
{
  VkSampler ret;
  CHECK_VKR(vkCreateSampler(device, info, NULL, &ret));
  samplers.push_back(ret);
  return ret;
}

VulkanCommands::VulkanCommands(VulkanGraphicsTest *test)
{
  m_Test = test;

  CHECK_VKR(vkCreateCommandPool(
      m_Test->device,
      vkh::CommandPoolCreateInfo(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                 m_Test->queueFamilyIndex),
      NULL, &cmdPool));
}

VulkanCommands::~VulkanCommands()
{
  vkDestroyCommandPool(m_Test->device, cmdPool, NULL);

  for(VkFence fence : fences)
    vkDestroyFence(m_Test->device, fence, NULL);
}

VkCommandBuffer VulkanCommands::GetCommandBuffer(VkCommandBufferLevel level)
{
  std::vector<VkCommandBuffer> &buflist = freeCommandBuffers[level];

  if(buflist.empty())
  {
    buflist.resize(4);

    CHECK_VKR(vkAllocateCommandBuffers(
        m_Test->device, vkh::CommandBufferAllocateInfo(cmdPool, 4, level), &buflist[0]));
  }

  VkCommandBuffer ret = buflist.back();
  buflist.pop_back();

  return ret;
}

void VulkanCommands::Submit(const std::vector<VkCommandBuffer> &cmds,
                            const std::vector<VkCommandBuffer> &seccmds, VkQueue q,
                            VkSemaphore wait, VkSemaphore signal)
{
  VkFence fence;
  CHECK_VKR(vkCreateFence(m_Test->device, vkh::FenceCreateInfo(), NULL, &fence));

  fences.insert(fence);

  if(m_Test->hasExt(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME))
  {
    VkSubmitInfo2KHR submit = {VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR};

    std::vector<VkCommandBufferSubmitInfoKHR> cmdSubmits;
    for(VkCommandBuffer cmd : cmds)
      cmdSubmits.push_back({VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR, NULL, cmd, 0});

    submit.commandBufferInfoCount = (uint32_t)cmdSubmits.size();
    submit.pCommandBufferInfos = cmdSubmits.data();

    VkSemaphoreSubmitInfoKHR waitInfo = {}, signalInfo = {};

    if(wait != VK_NULL_HANDLE)
    {
      waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
      waitInfo.semaphore = wait;
      waitInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;

      submit.waitSemaphoreInfoCount = 1;
      submit.pWaitSemaphoreInfos = &waitInfo;
    }

    if(signal != VK_NULL_HANDLE)
    {
      signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
      signalInfo.semaphore = signal;
      signalInfo.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT_KHR;

      submit.signalSemaphoreInfoCount = 1;
      submit.pSignalSemaphoreInfos = &signalInfo;
    }

    CHECK_VKR(vkQueueSubmit2KHR(q, 1, &submit, fence));
  }
  else
  {
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    VkSubmitInfo submit = vkh::SubmitInfo(cmds);

    if(wait != VK_NULL_HANDLE)
    {
      submit.waitSemaphoreCount = 1;
      submit.pWaitDstStageMask = &waitStage;
      submit.pWaitSemaphores = &wait;
    }

    if(signal != VK_NULL_HANDLE)
    {
      submit.signalSemaphoreCount = 1;
      submit.pSignalSemaphores = &signal;
    }

    CHECK_VKR(vkQueueSubmit(q, 1, &submit, fence));
  }

  for(const VkCommandBuffer &cmd : cmds)
    pendingCommandBuffers[0].push_back(std::make_pair(cmd, fence));

  for(const VkCommandBuffer &cmd : seccmds)
    pendingCommandBuffers[1].push_back(std::make_pair(cmd, fence));
}

VulkanWindow::VulkanWindow(VulkanGraphicsTest *test, GraphicsWindow *win)
    : GraphicsWindow(win->title), VulkanCommands(test)
{
  m_Test = test;
  m_Win = win;

  {
    std::lock_guard<std::mutex> lock(m_Test->mutex);

    for(size_t i = 0; i < ARRAY_COUNT(renderStartSemaphore); i++)
    {
      CHECK_VKR(vkCreateSemaphore(m_Test->device, vkh::SemaphoreCreateInfo(), NULL,
                                  &renderStartSemaphore[i]));
      CHECK_VKR(vkCreateSemaphore(m_Test->device, vkh::SemaphoreCreateInfo(), NULL,
                                  &renderEndSemaphore[i]));

      test->setName(renderStartSemaphore[i], title + " renderStartSemaphore" + std::to_string(i));
      test->setName(renderEndSemaphore[i], title + " renderEndSemaphore" + std::to_string(i));
    }

#if defined(WIN32)
    VkWin32SurfaceCreateInfoKHR createInfo;

    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.hwnd = ((Win32Window *)win)->wnd;
    createInfo.hinstance = GetModuleHandleA(NULL);

    vkCreateWin32SurfaceKHR(m_Test->instance, &createInfo, NULL, &surface);
#elif defined(ANDROID)
    VkAndroidSurfaceCreateInfoKHR createInfo;

    createInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.window = ((AndroidWindow *)win)->window;

    vkCreateAndroidSurfaceKHR(m_Test->instance, &createInfo, NULL, &surface);
#elif defined(__linux__)
    VkXcbSurfaceCreateInfoKHR createInfo;

    createInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.connection = ((X11Window *)win)->xcb.connection;
    createInfo.window = ((X11Window *)win)->xcb.window;

    vkCreateXcbSurfaceKHR(m_Test->instance, &createInfo, NULL, &surface);
#elif defined(__APPLE__)
    VkMacOSSurfaceCreateInfoMVK createInfo;

    createInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    createInfo.pNext = NULL;
    createInfo.flags = 0;
    createInfo.pView = ((AppleWindow *)win)->view;
    vkCreateMacOSSurfaceMVK(m_Test->instance, &createInfo, NULL, &surface);
#else
#error UNKNOWN PLATFORM
#endif
  }

  CreateSwapchain();

  Acquire();
}

VulkanWindow::~VulkanWindow()
{
  DestroySwapchain();

  {
    for(size_t i = 0; i < ARRAY_COUNT(renderStartSemaphore); i++)
    {
      vkDestroySemaphore(m_Test->device, renderStartSemaphore[i], NULL);
      vkDestroySemaphore(m_Test->device, renderEndSemaphore[i], NULL);
    }

    if(surface)
      vkDestroySurfaceKHR(m_Test->instance, surface, NULL);
  }

  delete m_Win;
}

void VulkanWindow::setViewScissor(VkCommandBuffer cmd)
{
  vkCmdSetViewport(cmd, 0, 1, &viewport);
  vkCmdSetScissor(cmd, 0, 1, &scissor);
}

bool VulkanWindow::CreateSwapchain()
{
  std::lock_guard<std::mutex> lock(m_Test->mutex);

  if(surface == VK_NULL_HANDLE)
    return false;

  VkResult vkr = VK_SUCCESS;

  VkSurfaceFormatKHR surfaceFormat = {};

  std::vector<VkSurfaceFormatKHR> formats;
  CHECK_VKR(vkh::getSurfaceFormatsKHR(formats, m_Test->phys, surface));

  VkBool32 support = VK_FALSE;
  CHECK_VKR(vkGetPhysicalDeviceSurfaceSupportKHR(m_Test->phys, m_Test->queueFamilyIndex, surface,
                                                 &support));
  TEST_ASSERT(support, "Presentation is not supported on surface");

  if(vkr != VK_SUCCESS || formats.empty())
  {
    TEST_ERROR("Error getting surface formats: %s", vkh::result_str(vkr));
    return false;
  }

  surfaceFormat = formats[0];

  for(const VkSurfaceFormatKHR &f : formats)
  {
    if(f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
    {
      surfaceFormat = f;
      break;
    }
  }

  if(surfaceFormat.format == VK_FORMAT_UNDEFINED)
  {
    surfaceFormat.format = VK_FORMAT_B8G8R8A8_SRGB;
    surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  }

  format = surfaceFormat.format;

  std::vector<VkPresentModeKHR> modes;
  CHECK_VKR(vkh::getSurfacePresentModesKHR(modes, m_Test->phys, surface));

  VkPresentModeKHR mode = VK_PRESENT_MODE_IMMEDIATE_KHR;

  if(std::find(modes.begin(), modes.end(), mode) == modes.end())
    mode = VK_PRESENT_MODE_FIFO_KHR;

  uint32_t width = 1, height = 1;

  VkSurfaceCapabilitiesKHR capabilities;
  CHECK_VKR(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_Test->phys, surface, &capabilities));

  width = capabilities.currentExtent.width;

  width = std::min(width, capabilities.maxImageExtent.width);
  width = std::max(width, capabilities.minImageExtent.width);

  height = capabilities.currentExtent.height;

  height = std::min(height, capabilities.maxImageExtent.height);
  height = std::max(height, capabilities.minImageExtent.height);

  viewport = vkh::Viewport(0, 0, (float)width, (float)height, 0.0f, 1.0f);
  scissor = vkh::Rect2D({0, 0}, {width, height});

  CHECK_VKR(vkCreateSwapchainKHR(
      m_Test->device,
      vkh::SwapchainCreateInfoKHR(
          surface, mode, surfaceFormat, {width, height},
          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT),
      NULL, &swap));

  CHECK_VKR(vkh::getSwapchainImagesKHR(imgs, m_Test->device, swap));

  if(rp == VK_NULL_HANDLE)
  {
    vkh::RenderPassCreator renderPassCreateInfo;

    renderPassCreateInfo.attachments.push_back(
        vkh::AttachmentDescription(format, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL));

    renderPassCreateInfo.addSubpass({VkAttachmentReference({0, VK_IMAGE_LAYOUT_GENERAL})});

    rp = m_Test->createRenderPass(renderPassCreateInfo);
  }

  TEST_ASSERT(imgs.size() <= ARRAY_COUNT(renderStartSemaphore),
              "Expected to have one semaphore set per image");

  imgviews.resize(imgs.size());
  for(size_t i = 0; i < imgs.size(); i++)
  {
    CHECK_VKR(vkCreateImageView(m_Test->device,
                                vkh::ImageViewCreateInfo(imgs[i], VK_IMAGE_VIEW_TYPE_2D, format),
                                NULL, &imgviews[i]));
  }
  fbs.resize(imgs.size());
  for(size_t i = 0; i < imgviews.size(); i++)
    fbs[i] = m_Test->createFramebuffer(vkh::FramebufferCreateInfo(rp, {imgviews[i]}, scissor.extent));

  return true;
}

void VulkanWindow::Acquire()
{
  if(swap == VK_NULL_HANDLE)
    return;

  semIdx = (semIdx + 1) % ARRAY_COUNT(renderStartSemaphore);

  VkResult vkr = vkAcquireNextImageKHR(m_Test->device, swap, UINT64_MAX,
                                       renderStartSemaphore[semIdx], VK_NULL_HANDLE, &imgIndex);

  if(vkr == VK_SUBOPTIMAL_KHR || vkr == VK_ERROR_OUT_OF_DATE_KHR)
  {
    DestroySwapchain();
    CreateSwapchain();

    vkr = vkAcquireNextImageKHR(m_Test->device, swap, UINT64_MAX, renderStartSemaphore[semIdx],
                                VK_NULL_HANDLE, &imgIndex);
  }
}

void VulkanWindow::Submit(int index, int totalSubmits, const std::vector<VkCommandBuffer> &cmds,
                          const std::vector<VkCommandBuffer> &seccmds, VkQueue q)
{
  VkSemaphore signal = VK_NULL_HANDLE, wait = VK_NULL_HANDLE;

  if(index == 0)
    wait = renderStartSemaphore[semIdx];
  if(index == totalSubmits - 1)
    signal = renderEndSemaphore[semIdx];

  VulkanCommands::Submit(cmds, seccmds, q, wait, signal);
}

void VulkanWindow::MultiPresent(VkQueue queue, std::vector<VulkanWindow *> windows)
{
  std::vector<VkSwapchainKHR> swaps;
  std::vector<uint32_t> idxs;
  std::vector<VkSemaphore> waitSems;
  std::vector<VkResult> vkrs;

  for(auto it : windows)
  {
    if(it->swap == VK_NULL_HANDLE)
      continue;

    swaps.push_back(it->swap);
    idxs.push_back(it->imgIndex);
    waitSems.push_back(it->renderEndSemaphore[it->semIdx]);
    vkrs.push_back(VK_SUCCESS);
  }

  if(swaps.empty())
    return;

  VkPresentInfoKHR info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  info.swapchainCount = (uint32_t)swaps.size();
  info.waitSemaphoreCount = (uint32_t)waitSems.size();
  info.pSwapchains = swaps.data();
  info.pImageIndices = idxs.data();
  info.pWaitSemaphores = waitSems.data();
  info.pResults = vkrs.data();

  vkQueuePresentKHR(queue, &info);

  size_t i = 0;
  for(auto it : windows)
  {
    if(it->swap == VK_NULL_HANDLE)
      continue;

    it->PostPresent(vkrs[i++]);
  }
}

void VulkanWindow::Present(VkQueue queue)
{
  if(swap == VK_NULL_HANDLE)
    return;

  VkResult vkr =
      vkQueuePresentKHR(queue, vkh::PresentInfoKHR(swap, imgIndex, &renderEndSemaphore[semIdx]));

  PostPresent(vkr);
}

void VulkanWindow::PostPresent(VkResult vkr)
{
  if(vkr == VK_SUBOPTIMAL_KHR || vkr == VK_ERROR_OUT_OF_DATE_KHR)
  {
    DestroySwapchain();
    CreateSwapchain();
  }
  else if(vkr != VK_SUCCESS)
  {
    VkResult queuePresentError = vkr;
    CHECK_VKR(queuePresentError);
  }

  VulkanCommands::ProcessCompletions();

  Acquire();
}

void VulkanCommands::ProcessCompletions()
{
  std::set<VkFence> doneFences;
  std::map<VkFence, VkResult> fenceStatus;

  // only test each fence once so we avoid the problem of testing a fence once, finding it's not
  // ready, then testing it again in a second use and finding that it's now ready, and deleting
  // it
  for(VkFence f : fences)
    fenceStatus[f] = vkGetFenceStatus(m_Test->device, f);

  for(int level = 0; level < 2; level++)
  {
    for(auto it = pendingCommandBuffers[level].begin(); it != pendingCommandBuffers[level].end();)
    {
      if(fenceStatus[it->second] == VK_SUCCESS)
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
    vkDestroyFence(m_Test->device, *it, NULL);
    fences.erase(*it);
  }
}

void VulkanWindow::DestroySwapchain()
{
  std::lock_guard<std::mutex> lock(m_Test->mutex);

  vkDeviceWaitIdle(m_Test->device);

  for(size_t i = 0; i < imgs.size(); i++)
    vkDestroyImageView(m_Test->device, imgviews[i], NULL);

  vkDestroySwapchainKHR(m_Test->device, swap, NULL);
}

void VulkanGraphicsTest::getPhysFeatures2(void *nextStruct)
{
  for(const char *ext : enabledInstExts)
  {
    if(!strcmp(ext, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
    {
      vkGetPhysicalDeviceFeatures2KHR(phys, vkh::PhysicalDeviceFeatures2KHR().next(nextStruct));
      return;
    }
  }
}

void VulkanGraphicsTest::getPhysProperties2(void *nextStruct)
{
  for(const char *ext : enabledInstExts)
  {
    if(!strcmp(ext, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
    {
      vkGetPhysicalDeviceProperties2KHR(phys, vkh::PhysicalDeviceProperties2KHR().next(nextStruct));
      return;
    }
  }
}

bool VulkanGraphicsTest::hasExt(const char *ext)
{
  return std::find_if(devExts.begin(), devExts.end(),
                      [ext](const char *a) { return !strcmp(a, ext); }) != devExts.end();
}

AllocatedImage::AllocatedImage(VulkanGraphicsTest *test, const VkImageCreateInfo &imgInfo,
                               const VmaAllocationCreateInfo &allocInfo)
{
  createInfo = imgInfo;
  this->test = test;
  allocator = test->allocator;
  vmaCreateImage(allocator, &imgInfo, &allocInfo, &image, &alloc, NULL);

  test->imageAllocs[image] = alloc;
}

void AllocatedImage::free()
{
  vmaDestroyImage(allocator, image, alloc);
  test->imageAllocs.erase(image);
}

AllocatedBuffer::AllocatedBuffer(VulkanGraphicsTest *test, const VkBufferCreateInfo &bufInfo,
                                 const VmaAllocationCreateInfo &allocInfo)
{
  this->test = test;
  allocator = test->allocator;
  vmaCreateBuffer(allocator, &bufInfo, &allocInfo, &buffer, &alloc, NULL);

  test->bufferAllocs[buffer] = alloc;
}

void AllocatedBuffer::free()
{
  vmaDestroyBuffer(allocator, buffer, alloc);
  test->bufferAllocs.erase(buffer);
}
