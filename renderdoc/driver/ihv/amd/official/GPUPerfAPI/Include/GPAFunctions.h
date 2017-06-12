//==============================================================================
// Copyright (c) 2014-2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  GPA required public function declarations wrapped in a macro
//==============================================================================

#ifndef GPA_FUNCTION_PREFIX
    #define GPA_FUNCTION_PREFIX( f )             ///< placeholder macro in case it's not defined before including this file
    #define NEED_TO_UNDEFINE_GPA_FUNCTION_PREFIX ///< used a a flag to indicate whether or not the macro needs to be undefined later
#endif

GPA_FUNCTION_PREFIX(GPA_RegisterLoggingCallback)

#ifdef AMDT_INTERNAL
    GPA_FUNCTION_PREFIX(GPA_RegisterLoggingDebugCallback)
#endif // AMDT_INTERNAL

GPA_FUNCTION_PREFIX(GPA_Initialize)
GPA_FUNCTION_PREFIX(GPA_Destroy)

// Context
GPA_FUNCTION_PREFIX(GPA_OpenContext)
GPA_FUNCTION_PREFIX(GPA_CloseContext)
GPA_FUNCTION_PREFIX(GPA_SelectContext)

// Counter Interrogation
GPA_FUNCTION_PREFIX(GPA_GetNumCounters)
GPA_FUNCTION_PREFIX(GPA_GetCounterName)
GPA_FUNCTION_PREFIX(GPA_GetCounterDescription)
GPA_FUNCTION_PREFIX(GPA_GetCounterDataType)
GPA_FUNCTION_PREFIX(GPA_GetCounterUsageType)
GPA_FUNCTION_PREFIX(GPA_GetDataTypeAsStr)
GPA_FUNCTION_PREFIX(GPA_GetUsageTypeAsStr)
GPA_FUNCTION_PREFIX(GPA_GetStatusAsStr)

GPA_FUNCTION_PREFIX(GPA_EnableCounter)
GPA_FUNCTION_PREFIX(GPA_DisableCounter)
GPA_FUNCTION_PREFIX(GPA_GetEnabledCount)
GPA_FUNCTION_PREFIX(GPA_GetEnabledIndex)

GPA_FUNCTION_PREFIX(GPA_IsCounterEnabled)

GPA_FUNCTION_PREFIX(GPA_EnableCounterStr)
GPA_FUNCTION_PREFIX(GPA_DisableCounterStr)

GPA_FUNCTION_PREFIX(GPA_EnableAllCounters)
GPA_FUNCTION_PREFIX(GPA_DisableAllCounters)
GPA_FUNCTION_PREFIX(GPA_GetCounterIndex)

GPA_FUNCTION_PREFIX(GPA_GetPassCount)

GPA_FUNCTION_PREFIX(GPA_BeginSession)
GPA_FUNCTION_PREFIX(GPA_EndSession)

GPA_FUNCTION_PREFIX(GPA_BeginPass)
GPA_FUNCTION_PREFIX(GPA_EndPass)

GPA_FUNCTION_PREFIX(GPA_BeginSample)
GPA_FUNCTION_PREFIX(GPA_EndSample)

GPA_FUNCTION_PREFIX(GPA_GetSampleCount)

GPA_FUNCTION_PREFIX(GPA_IsSampleReady)
GPA_FUNCTION_PREFIX(GPA_IsSessionReady)
GPA_FUNCTION_PREFIX(GPA_GetSampleUInt64)
GPA_FUNCTION_PREFIX(GPA_GetSampleUInt32)
GPA_FUNCTION_PREFIX(GPA_GetSampleFloat64)
GPA_FUNCTION_PREFIX(GPA_GetSampleFloat32)

GPA_FUNCTION_PREFIX(GPA_GetDeviceID)
GPA_FUNCTION_PREFIX(GPA_GetDeviceDesc)

#ifdef AMDT_INTERNAL
    GPA_FUNCTION_PREFIX(GPA_InternalSetDrawCallCounts)
#endif

#ifdef NEED_TO_UNDEFINE_GPA_FUNCTION_PREFIX
    #undef GPA_FUNCTION_PREFIX
    #undef NEED_TO_UNDEFINE_GPA_FUNCTION_PREFIX
#endif
