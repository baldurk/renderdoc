//==============================================================================
// Copyright (c) 2017-2021 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  This file should be included by an application that wishes to use
///         the Vulkan version of GPUPerfAPI. It defines structures that should
///         be passed to the GPA_OpenContext calls when using GPUPerfAPI with
///         Vulkan.
//==============================================================================

#ifndef GPU_PERFORMANCE_API_GPU_PERF_API_VK_H_
#define GPU_PERFORMANCE_API_GPU_PERF_API_VK_H_

#ifndef VULKAN_H_
#include <vulkan/vulkan.h>
#endif  // VULKAN_H_

/// Define the AMD GPA extension name.
#define VK_AMD_GPA_INTERFACE_EXTENSION_NAME "VK_AMD_gpa_interface"

/// Define the AMD shader core properties extension name.
#define VK_AMD_SHADER_CORE_PROPERTIES_EXTENSION_NAME "VK_AMD_shader_core_properties"

/// Define the AMD shader core properties 2 extension name.
#define VK_AMD_SHADER_CORE_PROPERTIES_EXTENSION_NAME2 "VK_AMD_shader_core_properties2"

/// Define a macro to help developers include all instance-level extensions required to support the AMD GPA Interface.
#define AMD_GPA_REQUIRED_INSTANCE_EXTENSION_NAME_LIST VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME

/// Define a macro to help developers include all device-level extensions required to support the AMD GPA Interface.
#define AMD_GPA_REQUIRED_DEVICE_EXTENSION_NAME_LIST VK_AMD_GPA_INTERFACE_EXTENSION_NAME, VK_AMD_SHADER_CORE_PROPERTIES_EXTENSION_NAME

/// Define a macro to help developers include optional device-level extensions to support the AMD GPA Interface.
#define AMD_GPA_OPTIONAL_DEVICE_EXTENSION_NAME_LIST VK_AMD_SHADER_CORE_PROPERTIES_EXTENSION_NAME2

/// @brief The struct that should be supplied to GpaOpenContext().
///
/// The instance, physical device, and device should be set prior to calling GpaOpenContext()
/// to reflect the Vulkan objects on which profiling will take place.
typedef struct GpaVkContextOpenInfoType
{
    VkInstance       instance;         ///< The instance on which to profile.
    VkPhysicalDevice physical_device;  ///< The physical device on which to profile.
    VkDevice         device;           ///< The device on which to profile.
} GpaVkContextOpenInfo;

#endif  // GPU_PERFORMANCE_API_GPU_PERF_API_VK_H_
