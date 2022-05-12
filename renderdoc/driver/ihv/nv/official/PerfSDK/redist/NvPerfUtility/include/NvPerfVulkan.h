/*
* Copyright 2014-2022 NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include <vulkan/vulkan.h>
#include "NvPerfInit.h"
#include "NvPerfDeviceProperties.h"
#include "NvPerfPeriodicSamplerGpu.h"
#include "nvperf_vulkan_host.h"
#include "nvperf_vulkan_target.h"

namespace nv { namespace perf {

    //
    // Vulkan Only Utilities
    //

    inline std::string VulkanGetDeviceName(VkPhysicalDevice physicalDevice)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        return properties.deviceName;
    }

    inline bool VulkanIsNvidiaDevice(VkPhysicalDevice physicalDevice)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        if (properties.vendorID != NVIDIA_VENDOR_ID)
        {
            return false;
        }

        return true;
    }

    inline uint32_t VulkanGetInstanceApiVersion()
    {
        PFN_vkEnumerateInstanceVersion pfnVkEnumerateInstanceVersion = (PFN_vkEnumerateInstanceVersion)vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
        //This API doesn't exist on 1.0 loader
        if (!pfnVkEnumerateInstanceVersion)
        {
            return VK_API_VERSION_1_0;
        }
        
        uint32_t loaderVersion;
        VkResult res = pfnVkEnumerateInstanceVersion(&loaderVersion);
        if (res != VK_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "Couldn't enumerate instance version!\n");
            return 0;
        }
        return loaderVersion;
    }

    inline uint32_t VulkanGetPhysicalDeviceApiVersion(VkPhysicalDevice physicalDevice)
    {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        return properties.apiVersion;
    }

    //
    // Vulkan NvPerf Utilities
    //
    inline bool VulkanAppendInstanceRequiredExtensions(std::vector<const char*>& instanceExtensionNames, uint32_t apiVersion)
    {
        NVPW_VK_Profiler_GetRequiredInstanceExtensions_Params getRequiredInstanceExtensionsParams = { NVPW_VK_Profiler_GetRequiredInstanceExtensions_Params_STRUCT_SIZE };
        getRequiredInstanceExtensionsParams.apiVersion = apiVersion;

        NVPA_Status nvpaStatus = NVPW_VK_Profiler_GetRequiredInstanceExtensions(&getRequiredInstanceExtensionsParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_VK_Profiler_GetRequiredInstanceExtensions failed\n");
            return false;
        }

        if (!getRequiredInstanceExtensionsParams.isOfficiallySupportedVersion)
        {
            uint32_t major = VK_VERSION_MAJOR(getRequiredInstanceExtensionsParams.apiVersion);
            uint32_t minor = VK_VERSION_MINOR(getRequiredInstanceExtensionsParams.apiVersion);
            uint32_t patch = VK_VERSION_PATCH(getRequiredInstanceExtensionsParams.apiVersion);
            // not an error - NvPerf treats any unknown version as the same as its latest known version.
            //                Unknown version warnings should be reported back to the Nsight Perf team to get official support
            NV_PERF_LOG_WRN(10, "Vulkan Instance API Version: %u.%u.%u - is not an officially supported version\n", major, minor, patch);
        }

        for (uint32_t extensionIndex=0; extensionIndex < getRequiredInstanceExtensionsParams.numInstanceExtensionNames; ++ extensionIndex)
        {
            instanceExtensionNames.push_back(getRequiredInstanceExtensionsParams.ppInstanceExtensionNames[extensionIndex]);
        }
        return true;
    }

    inline bool VulkanAppendDeviceRequiredExtensions(VkInstance instance, VkPhysicalDevice physicalDevice, void* pfnGetInstanceProcAddr, std::vector<const char*>& deviceExtensionNames)
    {
        if (!VulkanIsNvidiaDevice(physicalDevice))
        {
            return true; // do nothing on non-NVIDIA devices
        }

        NVPW_VK_Profiler_GetRequiredDeviceExtensions_Params getRequiredDeviceExtensionsParams = { NVPW_VK_Profiler_GetRequiredDeviceExtensions_Params_STRUCT_SIZE };
        getRequiredDeviceExtensionsParams.apiVersion = VulkanGetPhysicalDeviceApiVersion(physicalDevice);

        // optional parameters - this allows NvPerf to query if certain advanced features are available for use
        getRequiredDeviceExtensionsParams.instance = instance;
        getRequiredDeviceExtensionsParams.physicalDevice = physicalDevice;
        getRequiredDeviceExtensionsParams.pfnGetInstanceProcAddr = pfnGetInstanceProcAddr;

        NVPA_Status nvpaStatus = NVPW_VK_Profiler_GetRequiredDeviceExtensions(&getRequiredDeviceExtensionsParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_VK_Profiler_GetRequiredDeviceExtensions failed\n");
            return false;
        }

        if (!getRequiredDeviceExtensionsParams.isOfficiallySupportedVersion)
        {
            uint32_t major = VK_VERSION_MAJOR(getRequiredDeviceExtensionsParams.apiVersion);
            uint32_t minor = VK_VERSION_MINOR(getRequiredDeviceExtensionsParams.apiVersion);
            uint32_t patch = VK_VERSION_PATCH(getRequiredDeviceExtensionsParams.apiVersion);
            // not an error - NvPerf treats any unknown version as the same as its latest known version.
            //                Unknown version warnings should be reported back to the Nsight Perf team to get official support
            NV_PERF_LOG_WRN(100, "Vulkan Device API Version: %u.%u.%u - is not an officially supported version\n", major, minor, patch);
        }

        for (uint32_t extensionIndex=0; extensionIndex < getRequiredDeviceExtensionsParams.numDeviceExtensionNames; ++ extensionIndex)
        {
            deviceExtensionNames.push_back(getRequiredDeviceExtensionsParams.ppDeviceExtensionNames[extensionIndex]);
        }

        return true;
    }

    inline bool VulkanAppendRequiredExtensions(std::vector<const char*>& instanceExtensionNames, std::vector<const char*>& deviceExtensionNames, uint32_t apiVersion)
    {
        bool status = VulkanAppendInstanceRequiredExtensions(instanceExtensionNames, apiVersion);
        if (!status)
        {
            return false;
        }

        status = VulkanAppendDeviceRequiredExtensions(VK_NULL_HANDLE, VK_NULL_HANDLE, nullptr, deviceExtensionNames);
        if (!status)
        {
            return false;
        }

        return true;
    }

    inline bool VulkanLoadDriver(VkInstance instance)
    {
        NVPW_VK_LoadDriver_Params loadDriverParams = { NVPW_VK_LoadDriver_Params_STRUCT_SIZE };
        loadDriverParams.instance = instance;
        NVPA_Status nvpaStatus = NVPW_VK_LoadDriver(&loadDriverParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_VK_LoadDriver failed\n");
            return false;
        }
        return true;
    }

    inline size_t VulkanGetNvperfDeviceIndex(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, size_t sliIndex = 0)
    {
        NVPW_VK_Device_GetDeviceIndex_Params getDeviceIndexParams = { NVPW_VK_Device_GetDeviceIndex_Params_STRUCT_SIZE };
        getDeviceIndexParams.instance = instance;
        getDeviceIndexParams.physicalDevice = physicalDevice;
        getDeviceIndexParams.device = device;
        getDeviceIndexParams.sliIndex = sliIndex;
        getDeviceIndexParams.pfnGetInstanceProcAddr = (void*)vkGetInstanceProcAddr;
        getDeviceIndexParams.pfnGetDeviceProcAddr = (void*)vkGetDeviceProcAddr;

        NVPA_Status nvpaStatus = NVPW_VK_Device_GetDeviceIndex(&getDeviceIndexParams);
        if (nvpaStatus)
        {
            return ~size_t(0);
        }

        return getDeviceIndexParams.deviceIndex;
    }

    inline DeviceIdentifiers VulkanGetDeviceIdentifiers(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, size_t sliIndex = 0)
    {
        const size_t deviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device, sliIndex);

        DeviceIdentifiers deviceIdentifiers = GetDeviceIdentifiers(deviceIndex);
        return deviceIdentifiers;
    }

    inline NVPW_Device_ClockStatus VulkanGetDeviceClockState(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device)
    {
        size_t nvperfDeviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device);
        return GetDeviceClockState(nvperfDeviceIndex);
    }

    inline bool VulkanSetDeviceClockState(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, NVPW_Device_ClockSetting clockStatus)
    {
        size_t nvperfDeviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device);
        return SetDeviceClockState(nvperfDeviceIndex, clockStatus);
    }

    inline bool VulkanSetDeviceClockState(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, NVPW_Device_ClockStatus clockStatus)
    {
        size_t nvperfDeviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device);
        return SetDeviceClockState(nvperfDeviceIndex, clockStatus);
    }

    inline size_t VulkanCalculateMetricsEvaluatorScratchBufferSize(const char* pChipName)
    {
        NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize_Params calculateScratchBufferSizeParams = { NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize_Params_STRUCT_SIZE };
        calculateScratchBufferSizeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize(&calculateScratchBufferSizeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_VK_MetricsEvaluator_CalculateScratchBufferSize failed\n");
            return 0;
        }
        return calculateScratchBufferSizeParams.scratchBufferSize;
    }

    inline NVPW_MetricsEvaluator* VulkanCreateMetricsEvaluator(uint8_t* pScratchBuffer, size_t scratchBufferSize, const char* pChipName)
    {
        NVPW_VK_MetricsEvaluator_Initialize_Params initializeParams = { NVPW_VK_MetricsEvaluator_Initialize_Params_STRUCT_SIZE };
        initializeParams.pScratchBuffer = pScratchBuffer;
        initializeParams.scratchBufferSize = scratchBufferSize;
        initializeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_VK_MetricsEvaluator_Initialize(&initializeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_VK_MetricsEvaluator_Initialize failed\n");
            return nullptr;
        }
        return initializeParams.pMetricsEvaluator;
    }

}}

namespace nv { namespace perf { namespace profiler {

    inline NVPA_RawMetricsConfig* VulkanCreateRawMetricsConfig(const char* pChipName)
    {
        NVPW_VK_RawMetricsConfig_Create_Params configParams = { NVPW_VK_RawMetricsConfig_Create_Params_STRUCT_SIZE };
        configParams.activityKind = NVPA_ACTIVITY_KIND_PROFILER;
        configParams.pChipName = pChipName;

        NVPA_Status nvpaStatus = NVPW_VK_RawMetricsConfig_Create(&configParams);
        if (nvpaStatus)
        {
            return nullptr;
        }

        return configParams.pRawMetricsConfig;
    }

    inline bool VulkanIsGpuSupported(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, size_t sliIndex = 0)
    {
        const size_t deviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device, sliIndex);

        NVPW_VK_Profiler_IsGpuSupported_Params params = { NVPW_VK_Profiler_IsGpuSupported_Params_STRUCT_SIZE };
        params.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_VK_Profiler_IsGpuSupported(&params);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_VK_Profiler_IsGpuSupported failed on %s\n", VulkanGetDeviceName(physicalDevice).c_str());
            return false;
        }

        if (!params.isSupported)
        {
            NV_PERF_LOG_ERR(10, "%s is not supported for profiling\n", VulkanGetDeviceName(physicalDevice).c_str());
            if (params.gpuArchitectureSupportLevel != NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_SUPPORTED)
            {
                const DeviceIdentifiers deviceIdentifiers = VulkanGetDeviceIdentifiers(instance, physicalDevice, device, sliIndex);
                NV_PERF_LOG_ERR(10, "Unsupported GPU architecture %s\n", deviceIdentifiers.pChipName);
            }
            if (params.sliSupportLevel == NVPW_SLI_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Devices in SLI configuration are not supported.\n");
            }
            if (params.cmpSupportLevel == NVPW_CMP_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Cryptomining GPUs (NVIDIA CMP) are not supported.\n");
            }
            return false;
        }

        return true;
    }

    inline bool VulkanPushRange(VkCommandBuffer commandBuffer, const char* pRangeName)
    {
        NVPW_VK_Profiler_CommandBuffer_PushRange_Params pushRangeParams = { NVPW_VK_Profiler_CommandBuffer_PushRange_Params_STRUCT_SIZE };
        pushRangeParams.pRangeName = pRangeName;
        pushRangeParams.rangeNameLength = 0;
        pushRangeParams.commandBuffer = commandBuffer;
        NVPA_Status nvpaStatus = NVPW_VK_Profiler_CommandBuffer_PushRange(&pushRangeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(50, "NVPW_VK_Profiler_CommandBuffer_PushRange failed\n");
            return false;
        }
        return true;
    }
    inline bool VulkanPopRange(VkCommandBuffer commandBuffer)
    {
        NVPW_VK_Profiler_CommandBuffer_PopRange_Params popParams = { NVPW_VK_Profiler_CommandBuffer_PopRange_Params_STRUCT_SIZE };
        popParams.commandBuffer = commandBuffer;
        NVPA_Status nvpaStatus = NVPW_VK_Profiler_CommandBuffer_PopRange(&popParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(50, "NVPW_VK_Profiler_CommandBuffer_PopRange failed\n");
            return false;
        }
        return true;
    }

    inline bool VulkanPushRange_Nop(VkCommandBuffer commandBuffer, const char* pRangeName)
    {
        return false;
    }
    inline bool VulkanPopRange_Nop(VkCommandBuffer commandBuffer)
    {
        return false;
    }

    // 
    struct VulkanRangeCommands
    {
        bool isNvidiaDevice;
        bool(*PushRange)(VkCommandBuffer commandBuffer, const char* pRangeName);
        bool(*PopRange)(VkCommandBuffer commandBuffer);

    public:
        VulkanRangeCommands()
            : isNvidiaDevice(false)
            , PushRange(&VulkanPushRange_Nop)
            , PopRange(&VulkanPopRange_Nop)
        {
        }

        void Initialize(bool isNvidiaDevice_)
        {
            isNvidiaDevice = isNvidiaDevice_;
            if (isNvidiaDevice_)
            {
                PushRange = &VulkanPushRange;
                PopRange = &VulkanPopRange;
            }
            else
            {
                PushRange = &VulkanPushRange_Nop;
                PopRange = &VulkanPopRange_Nop;
            }
        }

        void Initialize(VkPhysicalDevice physicalDevice)
        {
            const bool isNvidiaDevice_ = VulkanIsNvidiaDevice(physicalDevice);
            return Initialize(isNvidiaDevice_);
        }
    };

}}} // nv::perf::profiler

namespace nv { namespace perf { namespace mini_trace {

    inline bool VulkanIsGpuSupported(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, size_t sliIndex = 0)
    {
        const size_t deviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device, sliIndex);
        if (deviceIndex == ~size_t(0))
        {
            NV_PERF_LOG_ERR(10, "VulkanGetNvperfDeviceIndex failed on %ls\n", VulkanGetDeviceName(physicalDevice).c_str());
            return false;
        }

        NVPW_VK_MiniTrace_IsGpuSupported_Params params = { NVPW_VK_MiniTrace_IsGpuSupported_Params_STRUCT_SIZE };
        params.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_VK_MiniTrace_IsGpuSupported(&params);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_VK_MiniTrace_IsGpuSupported failed on %s\n", VulkanGetDeviceName(physicalDevice).c_str());
            return false;
        }

        if (!params.isSupported)
        {
            NV_PERF_LOG_ERR(10, "%s is not supported for profiling\n", VulkanGetDeviceName(physicalDevice).c_str());
            if (params.gpuArchitectureSupportLevel != NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_SUPPORTED)
            {
                const DeviceIdentifiers deviceIdentifiers = VulkanGetDeviceIdentifiers(instance, physicalDevice, device, sliIndex);
                NV_PERF_LOG_ERR(10, "Unsupported GPU architecture %s\n", deviceIdentifiers.pChipName);
            }
            if (params.sliSupportLevel == NVPW_SLI_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Devices in SLI configuration are not supported.\n");
            }
            if (params.cmpSupportLevel == NVPW_CMP_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Cryptomining GPUs (NVIDIA CMP) are not supported.\n");
            }
            return false;
        }

        return true;
    }

}}} // nv::perf::mini_trace

namespace nv { namespace perf { namespace sampler {

    inline bool VulkanIsGpuSupported(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, size_t sliIndex = 0)
    {
        const size_t deviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device, sliIndex);
        if (deviceIndex == ~size_t(0))
        {
            NV_PERF_LOG_ERR(10, "VulkanGetNvperfDeviceIndex failed on %ls\n", VulkanGetDeviceName(physicalDevice).c_str());
            return false;
        }
        if (!GpuPeriodicSamplerIsGpuSupported(deviceIndex))
        {
            return false;
        }
        if (!mini_trace::VulkanIsGpuSupported(instance, physicalDevice, device, sliIndex))
        {
            return false;
        }
        return true;
    }

}}} // nv::perf::sampler