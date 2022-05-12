#ifndef NVPERF_D3D12_HOST_H
#define NVPERF_D3D12_HOST_H

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
#include "nvperf_host.h"

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
 *  @file   nvperf_d3d12_host.h
 */

    typedef struct NVPW_D3D12_RawMetricsConfig_Create_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        NVPA_ActivityKind activityKind;
        /// [in]
        const char* pChipName;
        /// [out] new NVPA_RawMetricsConfig object
        struct NVPA_RawMetricsConfig* pRawMetricsConfig;
    } NVPW_D3D12_RawMetricsConfig_Create_Params;
#define NVPW_D3D12_RawMetricsConfig_Create_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D12_RawMetricsConfig_Create_Params, pRawMetricsConfig)

    NVPA_Status NVPW_D3D12_RawMetricsConfig_Create(NVPW_D3D12_RawMetricsConfig_Create_Params* pParams);

    typedef struct NVPW_MetricsEvaluator NVPW_MetricsEvaluator;

    typedef struct NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        const char* pChipName;
        /// [out]
        size_t scratchBufferSize;
    } NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Params;
#define NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Params, scratchBufferSize)

    NVPA_Status NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize(NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Params* pParams);

    typedef struct NVPW_D3D12_MetricsEvaluator_Initialize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        uint8_t* pScratchBuffer;
        /// [in] the size of the 'pScratchBuffer' array, should be at least the size of the 'scratchBufferSize' returned
        /// by 'NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize'
        size_t scratchBufferSize;
        /// [in] use either 'pChipName' or 'pCounterDataImage', 'pChipName' will create the metrics evaluator based on a
        /// virtual device while 'pCounterDataImage' will create the metrics evaluator based on the actual device. If
        /// both are provided, 'pCounterDataImage' will be used
        const char* pChipName;
        /// [in]
        const uint8_t* pCounterDataImage;
        /// [in] must be provided if 'pCounterDataImage' is not NULL
        size_t counterDataImageSize;
        /// [out]
        struct NVPW_MetricsEvaluator* pMetricsEvaluator;
    } NVPW_D3D12_MetricsEvaluator_Initialize_Params;
#define NVPW_D3D12_MetricsEvaluator_Initialize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D12_MetricsEvaluator_Initialize_Params, pMetricsEvaluator)

    NVPA_Status NVPW_D3D12_MetricsEvaluator_Initialize(NVPW_D3D12_MetricsEvaluator_Initialize_Params* pParams);



#ifdef __cplusplus
} // extern "C"
#endif

#if defined(__GNUC__) && defined(NVPA_SHARED_LIB)
    #pragma GCC visibility pop
#endif

#endif // NVPERF_D3D12_HOST_H
