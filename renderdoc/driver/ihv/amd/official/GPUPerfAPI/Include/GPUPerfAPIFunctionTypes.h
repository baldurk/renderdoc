//==============================================================================
// Copyright (c) 2010-2017 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  This file defines function types to make it easier to dynamically load
///         different GPUPerfAPI DLLs into an application that supports multiple APIs.
///         Applications which statically link to GPUPerfAPI do not need to include
///         this file.
//==============================================================================

#ifndef _GPUPERFAPI_FUNCTION_TYPES_H_
#define _GPUPERFAPI_FUNCTION_TYPES_H_

#include "GPUPerfAPITypes.h"

typedef void(*GPA_LoggingCallbackPtrType)(GPA_Logging_Type messageType, const char* pMessage); ///< Typedef for a function pointer for a logging callback function

typedef GPA_Status(*GPA_RegisterLoggingCallbackPtrType)(GPA_Logging_Type loggingType, GPA_LoggingCallbackPtrType pCallbackFuncPtr); ///< Typedef for a function pointer for GPA_RegisterLoggingCallback

// Startup / exit
typedef GPA_Status(*GPA_InitializePtrType)();  ///< Typedef for a function pointer for GPA_Initialize
typedef GPA_Status(*GPA_DestroyPtrType)();  ///< Typedef for a function pointer for GPA_Destroy

// Context
typedef GPA_Status(*GPA_OpenContextPtrType)(void* pContext, GPA_OpenContextFlags flags);  ///< Typedef for a function pointer for GPA_OpenContext
typedef GPA_Status(*GPA_CloseContextPtrType)();  ///< Typedef for a function pointer for GPA_CloseContext
typedef GPA_Status(*GPA_SelectContextPtrType)(void* pCcontext);  ///< Typedef for a function pointer for GPA_SelectContext

// Counter Interrogation
typedef GPA_Status(*GPA_GetNumCountersPtrType)(gpa_uint32* pCount);  ///< Typedef for a function pointer for GPA_GetNumCounters
typedef GPA_Status(*GPA_GetCounterNamePtrType)(gpa_uint32 index, const char** ppName);  ///< Typedef for a function pointer for GPA_GetCounterName
typedef GPA_Status(*GPA_GetCounterCategoryPtrType)(gpa_uint32 index, const char** ppCategory); ///< Typedef for a function pointer for GPA_GetCounterCategory
typedef GPA_Status(*GPA_GetCounterDescriptionPtrType)(gpa_uint32 index, const char** ppDescription);  ///< Typedef for a function pointer for GPA_GetCounterDescription
typedef GPA_Status(*GPA_GetCounterDataTypePtrType)(gpa_uint32 index, GPA_Type* pCounterDataType);  ///< Typedef for a function pointer for GPA_GetCounterDataType
typedef GPA_Status(*GPA_GetCounterUsageTypePtrType)(gpa_uint32 index, GPA_Usage_Type* pCounterUsageType);  ///< Typedef for a function pointer for GPA_GetCounterUsageType
typedef GPA_Status(*GPA_GetDataTypeAsStrPtrType)(GPA_Type counterDataType, const char** ppTypeStr);  ///< Typedef for a function pointer for GPA_GetDataTypeAsStr
typedef GPA_Status(*GPA_GetUsageTypeAsStrPtrType)(GPA_Usage_Type counterUsageType, const char** ppTypeStr);  ///< Typedef for a function pointer for GPA_GetUsageTypeAsStr
typedef const char* (*GPA_GetStatusAsStrPtrType)(GPA_Status status);  ///< Typedef for a function pointer for GPA_GetStatusAsStr

typedef GPA_Status(*GPA_EnableCounterPtrType)(gpa_uint32 index);  ///< Typedef for a function pointer for GPA_EnableCounter
typedef GPA_Status(*GPA_DisableCounterPtrType)(gpa_uint32 index);  ///< Typedef for a function pointer for GPA_DisableCounter
typedef GPA_Status(*GPA_GetEnabledCountPtrType)(gpa_uint32* pCount);  ///< Typedef for a function pointer for GPA_GetEnabledCount
typedef GPA_Status(*GPA_GetEnabledIndexPtrType)(gpa_uint32 enabledNumber, gpa_uint32* pEnabledCounterIndex);  ///< Typedef for a function pointer for GPA_GetEnabledIndex

typedef GPA_Status(*GPA_IsCounterEnabledPtrType)(gpa_uint32 counterIndex);  ///< Typedef for a function pointer for GPA_IsCounterEnabled

typedef GPA_Status(*GPA_EnableCounterStrPtrType)(const char* pCounter);  ///< Typedef for a function pointer for GPA_EnableCounterStr
typedef GPA_Status(*GPA_DisableCounterStrPtrType)(const char* pCounter);  ///< Typedef for a function pointer for GPA_DisableCounterStr

typedef GPA_Status(*GPA_EnableAllCountersPtrType)();  ///< Typedef for a function pointer for GPA_EnableAllCounters
typedef GPA_Status(*GPA_DisableAllCountersPtrType)();  ///< Typedef for a function pointer for GPA_DisableAllCounters
typedef GPA_Status(*GPA_GetCounterIndexPtrType)(const char* pCounter, gpa_uint32* pIndex);  ///< Typedef for a function pointer for GPA_GetCounterIndex

typedef GPA_Status(*GPA_GetPassCountPtrType)(gpa_uint32* pNumPasses);  ///< Typedef for a function pointer for GPA_GetPassCount

typedef GPA_Status(*GPA_BeginSessionPtrType)(gpa_uint32* pSessionID);  ///< Typedef for a function pointer for GPA_BeginSession
typedef GPA_Status(*GPA_EndSessionPtrType)();  ///< Typedef for a function pointer for GPA_EndSession

typedef GPA_Status(*GPA_BeginPassPtrType)();  ///< Typedef for a function pointer for GPA_BeginPass
typedef GPA_Status(*GPA_EndPassPtrType)();  ///< Typedef for a function pointer for GPA_EndPass

typedef GPA_Status(*GPA_BeginSampleListPtrType)(void* pSampleList);  ///< Typedef for a function pointer for GPA_BeginSampleList
typedef GPA_Status(*GPA_EndSampleListPtrType)(void* pSampleList);  ///< Typedef for a function pointer for GPA_EndSampleList
typedef GPA_Status(*GPA_BeginSampleInSampleListPtrType)(gpa_uint32 sampleID, void* pSampleList);  ///< Typedef for a function pointer for GPA_BeginSampleInSampleList
typedef GPA_Status(*GPA_EndSampleInSampleListPtrType)(void* pSampleList);  ///< Typedef for a function pointer for GPA_EndSampleInSampleList
typedef GPA_Status(*GPA_BeginSamplePtrType)(gpa_uint32 sampleID);  ///< Typedef for a function pointer for GPA_BeginSample
typedef GPA_Status(*GPA_EndSamplePtrType)();  ///< Typedef for a function pointer for GPA_EndSample

typedef GPA_Status(*GPA_GetSampleCountPtrType)(gpa_uint32 sessionID, gpa_uint32* pSamples);  ///< Typedef for a function pointer for GPA_GetSampleCount

typedef GPA_Status(*GPA_IsSampleReadyPtrType)(gpa_uint8* pReadyResult, gpa_uint32 sessionID, gpa_uint32 sampleID);  ///< Typedef for a function pointer for GPA_IsSampleReady
typedef GPA_Status(*GPA_IsSessionReadyPtrType)(gpa_uint8* pReadyResult, gpa_uint32 sessionID);  ///< Typedef for a function pointer for GPA_IsSessionReady
typedef GPA_Status(*GPA_GetSampleUInt64PtrType)(gpa_uint32 sessionID, gpa_uint32 sampleID, gpa_uint32 counterID, gpa_uint64* pResult);  ///< Typedef for a function pointer for GPA_GetSampleUInt64
typedef GPA_Status(*GPA_GetSampleUInt32PtrType)(gpa_uint32 sessionID, gpa_uint32 sampleID, gpa_uint32 counterIndex, gpa_uint32* pResult);  ///< Typedef for a function pointer for GPA_GetSampleUInt32
typedef GPA_Status(*GPA_GetSampleFloat32PtrType)(gpa_uint32 sessionID, gpa_uint32 sampleID, gpa_uint32 counterIndex, gpa_float32* pResult);  ///< Typedef for a function pointer for GPA_GetSampleFloat32
typedef GPA_Status(*GPA_GetSampleFloat64PtrType)(gpa_uint32 sessionID, gpa_uint32 sampleID, gpa_uint32 counterIndex, gpa_float64* pResult);  ///< Typedef for a function pointer for GPA_GetSampleFloat64

typedef GPA_Status(*GPA_GetDeviceIDPtrType)(gpa_uint32* pDeviceID);  ///< Typedef for a function pointer for GPA_GetDeviceID
typedef GPA_Status(*GPA_GetDeviceDescPtrType)(const char** ppDesc);  ///< Typedef for a function pointer for GPA_GetDeviceDesc

typedef GPA_Status(*GPA_InternalSetDrawCallCountsPtrType)(const int iCounts);  /// Typedef for a function pointer for a function to set the number of draw calls in a frame

typedef GPA_Status(*GPA_GetFuncTablePtrType)(void** funcTable); /// Typedef for a function pointer for GPA function GPA_GetFuncTablePtrType function

#endif // _GPUPERFAPI_FUNCTION_TYPES_H_
