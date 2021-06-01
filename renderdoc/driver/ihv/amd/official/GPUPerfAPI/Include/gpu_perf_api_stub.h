//==============================================================================
// Copyright (c) 2018-2021 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  GPA Stub entry points.
//==============================================================================

#ifndef GPU_PERFORMANCE_API_GPU_PERF_API_STUB_H_
#define GPU_PERFORMANCE_API_GPU_PERF_API_STUB_H_

#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable : 4100)
#endif

#include <string.h>

#define RETURN_GPA_SUCCESS return kGpaStatusOk

static inline GpaStatus GpaRegisterLoggingCallback(GpaLoggingType logging_type, GpaLoggingCallbackPtrType callback_function)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaInitialize(GpaInitializeFlags flags)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaDestroy()
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaOpenContext(void* context, GpaOpenContextFlags flags, GpaContextId* context_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaCloseContext(GpaContextId context_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetSupportedSampleTypes(GpaContextId gpa_context_id, GpaContextSampleTypeFlags* sample_types)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetDeviceAndRevisionId(GpaContextId gpa_context_id, GpaUInt32* device_id, GpaUInt32* revision_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetDeviceName(GpaContextId gpa_context_id, const char** device_name)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetNumCounters(GpaContextId gpa_context_id, GpaUInt32* number_of_counters)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetCounterName(GpaContextId gpa_context_id, GpaUInt32 index, const char** counter_name)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetCounterIndex(GpaContextId gpa_context_id, const char* counter_name, GpaUInt32* counter_index)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetCounterGroup(GpaContextId gpa_context_id, GpaUInt32 index, const char** counter_group)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetCounterDescription(GpaContextId gpa_context_id, GpaUInt32 index, const char** counter_description)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetCounterDataType(GpaContextId gpa_context_id, GpaUInt32 index, GpaDataType* counter_data_type)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetCounterUsageType(GpaContextId gpa_context_id, GpaUInt32 index, GpaUsageType* counter_usage_type)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetCounterUuid(GpaContextId gpa_context_id, GpaUInt32 index, GpaUuid* counter_uuid)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetCounterSampleType(GpaContextId gpa_context_id, GpaUInt32 index, GpaCounterSampleType* counter_sample_type)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetDataTypeAsStr(GpaDataType counter_data_type, const char** type_as_str)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetUsageTypeAsStr(GpaUsageType counter_usage_type, const char** usage_type_as_str)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaCreateSession(GpaContextId gpa_context_id, GpaSessionSampleType sample_type, GpaSessionId* session_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaDeleteSession(GpaSessionId gpa_session_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaBeginSession(GpaSessionId gpa_session_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaEndSession(GpaSessionId gpa_session_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaEnableCounter(GpaSessionId gpa_session_id, GpaUInt32 counter_index)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaDisableCounter(GpaSessionId gpa_session_id, GpaUInt32 counter_index)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaEnableCounterByName(GpaSessionId gpa_session_id, const char* counter_name)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaDisableCounterByName(GpaSessionId gpa_session_id, const char* counter_name)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaEnableAllCounters(GpaSessionId gpa_session_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaDisableAllCounters(GpaSessionId gpa_session_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetPassCount(GpaSessionId gpa_session_id, GpaUInt32* number_of_passes)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetNumEnabledCounters(GpaSessionId gpa_session_id, GpaUInt32* enabled_counter_count)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetEnabledIndex(GpaSessionId gpa_session_id, GpaUInt32 enabledNumber, GpaUInt32* enabled_counter_index)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaIsCounterEnabled(GpaSessionId gpa_session_id, GpaUInt32 counter_index)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaBeginCommandList(GpaSessionId       session_id,
                                            GpaUInt32          pass_index,
                                            void*              command_list,
                                            GpaCommandListType command_list_type,
                                            GpaCommandListId*  gpa_command_list_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaEndCommandList(GpaCommandListId command_list_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaBeginSample(GpaUInt32 sample_id, GpaCommandListId command_list_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaEndSample(GpaCommandListId command_list_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaContinueSampleOnCommandList(GpaUInt32 source_sample_id, GpaCommandListId primary_gpa_command_list_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaCopySecondarySamples(GpaCommandListId secondary_gpa_command_list_id,
                                                GpaCommandListId primary_gpa_command_list_id,
                                                GpaUInt32        number_of_samples,
                                                GpaUInt32*       new_sample_ids)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetSampleCount(GpaSessionId gpa_session_id, GpaUInt32* sample_count)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetSampleId(GpaSessionId gpa_session_id, GpaUInt32 index, GpaUInt32* sample_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaIsPassComplete(GpaSessionId gpa_session_id, GpaUInt32 pass_index)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaIsSessionComplete(GpaSessionId gpa_session_id)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetSampleResultSize(GpaSessionId gpa_session_id, GpaUInt32 sample_id, size_t* sample_result_size_in_bytes)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetSampleResult(GpaSessionId gpa_session_id, GpaUInt32 sample_id, size_t sample_result_size_in_bytes, void* counter_sample_results)
{
    RETURN_GPA_SUCCESS;
}

static inline const char* GpaGetStatusAsStr(GpaStatus status)
{
    return NULL;
}

static inline GpaStatus GpaGetVersion(GpaUInt32* major_version, GpaUInt32* minor_version, GpaUInt32* build_version, GpaUInt32* update_version)
{
    RETURN_GPA_SUCCESS;
}

static inline GpaStatus GpaGetFuncTable(void* gpa_func_table)
{
    // All of the GPA functions will reside in user memory as this structure will be compiled along with the user code.
    // Fill the function table with the function in user memory.
    GpaFunctionTable* local_function_table_pointer = (GpaFunctionTable*)(gpa_func_table);

    GpaUInt32 major_version                = GPA_FUNCTION_TABLE_MAJOR_VERSION_NUMBER;
    GpaUInt32 correct_major_version        = (major_version == local_function_table_pointer->major_version ? 1 : 0);
    GpaUInt32 client_supplied_minor_verion = local_function_table_pointer->minor_version;

    local_function_table_pointer->major_version = GPA_FUNCTION_TABLE_MAJOR_VERSION_NUMBER;
    local_function_table_pointer->minor_version = GPA_FUNCTION_TABLE_MINOR_VERSION_NUMBER;

    if (!correct_major_version)
    {
        return kGpaStatusErrorLibLoadMajorVersionMismatch;
    }

    if (client_supplied_minor_verion > GPA_FUNCTION_TABLE_MINOR_VERSION_NUMBER)
    {
        return kGpaStatusErrorLibLoadMajorVersionMismatch;
    }

    GpaFunctionTable new_function_table;
#define GPA_FUNCTION_PREFIX(func) new_function_table.func = func;  ///< Macro used by gpu_perf_api_functions.h
#include "gpu_perf_api_functions.h"
#undef GPA_FUNCTION_PREFIX

    memcpy(gpa_func_table, &new_function_table, client_supplied_minor_verion);
    return kGpaStatusOk;
}

#ifdef _WIN32
#pragma warning(pop)
#endif

#endif  // GPU_PERFORMANCE_API_GPU_PERF_API_STUB_H_
