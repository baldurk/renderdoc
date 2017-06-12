//==============================================================================
// Copyright (c) 2010-2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Defines the data types and enumerations used by GPUPerfAPI.
///         This file does not need to be directly included by an application
///         that uses GPUPerfAPI.
//==============================================================================

#ifndef _GPUPERFAPI_TYPES_H_
#define _GPUPERFAPI_TYPES_H_

#include <limits.h>

// Type definitions
#ifdef _WIN32
    typedef char    gpa_int8;
    typedef short   gpa_int16;
    typedef int     gpa_int32;
    typedef __int64 gpa_int64;
    typedef float   gpa_float32;
    typedef double  gpa_float64;
    typedef unsigned char    gpa_uint8;
    typedef unsigned short   gpa_uint16;
    typedef unsigned int     gpa_uint32;
    typedef unsigned __int64 gpa_uint64;
    #ifndef __cplusplus
        typedef gpa_uint8 bool;
    #endif
#endif // _WIN32

#ifdef __linux__

    #ifdef GPALIB_DECL
    #else
        #ifdef __cplusplus
            #define GPALIB_DECL extern "C"
        #else
            #define GPALIB_DECL
        #endif // _cplusplus
    #endif

    typedef char    gpa_int8;
    typedef short   gpa_int16;
    typedef int     gpa_int32;
    typedef long long gpa_int64;
    typedef unsigned int UINT;
    typedef float   gpa_float32;
    typedef double  gpa_float64;
    typedef unsigned char    gpa_uint8;
    typedef unsigned short   gpa_uint16;
    typedef unsigned int     gpa_uint32;
    typedef unsigned long long gpa_uint64;
    #ifndef __cplusplus
        typedef gpa_uint8 bool;
    #endif

    #define UNREFERENCED_PARAMETER(x)
    #define UNREFERECED_VAR(x)

    #define _strcmpi(a, b) strcasecmp(a, b)

    // for now, just use non secure version for Linux
    #define strcpy_s(dst, ndst, src) strcpy(dst, src)
    #define strtok_s(a, b, c) strtok(a, b)

#endif // __linux__

// Limit definitions
/// macro for max int8
#define GPA_INT8_MAX SCHAR_MAX
/// macro for max int16
#define GPA_INT16_MAX SHRT_MAX
/// macro for max int32
#define GPA_INT32_MAX INT_MAX
/// macro for max int64
#define GPA_INT64_MAX LLONG_MAX

/// macro for max uint8
#define GPA_UINT8_MAX UCHAR_MAX
/// macro for max uint16
#define GPA_UINT16_MAX USHRT_MAX
/// macro for max uint32
#define GPA_UINT32_MAX UINT_MAX
/// macro for max uint64
#define GPA_UINT64_MAX ULLONG_MAX

/// Status enumerations
typedef enum
{
    GPA_STATUS_OK = 0,
    GPA_STATUS_ERROR_NULL_POINTER,
    GPA_STATUS_ERROR_COUNTERS_NOT_OPEN,
    GPA_STATUS_ERROR_COUNTERS_ALREADY_OPEN,
    GPA_STATUS_ERROR_INDEX_OUT_OF_RANGE,
    GPA_STATUS_ERROR_NOT_FOUND,
    GPA_STATUS_ERROR_ALREADY_ENABLED,
    GPA_STATUS_ERROR_NO_COUNTERS_ENABLED,
    GPA_STATUS_ERROR_NOT_ENABLED,
    GPA_STATUS_ERROR_SAMPLING_NOT_STARTED,
    GPA_STATUS_ERROR_SAMPLING_ALREADY_STARTED,
    GPA_STATUS_ERROR_SAMPLING_NOT_ENDED,
    GPA_STATUS_ERROR_NOT_ENOUGH_PASSES,
    GPA_STATUS_ERROR_PASS_NOT_ENDED,
    GPA_STATUS_ERROR_PASS_NOT_STARTED,
    GPA_STATUS_ERROR_PASS_ALREADY_STARTED,
    GPA_STATUS_ERROR_SAMPLE_NOT_STARTED,
    GPA_STATUS_ERROR_SAMPLE_ALREADY_STARTED,
    GPA_STATUS_ERROR_SAMPLE_NOT_ENDED,
    GPA_STATUS_ERROR_CANNOT_CHANGE_COUNTERS_WHEN_SAMPLING,
    GPA_STATUS_ERROR_SESSION_NOT_FOUND,
    GPA_STATUS_ERROR_SAMPLE_NOT_FOUND,
    GPA_STATUS_ERROR_SAMPLE_NOT_FOUND_IN_ALL_PASSES,
    GPA_STATUS_ERROR_COUNTER_NOT_OF_SPECIFIED_TYPE,
    GPA_STATUS_ERROR_READING_COUNTER_RESULT,
    GPA_STATUS_ERROR_VARIABLE_NUMBER_OF_SAMPLES_IN_PASSES,
    GPA_STATUS_ERROR_FAILED,
    GPA_STATUS_ERROR_HARDWARE_NOT_SUPPORTED,
    GPA_STATUS_ERROR_DRIVER_NOT_SUPPORTED,

    // following are status codes used internally within GPUPerfAPI
    GPA_STATUS_INTERNAL = 256,
    GPA_STATUS_OK_HANDLED = GPA_STATUS_INTERNAL,
} GPA_Status;

/// Value type definitions
typedef enum
{
    GPA_TYPE_FLOAT32,             ///< Result will be a 32-bit float
    GPA_TYPE_FLOAT64,             ///< Result will be a 64-bit float
    GPA_TYPE_UINT32,              ///< Result will be a 32-bit unsigned int
    GPA_TYPE_UINT64,              ///< Result will be a 64-bit unsigned int
    GPA_TYPE_INT32,               ///< Result will be a 32-bit int
    GPA_TYPE_INT64,               ///< Result will be a 64-bit int
    GPA_TYPE__LAST                ///< Marker indicating last element
} GPA_Type;

/// Result usage type definitions
typedef enum
{
    GPA_USAGE_TYPE_RATIO,         ///< Result is a ratio of two different values or types
    GPA_USAGE_TYPE_PERCENTAGE,    ///< Result is a percentage, typically within [0,100] range, but may be higher for certain counters
    GPA_USAGE_TYPE_CYCLES,        ///< Result is in clock cycles
    GPA_USAGE_TYPE_MILLISECONDS,  ///< Result is in milliseconds
    GPA_USAGE_TYPE_BYTES,         ///< Result is in bytes
    GPA_USAGE_TYPE_ITEMS,         ///< Result is a count of items or objects (ie, vertices, triangles, threads, pixels, texels, etc)
    GPA_USAGE_TYPE_KILOBYTES,     ///< Result is in kilobytes
    GPA_USAGE_TYPE__LAST          ///< Marker indicating last element
} GPA_Usage_Type;

/// Logging type definitions
typedef enum
{
    GPA_LOGGING_NONE = 0,
    GPA_LOGGING_ERROR = 1,
    GPA_LOGGING_MESSAGE = 2,
    GPA_LOGGING_ERROR_AND_MESSAGE = 3,
    GPA_LOGGING_TRACE = 4,
    GPA_LOGGING_ERROR_AND_TRACE = 5,
    GPA_LOGGING_MESSAGE_AND_TRACE = 6,
    GPA_LOGGING_ERROR_MESSAGE_AND_TRACE = 7,
    GPA_LOGGING_ALL = 0xFF
} GPA_Logging_Type;

/// APIs Supported (either publicly or internally) by GPUPerfAPI
typedef enum
{
    GPA_API_DIRECTX_11,
    GPA_API_DIRECTX_12,
    GPA_API_OPENGL,
    GPA_API_OPENGLES,
    GPA_API_OPENCL,
    GPA_API_HSA,
    GPA_API__LAST
} GPA_API_Type;

#endif // _GPUPERFAPI_TYPES_H_
