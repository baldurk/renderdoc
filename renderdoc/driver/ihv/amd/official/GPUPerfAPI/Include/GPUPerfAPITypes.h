//==============================================================================
// Copyright (c) 2010-2018 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Defines the data types and enumerations used by GPUPerfAPI.
///         This file does not need to be directly included by an application
///         that uses GPUPerfAPI.
//==============================================================================

#ifndef _GPUPERFAPI_TYPES_H_
#define _GPUPERFAPI_TYPES_H_

#include <limits.h>

// Platform specific definitions
#ifdef _WIN32
#include <Windows.h>
typedef HMODULE LibHandle; ///< typedef for HMODULE for loading the library on windows
typedef GUID GPA_UUID;     ///< typedef for Windows GUID definition
#else
typedef void* LibHandle;   ///< typedef for void* for loading the library on linux

/// Structure for holding UUID
typedef struct GPA_UUID
{
    unsigned long m_data1;    ///< first part of the UUID data
    unsigned short m_data2;   ///< second part of the UUID data
    unsigned short m_data3;   ///< third part of the UUID data
    unsigned char m_data4[8]; ///< fourth part of the UUID data

#ifdef __cplusplus
    /// operator overloaded function for equality comparison
    /// \param otherUUID the item being compared
    /// \return true if UUIDs are equal otherwise false
    bool operator==(const GPA_UUID& otherUUID)
    {
        bool isEqual = true;
        isEqual &= m_data1 == otherUUID.m_data1;
        isEqual &= m_data2 == otherUUID.m_data2;
        isEqual &= m_data3 == otherUUID.m_data3;
        isEqual &= m_data4[0] == otherUUID.m_data4[0];
        isEqual &= m_data4[1] == otherUUID.m_data4[1];
        isEqual &= m_data4[2] == otherUUID.m_data4[2];
        isEqual &= m_data4[3] == otherUUID.m_data4[3];
        isEqual &= m_data4[4] == otherUUID.m_data4[4];
        isEqual &= m_data4[5] == otherUUID.m_data4[5];
        isEqual &= m_data4[6] == otherUUID.m_data4[6];
        isEqual &= m_data4[7] == otherUUID.m_data4[7];
        return isEqual;
    }
#endif
} GPA_UUID;
#endif

// Type definitions
typedef float            gpa_float32; ///< GPA specific type for 32-bit float
typedef double           gpa_float64; ///< GPA specific type for 64-bit float
typedef unsigned char    gpa_uint8;   ///< GPA specific type for 8-bit unsigned integer
typedef unsigned short   gpa_uint16;  ///< GPA specific type for 16-bit unsigned integer
typedef unsigned int     gpa_uint32;  ///< GPA specific type for 32-bit unsigned integer

#ifdef _WIN32
    typedef unsigned __int64 gpa_uint64; ///< GPA specific type for 64-bit unsigned integer
#endif // _WIN32

#ifdef __linux__

    #ifndef GPALIB_DECL
        #ifdef __cplusplus
            #define GPALIB_DECL extern "C"
        #else
            #define GPALIB_DECL
        #endif // _cplusplus
    #endif

    typedef unsigned int       UINT;       ///< GPA specific type to define UINT on Linux
    typedef unsigned long long gpa_uint64; ///< GPA specific type for 64-bit unsigned integer

    #define UNREFERENCED_PARAMETER(x)

    #define _strcmpi(a, b) strcasecmp(a, b)
    #define _stricmp(a, b) strcasecmp(a, b)

    // for now, just use non secure version for Linux
    #define strcpy_s(dst, ndst, src) strcpy(dst, src)
    #define strcat_s(dst, ndst, src) strcat(dst, src)
    #define strtok_s(a, b, c) strtok(a, b)

    #ifndef TRUE
        #define TRUE 1
    #endif

    #ifndef FALSE
        #define FALSE 0
    #endif

#endif // __linux__

// Limit definitions

/// Macro for max uint32
#define GPA_UINT32_MAX UINT_MAX

/// Macro for max uint64
#define GPA_UINT64_MAX ULLONG_MAX

/// Macro to define opaque pointer types
#define GPA_DEFINE_OBJECT(ObjectType) typedef struct _GPA_##ObjectType* GPA_##ObjectType;

/// Context ID opaque pointer type
GPA_DEFINE_OBJECT(ContextId)

/// Session ID opaque pointer type
GPA_DEFINE_OBJECT(SessionId)

/// Command List ID opaque pointer type
GPA_DEFINE_OBJECT(CommandListId)

/// Macro for null command list
#define GPA_NULL_COMMAND_LIST NULL

/// Status enumerations
typedef enum
{
    GPA_STATUS_OK = 0,
    GPA_STATUS_RESULT_NOT_READY = 1,
    GPA_STATUS_MAX = GPA_STATUS_RESULT_NOT_READY,
    GPA_STATUS_ERROR_NULL_POINTER = -1,
    GPA_STATUS_ERROR_CONTEXT_NOT_OPEN = -2,
    GPA_STATUS_ERROR_CONTEXT_ALREADY_OPEN = -3,
    GPA_STATUS_ERROR_INDEX_OUT_OF_RANGE = -4,
    GPA_STATUS_ERROR_COUNTER_NOT_FOUND = -5,
    GPA_STATUS_ERROR_ALREADY_ENABLED = -6,
    GPA_STATUS_ERROR_NO_COUNTERS_ENABLED = -7,
    GPA_STATUS_ERROR_NOT_ENABLED = -8,
    GPA_STATUS_ERROR_COMMAND_LIST_ALREADY_ENDED = -9,
    GPA_STATUS_ERROR_COMMAND_LIST_ALREADY_STARTED = -10,
    GPA_STATUS_ERROR_COMMAND_LIST_NOT_ENDED = -11,
    GPA_STATUS_ERROR_NOT_ENOUGH_PASSES = -12,
    GPA_STATUS_ERROR_SAMPLE_NOT_STARTED = -13,
    GPA_STATUS_ERROR_SAMPLE_ALREADY_STARTED = -14,
    GPA_STATUS_ERROR_SAMPLE_NOT_ENDED = -15,
    GPA_STATUS_ERROR_CANNOT_CHANGE_COUNTERS_WHEN_SAMPLING = -16,
    GPA_STATUS_ERROR_SESSION_NOT_FOUND = -17,
    GPA_STATUS_ERROR_SAMPLE_NOT_FOUND = -18,
    GPA_STATUS_ERROR_CONTEXT_NOT_FOUND = -19,
    GPA_STATUS_ERROR_COMMAND_LIST_NOT_FOUND = -20,
    GPA_STATUS_ERROR_READING_SAMPLE_RESULT = -21,
    GPA_STATUS_ERROR_VARIABLE_NUMBER_OF_SAMPLES_IN_PASSES = -22,
    GPA_STATUS_ERROR_FAILED = -23,
    GPA_STATUS_ERROR_HARDWARE_NOT_SUPPORTED = -24,
    GPA_STATUS_ERROR_DRIVER_NOT_SUPPORTED = -25,
    GPA_STATUS_ERROR_API_NOT_SUPPORTED = -26,
    GPA_STATUS_ERROR_INVALID_PARAMETER = -27,
    GPA_STATUS_ERROR_LIB_LOAD_FAILED = -28,
    GPA_STATUS_ERROR_LIB_LOAD_MAJOR_VERSION_MISMATCH = -29,
    GPA_STATUS_ERROR_LIB_LOAD_MINOR_VERSION_MISMATCH = -30,
    GPA_STATUS_ERROR_GPA_NOT_INITIALIZED = -31,
    GPA_STATUS_ERROR_GPA_ALREADY_INITIALIZED = -32,
    GPA_STATUS_ERROR_SAMPLE_IN_SECONDARY_COMMAND_LIST = -33,
    GPA_STATUS_ERROR_INCOMPATIBLE_SAMPLE_TYPES = -34,
    GPA_STATUS_ERROR_SESSION_ALREADY_STARTED = -35,
    GPA_STATUS_ERROR_SESSION_NOT_STARTED = -36,
    GPA_STATUS_ERROR_SESSION_NOT_ENDED = -37,
    GPA_STATUS_MIN = GPA_STATUS_ERROR_SESSION_NOT_ENDED,
    // following are status codes used internally within GPUPerfAPI
    GPA_STATUS_INTERNAL = 256,
} GPA_Status;

/// Typedef for a set of flags that can be combined into an integer
typedef gpa_uint32 GPA_Flags;

/// Flags to pass into GPA_Initialize()
typedef enum
{
    GPA_INITIALIZE_DEFAULT_BIT = 0,                       ///< Initialize GPA using all default options
} GPA_Initialize_Bits;

/// Allows GPA_Initialize_Bits to be combined into a single parameter.
typedef GPA_Flags GPA_InitializeFlags;

/// Flags to pass into GPA_OpenContext()
typedef enum
{
    GPA_OPENCONTEXT_DEFAULT_BIT = 0,                    ///< Open contexts using all default options (all counters exposed, clocks are set to stable frequencies which are known to be power and thermal sustainable. The ratio between the engine and memory clock frequencies will be kept the same as much as possible)
    GPA_OPENCONTEXT_HIDE_PUBLIC_COUNTERS_BIT = 0x01,    ///< Prevent the public counters from being exposed
    GPA_OPENCONTEXT_HIDE_SOFTWARE_COUNTERS_BIT = 0x02,  ///< Prevent the software counters from being exposed
    GPA_OPENCONTEXT_HIDE_HARDWARE_COUNTERS_BIT = 0x04,  ///< Prevent the hardware counters from being exposed
    GPA_OPENCONTEXT_CLOCK_MODE_NONE_BIT = 0x0008,       ///< Clock frequencies are not altered and may vary widely during profiling based on GPU usage and other factors.
    GPA_OPENCONTEXT_CLOCK_MODE_PEAK_BIT = 0x0010,       ///< Clocks are set to peak frequencies. In most cases this is safe to do for short periods of time while profiling. However, the GPU clock frequencies could still be reduced from peak level under power and thermal constraints.
    GPA_OPENCONTEXT_CLOCK_MODE_MIN_MEMORY_BIT = 0x0020, ///< The memory clock frequency is set to the minimum level, while the engine clock is set to a power and thermal sustainable level.
    GPA_OPENCONTEXT_CLOCK_MODE_MIN_ENGINE_BIT = 0x0040  ///< The engine clock frequency is set to the minimum level, while the memory clock is set to a power and thermal sustainable level.
} GPA_OpenContext_Bits;

/// Allows GPA_OpenContext_Bits to be combined into a single parameter.
typedef GPA_Flags GPA_OpenContextFlags;

/// Value type definitions
typedef enum
{
    GPA_DATA_TYPE_FLOAT64, ///< Result will be a 64-bit float
    GPA_DATA_TYPE_UINT64,  ///< Result will be a 64-bit unsigned int
    GPA_DATA_TYPE__LAST    ///< Marker indicating last element
} GPA_Data_Type;

/// Result usage type definitions
typedef enum
{
    GPA_USAGE_TYPE_RATIO,        ///< Result is a ratio of two different values or types
    GPA_USAGE_TYPE_PERCENTAGE,   ///< Result is a percentage, typically within [0,100] range, but may be higher for certain counters
    GPA_USAGE_TYPE_CYCLES,       ///< Result is in clock cycles
    GPA_USAGE_TYPE_MILLISECONDS, ///< Result is in milliseconds
    GPA_USAGE_TYPE_BYTES,        ///< Result is in bytes
    GPA_USAGE_TYPE_ITEMS,        ///< Result is a count of items or objects (ie, vertices, triangles, threads, pixels, texels, etc)
    GPA_USAGE_TYPE_KILOBYTES,    ///< Result is in kilobytes
    GPA_USAGE_TYPE__LAST         ///< Marker indicating last element
} GPA_Usage_Type;

/// Logging type definitions
typedef enum
{
    GPA_LOGGING_NONE = 0x00,                                                                           ///< No logging
    GPA_LOGGING_ERROR = 0x01,                                                                          ///< Log errors
    GPA_LOGGING_MESSAGE = 0x02,                                                                        ///< Log messages
    GPA_LOGGING_ERROR_AND_MESSAGE = GPA_LOGGING_ERROR | GPA_LOGGING_MESSAGE,                           ///< Log errors and messages
    GPA_LOG_ERROR_AND_MESSAGE = GPA_LOGGING_ERROR_AND_MESSAGE,                                         ///< Log errors and messages - Backward Compatibility
    GPA_LOGGING_TRACE = 0x04,                                                                          ///< Log traces
    GPA_LOGGING_ERROR_AND_TRACE = GPA_LOGGING_ERROR | GPA_LOGGING_TRACE,                               ///< Log errors and traces
    GPA_LOGGING_MESSAGE_AND_TRACE = GPA_LOGGING_MESSAGE | GPA_LOGGING_TRACE,                           ///< Log messages traces
    GPA_LOGGING_ERROR_MESSAGE_AND_TRACE = GPA_LOGGING_ERROR | GPA_LOGGING_MESSAGE | GPA_LOGGING_TRACE, ///< Log errors and messages and traces
    GPA_LOGGING_ALL = 0xFF,                                                                            ///< Log all
    GPA_LOGGING_DEBUG_ERROR = 0x0100,                                                                  ///< Log debugging errors
    GPA_LOGGING_DEBUG_MESSAGE = 0x0200,                                                                ///< Log debugging messages
    GPA_LOGGING_DEBUG_TRACE = 0x0400,                                                                  ///< Log debugging traces
    GPA_LOGGING_DEBUG_COUNTERDEFS = 0x0800,                                                            ///< Log debugging counter defs
    GPA_LOGGING_DEBUG_ALL = 0xFF00                                                                     ///< Log all debugging
} GPA_Logging_Type;

/// APIs Supported (either publicly or internally) by GPUPerfAPI
typedef enum
{
    GPA_API__START,                      ///< Marker indicating first element
    GPA_API_DIRECTX_11 = GPA_API__START, ///< DirectX 11 API
    GPA_API_DIRECTX_12,                  ///< DirectX 12 API
    GPA_API_OPENGL,                      ///< OpenGL API
    GPA_API_OPENCL,                      ///< OpenCL API
    GPA_API_HSA,                         ///< HSA API
    GPA_API_VULKAN,                      ///< Vulkan API
    GPA_API_NO_SUPPORT,                  ///< APIs which are not yet supported or for which support has been removed
    GPA_API__LAST                        ///< Marker indicating last element
} GPA_API_Type;

/// This enum needs to be kept up to date with GDT_HW_GENERATION in DeviceInfo.h
typedef enum
{
    GPA_HW_GENERATION_NONE,                                    ///< undefined hw generation
    GPA_HW_GENERATION_NVIDIA,                                  ///< Used for nvidia cards by GPA
    GPA_HW_GENERATION_INTEL,                                   ///< Used for Intel cards by GPA
    GPA_HW_GENERATION_GFX6,                                    ///< GFX IP 6
    GPA_HW_GENERATION_SOUTHERNISLAND = GPA_HW_GENERATION_GFX6, ///< For backwards compatibility
    GPA_HW_GENERATION_GFX7,                                    ///< GFX IP 7
    GPA_HW_GENERATION_SEAISLAND = GPA_HW_GENERATION_GFX7,      ///< For backwards compatibility
    GPA_HW_GENERATION_GFX8,                                    ///< GFX IP 8
    GPA_HW_GENERATION_VOLCANICISLAND = GPA_HW_GENERATION_GFX8, ///< For backwards compatibility
    GPA_HW_GENERATION_GFX9,                                    ///< GFX IP 9
    GPA_HW_GENERATION__LAST                                    ///< Marker indicating last element
} GPA_Hw_Generation;

/// Command list / command buffer types
typedef enum
{
    GPA_COMMAND_LIST_NONE,      ///< no command list, used for APIs that do not directly expose command lists or command buffers (DirectX 11, OpenGL, OpenCL, HSA)
    GPA_COMMAND_LIST_PRIMARY,   ///< corresponds to DirectX 12 direct/compute/copy command list and Vulkan primary vkCommandBuffer
    GPA_COMMAND_LIST_SECONDARY, ///< corresponds to DirectX 12 bundle and Vulkan secondary vkCommandBuffer
    GPA_COMMAND_LIST__LAST      ///< Marker indicating last element
} GPA_Command_List_Type;

/// Counter sample types - used to indicate which sample types are supported by a counter
typedef enum
{
    GPA_COUNTER_SAMPLE_TYPE_DISCRETE, ///< Discrete counter type -- discrete counters provide a single value per workload measured
} GPA_Counter_Sample_Type;

/// Context Sample types -- used to indicate which sample types are supported by a context. A context can support any combination of these
typedef enum
{
    GPA_CONTEXT_SAMPLE_TYPE_DISCRETE_COUNTER = 0x01, ///< Discrete counters sample type -- discrete counters provide a single value per workload measured
} GPA_Context_Sample_Type_Bits;

/// Allows GPA_Context_Sample_Type_Bits to be combined into a single parameter.
typedef GPA_Flags GPA_ContextSampleTypeFlags;

/// Session Sample types -- used by the client to tell GPUPerfAPI which sample types will be created for a session
typedef enum
{
    GPA_SESSION_SAMPLE_TYPE_DISCRETE_COUNTER, ///< Discrete counters sample type -- discrete counters provide a single value per workload measured
} GPA_Session_Sample_Type;

#endif // _GPUPERFAPI_TYPES_H_
