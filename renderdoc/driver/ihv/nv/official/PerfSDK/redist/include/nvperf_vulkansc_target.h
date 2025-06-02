#ifndef NVPERF_VULKANSC_TARGET_H
#define NVPERF_VULKANSC_TARGET_H

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
 *  @file   nvperf_vulkansc_target.h
 */

    typedef struct NVPW_VKSC_Profiler_CounterDataImageOptions
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
    } NVPW_VKSC_Profiler_CounterDataImageOptions;
#define NVPW_VKSC_Profiler_CounterDataImageOptions_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_CounterDataImageOptions, maxRangeNameLength)

    typedef struct NVPW_VKSC_Profiler_CounterDataImage_CalculateSize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t counterDataImageOptionsSize;
        /// [in]
        const NVPW_VKSC_Profiler_CounterDataImageOptions* pOptions;
        /// [out]
        size_t counterDataImageSize;
    } NVPW_VKSC_Profiler_CounterDataImage_CalculateSize_Params;
#define NVPW_VKSC_Profiler_CounterDataImage_CalculateSize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_CounterDataImage_CalculateSize_Params, counterDataImageSize)

    NVPA_Status NVPW_VKSC_Profiler_CounterDataImage_CalculateSize(NVPW_VKSC_Profiler_CounterDataImage_CalculateSize_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_CounterDataImage_Initialize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t counterDataImageOptionsSize;
        /// [in]
        const NVPW_VKSC_Profiler_CounterDataImageOptions* pOptions;
        /// [in]
        size_t counterDataImageSize;
        /// [in] The buffer to be written.
        uint8_t* pCounterDataImage;
    } NVPW_VKSC_Profiler_CounterDataImage_Initialize_Params;
#define NVPW_VKSC_Profiler_CounterDataImage_Initialize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_CounterDataImage_Initialize_Params, pCounterDataImage)

    NVPA_Status NVPW_VKSC_Profiler_CounterDataImage_Initialize(NVPW_VKSC_Profiler_CounterDataImage_Initialize_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_CounterDataImage_CalculateScratchBufferSize_Params
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
    } NVPW_VKSC_Profiler_CounterDataImage_CalculateScratchBufferSize_Params;
#define NVPW_VKSC_Profiler_CounterDataImage_CalculateScratchBufferSize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_CounterDataImage_CalculateScratchBufferSize_Params, counterDataScratchBufferSize)

    NVPA_Status NVPW_VKSC_Profiler_CounterDataImage_CalculateScratchBufferSize(NVPW_VKSC_Profiler_CounterDataImage_CalculateScratchBufferSize_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_CounterDataImage_InitializeScratchBuffer_Params
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
    } NVPW_VKSC_Profiler_CounterDataImage_InitializeScratchBuffer_Params;
#define NVPW_VKSC_Profiler_CounterDataImage_InitializeScratchBuffer_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_CounterDataImage_InitializeScratchBuffer_Params, pCounterDataScratchBuffer)

    NVPA_Status NVPW_VKSC_Profiler_CounterDataImage_InitializeScratchBuffer(NVPW_VKSC_Profiler_CounterDataImage_InitializeScratchBuffer_Params* pParams);

    typedef struct NVPW_VKSC_LoadDriver_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkInstance_T* instance;
    } NVPW_VKSC_LoadDriver_Params;
#define NVPW_VKSC_LoadDriver_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_LoadDriver_Params, instance)

    NVPA_Status NVPW_VKSC_LoadDriver(NVPW_VKSC_LoadDriver_Params* pParams);

    typedef struct NVPW_VKSC_Device_GetDeviceIndex_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkPhysicalDevice_T* physicalDevice;
        /// [in]
        size_t sliIndex;
        /// [out]
        size_t deviceIndex;
        /// [in]
        struct VkInstance_T* instance;
        /// [in] Either a pointer to the loaders vkGetInstanceProcAddr or a pointer to the next layers
        /// vkGetInstanceProcAddr
        void* pfnGetInstanceProcAddr;
        /// [in] Either a pointer to the loaders vkGetDeviceProcAddr or a pointer to the next layers vkGetDeviceProcAddr
        void* pfnGetDeviceProcAddr;
    } NVPW_VKSC_Device_GetDeviceIndex_Params;
#define NVPW_VKSC_Device_GetDeviceIndex_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Device_GetDeviceIndex_Params, pfnGetDeviceProcAddr)

    NVPA_Status NVPW_VKSC_Device_GetDeviceIndex(NVPW_VKSC_Device_GetDeviceIndex_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_GetRequiredInstanceExtensions_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [out]
        const char* const* ppInstanceExtensionNames;
        /// [out]
        size_t numInstanceExtensionNames;
        /// [in] Vulkan API version (VK_API_VERSION_*)
        uint32_t apiVersion;
        /// [out] is apiVersion officially supported by the NvPerf API
        NVPA_Bool isOfficiallySupportedVersion;
    } NVPW_VKSC_Profiler_GetRequiredInstanceExtensions_Params;
#define NVPW_VKSC_Profiler_GetRequiredInstanceExtensions_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_GetRequiredInstanceExtensions_Params, isOfficiallySupportedVersion)

    NVPA_Status NVPW_VKSC_Profiler_GetRequiredInstanceExtensions(NVPW_VKSC_Profiler_GetRequiredInstanceExtensions_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_GetRequiredDeviceExtensions_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [out]
        const char* const* ppDeviceExtensionNames;
        /// [out]
        size_t numDeviceExtensionNames;
        /// [in] Vulkan API version (VK_API_VERSION_*)
        uint32_t apiVersion;
        /// [out] is apiVersion officially supported by the NvPerf API
        NVPA_Bool isOfficiallySupportedVersion;
        /// [in] [optional]
        struct VkInstance_T* instance;
        /// [in] [optional]
        struct VkPhysicalDevice_T* physicalDevice;
        /// [in] [optional] Either a pointer to the loaders vkGetInstanceProcAddr or a pointer to the next layers
        /// vkGetInstanceProcAddr
        void* pfnGetInstanceProcAddr;
    } NVPW_VKSC_Profiler_GetRequiredDeviceExtensions_Params;
#define NVPW_VKSC_Profiler_GetRequiredDeviceExtensions_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_GetRequiredDeviceExtensions_Params, pfnGetInstanceProcAddr)

    NVPA_Status NVPW_VKSC_Profiler_GetRequiredDeviceExtensions(NVPW_VKSC_Profiler_GetRequiredDeviceExtensions_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_CalcTraceBufferSize_Params
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
        /// NVPW_VKSC_Profiler_BeginSession_Params::traceBufferSize.
        size_t traceBufferSize;
    } NVPW_VKSC_Profiler_CalcTraceBufferSize_Params;
#define NVPW_VKSC_Profiler_CalcTraceBufferSize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_CalcTraceBufferSize_Params, traceBufferSize)

    NVPA_Status NVPW_VKSC_Profiler_CalcTraceBufferSize(NVPW_VKSC_Profiler_CalcTraceBufferSize_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_CalcPerfmonBufferSize_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t numTraceBuffers;
        /// [in] Maximum number of Push/Pop pairs that can be recorded in a single pass.
        size_t maxRangesPerPass;
        /// [out] Perfmon buffer size for a single pass.  Pass this to
        /// NVPW_VKSC_Profiler_BeginSession_Params::perfmonBufferGpuSize.
        size_t perfmonBufferGpuSize;
    } NVPW_VKSC_Profiler_CalcPerfmonBufferSize_Params;
#define NVPW_VKSC_Profiler_CalcPerfmonBufferSize_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_CalcPerfmonBufferSize_Params, perfmonBufferGpuSize)

    NVPA_Status NVPW_VKSC_Profiler_CalcPerfmonBufferSize(NVPW_VKSC_Profiler_CalcPerfmonBufferSize_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_GetDeviceObjectReservation_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        size_t deviceIndex;
        /// [in]
        size_t numTraceBuffers;
        /// [out] VkSemaphore count to reserve for range profiler.
        size_t semaphoreCount;
        /// [out] VkFence count to reserve for range profiler.
        size_t fenceCount;
        /// [out] VkDeviceMemory count to reserve for range profiler.
        size_t deviceMemoryCount;
    } NVPW_VKSC_Profiler_GetDeviceObjectReservation_Params;
#define NVPW_VKSC_Profiler_GetDeviceObjectReservation_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_GetDeviceObjectReservation_Params, deviceMemoryCount)

    NVPA_Status NVPW_VKSC_Profiler_GetDeviceObjectReservation(NVPW_VKSC_Profiler_GetDeviceObjectReservation_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_Queue_BeginSession_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkDevice_T* device;
        /// [in]
        struct VkQueue_T* queue;
        /// [in] Set to 1 if every pass is synchronized with CPU; for asynchronous collection, increase to
        /// (softwarePipelineDepth + 2).
        size_t numTraceBuffers;
        /// [in] Size of the per-pass TraceBuffer in bytes.  The profiler allocates a numTraceBuffers * traceBufferSize
        /// internally.
        size_t traceBufferSize;
        /// [in]
        uint8_t* pTraceArena;
        /// [in] The VK object holding tracebuffer. It must be created at the index of
        /// VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT flags.
        struct VkDeviceMemory_T* traceArenaDeviceMemory;
        /// [in] size of GPU PerfmonBuffer. This argument must be greater than 0.
        size_t perfmonBufferGpuSize;
        /// [in]
        struct VkInstance_T* instance;
        /// [in]
        struct VkPhysicalDevice_T* physicalDevice;
        /// [in] Either a pointer to the loaders vkGetInstanceProcAddr or a pointer to the next layers
        /// vkGetInstanceProcAddr
        void* pfnGetInstanceProcAddr;
        /// [in] Either a pointer to the loaders vkGetDeviceProcAddr or a pointer to the next layers vkGetDeviceProcAddr
        void* pfnGetDeviceProcAddr;
    } NVPW_VKSC_Profiler_Queue_BeginSession_Params;
#define NVPW_VKSC_Profiler_Queue_BeginSession_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_Queue_BeginSession_Params, pfnGetDeviceProcAddr)

    NVPA_Status NVPW_VKSC_Profiler_Queue_BeginSession(NVPW_VKSC_Profiler_Queue_BeginSession_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_Queue_EndSession_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkQueue_T* queue;
        /// [in] Maximum number of milliseconds to wait for pending GPU operations. Pass 0xFFFFFFFF to wait forever
        uint32_t timeout;
        /// [out] becomes true if timeout is reached while waiting for pending GPU operations.
        NVPA_Bool timeoutExpired;
    } NVPW_VKSC_Profiler_Queue_EndSession_Params;
#define NVPW_VKSC_Profiler_Queue_EndSession_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_Queue_EndSession_Params, timeoutExpired)

    NVPA_Status NVPW_VKSC_Profiler_Queue_EndSession(NVPW_VKSC_Profiler_Queue_EndSession_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_Queue_ServicePendingGpuOperations_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkQueue_T* queue;
        /// [in] The number of operations to process. Passing `0` will block until EndSession is called
        uint32_t numOperations;
        /// [in] Maximum number of milliseconds to wait for pending GPU operations. Pass 0xFFFFFFFF to wait forever
        uint32_t timeout;
        /// [out] becomes true if timeout is reached while waiting for pending GPU operations.
        NVPA_Bool timeoutExpired;
    } NVPW_VKSC_Profiler_Queue_ServicePendingGpuOperations_Params;
#define NVPW_VKSC_Profiler_Queue_ServicePendingGpuOperations_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_Queue_ServicePendingGpuOperations_Params, timeoutExpired)

    NVPA_Status NVPW_VKSC_Profiler_Queue_ServicePendingGpuOperations(NVPW_VKSC_Profiler_Queue_ServicePendingGpuOperations_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_Queue_SetConfig_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkQueue_T* queue;
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
    } NVPW_VKSC_Profiler_Queue_SetConfig_Params;
#define NVPW_VKSC_Profiler_Queue_SetConfig_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_Queue_SetConfig_Params, targetNestingLevel)

    NVPA_Status NVPW_VKSC_Profiler_Queue_SetConfig(NVPW_VKSC_Profiler_Queue_SetConfig_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_Queue_ClearConfig_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkQueue_T* queue;
    } NVPW_VKSC_Profiler_Queue_ClearConfig_Params;
#define NVPW_VKSC_Profiler_Queue_ClearConfig_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_Queue_ClearConfig_Params, queue)

    NVPA_Status NVPW_VKSC_Profiler_Queue_ClearConfig(NVPW_VKSC_Profiler_Queue_ClearConfig_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_Queue_BeginPass_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkQueue_T* queue;
    } NVPW_VKSC_Profiler_Queue_BeginPass_Params;
#define NVPW_VKSC_Profiler_Queue_BeginPass_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_Queue_BeginPass_Params, queue)

    NVPA_Status NVPW_VKSC_Profiler_Queue_BeginPass(NVPW_VKSC_Profiler_Queue_BeginPass_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_Queue_EndPass_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkQueue_T* queue;
        /// [out] The passIndex that will be collected by the *next* BeginPass.
        size_t passIndex;
        /// [out] The targetNestingLevel that will be collected by the *next* BeginPass.
        uint16_t targetNestingLevel;
        /// [out] becomes true when the last pass has been queued to the GPU
        NVPA_Bool allPassesSubmitted;
    } NVPW_VKSC_Profiler_Queue_EndPass_Params;
#define NVPW_VKSC_Profiler_Queue_EndPass_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_Queue_EndPass_Params, allPassesSubmitted)

    NVPA_Status NVPW_VKSC_Profiler_Queue_EndPass(NVPW_VKSC_Profiler_Queue_EndPass_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_CommandBuffer_PushRangeGraphics_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkCommandBuffer_T* commandBuffer;
        /// [in] specifies the range that subsequent launches' counters will be assigned to; must not be NULL
        const char* pRangeName;
        /// [in] assign to strlen(pRangeName) if known; if set to zero, the library will call strlen()
        size_t rangeNameLength;
    } NVPW_VKSC_Profiler_CommandBuffer_PushRangeGraphics_Params;
#define NVPW_VKSC_Profiler_CommandBuffer_PushRangeGraphics_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_CommandBuffer_PushRangeGraphics_Params, rangeNameLength)

    NVPA_Status NVPW_VKSC_Profiler_CommandBuffer_PushRangeGraphics(NVPW_VKSC_Profiler_CommandBuffer_PushRangeGraphics_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_CommandBuffer_PopRangeGraphics_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkCommandBuffer_T* commandBuffer;
    } NVPW_VKSC_Profiler_CommandBuffer_PopRangeGraphics_Params;
#define NVPW_VKSC_Profiler_CommandBuffer_PopRangeGraphics_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_CommandBuffer_PopRangeGraphics_Params, commandBuffer)

    NVPA_Status NVPW_VKSC_Profiler_CommandBuffer_PopRangeGraphics(NVPW_VKSC_Profiler_CommandBuffer_PopRangeGraphics_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_CommandBuffer_PushRangeCompute_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkCommandBuffer_T* commandBuffer;
        /// [in] specifies the range that subsequent launches' counters will be assigned to; must not be NULL
        const char* pRangeName;
        /// [in] assign to strlen(pRangeName) if known; if set to zero, the library will call strlen()
        size_t rangeNameLength;
    } NVPW_VKSC_Profiler_CommandBuffer_PushRangeCompute_Params;
#define NVPW_VKSC_Profiler_CommandBuffer_PushRangeCompute_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_CommandBuffer_PushRangeCompute_Params, rangeNameLength)

    NVPA_Status NVPW_VKSC_Profiler_CommandBuffer_PushRangeCompute(NVPW_VKSC_Profiler_CommandBuffer_PushRangeCompute_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_CommandBuffer_PopRangeCompute_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkCommandBuffer_T* commandBuffer;
    } NVPW_VKSC_Profiler_CommandBuffer_PopRangeCompute_Params;
#define NVPW_VKSC_Profiler_CommandBuffer_PopRangeCompute_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_CommandBuffer_PopRangeCompute_Params, commandBuffer)

    NVPA_Status NVPW_VKSC_Profiler_CommandBuffer_PopRangeCompute(NVPW_VKSC_Profiler_CommandBuffer_PopRangeCompute_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_Queue_InitializeRangeCommands_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkQueue_T* queue;
    } NVPW_VKSC_Profiler_Queue_InitializeRangeCommands_Params;
#define NVPW_VKSC_Profiler_Queue_InitializeRangeCommands_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_Queue_InitializeRangeCommands_Params, queue)

    /// Prepare a queue for range command submissions.
    /// It must be called on a queue if any range command is submitted to that queue before BeginPass.
    /// Failing to do so could result in undefined behaviors including GPU fault.
    /// It's optional when range commands are only submitted between BeginPass and EndPass.
    /// Range commands are generated by NVPW_VKSC_Profiler_CommandBuffer*Range*().
    /// This function may not be called between NVPW_VKSC_Profiler_Queue_BeginPass and NVPW_VKSC_Profiler_Queue_EndPass.
    NVPA_Status NVPW_VKSC_Profiler_Queue_InitializeRangeCommands(NVPW_VKSC_Profiler_Queue_InitializeRangeCommands_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_Queue_DecodeCounters_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in]
        struct VkQueue_T* queue;
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
    } NVPW_VKSC_Profiler_Queue_DecodeCounters_Params;
#define NVPW_VKSC_Profiler_Queue_DecodeCounters_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_Queue_DecodeCounters_Params, passIndexDecoded)

    NVPA_Status NVPW_VKSC_Profiler_Queue_DecodeCounters(NVPW_VKSC_Profiler_Queue_DecodeCounters_Params* pParams);

    typedef struct NVPW_VKSC_Profiler_IsGpuSupported_Params
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
    } NVPW_VKSC_Profiler_IsGpuSupported_Params;
#define NVPW_VKSC_Profiler_IsGpuSupported_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_VKSC_Profiler_IsGpuSupported_Params, wslSupportLevel)

    /// NVPW_VKSC_LoadDriver must be called prior to this API
    NVPA_Status NVPW_VKSC_Profiler_IsGpuSupported(NVPW_VKSC_Profiler_IsGpuSupported_Params* pParams);



#ifdef __cplusplus
} // extern "C"
#endif

#if defined(__GNUC__) && defined(NVPA_SHARED_LIB)
    #pragma GCC visibility pop
#endif

#endif // NVPERF_VULKANSC_TARGET_H
