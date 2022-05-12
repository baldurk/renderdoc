#ifndef NVPERF_HOST_H
#define NVPERF_HOST_H

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
 *  @file   nvperf_host.h
 */


// Guard against multiple definition of NvPerf host types
#ifndef NVPERF_HOST_API_DEFINED
#define NVPERF_HOST_API_DEFINED


/***************************************************************************//**
 *  @name   Host Configuration
 *  @{
 */

    typedef struct NVPW_InitializeHost_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
    } NVPW_InitializeHost_Params;
#define NVPW_InitializeHost_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_InitializeHost_Params, pPriv)

    /// Load the host library.
    NVPA_Status NVPW_InitializeHost(NVPW_InitializeHost_Params* pParams);

    typedef struct NVPW_CounterData_CalculateCounterDataImageCopySize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// The CounterDataPrefix generated from e.g.    nvperf2 initdata   or
        /// NVPW_CounterDataBuilder_GetCounterDataPrefix().  Must be align(8).
        const uint8_t* pCounterDataPrefix;
        size_t counterDataPrefixSize;
        /// max number of ranges that can be profiled
        uint32_t maxNumRanges;
        /// max number of RangeTree nodes; must be >= maxNumRanges
        uint32_t maxNumRangeTreeNodes;
        /// max string length of each RangeName, including the trailing NUL character
        uint32_t maxRangeNameLength;
        const uint8_t* pCounterDataSrc;
        /// [out] required size of the copy buffer
        size_t copyDataImageCounterSize;
    } NVPW_CounterData_CalculateCounterDataImageCopySize_Params;
#define NVPW_CounterData_CalculateCounterDataImageCopySize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterData_CalculateCounterDataImageCopySize_Params, copyDataImageCounterSize)

    NVPA_Status NVPW_CounterData_CalculateCounterDataImageCopySize(NVPW_CounterData_CalculateCounterDataImageCopySize_Params* pParams);

    typedef struct NVPW_CounterData_InitializeCounterDataImageCopy_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// The CounterDataPrefix generated from e.g.    nvperf2 initdata   or
        /// NVPW_CounterDataBuilder_GetCounterDataPrefix().  Must be align(8).
        const uint8_t* pCounterDataPrefix;
        size_t counterDataPrefixSize;
        /// max number of ranges that can be profiled
        uint32_t maxNumRanges;
        /// max number of RangeTree nodes; must be >= maxNumRanges
        uint32_t maxNumRangeTreeNodes;
        /// max string length of each RangeName, including the trailing NUL character
        uint32_t maxRangeNameLength;
        const uint8_t* pCounterDataSrc;
        uint8_t* pCounterDataDst;
    } NVPW_CounterData_InitializeCounterDataImageCopy_Params;
#define NVPW_CounterData_InitializeCounterDataImageCopy_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterData_InitializeCounterDataImageCopy_Params, pCounterDataDst)

    NVPA_Status NVPW_CounterData_InitializeCounterDataImageCopy(NVPW_CounterData_InitializeCounterDataImageCopy_Params* pParams);

    typedef struct NVPA_CounterDataCombiner NVPA_CounterDataCombiner;

    typedef struct NVPW_CounterDataCombiner_Create_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// The destination counter data into which the source datas will be combined
        uint8_t* pCounterDataDst;
        /// [out] The created counter data combiner
        NVPA_CounterDataCombiner* pCounterDataCombiner;
    } NVPW_CounterDataCombiner_Create_Params;
#define NVPW_CounterDataCombiner_Create_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterDataCombiner_Create_Params, pCounterDataCombiner)

    NVPA_Status NVPW_CounterDataCombiner_Create(NVPW_CounterDataCombiner_Create_Params* pParams);

    typedef struct NVPW_CounterDataCombiner_Destroy_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_CounterDataCombiner* pCounterDataCombiner;
    } NVPW_CounterDataCombiner_Destroy_Params;
#define NVPW_CounterDataCombiner_Destroy_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterDataCombiner_Destroy_Params, pCounterDataCombiner)

    NVPA_Status NVPW_CounterDataCombiner_Destroy(NVPW_CounterDataCombiner_Destroy_Params* pParams);

    typedef struct NVPW_CounterDataCombiner_CreateRange_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_CounterDataCombiner* pCounterDataCombiner;
        size_t numDescriptions;
        const char* const* ppDescriptions;
        /// [out]
        size_t rangeIndexDst;
    } NVPW_CounterDataCombiner_CreateRange_Params;
#define NVPW_CounterDataCombiner_CreateRange_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterDataCombiner_CreateRange_Params, rangeIndexDst)

    NVPA_Status NVPW_CounterDataCombiner_CreateRange(NVPW_CounterDataCombiner_CreateRange_Params* pParams);

    typedef struct NVPW_CounterDataCombiner_CopyIntoRange_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        NVPA_CounterDataCombiner* pCounterDataCombiner;
        /// [in]
        size_t rangeIndexDst;
        /// [in]
        const uint8_t* pCounterDataSrc;
        /// [in]
        size_t rangeIndexSrc;
    } NVPW_CounterDataCombiner_CopyIntoRange_Params;
#define NVPW_CounterDataCombiner_CopyIntoRange_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterDataCombiner_CopyIntoRange_Params, rangeIndexSrc)

    /// In order to use this API, the source counter data and the destination counter data must have identical counters
    NVPA_Status NVPW_CounterDataCombiner_CopyIntoRange(NVPW_CounterDataCombiner_CopyIntoRange_Params* pParams);

    typedef struct NVPW_CounterDataCombiner_AccumulateIntoRange_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_CounterDataCombiner* pCounterDataCombiner;
        size_t rangeIndexDst;
        uint32_t dstMultiplier;
        const uint8_t* pCounterDataSrc;
        size_t rangeIndexSrc;
        uint32_t srcMultiplier;
    } NVPW_CounterDataCombiner_AccumulateIntoRange_Params;
#define NVPW_CounterDataCombiner_AccumulateIntoRange_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterDataCombiner_AccumulateIntoRange_Params, srcMultiplier)

    NVPA_Status NVPW_CounterDataCombiner_AccumulateIntoRange(NVPW_CounterDataCombiner_AccumulateIntoRange_Params* pParams);

    typedef struct NVPW_CounterDataCombiner_SumIntoRange_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_CounterDataCombiner* pCounterDataCombiner;
        size_t rangeIndexDst;
        const uint8_t* pCounterDataSrc;
        size_t rangeIndexSrc;
    } NVPW_CounterDataCombiner_SumIntoRange_Params;
#define NVPW_CounterDataCombiner_SumIntoRange_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterDataCombiner_SumIntoRange_Params, rangeIndexSrc)

    NVPA_Status NVPW_CounterDataCombiner_SumIntoRange(NVPW_CounterDataCombiner_SumIntoRange_Params* pParams);

    typedef struct NVPW_CounterDataCombiner_WeightedSumIntoRange_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_CounterDataCombiner* pCounterDataCombiner;
        size_t rangeIndexDst;
        double dstMultiplier;
        const uint8_t* pCounterDataSrc;
        size_t rangeIndexSrc;
        double srcMultiplier;
    } NVPW_CounterDataCombiner_WeightedSumIntoRange_Params;
#define NVPW_CounterDataCombiner_WeightedSumIntoRange_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterDataCombiner_WeightedSumIntoRange_Params, srcMultiplier)

    NVPA_Status NVPW_CounterDataCombiner_WeightedSumIntoRange(NVPW_CounterDataCombiner_WeightedSumIntoRange_Params* pParams);

/**
 *  @}
 ******************************************************************************/
 
/***************************************************************************//**
 *  @name   Metrics Configuration
 *  @{
 */

    typedef struct NVPA_RawMetricsConfig NVPA_RawMetricsConfig;

    typedef struct NVPA_RawMetricRequest
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// in
        const char* pMetricName;
        /// in; reserved
        NVPA_Bool rsvd0018;
        /// in; ignored by AddMetric but observed by CounterData initialization
        NVPA_Bool keepInstances;
    } NVPA_RawMetricRequest;
#define NVPA_RAW_METRIC_REQUEST_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPA_RawMetricRequest, keepInstances)

    typedef struct NVPW_GetSupportedChipNames_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [out]
        const char* const* ppChipNames;
        /// [out]
        size_t numChipNames;
    } NVPW_GetSupportedChipNames_Params;
#define NVPW_GetSupportedChipNames_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_GetSupportedChipNames_Params, numChipNames)

    NVPA_Status NVPW_GetSupportedChipNames(NVPW_GetSupportedChipNames_Params* pParams);

    typedef struct NVPW_RawMetricsConfig_Destroy_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_RawMetricsConfig* pRawMetricsConfig;
    } NVPW_RawMetricsConfig_Destroy_Params;
#define NVPW_RawMetricsConfig_Destroy_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawMetricsConfig_Destroy_Params, pRawMetricsConfig)

    NVPA_Status NVPW_RawMetricsConfig_Destroy(NVPW_RawMetricsConfig_Destroy_Params* pParams);

    typedef struct NVPW_RawMetricsConfig_SetCounterAvailability_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_RawMetricsConfig* pRawMetricsConfig;
        /// [in] buffer with counter availability image
        const uint8_t* pCounterAvailabilityImage;
    } NVPW_RawMetricsConfig_SetCounterAvailability_Params;
#define NVPW_RawMetricsConfig_SetCounterAvailability_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawMetricsConfig_SetCounterAvailability_Params, pCounterAvailabilityImage)

    NVPA_Status NVPW_RawMetricsConfig_SetCounterAvailability(NVPW_RawMetricsConfig_SetCounterAvailability_Params* pParams);

    typedef struct NVPW_RawMetricsConfig_BeginPassGroup_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_RawMetricsConfig* pRawMetricsConfig;
        size_t maxPassCount;
    } NVPW_RawMetricsConfig_BeginPassGroup_Params;
#define NVPW_RawMetricsConfig_BeginPassGroup_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawMetricsConfig_BeginPassGroup_Params, maxPassCount)

    NVPA_Status NVPW_RawMetricsConfig_BeginPassGroup(NVPW_RawMetricsConfig_BeginPassGroup_Params* pParams);

    typedef struct NVPW_RawMetricsConfig_EndPassGroup_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_RawMetricsConfig* pRawMetricsConfig;
    } NVPW_RawMetricsConfig_EndPassGroup_Params;
#define NVPW_RawMetricsConfig_EndPassGroup_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawMetricsConfig_EndPassGroup_Params, pRawMetricsConfig)

    NVPA_Status NVPW_RawMetricsConfig_EndPassGroup(NVPW_RawMetricsConfig_EndPassGroup_Params* pParams);

    typedef struct NVPW_RawMetricsConfig_GetNumMetrics_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        const NVPA_RawMetricsConfig* pRawMetricsConfig;
        /// [out]
        size_t numMetrics;
    } NVPW_RawMetricsConfig_GetNumMetrics_Params;
#define NVPW_RawMetricsConfig_GetNumMetrics_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawMetricsConfig_GetNumMetrics_Params, numMetrics)

    NVPA_Status NVPW_RawMetricsConfig_GetNumMetrics(NVPW_RawMetricsConfig_GetNumMetrics_Params* pParams);

    typedef struct NVPW_RawMetricsConfig_GetMetricProperties_V2_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        const NVPA_RawMetricsConfig* pRawMetricsConfig;
        size_t metricIndex;
        /// [out]
        const char* pMetricName;
    } NVPW_RawMetricsConfig_GetMetricProperties_V2_Params;
#define NVPW_RawMetricsConfig_GetMetricProperties_V2_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawMetricsConfig_GetMetricProperties_V2_Params, pMetricName)

    NVPA_Status NVPW_RawMetricsConfig_GetMetricProperties_V2(NVPW_RawMetricsConfig_GetMetricProperties_V2_Params* pParams);

    typedef struct NVPW_RawMetricsConfig_AddMetrics_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_RawMetricsConfig* pRawMetricsConfig;
        const NVPA_RawMetricRequest* pRawMetricRequests;
        size_t numMetricRequests;
    } NVPW_RawMetricsConfig_AddMetrics_Params;
#define NVPW_RawMetricsConfig_AddMetrics_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawMetricsConfig_AddMetrics_Params, numMetricRequests)

    NVPA_Status NVPW_RawMetricsConfig_AddMetrics(NVPW_RawMetricsConfig_AddMetrics_Params* pParams);

    typedef struct NVPW_RawMetricsConfig_IsAddMetricsPossible_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        const NVPA_RawMetricsConfig* pRawMetricsConfig;
        const NVPA_RawMetricRequest* pRawMetricRequests;
        size_t numMetricRequests;
        /// [out]
        NVPA_Bool isPossible;
    } NVPW_RawMetricsConfig_IsAddMetricsPossible_Params;
#define NVPW_RawMetricsConfig_IsAddMetricsPossible_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawMetricsConfig_IsAddMetricsPossible_Params, isPossible)

    NVPA_Status NVPW_RawMetricsConfig_IsAddMetricsPossible(NVPW_RawMetricsConfig_IsAddMetricsPossible_Params* pParams);

    typedef struct NVPW_RawMetricsConfig_GenerateConfigImage_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_RawMetricsConfig* pRawMetricsConfig;
        /// [in] If true, all existing pass groups may be merged to reduce number of passes.
        /// If merge was successful, distribution of counters in passes may be updated as a side-effect. The effects
        /// will be persistent in pRawMetricsConfig.
        NVPA_Bool mergeAllPassGroups;
    } NVPW_RawMetricsConfig_GenerateConfigImage_Params;
#define NVPW_RawMetricsConfig_GenerateConfigImage_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawMetricsConfig_GenerateConfigImage_Params, mergeAllPassGroups)

    /// This API may fail if called inside a pass group with `mergeAllPassGroups` = true.
    NVPA_Status NVPW_RawMetricsConfig_GenerateConfigImage(NVPW_RawMetricsConfig_GenerateConfigImage_Params* pParams);

    typedef struct NVPW_RawMetricsConfig_GetConfigImage_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        const NVPA_RawMetricsConfig* pRawMetricsConfig;
        /// [in] Number of bytes allocated for pBuffer
        size_t bytesAllocated;
        /// [out] [optional] Buffer receiving the config image
        uint8_t* pBuffer;
        /// [out] Count of bytes that would be copied into pBuffer
        size_t bytesCopied;
    } NVPW_RawMetricsConfig_GetConfigImage_Params;
#define NVPW_RawMetricsConfig_GetConfigImage_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawMetricsConfig_GetConfigImage_Params, bytesCopied)

    NVPA_Status NVPW_RawMetricsConfig_GetConfigImage(NVPW_RawMetricsConfig_GetConfigImage_Params* pParams);

    typedef struct NVPW_RawMetricsConfig_GetNumPasses_V2_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const NVPA_RawMetricsConfig* pRawMetricsConfig;
        /// [out]
        size_t numPasses;
    } NVPW_RawMetricsConfig_GetNumPasses_V2_Params;
#define NVPW_RawMetricsConfig_GetNumPasses_V2_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawMetricsConfig_GetNumPasses_V2_Params, numPasses)

    /// Total num passes = numPasses * numNestingLevels
    NVPA_Status NVPW_RawMetricsConfig_GetNumPasses_V2(NVPW_RawMetricsConfig_GetNumPasses_V2_Params* pParams);

    typedef struct NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in] Typically created by e.g. NVPW_RawMetricsConfig_GetConfigImage(), must be align(8).
        const uint8_t* pConfig;
        /// [in]
        size_t configSize;
        /// [out]
        size_t sampleSize;
    } NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize_Params;
#define NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize_Params, sampleSize)

    /// Estimate per sample records size based on a virtual device
    NVPA_Status NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize(NVPW_PeriodicSampler_Config_GetSocEstimatedSampleSize_Params* pParams);

    typedef struct NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in] Typically created by e.g. NVPW_RawMetricsConfig_GetConfigImage(), must be align(8).
        const uint8_t* pConfig;
        /// [in]
        size_t configSize;
        /// [out]
        size_t sampleSize;
    } NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize_Params;
#define NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize_Params, sampleSize)

    /// Estimate per sample records size based on a virtual device
    NVPA_Status NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize(NVPW_PeriodicSampler_Config_GetGpuEstimatedSampleSize_Params* pParams);

/**
 *  @}
 ******************************************************************************/
 
/***************************************************************************//**
 *  @name   CounterData Creation
 *  @{
 */

    typedef struct NVPA_CounterDataBuilder NVPA_CounterDataBuilder;

    typedef struct NVPW_CounterDataBuilder_Create_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [out]
        NVPA_CounterDataBuilder* pCounterDataBuilder;
        const char* pChipName;
    } NVPW_CounterDataBuilder_Create_Params;
#define NVPW_CounterDataBuilder_Create_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterDataBuilder_Create_Params, pChipName)

    NVPA_Status NVPW_CounterDataBuilder_Create(NVPW_CounterDataBuilder_Create_Params* pParams);

    typedef struct NVPW_CounterDataBuilder_Destroy_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_CounterDataBuilder* pCounterDataBuilder;
    } NVPW_CounterDataBuilder_Destroy_Params;
#define NVPW_CounterDataBuilder_Destroy_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterDataBuilder_Destroy_Params, pCounterDataBuilder)

    NVPA_Status NVPW_CounterDataBuilder_Destroy(NVPW_CounterDataBuilder_Destroy_Params* pParams);

    typedef struct NVPW_CounterDataBuilder_AddMetrics_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_CounterDataBuilder* pCounterDataBuilder;
        const NVPA_RawMetricRequest* pRawMetricRequests;
        size_t numMetricRequests;
    } NVPW_CounterDataBuilder_AddMetrics_Params;
#define NVPW_CounterDataBuilder_AddMetrics_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterDataBuilder_AddMetrics_Params, numMetricRequests)

    NVPA_Status NVPW_CounterDataBuilder_AddMetrics(NVPW_CounterDataBuilder_AddMetrics_Params* pParams);

    typedef struct NVPW_CounterDataBuilder_GetCounterDataPrefix_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        NVPA_CounterDataBuilder* pCounterDataBuilder;
        /// [in] Number of bytes allocated for pBuffer
        size_t bytesAllocated;
        /// [out] [optional] Buffer receiving the counter data prefix
        uint8_t* pBuffer;
        /// [out] Count of bytes that would be copied to pBuffer
        size_t bytesCopied;
    } NVPW_CounterDataBuilder_GetCounterDataPrefix_Params;
#define NVPW_CounterDataBuilder_GetCounterDataPrefix_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterDataBuilder_GetCounterDataPrefix_Params, bytesCopied)

    NVPA_Status NVPW_CounterDataBuilder_GetCounterDataPrefix(NVPW_CounterDataBuilder_GetCounterDataPrefix_Params* pParams);

/**
 *  @}
 ******************************************************************************/
 
/***************************************************************************//**
 *  @name   Metrics Evaluator
 *  @{
 */

    typedef struct NVPW_MetricsEvaluator NVPW_MetricsEvaluator;

#ifndef NVPW_DIM_UNIT_DEFINED
#define NVPW_DIM_UNIT_DEFINED
    typedef enum NVPW_DimUnitName
    {
        NVPW_DIM_UNIT_INVALID = 3518299157,
        NVPW_DIM_UNIT_UNITLESS = 2126137902,
        NVPW_DIM_UNIT_ATTRIBUTES = 3776338729,
        NVPW_DIM_UNIT_BYTES = 3797850191,
        NVPW_DIM_UNIT_CTAS = 1960564139,
        NVPW_DIM_UNIT_DRAM_CYCLES = 2650981327,
        NVPW_DIM_UNIT_FBP_CYCLES = 1785238957,
        NVPW_DIM_UNIT_FE_OPS = 2919159083,
        NVPW_DIM_UNIT_GPC_CYCLES = 1222631184,
        NVPW_DIM_UNIT_IDC_REQUESTS = 2012649669,
        NVPW_DIM_UNIT_INSTRUCTIONS = 1418625543,
        NVPW_DIM_UNIT_L1DATA_BANK_ACCESSES = 1479493682,
        NVPW_DIM_UNIT_L1DATA_BANK_CONFLICTS = 3433170787,
        NVPW_DIM_UNIT_L1TEX_REQUESTS = 1306473767,
        NVPW_DIM_UNIT_L1TEX_TAGS = 26573010,
        NVPW_DIM_UNIT_L1TEX_WAVEFRONTS = 129373765,
        NVPW_DIM_UNIT_L2_REQUESTS = 1143695106,
        NVPW_DIM_UNIT_L2_SECTORS = 3424101564,
        NVPW_DIM_UNIT_L2_TAGS = 3755612781,
        NVPW_DIM_UNIT_NANOSECONDS = 3047500672,
        NVPW_DIM_UNIT_NVLRX_CYCLES = 4059934930,
        NVPW_DIM_UNIT_NVLTX_CYCLES = 1814350488,
        NVPW_DIM_UNIT_PCIE_CYCLES = 1230450943,
        NVPW_DIM_UNIT_PERCENT = 1284354694,
        NVPW_DIM_UNIT_PIXELS = 4227616663,
        NVPW_DIM_UNIT_PIXEL_SHADER_BARRIERS = 3705502518,
        NVPW_DIM_UNIT_PRIMITIVES = 2373084002,
        NVPW_DIM_UNIT_QUADS = 1539753497,
        NVPW_DIM_UNIT_REGISTERS = 2837260947,
        NVPW_DIM_UNIT_SAMPLES = 746046551,
        NVPW_DIM_UNIT_SECONDS = 1164825258,
        NVPW_DIM_UNIT_SYS_CYCLES = 3310821688,
        NVPW_DIM_UNIT_TEXELS = 1293214069,
        NVPW_DIM_UNIT_THREADS = 164261907,
        NVPW_DIM_UNIT_VERTICES = 1873662209,
        NVPW_DIM_UNIT_WARPS = 97951949,
        NVPW_DIM_UNIT_WORKLOADS = 1728142656
    } NVPW_DimUnitName;
#endif //NVPW_DIM_UNIT_DEFINED

#ifndef NVPW_HW_UNIT_DEFINED
#define NVPW_HW_UNIT_DEFINED
    typedef enum NVPW_HwUnit
    {
        NVPW_HW_UNIT_INVALID = 3498035701,
        NVPW_HW_UNIT_CROP = 2872137846,
        NVPW_HW_UNIT_DRAM = 1662616918,
        NVPW_HW_UNIT_DRAMC = 1401232876,
        NVPW_HW_UNIT_FBP = 2947194306,
        NVPW_HW_UNIT_FBPA = 690045803,
        NVPW_HW_UNIT_FE = 2204924321,
        NVPW_HW_UNIT_GPC = 1911735839,
        NVPW_HW_UNIT_GPU = 1014363534,
        NVPW_HW_UNIT_GR = 2933618517,
        NVPW_HW_UNIT_IDC = 842765289,
        NVPW_HW_UNIT_L1TEX = 893940957,
        NVPW_HW_UNIT_LTS = 2333266697,
        NVPW_HW_UNIT_NVLRX = 3091684901,
        NVPW_HW_UNIT_NVLTX = 869679659,
        NVPW_HW_UNIT_PCIE = 3433264174,
        NVPW_HW_UNIT_PDA = 345193251,
        NVPW_HW_UNIT_PES = 804128425,
        NVPW_HW_UNIT_PROP = 3339255507,
        NVPW_HW_UNIT_RASTER = 187932504,
        NVPW_HW_UNIT_SM = 724224710,
        NVPW_HW_UNIT_SMSP = 2837616917,
        NVPW_HW_UNIT_SYS = 768990063,
        NVPW_HW_UNIT_TPC = 1889024613,
        NVPW_HW_UNIT_VAF = 753670509,
        NVPW_HW_UNIT_VPC = 275561583,
        NVPW_HW_UNIT_ZROP = 979500456
    } NVPW_HwUnit;
#endif //NVPW_HW_UNIT_DEFINED

    typedef enum NVPW_RollupOp
    {
        NVPW_ROLLUP_OP_AVG = 0,
        NVPW_ROLLUP_OP_MAX,
        NVPW_ROLLUP_OP_MIN,
        NVPW_ROLLUP_OP_SUM,
        NVPW_ROLLUP_OP__COUNT
    } NVPW_RollupOp;

    typedef enum NVPW_MetricType
    {
        NVPW_METRIC_TYPE_COUNTER = 0,
        NVPW_METRIC_TYPE_RATIO,
        NVPW_METRIC_TYPE_THROUGHPUT,
        NVPW_METRIC_TYPE__COUNT
    } NVPW_MetricType;

    typedef enum NVPW_Submetric
    {
        NVPW_SUBMETRIC_NONE = 0,
        NVPW_SUBMETRIC_PEAK_SUSTAINED = 1,
        NVPW_SUBMETRIC_PEAK_SUSTAINED_ACTIVE = 2,
        NVPW_SUBMETRIC_PEAK_SUSTAINED_ACTIVE_PER_SECOND = 3,
        NVPW_SUBMETRIC_PEAK_SUSTAINED_ELAPSED = 4,
        NVPW_SUBMETRIC_PEAK_SUSTAINED_ELAPSED_PER_SECOND = 5,
        NVPW_SUBMETRIC_PEAK_SUSTAINED_FRAME = 6,
        NVPW_SUBMETRIC_PEAK_SUSTAINED_FRAME_PER_SECOND = 7,
        NVPW_SUBMETRIC_PEAK_SUSTAINED_REGION = 8,
        NVPW_SUBMETRIC_PEAK_SUSTAINED_REGION_PER_SECOND = 9,
        NVPW_SUBMETRIC_PER_CYCLE_ACTIVE = 10,
        NVPW_SUBMETRIC_PER_CYCLE_ELAPSED = 11,
        NVPW_SUBMETRIC_PER_CYCLE_IN_FRAME = 12,
        NVPW_SUBMETRIC_PER_CYCLE_IN_REGION = 13,
        NVPW_SUBMETRIC_PER_SECOND = 14,
        NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_ACTIVE = 15,
        NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_ELAPSED = 16,
        NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_FRAME = 17,
        NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_REGION = 18,
        NVPW_SUBMETRIC_MAX_RATE = 19,
        NVPW_SUBMETRIC_PCT = 20,
        NVPW_SUBMETRIC_RATIO = 21,
        NVPW_SUBMETRIC__COUNT
    } NVPW_Submetric;

    typedef struct NVPW_MetricEvalRequest
    {
        /// the metric index as in 'NVPW_MetricsEvaluator_GetMetricNames'
        size_t metricIndex;
        /// one of 'NVPW_MetricType'
        uint8_t metricType;
        /// one of 'NVPW_RollupOp', required for Counter and Throughput, doesn't apply to Ratio
        uint8_t rollupOp;
        /// one of 'NVPW_Submetric', required for Ratio and Throughput, optional for Counter
        uint16_t submetric;
    } NVPW_MetricEvalRequest;
#define NVPW_MetricEvalRequest_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricEvalRequest, submetric)

    typedef struct NVPW_DimUnitFactor
    {
        /// one of 'NVPW_DimUnitName'
        uint32_t dimUnit;
        int8_t exponent;
    } NVPW_DimUnitFactor;
#define NVPW_DimUnitFactor_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_DimUnitFactor, exponent)

    typedef struct NVPW_MetricsEvaluator_Destroy_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
    } NVPW_MetricsEvaluator_Destroy_Params;
#define NVPW_MetricsEvaluator_Destroy_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_Destroy_Params, pMetricsEvaluator)

    NVPA_Status NVPW_MetricsEvaluator_Destroy(NVPW_MetricsEvaluator_Destroy_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_GetMetricNames_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in] one of 'NVPW_MetricType'
        uint8_t metricType;
        /// [out]
        const char* pMetricNames;
        /// [out]
        const size_t* pMetricNameBeginIndices;
        /// [out]
        size_t numMetrics;
    } NVPW_MetricsEvaluator_GetMetricNames_Params;
#define NVPW_MetricsEvaluator_GetMetricNames_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_GetMetricNames_Params, numMetrics)

    NVPA_Status NVPW_MetricsEvaluator_GetMetricNames(NVPW_MetricsEvaluator_GetMetricNames_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in] can be either a base metric or a metric
        const char* pMetricName;
        /// [out] one of 'NVPW_MetricType'
        uint8_t metricType;
        /// [out] the metric index as in 'NVPW_MetricsEvaluator_GetMetricNames'
        size_t metricIndex;
    } NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Params;
#define NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Params, metricIndex)

    NVPA_Status NVPW_MetricsEvaluator_GetMetricTypeAndIndex(NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in]
        const char* pMetricName;
        /// [inout] 'pMetricEvalRequest' is in, '*pMetricEvalRequest' is out
        struct NVPW_MetricEvalRequest* pMetricEvalRequest;
        /// [in] set to 'NVPW_MetricEvalRequest_STRUCT_SIZE'
        size_t metricEvalRequestStructSize;
    } NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params;
#define NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params, metricEvalRequestStructSize)

    NVPA_Status NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest(NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_HwUnitToString_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in] one of 'NVPW_HwUnit'
        uint32_t hwUnit;
        /// [out]
        const char* pHwUnitName;
    } NVPW_MetricsEvaluator_HwUnitToString_Params;
#define NVPW_MetricsEvaluator_HwUnitToString_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_HwUnitToString_Params, pHwUnitName)

    NVPA_Status NVPW_MetricsEvaluator_HwUnitToString(NVPW_MetricsEvaluator_HwUnitToString_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_GetCounterProperties_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in] the metric index as in 'NVPW_MetricsEvaluator_GetMetricNames'
        size_t counterIndex;
        /// [out]
        const char* pDescription;
        /// [out] one of 'NVPW_HwUnit'
        uint32_t hwUnit;
    } NVPW_MetricsEvaluator_GetCounterProperties_Params;
#define NVPW_MetricsEvaluator_GetCounterProperties_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_GetCounterProperties_Params, hwUnit)

    NVPA_Status NVPW_MetricsEvaluator_GetCounterProperties(NVPW_MetricsEvaluator_GetCounterProperties_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_GetRatioMetricProperties_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in] the metric index as in 'NVPW_MetricsEvaluator_GetMetricNames'
        size_t ratioMetricIndex;
        /// [out]
        const char* pDescription;
        /// [out]
        uint64_t hwUnit;
    } NVPW_MetricsEvaluator_GetRatioMetricProperties_Params;
#define NVPW_MetricsEvaluator_GetRatioMetricProperties_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_GetRatioMetricProperties_Params, hwUnit)

    NVPA_Status NVPW_MetricsEvaluator_GetRatioMetricProperties(NVPW_MetricsEvaluator_GetRatioMetricProperties_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_GetThroughputMetricProperties_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in] the metric index as in 'NVPW_MetricsEvaluator_GetMetricNames'
        size_t throughputMetricIndex;
        /// [out]
        const char* pDescription;
        /// [out]
        uint32_t hwUnit;
        /// [out] number of constituent counters for the throughput metric
        size_t numCounters;
        /// [out] metric indices as in 'NVPW_MetricsEvaluator_GetMetricNames', valid if 'numCounters' > 0, otherwise
        /// returned as nullptr
        const size_t* pCounterIndices;
        /// [out] number of constituent sub-throughputs for the throughput metric
        size_t numSubThroughputs;
        /// [out] metric indices as in 'NVPW_MetricsEvaluator_GetMetricNames', valid if 'numSubThroughputs' > 0,
        /// otherwise returned as nullptr
        const size_t* pSubThroughputIndices;
    } NVPW_MetricsEvaluator_GetThroughputMetricProperties_Params;
#define NVPW_MetricsEvaluator_GetThroughputMetricProperties_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_GetThroughputMetricProperties_Params, pSubThroughputIndices)

    NVPA_Status NVPW_MetricsEvaluator_GetThroughputMetricProperties(NVPW_MetricsEvaluator_GetThroughputMetricProperties_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_GetSupportedSubmetrics_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in] one of 'NVPW_MetricType'
        uint8_t metricType;
        /// [out] an array of 'NVPW_Submetric'
        const uint16_t* pSupportedSubmetrics;
        /// [out]
        size_t numSupportedSubmetrics;
    } NVPW_MetricsEvaluator_GetSupportedSubmetrics_Params;
#define NVPW_MetricsEvaluator_GetSupportedSubmetrics_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_GetSupportedSubmetrics_Params, numSupportedSubmetrics)

    NVPA_Status NVPW_MetricsEvaluator_GetSupportedSubmetrics(NVPW_MetricsEvaluator_GetSupportedSubmetrics_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_GetMetricRawDependencies_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in]
        const struct NVPW_MetricEvalRequest* pMetricEvalRequests;
        /// [in]
        size_t numMetricEvalRequests;
        /// [in] set to 'NVPW_MetricEvalRequest_STRUCT_SIZE'
        size_t metricEvalRequestStructSize;
        /// [in] set to sizeof('NVPW_MetricEvalRequest')
        size_t metricEvalRequestStrideSize;
        /// [inout] 'ppRawDependencies' is in, '*ppRawDependencies' is out
        const char** ppRawDependencies;
        /// [inout] if 'ppRawDependencies' is NULL, number of raw dependencies available will be returned; otherwise it
        /// should be set to the number of elements allocated for 'ppRawDependencies', and on return, it will be
        /// overwritten by number of elements copied to 'ppRawDependencies'
        size_t numRawDependencies;
        /// [inout] 'ppOptionalRawDependencies' is in, '*ppOptionalRawDependencies' is out
        const char** ppOptionalRawDependencies;
        /// [inout] if 'ppOptionalRawDependencies' is NULL, number of optional raw dependencies available will be
        /// returned; otherwise it should be set to the number of elements allocated for 'ppOptionalRawDependencies',
        /// and on return, it will be overwritten by number of elements copied to 'ppOptionalRawDependencies'
        size_t numOptionalRawDependencies;
    } NVPW_MetricsEvaluator_GetMetricRawDependencies_Params;
#define NVPW_MetricsEvaluator_GetMetricRawDependencies_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_GetMetricRawDependencies_Params, numOptionalRawDependencies)

    NVPA_Status NVPW_MetricsEvaluator_GetMetricRawDependencies(NVPW_MetricsEvaluator_GetMetricRawDependencies_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_DimUnitToString_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in] one of 'NVPW_DimUnitName'
        uint32_t dimUnit;
        /// [out]
        const char* pSingularName;
        /// [out]
        const char* pPluralName;
    } NVPW_MetricsEvaluator_DimUnitToString_Params;
#define NVPW_MetricsEvaluator_DimUnitToString_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_DimUnitToString_Params, pPluralName)

    NVPA_Status NVPW_MetricsEvaluator_DimUnitToString(NVPW_MetricsEvaluator_DimUnitToString_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_GetMetricDimUnits_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in]
        const struct NVPW_MetricEvalRequest* pMetricEvalRequest;
        /// [in] set to 'NVPW_MetricEvalRequest_STRUCT_SIZE'
        size_t metricEvalRequestStructSize;
        /// [inout] 'pDimUnits' is in, '*pDimUnits' is out
        NVPW_DimUnitFactor* pDimUnits;
        /// [inout] if 'pDimUnits' is NULL, number of dim-units available will be returned; otherwise it should be set
        /// to the number of elements allocated for 'pDimUnits', and on return, it will be overwritten by number of
        /// elements copied to 'pDimUnits'
        size_t numDimUnits;
        /// [in] set to 'NVPW_DimUnitFactor_STRUCT_SIZE'
        size_t dimUnitFactorStructSize;
    } NVPW_MetricsEvaluator_GetMetricDimUnits_Params;
#define NVPW_MetricsEvaluator_GetMetricDimUnits_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_GetMetricDimUnits_Params, dimUnitFactorStructSize)

    NVPA_Status NVPW_MetricsEvaluator_GetMetricDimUnits(NVPW_MetricsEvaluator_GetMetricDimUnits_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_SetUserData_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in] duration in ns of user defined frame
        double frameDuration;
        /// [in] duration in ns of user defined region
        double regionDuration;
        /// [in] reserved
        NVPA_Bool rsvd0028;
    } NVPW_MetricsEvaluator_SetUserData_Params;
#define NVPW_MetricsEvaluator_SetUserData_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_SetUserData_Params, rsvd0028)

    NVPA_Status NVPW_MetricsEvaluator_SetUserData(NVPW_MetricsEvaluator_SetUserData_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_EvaluateToGpuValues_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in]
        const struct NVPW_MetricEvalRequest* pMetricEvalRequests;
        /// [in]
        size_t numMetricEvalRequests;
        /// [in] set to 'NVPW_MetricEvalRequest_STRUCT_SIZE'
        size_t metricEvalRequestStructSize;
        /// [in] set to sizeof('NVPW_MetricEvalRequest')
        size_t metricEvalRequestStrideSize;
        /// [in]
        const uint8_t* pCounterDataImage;
        /// [in]
        size_t counterDataImageSize;
        /// [in]
        size_t rangeIndex;
        /// [in] reserved
        NVPA_Bool rsvd0050;
        /// [inout] 'pMetricValues' is in, '*pMetricValues' is out
        double* pMetricValues;
    } NVPW_MetricsEvaluator_EvaluateToGpuValues_Params;
#define NVPW_MetricsEvaluator_EvaluateToGpuValues_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_EvaluateToGpuValues_Params, pMetricValues)

    NVPA_Status NVPW_MetricsEvaluator_EvaluateToGpuValues(NVPW_MetricsEvaluator_EvaluateToGpuValues_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_SetDeviceAttributes_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in]
        const uint8_t* pCounterDataImage;
        /// [in]
        size_t counterDataImageSize;
    } NVPW_MetricsEvaluator_SetDeviceAttributes_Params;
#define NVPW_MetricsEvaluator_SetDeviceAttributes_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_SetDeviceAttributes_Params, counterDataImageSize)

    NVPA_Status NVPW_MetricsEvaluator_SetDeviceAttributes(NVPW_MetricsEvaluator_SetDeviceAttributes_Params* pParams);

/**
 *  @}
 ******************************************************************************/
 

#endif // NVPERF_HOST_API_DEFINED




#ifdef __cplusplus
} // extern "C"
#endif

#if defined(__GNUC__) && defined(NVPA_SHARED_LIB)
    #pragma GCC visibility pop
#endif

#endif // NVPERF_HOST_H
