//==============================================================================
// Copyright (c) 2012-2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  GPUPerfAPI Counter Generator function
//==============================================================================


#ifndef _GPA_COUNTER_GENERATOR_H_
#define _GPA_COUNTER_GENERATOR_H_

#include "GPAICounterAccessor.h"
#include "GPAICounterScheduler.h"
#include "GPUPerfAPITypes.h"

// Internal function. We don't want this exposed by the internal DLLs though, so it doesn't use GPALIB_DECL
/// Generates a counter accessor object that can be used to obtain the counters to expose
/// \param[in] desiredAPI The API to generate counters for
/// \param[in] vendorId The vendor id to generate counters for
/// \param[in] deviceId The device id to generate counters for
/// \param[in] revisionId The revision id to generate counters for
/// \param[inout] ppCounterAccessorOut Address of a GPA_ICounterAccessor pointer which will be set to the necessary counter accessor
/// \param[inout] ppCounterSchedulerOut Address of a GPA_ICounterScheduler pointer which will be set to the necessary counter scheduler
/// \return GPA_STATUS_ERROR_NULL_POINTER if ppCounterAccessorOut or ppCounterSchedulerOut is nullptr
/// \return GPA_STATUS_ERROR_NOT_FOUND if the desired API is not supported
/// \return GPA_STATUS_ERROR_NOT_ENABLED if the desired API is not allowing any counters to be exposed
/// \return GPA_STATUS_ERROR_HARDWARE_NOT_SUPPORTED if the desired generation is not supported
/// \return GPA_STATUS_OK if the desired API and generation are supported
GPA_Status GenerateCounters(GPA_API_Type desiredAPI, gpa_uint32 vendorId, gpa_uint32 deviceId, gpa_uint32 revisionId, GPA_ICounterAccessor** ppCounterAccessorOut, GPA_ICounterScheduler** ppCounterSchedulerOut);

#endif // _GPA_COUNTER_GENERATOR_H_
