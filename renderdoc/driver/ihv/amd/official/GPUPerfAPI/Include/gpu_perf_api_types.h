//==============================================================================
// Copyright (c) 2010-2025 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  Defines the data types and enumerations used by GPUPerfAPI.
///         This file does not need to be directly included by an application
///         that uses GPUPerfAPI.
//==============================================================================

#ifndef GPU_PERFORMANCE_API_GPU_PERF_API_TYPES_H_
#define GPU_PERFORMANCE_API_GPU_PERF_API_TYPES_H_

#include <limits.h>

#ifdef _WIN32
#include <Windows.h>
typedef HMODULE LibHandle;  ///< Typedef for HMODULE for loading the library on windows.
typedef GUID    GpaUuid;    ///< Typedef for Windows GUID definition.
#else
typedef void* LibHandle;  ///< Typedef for void* for loading the library on linux.

/// @brief Structure for holding UUID.
typedef struct _GpaUuid
{
    unsigned int   data_1;     ///< First part of the UUID data.
    unsigned short data_2;     ///< Second part of the UUID data.
    unsigned short data_3;     ///< Third part of the UUID data.
    unsigned char  data_4[8];  ///< Fourth part of the UUID data.

#ifdef __cplusplus
    /// @brief Operator overloaded function for equality comparison.
    ///
    /// @param other_uuid The item being compared.
    ///
    /// @return True if UUIDs are equal otherwise false.
    bool operator==(const _GpaUuid& other_uuid) const
    {
        bool is_equal = true;
        is_equal &= data_1 == other_uuid.data_1;
        is_equal &= data_2 == other_uuid.data_2;
        is_equal &= data_3 == other_uuid.data_3;
        is_equal &= data_4[0] == other_uuid.data_4[0];
        is_equal &= data_4[1] == other_uuid.data_4[1];
        is_equal &= data_4[2] == other_uuid.data_4[2];
        is_equal &= data_4[3] == other_uuid.data_4[3];
        is_equal &= data_4[4] == other_uuid.data_4[4];
        is_equal &= data_4[5] == other_uuid.data_4[5];
        is_equal &= data_4[6] == other_uuid.data_4[6];
        is_equal &= data_4[7] == other_uuid.data_4[7];
        return is_equal;
    }
#endif
} GpaUuid;
#endif
static_assert(sizeof(GpaUuid) == 16, "GpaUuid is expected to be a 16-byte packed structure");

typedef float          GpaFloat32;  ///< GPA specific type for 32-bit float.
typedef double         GpaFloat64;  ///< GPA specific type for 64-bit float.
typedef unsigned char  GpaUInt8;    ///< GPA specific type for 8-bit unsigned integer.
typedef unsigned short GpaUInt16;   ///< GPA specific type for 16-bit unsigned integer.
typedef unsigned int   GpaUInt32;   ///< GPA specific type for 32-bit unsigned integer.

#ifdef _WIN32
typedef unsigned __int64 GpaUInt64;  ///< GPA specific type for 64-bit unsigned integer.
#else                                // _WIN32
#ifndef GPA_LIB_DECL
#ifdef __cplusplus
#define GPA_LIB_DECL extern "C"
#else
#define GPA_LIB_DECL
#endif  // _cplusplus
#endif

typedef unsigned long long GpaUInt64;  ///< GPA specific type for 64-bit unsigned integer.

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x) (void)(x)
#endif

#define _strcmpi(a, b) strcasecmp(a, b)
#define _stricmp(a, b) strcasecmp(a, b)

#define strcpy_s(dst, ndst, src) strcpy(dst, src)
#define strcat_s(dst, ndst, src) strcat(dst, src)
#define strtok_s(a, b, c) strtok(a, b)
#define strnlen_s(a, b) strlen(a)
#define strncpy_s(a, b, c, d) strncpy(a, c, d)

#define wcscat_s(dest, dest_size, src) wcscat(dest, src)
#define wcscpy_s(dest, dest_size, src) wcscpy(dest, src)
#define wcsncpy_s(dest, dest_size, src, count) wcsncpy(dest, src, count)
#define wcsnlen_s(str, str_length) wcsnlen(str, str_length)

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#endif  // __linux__

/// Macro for max uint32.
#define GPA_UINT32_MAX UINT_MAX

/// Macro for max uint64.
#define GPA_UINT64_MAX ULLONG_MAX

/// Macro to define opaque pointer types.
#define GPA_DEFINE_OBJECT(ObjectType) typedef struct _Gpa##ObjectType* Gpa##ObjectType;

/// Context ID opaque pointer type.
GPA_DEFINE_OBJECT(ContextId)

/// Session ID opaque pointer type.
GPA_DEFINE_OBJECT(SessionId)

/// Command List ID opaque pointer type.
GPA_DEFINE_OBJECT(CommandListId)

/// Macro for null command list.
#define GPA_NULL_COMMAND_LIST NULL

/// @brief Status enumerations.
typedef enum
{
    kGpaStatusOk                                    = 0,
    kGpaStatusResultNotReady                        = 1,
    kGpaStatusMax                                   = kGpaStatusResultNotReady,
    kGpaStatusErrorNullPointer                      = -1,
    kGpaStatusErrorContextNotOpen                   = -2,
    kGpaStatusErrorContextAlreadyOpen               = -3,
    kGpaStatusErrorIndexOutOfRange                  = -4,
    kGpaStatusErrorCounterNotFound                  = -5,
    kGpaStatusErrorAlreadyEnabled                   = -6,
    kGpaStatusErrorNoCountersEnabled                = -7,
    kGpaStatusErrorNotEnabled                       = -8,
    kGpaStatusErrorCommandListAlreadyEnded          = -9,
    kGpaStatusErrorCommandListAlreadyStarted        = -10,
    kGpaStatusErrorCommandListNotEnded              = -11,
    kGpaStatusErrorNotEnoughPasses                  = -12,
    kGpaStatusErrorSampleNotStarted                 = -13,
    kGpaStatusErrorSampleAlreadyStarted             = -14,
    kGpaStatusErrorSampleNotEnded                   = -15,
    kGpaStatusErrorCannotChangeCountersWhenSampling = -16,
    kGpaStatusErrorSessionNotFound                  = -17,
    kGpaStatusErrorSampleNotFound                   = -18,
    kGpaStatusErrorContextNotFound                  = -19,
    kGpaStatusErrorCommandListNotFound              = -20,
    kGpaStatusErrorReadingSampleResult              = -21,
    kGpaStatusErrorVariableNumberOfSamplesInPasses  = -22,
    kGpaStatusErrorFailed                           = -23,
    kGpaStatusErrorHardwareNotSupported             = -24,
    kGpaStatusErrorDriverNotSupported               = -25,
    kGpaStatusErrorApiNotSupported                  = -26,
    kGpaStatusErrorInvalidParameter                 = -27,
    kGpaStatusErrorLibLoadFailed                    = -28,
    kGpaStatusErrorLibLoadMajorVersionMismatch      = -29,
    kGpaStatusErrorLibLoadMinorVersionMismatch      = -30,
    kGpaStatusErrorGpaNotInitialized                = -31,
    kGpaStatusErrorGpaAlreadyInitialized            = -32,
    kGpaStatusErrorSampleInSecondaryCommandList     = -33,
    kGpaStatusErrorIncompatibleSampleTypes          = -34,
    kGpaStatusErrorSessionAlreadyStarted            = -35,
    kGpaStatusErrorSessionNotStarted                = -36,
    kGpaStatusErrorSessionNotEnded                  = -37,
    kGpaStatusErrorInvalidDataType                  = -38,
    kGpaStatusErrorInvalidCounterEquation           = -39,
    kGpaStatusErrorTimeout                          = -40,
    kGpaStatusErrorLibAlreadyLoaded                 = -41,
    kGpaStatusErrorOtherSessionActive               = -42,
    kGpaStatusErrorException                        = -43,
    kGpaStatusErrorInvalidCounterGroupData          = -44,
    kGpaStatusMin                                   = kGpaStatusErrorInvalidCounterGroupData,
    kGpaStatusInternal                              = 256,  ///< Status codes used internally within GPUPerfAPI.
} GpaStatus;

/// Typedef for a set of flags that can be combined into an integer.
typedef GpaUInt32 GpaFlags;

/// @brief Flags to pass in when initializing GPA.
typedef enum
{
    kGpaInitializeDefaultBit                  = 0,     ///< Initialize GPA using all default options.
    kGpaInitializeSimultaneousQueuesEnableBit = 0x01,  ///< Enable streaming counter and SQTT data to include data from all hardware queues.
    kGpaInitializeEnableSqttBit =
        0x03  ///< Perform extra initialization required in order to collect SQTT data -- this also enables the kGpaInitializeSimultaneousQueuesEnableBit.
} GpaInitializeBits;

/// Allows GpaInitializeBits to be combined into a single parameter.
typedef GpaFlags GpaInitializeFlags;

/// @brief Flags to pass in when opening a GPA context.
typedef enum
{
    kGpaOpenContextDefaultBit =
        0,  ///< Open contexts using all default options (all counters exposed, clocks are set to stable frequencies which are known to be power and thermal sustainable. The ratio between the engine and memory clock frequencies will be kept the same as much as possible).
    kGpaOpenContextHideDerivedCountersBit = 0x01,                                   ///< Prevent the derived counters from being exposed.
    kGpaOpenContextHidePublicCountersBit  = kGpaOpenContextHideDerivedCountersBit,  ///< For backwards compatibility.
    kGpaOpenContextClockModeNoneBit = 0x0008,  ///< Clock frequencies are not altered and may vary widely during profiling based on GPU usage and other factors.
    kGpaOpenContextClockModePeakBit =
        0x0010,  ///< Clocks are set to peak frequencies. In most cases this is safe to do for short periods of time while profiling. However, the GPU clock frequencies could still be reduced from peak level under power and thermal constraints.
    kGpaOpenContextClockModeMinMemoryBit =
        0x0020,  ///< The memory clock frequency is set to the minimum level, while the engine clock is set to a power and thermal sustainable level.
    kGpaOpenContextClockModeMinEngineBit =
        0x0040,  ///< The engine clock frequency is set to the minimum level, while the memory clock is set to a power and thermal sustainable level.
    kGpaOpenContextEnableHardwareCountersBit = 0x0080  ///< Include the hardware counters when exposing counters.
} GpaOpenContextBits;

/// Allows GpaOpenContextBits to be combined into a single parameter.
typedef GpaFlags GpaOpenContextFlags;

/// @brief Value type definitions.
typedef enum
{
    kGpaDataTypeFloat64,  ///< Result will be a 64-bit float.
    kGpaDataTypeUint64,   ///< Result will be a 64-bit unsigned int.
    kGpaDataTypeLast      ///< Marker indicating last element.
} GpaDataType;

/// @brief Result usage type definitions.
typedef enum
{
    kGpaUsageTypeRatio,         ///< Result is a ratio of two different values or types.
    kGpaUsageTypePercentage,    ///< Result is a percentage, typically within [0,100] range, but may be higher for certain counters.
    kGpaUsageTypeCycles,        ///< Result is in clock cycles.
    kGpaUsageTypeMilliseconds,  ///< Result is in milliseconds.
    kGpaUsageTypeBytes,         ///< Result is in bytes.
    kGpaUsageTypeItems,         ///< Result is a count of items or objects (ie, vertices, triangles, threads, pixels, texels, etc).
    kGpaUsageTypeKilobytes,     ///< Result is in kilobytes.
    kGpaUsageTypeNanoseconds,   ///< Result is in nanoseconds.
    kGpaUsageTypeLast           ///< Marker indicating last element.
} GpaUsageType;

/// @brief Logging type definitions.
typedef enum
{
    kGpaLoggingNone                    = 0x00,                                                      ///< No logging.
    kGpaLoggingError                   = 0x01,                                                      ///< Log errors.
    kGpaLoggingMessage                 = 0x02,                                                      ///< Log messages.
    kGpaLoggingErrorAndMessage         = kGpaLoggingError | kGpaLoggingMessage,                     ///< Log errors and messages.
    kGpaLogErrorAndMessage             = kGpaLoggingErrorAndMessage,                                ///< Log errors and messages - Backward Compatibility.
    kGpaLoggingTrace                   = 0x04,                                                      ///< Log traces.
    kGpaLoggingErrorAndTrace           = kGpaLoggingError | kGpaLoggingTrace,                       ///< Log errors and traces.
    kGpaLoggingMessageAndTrace         = kGpaLoggingMessage | kGpaLoggingTrace,                     ///< Log messages traces.
    kGpaLoggingErrorMessageAndTrace    = kGpaLoggingError | kGpaLoggingMessage | kGpaLoggingTrace,  ///< Log errors and messages and traces.
    kGpaLoggingAll                     = 0xFF,                                                      ///< Log all.
    kGpaLoggingDebugError              = 0x0100,                                                    ///< Log debugging errors.
    kGpaLoggingDebugMessage            = 0x0200,                                                    ///< Log debugging messages.
    kGpaLoggingDebugTrace              = 0x0400,                                                    ///< Log debugging traces.
    kGpaLoggingDebugCounterDefinitions = 0x0800,                                                    ///< Log debugging counter definitions.
    kGpaLoggingInternal                = 0x1000,                                                    ///< Log internal GPA.
    kGpaLoggingDebugAll                = 0xFF00                                                     ///< Log all debugging.
} GpaLoggingType;

/// @brief APIs Supported (either publicly or internally) by GPUPerfAPI.
typedef enum
{
    kGpaApiStart,                     ///< Marker indicating first element.
    kGpaApiDirectx11 = kGpaApiStart,  ///< DirectX 11 API.
    kGpaApiDirectx12,                 ///< DirectX 12 API.
    kGpaApiOpengl,                    ///< OpenGL API.
    kGpaApiVulkan,                    ///< Vulkan API.
    kGpaApiNoSupport,                 ///< APIs which are not yet supported or for which support has been removed.
    kGpaApiLast                       ///< Marker indicating last element.
} GpaApiType;

/// @brief This enum needs to be kept up to date with GDT_HW_GENERATION in DeviceInfo.h.
typedef enum
{
    kGpaHwGenerationNone,                                   ///< Undefined hw generation.
    kGpaHwGenerationNvidia,                                 ///< Used for nvidia cards by GPA.
    kGpaHwGenerationIntel,                                  ///< Used for Intel cards by GPA.
    kGpaHwGenerationGfx6,                                   ///< GFX IP 6.
    kGpaHwGenerationSouthernIsland = kGpaHwGenerationGfx6,  ///< For backwards compatibility.
    kGpaHwGenerationGfx7,                                   ///< GFX IP 7.
    kGpaHwGenerationSeaIsland = kGpaHwGenerationGfx7,       ///< For backwards compatibility.
    kGpaHwGenerationGfx8,                                   ///< GFX IP 8.
    kGpaHwGenerationVolcanicIsland = kGpaHwGenerationGfx8,  ///< For backwards compatibility.
    kGpaHwGenerationGfx9,                                   ///< GFX IP 9.
    kGpaHwGenerationGfx10,                                  ///< GFX IP 10.
    kGpaHwGenerationGfx103,                                 ///< GFX IP 10.3.
    kGpaHwGenerationGfx11,                                  ///< GFX IP 11.
    kGpaHwGenerationCdna,                                   ///< CDNA
    kGpaHwGenerationCdna2,                                  ///< CDNA 2
    kGpaHwGenerationCdna3,                                  ///< CDNA 3
    kGpaHwGenerationGfx12,                                  ///< GFX IP 12.
    kGpaHwGenerationLast                                    ///< Marker indicating last element.
} GpaHwGeneration;

/// @brief Command list / command buffer types.
typedef enum
{
    kGpaCommandListNone,       ///< No command list, used for APIs that do not directly expose command lists or command buffers (DirectX 11, OpenGL).
    kGpaCommandListPrimary,    ///< Corresponds to DirectX 12 direct/compute/copy command list and Vulkan primary vkCommandBuffer.
    kGpaCommandListSecondary,  ///< Corresponds to DirectX 12 bundle and Vulkan secondary vkCommandBuffer.
    kGpaCommandListLast        ///< Marker indicating last element.
} GpaCommandListType;

/// @brief Counter sample types - used to indicate which sample types are supported by a counter.
typedef enum
{
    kGpaCounterSampleTypeDiscrete  = 0x1,  ///< Discrete counter type -- discrete counters provide a single value per workload measured.
    kGpaCounterSampleTypeStreaming = 0x2,  ///< Streaming counter type -- streaming counters provide interval-based multiple values per workload measured.
} GpaCounterSampleType;

/// @brief Context Sample types -- used to indicate which sample types are supported by a context. A context can support any combination of these.
typedef enum
{
    kGpaContextSampleTypeDiscreteCounter = 0x01,  ///< Discrete counters sample type -- discrete counters provide a single value per workload measured.
    kGpaContextSampleTypeStreamingCounter =
        0x02,  ///< Streaming counters sample type -- streaming counters provide interval-based multiple values per workload measured.
    kGpaContextSampleTypeSqtt =
        0x04,  ///< SQTT sample type -- provides detailed wave-level SQTT information per workload measured. For some driver stacks, the SQTT-data may be wrapped in an RGP-file format.
} GpaContextSampleTypeBits;

/// @brief Allows GpaContextSampleTypeBits to be combined into a single parameter.
typedef GpaFlags GpaContextSampleTypeFlags;

/// @brief Session Sample types -- used by the client to tell GPUPerfAPI which sample types will be created for a session.
typedef enum
{
    kGpaSessionSampleTypeDiscreteCounter,  ///< Discrete counters sample type -- discrete counters provide a single value per workload measured.
    kGpaSessionSampleTypeStreamingCounter,  ///< Streaming counters sample type -- streaming counters provide interval-based multiple values per workload measured.
    kGpaSessionSampleTypeSqtt,  ///< SQTT sample type -- provides detailed wave-level SQTT information per workload measured. For some driver stacks, the SQTT-data may be wrapped in an RGP-file format.
    kGpaSessionSampleTypeStreamingCounterAndSqtt,  ///< Streaming counters and SQTT are enabled.
    kGpaSessionSampleTypeLast                      ///< Marker indicating last element.

} GpaSessionSampleType;

/// @brief Type used to define the mask of instructions included in SQTT data.
typedef enum
{
    kGpaSqttInstructionTypeNone = 0x00,        ///< Exclude all instructions from SQTT data.
    kGpaSqttInstructionTypeAll  = 0x7FFFFFFF,  ///< Include all instructions in SQTT data.
} GpaSqttInstructionBits;

/// Allows GpaSqttInstructionBits to be combined into a single parameter.
typedef GpaFlags GpaSqttInstructionFlags;

/// GPA SPM counter info.
typedef struct GpaSpmCounterInfo
{
    GpaUInt32 gpu_block_id;        ///< GPU block identifier.
    GpaUInt32 gpu_block_instance;  ///< GPU block instance.
    GpaUInt32 data_offset;         ///< Offset from the start of counterDeltaValues for the deltas for this counter instance.
    GpaUInt32 event_index;         ///< Event index.
} GpaSpmCounterInfo;

/// GPA SPM data
typedef struct GpaSpmData
{
    GpaUInt32                number_of_timestamps;              ///< Number of timestamps.
    GpaUInt32                number_of_spm_counter_info;        ///< Number of SpmCounterInfo structs.
    GpaUInt32                number_of_counter_data;            ///< Number of CounterData values.
    GpaUInt32                number_of_bytes_per_counter_data;  ///< Number of bytes for each CounterData entry.
    const GpaUInt64*         timestamps;                        ///< Array of number_of_timestamps number of timestamps.
    const GpaSpmCounterInfo* spm_counter_info;                  ///< Array of number_of_spm_counter_info number of SpmCounterInfo.
    const GpaUInt16* counter_data_16bit;  ///< Array of number_of_spm_counter_info * number_of_timestamps counter delta values (may be 16 or 32-bit values).
} GpaSpmData;

#endif  // GPU_PERFORMANCE_API_GPU_PERF_API_TYPES_H_
