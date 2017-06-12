//==============================================================================
// Copyright (c) 2010-2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  This file is for internal use to define the function types.
//==============================================================================

#ifndef _GPUPERFAPI_TYPES_INTERNAL_H_
#define _GPUPERFAPI_TYPES_INTERNAL_H_

#include "GPUPerfAPITypes.h"

// For internal use only

/// Counter type definitions
typedef enum
{
    GPA_COUNTER_TYPE_DYNAMIC,     ///< hardware per sample counter type
    GPA_COUNTER_TYPE_SESSION,     ///< hardware per session counter type
    GPA_COUNTER_TYPE_API_DYNAMIC, ///< api per sample counter type
    GPA_COUNTER_TYPE_API_SESSION, ///< api per session counter
    GPA_COUNTER_TYPE__LAST        ///< Marker indicating last element
} GPA_CounterType;

/// Private Logging types that adds messages defined in Debug Builds only
enum GPA_Log_Debug_Type
{
    // these must match the public GPA_Logging_type enum (but note we change LOGGING to LOG)
    GPA_LOG_NONE = 0,
    GPA_LOG_ERROR = 1,
    GPA_LOG_MESSAGE = 2,
    GPA_LOG_ERROR_AND_MESSAGE = 3,
    GPA_LOG_TRACE = 4,
    GPA_LOG_ERROR_AND_TRACE = 5,
    GPA_LOG_MESSAGE_AND_TRACE = 6,
    GPA_LOG_ERROR_MESSAGE_AND_TRACE = 7,
    GPA_LOG_ALL = 0xFF,

#ifdef AMDT_INTERNAL
    // these are private types that are only defined in internal builds
    GPA_LOG_DEBUG_ERROR        = 0x0100,
    GPA_LOG_DEBUG_MESSAGE      = 0x0200,
    GPA_LOG_DEBUG_TRACE        = 0x0400,
    GPA_LOG_DEBUG_COUNTERDEFS  = 0x0800,
    GPA_LOG_DEBUG_ALL          = 0xFF00
#endif // AMDT_INTERNAL
};

/// this enum needs to be kept up to date with GDT_HW_GENERATION in DeviceInfo.h
enum GPA_HW_GENERATION
{
    GPA_HW_GENERATION_NONE,             ///< undefined hw generation
    GPA_HW_GENERATION_NVIDIA,           ///< Used for nvidia cards by GPA
    GPA_HW_GENERATION_INTEL,            ///< Used for Intel cards by GPA
    GPA_HW_GENERATION_SOUTHERNISLAND,   ///< GFX IP 6
    GPA_HW_GENERATION_SEAISLAND,        ///< GFX IP 7
    GPA_HW_GENERATION_VOLCANICISLAND,   ///< GFX IP 8
    GPA_HW_GENERATION_LAST
};

#endif // _GPUPERFAPI_TYPES_INTERNAL_H_
