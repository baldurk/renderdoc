#ifndef NVPERF_HOST_H
#define NVPERF_HOST_H

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
 *  @name   General Host APIs
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

/**
 *  @}
 ******************************************************************************/
 
/***************************************************************************//**
 *  @name   Raw Metrics Config. This group of APIs is deprecated and will be removed in the future. Please use the `Raw Counter Config` APIs instead.
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

/**
 *  @}
 ******************************************************************************/
 
/***************************************************************************//**
 *  @name   Raw Counter Config
 *  @{
 */

    typedef struct NVPW_RawCounterConfig NVPW_RawCounterConfig;

    /// This is a list of "Singular Counter Domains" (or "SCDs"). Note their enum values can be non-contiguous.
    typedef enum NVPW_RawCounterDomain
    {
        NVPW_RAW_COUNTER_DOMAIN_INVALID = 0,
        NVPW_RAW_COUNTER_DOMAIN_TRACE = 1,
        NVPW_RAW_COUNTER_DOMAIN_GPU_SASS = 2,
        NVPW_RAW_COUNTER_DOMAIN_GPU_SM_B = 3,
        NVPW_RAW_COUNTER_DOMAIN_GPU_SM_C = 4,
        NVPW_RAW_COUNTER_DOMAIN_GPU_CTC = 6,
        NVPW_RAW_COUNTER_DOMAIN_GPU_FBPA = 8,
        NVPW_RAW_COUNTER_DOMAIN_GPU_FBSP = 9,
        NVPW_RAW_COUNTER_DOMAIN_GPU_FE_A = 10,
        NVPW_RAW_COUNTER_DOMAIN_GPU_FE_B = 11,
        NVPW_RAW_COUNTER_DOMAIN_GPU_FE_C = 12,
        NVPW_RAW_COUNTER_DOMAIN_GPU_GPC_A = 13,
        NVPW_RAW_COUNTER_DOMAIN_GPU_GPC_B = 14,
        NVPW_RAW_COUNTER_DOMAIN_GPU_GPC_C = 15,
        NVPW_RAW_COUNTER_DOMAIN_GPU_HOST = 16,
        NVPW_RAW_COUNTER_DOMAIN_GPU_HUB = 17,
        NVPW_RAW_COUNTER_DOMAIN_GPU_HUB_A = 18,
        NVPW_RAW_COUNTER_DOMAIN_GPU_HUB_B = 19,
        NVPW_RAW_COUNTER_DOMAIN_GPU_HUB_C = 20,
        NVPW_RAW_COUNTER_DOMAIN_GPU_LTS = 23,
        NVPW_RAW_COUNTER_DOMAIN_GPU_NVLRX = 26,
        NVPW_RAW_COUNTER_DOMAIN_GPU_NVLTX = 28,
        NVPW_RAW_COUNTER_DOMAIN_GPU_PCI = 29,
        NVPW_RAW_COUNTER_DOMAIN_GPU_PWR = 30,
        NVPW_RAW_COUNTER_DOMAIN_GPU_ROP = 31,
        NVPW_RAW_COUNTER_DOMAIN_GPU_SM_A = 32,
        NVPW_RAW_COUNTER_DOMAIN_GPU_TPC = 33,
        NVPW_RAW_COUNTER_DOMAIN_SOC_MCC = 38,
        NVPW_RAW_COUNTER_DOMAIN_SOC_NVENC = 48,
        NVPW_RAW_COUNTER_DOMAIN_SOC_OFA = 50,
        NVPW_RAW_COUNTER_DOMAIN_SOC_VIC = 53,
        NVPW_RAW_COUNTER_DOMAIN_SOC_DLA = 57,
        NVPW_RAW_COUNTER_DOMAIN_SOC_PVA_A = 58,
        NVPW_RAW_COUNTER_DOMAIN_SOC_PVA_B = 59,
        NVPW_RAW_COUNTER_DOMAIN_GPU_CTC_A = 62,
        NVPW_RAW_COUNTER_DOMAIN_GPU_CTC_B = 63,
        NVPW_RAW_COUNTER_DOMAIN_GPU_SYSLTS = 74,
        NVPW_RAW_COUNTER_DOMAIN_GPU_FE_D = 75,
        NVPW_RAW_COUNTER_DOMAIN_GPU_FBP_B = 78,
        NVPW_RAW_COUNTER_DOMAIN_GPU_SYSLTS_B = 79,
        NVPW_RAW_COUNTER_DOMAIN_GPU_TPC_B = 80
    } NVPW_RawCounterDomain;

    /// This is a list of "Cooperative Domain Groups" (or "CDGs"). A CDG is a logical domain that groups multiple SCDs
    /// together, and it's used to represent cross-domain counter dependencies. That is, collecting a counter from a CDG
    /// can use resources from multiple SCDs in this CDG. Note their enum values can be non-contiguous. CDGs and SCDs
    /// are non-overlapping in enum values.
    typedef enum NVPW_RawCounterDomain_CooperativeDomainGroup
    {
        NVPW_RAW_COUNTER_DOMAIN_CDG_GPU_GPC_A_GPC_B = 1048576,
        NVPW_RAW_COUNTER_DOMAIN_CDG_GPU_TPC_SM_A = 1048577,
        NVPW_RAW_COUNTER_DOMAIN_CDG_GPU_SM_B_SM_A = 1048578,
        NVPW_RAW_COUNTER_DOMAIN_CDG_GPU_SM_B_TPC = 1048579,
        NVPW_RAW_COUNTER_DOMAIN_CDG_GPU_SM_C_SM_A = 1048580
    } NVPW_RawCounterDomain_CooperativeDomainGroup;

    typedef struct NVPW_RawCounterConfig_RawCounterDomainToString_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in] can be either a SCD or a CDG
        uint32_t domain;
        /// [out]
        const char* pDomainName;
    } NVPW_RawCounterConfig_RawCounterDomainToString_Params;
#define NVPW_RawCounterConfig_RawCounterDomainToString_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_RawCounterDomainToString_Params, pDomainName)

    /// Convenience function
    NVPA_Status NVPW_RawCounterConfig_RawCounterDomainToString(NVPW_RawCounterConfig_RawCounterDomainToString_Params* pParams);

    typedef struct NVPW_RawCounterConfig_StringToRawCounterDomain_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in] can be either a SCD or a CDG. Should match output of 'NVPW_RawCounterConfig_RawCounterDomainToString',
        /// typically in lower case and has no enum prefix.
        const char* pDomainName;
        /// [out]
        uint32_t domain;
    } NVPW_RawCounterConfig_StringToRawCounterDomain_Params;
#define NVPW_RawCounterConfig_StringToRawCounterDomain_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_StringToRawCounterDomain_Params, domain)

    /// Convenience function, reverse of 'NVPW_RawCounterConfig_RawCounterDomainToString'
    NVPA_Status NVPW_RawCounterConfig_StringToRawCounterDomain(NVPW_RawCounterConfig_StringToRawCounterDomain_Params* pParams);

    typedef struct NVPW_RawCounterConfig_GetSupportedChipNames_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [out]
        const char* const* ppChipNames;
        /// [out]
        size_t numChipNames;
    } NVPW_RawCounterConfig_GetSupportedChipNames_Params;
#define NVPW_RawCounterConfig_GetSupportedChipNames_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_GetSupportedChipNames_Params, numChipNames)

    NVPA_Status NVPW_RawCounterConfig_GetSupportedChipNames(NVPW_RawCounterConfig_GetSupportedChipNames_Params* pParams);

    typedef struct NVPW_RawCounterConfig_Destroy_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_RawCounterConfig* pRawCounterConfig;
    } NVPW_RawCounterConfig_Destroy_Params;
#define NVPW_RawCounterConfig_Destroy_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_Destroy_Params, pRawCounterConfig)

    NVPA_Status NVPW_RawCounterConfig_Destroy(NVPW_RawCounterConfig_Destroy_Params* pParams);

    typedef struct NVPW_RawCounterConfig_SetCounterAvailability_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_RawCounterConfig* pRawCounterConfig;
        /// [in] buffer with counter availability image
        const uint8_t* pCounterAvailabilityImage;
    } NVPW_RawCounterConfig_SetCounterAvailability_Params;
#define NVPW_RawCounterConfig_SetCounterAvailability_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_SetCounterAvailability_Params, pCounterAvailabilityImage)

    NVPA_Status NVPW_RawCounterConfig_SetCounterAvailability(NVPW_RawCounterConfig_SetCounterAvailability_Params* pParams);

    typedef struct NVPW_RawCounterConfig_GetAllAvailableRawCounterDomains_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const struct NVPW_RawCounterConfig* pRawCounterConfig;
        /// [inout] if 'pAvailableDomains' is NULL, number of domains available on this chip will be returned; otherwise
        /// it should be set to the number of elements allocated for 'pAvailableDomains', and on return, it will be
        /// overwritten by number of elements copied to 'pAvailableDomains'
        size_t numAvailableDomains;
        /// [inout] 'pAvailableDomains' is in, '*pAvailableDomains' is out.
        NVPW_RawCounterDomain* pAvailableDomains;
    } NVPW_RawCounterConfig_GetAllAvailableRawCounterDomains_Params;
#define NVPW_RawCounterConfig_GetAllAvailableRawCounterDomains_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_GetAllAvailableRawCounterDomains_Params, pAvailableDomains)

    /// Note for backward-compatibility, the returned domains include only SCDs. For CDG, use
    /// 'NVPW_RawCounterConfig_GetAllAvailableCooperativeDomainGroups' instead.
    NVPA_Status NVPW_RawCounterConfig_GetAllAvailableRawCounterDomains(NVPW_RawCounterConfig_GetAllAvailableRawCounterDomains_Params* pParams);

    typedef struct NVPW_RawCounterConfig_GetAllAvailableCooperativeDomainGroups_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const struct NVPW_RawCounterConfig* pRawCounterConfig;
        /// [inout] if 'pAvailableDomains' is NULL, number of domains available on this chip will be returned; otherwise
        /// it should be set to the number of elements allocated for 'pAvailableDomains', and on return, it will be
        /// overwritten by number of elements copied to 'pAvailableDomains'
        size_t numAvailableDomains;
        /// [inout] 'pAvailableDomains' is in, '*pAvailableDomains' is out.
        uint32_t* pAvailableDomains;
    } NVPW_RawCounterConfig_GetAllAvailableCooperativeDomainGroups_Params;
#define NVPW_RawCounterConfig_GetAllAvailableCooperativeDomainGroups_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_GetAllAvailableCooperativeDomainGroups_Params, pAvailableDomains)

    NVPA_Status NVPW_RawCounterConfig_GetAllAvailableCooperativeDomainGroups(NVPW_RawCounterConfig_GetAllAvailableCooperativeDomainGroups_Params* pParams);

    typedef struct NVPW_RawCounterConfig_IsCooperativeDomainGroup_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const struct NVPW_RawCounterConfig* pRawCounterConfig;
        /// [in]
        uint32_t domain;
        /// [out]
        NVPA_Bool isCdg;
    } NVPW_RawCounterConfig_IsCooperativeDomainGroup_Params;
#define NVPW_RawCounterConfig_IsCooperativeDomainGroup_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_IsCooperativeDomainGroup_Params, isCdg)

    NVPA_Status NVPW_RawCounterConfig_IsCooperativeDomainGroup(NVPW_RawCounterConfig_IsCooperativeDomainGroup_Params* pParams);

    typedef struct NVPW_RawCounterConfig_CooperativeDomainGroup_GetMemberDomains_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const struct NVPW_RawCounterConfig* pRawCounterConfig;
        /// [in] must be a CDG.
        uint32_t domain;
        /// [inout] if 'pMemberDomains' is NULL, number of member domains will be returned; otherwise it should be set
        /// to the number of elements allocated for 'pMemberDomains', and on return, it will be overwritten by number of
        /// elements copied to 'pMemberDomains'
        size_t numMemberDomains;
        /// [inout] 'pMemberDomains' is in, '*pMemberDomains' is out.
        NVPW_RawCounterDomain* pMemberDomains;
    } NVPW_RawCounterConfig_CooperativeDomainGroup_GetMemberDomains_Params;
#define NVPW_RawCounterConfig_CooperativeDomainGroup_GetMemberDomains_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_CooperativeDomainGroup_GetMemberDomains_Params, pMemberDomains)

    NVPA_Status NVPW_RawCounterConfig_CooperativeDomainGroup_GetMemberDomains(NVPW_RawCounterConfig_CooperativeDomainGroup_GetMemberDomains_Params* pParams);

    typedef struct NVPW_RawCounterConfig_BeginPassGroup_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_RawCounterConfig* pRawCounterConfig;
        size_t numDomains;
        /// [in] Each input domain must be a SCD and not be in any open pass group. A CDG is considered in an open pass
        /// group if all its member SCDs are in an open pass group.
        const NVPW_RawCounterDomain* pDomains;
    } NVPW_RawCounterConfig_BeginPassGroup_Params;
#define NVPW_RawCounterConfig_BeginPassGroup_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_BeginPassGroup_Params, pDomains)

    NVPA_Status NVPW_RawCounterConfig_BeginPassGroup(NVPW_RawCounterConfig_BeginPassGroup_Params* pParams);

    typedef struct NVPW_RawCounterConfig_EndPassGroup_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_RawCounterConfig* pRawCounterConfig;
        size_t numDomains;
        /// [in] Each input domain must be a SCD and already in an open pass group.
        const NVPW_RawCounterDomain* pDomains;
    } NVPW_RawCounterConfig_EndPassGroup_Params;
#define NVPW_RawCounterConfig_EndPassGroup_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_EndPassGroup_Params, pDomains)

    /// By default, scheduler will pack passes generated out of those specified domains from a single EndPassGroup call
    /// altogether. If there are multiple EndPassGroup calls on different sets of domains, one can choose to later use
    /// 'NVPW_RawCounterConfig_MergePassGroups' to unpack and repack all previously scheduled counter domains in order
    /// to yield less number of passes at the cost of mutating previous mapping of counter domains and their passes.
    /// It's also fine to end a domain's pass group without adding any counters, for example, just use it for counter
    /// enumeration or property query.
    NVPA_Status NVPW_RawCounterConfig_EndPassGroup(NVPW_RawCounterConfig_EndPassGroup_Params* pParams);

    typedef struct NVPW_RawCounterConfig_GetNumRawCounters_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const struct NVPW_RawCounterConfig* pRawCounterConfig;
        /// [in] can be either a SCD or a CDG. There must be an open pass group for this counter domain.
        uint32_t domain;
        /// [out] Total number of raw counters in the counter domain
        size_t numRawCounters;
    } NVPW_RawCounterConfig_GetNumRawCounters_Params;
#define NVPW_RawCounterConfig_GetNumRawCounters_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_GetNumRawCounters_Params, numRawCounters)

    NVPA_Status NVPW_RawCounterConfig_GetNumRawCounters(NVPW_RawCounterConfig_GetNumRawCounters_Params* pParams);

    typedef struct NVPW_RawCounterConfig_GetRawCounterName_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const struct NVPW_RawCounterConfig* pRawCounterConfig;
        /// [in] can be either a SCD or a CDG. There must be an open pass group for this counter domain.
        uint32_t domain;
        /// [in] index within the counter domain
        size_t rawCounterIndex;
        /// [out]
        const char* pRawCounterName;
    } NVPW_RawCounterConfig_GetRawCounterName_Params;
#define NVPW_RawCounterConfig_GetRawCounterName_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_GetRawCounterName_Params, pRawCounterName)

    NVPA_Status NVPW_RawCounterConfig_GetRawCounterName(NVPW_RawCounterConfig_GetRawCounterName_Params* pParams);

    typedef struct NVPW_RawCounterConfig_GetRawCounterProperties_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const struct NVPW_RawCounterConfig* pRawCounterConfig;
        /// [in] This raw counter must be available from one of the open pass groups.
        const char* pRawCounterName;
        /// [inout] if 'pSupportedDomains' is NULL, number of SCD domains will be returned; otherwise it should be set
        /// to the number of elements allocated for 'pSupportedDomains', and on return, it will be overwritten by number
        /// of elements copied to 'pSupportedDomains'
        size_t numSupportedDomains;
        /// [inout] 'pSupportedDomains' is in, '*pSupportedDomains' is out. These will be restricted to only SCDs in an
        /// open passs group.
        NVPW_RawCounterDomain* pSupportedDomains;
        /// [inout] if 'pSupportedCdgDomains' is NULL, number of CDG domains will be returned; otherwise it should be
        /// set to the number of elements allocated for 'pSupportedCdgDomains', and on return, it will be overwritten by
        /// number of elements copied to 'pSupportedCdgDomains'
        size_t numSupportedCdgDomains;
        /// [inout] 'pSupportedCdgDomains' is in, '*pSupportedCdgDomains' is out. These will be restricted to only CDGs
        /// in an open passs group.
        uint32_t* pSupportedCdgDomains;
    } NVPW_RawCounterConfig_GetRawCounterProperties_Params;
#define NVPW_RawCounterConfig_GetRawCounterProperties_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_GetRawCounterProperties_Params, pSupportedCdgDomains)

    /// The properties include both SCDs and CDGs that can be used to collect this raw counter. SCDs are returned via
    /// `pRawCounterName` and `pSupportedDomains`, while CDGs are returned via `numSupportedCdgDomains` and
    /// `pSupportedCdgDomains`.
    NVPA_Status NVPW_RawCounterConfig_GetRawCounterProperties(NVPW_RawCounterConfig_GetRawCounterProperties_Params* pParams);

    typedef struct NVPW_RawCounterRequest
    {
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const char* pRawCounterName;
        /// [in] can be either a SCD or a CDG, this domain must be in an open pass group. Set it to
        /// 'NVPW_RAW_COUNTER_DOMAIN_INVALID' to have it automatically determined by the scheduler. This param is
        /// ignored by CounterDataBuilder but observed by AddRawCounters.
        uint32_t domain;
        /// [in] ignored by AddRawCounters but observed by CounterDataBuilder
        NVPA_Bool keepInstances;
    } NVPW_RawCounterRequest;
#define NVPW_RAW_COUNTER_REQUEST_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterRequest, keepInstances)

    typedef struct NVPW_RawCounterConfig_AddRawCounters_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_RawCounterConfig* pRawCounterConfig;
        /// [in] set to 'NVPW_RAW_COUNTER_REQUEST_STRUCT_SIZE'
        size_t rawCounterRequestStructSize;
        /// [in]
        size_t numRawCounterRequests;
        /// [in]
        const NVPW_RawCounterRequest* pRawCounterRequests;
    } NVPW_RawCounterConfig_AddRawCounters_Params;
#define NVPW_RawCounterConfig_AddRawCounters_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_AddRawCounters_Params, pRawCounterRequests)

    NVPA_Status NVPW_RawCounterConfig_AddRawCounters(NVPW_RawCounterConfig_AddRawCounters_Params* pParams);

    typedef struct NVPW_RawCounterConfig_AreRawCountersSchedulable_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const struct NVPW_RawCounterConfig* pRawCounterConfig;
        /// [in] set to 'NVPW_RAW_COUNTER_REQUEST_STRUCT_SIZE'
        size_t rawCounterRequestStructSize;
        /// [in]
        size_t numRawCounterRequests;
        /// [in]
        const NVPW_RawCounterRequest* pRawCounterRequests;
        /// [in] if true, `schedulable` is only set to true if the raw counters can be scheduled without adding extra
        /// passes. This can be useful for testing raw counters during single pass scheduling. Note: Enabling this
        /// option can be computationally expensive.
        NVPA_Bool disallowAddingExtraPasses;
        /// [out]
        NVPA_Bool schedulable;
    } NVPW_RawCounterConfig_AreRawCountersSchedulable_Params;
#define NVPW_RawCounterConfig_AreRawCountersSchedulable_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_AreRawCountersSchedulable_Params, schedulable)

    NVPA_Status NVPW_RawCounterConfig_AreRawCountersSchedulable(NVPW_RawCounterConfig_AreRawCountersSchedulable_Params* pParams);

    typedef struct NVPW_RawCounterConfig_MergePassGroups_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_RawCounterConfig* pRawCounterConfig;
    } NVPW_RawCounterConfig_MergePassGroups_Params;
#define NVPW_RawCounterConfig_MergePassGroups_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_MergePassGroups_Params, pRawCounterConfig)

    /// All existing passes' counter domains will be unpacked and repacked in an attempt to reduce number of passes.
    /// This API can only be called if there is no open pass group.
    NVPA_Status NVPW_RawCounterConfig_MergePassGroups(NVPW_RawCounterConfig_MergePassGroups_Params* pParams);

    typedef struct NVPW_RawCounterConfig_GenerateConfigImage_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_RawCounterConfig* pRawCounterConfig;
    } NVPW_RawCounterConfig_GenerateConfigImage_Params;
#define NVPW_RawCounterConfig_GenerateConfigImage_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_GenerateConfigImage_Params, pRawCounterConfig)

    NVPA_Status NVPW_RawCounterConfig_GenerateConfigImage(NVPW_RawCounterConfig_GenerateConfigImage_Params* pParams);

    typedef struct NVPW_RawCounterConfig_GetConfigImage_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const struct NVPW_RawCounterConfig* pRawCounterConfig;
        /// [in] Number of bytes allocated for pBuffer
        size_t bytesAllocated;
        /// [in] If not NULL, it must point to a buffer receiving the config image
        uint8_t* pBuffer;
        /// [out] Count of bytes that would be copied into pBuffer
        size_t bytesCopied;
    } NVPW_RawCounterConfig_GetConfigImage_Params;
#define NVPW_RawCounterConfig_GetConfigImage_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_GetConfigImage_Params, bytesCopied)

    NVPA_Status NVPW_RawCounterConfig_GetConfigImage(NVPW_RawCounterConfig_GetConfigImage_Params* pParams);

    typedef struct NVPW_RawCounterConfig_GetNumPasses_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const struct NVPW_RawCounterConfig* pRawCounterConfig;
        /// [out]
        size_t numPasses;
    } NVPW_RawCounterConfig_GetNumPasses_Params;
#define NVPW_RawCounterConfig_GetNumPasses_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_RawCounterConfig_GetNumPasses_Params, numPasses)

    NVPA_Status NVPW_RawCounterConfig_GetNumPasses(NVPW_RawCounterConfig_GetNumPasses_Params* pParams);

/**
 *  @}
 ******************************************************************************/
 
/***************************************************************************//**
 *  @name   General Counter Config
 *  @{
 */

    typedef struct NVPW_Config_GetRawCounterInfo_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const uint8_t* pConfig;
        /// [in]
        size_t configSize;
        /// [in]
        const char* pRawCounterName;
        /// [inout] array containing indices of passes the counter resides in. 'pPassIndices' is in, '*pPassIndices' is
        /// out.
        size_t* pPassIndices;
        /// [inout] if 'pPassIndices' is NULL, the count of passes this counter resides in will be returned; otherwise
        /// it should be set to the capacity of 'pPassIndices' array, and on return, it will be overwritten to reflect
        /// the actual count filled into 'pPassIndices'
        size_t numPassIndices;
    } NVPW_Config_GetRawCounterInfo_Params;
#define NVPW_Config_GetRawCounterInfo_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_Config_GetRawCounterInfo_Params, numPassIndices)

    NVPA_Status NVPW_Config_GetRawCounterInfo(NVPW_Config_GetRawCounterInfo_Params* pParams);

    typedef struct NVPW_Config_GetRawCounters_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const uint8_t* pConfig;
        /// [in]
        size_t configSize;
        /// [in]
        size_t passIndex;
        /// [inout] array containing raw counter names. 'ppRawCounterNames' is in, '*ppRawCounterNames' is out.
        const char** ppRawCounterNames;
        /// [inout] if 'ppRawCounterNames' is NULL, the count of raw counters will be returned; otherwise it should be
        /// set to the capacity of 'ppRawCounterNames' array, and on return, it will be overwritten to reflect the
        /// actual count filled into 'ppRawCounterNames'
        size_t numRawCounters;
    } NVPW_Config_GetRawCounters_Params;
#define NVPW_Config_GetRawCounters_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_Config_GetRawCounters_Params, numRawCounters)

    NVPA_Status NVPW_Config_GetRawCounters(NVPW_Config_GetRawCounters_Params* pParams);

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
        NVPW_DIM_UNIT_CTC_CYCLES = 2224883873,
        NVPW_DIM_UNIT_DRAM_CYCLES = 2650981327,
        NVPW_DIM_UNIT_FBP_CYCLES = 1785238957,
        NVPW_DIM_UNIT_FE_OPS = 2919159083,
        NVPW_DIM_UNIT_GPC_CYCLES = 1222631184,
        NVPW_DIM_UNIT_IDC_REQUESTS = 2012649669,
        NVPW_DIM_UNIT_INSTRUCTIONS = 1418625543,
        NVPW_DIM_UNIT_KILOBYTES = 1335980302,
        NVPW_DIM_UNIT_L1DATA_BANK_ACCESSES = 1479493682,
        NVPW_DIM_UNIT_L1DATA_BANK_CONFLICTS = 3433170787,
        NVPW_DIM_UNIT_L1TEX_LINES = 1899735838,
        NVPW_DIM_UNIT_L1TEX_REQUESTS = 1306473767,
        NVPW_DIM_UNIT_L1TEX_TAGS = 26573010,
        NVPW_DIM_UNIT_L1TEX_WAVEFRONTS = 129373765,
        NVPW_DIM_UNIT_L2_REQUESTS = 1143695106,
        NVPW_DIM_UNIT_L2_SECTORS = 3424101564,
        NVPW_DIM_UNIT_L2_TAGS = 3755612781,
        NVPW_DIM_UNIT_LRC_REQUESTS = 2280914327,
        NVPW_DIM_UNIT_LRC_SECTORS = 7212034,
        NVPW_DIM_UNIT_MCC_CYCLES = 1826685787,
        NVPW_DIM_UNIT_NANOSECONDS = 3047500672,
        NVPW_DIM_UNIT_NVDLA_CYCLES = 3374059789,
        NVPW_DIM_UNIT_NVENC_CYCLES = 2267185244,
        NVPW_DIM_UNIT_NVLRX_CYCLES = 4059934930,
        NVPW_DIM_UNIT_NVLTX_CYCLES = 1814350488,
        NVPW_DIM_UNIT_OFA_CYCLES = 4290210307,
        NVPW_DIM_UNIT_PCIE_CYCLES = 1230450943,
        NVPW_DIM_UNIT_PERCENT = 1284354694,
        NVPW_DIM_UNIT_PIXELS = 4227616663,
        NVPW_DIM_UNIT_PIXEL_SHADER_BARRIERS = 3705502518,
        NVPW_DIM_UNIT_PRIMITIVES = 2373084002,
        NVPW_DIM_UNIT_PVAVPU_CYCLES = 2238259366,
        NVPW_DIM_UNIT_PVA_CYCLES = 202044173,
        NVPW_DIM_UNIT_QUADS = 1539753497,
        NVPW_DIM_UNIT_REGISTERS = 2837260947,
        NVPW_DIM_UNIT_RF_QUANTA = 2083886015,
        NVPW_DIM_UNIT_SAMPLES = 746046551,
        NVPW_DIM_UNIT_SECONDS = 1164825258,
        NVPW_DIM_UNIT_SYSL2_REQUESTS = 2165109286,
        NVPW_DIM_UNIT_SYSL2_SECTORS = 2268734175,
        NVPW_DIM_UNIT_SYSL2_TAGS = 3308651352,
        NVPW_DIM_UNIT_SYSLRC_REQUESTS = 3328245480,
        NVPW_DIM_UNIT_SYSLRC_SECTORS = 1190477493,
        NVPW_DIM_UNIT_SYS_CYCLES = 3310821688,
        NVPW_DIM_UNIT_TEXELS = 1293214069,
        NVPW_DIM_UNIT_THREADS = 164261907,
        NVPW_DIM_UNIT_TMEM_ACCESSES = 3742902067,
        NVPW_DIM_UNIT_VERTICES = 1873662209,
        NVPW_DIM_UNIT_VIC_CYCLES = 103143588,
        NVPW_DIM_UNIT_WARPS = 97951949,
        NVPW_DIM_UNIT_WORKIDS = 1971113483,
        NVPW_DIM_UNIT_WORKLOADS = 1728142656
    } NVPW_DimUnitName;
#endif //NVPW_DIM_UNIT_DEFINED

#ifndef NVPW_HW_UNIT_DEFINED
#define NVPW_HW_UNIT_DEFINED
    typedef enum NVPW_HwUnit
    {
        NVPW_HW_UNIT_INVALID = 3498035701,
        NVPW_HW_UNIT_CROP = 2872137846,
        NVPW_HW_UNIT_CTC = 4123164475,
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
        NVPW_HW_UNIT_LRC = 4004756136,
        NVPW_HW_UNIT_LTS = 2333266697,
        NVPW_HW_UNIT_MCC = 3980130194,
        NVPW_HW_UNIT_NVDLA = 4201167892,
        NVPW_HW_UNIT_NVENC = 207708260,
        NVPW_HW_UNIT_NVLRX = 3091684901,
        NVPW_HW_UNIT_NVLTX = 869679659,
        NVPW_HW_UNIT_OFA = 70307371,
        NVPW_HW_UNIT_PCIE = 3433264174,
        NVPW_HW_UNIT_PDA = 345193251,
        NVPW_HW_UNIT_PES = 804128425,
        NVPW_HW_UNIT_PROP = 3339255507,
        NVPW_HW_UNIT_PVA = 2565499490,
        NVPW_HW_UNIT_PVAVPU = 1656645655,
        NVPW_HW_UNIT_RASTER = 187932504,
        NVPW_HW_UNIT_SM = 724224710,
        NVPW_HW_UNIT_SMSP = 2837616917,
        NVPW_HW_UNIT_SYS = 768990063,
        NVPW_HW_UNIT_SYSLRC = 3247626950,
        NVPW_HW_UNIT_SYSLTS = 4137740217,
        NVPW_HW_UNIT_TPC = 1889024613,
        NVPW_HW_UNIT_VAF = 753670509,
        NVPW_HW_UNIT_VIC = 322439594,
        NVPW_HW_UNIT_VPC = 275561583,
        NVPW_HW_UNIT_ZCULL = 2401248356,
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

    /// This API only works for enumerating predefined metrics.
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


    /// The interpreter's standard output and standard error are redirected to clients through the provided callbacks.
    /// The lifetime of these callbacks must persist through all calls to Execute().
    typedef void (*NVPW_MetricsEvaluator_UserDefinedMetrics_OutputCallback)(void* pUserData, const char* pOutput);

    typedef struct NVPW_MetricsEvaluator_UserDefinedMetrics_Initialize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in] used in output callbacks
        void* pOutputUserData;
        /// [in]
        NVPW_MetricsEvaluator_UserDefinedMetrics_OutputCallback stdoutCallback;
        /// [in]
        NVPW_MetricsEvaluator_UserDefinedMetrics_OutputCallback stderrCallback;
    } NVPW_MetricsEvaluator_UserDefinedMetrics_Initialize_Params;
#define NVPW_MetricsEvaluator_UserDefinedMetrics_Initialize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_UserDefinedMetrics_Initialize_Params, stderrCallback)

    /// The user-defined metrics feature can be considered an optional feature on top of the existing
    /// `NVPW_MetricsEvaluator*` API object. By default, when a `NVPW_MetricsEvaluator*` object is created, no data
    /// related to user-defined metrics will be initialized, so that clients who do not use this feature incur no extra
    /// cost. When this API is called, the library will then allocate and initialize data to prepare for future user-
    /// defined metrics configurations. Please note that user-defined metrics' heap usage is NOT through the fixed
    /// preallocated buffer `pScratchBuffer` clients passed in at the initial creation of `NVPW_MetricsEvaluator*`
    /// object, but instead directly allocated and managed by the library based on need. For destruction, since all the
    /// data of user-defined metrics is attached to a `NVPW_MetricsEvaluator*` object, when that
    /// `NVPW_MetricsEvaluator*` object is destroyed through `NVPW_MetricsEvaluator_Destroy`, all the memory used by
    /// user-defined metrics is automatically released with it too.
    NVPA_Status NVPW_MetricsEvaluator_UserDefinedMetrics_Initialize(NVPW_MetricsEvaluator_UserDefinedMetrics_Initialize_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_UserDefinedMetrics_ExecuteScriptFromString_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
        /// [in]
        const char* pScriptString;
        /// [in] if 0, 'pScriptString' must be null-terminated, and the length will be computed internally
        size_t scriptStringLength;
    } NVPW_MetricsEvaluator_UserDefinedMetrics_ExecuteScriptFromString_Params;
#define NVPW_MetricsEvaluator_UserDefinedMetrics_ExecuteScriptFromString_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_UserDefinedMetrics_ExecuteScriptFromString_Params, scriptStringLength)

    /// This allows users to define metrics using a Lua-based script, more syntax details can be found in the
    /// documentation. The stdout and stderr output will be through the callbacks registered at Initialize(). An error
    /// code will be returned upon execution failures. Note this API takes a string as input. For executing a script
    /// file on disk, it's expected to be read into a string first in client's code before feeding into the API. Clients
    /// can potentially execute multiple script strings for defining metrics. All metrics configured in the script will
    /// be isolated to this `NVPW_MetricsEvaluator*` instance, i.e., metrics defined in one `NVPW_MetricsEvaluator*`
    /// instance will not be visible from another object.
    NVPA_Status NVPW_MetricsEvaluator_UserDefinedMetrics_ExecuteScriptFromString(NVPW_MetricsEvaluator_UserDefinedMetrics_ExecuteScriptFromString_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_UserDefinedMetrics_Commit_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
    } NVPW_MetricsEvaluator_UserDefinedMetrics_Commit_Params;
#define NVPW_MetricsEvaluator_UserDefinedMetrics_Commit_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_UserDefinedMetrics_Commit_Params, pMetricsEvaluator)

    /// This API finalizes configurations after defining metrics. It will perform any expensive operations only after
    /// all metrics are defined. Between an Execute() and a Commit(), the behaviors of APIs of enumerating user defined
    /// metrics, querying properties of user defined metrics, evaluating the values of user defined metrics, are
    /// undefined. Note that leaving user-defined metrics in an unstaged state does not affect the functionality of
    /// predefined metrics. APIs related to predefined metrics are expected to work normally. If after a Commit(), new
    /// user-defined metrics are added or existing user-defined metrics are adjusted through Execute(), a re-commit is
    /// required.
    NVPA_Status NVPW_MetricsEvaluator_UserDefinedMetrics_Commit(NVPW_MetricsEvaluator_UserDefinedMetrics_Commit_Params* pParams);

    typedef struct NVPW_MetricsEvaluator_UserDefinedMetrics_GetMetricNames_Params
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
    } NVPW_MetricsEvaluator_UserDefinedMetrics_GetMetricNames_Params;
#define NVPW_MetricsEvaluator_UserDefinedMetrics_GetMetricNames_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_MetricsEvaluator_UserDefinedMetrics_GetMetricNames_Params, numMetrics)

    /// This API only works for enumerating user-defined metrics. For enumerating predefined metrics, use
    /// `NVPW_MetricsEvaluator_GetMetricNames` instead. Pointers in the output are only valid after Commit() and will be
    /// invalidated after new calls to Execute().
    NVPA_Status NVPW_MetricsEvaluator_UserDefinedMetrics_GetMetricNames(NVPW_MetricsEvaluator_UserDefinedMetrics_GetMetricNames_Params* pParams);

/**
 *  @}
 ******************************************************************************/
 
/***************************************************************************//**
 *  @name   Counter Data
 *  @{
 */

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

    typedef struct NVPW_CounterData_ExtractCounterDataPrefix_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// The source buffer to extract the prefix from.
        const uint8_t* pCounterDataSrc;
        size_t counterDataSrcSize;
        /// [in] If not NULL, the prefix will be copied into this buffer.
        uint8_t* pCounterDataPrefix;
        /// [inout] if 'pCounterDataPrefix' is NULL, size of counter data prefix will be returned; otherwise it should
        /// be set to the size of buffer allocated for 'pCounterDataPrefix'.
        size_t counterDataPrefixSize;
    } NVPW_CounterData_ExtractCounterDataPrefix_Params;
#define NVPW_CounterData_ExtractCounterDataPrefix_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterData_ExtractCounterDataPrefix_Params, counterDataPrefixSize)

    NVPA_Status NVPW_CounterData_ExtractCounterDataPrefix(NVPW_CounterData_ExtractCounterDataPrefix_Params* pParams);

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

    /// This API is deprecated and will be removed in the future. Please use `NVPW_CounterDataBuilder_AddRawCounters`
    /// instead.
    NVPA_Status NVPW_CounterDataBuilder_AddMetrics(NVPW_CounterDataBuilder_AddMetrics_Params* pParams);

    typedef struct NVPW_CounterDataBuilder_AddRawCounters_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        NVPA_CounterDataBuilder* pCounterDataBuilder;
        /// [in] set to 'NVPW_RAW_COUNTER_REQUEST_STRUCT_SIZE'
        size_t rawCounterRequestStructSize;
        /// [in]
        size_t numRawCounterRequests;
        /// [in]
        const NVPW_RawCounterRequest* pRawCounterRequests;
    } NVPW_CounterDataBuilder_AddRawCounters_Params;
#define NVPW_CounterDataBuilder_AddRawCounters_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_CounterDataBuilder_AddRawCounters_Params, pRawCounterRequests)

    NVPA_Status NVPW_CounterDataBuilder_AddRawCounters(NVPW_CounterDataBuilder_AddRawCounters_Params* pParams);

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
 

#endif // NVPERF_HOST_API_DEFINED




#ifdef __cplusplus
} // extern "C"
#endif

#if defined(__GNUC__) && defined(NVPA_SHARED_LIB)
    #pragma GCC visibility pop
#endif

#endif // NVPERF_HOST_H
