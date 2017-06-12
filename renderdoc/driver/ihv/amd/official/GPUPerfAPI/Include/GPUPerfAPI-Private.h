//==============================================================================
// Copyright (c) 2009-2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Include this file rather than GPUPerfAPI.h for our internal usage.
//==============================================================================

#ifndef _GPUPERFAPI_PRIVATE_H_
#define _GPUPERFAPI_PRIVATE_H_
#include "GPUPerfAPIOS.h"
#include "GPUPerfAPI.h"
#include "GPUPerfAPITypes-Private.h"
#include "GPUPerfAPIFunctionTypes-Private.h"

// For internal use only

// *INDENT-OFF*
#ifdef AMDT_INTERNAL
/// \brief Register a debug callback function to receive debug log messages.
///
/// Only one debug callback function can be registered, so the implementation should be able
/// to handle the different types of messages. A parameter to the callback function will
/// indicate the message type being received.
/// \param loggingType Identifies the type of messages to receive callbacks for.
/// \param callbackFuncPtr Pointer to the callback function
/// \return GPA_STATUS_OK, unless the callbackFuncPtr is nullptr and the loggingType is not
/// GPA_LOGGING_NONE, in which case GPA_STATUS_ERROR_NULL_POINTER is returned.
GPALIB_DECL GPA_Status GPA_RegisterLoggingDebugCallback(GPA_Log_Debug_Type loggingType, GPA_LoggingDebugCallbackPtrType callbackFuncPtr);

/// \brief Internal function. Pass draw call counts to GPA for internal purposes.
/// \param iCounts[in] the draw counts for the current frame
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_InternalSetDrawCallCounts(const int iCounts);

#endif // AMDT_INTERNAL
// *INDENT-ON*

/// \brief Internal function. Unsupported and may be removed from the API at any time.
///
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_InternalProfileStart();

/// \brief Internal function. Unsupported and may be removed from the API at any time.
///
/// \param pFilename the name of the file to write profile results
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_InternalProfileStop(const char* pFilename);

#endif // _GPUPERFAPI_PRIVATE_H_
