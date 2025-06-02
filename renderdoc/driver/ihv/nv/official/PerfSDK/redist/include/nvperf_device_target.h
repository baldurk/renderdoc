#ifndef NVPERF_DEVICE_TARGET_H
#define NVPERF_DEVICE_TARGET_H

/*
 * Copyright 2014-2025 NVIDIA Corporation.  All rights reserved.
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

// By generating periodic triggers, the API enables the sampling of programmed counters on the GPU. Counters are streamed as `records` from the GPU into a `record buffer`.
// This buffer is designed as a ring buffer, with the GPU serving as the producer, writing records to the buffer, while the API acts as the consumer, reading the records and decoding their counter values.
// 
// Users can enable or disable periodic triggers and records streaming by invoking the StartSampling() and StopSampling() APIs, respectively.
// The record buffer uses read and write offsets to manage the flow of data. They are offsets relative to the starting point of the record buffer.
// The write offset, managed by the GPU, indicates the next position where a record will be written. The read offset indicates the position that the API has read up to.
// 
// The record buffer operates in two modes: keep latest and keep oldest:
// * Keep oldest mode: In this mode, the API needs to inform the hardware of its read position to prevent data overwriting.
//   Buffer overflow occurs when the write offset equals to the read offset, indicating that the GPU is too far ahead of the software.
//   If an overflow occurs, the GPU sets the "overflow" sticky state and refuses to write more records, even when the periodic triggers and record streaming are still enabled.
//   To clear this sticky state, clients must call StopSampling() and then SetConfig() with the same or a new configuration to clear the overflow state and reprogram the GPU profiling system.
// * Keep latest mode: In contrast, keep latest mode allows the GPU to continue writing records, even if overwriting any unread data, ensuring the most recent records are available for analysis.
//   Overflow is not applicable in this mode, but the client is responsible for consuming the data in time to avoid overwriting if the use case expects no data loss.
//   For clients who prioritize obtaining the latest data and can tolerate losing earlier data, it is acceptable not to consume the data in a timely manner.
//   The API provides a function, IsRecordBufferKeepLatestModeSupported(), to check if the underlying GPU supports the keep latest mode.

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
        /// `NVPW_GPU_PeriodicSampler_TriggerSource`.
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
        /// [in] This parameter has been deprecated and is no longer utilized by the library.Currently, it needs to be
        /// kept and must be 1 to support legacy DecodeCounters().
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
        /// the actual allocated size can be queried via `NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_V2`
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
        /// the actual allocated size can be queried via `NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_V2`
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
    /// temporarily out-of-sync during sampling. This API is deprecated, please use
    /// `NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_V2` instead.
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

    /// CPU-side write offset will be queried through syscall in the beginning of this API routine with large CPU time
    /// overhead. Both GPU-side and CPU-side read offset of the record buffer will be updated at the end of this API
    /// routine, GPU-side updating is with large CPU time overhead. The first sample in a sampling range, by definition,
    /// represents the accumulated counter values between SetConfig (or the last trigger from the previous sampling
    /// range) and the first trigger in the current sampling range which may result in larger counter values. Since it
    /// may not have a prior trigger, the startTimestamp can be 0. The recommended approach is to discard the first
    /// sample during post-processing. Similarly, the last sample might contain only trailing overflow prevention
    /// records emitted after the last trigger. As it is not associated with a trigger, it has an endTimestamp of 0.
    /// This sample can also be discarded through post-processing.This API is deprecated, please use
    /// `NVPW_GPU_PeriodicSampler_DecodeCounters_V3` instead.
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

    /// CPU-side write offset will be queried through syscall in the beginning of this API routine with large CPU time
    /// overhead. Both GPU-side and CPU-side read offset of the record buffer will be updated at the end of this API
    /// routine, GPU-side updating is with large CPU time overhead. The first sample in a sampling range, by definition,
    /// represents the accumulated counter values between SetConfig (or the last trigger from the previous sampling
    /// range) and the first trigger in the current sampling range which may result in larger counter values. Since it
    /// may not have a prior trigger, the startTimestamp can be 0. The recommended approach is to discard the first
    /// sample during post-processing. Similarly, the last sample might contain only trailing overflow prevention
    /// records emitted after the last trigger. As it is not associated with a trigger, it has an endTimestamp of 0.
    /// This sample can also be discarded through post-processing. This API is deprecated, please use
    /// `NVPW_GPU_PeriodicSampler_DecodeCounters_V3` instead.
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

    typedef struct NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_V2_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [in]
        NVPA_Bool queryNumUnreadBytes;
        /// [in]
        NVPA_Bool queryOverflow;
        /// [in]
        NVPA_Bool queryWriteOffset;
        /// [in]
        NVPA_Bool queryReadOffset;
        /// [out]
        size_t totalSize;
        /// [out] only valid when `queryNumUnreadBytes` == true, indicating how many bytes has not been read in the
        /// record buffer.
        size_t numUnreadBytes;
        /// [out] only valid when `queryOverflow` == true. It will be true only in the keep-oldest mode.
        NVPA_Bool overflow;
        /// [out] only valid when `queryWriteOffset` == true, points to the end of the record stream.
        size_t writeOffset;
        /// [out] only valid when `queryReadOffset` == true, points to the first sample which has not been read. After
        /// decoding, it will be increased with numBytesConsumed.
        size_t readOffset;
    } NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_V2_Params;
#define NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_V2_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_V2_Params, readOffset)

    /// `queryNumUnreadBytes`, `queryOverflow`, `queryWriteOffset` operations involve reading states from GPU, thus they
    /// can be slow. Only enable them if needed.
    NVPA_Status NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_V2(NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_V2_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_GetGpuTimestamp_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [out]
        uint64_t timestamp;
    } NVPW_GPU_PeriodicSampler_GetGpuTimestamp_Params;
#define NVPW_GPU_PeriodicSampler_GetGpuTimestamp_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_GetGpuTimestamp_Params, timestamp)

    NVPA_Status NVPW_GPU_PeriodicSampler_GetGpuTimestamp(NVPW_GPU_PeriodicSampler_GetGpuTimestamp_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_AcknowledgeRecordBuffer_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [in]
        size_t numBytes;
    } NVPW_GPU_PeriodicSampler_AcknowledgeRecordBuffer_Params;
#define NVPW_GPU_PeriodicSampler_AcknowledgeRecordBuffer_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_AcknowledgeRecordBuffer_Params, numBytes)

    /// This API is typically used in keep-oldest mode to acknowledge the number of bytes that the software has read. By
    /// doing so, it frees up space in the record buffer, allowing the GPU to write new records.In keep-latest mode,
    /// since the GPU is allowed to continue writing records even if it overwrites unread data, this API is not strictly
    /// required.
    NVPA_Status NVPW_GPU_PeriodicSampler_AcknowledgeRecordBuffer(NVPW_GPU_PeriodicSampler_AcknowledgeRecordBuffer_Params* pParams);

    typedef struct NVPW_GPU_PeriodicSampler_SetRecordBufferReadOffset_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [in]
        size_t readOffset;
    } NVPW_GPU_PeriodicSampler_SetRecordBufferReadOffset_Params;
#define NVPW_GPU_PeriodicSampler_SetRecordBufferReadOffset_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_SetRecordBufferReadOffset_Params, readOffset)

    /// Please note that this API only adjusts the CPU-side read offset and does not modify the corresponding state on
    /// the GPU. To alter the state on the GPU, use the AcknowledgeRecordBuffer(). There are several scenarios where
    /// adjusting this can be beneficial. One notable example is in keep-latest mode, where clients can use the API to
    /// reposition the read offset to point to the oldest data.
    NVPA_Status NVPW_GPU_PeriodicSampler_SetRecordBufferReadOffset(NVPW_GPU_PeriodicSampler_SetRecordBufferReadOffset_Params* pParams);

#ifndef NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_DEFINED
#define NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_DEFINED
    typedef enum NVPW_GPU_PeriodicSampler_DecodeStopReason
    {
        NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_OTHER = 0,
        /// `readEndPointTimestamp` is reached.
        NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_END_TIMESTAMP_REACHED = 1,
        /// `numBytesToRead` is reached.
        NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_ALL_GIVEN_BYTES_READ = 2,
        /// Counter data image is full.
        NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_COUNTER_DATA_FULL = 3,
        /// End of records is reached.
        NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_END_OF_RECORDS = 4,
        /// Encountered an unexpected record.
        NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_UNEXPECTED_RECORD = 5,
        /// Encountered an out-of-order record, may be caused by not setting readoffset correctly.
        NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_OUT_OF_ORDER_RECORD = 6,
        /// Encountered records with potential data loss due to excessive backpressure. Try reducing the sampling rate.
        NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_EXCESSIVE_BACKPRESSURE = 7,
        NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON__COUNT
    } NVPW_GPU_PeriodicSampler_DecodeStopReason;
#endif //NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_DEFINED

    typedef struct NVPW_GPU_PeriodicSampler_DecodeCounters_V3_Params
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
        /// [in] If it's set to 0, `readStartpointTimestamp` & `readEndpointTimestamp` will be used.
        size_t numBytesToRead;
        /// [in] Records that have a timestamp earlier than this point will be discarded.
        uint64_t readStartpointTimestamp;
        /// [in] If both `readStartpointTimestamp` & `readEndpointTimestamp` are set to 0, `numBytesToRead` will be
        /// used.
        uint64_t readEndpointTimestamp;
        /// [out] one of `NVPW_GPU_PeriodicSampler_DecodeStopReason`
        uint32_t decodeStopReason;
        /// [out] number of samples merged due to insufficient sample interval
        size_t numSamplesMerged;
        /// [out]
        size_t numBytesConsumed;
    } NVPW_GPU_PeriodicSampler_DecodeCounters_V3_Params;
#define NVPW_GPU_PeriodicSampler_DecodeCounters_V3_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GPU_PeriodicSampler_DecodeCounters_V3_Params, numBytesConsumed)

    /// Use either `numBytesToRead` or [`readStartpointTimestamp`, `readEndpointTimestamp`) to control the decode range.
    /// Decoding will stop either when the specified `numBytesToRead` or `readEndPointTimestamp` is reached. i.e., the
    /// given numBytesToRead and timestamps cannot be both zero or both non-zero, readStartpointTimestamp must be
    /// smaller than readEndpointTimestamp. However, there are other conditions that may cause the decoding process to
    /// terminate, such as when the counter data image is full or when the end of the record stream is reached.See also
    /// `NVPW_GPU_PeriodicSampler_DecodeStopReason`. Only the CPU-side readOffset will be updated automatically at the
    /// end of this function. Please note that due to performance considerations, the API minimally processes decoded
    /// counter values from the hardware records and stores them directly into the counter data. The first sample in a
    /// sampling range, by definition, represents the accumulated counter values between SetConfig (or the last trigger
    /// from the previous sampling range) and the first trigger in the current sampling range which may result in larger
    /// counter values. Since it may not have a prior trigger, the startTimestamp can be 0. The recommended approach is
    /// to either discard the first sample during post-processing or provide a startTimestamp to the API. Similarly, the
    /// last sample might contain only trailing overflow prevention records emitted after the last trigger. As it is not
    /// associated with a trigger, it has an endTimestamp of 0. This sample can also be discarded through post-
    /// processing or by providing an endTimestamp to the API.
    NVPA_Status NVPW_GPU_PeriodicSampler_DecodeCounters_V3(NVPW_GPU_PeriodicSampler_DecodeCounters_V3_Params* pParams);

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
