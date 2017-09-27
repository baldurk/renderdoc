//==============================================================================
// Copyright (c) 2015-2017 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  This file should be included by an application that wishes to use
///         the Vulkan version of GPUPerfAPI. It defines structures that should
///         be passed to the GPA_OpenContext and GPA_BeginSampleList calls when
///         using GPUPerfAPI with Vulkan.
//==============================================================================

#ifndef _GPUPERFAPI_VK_H_
#define _GPUPERFAPI_VK_H_

#include <vulkan/vulkan.h>

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
