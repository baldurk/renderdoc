//==============================================================================
// Copyright (c) 2012-2021 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  Interface to access to the available counters in GPUPerfAPI.
//==============================================================================

#ifndef GPU_PERF_API_GPU_PERF_API_COUNTERS_H_
#define GPU_PERF_API_GPU_PERF_API_COUNTERS_H_

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4201)
#endif

#ifndef __cplusplus
#include <stdbool.h>
#endif

/// macro to export public API functions
#ifndef GPU_PERF_API_COUNTERS_DECL
#ifdef _WIN32
#ifdef __cplusplus
#define GPU_PERF_API_COUNTERS_DECL extern "C" __declspec(dllimport)
#else
#define GPU_PERF_API_COUNTERS_DECL __declspec(dllimport)
#endif
#else  //_LINUX
#define GPU_PERF_API_COUNTERS_DECL extern
#endif
#endif

#include "gpu_perf_api_types.h"

/// Virtual Context ID opaque pointer type
GPA_DEFINE_OBJECT(CounterContext)

/// Gpa counter library major version
#define GPA_COUNTER_LIB_FUNC_TABLE_MAJOR_VERSION 3

/// Gpa counter library minor version
#define GPA_COUNTER_LIB_FUNC_TABLE_MINOR_VERSION (sizeof(struct _GpaCounterLibFuncTable))

/// Gpa hardware blocks
typedef enum
{
    kGpaHwBlockCpf = 0,  ///< The Gpa hardware block is CPF
    kGpaHwBlockIa,       ///< The Gpa hardware block is IA
    kGpaHwBlockVgt,      ///< The Gpa hardware block is VGT
    kGpaHwBlockPa,       ///< The Gpa hardware block is PA
    kGpaHwBlockSc,       ///< The Gpa hardware block is SC
    kGpaHwBlockSpi,      ///< The Gpa hardware block is SPI
    kGpaHwBlockSq,       ///< The Gpa hardware block is SQ
    kGpaHwBlockSx,       ///< The Gpa hardware block is SX
    kGpaHwBlockTa,       ///< The Gpa hardware block is TA
    kGpaHwBlockTd,       ///< The Gpa hardware block is TD
    kGpaHwBlockTcp,      ///< The Gpa hardware block is TCP
    kGpaHwBlockTcc,      ///< The Gpa hardware block is TCC
    kGpaHwBlockTca,      ///< The Gpa hardware block is TCA
    kGpaHwBlockDb,       ///< The Gpa hardware block is DB
    kGpaHwBlockCb,       ///< The Gpa hardware block is CB
    kGpaHwBlockGds,      ///< The Gpa hardware block is GDS
    kGpaHwBlockSrbm,     ///< The Gpa hardware block is SRBM
    kGpaHwBlockGrbm,     ///< The Gpa hardware block is GRBM
    kGpaHwBlockGrbmse,   ///< The Gpa hardware block is GRBMSE
    kGpaHwBlockRlc,      ///< The Gpa hardware block is RLC
    kGpaHwBlockDma,      ///< The Gpa hardware block is DMA
    kGpaHwBlockMc,       ///< The Gpa hardware block is MC
    kGpaHwBlockCpg,      ///< The Gpa hardware block is CPG
    kGpaHwBlockCpc,      ///< The Gpa hardware block is CPC
    kGpaHwBlockWd,       ///< The Gpa hardware block is WD
    kGpaHwBlockTcs,      ///< The Gpa hardware block is TCS
    kGpaHwBlockAtc,      ///< The Gpa hardware block is ATC
    kGpaHwBlockAtcl2,    ///< The Gpa hardware block is ATCL2
    kGpaHwBlockMcvml2,   ///< The Gpa hardware block is MCVML2
    kGpaHwBlockEa,       ///< The Gpa hardware block is EA
    kGpaHwBlockRpb,      ///< The Gpa hardware block is RPB
    kGpaHwBlockRmi,      ///< The Gpa hardware block is RMI
    kGpaHwBlockUmcch,    ///< The Gpa hardware block is UMCCH
    kGpaHwBlockGe,       ///< The Gpa hardware block is GE
    kGpaHwBlockGl1A,     ///< The Gpa hardware block is GL1A
    kGpaHwBlockGl1C,     ///< The Gpa hardware block is GL1C
    kGpaHwBlockGl1Cg,    ///< The Gpa hardware block is GL1CG
    kGpaHwBlockGl2A,     ///< The Gpa hardware block is GL2A
    kGpaHwBlockGl2C,     ///< The Gpa hardware block is GL2C
    kGpaHwBlockCha,      ///< The Gpa hardware block is CHA
    kGpaHwBlockChc,      ///< The Gpa hardware block is CHC
    kGpaHwBlockChcg,     ///< The Gpa hardware block is CHCG
    kGpaHwBlockGus,      ///< The Gpa hardware block is GUS
    kGpaHwBlockGcr,      ///< The Gpa hardware block is GCR
    kGpaHwBlockPh,       ///< The Gpa hardware block is PH
    kGpaHwBlockUtcl1,    ///< The Gpa hardware block is UTCL1
    kGpaHwBlockGedist,   ///< The Gpa hardware block is GEDIST
    kGpaHwBlockGese,     ///< The Gpa hardware block is GESE
    kGpaHwBlockDfmall,   ///< The Gpa hardware block is DFMALL
    kGpaHwBlockCount,    ///< Count
} GpaHwBlock;

/// Gpa shader masks
typedef enum
{
    kGpaShaderMaskPs,   ///< The Gpa PS shader mask
    kGpaShaderMaskVs,   ///< The Gpa VS shader mask
    kGpaShaderMaskGs,   ///< The Gpa GS shader mask
    kGpaShaderMaskEs,   ///< The Gpa ES shader mask
    kGpaShaderMaskHs,   ///< The Gpa HS shader mask
    kGpaShaderMaskLs,   ///< The Gpa LS shader mask
    kGpaShaderMaskCs,   ///< The Gpa CS shader mask
    kGpaShaderMaskAll,  ///< The Gpa all shader mask
} GpaShaderMask;

/// Gpa hardware attribute types
typedef enum
{
    kGpaHardwareAttributeNumShaderEngines,        ///< number of shader engines
    kGpaHardwareAttributeNumShaderArrays,         ///< number of shader arrays
    kGpaHardwareAttributeNumSimds,                ///< number of simds
    kGpaHardwareAttributeNumComputeUnits,         ///< number of compute units
    kGpaHardwareAttributeNumRenderBackends,       ///< number of render backends
    kGpaHardwareAttributeClocksPerPrimitive,      ///< clocks per primitive
    kGpaHardwareAttributeNumPrimitivePipes,       ///< number of primitive pipes
    kGpaHardwareAttributeTimestampFrequency,      ///< timestamp frequency
    kGpaHardwareAttributePeakVerticesPerClock,    ///< peak vertices per clock
    kGpaHardwareAttributePeakPrimitivesPerClock,  ///< peak primitives per clock
    kGpaHardwareAttributePeakPixelsPerClock       ///< peak pixels per clocks
} GpaHardwareAttributeType;

/// Gpa Hardware attribute
typedef struct _GpaHardwareAttribute
{
    GpaHardwareAttributeType gpa_hardware_attribute_type;   ///< gpa hardware attribute type
    GpaUInt32                gpa_hardware_attribute_value;  ///< gpa hardware attribute value
} GpaHardwareAttribute;

/// Gpa counter context hardware info
typedef struct _GpaCounterContextHardwareInfo
{
    GpaUInt32 vendor_id;    ///< vendor Id
    GpaUInt32 device_id;    ///< device Id
    GpaUInt32 revision_id;  ///< revision Id

    GpaHardwareAttribute* gpa_hardware_attributes;       ///< pointer to array of hardware attributes
    GpaUInt32             gpa_hardware_attribute_count;  ///< number of hardware attributes

} GpaCounterContextHardwareInfo;

/// Hardware counter info
typedef struct _GpaHwCounter
{
    bool is_timing_block;  ///< flag indicating time based derived counter
    union
    {
        union
        {
            GpaUInt32 gpu_time_bottom_to_bottom_duration;  ///< index of gpu_time_bottom_to_bottom_duration counter
            GpaUInt32 gpu_time_bottom_to_bottom_start;     ///< index of gpu_time_bottom_to_bottom_duration counter
            GpaUInt32 gpu_time_bottom_to_bottom_end;       ///< index of gpu_time_bottom_to_bottom_duration counter
            GpaUInt32 gpu_time_top_to_bottom_duration;     ///< index of gpu_time_top_to_bottom_duration counter
            GpaUInt32 gpu_time_top_to_bottom_start;        ///< index of gpu_time_top_to_bottom_start counter
            GpaUInt32 gpu_time_top_to_bottom_end;          ///< index of gpu_time_top_to_bottom_end counter
        };

        struct
        {
            GpaHwBlock    gpa_hw_block;           ///< Gpa hardware block
            GpaUInt32     gpa_hw_block_instance;  ///< Gpa hardware block 0-based instance index
            GpaUInt32     gpa_hw_block_event_id;  ///< Gpa hardware block 0-based event id
            GpaShaderMask gpa_shader_mask;        ///< Gpa shader mask, only used if SQ block is queried
        };
    };

} GpaHwCounter;

/// Gpa derived counter info
typedef struct _GpaDerivedCounterInfo
{
    GpaHwCounter* gpa_hw_counters;       ///< hardware counters
    GpaUInt32     gpa_hw_counter_count;  ///< number of hardware counter
    GpaUsageType  counter_usage_type;    ///< usage of the derived counter

} GpaDerivedCounterInfo;

/// Gpa counter info -- can be a derived counter or a hardware counter
typedef struct _GpaCounterInfo
{
    bool is_derived_counter;  ///< flag indicating this is a derived counter
    union
    {
        GpaDerivedCounterInfo* gpa_derived_counter;  ///< derived counter
        GpaHwCounter*          gpa_hw_counter;       ///< hardware counter
    };
} GpaCounterInfo;

/// Gpa counter parameter
typedef struct _GpaCounterParam
{
    bool is_derived_counter;  ///< flag indicating derived counter
    union
    {
        const char*  derived_counter_name;  ///< derived counter name
        GpaHwCounter gpa_hw_counter;        ///< hardware counter
    };
} GpaCounterParam;

///  Gpa counters in a pass
typedef struct _GpaPassCounter
{
    GpaUInt32  pass_index;       ///< pass index
    GpaUInt32  counter_count;    ///< number of counters
    GpaUInt32* counter_indices;  ///< indices of the counters
} GpaPassCounter;

/// \brief Gets the Gpa Counter lib version
///
/// \param[out] major_version The value that will hold the major version of GPA upon successful execution.
/// \param[out] minor_version The value that will hold the minor version of GPA upon successful execution.
/// \param[out] build_number The value that will hold the build number of GPA upon successful execution.
/// \param[out] update_version The value that will hold the update version of GPA upon successful execution.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetVersion(GpaUInt32* major_version,
                                                             GpaUInt32* minor_version,
                                                             GpaUInt32* build_number,
                                                             GpaUInt32* update_version);

/// typedef for GpaCounterLibGetVersion function pointer
typedef GpaStatus (*GpaCounterLibGetVersionPtrType)(GpaUInt32*, GpaUInt32*, GpaUInt32*, GpaUInt32*);

/// \brief Gets the Gpa counter library function table.
///
/// \param[out] gpa_counter_lib_function_table pointer to the Gpa counter library function table.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetFuncTable(void* gpa_counter_lib_function_table);

/// typedef for GpaCounterLibGetFuncTable function pointer
typedef GpaStatus (*GpaCounterLibGetFuncTablePtrType)(void*);

/// \brief Creates a virtual context to interrogate the counter information.
///
/// \param[in] api the api whose available counters are requested.
/// \param[in] gpa_counter_context_hardware_info counter context hardware info.
/// \param[in] context_flags Flags used to initialize the context. Should be a combination of GPA_OpenContext_Bits.
/// \param[in] generate_asic_specific_counters Flag that indicates whether the counters should be ASIC specific, if available.
/// \param[out] gpa_virtual_context Unique identifier of the opened virtual context.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibOpenCounterContext(GpaApiType                    api,
                                                                     GpaCounterContextHardwareInfo gpa_counter_context_hardware_info,
                                                                     GpaOpenContextFlags           context_flags,
                                                                     GpaUInt8                      generate_asic_specific_counters,
                                                                     GpaCounterContext*            gpa_virtual_context);

/// typedef for GpaCounterLibOpenCounterContext function pointer
typedef GpaStatus (*GpaCounterLibOpenCounterContextPtrType)(GpaApiType, GpaCounterContextHardwareInfo, GpaOpenContextFlags, GpaUInt8, GpaCounterContext*);

/// \brief Closes the specified context, which ends access to GPU performance counters.
///
/// Counter functions should not be called again until the counters are reopened with GpaCounterLib_OpenCounterContext.
/// \param[in] gpa_virtual_context Unique identifier of the opened virtual context.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibCloseCounterContext(const GpaCounterContext gpa_virtual_context);

/// typedef for GpaCounterLibCloseCounterContextPtrType function pointer
typedef GpaStatus (*GpaCounterLibCloseCounterContextPtrType)(const GpaCounterContext);

/// \brief Gets the number of counters available.
///
/// \param[in] gpa_virtual_context Unique identifier of the opened virtual context.
/// \param[out] gpa_counter_count The value which will hold the count upon successful execution.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetNumCounters(const GpaCounterContext gpa_virtual_context, GpaUInt32* gpa_counter_count);

/// typedef for GpaCounterLibGetNumCountersPtr function pointer
typedef GpaStatus (*GpaCounterLibGetNumCountersPtrType)(const GpaCounterContext, GpaUInt32*);

/// \brief Gets the name of the specified counter.
///
/// \param[in] gpa_virtual_context Unique identifier of the opened virtual context.
/// \param[in] gpa_counter_index The derived_gpa_counter_index of the counter whose name is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// \param[out] gpa_counter_name The address which will hold the name upon successful execution.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterName(const GpaCounterContext gpa_virtual_context,
                                                                 GpaUInt32               gpa_counter_index,
                                                                 const char**            gpa_counter_name);

/// typedef for GpaCounterLibGetCounterName function pointer
typedef GpaStatus (*GpaCounterLibGetCounterNamePtrType)(const GpaCounterContext, GpaUInt32, const char**);

/// \brief Gets index of a counter given its name (case insensitive).
///
/// \param[in] gpa_virtual_context Unique identifier of the session.
/// \param[in] gpa_counter_info The name of the counter whose index is needed.
/// \param[out] gpa_counter_index The address which will hold the index upon successful execution.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterIndex(const GpaCounterContext gpa_virtual_context,
                                                                  const GpaCounterParam*  gpa_counter_info,
                                                                  GpaUInt32*              gpa_counter_index);

/// typedef for GpaCounterLibGetCounterIndex function pointer
typedef GpaStatus (*GpaCounterLibGetCounterIndexPtrType)(const GpaCounterContext, const GpaCounterParam*, GpaUInt32*);

/// \brief Gets the group of the specified counter.
///
/// \param[in] gpa_virtual_context Unique identifier of the opened virtual context.
/// \param[in] gpa_counter_index The index of the counter whose group is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// \param[out] gpa_counter_group The address which will hold the group string upon successful execution.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterGroup(const GpaCounterContext gpa_virtual_context,
                                                                  GpaUInt32               gpa_counter_index,
                                                                  const char**            gpa_counter_group);

/// typedef for GpaCounterLibGetCounterGroup function pointer
typedef GpaStatus (*GpaCounterLibGetCounterGroupPtrType)(const GpaCounterContext, GpaUInt32, const char**);

/// \brief Gets the description of the specified counter.
///
/// \param[in] gpa_virtual_context Unique identifier of the opened virtual context.
/// \param[in] gpa_counter_index The index of the counter whose description is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// \param[out] gpa_counter_description The address which will hold the description upon successful execution.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterDescription(const GpaCounterContext gpa_virtual_context,
                                                                        GpaUInt32               gpa_counter_index,
                                                                        const char**            gpa_counter_description);

/// typedef for GpaCounterLibGetCounterDescription function pointer
typedef GpaStatus (*GpaCounterLibGetCounterDescriptionPtrType)(const GpaCounterContext, GpaUInt32, const char**);

/// \brief Gets the data type of the specified counter.
///
/// \param[in] gpa_virtual_context Unique identifier of the opened virtual context.
/// \param[in] gpa_counter_index The index of the counter whose data type is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// \param[out] gpa_counter_data_type The value which will hold the counter data type upon successful execution.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterDataType(const GpaCounterContext gpa_virtual_context,
                                                                     GpaUInt32               gpa_counter_index,
                                                                     GpaDataType*            gpa_counter_data_type);

/// typedef for GpaCounterLibGetCounterDataType function pointer
typedef GpaStatus (*GpaCounterLibGetCounterDataTypePtrType)(const GpaCounterContext, GpaUInt32, GpaDataType*);

/// \brief Gets the usage type of the specified counter.
///
/// \param[in] gpa_virtual_context Unique identifier of the opened virtual context.
/// \param[in] gpa_counter_index The index of the counter whose usage type is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// \param[out] gpa_counter_usage_type The value which will hold the counter usage type upon successful execution.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterUsageType(const GpaCounterContext gpa_virtual_context,
                                                                      GpaUInt32               gpa_counter_index,
                                                                      GpaUsageType*           gpa_counter_usage_type);

/// typedef for GpaCounterLibGetCounterUsageType function pointer
typedef GpaStatus (*GpaCounterLibGetCounterUsageTypePtrType)(const GpaCounterContext, GpaUInt32, GpaUsageType*);

/// \brief Gets the UUID of the specified counter.
///
/// \param[in] gpa_virtual_context Unique identifier of the opened virtual context.
/// \param[in] gpa_counter_index The index of the counter whose UUID is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// \param[out] gpa_counter_uuid The value which will hold the counter UUID upon successful execution.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterUuid(const GpaCounterContext gpa_virtual_context,
                                                                 GpaUInt32               gpa_counter_index,
                                                                 GpaUuid*                gpa_counter_uuid);

/// typedef for GpaCounterLibGetCounterUuid function pointer
typedef GpaStatus (*GpaCounterLibGetCounterUuidPtrType)(const GpaCounterContext, GpaUInt32, GpaUuid*);

/// \brief Gets the supported sample type of the specified counter.
///
/// Currently, only a single counter type (discrete) is supported
/// \param[in] gpa_virtual_context Unique identifier of the opened virtual context.
/// \param[in] gpa_counter_index The index of the counter whose sample type is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// \param[out] gpa_counter_sample_type The value which will hold the counter's supported sample type upon successful execution.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterSampleType(const GpaCounterContext gpa_virtual_context,
                                                                       GpaUInt32               gpa_counter_index,
                                                                       GpaCounterSampleType*   gpa_counter_sample_type);

/// typedef for GpaCounterLibGetCounterSampleType function pointer
typedef GpaStatus (*GpaCounterLibGetCounterSampleTypePtrType)(const GpaCounterContext, GpaUInt32, GpaCounterSampleType*);

/// \brief Get the counter info.
///
/// This can be used only if GpaOpenContextHidePublicCountersBit flag is not used while opening the virtual context.
/// \param[in] gpa_virtual_context Unique identifier of the opened virtual context.
/// \param[in] gpa_counter_index index of the counter.
/// \param[out] gpa_counter_info counter information.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful. GPA_STATUS_ERROR_FAILED is returned if whitelist/hardware counter index is passed.

GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterInfo(const GpaCounterContext gpa_virtual_context,
                                                                 GpaUInt32               gpa_counter_index,
                                                                 const GpaCounterInfo**  gpa_counter_info);

/// typedef for GpaCounterLibGetCounterInfo function pointer
typedef GpaStatus (*GpaCounterLibGetCounterInfoPtrType)(const GpaCounterContext, GpaUInt32, const GpaCounterInfo**);

/// \brief Computes the derived counter result.
///
/// This can be used only if GpaOpenContextHidePublicCountersBit flag is not used while opening the virtual context.
/// \param[in] gpa_virtual_context Unique identifier of the opened virtual context.
/// \param[in] gpa_derived_counter_index index of the derived counter.
/// \param[in] gpa_hw_counter_result hardware counter data
/// \param[in] gpa_hw_counter_result_count number of hardware counter data
/// \param[out] gpa_derived_counter_result computed derive counter result.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful. GPA_STATUS_ERROR_FAILED is returned if whitelist/hardware counter index is passed.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibComputeDerivedCounterResult(const GpaCounterContext gpa_virtual_context,
                                                                              GpaUInt32               gpa_derived_counter_index,
                                                                              const GpaUInt64*        gpa_hw_counter_result,
                                                                              GpaUInt32               gpa_hw_counter_result_count,
                                                                              GpaFloat64*             gpa_derived_counter_result);

/// typedef for GpaCounterLibComputeDerivedCounterResult function pointer
typedef GpaStatus (*GpaCounterLibComputeDerivedCounterResultPtrType)(const GpaCounterContext, GpaUInt32, const GpaUInt64*, GpaUInt32, GpaFloat64*);

/// \brief Gets the number of passes required for the set of counters.
///
/// \param[in] gpa_virtual_context Unique identifier of the opened virtual context.
/// \param[in] gpa_counter_indices indices of the counters to be enabled.
/// \param[in] gpa_counter_count number of counters.
/// \param[out] number_of_pass_req The value which will hold the number of required passes upon successful execution.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetPassCount(const GpaCounterContext gpa_virtual_context,
                                                               const GpaUInt32*        gpa_counter_indices,
                                                               GpaUInt32               gpa_counter_count,
                                                               GpaUInt32*              number_of_pass_req);

/// typedef for GpaCounterLibGetPassCount function pointer
typedef GpaStatus (*GpaCounterLibGetPassCountPtrType)(const GpaCounterContext, const GpaUInt32*, GpaUInt32, GpaUInt32*);

/// \brief For a given set of counters, get information on how the corresponding hardware counters are scheduled into passes
///
/// \param[in] gpa_virtual_context Unique identifier of the opened virtual context.
/// \param[in] gpa_counter_count number of counters.
/// \param[in] gpa_counter_indices indices of the counters to be enabled.
/// \param[in, out] pass_count contains number of passes required for given set of counters if counter_by_pass_list is null, otherwise represents size of the input counter_by_pass_list array.
/// \param[out] counter_by_pass_list list containing number of counters in each pass. Use this to allocate memory for the counter values.
/// \param[out] gpa_pass_counters list containing number of counters in each pass. Use this to allocate memory for the counter values.
/// \return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCountersByPass(const GpaCounterContext gpa_virtual_context,
                                                                    GpaUInt32               gpa_counter_count,
                                                                    const GpaUInt32*        gpa_counter_indices,
                                                                    GpaUInt32*              pass_count,
                                                                    GpaUInt32*              counter_by_pass_list,
                                                                    GpaPassCounter*         gpa_pass_counters);

/// typedef for GpaCounterLibGetPassCount function pointer
typedef GpaStatus (
    *GpaCounterLibGetCountersByPassPtrType)(const GpaCounterContext, GpaUInt32, const GpaUInt32*, GpaUInt32*, GpaUInt32*, GpaPassCounter*);

#define GPA_COUNTER_LIB_FUNC(X)                 \
    X(GpaCounterLibGetVersion)                  \
    X(GpaCounterLibGetFuncTable)                \
    X(GpaCounterLibOpenCounterContext)          \
    X(GpaCounterLibCloseCounterContext)         \
    X(GpaCounterLibGetNumCounters)              \
    X(GpaCounterLibGetCounterName)              \
    X(GpaCounterLibGetCounterIndex)             \
    X(GpaCounterLibGetCounterGroup)             \
    X(GpaCounterLibGetCounterDescription)       \
    X(GpaCounterLibGetCounterDataType)          \
    X(GpaCounterLibGetCounterUsageType)         \
    X(GpaCounterLibGetCounterUuid)              \
    X(GpaCounterLibGetCounterSampleType)        \
    X(GpaCounterLibGetCounterInfo)              \
    X(GpaCounterLibComputeDerivedCounterResult) \
    X(GpaCounterLibGetPassCount)                \
    X(GpaCounterLibGetCountersByPass)

/// Gpa counter library function table
typedef struct _GpaCounterLibFuncTable
{
    GpaUInt32 gpa_counter_lib_major_version;
    GpaUInt32 gpa_counter_lib_minor_version;
#define GPA_COUNTER_LIB_DECLARE_FUNC_PTR(func) func##PtrType func;

    GPA_COUNTER_LIB_FUNC(GPA_COUNTER_LIB_DECLARE_FUNC_PTR)

#ifdef __cplusplus
    _GpaCounterLibFuncTable()
    {
        gpa_counter_lib_major_version = GPA_COUNTER_LIB_FUNC_TABLE_MAJOR_VERSION;
        gpa_counter_lib_minor_version = GPA_COUNTER_LIB_FUNC_TABLE_MINOR_VERSION;

#define GPA_COUNTER_LIB_ASSIGN_NULL(func) func = nullptr;
        GPA_COUNTER_LIB_FUNC(GPA_COUNTER_LIB_ASSIGN_NULL)
    }

    bool IsInit() const
    {
        bool is_init = true;

#define GPA_COUNTER_LIB_FUNC_IS_NULL(func) is_init &= nullptr != (func);
        GPA_COUNTER_LIB_FUNC(GPA_COUNTER_LIB_FUNC_IS_NULL)
        return is_init;
    }
#endif

} GpaCounterLibFuncTable;

#ifdef _WIN32
#pragma warning(pop)
#endif

#endif  // GPU_PERF_API_GPU_PERF_API_COUNTERS_H_
