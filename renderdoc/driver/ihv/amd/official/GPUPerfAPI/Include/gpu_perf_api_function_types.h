//==============================================================================
// Copyright (c) 2010-2021 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  This file defines function types to make it easier to dynamically load
///         different GPUPerfAPI DLLs into an application that supports multiple APIs.
///         Applications which statically link to GPUPerfAPI do not need to include
///         this file.
//==============================================================================

#ifndef GPU_PERFORMANCE_API_GPU_PERF_API_FUNCTION_TYPES_H_
#define GPU_PERFORMANCE_API_GPU_PERF_API_FUNCTION_TYPES_H_

#include "gpu_perf_api_types.h"

/// Typedef for a function pointer for GpaGetVersion.
typedef GpaStatus (*GpaGetVersionPtrType)(GpaUInt32*, GpaUInt32*, GpaUInt32*, GpaUInt32*);

/// Typedef for a function pointer for GpaGetFuncTable.
typedef GpaStatus (*GpaGetFuncTablePtrType)(void*);

/// Typedef for a function pointer for a logging callback function.
typedef void (*GpaLoggingCallbackPtrType)(GpaLoggingType, const char*);

/// Typedef for a function pointer for GpaRegisterLoggingCallback.
typedef GpaStatus (*GpaRegisterLoggingCallbackPtrType)(GpaLoggingType, GpaLoggingCallbackPtrType);

/// Typedef for a function pointer for GpaInitialize.
typedef GpaStatus (*GpaInitializePtrType)(GpaInitializeFlags);

/// Typedef for a function pointer for GpaDestroy.
typedef GpaStatus (*GpaDestroyPtrType)();

/// Typedef for a function pointer for GpaOpenContext.
typedef GpaStatus (*GpaOpenContextPtrType)(void*, GpaOpenContextFlags, GpaContextId*);

/// Typedef for a function pointer for GpaCloseContext.
typedef GpaStatus (*GpaCloseContextPtrType)(GpaContextId);

/// Typedef for a function pointer for GpaGetSupportedSampleTypes.
typedef GpaStatus (*GpaGetSupportedSampleTypesPtrType)(GpaContextId, GpaContextSampleTypeFlags*);

/// Typedef for a function pointer for GpaGetDeviceAndRevisionId.
typedef GpaStatus (*GpaGetDeviceAndRevisionIdPtrType)(GpaContextId, GpaUInt32*, GpaUInt32*);

/// Typedef for a function pointer for GpaGetDeviceName.
typedef GpaStatus (*GpaGetDeviceNamePtrType)(GpaContextId, const char**);

/// Typedef for a function pointer for GpaGetNumCounters.
typedef GpaStatus (*GpaGetNumCountersPtrType)(GpaContextId, GpaUInt32*);

/// Typedef for a function pointer for GpaGetCounterName.
typedef GpaStatus (*GpaGetCounterNamePtrType)(GpaContextId, GpaUInt32, const char**);

/// Typedef for a function pointer for GpaGetCounterIndex.
typedef GpaStatus (*GpaGetCounterIndexPtrType)(GpaContextId, const char*, GpaUInt32*);

/// Typedef for a function pointer for GpaGetCounterGroup.
typedef GpaStatus (*GpaGetCounterGroupPtrType)(GpaContextId, GpaUInt32, const char**);

/// Typedef for a function pointer for GpaGetCounterDescription.
typedef GpaStatus (*GpaGetCounterDescriptionPtrType)(GpaContextId, GpaUInt32, const char**);

/// Typedef for a function pointer for GpaGetCounterDataType.
typedef GpaStatus (*GpaGetCounterDataTypePtrType)(GpaContextId, GpaUInt32, GpaDataType*);

/// Typedef for a function pointer for GpaGetCounterUsageType.
typedef GpaStatus (*GpaGetCounterUsageTypePtrType)(GpaContextId, GpaUInt32, GpaUsageType*);

/// Typedef for a function pointer for GpaGetCounterUuid.
typedef GpaStatus (*GpaGetCounterUuidPtrType)(GpaContextId, GpaUInt32, GpaUuid*);

/// Typedef for a function pointer for GpaGetCounterSampleType.
typedef GpaStatus (*GpaGetCounterSampleTypePtrType)(GpaContextId, GpaUInt32, GpaCounterSampleType*);

/// Typedef for a function pointer for GpaGetDataTypeAsStr.
typedef GpaStatus (*GpaGetDataTypeAsStrPtrType)(GpaDataType, const char**);

/// Typedef for a function pointer for GpaGetUsageTypeAsStr.
typedef GpaStatus (*GpaGetUsageTypeAsStrPtrType)(GpaUsageType, const char**);

/// Typedef for a function pointer for GpaCreateSession.
typedef GpaStatus (*GpaCreateSessionPtrType)(GpaContextId, GpaSessionSampleType, GpaSessionId*);

/// Typedef for a function pointer for GpaDeleteSession.
typedef GpaStatus (*GpaDeleteSessionPtrType)(GpaSessionId);

/// Typedef for a function pointer for GpaBeginSession.
typedef GpaStatus (*GpaBeginSessionPtrType)(GpaSessionId);

/// Typedef for a function pointer for GpaEndSession.
typedef GpaStatus (*GpaEndSessionPtrType)(GpaSessionId);

/// Typedef for a function pointer for GpaEnableCounter.
typedef GpaStatus (*GpaEnableCounterPtrType)(GpaSessionId, GpaUInt32);

/// Typedef for a function pointer for GpaDisableCounter.
typedef GpaStatus (*GpaDisableCounterPtrType)(GpaSessionId, GpaUInt32);

/// Typedef for a function pointer for GpaEnableCounterByName.
typedef GpaStatus (*GpaEnableCounterByNamePtrType)(GpaSessionId, const char*);

/// Typedef for a function pointer for GpaDisableCounterByName.
typedef GpaStatus (*GpaDisableCounterByNamePtrType)(GpaSessionId, const char*);

/// Typedef for a function pointer for GpaEnableAllCounters.
typedef GpaStatus (*GpaEnableAllCountersPtrType)(GpaSessionId);

/// Typedef for a function pointer for GpaDisableAllCounters.
typedef GpaStatus (*GpaDisableAllCountersPtrType)(GpaSessionId);

/// Typedef for a function pointer for GpaGetPassCount.
typedef GpaStatus (*GpaGetPassCountPtrType)(GpaSessionId, GpaUInt32*);

/// Typedef for a function pointer for GetNumEnabledCounters.
typedef GpaStatus (*GpaGetNumEnabledCountersPtrType)(GpaSessionId, GpaUInt32*);

/// Typedef for a function pointer for GpaGetEnabledIndex.
typedef GpaStatus (*GpaGetEnabledIndexPtrType)(GpaSessionId, GpaUInt32, GpaUInt32*);

/// Typedef for a function pointer for GpaIsCounterEnabled.
typedef GpaStatus (*GpaIsCounterEnabledPtrType)(GpaSessionId, GpaUInt32);

/// Typedef for a function pointer for GpaBeginCommandList.
typedef GpaStatus (*GpaBeginCommandListPtrType)(GpaSessionId, GpaUInt32, void*, GpaCommandListType, GpaCommandListId*);

/// Typedef for a function pointer for GpaEndCommandList.
typedef GpaStatus (*GpaEndCommandListPtrType)(GpaCommandListId);

/// Typedef for a function pointer for GpaBeginSample.
typedef GpaStatus (*GpaBeginSamplePtrType)(GpaUInt32, GpaCommandListId);

/// Typedef for a function pointer for GpaEndSample.
typedef GpaStatus (*GpaEndSamplePtrType)(GpaCommandListId);

/// Typedef for a function pointer for GpaContinueSampleOnCommandList.
typedef GpaStatus (*GpaContinueSampleOnCommandListPtrType)(GpaUInt32,
                                                           GpaCommandListId);

/// Typedef for a function pointer for GpaCopySecondarySamples.
typedef GpaStatus (*GpaCopySecondarySamplesPtrType)(GpaCommandListId,
                                                    GpaCommandListId,
                                                    GpaUInt32,
                                                    GpaUInt32*);

/// Typedef for a function pointer for GpaGetSampleCount.
typedef GpaStatus (*GpaGetSampleCountPtrType)(GpaSessionId, GpaUInt32*);

/// Typedef for a function pointer for GpaGetSampleId.
typedef GpaStatus (*GpaGetSampleIdPtrType)(GpaSessionId, GpaUInt32, GpaUInt32*);

/// Typedef for a function pointer for GpaIsSessionComplete.
typedef GpaStatus (*GpaIsSessionCompletePtrType)(GpaSessionId);

/// Typedef for a function pointer for GpaIsPassComplete.
typedef GpaStatus (*GpaIsPassCompletePtrType)(GpaSessionId, GpaUInt32);

/// Typedef for a function pointer for GpaGetSampleResultSize.
typedef GpaStatus (*GpaGetSampleResultSizePtrType)(GpaSessionId, GpaUInt32, size_t*);

/// Typedef for a function pointer for GpaGetSampleResult.
typedef GpaStatus (*GpaGetSampleResultPtrType)(GpaSessionId, GpaUInt32, size_t, void*);

/// Typedef for a function pointer for GpaGetStatusAsStr.
typedef const char* (*GpaGetStatusAsStrPtrType)(GpaStatus);

#endif  // GPU_PERFORMANCE_API_GPU_PERF_API_FUNCTION_TYPES_H_
