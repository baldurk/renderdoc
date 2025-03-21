//==============================================================================
// Copyright (c) 2012-2024 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief Interface to access to the available counters in GPUPerfAPI.
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

/// Macro to export public API functions.
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

/// Virtual Context ID opaque pointer type.
GPA_DEFINE_OBJECT(CounterContext)

/// GPA counter library major version.
#define GPA_COUNTER_LIB_FUNC_TABLE_MAJOR_VERSION 3

/// GPA counter library minor version.
#define GPA_COUNTER_LIB_FUNC_TABLE_MINOR_VERSION (sizeof(struct _GpaCounterLibFuncTable))

/// GPA hardware blocks.
typedef enum
{
    kGpaHwBlockCpf = 0,  ///< The GPA hardware block is CPF.
    kGpaHwBlockIa,       ///< The GPA hardware block is IA.
    kGpaHwBlockVgt,      ///< The GPA hardware block is VGT.
    kGpaHwBlockPa,       ///< The GPA hardware block is PA.
    kGpaHwBlockSc,       ///< The GPA hardware block is SC.
    kGpaHwBlockSpi,      ///< The GPA hardware block is SPI.
    kGpaHwBlockSq,       ///< The GPA hardware block is SQ.
    kGpaHwBlockSx,       ///< The GPA hardware block is SX.
    kGpaHwBlockTa,       ///< The GPA hardware block is TA.
    kGpaHwBlockTd,       ///< The GPA hardware block is TD.
    kGpaHwBlockTcp,      ///< The GPA hardware block is TCP.
    kGpaHwBlockTcc,      ///< The GPA hardware block is TCC.
    kGpaHwBlockTca,      ///< The GPA hardware block is TCA.
    kGpaHwBlockDb,       ///< The GPA hardware block is DB.
    kGpaHwBlockCb,       ///< The GPA hardware block is CB.
    kGpaHwBlockGds,      ///< The GPA hardware block is GDS.
    kGpaHwBlockSrbm,     ///< The GPA hardware block is SRBM.
    kGpaHwBlockGrbm,     ///< The GPA hardware block is GRBM.
    kGpaHwBlockGrbmse,   ///< The GPA hardware block is GRBMSE.
    kGpaHwBlockRlc,      ///< The GPA hardware block is RLC.
    kGpaHwBlockDma,      ///< The GPA hardware block is DMA.
    kGpaHwBlockMc,       ///< The GPA hardware block is MC.
    kGpaHwBlockCpg,      ///< The GPA hardware block is CPG.
    kGpaHwBlockCpc,      ///< The GPA hardware block is CPC.
    kGpaHwBlockWd,       ///< The GPA hardware block is WD.
    kGpaHwBlockTcs,      ///< The GPA hardware block is TCS.
    kGpaHwBlockAtc,      ///< The GPA hardware block is ATC.
    kGpaHwBlockAtcl2,    ///< The GPA hardware block is ATCL2.
    kGpaHwBlockMcvml2,   ///< The GPA hardware block is MCVML2.
    kGpaHwBlockEa,       ///< The GPA hardware block is EA.
    kGpaHwBlockRpb,      ///< The GPA hardware block is RPB.
    kGpaHwBlockRmi,      ///< The GPA hardware block is RMI.
    kGpaHwBlockUmcch,    ///< The GPA hardware block is UMCCH.
    kGpaHwBlockGe,       ///< The GPA hardware block is GE.
    kGpaHwBlockGl1A,     ///< The GPA hardware block is GL1A.
    kGpaHwBlockGl1C,     ///< The GPA hardware block is GL1C.
    kGpaHwBlockGl1Cg,    ///< The GPA hardware block is GL1CG.
    kGpaHwBlockGl2A,     ///< The GPA hardware block is GL2A.
    kGpaHwBlockGl2C,     ///< The GPA hardware block is GL2C.
    kGpaHwBlockCha,      ///< The GPA hardware block is CHA.
    kGpaHwBlockChc,      ///< The GPA hardware block is CHC.
    kGpaHwBlockChcg,     ///< The GPA hardware block is CHCG.
    kGpaHwBlockGus,      ///< The GPA hardware block is GUS.
    kGpaHwBlockGcr,      ///< The GPA hardware block is GCR.
    kGpaHwBlockPh,       ///< The GPA hardware block is PH.
    kGpaHwBlockUtcl1,    ///< The GPA hardware block is UTCL1.
    kGpaHwBlockGedist,   ///< The GPA hardware block is GEDIST.
    kGpaHwBlockGese,     ///< The GPA hardware block is GESE.
    kGpaHwBlockDfmall,   ///< The GPA hardware block is DFMALL.
    kGpaHwBlockSqWgp,    ///< The GPA hardware block is SQWGP.
    kGpaHwBlockPc,       ///< The GPA hardware block is PC.
    kGpaHwBlockGl1XA,    ///< The GPA hardware block is GL1XA.
    kGpaHwBlockGl1XC,    ///< The GPA hardware block is GL1XC.
    kGpaHwBlockWgs,      ///< The GPA hardware block is WGS.
    kGpaHwBlockEaCpwd,   ///< The GPA hardware block is EA_CPWD.
    kGpaHwBlockEaSe,     ///< The GPA hardware block is EA_SE.
    kGpaHwBlockCount,    ///< Count.
} GpaHwBlock;

/// GPA shader masks.
typedef enum
{
    kGpaShaderMaskPs,   ///< The GPA PS shader mask.
    kGpaShaderMaskVs,   ///< The GPA VS shader mask.
    kGpaShaderMaskGs,   ///< The GPA GS shader mask.
    kGpaShaderMaskEs,   ///< The GPA ES shader mask.
    kGpaShaderMaskHs,   ///< The GPA HS shader mask.
    kGpaShaderMaskLs,   ///< The GPA LS shader mask.
    kGpaShaderMaskCs,   ///< The GPA CS shader mask.
    kGpaShaderMaskAll,  ///< The GPA all shader mask.
} GpaShaderMask;

/// GPA hardware attribute types.
typedef enum
{
    kGpaHardwareAttributeNumShaderEngines,        ///< Number of shader engines.
    kGpaHardwareAttributeNumShaderArrays,         ///< Number of shader arrays.
    kGpaHardwareAttributeNumSimds,                ///< Number of simds.
    kGpaHardwareAttributeNumComputeUnits,         ///< Number of compute units.
    kGpaHardwareAttributeNumRenderBackends,       ///< Number of render backends.
    kGpaHardwareAttributeClocksPerPrimitive,      ///< Clocks per primitive.
    kGpaHardwareAttributeNumPrimitivePipes,       ///< Number of primitive pipes.
    kGpaHardwareAttributeTimestampFrequency,      ///< Timestamp frequency.
    kGpaHardwareAttributePeakVerticesPerClock,    ///< Peak vertices per clock.
    kGpaHardwareAttributePeakPrimitivesPerClock,  ///< Peak primitives per clock.
    kGpaHardwareAttributePeakPixelsPerClock       ///< Peak pixels per clocks.
} GpaHardwareAttributeType;

/// GPA Hardware attribute.
typedef struct _GpaHardwareAttribute
{
    GpaHardwareAttributeType gpa_hardware_attribute_type;   ///< GPA hardware attribute type.
    GpaUInt32                gpa_hardware_attribute_value;  ///< GPA hardware attribute value.
} GpaHardwareAttribute;

/// GPA counter context hardware info.
typedef struct _GpaCounterContextHardwareInfo
{
    GpaUInt32 vendor_id;    ///< Vendor Id.
    GpaUInt32 device_id;    ///< Device Id.
    GpaUInt32 revision_id;  ///< Revision Id.

    GpaHardwareAttribute* gpa_hardware_attributes;       ///< Pointer to array of hardware attributes.
    GpaUInt32             gpa_hardware_attribute_count;  ///< Number of hardware attributes.

} GpaCounterContextHardwareInfo;

/// Hardware counter info.
typedef struct _GpaHwCounter
{
    bool is_timing_block;  ///< Flag indicating time based derived counter.
    union
    {
        union
        {
            GpaUInt32 gpu_time_bottom_to_bottom_duration;  ///< Index of gpu_time_bottom_to_bottom_duration counter.
            GpaUInt32 gpu_time_bottom_to_bottom_start;     ///< Index of gpu_time_bottom_to_bottom_duration counter.
            GpaUInt32 gpu_time_bottom_to_bottom_end;       ///< Index of gpu_time_bottom_to_bottom_duration counter.
            GpaUInt32 gpu_time_top_to_bottom_duration;     ///< Index of gpu_time_top_to_bottom_duration counter.
            GpaUInt32 gpu_time_top_to_bottom_start;        ///< Index of gpu_time_top_to_bottom_start counter.
            GpaUInt32 gpu_time_top_to_bottom_end;          ///< Index of gpu_time_top_to_bottom_end counter.
        };

        struct
        {
            GpaHwBlock    gpa_hw_block;           ///< GPA hardware block.
            GpaUInt32     gpa_hw_block_instance;  ///< GPA hardware block 0-based instance index.
            GpaUInt32     gpa_hw_block_event_id;  ///< GPA hardware block 0-based event id.
            GpaShaderMask gpa_shader_mask;        ///< GPA shader mask, only used if SQ block is queried.
        };
    };

} GpaHwCounter;

/// GPA derived counter info.
typedef struct _GpaDerivedCounterInfo
{
    GpaHwCounter* gpa_hw_counters;       ///< Hardware counters.
    GpaUInt32     gpa_hw_counter_count;  ///< Number of hardware counter.
    GpaUsageType  counter_usage_type;    ///< Usage of the derived counter.

} GpaDerivedCounterInfo;

/// GPA counter info -- can be a derived counter or a hardware counter.
typedef struct _GpaCounterInfo
{
    bool is_derived_counter;  ///< Flag indicating this is a derived counter.
    union
    {
        GpaDerivedCounterInfo* gpa_derived_counter;  ///< Derived counter.
        GpaHwCounter*          gpa_hw_counter;       ///< Hardware counter.
    };
} GpaCounterInfo;

/// GPA counter parameter.
typedef struct _GpaCounterParam
{
    bool is_derived_counter;  ///< Flag indicating derived counter.
    union
    {
        const char*  derived_counter_name;  ///< Derived counter name.
        GpaHwCounter gpa_hw_counter;        ///< Hardware counter.
    };
} GpaCounterParam;

/// GPA counters in a pass.
typedef struct _GpaPassCounter
{
    GpaUInt32  pass_index;       ///< Pass index.
    GpaUInt32  counter_count;    ///< Number of counters.
    GpaUInt32* counter_indices;  ///< Indices of the counters.
} GpaPassCounter;

/// @brief Gets the GPA Counter lib version.
///
/// @param [out] major_version The value that will hold the major version of GPA upon successful execution.
/// @param [out] minor_version The value that will hold the minor version of GPA upon successful execution.
/// @param [out] build_number The value that will hold the build number of GPA upon successful execution.
/// @param [out] update_version The value that will hold the update version of GPA upon successful execution.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetVersion(GpaUInt32* major_version,
                                                             GpaUInt32* minor_version,
                                                             GpaUInt32* build_number,
                                                             GpaUInt32* update_version);

/// Typedef for GpaCounterLibGetVersion function pointer.
typedef GpaStatus (*GpaCounterLibGetVersionPtrType)(GpaUInt32*, GpaUInt32*, GpaUInt32*, GpaUInt32*);

/// @brief Gets the GPA counter library function table.
///
/// @param [out] gpa_counter_lib_function_table pointer to the GPA counter library function table.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetFuncTable(void* gpa_counter_lib_function_table);

/// Typedef for GpaCounterLibGetFuncTable function pointer.
typedef GpaStatus (*GpaCounterLibGetFuncTablePtrType)(void*);

/// @brief Creates a virtual context to interrogate the counter information.
///
/// @param [in] api The api whose available counters are requested.
/// @param [in] sample_type The type of sample to be collected.
/// @param [in] gpa_counter_context_hardware_info counter context hardware info.
/// @param [in] context_flags Flags used to initialize the context. Should be a combination of GpaOpenContextBits.
/// @param [out] gpa_virtual_context Unique identifier of the opened virtual context.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
/// @retval kGpaStatusErrorNullPointer If the supplied gpa_virtual_context is a nullptr.
/// @retval kGpaStatusErrorInvalidParameter If any value other than 1 (true) is passed in for generate_asic_specific_counters_deprecated,
///         or if the context_flags specified would result in zero counters being exposed.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibOpenCounterContext(GpaApiType                    api,
                                                                     GpaSessionSampleType          sample_type,
                                                                     GpaCounterContextHardwareInfo gpa_counter_context_hardware_info,
                                                                     GpaOpenContextFlags           context_flags,
                                                                     GpaCounterContext*            gpa_virtual_context);

/// typedef for GpaCounterLibOpenCounterContext function pointer.
typedef GpaStatus (
    *GpaCounterLibOpenCounterContextPtrType)(GpaApiType, GpaSessionSampleType, GpaCounterContextHardwareInfo, GpaOpenContextFlags, GpaCounterContext*);

/// @brief Closes the specified context, which ends access to GPU performance counters.
///
/// Counter functions should not be called again until the counters are reopened with GpaCounterLib_OpenCounterContext.
///
/// @param [in] gpa_virtual_context Unique identifier of the opened virtual context.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibCloseCounterContext(const GpaCounterContext gpa_virtual_context);

/// Typedef for GpaCounterLibCloseCounterContextPtrType function pointer.
typedef GpaStatus (*GpaCounterLibCloseCounterContextPtrType)(const GpaCounterContext);

/// @brief Gets the number of counters available.
///
/// @param [in] gpa_virtual_context Unique identifier of the opened virtual context.
/// @param [out] gpa_counter_count The value which will hold the count upon successful execution.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetNumCounters(const GpaCounterContext gpa_virtual_context, GpaUInt32* gpa_counter_count);

/// Typedef for GpaCounterLibGetNumCountersPtr function pointer.
typedef GpaStatus (*GpaCounterLibGetNumCountersPtrType)(const GpaCounterContext, GpaUInt32*);

/// @brief Gets the name of the specified counter.
///
/// @param [in] gpa_virtual_context Unique identifier of the opened virtual context.
/// @param [in] gpa_counter_index The derived_gpa_counter_index of the counter whose name is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param [out] gpa_counter_name The address which will hold the name upon successful execution.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterName(const GpaCounterContext gpa_virtual_context,
                                                                 GpaUInt32               gpa_counter_index,
                                                                 const char**            gpa_counter_name);

/// Typedef for GpaCounterLibGetCounterName function pointer.
typedef GpaStatus (*GpaCounterLibGetCounterNamePtrType)(const GpaCounterContext, GpaUInt32, const char**);

/// @brief Gets index of a counter given its name (case insensitive).
///
/// @param [in] gpa_virtual_context Unique identifier of the session.
/// @param [in] gpa_counter_info The name of the counter whose index is needed.
/// @param [out] gpa_counter_index The address which will hold the index upon successful execution.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
/// @retval kGpaStatusErrorNullPointer if the gpa_counter_info or gpa_counter_index is null, or if the gpa_counter_info
///         indicates that it is a derived counter, but the derived counter name is null.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterIndex(const GpaCounterContext gpa_virtual_context,
                                                                  const GpaCounterParam*  gpa_counter_info,
                                                                  GpaUInt32*              gpa_counter_index);

/// Typedef for GpaCounterLibGetCounterIndex function pointer.
typedef GpaStatus (*GpaCounterLibGetCounterIndexPtrType)(const GpaCounterContext, const GpaCounterParam*, GpaUInt32*);

/// @brief Gets the group of the specified counter.
///
/// @param [in] gpa_virtual_context Unique identifier of the opened virtual context.
/// @param [in] gpa_counter_index The index of the counter whose group is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param [out] gpa_counter_group The address which will hold the group string upon successful execution.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterGroup(const GpaCounterContext gpa_virtual_context,
                                                                  GpaUInt32               gpa_counter_index,
                                                                  const char**            gpa_counter_group);

/// Typedef for GpaCounterLibGetCounterGroup function pointer.
typedef GpaStatus (*GpaCounterLibGetCounterGroupPtrType)(const GpaCounterContext, GpaUInt32, const char**);

/// @brief Gets the description of the specified counter.
///
/// @param [in] gpa_virtual_context Unique identifier of the opened virtual context.
/// @param [in] gpa_counter_index The index of the counter whose description is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param [out] gpa_counter_description The address which will hold the description upon successful execution.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterDescription(const GpaCounterContext gpa_virtual_context,
                                                                        GpaUInt32               gpa_counter_index,
                                                                        const char**            gpa_counter_description);

/// Typedef for GpaCounterLibGetCounterDescription function pointer.
typedef GpaStatus (*GpaCounterLibGetCounterDescriptionPtrType)(const GpaCounterContext, GpaUInt32, const char**);

/// @brief Gets the data type of the specified counter.
///
/// @param [in] gpa_virtual_context Unique identifier of the opened virtual context.
/// @param [in] gpa_counter_index The index of the counter whose data type is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param [out] gpa_counter_data_type The value which will hold the counter data type upon successful execution.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterDataType(const GpaCounterContext gpa_virtual_context,
                                                                     GpaUInt32               gpa_counter_index,
                                                                     GpaDataType*            gpa_counter_data_type);

/// Typedef for GpaCounterLibGetCounterDataType function pointer.
typedef GpaStatus (*GpaCounterLibGetCounterDataTypePtrType)(const GpaCounterContext, GpaUInt32, GpaDataType*);

/// @brief Gets the usage type of the specified counter.
///
/// @param [in] gpa_virtual_context Unique identifier of the opened virtual context.
/// @param [in] gpa_counter_index The index of the counter whose usage type is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param [out] gpa_counter_usage_type The value which will hold the counter usage type upon successful execution.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterUsageType(const GpaCounterContext gpa_virtual_context,
                                                                      GpaUInt32               gpa_counter_index,
                                                                      GpaUsageType*           gpa_counter_usage_type);

/// Typedef for GpaCounterLibGetCounterUsageType function pointer.
typedef GpaStatus (*GpaCounterLibGetCounterUsageTypePtrType)(const GpaCounterContext, GpaUInt32, GpaUsageType*);

/// @brief Gets the UUID of the specified counter.
///
/// @param [in] gpa_virtual_context Unique identifier of the opened virtual context.
/// @param [in] gpa_counter_index The index of the counter whose UUID is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param [out] gpa_counter_uuid The value which will hold the counter UUID upon successful execution.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterUuid(const GpaCounterContext gpa_virtual_context,
                                                                 GpaUInt32               gpa_counter_index,
                                                                 GpaUuid*                gpa_counter_uuid);

/// Typedef for GpaCounterLibGetCounterUuid function pointer.
typedef GpaStatus (*GpaCounterLibGetCounterUuidPtrType)(const GpaCounterContext, GpaUInt32, GpaUuid*);

/// @brief Gets the supported sample type of the specified counter.
///
/// Currently, only a single counter type (discrete) is supported
/// @param [in] gpa_virtual_context Unique identifier of the opened virtual context.
/// @param [in] gpa_counter_index The index of the counter whose sample type is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param [out] gpa_counter_sample_type The value which will hold the counter's supported sample type upon successful execution.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterSampleType(const GpaCounterContext gpa_virtual_context,
                                                                       GpaUInt32               gpa_counter_index,
                                                                       GpaCounterSampleType*   gpa_counter_sample_type);

/// Typedef for GpaCounterLibGetCounterSampleType function pointer.
typedef GpaStatus (*GpaCounterLibGetCounterSampleTypePtrType)(const GpaCounterContext, GpaUInt32, GpaCounterSampleType*);

/// @brief Get the counter info.
///
/// This can be used only if GpaOpenContextHidePublicCountersBit flag is not used while opening the virtual context.
/// @param [in] gpa_virtual_context Unique identifier of the opened virtual context.
/// @param [in] gpa_counter_index index of the counter.
/// @param [out] gpa_counter_info counter information.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful. GPA_STATUS_ERROR_FAILED is returned if whitelist/hardware counter index is passed.

GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCounterInfo(const GpaCounterContext gpa_virtual_context,
                                                                 GpaUInt32               gpa_counter_index,
                                                                 const GpaCounterInfo**  gpa_counter_info);

/// Typedef for GpaCounterLibGetCounterInfo function pointer.
typedef GpaStatus (*GpaCounterLibGetCounterInfoPtrType)(const GpaCounterContext, GpaUInt32, const GpaCounterInfo**);

/// @brief Computes the derived counter result.
///
/// This can be used only if GpaOpenContextHidePublicCountersBit flag is not used while opening the virtual context.
/// @param [in] gpa_virtual_context Unique identifier of the opened virtual context.
/// @param [in] gpa_derived_counter_index index of the derived counter.
/// @param [in] gpa_hw_counter_result hardware counter data
/// @param [in] gpa_hw_counter_result_count number of hardware counter data.
/// @param [out] gpa_derived_counter_result computed derive counter result.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful. GPA_STATUS_ERROR_FAILED is returned if whitelist/hardware counter index is passed.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibComputeDerivedCounterResult(const GpaCounterContext gpa_virtual_context,
                                                                              GpaUInt32               gpa_derived_counter_index,
                                                                              const GpaUInt64*        gpa_hw_counter_result,
                                                                              GpaUInt32               gpa_hw_counter_result_count,
                                                                              GpaFloat64*             gpa_derived_counter_result);

/// Typedef for GpaCounterLibComputeDerivedCounterResult function pointer.
typedef GpaStatus (*GpaCounterLibComputeDerivedCounterResultPtrType)(const GpaCounterContext, GpaUInt32, const GpaUInt64*, GpaUInt32, GpaFloat64*);

/// @brief Gets the number of passes required for the set of counters.
///
/// @param [in] gpa_virtual_context Unique identifier of the opened virtual context.
/// @param [in] gpa_counter_indices indices of the counters to be enabled.
/// @param [in] gpa_counter_count number of counters.
/// @param [out] number_of_pass_req The value which will hold the number of required passes upon successful execution.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetPassCount(const GpaCounterContext gpa_virtual_context,
                                                               const GpaUInt32*        gpa_counter_indices,
                                                               GpaUInt32               gpa_counter_count,
                                                               GpaUInt32*              number_of_pass_req);

/// Typedef for GpaCounterLibGetPassCount function pointer.
typedef GpaStatus (*GpaCounterLibGetPassCountPtrType)(const GpaCounterContext, const GpaUInt32*, GpaUInt32, GpaUInt32*);

/// @brief For a given set of counters, get information on how the corresponding hardware counters are scheduled into passes
///
/// @param [in] gpa_virtual_context Unique identifier of the opened virtual context.
/// @param [in] gpa_counter_count number of counters.
/// @param [in] gpa_counter_indices indices of the counters to be enabled.
/// @param [in, out] pass_count contains number of passes required for given set of counters if counter_by_pass_list is null, otherwise represents size of the input counter_by_pass_list array.
/// @param [out] counter_by_pass_list list containing number of counters in each pass. Use this to allocate memory for the counter values.
/// @param [out] gpa_pass_counters list containing number of counters in each pass. Use this to allocate memory for the counter values.
///
/// @return The GPA result status of the operation. kGpaStatusOk is returned if the operation is successful.
GPU_PERF_API_COUNTERS_DECL GpaStatus GpaCounterLibGetCountersByPass(const GpaCounterContext gpa_virtual_context,
                                                                    GpaUInt32               gpa_counter_count,
                                                                    const GpaUInt32*        gpa_counter_indices,
                                                                    GpaUInt32*              pass_count,
                                                                    GpaUInt32*              counter_by_pass_list,
                                                                    GpaPassCounter*         gpa_pass_counters);

/// Typedef for GpaCounterLibGetPassCount function pointer.
typedef GpaStatus (*GpaCounterLibGetCountersByPassPtrType)(const GpaCounterContext, GpaUInt32, const GpaUInt32*, GpaUInt32*, GpaUInt32*, GpaPassCounter*);

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

/// GPA counter library function table.
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
