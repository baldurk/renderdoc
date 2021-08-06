//==============================================================================
// Copyright (c) 2014-2021 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  GPA required public function declarations wrapped in a macro.
//==============================================================================

#ifndef GPA_FUNCTION_PREFIX
#define GPA_FUNCTION_PREFIX(f)                ///< Placeholder macro in case it's not defined before including this file.
#define NEED_TO_UNDEFINE_GPA_FUNCTION_PREFIX  ///< Used as a flag to indicate whether or not the macro needs to be undefined later.
#endif

// GPA API Table.
GPA_FUNCTION_PREFIX(GpaGetFuncTable)

// Logging.
GPA_FUNCTION_PREFIX(GpaRegisterLoggingCallback)

// Init / Destroy GPA.
GPA_FUNCTION_PREFIX(GpaInitialize)
GPA_FUNCTION_PREFIX(GpaDestroy)

// Context Startup / Finish.
GPA_FUNCTION_PREFIX(GpaOpenContext)
GPA_FUNCTION_PREFIX(GpaCloseContext)

// Context Interrogation.
GPA_FUNCTION_PREFIX(GpaGetSupportedSampleTypes)
GPA_FUNCTION_PREFIX(GpaGetDeviceAndRevisionId)
GPA_FUNCTION_PREFIX(GpaGetDeviceName)

// Counter Interrogation.
GPA_FUNCTION_PREFIX(GpaGetNumCounters)
GPA_FUNCTION_PREFIX(GpaGetCounterName)
GPA_FUNCTION_PREFIX(GpaGetCounterIndex)
GPA_FUNCTION_PREFIX(GpaGetCounterGroup)
GPA_FUNCTION_PREFIX(GpaGetCounterDescription)
GPA_FUNCTION_PREFIX(GpaGetCounterDataType)
GPA_FUNCTION_PREFIX(GpaGetCounterUsageType)
GPA_FUNCTION_PREFIX(GpaGetCounterUuid)
GPA_FUNCTION_PREFIX(GpaGetCounterSampleType)
GPA_FUNCTION_PREFIX(GpaGetDataTypeAsStr)
GPA_FUNCTION_PREFIX(GpaGetUsageTypeAsStr)

// Session handling.
GPA_FUNCTION_PREFIX(GpaCreateSession)
GPA_FUNCTION_PREFIX(GpaDeleteSession)
GPA_FUNCTION_PREFIX(GpaBeginSession)
GPA_FUNCTION_PREFIX(GpaEndSession)

// Counter Scheduling.
GPA_FUNCTION_PREFIX(GpaEnableCounter)
GPA_FUNCTION_PREFIX(GpaDisableCounter)
GPA_FUNCTION_PREFIX(GpaEnableCounterByName)
GPA_FUNCTION_PREFIX(GpaDisableCounterByName)
GPA_FUNCTION_PREFIX(GpaEnableAllCounters)
GPA_FUNCTION_PREFIX(GpaDisableAllCounters)

// Query Counter Scheduling.
GPA_FUNCTION_PREFIX(GpaGetPassCount)
GPA_FUNCTION_PREFIX(GpaGetNumEnabledCounters)
GPA_FUNCTION_PREFIX(GpaGetEnabledIndex)
GPA_FUNCTION_PREFIX(GpaIsCounterEnabled)

// Sample Handling.
GPA_FUNCTION_PREFIX(GpaBeginCommandList)
GPA_FUNCTION_PREFIX(GpaEndCommandList)
GPA_FUNCTION_PREFIX(GpaBeginSample)
GPA_FUNCTION_PREFIX(GpaEndSample)
GPA_FUNCTION_PREFIX(GpaContinueSampleOnCommandList)
GPA_FUNCTION_PREFIX(GpaCopySecondarySamples)
GPA_FUNCTION_PREFIX(GpaGetSampleCount)

// Query Results.
GPA_FUNCTION_PREFIX(GpaIsPassComplete)
GPA_FUNCTION_PREFIX(GpaIsSessionComplete)
GPA_FUNCTION_PREFIX(GpaGetSampleResultSize)
GPA_FUNCTION_PREFIX(GpaGetSampleResult)

// Status / Error Query.
GPA_FUNCTION_PREFIX(GpaGetStatusAsStr)

// Sample Handling.
GPA_FUNCTION_PREFIX(GpaGetSampleId)

// GPA API Version.
GPA_FUNCTION_PREFIX(GpaGetVersion)

#ifdef NEED_TO_UNDEFINE_GPA_FUNCTION_PREFIX
#undef GPA_FUNCTION_PREFIX
#undef NEED_TO_UNDEFINE_GPA_FUNCTION_PREFIX
#endif
