//==============================================================================
// Copyright (c) 2010-2021 Advanced Micro Devices, Inc. All rights reserved.
/// @author AMD Developer Tools Team
/// @file
/// @brief  This is the header file that must be included by an application that
///         wishes to use GPUPerfAPI. It defines all the available entry points.
//==============================================================================

#ifndef GPU_PERFORMANCE_API_GPU_PERF_API_H_
#define GPU_PERFORMANCE_API_GPU_PERF_API_H_

#include <stddef.h>

#ifndef GPA_LIB_DECL
/// Macro for exporting an API function.
#ifdef _WIN32
#ifdef __cplusplus
#define GPA_LIB_DECL extern "C" __declspec(dllimport)
#else
#define GPA_LIB_DECL __declspec(dllimport)
#endif
#else  //__linux__
#ifdef __cplusplus
#define GPA_LIB_DECL extern "C"
#else
#define GPA_LIB_DECL extern
#endif
#endif
#endif

#if DISABLE_GPA
#define USE_GPA 0  ///< Macro used to determine if GPA functions should be stubbed out.
#else
#define USE_GPA 1  ///< Macro used to determine if GPA functions should be stubbed out.
#endif

#include "gpu_perf_api_function_types.h"
#include "gpu_perf_api_types.h"

#define GPA_FUNCTION_TABLE_MAJOR_VERSION_NUMBER \
    3  ///< API major version -- will be incremented if/when there are non-backwards compatible API changes introduced.
#define GPA_FUNCTION_TABLE_MINOR_VERSION_NUMBER \
    (sizeof(struct _GpaFunctionTable))  ///< API minor version -- set to the structure size; will increase when new API functions are added.

/// @brief Structure to hold the function table of the exported GPA APIs.
typedef struct _GpaFunctionTable
{
    GpaUInt32 major_version;  ///< API major version.
    GpaUInt32 minor_version;  ///< API minor version.

#define GPA_FUNCTION_PREFIX(func) func##PtrType func;  ///< Macro used by gpu_perf_api_functions.h.
#include "gpu_perf_api_functions.h"
#undef GPA_FUNCTION_PREFIX

#ifdef __cplusplus
    /// @brief Constructor.
    _GpaFunctionTable()
    {
        major_version = GPA_FUNCTION_TABLE_MAJOR_VERSION_NUMBER;
        minor_version = GPA_FUNCTION_TABLE_MINOR_VERSION_NUMBER;
#define GPA_FUNCTION_PREFIX(func) func = nullptr;  ///< Macro used by gpu_perf_api_functions.h.
#include "gpu_perf_api_functions.h"
#undef GPA_FUNCTION_PREFIX
    }
#endif

} GpaFunctionTable;

#if USE_GPA

/// @defgroup gpa_api_version GPA API Version

/// @brief Gets the GPA version.
///
/// @ingroup gpa_api_version
///
/// @param [out] major_version The value that will hold the major version of GPA upon successful execution.
/// @param [out] minor_version The value that will hold the minor version of GPA upon successful execution.
/// @param [out] build_version The value that will hold the build number of GPA upon successful execution.
/// @param [out] update_version The value that will hold the update version of GPA upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorException If an unexpected error occurred.
GPA_LIB_DECL GpaStatus GpaGetVersion(GpaUInt32* major_version, GpaUInt32* minor_version, GpaUInt32* build_version, GpaUInt32* update_version);

/// @defgroup gpa_api_table GPA API Function Table

/// @brief Gets the GPA API function table.
///
/// @ingroup gpa_api_table
///
/// @param [out] gpa_func_table pointer to GPA Function table structure.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorLibLoadMajorVersionMismatch If the major version of the loaded binary does not match the header's.
/// @retval kGpaStatusErrorLibLoadMinorVersionMismatch If the minor version of the loaded binary does not match the header's.
/// @retval kGpaStatusErrorException If an unexpected error occurred.
GPA_LIB_DECL GpaStatus GpaGetFuncTable(void* gpa_func_table);

/// @defgroup gpa_logging Logging

/// @brief Registers a callback function to receive log messages.
///
/// Only one callback function can be registered, so the implementation should be able
/// to handle the different types of messages. A parameter to the callback function will
/// indicate the message type being received. Messages will not contain a newline character
/// at the end of the message.
///
/// @ingroup gpa_logging
///
/// @param [in] logging_type Identifies the type of messages to receive callbacks for.
/// @param [in] callback_func_ptr Pointer to the callback function.
///
/// @retval kGpaStatusOk On Success.
/// @retval kGpaStatusErrorNullPointer If callback_func_ptr is nullptr and the logging_type is not kGpaLoggingNone.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaRegisterLoggingCallback(GpaLoggingType logging_type, GpaLoggingCallbackPtrType callback_func_ptr);

/// @defgroup gpa_init_destroy GPA Initialization and Destruction

/// @brief Initializes the driver so that counters are exposed.
///
/// This function must be called before the rendering context or device is created. In the case of DirectX 12
/// or Vulkan, this function must be called before a queue is created.
///
/// @ingroup gpa_init_destroy
///
/// @param [in] gpa_initialize_flags Flags used to initialize GPA. This should be a combination of GpaInitializeBits.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorGpaAlreadyInitialized If GPA is already initialized.
/// @retval kGpaStatusErrorInvalidParameter If invalid flags have been supplied.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaInitialize(GpaInitializeFlags gpa_initialize_flags);

/// @brief Undoes any initialization to ensure proper behavior in applications that are not being profiled.
///
/// This function must be called after the rendering context or device is released / destroyed.
///
/// @ingroup gpa_init_destroy
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorGpaNotInitialized If GPA has not been initialized.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaDestroy();

/// @defgroup gpa_context_start_finish GPA Context Startup and Finish

/// @brief Opens the specified context, which provides access to GPU performance counters.
///
/// This function must be called after GpaInitialize and before any other GPA functions.
///
/// @ingroup gpa_context_start_finish
///
/// @param [in] api_context The context to open counters for. Typically a device pointer. Refer to GPA API documentation for further details.
/// @param [in] gpa_open_context_flags Flags used to initialize the context. This should be a combination of GpaOpenContextBits.
/// @param [out] gpa_context_id Unique identifier of the opened context.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If either of the context parameters are NULL.
/// @retval kGpaStatusErrorContextAlreadyOpen If the context has already been opened.
/// @retval kGpaStatusErrorInvalidParameter If one the flags is invalid.
/// @retval kGpaStatusErrorHardwareNotSupported If the hardware is not supported.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaOpenContext(void* api_context, GpaOpenContextFlags gpa_open_context_flags, GpaContextId* gpa_context_id);

/// @brief Closes the specified context, which ends access to GPU performance counters.
///
/// GPA functions should not be called again until the counters are reopened with GpaOpenContext.
///
/// @ingroup gpa_context_start_finish
///
/// @param [in] gpa_context_id Unique identifier of the opened context.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorInvalidParameter The supplied context has invalid state.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaCloseContext(GpaContextId gpa_context_id);

/// @defgroup gpa_context_interrogation GPA Context Interrogation

/// @brief Gets a mask of the sample types supported by the specified context.
///
/// A call to GPA_CreateSession will fail if the requested sample types are not compatible with the context's sample types.
/// supported by the context.
///
/// @ingroup gpa_context_interrogation
///
/// @param [in] gpa_context_id Unique identifier of the opened context.
/// @param [out] sample_types The value that will be set to the mask of the supported sample types upon successful execution. This will be a combination of GPA_Sample_Bits.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetSupportedSampleTypes(GpaContextId gpa_context_id, GpaContextSampleTypeFlags* sample_types);

/// @brief Gets the GPU device id and revision id associated with the specified context.
///
/// @ingroup gpa_context_interrogation
///
/// @param[in] gpa_context_id Unique identifier of the opened context.
/// @param[out] device_id The value that will be set to the device id upon successful execution.
/// @param[out] revision_id The value that will be set to the device revision id upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetDeviceAndRevisionId(GpaContextId gpa_context_id, GpaUInt32* device_id, GpaUInt32* revision_id);

/// @brief Gets the device name of the GPU associated with the specified context.
///
/// @ingroup gpa_context_interrogation
///
/// @param[in] gpa_context_id Unique identifier of the opened context.
/// @param[out] device_name The value that will be set to the device name upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetDeviceName(GpaContextId gpa_context_id, const char** device_name);

/// @defgroup gpa_counter_interrogation GPA Counter Interrogation

/// @brief Gets the number of counters available.
///
/// @ingroup gpa_counter_interrogation
///
/// @param[in] gpa_context_id Unique identifier of the opened context.
/// @param[out] number_of_counters The value which will hold the count upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetNumCounters(GpaContextId gpa_context_id, GpaUInt32* number_of_counters);

/// @brief Gets the name of the specified counter.
///
/// @ingroup gpa_counter_interrogation
///
/// @param[in] gpa_context_id Unique identifier of the opened context.
/// @param[in] index The index of the counter whose name is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param[out] counter_name The address which will hold the name upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorIndexOutOfRange If the counter index is out of range.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetCounterName(GpaContextId gpa_context_id, GpaUInt32 index, const char** counter_name);

/// @brief Gets index of a counter given its name (case insensitive).
///
/// @ingroup gpa_counter_interrogation
///
/// @param [in] gpa_context_id Unique identifier of the session.
/// @param [in] counter_name The name of the counter whose index is needed.
/// @param [out] counter_index The address which will hold the index upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorCounterNotFound If the supplied counter name cannot be found.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetCounterIndex(GpaContextId gpa_context_id, const char* counter_name, GpaUInt32* counter_index);

/// @brief Gets the group of the specified counter.
///
/// @ingroup gpa_counter_interrogation
///
/// @param [in] gpa_context_id Unique identifier of the opened context.
/// @param [in] index The index of the counter whose group is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param [out] counter_group The address which will hold the group string upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorIndexOutOfRange If the counter index is out of range.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetCounterGroup(GpaContextId gpa_context_id, GpaUInt32 index, const char** counter_group);

/// @brief Gets the description of the specified counter.
///
/// @ingroup gpa_counter_interrogation
///
/// @param [in] gpa_context_id Unique identifier of the opened context.
/// @param [in] index The index of the counter whose description is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param [out] counter_description The address which will hold the description upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorIndexOutOfRange If the counter index is out of range.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetCounterDescription(GpaContextId gpa_context_id, GpaUInt32 index, const char** counter_description);

/// @brief Gets the data type of the specified counter.
///
/// @ingroup gpa_counter_interrogation
///
/// @param [in] gpa_context_id Unique identifier of the opened context.
/// @param [in] index The index of the counter whose data type is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param [out] counter_data_type The value which will hold the counter data type upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorIndexOutOfRange If the counter index is out of range.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetCounterDataType(GpaContextId gpa_context_id, GpaUInt32 index, GpaDataType* counter_data_type);

/// @brief Gets the usage type of the specified counter.
///
/// @ingroup gpa_counter_interrogation
///
/// @param [in] gpa_context_id Unique identifier of the opened context.
/// @param [in] index The index of the counter whose usage type is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param [out] counter_usage_type The value which will hold the counter usage type upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorIndexOutOfRange If the counter index is out of range.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetCounterUsageType(GpaContextId gpa_context_id, GpaUInt32 index, GpaUsageType* counter_usage_type);

/// @brief Gets the UUID of the specified counter.
///
/// @ingroup gpa_counter_interrogation
///
/// @param [in] gpa_context_id Unique identifier of the opened context.
/// @param [in] index The index of the counter whose UUID is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param [out] counter_uuid The value which will hold the counter UUID upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorIndexOutOfRange If the counter index is out of range.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetCounterUuid(GpaContextId gpa_context_id, GpaUInt32 index, GpaUuid* counter_uuid);

/// @brief Gets the supported sample type of the specified counter.
///
/// Currently, only a single counter type (discrete) is supported.
///
/// @ingroup gpa_counter_interrogation
///
/// @param [in] gpa_context_id Unique identifier of the opened context.
/// @param [in] index The index of the counter whose sample type is needed. Must lie between 0 and (GpaGetNumCounters result - 1).
/// @param [out] counter_sample_type The value which will hold the counter's supported sample type upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorIndexOutOfRange If the counter index is out of range.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetCounterSampleType(GpaContextId gpa_context_id, GpaUInt32 index, GpaCounterSampleType* counter_sample_type);

/// @brief Gets a string representation of the specified counter data type.
///
/// This could be used to display counter types along with their name or value.
/// For example, the kGpaDataTypeUint64 counter_data_type would return "gpa_uint64".
///
/// @ingroup gpa_counter_interrogation
///
/// @param [in] counter_data_type The data type whose string representation is needed.
/// @param [out] type_as_str The address which will hold the string representation upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorInvalidParameter An invalid data type was supplied.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetDataTypeAsStr(GpaDataType counter_data_type, const char** type_as_str);

/// @brief Gets a string representation of the specified counter usage type.
///
/// This could be used to display counter units along with their name or value.
/// For example, the GPA_USAGE_TYPE_PERCENTAGE counterUsageType would return "percentage".
///
/// @ingroup gpa_counter_interrogation
///
/// @param [in] counter_usage_type The usage type whose string representation is needed.
/// @param [out] usage_type_as_str The address which will hold the string representation upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorInvalidParameter An invalid usage type was supplied.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetUsageTypeAsStr(GpaUsageType counter_usage_type, const char** usage_type_as_str);

/// @defgroup gpa_session_handling GPA Session Handling

/// @brief Creates a session on the specified context.
///
/// A unique session identifier will be returned which allows counters to be enabled, samples to be
/// measured, and stores the results of the profile. The sample type for the session should be
/// specified by the caller. The requested sample types must be supported by the supplied context.
/// Use GpaGetSupportedSampleTypes to determine which sample types are supported by a context.
///
/// @ingroup gpa_session_handling
///
/// @param [in] gpa_context_id The context on which to create the session.
/// @param [in] gpa_session_sample_type The sample type that will be created on this session.
/// @param [out] gpa_session_id The address of a GPA_SessionId which will be populated with the created session Id.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorContextNotFound If the supplied context is invalid.
/// @retval kGpaStatusErrorContextNotOpen If the supplied context has not been opened.
/// @retval kGpaStatusErrorInvalidParameter An invalid sample type was supplied.
/// @retval kGpaStatusErrorIncompatibleSampleTypes The supplied sample type is not compatible with the supplied context.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaCreateSession(GpaContextId gpa_context_id, GpaSessionSampleType gpa_session_sample_type, GpaSessionId* gpa_session_id);

/// @brief Deletes a session object.
///
/// Deletes the specified session, along with all counter results associated with the session.
///
/// @ingroup gpa_session_handling
///
/// @param [in] gpa_session_id The session id that is to be deleted.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaDeleteSession(GpaSessionId gpa_session_id);

/// @brief Begins sampling with the currently enabled set of counters.
///
/// This must be called to begin the counter sampling process.
/// Counters must be appropriately enabled (or disabled) before BeginSession is called.
/// The set of enabled counters cannot be changed inside a BeginSession/EndSession sequence.
///
/// @ingroup gpa_session_handling
///
/// @param [in] gpa_session_id Unique identifier of the GPA Session Object.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorContextNotOpen The context on this session is not open.
/// @retval kGpaStatusErrorOtherSessionActive A different session is already active.
/// @retval kGpaStatusErrorSessionAlreadyStarted This session has already started.
/// @retval kGpaStatusErrorNoCountersEnabled No counters have been enabled on this session.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaBeginSession(GpaSessionId gpa_session_id);

/// @brief Ends sampling with the currently enabled set of counters.
///
/// @ingroup gpa_session_handling
///
/// @param [in] gpa_session_id Unique identifier of the GPA Session Object.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorOtherSessionActive A different session is currently active.
/// @retval kGpaStatusErrorSessionNotStarted The supplied session has not been started.
/// @retval kGpaStatusErrorNotEnoughPasses There have not been enough passes to complete the counter collection on this session.
/// @retval kGpaStatusErrorVariableNumberOfSamplesInPasses The number of samples in each pass was inconsistent.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaEndSession(GpaSessionId gpa_session_id);

/// @defgroup gpa_counter_scheduling GPA Counter Scheduling

/// @brief Enables the specified counter.
///
/// Subsequent sampling sessions will provide values for any enabled counters.
/// Initially all counters are disabled, and must explicitly be enabled.
///
/// @ingroup gpa_counter_scheduling
///
/// @param [in] gpa_session_id Unique identifier of the session on which to enable the counter.
/// @param [in] counter_index The index of the counter to enable. Must lie between 0 and (GpaGetNumCounters result - 1).
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorCannotChangeCountersWhenSampling It is invalid to enable a counter while the session is active.
/// @retval kGpaStatusErrorIndexOutOfRange If the counter index is out of range.
/// @retval kGpaStatusErrorIncompatibleSampleTypes The specified counter is not compatible with the supplied session.
/// @retval kGpaStatusErrorAlreadyEnabled The specified counter is already enabled.
/// @retval kGpaStatusErrorNotEnabled The counter could not be enabled.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaEnableCounter(GpaSessionId gpa_session_id, GpaUInt32 counter_index);

/// @brief Disables the specified counter.
///
/// Subsequent sampling sessions will not provide values for any disabled counters.
/// Initially all counters are disabled, and must explicitly be enabled.
///
/// @ingroup gpa_counter_scheduling
///
/// @param [in] gpa_session_id Unique identifier of the session on which to disable the counter.
/// @param [in] counter_index The index of the counter to disable. Must lie between 0 and (GpaGetNumCounters result - 1).
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorCannotChangeCountersWhenSampling It is invalid to disable a counter while the session is active.
/// @retval kGpaStatusErrorIndexOutOfRange If the counter index is out of range.
/// @retval kGpaStatusErrorNotEnabled The counter was not enabled, so it cannot be disabled.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaDisableCounter(GpaSessionId gpa_session_id, GpaUInt32 counter_index);

/// @brief Enables the counter with the specified counter name (case insensitive).
///
/// Subsequent sampling sessions will provide values for any enabled counters.
/// Initially all counters are disabled, and must explicitly be enabled.
///
/// @ingroup gpa_counter_scheduling
///
/// @param [in] gpa_session_id Unique identifier of the session.
/// @param [in] counter_name The name of the counter to enable.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorCannotChangeCountersWhenSampling It is invalid to enable a counter while the session is active.
/// @retval kGpaStatusErrorContextNotOpen The context on this session is not open.
/// @retval kGpaStatusErrorCounterNotFound The named counter cannot be found.
/// @retval kGpaStatusErrorIncompatibleSampleTypes The specified counter is not compatible with the supplied session.
/// @retval kGpaStatusErrorAlreadyEnabled The specified counter is already enabled.
/// @retval kGpaStatusErrorNotEnabled The counter could not be enabled.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaEnableCounterByName(GpaSessionId gpa_session_id, const char* counter_name);

/// @brief Disables the counter with the specified counter name (case insensitive).
///
/// Subsequent sampling sessions will not provide values for any disabled counters.
/// Initially all counters are disabled, and must explicitly be enabled.
///
/// @ingroup gpa_counter_scheduling
///
/// @param [in] gpa_session_id Unique identifier of the session.
/// @param [in] counter_name The name of the counter to disable.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorCannotChangeCountersWhenSampling It is invalid to disable a counter while the session is active.
/// @retval kGpaStatusErrorContextNotOpen The context on this session is not open.
/// @retval kGpaStatusErrorCounterNotFound The named counter cannot be found.
/// @retval kGpaStatusErrorIncompatibleSampleTypes The specified counter is not compatible with the supplied session.
/// @retval kGpaStatusErrorNotEnabled The counter could not be enabled.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaDisableCounterByName(GpaSessionId gpa_session_id, const char* counter_name);

/// @brief Enables all counters.
///
/// Subsequent sampling sessions will provide values for all counters.
/// Initially all counters are disabled, and must explicitly be enabled.
///
/// @ingroup gpa_counter_scheduling
///
/// @param [in] gpa_session_id Unique identifier of the session.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorCannotChangeCountersWhenSampling It is invalid to enable a counter while the session is active.
/// @retval kGpaStatusErrorContextNotOpen The context on this session is not open.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaEnableAllCounters(GpaSessionId gpa_session_id);

/// @brief Disables all counters.
///
/// Subsequent sampling sessions will not provide values for any disabled counters.
/// Initially all counters are disabled, and must explicitly be enabled.
///
/// @ingroup gpa_counter_scheduling
///
/// @param [in] gpa_session_id Unique identifier of the session.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorCannotChangeCountersWhenSampling It is invalid to disable a counter while the session is active.
/// @retval kGpaStatusErrorContextNotOpen The context on this session is not open.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaDisableAllCounters(GpaSessionId gpa_session_id);

/// @defgroup gpa_counter_scheduling Counter Scheduling Queries

/// @brief Gets the number of passes required for the currently enabled set of counters.
///
/// This represents the number of times the same sequence must be repeated to capture the counter data.
/// On each pass a different (compatible) set of counters will be measured.
///
/// @ingroup gpa_counter_scheduling
///
/// @param [in] gpa_session_id Unique identifier of the session.
/// @param [out] number_of_passes The value which will hold the number of required passes upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorContextNotOpen The context on this session is not open.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetPassCount(GpaSessionId gpa_session_id, GpaUInt32* number_of_passes);

/// @brief Gets the number of enabled counters.
///
/// @ingroup gpa_counter_scheduling
///
/// @param [in] gpa_session_id Unique identifier of the session.
/// @param [out] enabled_counter_count The value which will hold the number of enabled counters contained within the session upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorContextNotOpen The context on this session is not open.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetNumEnabledCounters(GpaSessionId gpa_session_id, GpaUInt32* enabled_counter_count);

/// @brief Gets the counter index for an enabled counter.
///
/// This is meant to be used with GPA_GetNumEnabledCounters. Once you determine the number of enabled counters,
/// you can use GPA_GetEnabledIndex to determine which counters are enabled.
///
/// @ingroup gpa_counter_scheduling
///
/// @param [in] gpa_session_id Unique identifier of the session.
/// @param [in] enabled_number The number of the enabled counter to get the counter index for. Must lie between 0 and (GPA_GetNumEnabledCounters result - 1).
/// @param [out] enabled_counter_index The value that will hold the index of the counter upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorContextNotOpen The context on this session is not open.
/// @retval kGpaStatusErrorIndexOutOfRange The enabled number is higher than the number of enabled counters.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetEnabledIndex(GpaSessionId gpa_session_id, GpaUInt32 enabled_number, GpaUInt32* enabled_counter_index);

/// @brief Checks whether or not a counter is enabled.
///
/// @ingroup gpa_counter_scheduling
///
/// @param [in] gpa_session_id Unique identifier of the session.
/// @param [in] counter_index The index of the counter. Must lie between 0 and (GpaGetNumCounters result - 1).
///
/// @retval kGpaStatusOk is returned if the counter is enabled.
/// @retval kGpaStatusErrorCounterNotFound is returned if it is not enabled.
/// @retval kGpaStatusErrorNullPointer If any of the parameters are NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorContextNotOpen The context on this session is not open.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaIsCounterEnabled(GpaSessionId gpa_session_id, GpaUInt32 counter_index);

/// @defgroup gpa_sample_handling GPA Sample Handling

/// @brief Begins command list for sampling.
///
/// You will be unable to create samples on the specified command list before GpaBeginCommandList is called.
/// Command list corresponds to ID3D12GraphicsCommandList in DirectX 12 and vkCommandBuffer in Vulkan.
/// In OpenCL/OpenGL/DirectX 11, use GPA_NULL_COMMAND_LIST for the command_list parameter and kGpaCommandListNone for the command_list_type parameter.
///
/// @ingroup gpa_sample_handling
///
/// @param [in] gpa_session_id Unique identifier of the GPA Session Object.
/// @param [in] pass_index 0-based index of the pass.
/// @param [in] command_list The command list on which to begin sampling - ignored in OpenCL/OpenGL/DX11 applications.
/// @param [in] command_list_type Command list type.
/// @param [out] gpa_command_list_id GPA-generated unique command list id.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If the command list is required, it must not be NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorSessionNotStarted The supplied session has not been started.
/// @retval kGpaStatusErrorInvalidParameter The command list type is invalid, or the command list must be null and the type must be kGpaCommandListNone.
/// @retval kGpaStatusErrorCommandListAlreadyStarted The supplied command list has already been started, and cannot be started again.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaBeginCommandList(GpaSessionId       gpa_session_id,
                                           GpaUInt32          pass_index,
                                           void*              command_list,
                                           GpaCommandListType command_list_type,
                                           GpaCommandListId*  gpa_command_list_id);

/// @brief Ends command list for sampling.
///
/// You will be unable to create samples on the specified command list after GPA_EndCommandList is called.
///
/// @ingroup gpa_sample_handling
///
/// @param [in] gpa_command_list_id The command list on which to end sampling - ignored in OpenCL/OpenGL applications.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If a NULL command list has been supplied.
/// @retval kGpaStatusErrorCommandListNotFound If the supplied command list cannot be found.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaEndCommandList(GpaCommandListId gpa_command_list_id);

/// @brief Begins a sample in a command list.
///
/// A sample is a particular workload for which counters will be collected.
/// If the owning session was created with GPA_SESSION_SAMPLE_TYPE_DISCRETE_COUNTER and
/// one or more counters have been enabled, then those counters will be collected for this sample.
/// Each sample must be associated with a GPA command list.
/// Samples can be created by multiple threads provided no two threads are creating samples on same command list.
/// You must provide a unique Id for every new sample. When performing multiple passes, a sample must exist in all passes.
/// You may create as many samples as needed. However, nesting of samples is not allowed.
/// Each sample must be wrapped in sequence of GPA_BeginSample/GPA_EndSample before starting another one.
/// A sample can be started in one primary command list and continued/ended on another primary command list - See GPA_ContinueSampleOnCommandList.
///
/// @ingroup gpa_sample_handling
///
/// @param [in] sample_id Unique sample id.
/// @param [in] gpa_command_list_id Unique identifier of a previously-created GPA Command List Object.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If a NULL command list has been supplied.
/// @retval kGpaStatusErrorCommandListNotFound If the supplied command list cannot be found.
/// @retval kGpaStatusErrorIndexOutOfRange The sample has been started in too many passes.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaBeginSample(GpaUInt32 sample_id, GpaCommandListId gpa_command_list_id);

/// @brief Ends a sample in a command list.
///
/// A sample is a particular workload for which counters will be collected.
/// If the owning session was created with kGpaSessionSampleTypeDiscreteCounter and
/// one or more counters have been enabled, then those counters will be collected for this sample.
/// Each sample must be associated with a GPA command list.
/// Samples can be created by using multiple threads provided no two threads are creating samples on same command list.
/// You must provide a unique Id for every new sample.
/// You may create as many samples as needed. However, nesting of samples is not allowed.
/// Each sample must be wrapped in sequence of GpaBeginSample/GpaEndSample before starting another one.
/// A sample can be started in one primary command list and continued/ended on another primary command list - See GpaContinueSampleOnCommandList.
///
/// @ingroup gpa_sample_handling
///
/// @param [in] gpa_command_list_id Command list id on which the sample is ending - the command list may be different than the command list on which the sample was started.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer If a NULL command list has been supplied.
/// @retval kGpaStatusErrorCommandListNotFound If the supplied command list cannot be found.
/// @retval kGpaStatusErrorIndexOutOfRange The sample has been ended in too many passes.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaEndSample(GpaCommandListId gpa_command_list_id);

/// @brief Continues a primary command list sample on another primary command list.
///
/// This function is only supported for DirectX 12 and Vulkan.
/// Samples can be started on one primary command list and continued/ended on another primary command list.
///
/// @ingroup gpa_sample_handling
///
/// @param [in] source_sample_id The sample id of the sample being continued on a different command list.
/// @param [in] primary_gpa_command_list_id Primary command list id on which the sample is continuing.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorApiNotSupported This entrypoint is being called in a graphics API that does not support it.
/// @retval kGpaStatusErrorNullPointer The supplied command list id is NULL.
/// @retval kGpaStatusErrorCommandListNotFound The supplied command list id cannot be found.
/// @retval kGpaStatusErrorSampleNotFound The source sample id cannot be found.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaContinueSampleOnCommandList(GpaUInt32 source_sample_id, GpaCommandListId primary_gpa_command_list_id);

/// @brief Copies a set of samples from a secondary command list back to the primary command list that executed the secondary command list.
///
/// This function is only supported for DirectX 12 and Vulkan.
/// GPA doesn't collect data for the samples created on secondary command lists unless they are copied to a new set of samples for the primary command list.
///
/// @ingroup gpa_sample_handling
///
/// @param [in] secondary_gpa_command_list_id Aecondary command list where the secondary samples were created.
/// @param [in] primary_gpa_command_list_id Primary command list to which the samples results should be copied. This should be the command list that executed the secondary command list.
/// @param [in] number_of_samples Number of secondary samples.
/// @param [in] new_sample_ids New sample ids on a primary command list.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorApiNotSupported This entrypoint is being called in a graphics API that does not support it.
/// @retval kGpaStatusErrorNullPointer One of the supplied command list IDs is NULL.
/// @retval kGpaStatusErrorCommandListNotFound One of the supplied command list IDs cannot be found.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaCopySecondarySamples(GpaCommandListId secondary_gpa_command_list_id,
                                               GpaCommandListId primary_gpa_command_list_id,
                                               GpaUInt32        number_of_samples,
                                               GpaUInt32*       new_sample_ids);

/// @brief Gets the number of samples created for the specified session.
///
/// This is useful if samples are conditionally created and a count is not kept.
///
/// @ingroup gpa_sample_handling
///
/// @param [in] gpa_session_id Unique identifier of the GPA Session Object.
/// @param [out] sample_count The value which will hold the number of samples contained within the session upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer One of the parameters is NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorSessionNotEnded The session has not been ended.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetSampleCount(GpaSessionId gpa_session_id, GpaUInt32* sample_count);

/// @brief Gets the sample id by index.
///
/// This is useful if sample ids are either not zero-based or not consecutive.
///
/// @ingroup gpa_sample_handling
///
/// @param [in] gpa_session_id Unique identifier of the GPA Session Object.
/// @param [in] index The index of the sample. Must lie between 0 and (GpaGetSampleCount result - 1).
/// @param [out] sample_id The value that will hold the id of the sample upon successful execution.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer One of the parameters is NULL.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorSessionNotEnded The session has not been ended.
/// @retval kGpaStatusErrorSampleNotFound The supplied index is greater than the number of samples.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetSampleId(GpaSessionId gpa_session_id, GpaUInt32 index, GpaUInt32* sample_id);

/// @defgroup gpa_query_results Query Results

/// @brief Checks whether or not a pass has finished.
///
/// After sampling a workload, results may be available immediately or take a certain amount of time to become available.
/// This function allows you to determine when the pass has finished and associated resources are no longer needed in the application.
/// The function does not block, permitting periodic polling.
/// The application must not free its resources until this function returns kGpaStatusOk.
///
/// @ingroup gpa_query_results
///
/// @param [in] gpa_session_id Unique identifier of the GPA Session Object.
/// @param [in] pass_index 0-based index of the pass.
///
/// @retval kGpaStatusOk If the pass is complete.
/// @retval kGpaStatusResultNotReady If the result is not yet ready.
/// @retval kGpaStatusErrorNullPointer An invalid session was supplied.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorSessionNotStarted The supplied session has not been started.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaIsPassComplete(GpaSessionId gpa_session_id, GpaUInt32 pass_index);

/// @brief Checks if results for all samples within a session are available.
///
/// After a sampling session results may be available immediately or take a certain amount of time to become available.
/// This function allows you to determine when the results of a session can be read.
/// The function does not block, permitting periodic polling.
/// To block until a sample is ready use GPA_GetSampleResult instead.
///
/// @ingroup gpa_query_results
///
/// @param [in] gpa_session_id The value that will be set to the session identifier.
///
/// @retval kGpaStatusOk If the session is complete.
/// @retval kGpaStatusResultNotReady If the result is not yet ready.
/// @retval kGpaStatusErrorNullPointer An invalid session was supplied.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorSessionNotStarted If the supplied session has not been started.
/// @retval kGpaStatusErrorSessionNotEnded The session has not been ended.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaIsSessionComplete(GpaSessionId gpa_session_id);

/// @brief Gets the result size (in bytes) for a given sample.
///
/// For discrete counter samples, the size will be the same for all samples, so it would be valid to retrieve the
/// result size for one sample and use that when retrieving results for all samples.
///
/// @ingroup gpa_query_results
///
/// @param [in] gpa_session_id Unique identifier of the GPA Session Object.
/// @param [in] sample_id The identifier of the sample to get the result size for.
/// @param [out] sample_result_size_in_bytes The value that will be set to the result size upon successful execution  - this value needs to be passed to GetSampleResult.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer An invalid session was supplied.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorSampleNotFound The supplied sample id could not be found.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetSampleResultSize(GpaSessionId gpa_session_id, GpaUInt32 sample_id, size_t* sample_result_size_in_bytes);

/// @brief Gets the result data for a given sample.
///
/// This function will block until results are ready. Use GPA_IsSessionComplete to check if results are ready.
///
/// @ingroup gpa_query_results
///
/// @param [in] gpa_session_id The session identifier with the sample you wish to retrieve the result of.
/// @param [in] sample_id The identifier of the sample to get the result for.
/// @param [in] sample_result_size_in_bytes Size of sample in bytes.
/// @param [out] counter_sample_results Address to which the counter data for the sample will be copied to.
///
/// @return The GPA result status of the operation.
/// @retval kGpaStatusOk If the operation is successful.
/// @retval kGpaStatusErrorNullPointer An invalid or NULL parameter was supplied.
/// @retval kGpaStatusErrorSessionNotFound The supplied session could not be found.
/// @retval kGpaStatusErrorSampleNotFound The supplied sample id could not be found.
/// @retval kGpaStatusErrorReadingSampleResult The supplied buffer is too small for the sample results.
/// @retval kGpaStatusErrorFailed If an internal error has occurred.
/// @retval kGpaStatusErrorException If an unexpected error has occurred.
GPA_LIB_DECL GpaStatus GpaGetSampleResult(GpaSessionId gpa_session_id, GpaUInt32 sample_id, size_t sample_result_size_in_bytes, void* counter_sample_results);

/// @defgroup gpa_status_query GPA Status/Error Query

/// @brief Gets a string representation of the specified GPA status value.
///
/// Provides a simple method to convert a status enum value into a string which can be used to display log messages.
///
/// @ingroup gpa_status_query
///
/// @param [in] status The status whose string representation is needed.
///
/// @return A string which briefly describes the specified status.
GPA_LIB_DECL const char* GpaGetStatusAsStr(GpaStatus status);

#else  // Not USE_GPA
#include "gpu_perf_api_stub.h"
#endif  // USE_GPA

#endif  // GPU_PERFORMANCE_API_GPU_PERF_API_H_
