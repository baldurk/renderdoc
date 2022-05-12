#ifndef NVPERF_D3D11_TARGET_H
#define NVPERF_D3D11_TARGET_H

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
 *  @file   nvperf_d3d11_target.h
 */

/***************************************************************************//**
 *  @name   External Types
 *  @{
 */


    struct ID3D11DeviceContext;
    struct ID3D11Device;
    typedef struct _LUID LUID;


/**
 *  @}
 ******************************************************************************/
 
    typedef struct NVPW_D3D11_Profiler_CounterDataImageOptions
    {
        /// [in]
        size_t structSize;
        /// The CounterDataPrefix generated from e.g. NVPW_CounterDataBuilder_GetCounterDataPrefix().  Must be align(8).
        const uint8_t* pCounterDataPrefix;
        size_t counterDataPrefixSize;
        /// max number of ranges that can be specified
        uint32_t maxNumRanges;
        /// max number of RangeTree nodes; must be >= maxNumRanges
        uint32_t maxNumRangeTreeNodes;
        /// max string length of each RangeName, including the trailing NUL character
        uint32_t maxRangeNameLength;
    } NVPW_D3D11_Profiler_CounterDataImageOptions;
#define NVPW_D3D11_Profiler_CounterDataImageOptions_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_CounterDataImageOptions, maxRangeNameLength)

    typedef struct NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t counterDataImageOptionsSize;
        /// [in]
        const NVPW_D3D11_Profiler_CounterDataImageOptions* pOptions;
        /// [out]
        size_t counterDataImageSize;
    } NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Params;
#define NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Params, counterDataImageSize)

    NVPA_Status NVPW_D3D11_Profiler_CounterDataImage_CalculateSize(NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_CounterDataImage_Initialize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t counterDataImageOptionsSize;
        /// [in]
        const NVPW_D3D11_Profiler_CounterDataImageOptions* pOptions;
        /// [in]
        size_t counterDataImageSize;
        /// [in] The buffer to be written.
        uint8_t* pCounterDataImage;
    } NVPW_D3D11_Profiler_CounterDataImage_Initialize_Params;
#define NVPW_D3D11_Profiler_CounterDataImage_Initialize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_CounterDataImage_Initialize_Params, pCounterDataImage)

    NVPA_Status NVPW_D3D11_Profiler_CounterDataImage_Initialize(NVPW_D3D11_Profiler_CounterDataImage_Initialize_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t counterDataImageSize;
        /// [in]
        uint8_t* pCounterDataImage;
        /// [out]
        size_t counterDataScratchBufferSize;
    } NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Params;
#define NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Params, counterDataScratchBufferSize)

    NVPA_Status NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize(NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t counterDataImageSize;
        /// [in]
        uint8_t* pCounterDataImage;
        /// [in]
        size_t counterDataScratchBufferSize;
        /// [in] The scratch buffer to be written.
        uint8_t* pCounterDataScratchBuffer;
    } NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Params;
#define NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Params, pCounterDataScratchBuffer)

    NVPA_Status NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer(NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams);

    typedef struct NVPW_D3D11_LoadDriver_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
    } NVPW_D3D11_LoadDriver_Params;
#define NVPW_D3D11_LoadDriver_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_LoadDriver_Params, pPriv)

    NVPA_Status NVPW_D3D11_LoadDriver(NVPW_D3D11_LoadDriver_Params* pParams);

    typedef struct NVPW_D3D11_GetLUID_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [out]
        LUID* luid;
    } NVPW_D3D11_GetLUID_Params;
#define NVPW_D3D11_GetLUID_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_GetLUID_Params, luid)

    NVPA_Status NVPW_D3D11_GetLUID(NVPW_D3D11_GetLUID_Params* pParams);

    typedef struct NVPW_D3D11_Device_GetDeviceIndex_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct ID3D11Device* pDevice;
        /// [in]
        size_t sliIndex;
        /// [out]
        size_t deviceIndex;
    } NVPW_D3D11_Device_GetDeviceIndex_Params;
#define NVPW_D3D11_Device_GetDeviceIndex_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Device_GetDeviceIndex_Params, deviceIndex)

    NVPA_Status NVPW_D3D11_Device_GetDeviceIndex(NVPW_D3D11_Device_GetDeviceIndex_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_CalcTraceBufferSize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in] Maximum number of Push/Pop pairs that can be recorded in a single pass.
        size_t maxRangesPerPass;
        /// [in] for sizing internal buffers
        size_t avgRangeNameLength;
        /// [out] TraceBuffer size for a single pass.  Pass this to
        /// NVPW_D3D11_Profiler_BeginSession_Params::traceBufferSize.
        size_t traceBufferSize;
    } NVPW_D3D11_Profiler_CalcTraceBufferSize_Params;
#define NVPW_D3D11_Profiler_CalcTraceBufferSize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_CalcTraceBufferSize_Params, traceBufferSize)

    NVPA_Status NVPW_D3D11_Profiler_CalcTraceBufferSize(NVPW_D3D11_Profiler_CalcTraceBufferSize_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_DeviceContext_BeginSession_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct ID3D11DeviceContext* pDeviceContext;
        /// [in] Set to 1 if every pass is synchronized with CPU; for asynchronous collection, increase to
        /// (softwarePipelineDepth + 2).
        size_t numTraceBuffers;
        /// [in] Size of the per-pass TraceBuffer in bytes.  The profiler allocates a numTraceBuffers * traceBufferSize
        /// internally.
        size_t traceBufferSize;
        /// [in] Maximum number of ranges that can be recorded in a single pass. This argument must be greater than 0.
        size_t maxRangesPerPass;
        /// [in] UNUSED
        size_t maxLaunchesPerPass;
    } NVPW_D3D11_Profiler_DeviceContext_BeginSession_Params;
#define NVPW_D3D11_Profiler_DeviceContext_BeginSession_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_DeviceContext_BeginSession_Params, maxLaunchesPerPass)

    NVPA_Status NVPW_D3D11_Profiler_DeviceContext_BeginSession(NVPW_D3D11_Profiler_DeviceContext_BeginSession_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_DeviceContext_EndSession_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct ID3D11DeviceContext* pDeviceContext;
    } NVPW_D3D11_Profiler_DeviceContext_EndSession_Params;
#define NVPW_D3D11_Profiler_DeviceContext_EndSession_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_DeviceContext_EndSession_Params, pDeviceContext)

    NVPA_Status NVPW_D3D11_Profiler_DeviceContext_EndSession(NVPW_D3D11_Profiler_DeviceContext_EndSession_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_DeviceContext_SetConfig_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct ID3D11DeviceContext* pDeviceContext;
        /// [in] Config created by e.g. NVPW_RawMetricsConfig_GetConfigImage().  Must be align(8).
        const uint8_t* pConfig;
        size_t configSize;
        /// [in] the lowest nesting level to be profiled; must be >= 1
        uint16_t minNestingLevel;
        /// [in] the number of nesting levels to profile; must be >= 1
        uint16_t numNestingLevels;
        /// [in] Set this to zero for in-app replay.  Set this to the output of EndPass() for application replay.
        size_t passIndex;
        /// [in] Set this to minNestingLevel for in-app replay.  Set this to the output of EndPass() for application
        /// replay.
        uint16_t targetNestingLevel;
    } NVPW_D3D11_Profiler_DeviceContext_SetConfig_Params;
#define NVPW_D3D11_Profiler_DeviceContext_SetConfig_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_DeviceContext_SetConfig_Params, targetNestingLevel)

    NVPA_Status NVPW_D3D11_Profiler_DeviceContext_SetConfig(NVPW_D3D11_Profiler_DeviceContext_SetConfig_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_DeviceContext_ClearConfig_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct ID3D11DeviceContext* pDeviceContext;
    } NVPW_D3D11_Profiler_DeviceContext_ClearConfig_Params;
#define NVPW_D3D11_Profiler_DeviceContext_ClearConfig_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_DeviceContext_ClearConfig_Params, pDeviceContext)

    NVPA_Status NVPW_D3D11_Profiler_DeviceContext_ClearConfig(NVPW_D3D11_Profiler_DeviceContext_ClearConfig_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_DeviceContext_BeginPass_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct ID3D11DeviceContext* pDeviceContext;
    } NVPW_D3D11_Profiler_DeviceContext_BeginPass_Params;
#define NVPW_D3D11_Profiler_DeviceContext_BeginPass_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_DeviceContext_BeginPass_Params, pDeviceContext)

    NVPA_Status NVPW_D3D11_Profiler_DeviceContext_BeginPass(NVPW_D3D11_Profiler_DeviceContext_BeginPass_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_DeviceContext_EndPass_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct ID3D11DeviceContext* pDeviceContext;
    } NVPW_D3D11_Profiler_DeviceContext_EndPass_Params;
#define NVPW_D3D11_Profiler_DeviceContext_EndPass_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_DeviceContext_EndPass_Params, pDeviceContext)

    NVPA_Status NVPW_D3D11_Profiler_DeviceContext_EndPass(NVPW_D3D11_Profiler_DeviceContext_EndPass_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_DeviceContext_PushRange_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct ID3D11DeviceContext* pDeviceContext;
        /// [in] specifies the range that subsequent launches' counters will be assigned to; must not be NULL
        const char* pRangeName;
        /// [in] assign to strlen(pRangeName) if known; if set to zero, the library will call strlen()
        size_t rangeNameLength;
    } NVPW_D3D11_Profiler_DeviceContext_PushRange_Params;
#define NVPW_D3D11_Profiler_DeviceContext_PushRange_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_DeviceContext_PushRange_Params, rangeNameLength)

    NVPA_Status NVPW_D3D11_Profiler_DeviceContext_PushRange(NVPW_D3D11_Profiler_DeviceContext_PushRange_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_DeviceContext_PopRange_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct ID3D11DeviceContext* pDeviceContext;
    } NVPW_D3D11_Profiler_DeviceContext_PopRange_Params;
#define NVPW_D3D11_Profiler_DeviceContext_PopRange_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_DeviceContext_PopRange_Params, pDeviceContext)

    NVPA_Status NVPW_D3D11_Profiler_DeviceContext_PopRange(NVPW_D3D11_Profiler_DeviceContext_PopRange_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct ID3D11DeviceContext* pDeviceContext;
        /// [in]
        size_t counterDataImageSize;
        /// [in]
        uint8_t* pCounterDataImage;
        /// [in]
        size_t counterDataScratchBufferSize;
        /// [in]
        uint8_t* pCounterDataScratchBuffer;
        /// [out] number of ranges whose data was dropped in the processed pass
        size_t numRangesDropped;
        /// [out] number of bytes not written to TraceBuffer due to buffer full
        size_t numTraceBytesDropped;
        /// [out] true if a pass was successfully decoded
        NVPA_Bool onePassCollected;
        /// [out] becomes true when the last pass has been decoded
        NVPA_Bool allPassesCollected;
        /// [out] the Config decoded by this call
        const uint8_t* pConfigDecoded;
        /// [out] the passIndex decoded
        size_t passIndexDecoded;
    } NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Params;
#define NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Params, passIndexDecoded)

    NVPA_Status NVPW_D3D11_Profiler_DeviceContext_DecodeCounters(NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_IsGpuSupported_Params
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
        NVPW_CmpSupportLevel cmpSupportLevel;
        /// [out]
        NVPW_WslSupportLevel wslSupportLevel;
    } NVPW_D3D11_Profiler_IsGpuSupported_Params;
#define NVPW_D3D11_Profiler_IsGpuSupported_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_IsGpuSupported_Params, wslSupportLevel)

    /// NVPW_D3D11_LoadDriver must be called prior to this API
    NVPA_Status NVPW_D3D11_Profiler_IsGpuSupported(NVPW_D3D11_Profiler_IsGpuSupported_Params* pParams);

    typedef struct NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct ID3D11DeviceContext* pDeviceContext;
        /// [in/out] If `pCounterAvailabilityImage` is NULL, then the required size is returned in
        /// `counterAvailabilityImageSize`, otherwise `counterAvailabilityImageSize` should be set to the size of
        /// `pCounterAvailabilityImage`, and on return it would be overwritten with number of actual bytes copied
        size_t counterAvailabilityImageSize;
        /// [in] buffer receiving counter availability image, may be NULL
        uint8_t* pCounterAvailabilityImage;
    } NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability_Params;
#define NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability_Params, pCounterAvailabilityImage)

    /// This API may fail, if any profiling or sampling session is active on the specified ID3D11DeviceContext or its
    /// device
    NVPA_Status NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability(NVPW_D3D11_Profiler_DeviceContext_GetCounterAvailability_Params* pParams);



#ifdef __cplusplus
} // extern "C"
#endif

#if defined(__GNUC__) && defined(NVPA_SHARED_LIB)
    #pragma GCC visibility pop
#endif

#endif // NVPERF_D3D11_TARGET_H
