//==============================================================================
// Copyright (c) 2010-2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  This file is for internal use to define the function types.
//==============================================================================

#ifndef _GPUPERFAPI_FUNCTION_TYPES_INTERNAL_H_
#define _GPUPERFAPI_FUNCTION_TYPES_INTERNAL_H_

#include "GPUPerfAPIFunctionTypes.h"

/// Typedef for a function pointer for a function to enable counters from a file
typedef GPA_Status(*GPA_EnableCountersFromFilePtrType)(const char* pFile, gpa_uint32* pCountersRead);

/// Typedef for a function pointer for a debug logging callback
typedef void(*GPA_LoggingDebugCallbackPtrType)(GPA_Log_Debug_Type messageType, const char* pMessage);

/// Typedef for a function pointer for a function to register a debug logging callback
typedef GPA_Status(*GPA_RegisterLoggingDebugCallbackPtrType)(GPA_Log_Debug_Type loggingType, GPA_LoggingDebugCallbackPtrType pCallbackFuncPtr);


/// Typedef for a function pointer for a function to start profiling a GPA function
typedef GPA_Status(*GPA_InternalProfileStartPtrType)();

/// Typedef for a function pointer for a function to stop profiling a GPA function
typedef GPA_Status(*GPA_InternalProfileStopPtrType)(const char* pFilename);

/// Typedef for a function pointer for a function to set the number of draw calls in a frame
/// For internal purposes only -- not needed for normal operation of GPUPerfAPI
typedef GPA_Status(*GPA_InternalSetDrawCallCountsPtrType)(const int iCounts);

#endif // _GPUPERFAPI_FUNCTION_TYPES_INTERNAL_H_
