#ifndef NVPERF_DEVICE_TARGET_H
#define NVPERF_DEVICE_TARGET_H

/*
 * Copyright 2014-2022  NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO USER:
 *
 * This source code is subject to NVIDIA ownership rights under U.S. and
 * international Copyright laws.
 *
 * This software and the information contained herein is PROPRIETARY and
 * CONFIDENTIAL to NVIDIA and is being provided under the terms and conditions
 * of a form of NVIDIA software license agreement.
 *
 * NVIDIA MAKES NO REPRESENTATION ABOUT THE SUITABILITY OF THIS SOURCE
 * CODE FOR ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR
 * IMPLIED WARRANTY OF ANY KIND.  NVIDIA DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOURCE CODE, INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL,
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS,  WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION,  ARISING OUT OF OR IN CONNECTION WITH THE USE
 * OR PERFORMANCE OF THIS SOURCE CODE.
 *
 * U.S. Government End Users.   This source code is a "commercial item" as
 * that term is defined at  48 C.F.R. 2.101 (OCT 1995), consisting  of
 * "commercial computer  software"  and "commercial computer software
 * documentation" as such terms are  used in 48 C.F.R. 12.212 (SEPT 1995)
 * and is provided to the U.S. Government only as a commercial end item.
 * Consistent with 48 C.F.R.12.212 and 48 C.F.R. 227.7202-1 through
 * 227.7202-4 (JUNE 1995), all U.S. Government End Users acquire the
 * source code with only those rights set forth herein.
 *
 * Any use of this source code in individual and commercial software must
 * include, in the user documentation and internal comments to the code,
 * the above Disclaimer and U.S. Government End Users Notice.
 */

#include <stddef.h>
#include <stdint.h>
#include "nvperf_common.h"
#include "nvperf_target.h"

#if defined(__GNUC__) && defined(NVPA_SHARED_LIB)
    #pragma GCC visibility push(default)
    #if !defined(NVPW_LOCAL)
        #define NVPW_LOCAL __attribute__ ((visibility ("hidden")))
    #endif
#else
    #if !defined(NVPW_LOCAL)
        #define NVPW_LOCAL
    #endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @file   nvperf_device_target.h
 */

/***************************************************************************//**
 *  @name   Periodic Sampling - GPU
 *  @{
 */

#ifndef NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_DEFINED
#define NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_DEFINED
    typedef enum NVPW_GPU_PeriodicSampler_TriggerSource
    {
        NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_INVALID = 0,
        /// The trigger is based off of system calls.
        NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_CPU_SYSCALL = 1,
        /// The trigger is based off of the SYSCLK interval, note SYS frequency by default is variable.
        NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_SYSCLK_INTERVAL = 2,
        /// The trigger is based off of a fixed frequency source.
        NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_TIME_INTERVAL = 3,
        /// GR pushbuffer trigger that can come from this or other processes.
        NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_ENGINE_TRIGGER = 4,
        NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE__COUNT
    } NVPW_GPU_PeriodicSampler_TriggerSource;
#endif //NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_DEFINED

#ifndef NVPW_GPU_PERIODIC_SAMPLER_RECORD_BUFFER_APPEND_MODE_DEFINED
#define NVPW_GPU_PERIODIC_SAMPLER_RECORD_BUFFER_APPEND_MODE_DEFINED
    typedef enum NVPW_GPU_PeriodicSampler_RecordBuffer_AppendMode
    {
        NVPW_GPU_PERIODIC_SAMPLER_RECORD_BUFFER_APPEND_MODE_KEEP_OLDEST = 0,
        NVPW_GPU_PERIODIC_SAMPLER_RECORD_BUFFER_APPEND_MODE_KEEP_LATEST = 1
    } NVPW_GPU_PeriodicSampler_RecordBuffer_AppendMode;
#endif //NVPW_GPU_PERIODIC_SAMPLER_RECORD_BUFFER_APPEND_MODE_DEFINED

    typedef struct NVPW_GPU_PeriodicSampler_CounterDataImageOptions
    {
        /// [in]
        size_t structSize;
        /// The CounterDataPrefix generated from e.g. NVPW_CounterDataBuilder_GetCounterDataPrefix().  Must be align(8).
        const uint8_t* pCounterDataPrefix;
        size_t counterDataPrefixSize;
        /// maximum number of samples
        uint32_t maxSamples;
        /// one of `NVPW_PeriodicSampler_CounterData_AppendMode`
        uint32_t appendMode;
    } NVPW_GPU_PeriodicSampler_CounterDataImageOptions;
#define NVPW_GPU_PeriodicSampler_CounterDataImageOptions_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_CounterDataImageOptions, appendMode)

    typedef struct NVPW_GPU_PeriodicSampler_IsGpuSupported_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [out]
        NVPA_Bool isSupported;
        /// [out]
        NVPW_GpuArchitectureSupportLevel gpuArchitectureSupportLevel;
        /// [out]
        NVPW_SliSupportLevel sliSupportLevel;
        /// [out]
        NVPW_VGpuSupportLevel vGpuSupportLevel;
        /// [out]
        NVPW_ConfidentialComputeSupportLevel confidentialComputeSupportLevel;
        /// [out]
        NVPW_CmpSupportLevel cmpSupportLevel;
        /// [out]
        NVPW_WslSupportLevel wslSupportLevel;
    } NVPW_GPU_PeriodicSampler_IsGpuSupported_Params;
#define NVPW_GPU_PeriodicSampler_IsGpuSupported_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_IsGpuSupported_Params, wslSupportLevel)

    /// LoadDriver must be called prior to this API.
    NVPA_Status NVPW_GPU_PeriodicSampler_IsGpuSupported(NVPW_GPU_PeriodicSampler_IsGpuSupported_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [inout] `pTriggerSources` is in, `*pTriggerSources` is out, each element is one of
        /// `NVPW_GPU_PeriodicSampler_TriggerSource
        uint32_t* pTriggerSources;
        /// [inout] if `pTriggerSources` is NULL, number of supported trigger sources will be returned; otherwise it
        /// should be set to the number of elements allocated for `pTriggerSources`, and on return, it will be
        /// overwritten by number of elements copied to `pTriggerSources`
        size_t numTriggerSources;
    } NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Params;
#define NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Params, numTriggerSources)

    /// LoadDriver must be called prior to this API.
    NVPA_Status NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources(NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [in] Typically created by e.g. NVPW_RawMetricsConfig_GetConfigImage(), must be align(8). If the input config
        /// has multiple passes, the maximum size of each pass will be returned. Use 'NULL' to calculate based on the
        /// total number of counter collection units on the system.
        const uint8_t* pConfig;
        /// [in]
        size_t configSize;
        /// [in] max number of undecoded samples to keep
        size_t maxNumUndecodedSamples;
        /// [out]
        size_t recordBufferSize;
    } NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Params;
#define NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Params, recordBufferSize)

    /// Calculate record buffer size based on a real device. LoadDriver must be called prior to this API. The returned
    /// size will be aligned up to meet OS/HW requirements.
    NVPA_Status NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize(NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_BeginSession_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [in] maximum number of undecoded sampling ranges there can be, where a sampling range is formed by one pair
        /// of `NVPW_GPU_PeriodicSampler_StartSampling` & `NVPW_GPU_PeriodicSampler_StopSampling`. Must be 1.
        size_t maxNumUndecodedSamplingRanges;
        /// [in] an array of trigger sources to use during the session, where each element is one of
        /// `NVPW_GPU_PeriodicSampler_TriggerSource`. Some combinations can be invalid.
        const uint32_t* pTriggerSources;
        /// [in]
        size_t numTriggerSources;
        /// [in] if trigger sources include `NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_SYSCLK_INTERVAL`, then it
        /// should be the number of SYS CLKs; or if trigger sources include
        /// `NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_TIME_INTERVAL`, then it should be the number of nanoseconds;
        /// otherwise it's not used.
        uint64_t samplingInterval;
        /// [in] output of `NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize`. If multiple configs will be used in a
        /// session, use their max size here. This value may be clamped internally to meet HW & profiling requirements,
        /// the actual allocated size can be queried via `NVPW_GPU_PeriodicSampler_GetRecordBufferStatus`
        size_t recordBufferSize;
    } NVPW_GPU_PeriodicSampler_BeginSession_Params;
#define NVPW_GPU_PeriodicSampler_BeginSession_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_BeginSession_Params, recordBufferSize)

    /// This API is deprecated, please use `NVPW_GPU_PeriodicSampler_BeginSession_V2` instead.
    NVPA_Status NVPW_GPU_PeriodicSampler_BeginSession(NVPW_GPU_PeriodicSampler_BeginSession_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_BeginSession_V2_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [in] maximum number of undecoded sampling ranges there can be, where a sampling range is formed by one pair
        /// of `NVPW_GPU_PeriodicSampler_StartSampling` & `NVPW_GPU_PeriodicSampler_StopSampling`. Must be 1.
        size_t maxNumUndecodedSamplingRanges;
        /// [in] an array of trigger sources to use during the session, where each element is one of
        /// `NVPW_GPU_PeriodicSampler_TriggerSource`. Some combinations can be invalid.
        const uint32_t* pTriggerSources;
        /// [in]
        size_t numTriggerSources;
        /// [in] if trigger sources include `NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_SYSCLK_INTERVAL`, then it
        /// should be the number of SYS CLKs; or if trigger sources include
        /// `NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_TIME_INTERVAL`, then it should be the number of nanoseconds;
        /// otherwise it's not used.
        uint64_t samplingInterval;
        /// [in] output of `NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize`. If multiple configs will be used in a
        /// session, use their max size here. This value may be clamped internally to meet HW & profiling requirements,
        /// the actual allocated size can be queried via `NVPW_GPU_PeriodicSampler_GetRecordBufferStatus`
        size_t recordBufferSize;
        /// [in] one of `NVPW_GPU_PeriodicSampler_RecordBuffer_AppendMode`
        uint32_t recordBufferAppendMode;
    } NVPW_GPU_PeriodicSampler_BeginSession_V2_Params;
#define NVPW_GPU_PeriodicSampler_BeginSession_V2_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_BeginSession_V2_Params, recordBufferAppendMode)

    /// LoadDriver must be called prior to this API.
    NVPA_Status NVPW_GPU_PeriodicSampler_BeginSession_V2(NVPW_GPU_PeriodicSampler_BeginSession_V2_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_EndSession_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
    } NVPW_GPU_PeriodicSampler_EndSession_Params;
#define NVPW_GPU_PeriodicSampler_EndSession_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_EndSession_Params, deviceIndex)

    NVPA_Status NVPW_GPU_PeriodicSampler_EndSession(NVPW_GPU_PeriodicSampler_EndSession_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_GetCounterAvailability_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [in/out] If `pCounterAvailabilityImage` is NULL, then the required size is returned in
        /// `counterAvailabilityImageSize`, otherwise `counterAvailabilityImageSize` should be set to the size of
        /// `pCounterAvailabilityImage`, and on return it would be overwritten with number of actual bytes copied
        size_t counterAvailabilityImageSize;
        /// [in] buffer receiving counter availability image, may be NULL
        uint8_t* pCounterAvailabilityImage;
    } NVPW_GPU_PeriodicSampler_GetCounterAvailability_Params;
#define NVPW_GPU_PeriodicSampler_GetCounterAvailability_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_GetCounterAvailability_Params, pCounterAvailabilityImage)

    /// LoadDriver must be called prior to this API. This API may fail, if any profiling or sampling session is active
    /// on the specified device
    NVPA_Status NVPW_GPU_PeriodicSampler_GetCounterAvailability(NVPW_GPU_PeriodicSampler_GetCounterAvailability_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_SetConfig_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [in] Config created by e.g. NVPW_RawMetricsConfig_GetConfigImage().  Must be align(8).
        const uint8_t* pConfig;
        /// [in]
        size_t configSize;
        /// [in]
        size_t passIndex;
    } NVPW_GPU_PeriodicSampler_SetConfig_Params;
#define NVPW_GPU_PeriodicSampler_SetConfig_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_SetConfig_Params, passIndex)

    /// This API must be called inside a session.
    NVPA_Status NVPW_GPU_PeriodicSampler_SetConfig(NVPW_GPU_PeriodicSampler_SetConfig_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_StartSampling_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
    } NVPW_GPU_PeriodicSampler_StartSampling_Params;
#define NVPW_GPU_PeriodicSampler_StartSampling_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_StartSampling_Params, deviceIndex)

    /// This API must be called inside a session.
    NVPA_Status NVPW_GPU_PeriodicSampler_StartSampling(NVPW_GPU_PeriodicSampler_StartSampling_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_StopSampling_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
    } NVPW_GPU_PeriodicSampler_StopSampling_Params;
#define NVPW_GPU_PeriodicSampler_StopSampling_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_StopSampling_Params, deviceIndex)

    NVPA_Status NVPW_GPU_PeriodicSampler_StopSampling(NVPW_GPU_PeriodicSampler_StopSampling_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_CpuTrigger_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in] trigger through system call
        size_t deviceIndex;
    } NVPW_GPU_PeriodicSampler_CpuTrigger_Params;
#define NVPW_GPU_PeriodicSampler_CpuTrigger_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_CpuTrigger_Params, deviceIndex)

    /// This API must be called inside a session.
    NVPA_Status NVPW_GPU_PeriodicSampler_CpuTrigger(NVPW_GPU_PeriodicSampler_CpuTrigger_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [in]
        const NVPW_GPU_PeriodicSampler_CounterDataImageOptions* pOptions;
        /// [out]
        size_t counterDataImageSize;
    } NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Params;
#define NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Params, counterDataImageSize)

    /// LoadDriver must be called prior to this API.
    NVPA_Status NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize(NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [in]
        const NVPW_GPU_PeriodicSampler_CounterDataImageOptions* pOptions;
        /// [in] the buffer to be written
        uint8_t* pCounterDataImage;
        /// [in]
        size_t counterDataImageSize;
    } NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Params;
#define NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Params, counterDataImageSize)

    /// LoadDriver must be called prior to this API.
    NVPA_Status NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize(NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [out]
        size_t totalSize;
        /// [out]
        size_t usedSize;
        /// [out]
        NVPA_Bool overflow;
    } NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Params;
#define NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Params, overflow)

    /// This API must be called inside a session. Due to hardware limitation, `overflow` and `usedSize` may be
    /// temporarily out-of-sync during sampling.
    NVPA_Status NVPW_GPU_PeriodicSampler_GetRecordBufferStatus(NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_DecodeCounters_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [in]
        uint8_t* pCounterDataImage;
        /// [in]
        size_t counterDataImageSize;
        /// [in] number of sampling ranges to decode, where a sampling range is formed by one pair of
        /// `NVPW_GPU_PeriodicSampler_StartSampling` & `NVPW_GPU_PeriodicSampler_StopSampling`. Use '0' for decoding all
        /// available ranges.
        size_t numRangesToDecode;
        /// [out] number of sampling ranges fully decoded
        size_t numRangesDecoded;
        /// [out]
        NVPA_Bool recordBufferOverflow;
        /// [out] number of samples dropped due to CounterDataImage overflow
        size_t numSamplesDropped;
        /// [out] number of samples merged due to insufficient sample interval
        size_t numSamplesMerged;
    } NVPW_GPU_PeriodicSampler_DecodeCounters_Params;
#define NVPW_GPU_PeriodicSampler_DecodeCounters_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_DecodeCounters_Params, numSamplesMerged)

    /// This API is deprecated, please use `NVPW_GPU_PeriodicSampler_DecodeCounters_V2` instead.
    NVPA_Status NVPW_GPU_PeriodicSampler_DecodeCounters(NVPW_GPU_PeriodicSampler_DecodeCounters_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [in]
        uint8_t* pCounterDataImage;
        /// [in]
        size_t counterDataImageSize;
        /// [in] number of sampling ranges to decode, where a sampling range is formed by one pair of
        /// `NVPW_GPU_PeriodicSampler_StartSampling` & `NVPW_GPU_PeriodicSampler_StopSampling`. Must be 1.
        size_t numRangesToDecode;
        /// [in] in case the counter data buffer is full, stop decoding where it is as opposed to proceeding and
        /// dropping samples.
        NVPA_Bool doNotDropSamples;
        /// [out] number of sampling ranges fully decoded
        size_t numRangesDecoded;
        /// [out]
        NVPA_Bool recordBufferOverflow;
        /// [out] number of samples dropped due to CounterDataImage overflow
        size_t numSamplesDropped;
        /// [out] number of samples merged due to insufficient sample interval
        size_t numSamplesMerged;
    } NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Params;
#define NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Params, numSamplesMerged)

    NVPA_Status NVPW_GPU_PeriodicSampler_DecodeCounters_V2(NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [out]
        NVPA_Bool isSupported;
    } NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Params;
#define NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Params, isSupported)

    /// LoadDriver must be called prior to this API.
    NVPA_Status NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported(NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Params* pParams);

/**
 *  @}
 ******************************************************************************/
 


#ifdef __cplusplus
} // extern "C"
#endif

#if defined(__GNUC__) && defined(NVPA_SHARED_LIB)
    #pragma GCC visibility pop
#endif

#endif // NVPERF_DEVICE_TARGET_H
