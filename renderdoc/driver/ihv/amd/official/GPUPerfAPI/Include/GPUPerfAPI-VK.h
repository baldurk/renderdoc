//==============================================================================
// Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  This file should be included by an application that wishes to use
///         the Vulkan version of GPUPerfAPI. It defines structures that should
///         be passed to the GPA_OpenContext calls when using GPUPerfAPI with
///         Vulkan.
//==============================================================================

#ifndef _GPUPERFAPI_VK_H_
#define _GPUPERFAPI_VK_H_

/// Define the AMD GPA extension name
#define VK_AMD_GPA_INTERFACE_EXTENSION_NAME "VK_AMD_gpa_interface"

/// Define a macro to help developers include all instance-level extensions required to support the AMD GPA Interface.
#define AMD_GPA_REQUIRED_INSTANCE_EXTENSION_NAME_LIST     \
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME

/// Define a macro to help developers include all device-level extensions required to support the AMD GPA Interface.
#define AMD_GPA_REQUIRED_DEVICE_EXTENSION_NAME_LIST       \
    VK_AMD_GPA_INTERFACE_EXTENSION_NAME

/// The struct that should be supplied to GPA_OpenContext().
/// The instance, physicalDevice, and device should be set prior to
/// calling OpenContext() to reflect the Vulkan objects on which profiling
/// will take place.
typedef struct GPA_vkContextOpenInfo
{
    VkInstance instance;                ///< The instance on which to profile
    VkPhysicalDevice physicalDevice;    ///< The physical device on which to profile
    VkDevice device;                    ///< The device on which to profile
} GPA_vkContextOpenInfo;

#endif // _GPUPERFAPI_VK_H_
