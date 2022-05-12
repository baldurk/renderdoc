#ifndef NVPERF_COMMON_H
#define NVPERF_COMMON_H

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
 *  @file   nvperf_common.h
 */

#ifndef NVPERF_NVPA_STATUS_DEFINED
#define NVPERF_NVPA_STATUS_DEFINED

    /// Error codes.
    typedef enum NVPA_Status
    {
        /// Success
        NVPA_STATUS_SUCCESS = 0,
        /// Generic error.
        NVPA_STATUS_ERROR = 1,
        /// Internal error.  Please file a bug!
        NVPA_STATUS_INTERNAL_ERROR = 2,
        /// NVPW_InitializeTarget() has not been called yet.
        NVPA_STATUS_NOT_INITIALIZED = 3,
        /// The NvPerf DLL/DSO could not be loaded during NVPW_Initialize*.
        NVPA_STATUS_NOT_LOADED = 4,
        /// The function was not found in this version of the NvPerf DLL/DSO.
        NVPA_STATUS_FUNCTION_NOT_FOUND = 5,
        /// The request was intentionally not supported.
        NVPA_STATUS_NOT_SUPPORTED = 6,
        /// The request was not implemented by this version.
        NVPA_STATUS_NOT_IMPLEMENTED = 7,
        /// Invalid argument.
        NVPA_STATUS_INVALID_ARGUMENT = 8,
        /// UNUSED
        NVPA_STATUS_INVALID_METRIC_ID = 9,
        /// No driver has been loaded via NVPW_*_LoadDriver().
        NVPA_STATUS_DRIVER_NOT_LOADED = 10,
        /// Failed memory allocation.
        NVPA_STATUS_OUT_OF_MEMORY = 11,
        /// UNUSED
        NVPA_STATUS_INVALID_THREAD_STATE = 12,
        /// UNUSED
        NVPA_STATUS_FAILED_CONTEXT_ALLOC = 13,
        /// The specified GPU is not supported.
        NVPA_STATUS_UNSUPPORTED_GPU = 14,
        /// The installed NVIDIA driver is too old.
        NVPA_STATUS_INSUFFICIENT_DRIVER_VERSION = 15,
        /// UNUSED
        NVPA_STATUS_OBJECT_NOT_REGISTERED = 16,
        /// Profiling permission not granted; see https://developer.nvidia.com/nvidia-development-tools-solutions-
        /// ERR_NVGPUCTRPERM-permission-issue-performance-counters
        NVPA_STATUS_INSUFFICIENT_PRIVILEGE = 17,
        /// UNUSED
        NVPA_STATUS_INVALID_CONTEXT_STATE = 18,
        /// UNUSED
        NVPA_STATUS_INVALID_OBJECT_STATE = 19,
        /// The request could not be fulfilled because a system resource is already in use.
        NVPA_STATUS_RESOURCE_UNAVAILABLE = 20,
        /// UNUSED
        NVPA_STATUS_DRIVER_LOADED_TOO_LATE = 21,
        /// The provided buffer is not large enough.
        NVPA_STATUS_INSUFFICIENT_SPACE = 22,
        /// UNUSED
        NVPA_STATUS_OBJECT_MISMATCH = 23,
        /// Virtualized GPU (vGPU) is not supported.
        NVPA_STATUS_VIRTUALIZED_DEVICE_NOT_SUPPORTED = 24,
        /// Profiling permission was not granted or the device was disabled.
        NVPA_STATUS_PROFILING_NOT_ALLOWED = 25,
        NVPA_STATUS__COUNT
    } NVPA_Status;


#endif // NVPERF_NVPA_STATUS_DEFINED


#ifndef NVPERF_NVPA_ACTIVITY_KIND_DEFINED
#define NVPERF_NVPA_ACTIVITY_KIND_DEFINED

    /// The configuration's activity-kind dictates which types of data may be collected.
    typedef enum NVPA_ActivityKind
    {
        /// Invalid value.
        NVPA_ACTIVITY_KIND_INVALID = 0,
        /// A workload-centric activity for serialized collection. The library introduces any synchronization required
        /// to collect metrics.
        NVPA_ACTIVITY_KIND_PROFILER,
        /// A realtime activity for sampling counters from the CPU or GPU.
        NVPA_ACTIVITY_KIND_REALTIME_SAMPLED,
        /// A realtime activity for profiling counters from the CPU or GPU without CPU/GPU synchronizations.
        NVPA_ACTIVITY_KIND_REALTIME_PROFILER,
        NVPA_ACTIVITY_KIND__COUNT
    } NVPA_ActivityKind;


#endif // NVPERF_NVPA_ACTIVITY_KIND_DEFINED


#ifndef NVPERF_NVPA_BOOL_DEFINED
#define NVPERF_NVPA_BOOL_DEFINED
    /// The type used for boolean values.
    typedef uint8_t NVPA_Bool;
#endif // NVPERF_NVPA_BOOL_DEFINED

#ifndef NVPA_STRUCT_SIZE
#define NVPA_STRUCT_SIZE(type_, lastfield_)                     (offsetof(type_, lastfield_) + sizeof(((type_*)0)->lastfield_))
#endif // NVPA_STRUCT_SIZE

#ifndef NVPW_FIELD_EXISTS
#define NVPW_FIELD_EXISTS(pParams_, name_) \
    ((pParams_)->structSize >= (size_t)((const uint8_t*)(&(pParams_)->name_) + sizeof(pParams_)->name_ - (const uint8_t*)(pParams_)))
#endif // NVPW_FIELD_EXISTS


#ifndef NVPERF_NVPA_GETPROCADDRESS_DEFINED
#define NVPERF_NVPA_GETPROCADDRESS_DEFINED

typedef NVPA_Status (*NVPA_GenericFn)(void);


    /// 
    /// Gets the address of an NvPerf API function.
    /// 
    /// \return A function pointer to the function, or NULL if the function is not available.
    /// 
    /// \param pFunctionName [in] Name of the function to retrieve.
    NVPA_GenericFn NVPA_GetProcAddress(const char* pFunctionName);

#endif

#ifndef NVPERF_NVPW_SETLIBRARYLOADPATHS_DEFINED
#define NVPERF_NVPW_SETLIBRARYLOADPATHS_DEFINED


    typedef struct NVPW_SetLibraryLoadPaths_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in] number of paths in ppPaths
        size_t numPaths;
        /// [in] array of null-terminated paths
        const char** ppPaths;
    } NVPW_SetLibraryLoadPaths_Params;
#define NVPW_SetLibraryLoadPaths_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_SetLibraryLoadPaths_Params, ppPaths)

    /// Sets library search path for \ref NVPW_InitializeHost() and \ref NVPW_InitializeTarget().
    /// \ref NVPW_InitializeHost() and \ref NVPW_InitializeTarget load the NvPerf DLL/DSO.  This function sets
    /// ordered paths that will be searched with the LoadLibrary() or dlopen() call.
    /// If load paths are set by this function, the default set of load paths
    /// will not be attempted.
    /// Each path must point at a directory (not a file name).
    /// This function is not thread-safe.
    /// Example Usage:
    /// \code
    ///     const char* paths[] = {
    ///         "path1", "path2", etc
    ///     };
    ///     NVPW_SetLibraryLoadPaths_Params params{NVPW_SetLibraryLoadPaths_Params_STRUCT_SIZE};
    ///     params.numPaths = sizeof(paths)/sizeof(paths[0]);
    ///     params.ppPaths = paths;
    ///     NVPW_SetLibraryLoadPaths(&params);
    ///     NVPW_InitializeHost();
    ///     params.numPaths = 0;
    ///     params.ppPaths = NULL;
    ///     NVPW_SetLibraryLoadPaths(&params);
    /// \endcode
    NVPA_Status NVPW_SetLibraryLoadPaths(NVPW_SetLibraryLoadPaths_Params* pParams);

    typedef struct NVPW_SetLibraryLoadPathsW_Params
    {
        /// [in]
        size_t structSize;
        /// [in] assign to NULL
        void* pPriv;
        /// [in] number of paths in ppwPaths
        size_t numPaths;
        /// [in] array of null-terminated paths
        const wchar_t** ppwPaths;
    } NVPW_SetLibraryLoadPathsW_Params;
#define NVPW_SetLibraryLoadPathsW_Params_STRUCT_SIZE NVPA_STRUCT_SIZE(NVPW_SetLibraryLoadPathsW_Params, ppwPaths)

    /// Sets library search path for \ref NVPW_InitializeHost() and \ref NVPW_InitializeTarget().
    /// \ref NVPW_InitializeHost() and \ref NVPW_InitializeTarget load the NvPerf DLL/DSO.  This function sets
    /// ordered paths that will be searched with the LoadLibrary() or dlopen() call.
    /// If load paths are set by this function, the default set of load paths
    /// will not be attempted.
    /// Each path must point at a directory (not a file name).
    /// This function is not thread-safe.
    /// Example Usage:
    /// \code
    ///     const wchar_t* wpaths[] = {
    ///         L"path1", L"path2", etc
    ///     };
    ///     NVPW_SetLibraryLoadPathsW_Params params{NVPW_SetLibraryLoadPathsW_Params_STRUCT_SIZE};
    ///     params.numPaths = sizeof(wpaths)/sizeof(wpaths[0]);
    ///     params.ppwPaths = wpaths;
    ///     NVPW_SetLibraryLoadPathsW(&params);
    ///     NVPW_InitializeHost();
    ///     params.numPaths = 0;
    ///     params.ppwPaths = NULL;
    ///     NVPW_SetLibraryLoadPathsW(&params);
    /// \endcode
    NVPA_Status NVPW_SetLibraryLoadPathsW(NVPW_SetLibraryLoadPathsW_Params* pParams);

#endif



#ifdef __cplusplus
} // extern "C"
#endif

#if defined(__GNUC__) && defined(NVPA_SHARED_LIB)
    #pragma GCC visibility pop
#endif

#endif // NVPERF_COMMON_H
