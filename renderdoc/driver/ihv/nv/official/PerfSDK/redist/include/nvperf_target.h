#ifndef NVPERF_TARGET_H
#define NVPERF_TARGET_H

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
 *  @file   nvperf_target.h
 */

#ifndef NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_DEFINED
#define NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_DEFINED
    /// GPU architecture support level
    typedef enum NVPW_GpuArchitectureSupportLevel
    {
        NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_UNKNOWN = 0,
        NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_UNSUPPORTED,
        NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_SUPPORTED
    } NVPW_GpuArchitectureSupportLevel;
#endif //NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_DEFINED

#ifndef NVPW_SLI_SUPPORT_LEVEL_DEFINED
#define NVPW_SLI_SUPPORT_LEVEL_DEFINED
    /// SLI configuration support level
    typedef enum NVPW_SliSupportLevel
    {
        NVPW_SLI_SUPPORT_LEVEL_UNKNOWN = 0,
        NVPW_SLI_SUPPORT_LEVEL_UNSUPPORTED,
        /// Only Non-SLI configurations are supported.
        NVPW_SLI_SUPPORT_LEVEL_SUPPORTED_NON_SLI_CONFIGURATION
    } NVPW_SliSupportLevel;
#endif //NVPW_SLI_SUPPORT_LEVEL_DEFINED

#ifndef NVPW_VGPU_SUPPORT_LEVEL_DEFINED
#define NVPW_VGPU_SUPPORT_LEVEL_DEFINED
    /// Virtualized GPU configuration support level
    typedef enum NVPW_VGpuSupportLevel
    {
        NVPW_VGPU_SUPPORT_LEVEL_UNKNOWN = 0,
        NVPW_VGPU_SUPPORT_LEVEL_UNSUPPORTED,
        /// Supported but not allowed by system admin.
        NVPW_VGPU_SUPPORT_LEVEL_SUPPORTED_DISALLOWED,
        NVPW_VGPU_SUPPORT_LEVEL_SUPPORTED_ALLOWED,
        NVPW_VGPU_SUPPORT_LEVEL_SUPPORTED_NON_VGPU_CONFIGURATION
    } NVPW_VGpuSupportLevel;
#endif //NVPW_VGPU_SUPPORT_LEVEL_DEFINED

#ifndef NVPW_CONF_COMPUTE_SUPPORT_LEVEL_DEFINED
#define NVPW_CONF_COMPUTE_SUPPORT_LEVEL_DEFINED
    /// Confidential Compute mode support level
    typedef enum NVPW_ConfidentialComputeSupportLevel
    {
        NVPW_CONF_COMPUTE_SUPPORT_LEVEL_UNKNOWN = 0,
        NVPW_CONF_COMPUTE_SUPPORT_LEVEL_UNSUPPORTED,
        NVPW_CONF_COMPUTE_SUPPORT_LEVEL_SUPPORTED_NON_CONF_COMPUTE_CONFIGURATION
    } NVPW_ConfidentialComputeSupportLevel;
#endif //NVPW_CONF_COMPUTE_SUPPORT_LEVEL_DEFINED

#ifndef NVPW_CMP_SUPPORT_LEVEL_DEFINED
#define NVPW_CMP_SUPPORT_LEVEL_DEFINED
    /// CMP support level
    typedef enum NVPW_CmpSupportLevel
    {
        NVPW_CMP_SUPPORT_LEVEL_UNKNOWN = 0,
        NVPW_CMP_SUPPORT_LEVEL_UNSUPPORTED,
        NVPW_CMP_SUPPORT_LEVEL_SUPPORTED_NON_CMP_CONFIGURATON
    } NVPW_CmpSupportLevel;
#endif //NVPW_CMP_SUPPORT_LEVEL_DEFINED

#ifndef NVPW_WSL_SUPPORT_LEVEL_DEFINED
#define NVPW_WSL_SUPPORT_LEVEL_DEFINED
    /// WSL support level
    typedef enum NVPW_WslSupportLevel
    {
        NVPW_WSL_SUPPORT_LEVEL_UNKNOWN = 0,
        NVPW_WSL_SUPPORT_LEVEL_UNSUPPORTED_INSUFFICIENT_DRIVER_VERSION,
        NVPW_WSL_SUPPORT_LEVEL_SUPPORTED,
        NVPW_WSL_SUPPORT_LEVEL_SUPPORTED_NON_WSL_CONFIGURATION
    } NVPW_WslSupportLevel;
#endif //NVPW_WSL_SUPPORT_LEVEL_DEFINED

    typedef struct NVPW_InitializeTarget_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
    } NVPW_InitializeTarget_Params;
#define NVPW_InitializeTarget_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_InitializeTarget_Params, pPriv)

    /// Load the target library.
    NVPA_Status NVPW_InitializeTarget(NVPW_InitializeTarget_Params* pParams);

    typedef struct NVPW_GetDeviceCount_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        size_t numDevices;
    } NVPW_GetDeviceCount_Params;
#define NVPW_GetDeviceCount_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GetDeviceCount_Params, numDevices)

    NVPA_Status NVPW_GetDeviceCount(NVPW_GetDeviceCount_Params* pParams);

    typedef struct NVPW_Device_GetNames_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        size_t deviceIndex;
        const char* pDeviceName;
        const char* pChipName;
    } NVPW_Device_GetNames_Params;
#define NVPW_Device_GetNames_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_Device_GetNames_Params, pChipName)

    NVPA_Status NVPW_Device_GetNames(NVPW_Device_GetNames_Params* pParams);

    typedef struct NVPW_PciBusId
    {
        /// The PCI domain on which the device bus resides.
        uint32_t domain;
        ///  The bus on which the device resides.
        uint16_t bus;
        /// device ID.
        uint16_t device;
    } NVPW_PciBusId;
#define NVPW_PciBusId_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_PciBusId, device)

    typedef struct NVPW_Device_GetPciBusIds_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in] caller-allocated array of NVPW_PciBusId, indexed by NVPW deviceIndex
        NVPW_PciBusId* pBusIds;
        /// [in] size of the pBusIDs array; use result from NVPW_GetDeviceCount
        size_t numDevices;
    } NVPW_Device_GetPciBusIds_Params;
#define NVPW_Device_GetPciBusIds_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_Device_GetPciBusIds_Params, numDevices)

    NVPA_Status NVPW_Device_GetPciBusIds(NVPW_Device_GetPciBusIds_Params* pParams);


#define NVPW_DEVICE_MIG_GPU_INSTANCE_ID_INVALID     0xFFFFFFFFu
#define NVPW_DEVICE_MIG_GPU_INSTANCE_ID_FULLCHIP    0xFFFFFFFEu


    typedef struct NVPW_Device_GetMigAttributes_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [out]
        NVPA_Bool isMigPartition;
        /// [out]
        uint32_t gpuInstanceId;
        /// [out]
        uint32_t computeInstanceId;
    } NVPW_Device_GetMigAttributes_Params;
#define NVPW_Device_GetMigAttributes_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_Device_GetMigAttributes_Params, computeInstanceId)

    NVPA_Status NVPW_Device_GetMigAttributes(NVPW_Device_GetMigAttributes_Params* pParams);

    typedef struct NVPW_Adapter_GetDeviceIndex_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct IDXGIAdapter* pAdapter;
        /// [in]
        size_t sliIndex;
        /// [out]
        size_t deviceIndex;
    } NVPW_Adapter_GetDeviceIndex_Params;
#define NVPW_Adapter_GetDeviceIndex_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_Adapter_GetDeviceIndex_Params, deviceIndex)

    NVPA_Status NVPW_Adapter_GetDeviceIndex(NVPW_Adapter_GetDeviceIndex_Params* pParams);

    typedef struct NVPW_CounterData_GetNumRanges_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        const uint8_t* pCounterDataImage;
        size_t numRanges;
    } NVPW_CounterData_GetNumRanges_Params;
#define NVPW_CounterData_GetNumRanges_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterData_GetNumRanges_Params, numRanges)

    NVPA_Status NVPW_CounterData_GetNumRanges(NVPW_CounterData_GetNumRanges_Params* pParams);

    typedef struct NVPW_CounterData_GetChipName_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const uint8_t* pCounterDataImage;
        /// [in]
        size_t counterDataImageSize;
        /// [out]
        const char* pChipName;
    } NVPW_CounterData_GetChipName_Params;
#define NVPW_CounterData_GetChipName_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterData_GetChipName_Params, pChipName)

    NVPA_Status NVPW_CounterData_GetChipName(NVPW_CounterData_GetChipName_Params* pParams);

    typedef struct NVPW_Config_GetNumPasses_V2_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const uint8_t* pConfig;
        /// [out]
        size_t numPasses;
    } NVPW_Config_GetNumPasses_V2_Params;
#define NVPW_Config_GetNumPasses_V2_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_Config_GetNumPasses_V2_Params, numPasses)

    /// Total num passes = numPasses * numNestingLevels
    NVPA_Status NVPW_Config_GetNumPasses_V2(NVPW_Config_GetNumPasses_V2_Params* pParams);

#define NVPW_API_SET_CUDA_PROFILER             0x18209d0775b2f89dULL

#define NVPW_API_SET_D3D11_PROFILER            0xca55c6738445db2bULL

#define NVPW_API_SET_D3D12_PROFILER            0xc0c2d46dd7c7ad78ULL

#define NVPW_API_SET_EGL_PROFILER              0x3c3747dae1f9565cULL

#define NVPW_API_SET_GPU_PERIODICSAMPLER       0x9f4c2571fc0b2e8aULL

#define NVPW_API_SET_METRICSCONTEXT            0x7c8579f6f2144beaULL

#define NVPW_API_SET_METRICSEVALUATOR          0x0368a8768d811af9ULL

#define NVPW_API_SET_METRICS_GA100_COMP        0x16b7d8c20d8b4915ULL

#define NVPW_API_SET_METRICS_GA100_GRFX        0xc94eaabec04a94faULL

#define NVPW_API_SET_METRICS_GA10X_COMP        0xb5d6391c2e299ab5ULL

#define NVPW_API_SET_METRICS_GA10X_GRFX        0x6ebc121178b5ce0bULL

#define NVPW_API_SET_METRICS_GV100_COMP        0x863705cc57919f72ULL

#define NVPW_API_SET_METRICS_GV100_GRFX        0x9900da75d164fecfULL

#define NVPW_API_SET_METRICS_GV11B_COMP        0xd3f79a859235848fULL

#define NVPW_API_SET_METRICS_GV11B_GRFX        0xeb8e26220106e227ULL

#define NVPW_API_SET_METRICS_TU10X_COMP        0x70f40be0afd35da8ULL

#define NVPW_API_SET_METRICS_TU10X_GRFX        0xdf219cb838db6968ULL

#define NVPW_API_SET_METRICS_TU11X_COMP        0xeb0069d7d0956678ULL

#define NVPW_API_SET_METRICS_TU11X_GRFX        0x0977d9342bd62743ULL

#define NVPW_API_SET_OPENGL_PROFILER           0xe4cd9ea40f2ee777ULL

#define NVPW_API_SET_VULKAN_PROFILER           0x8c56b6a03d779689ULL

#define NVPW_SDK_VERSION               0x1e128b6f001423fcULL

    typedef struct NVPW_QueryVersionNumber_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        uint64_t apiSet;
        /// [out]
        uint32_t major;
        /// [out]
        uint32_t minor;
        /// [out]
        uint32_t patch;
        /// [out]
        uint32_t relMajor;
        /// [out]
        uint32_t relMinor;
        /// [out]
        uint32_t relPatch;
    } NVPW_QueryVersionNumber_Params;
#define NVPW_QueryVersionNumber_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_QueryVersionNumber_Params, relPatch)

    /// Query version number of an API set
    NVPA_Status NVPW_QueryVersionNumber(NVPW_QueryVersionNumber_Params* pParams);

    typedef enum NVPW_Device_ClockStatus
    {
        /// clock status is unknown
        NVPW_DEVICE_CLOCK_STATUS_UNKNOWN,
        /// clocks are locked to rated tdp values
        NVPW_DEVICE_CLOCK_STATUS_LOCKED_TO_RATED_TDP,
        /// clocks are not locked and can boost above rated tdp
        NVPW_DEVICE_CLOCK_STATUS_BOOST_ENABLED,
        /// clocks are not locked and will not go above rated tdp
        NVPW_DEVICE_CLOCK_STATUS_BOOST_DISABLED,
        NVPW_DEVICE_CLOCK_STATUS__COUNT
    } NVPW_Device_ClockStatus;

    typedef struct NVPW_Device_GetClockStatus_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        size_t deviceIndex;
        /// [in]
        NVPW_Device_ClockStatus clockStatus;
    } NVPW_Device_GetClockStatus_Params;
#define NVPW_Device_GetClockStatus_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_Device_GetClockStatus_Params, clockStatus)

    NVPA_Status NVPW_Device_GetClockStatus(NVPW_Device_GetClockStatus_Params* pParams);

    typedef enum NVPW_Device_ClockSetting
    {
        /// invalid op, specify valid clocks operation during profiling
        NVPW_DEVICE_CLOCK_SETTING_INVALID,
        /// default to driver/application config (normally unlocked and not boosted, but could be unlocked boosted, or
        /// locked to rated TDP)
        NVPW_DEVICE_CLOCK_SETTING_DEFAULT,
        /// lock clocks at rated tdp base values
        NVPW_DEVICE_CLOCK_SETTING_LOCK_TO_RATED_TDP,
        NVPW_DEVICE_CLOCK_SETTING__COUNT
    } NVPW_Device_ClockSetting;

    typedef struct NVPW_Device_SetClockSetting_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        size_t deviceIndex;
        /// [in]
        NVPW_Device_ClockSetting clockSetting;
    } NVPW_Device_SetClockSetting_Params;
#define NVPW_Device_SetClockSetting_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_Device_SetClockSetting_Params, clockSetting)

    NVPA_Status NVPW_Device_SetClockSetting(NVPW_Device_SetClockSetting_Params* pParams);

    typedef struct NVPW_CounterData_GetRangeDescriptions_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        const uint8_t* pCounterDataImage;
        size_t rangeIndex;
        /// [inout] Number of descriptions allocated in ppDescriptions
        size_t numDescriptions;
        const char** ppDescriptions;
    } NVPW_CounterData_GetRangeDescriptions_Params;
#define NVPW_CounterData_GetRangeDescriptions_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterData_GetRangeDescriptions_Params, ppDescriptions)

    NVPA_Status NVPW_CounterData_GetRangeDescriptions(NVPW_CounterData_GetRangeDescriptions_Params* pParams);

    typedef struct NVPW_Profiler_CounterData_GetRangeDescriptions_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        const uint8_t* pCounterDataImage;
        size_t rangeIndex;
        /// [inout] Number of descriptions allocated in ppDescriptions
        size_t numDescriptions;
        const char** ppDescriptions;
    } NVPW_Profiler_CounterData_GetRangeDescriptions_Params;
#define NVPW_Profiler_CounterData_GetRangeDescriptions_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_Profiler_CounterData_GetRangeDescriptions_Params, ppDescriptions)

    NVPA_Status NVPW_Profiler_CounterData_GetRangeDescriptions(NVPW_Profiler_CounterData_GetRangeDescriptions_Params* pParams);

#ifndef NVPW_PERIODIC_SAMPLER_COUNTER_DATA_APPEND_MODE_DEFINED
#define NVPW_PERIODIC_SAMPLER_COUNTER_DATA_APPEND_MODE_DEFINED
    typedef enum NVPW_PeriodicSampler_CounterData_AppendMode
    {
        NVPW_PERIODIC_SAMPLER_COUNTER_DATA_APPEND_MODE_LINEAR = 0,
        NVPW_PERIODIC_SAMPLER_COUNTER_DATA_APPEND_MODE_CIRCULAR = 1,
        NVPW_PERIODIC_SAMPLER_COUNTER_DATA_APPEND_MODE__COUNT
    } NVPW_PeriodicSampler_CounterData_AppendMode;
#endif //NVPW_PERIODIC_SAMPLER_COUNTER_DATA_APPEND_MODE_DEFINED

    typedef struct NVPW_PeriodicSampler_CounterData_GetSampleTime_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const uint8_t* pCounterDataImage;
        /// [in]
        size_t rangeIndex;
        /// [out]
        uint64_t timestampStart;
        /// [out]
        uint64_t timestampEnd;
    } NVPW_PeriodicSampler_CounterData_GetSampleTime_Params;
#define NVPW_PeriodicSampler_CounterData_GetSampleTime_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_PeriodicSampler_CounterData_GetSampleTime_Params, timestampEnd)

    NVPA_Status NVPW_PeriodicSampler_CounterData_GetSampleTime(NVPW_PeriodicSampler_CounterData_GetSampleTime_Params* pParams);

    typedef struct NVPW_PeriodicSampler_CounterData_TrimInPlace_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        uint8_t* pCounterDataImage;
        /// [in]
        size_t counterDataImageSize;
        /// [out]
        size_t counterDataImageTrimmedSize;
    } NVPW_PeriodicSampler_CounterData_TrimInPlace_Params;
#define NVPW_PeriodicSampler_CounterData_TrimInPlace_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_PeriodicSampler_CounterData_TrimInPlace_Params, counterDataImageTrimmedSize)

    NVPA_Status NVPW_PeriodicSampler_CounterData_TrimInPlace(NVPW_PeriodicSampler_CounterData_TrimInPlace_Params* pParams);

    typedef struct NVPW_PeriodicSampler_CounterData_GetInfo_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const uint8_t* pCounterDataImage;
        /// [in]
        size_t counterDataImageSize;
        /// [out] total number of ranges in the counter data
        size_t numTotalRanges;
        /// [out] if in "linear" mode, this API returns the number of "populated" ranges; if it's in "circular" mode,
        /// then it returns the last "populated" range index + 1, when there is no such range, it returns 0.
        size_t numPopulatedRanges;
        /// [out] if in "linear" mode, this API returns the number of "completed" ranges; if it's in "circular" mode,
        /// then it returns the last "completed" range index + 1, when there is no such range, it returns 0.
        size_t numCompletedRanges;
    } NVPW_PeriodicSampler_CounterData_GetInfo_Params;
#define NVPW_PeriodicSampler_CounterData_GetInfo_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_PeriodicSampler_CounterData_GetInfo_Params, numCompletedRanges)

    /// In periodic sampler, a range in counter data stores exactly one sample's data. For better performance, periodic
    /// sampler may operate in an out-of-order fashion when populating sample data, i.e. it may not fully populate all
    /// counters of a sample/range before starting to populate the next sample/range. As a result, we have two concepts
    /// here, "populated" & "completed": a range is considered "populated" even if only partial counters have been
    /// written; on the other hand, a range is only considered "completed" if all the collecting counters have been
    /// written.
    NVPA_Status NVPW_PeriodicSampler_CounterData_GetInfo(NVPW_PeriodicSampler_CounterData_GetInfo_Params* pParams);

    typedef struct NVPW_PeriodicSampler_CounterData_GetTriggerCount_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const uint8_t* pCounterDataImage;
        /// [in]
        size_t counterDataImageSize;
        /// [in]
        size_t rangeIndex;
        /// [out]
        uint32_t triggerCount;
    } NVPW_PeriodicSampler_CounterData_GetTriggerCount_Params;
#define NVPW_PeriodicSampler_CounterData_GetTriggerCount_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_PeriodicSampler_CounterData_GetTriggerCount_Params, triggerCount)

    NVPA_Status NVPW_PeriodicSampler_CounterData_GetTriggerCount(NVPW_PeriodicSampler_CounterData_GetTriggerCount_Params* pParams);


    typedef struct NVPW_TimestampReport
    {
        uint32_t payload;
        uint8_t reserved0004[4];
        uint64_t timestamp;
    } NVPW_TimestampReport;




#ifdef __cplusplus
} // extern "C"
#endif

#if defined(__GNUC__) && defined(NVPA_SHARED_LIB)
    #pragma GCC visibility pop
#endif

#endif // NVPERF_TARGET_H
