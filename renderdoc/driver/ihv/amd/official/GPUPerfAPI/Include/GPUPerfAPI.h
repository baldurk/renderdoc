//==============================================================================
// Copyright (c) 2010-2018 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  This is the header file that must be included by an application that
///         wishes to use GPUPerfAPI. It defines all the available entrypoints.
//==============================================================================

#ifndef _GPUPERFAPI_H_
#define _GPUPERFAPI_H_

#ifndef GPALIB_DECL
    /// macro for exporting an API function
    #ifdef _WIN32
        #ifdef __cplusplus
            #define GPALIB_DECL extern "C" __declspec( dllimport )
        #else
            #define GPALIB_DECL __declspec( dllimport )
        #endif
    #else //__linux__
        #ifdef __cplusplus
            #define GPALIB_DECL extern "C"
        #else
            #define GPALIB_DECL extern
        #endif
    #endif
#endif

#if DISABLE_GPA
    #define USE_GPA 0 ///< Macro used to determine if GPA functions should be stubbed out
#else
    #define USE_GPA 1 ///< Macro used to determine if GPA functions should be stubbed out
#endif

#include "GPUPerfAPITypes.h"
#include "GPUPerfAPIFunctionTypes.h"

#define GPA_FUNCTION_TABLE_MAJOR_VERSION_NUMBER 3                                  ///< API major version -- will be incremented if/when there are non-backwards compatible API changes introduced
#define GPA_FUNCTION_TABLE_MINOR_VERSION_NUMBER (sizeof(struct _GPAFunctionTable)) ///< API minor version -- set to the structure size; will increase when new API functions are added

/// Structure to hold the function table of the exported GPA APIs
typedef struct _GPAFunctionTable
{
    gpa_uint32 m_majorVer; ///< API major version
    gpa_uint32 m_minorVer; ///< API minor version

#define GPA_FUNCTION_PREFIX(func) func##PtrType func; ///< Macro used by GPAFunctions.h
#include "GPAFunctions.h"
#undef GPA_FUNCTION_PREFIX

#ifdef __cplusplus
    /// Constructor
    _GPAFunctionTable()
    {
        m_majorVer = GPA_FUNCTION_TABLE_MAJOR_VERSION_NUMBER;
        m_minorVer = GPA_FUNCTION_TABLE_MINOR_VERSION_NUMBER;
#define GPA_FUNCTION_PREFIX(func) func = nullptr; ///< Macro used by GPAFunctions.h
#include "GPAFunctions.h"
#undef GPA_FUNCTION_PREFIX
    }
#endif

} GPAFunctionTable;

#if USE_GPA

// GPA API Version

/// \brief Gets the GPA version
///
/// \param[out] pMajorVersion The value that will hold the major version of GPA upon successful execution.
/// \param[out] pMinorVersion The value that will hold the minor version of GPA upon successful execution.
/// \param[out] pBuild The value that will hold the build number of GPA upon successful execution.
/// \param[out] pUpdateVersion The value that will hold the update version of GPA upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetVersion(
    gpa_uint32* pMajorVersion,
    gpa_uint32* pMinorVersion,
    gpa_uint32* pBuild,
    gpa_uint32* pUpdateVersion);

// GPA API Table

/// \brief Gets the GPA API function table.
///
/// \param[out] pGPAFuncTable pointer to GPA Function table structure.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetFuncTable(
    void* pGPAFuncTable);

// Logging

/// \brief Registers a callback function to receive log messages.
///
/// Only one callback function can be registered, so the implementation should be able
/// to handle the different types of messages. A parameter to the callback function will
/// indicate the message type being received. Messages will not contain a newline character
/// at the end of the message.
/// \param[in] loggingType Identifies the type of messages to receive callbacks for.
/// \param[in] pCallbackFuncPtr Pointer to the callback function.
/// \return GPA_STATUS_OK, unless the callbackFuncPtr is nullptr and the loggingType is not
/// GPA_LOGGING_NONE, in which case GPA_STATUS_ERROR_NULL_POINTER is returned.
GPALIB_DECL GPA_Status GPA_RegisterLoggingCallback(
    GPA_Logging_Type loggingType,
    GPA_LoggingCallbackPtrType pCallbackFuncPtr);

// Init / Destroy GPA

/// \brief Initializes the driver so that counters are exposed.
///
/// This function must be called before the rendering context or device is created. In the case of DirectX 12
/// or Vulkan, this function must be called before a queue is created. For HSA/ROCm, this function must be called
/// before the first call to hsa_init.
/// \param[in] flags Flags used to initialize GPA. This should be a combination of GPA_Initialize_Bits.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_Initialize(
    GPA_InitializeFlags flags);

/// \brief Undoes any initialization to ensure proper behavior in applications that are not being profiled.
///
/// This function must be called after the rendering context or device is released / destroyed.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_Destroy();

// Context Startup / Finish

/// \brief Opens the specified context, which provides access to GPU performance counters.
///
/// This function must be called after GPA_Initialize and before any other GPA functions.
/// \param[in] pContext The context to open counters for. Typically a device pointer. Refer to GPA API documentation for further details.
/// \param[in] flags Flags used to initialize the context. This should be a combination of GPA_OpenContext_Bits.
/// \param[out] pContextId Unique identifier of the opened context.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_OpenContext(
    void* pContext,
    GPA_OpenContextFlags flags,
    GPA_ContextId* pContextId);

/// \brief Closes the specified context, which ends access to GPU performance counters.
///
/// GPA functions should not be called again until the counters are reopened with GPA_OpenContext.
/// \param[in] contextId Unique identifier of the opened context.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_CloseContext(
    GPA_ContextId contextId);

// Context Interrogation

/// \brief Gets a mask of the sample types supported by the specified context.
///
/// A call to GPA_CreateSession will fail if the requested sample types are not compatible with the context's sample types.
/// supported by the context.
/// \param[in] contextId Unique identifier of the opened context.
/// \param[out] pSampleTypes The value that will be set to the mask of the supported sample types upon successful execution. This will be a combination of GPA_Sample_Bits.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetSupportedSampleTypes(
    GPA_ContextId contextId,
    GPA_ContextSampleTypeFlags* pSampleTypes);

/// \brief Gets the GPU device id and revision id associated with the specified context.
///
/// \param[in] contextId Unique identifier of the opened context.
/// \param[out] pDeviceId The value that will be set to the device id upon successful execution.
/// \param[out] pRevisionId The value that will be set to the device revision id upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetDeviceAndRevisionId(
    GPA_ContextId contextId,
    gpa_uint32* pDeviceId,
    gpa_uint32* pRevisionId);

/// \brief Gets the device name of the GPU associated with the specified context.
///
/// \param[in] contextId Unique identifier of the opened context.
/// \param[out] ppDeviceName The value that will be set to the device name upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetDeviceName(
    GPA_ContextId contextId,
    const char** ppDeviceName);

// Counter Interrogation

/// \brief Gets the number of counters available.
///
/// \param[in] contextId Unique identifier of the opened context.
/// \param[out] pCount The value which will hold the count upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetNumCounters(
    GPA_ContextId contextId,
    gpa_uint32* pCount);

/// \brief Gets the name of the specified counter.
///
/// \param[in] contextId Unique identifier of the opened context.
/// \param[in] index The index of the counter whose name is needed. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \param[out] ppName The address which will hold the name upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterName(
    GPA_ContextId contextId,
    gpa_uint32 index,
    const char** ppName);

/// \brief Gets index of a counter given its name (case insensitive).
///
/// \param[in] contextId Unique identifier of the session.
/// \param[in] pCounterName The name of the counter whose index is needed.
/// \param[out] pIndex The address which will hold the index upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterIndex(
    GPA_ContextId contextId,
    const char* pCounterName,
    gpa_uint32* pIndex);

/// \brief Gets the group of the specified counter.
///
/// \param[in] contextId Unique identifier of the opened context.
/// \param[in] index The index of the counter whose group is needed. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \param[out] ppGroup The address which will hold the group string upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterGroup(
    GPA_ContextId contextId,
    gpa_uint32 index,
    const char** ppGroup);

/// \brief Gets the description of the specified counter.
///
/// \param[in] contextId Unique identifier of the opened context.
/// \param[in] index The index of the counter whose description is needed.. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \param[out] ppDescription The address which will hold the description upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterDescription(
    GPA_ContextId contextId,
    gpa_uint32 index,
    const char** ppDescription);

/// \brief Gets the data type of the specified counter.
///
/// \param[in] contextId Unique identifier of the opened context.
/// \param[in] index The index of the counter whose data type is needed.. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \param[out] pCounterDataType The value which will hold the counter data type upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterDataType(
    GPA_ContextId contextId,
    gpa_uint32 index,
    GPA_Data_Type* pCounterDataType);

/// \brief Gets the usage type of the specified counter.
///
/// \param[in] contextId Unique identifier of the opened context.
/// \param[in] index The index of the counter whose usage type is needed.. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \param[out] pCounterUsageType The value which will hold the counter usage type upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterUsageType(
    GPA_ContextId contextId,
    gpa_uint32 index,
    GPA_Usage_Type* pCounterUsageType);

/// \brief Gets the UUID of the specified counter.
///
/// \param[in] contextId Unique identifier of the opened context.
/// \param[in] index The index of the counter whose UUID is needed.. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \param[out] pCounterUuid The value which will hold the counter UUID upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterUuid(
    GPA_ContextId contextId,
    gpa_uint32 index,
    GPA_UUID* pCounterUuid);

/// \brief Gets the supported sample type of the specified counter.
///
/// Currently, only a single counter type (discrete) is supported
/// \param[in] contextId Unique identifier of the opened context.
/// \param[in] index The index of the counter whose sample type is needed.. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \param[out] pCounterSampleType The value which will hold the counter's supported sample type upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterSampleType(
    GPA_ContextId contextId,
    gpa_uint32 index,
    GPA_Counter_Sample_Type* pCounterSampleType);

/// \brief Gets a string representation of the specified counter data type.
///
/// This could be used to display counter types along with their name or value.
/// For example, the GPA_DATA_TYPE_UINT64 counterDataType would return "gpa_uint64".
/// \param[in] counterDataType The data type whose string representation is needed.
/// \param[out] ppTypeStr The address which will hold the string representation upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetDataTypeAsStr(
    GPA_Data_Type counterDataType,
    const char** ppTypeStr);

/// \brief Gets a string representation of the specified counter usage type.
///
/// This could be used to display counter units along with their name or value.
/// For example, the GPA_USAGE_TYPE_PERCENTAGE counterUsageType would return "percentage".
/// \param[in] counterUsageType The usage type whose string representation is needed.
/// \param[out] ppUsageTypeStr The address which will hold the string representation upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetUsageTypeAsStr(
    GPA_Usage_Type counterUsageType,
    const char** ppUsageTypeStr);

// Session handling

/// \brief Creates a session on the specified context.
///
/// A unique session identifier will be returned which allows counters to be enabled, samples to be
/// measured, and stores the results of the profile. The sample type for the session should be
/// specified by the caller. The requested sample types must be supported by the supplied context.
/// Use GPA_GetSupportedSampleTypes to determine which sample types are supported by a context.
/// \param[in] contextId The context on which to create the session.
/// \param[in] sampleType The sample type that will be created on this session.
/// \param[out] pSessionId The address of a GPA_SessionId which will be populated with the created session Id.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_CreateSession(
    GPA_ContextId contextId,
    GPA_Session_Sample_Type sampleType,
    GPA_SessionId* pSessionId);

/// \brief Deletes a session object.
///
/// Deletes the specified session, along with all counter results associated with the session.
/// \param[in] sessionId The session id that is to be deleted.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_DeleteSession(
    GPA_SessionId sessionId);

/// \brief Begins sampling with the currently enabled set of counters.
///
/// This must be called to begin the counter sampling process.
/// The set of enabled counters cannot be changed inside a BeginSession/EndSession sequence.
/// \param[in] sessionId Unique identifier of the GPA Session Object.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_BeginSession(
    GPA_SessionId sessionId);

/// \brief Ends sampling with the currently enabled set of counters.
///
/// \param[in] sessionId Unique identifier of the GPA Session Object.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EndSession(
    GPA_SessionId sessionId);

// Counter Scheduling

/// \brief Enables the specified counter.
///
/// Subsequent sampling sessions will provide values for any enabled counters.
/// Initially all counters are disabled, and must explicitly be enabled.
/// \param[in] sessionId Unique identifier of the session on which to enable the counter.
/// \param[in] index The index of the counter to enable. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EnableCounter(
    GPA_SessionId sessionId,
    gpa_uint32 index);

/// \brief Disables the specified counter.
///
/// Subsequent sampling sessions will not provide values for any disabled counters.
/// Initially all counters are disabled, and must explicitly be enabled.
/// \param[in] sessionId Unique identifier of the session on which to disable the counter.
/// \param[in] index The index of the counter to disable. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_DisableCounter(
    GPA_SessionId sessionId,
    gpa_uint32 index);

/// \brief Enables the counter with the specified counter name (case insensitive).
///
/// Subsequent sampling sessions will provide values for any enabled counters.
/// Initially all counters are disabled, and must explicitly be enabled.
/// \param[in] sessionId Unique identifier of the session.
/// \param[in] pCounterName The name of the counter to enable.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EnableCounterByName(
    GPA_SessionId sessionId,
    const char* pCounterName);

/// \brief Disables the counter with the specified counter name (case insensitive).
///
/// Subsequent sampling sessions will not provide values for any disabled counters.
/// Initially all counters are disabled, and must explicitly be enabled.
/// \param[in] sessionId Unique identifier of the session.
/// \param[in] pCounterName The name of the counter to disable.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_DisableCounterByName(
    GPA_SessionId sessionId,
    const char* pCounterName);

/// \brief Enables all counters.
///
/// Subsequent sampling sessions will provide values for all counters.
/// Initially all counters are disabled, and must explicitly be enabled.
/// \param[in] sessionId Unique identifier of the session.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EnableAllCounters(
    GPA_SessionId sessionId);

/// \brief Disables all counters.
///
/// Subsequent sampling sessions will not provide values for any disabled counters.
/// Initially all counters are disabled, and must explicitly be enabled.
/// \param[in] sessionId Unique identifier of the session.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_DisableAllCounters(
    GPA_SessionId sessionId);

// Query Counter Scheduling

/// \brief Gets the number of passes required for the currently enabled set of counters.
///
/// This represents the number of times the same sequence must be repeated to capture the counter data.
/// On each pass a different (compatible) set of counters will be measured.
/// \param[in] sessionId Unique identifier of the session.
/// \param[out] pNumPasses The value which will hold the number of required passes upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetPassCount(
    GPA_SessionId sessionId,
    gpa_uint32* pNumPasses);

/// \brief Gets the number of enabled counters.
///
/// \param[in] sessionId Unique identifier of the session.
/// \param[out] pCount The value which will hold the number of enabled counters contained within the session upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetNumEnabledCounters(
    GPA_SessionId sessionId,
    gpa_uint32* pCount);

/// \brief Gets the counter index for an enabled counter.
///
/// This is meant to be used with GPA_GetNumEnabledCounters. Once you determine the number of enabled counters,
/// you can use GPA_GetEnabledIndex to determine which counters are enabled.
/// \param[in] sessionId Unique identifier of the session.
/// \param[in] enabledNumber The number of the enabled counter to get the counter index for. Must lie between 0 and (GPA_GetNumEnabledCounters result - 1).
/// \param[out] pEnabledCounterIndex The value that will hold the index of the counter upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetEnabledIndex(
    GPA_SessionId sessionId,
    gpa_uint32 enabledNumber,
    gpa_uint32* pEnabledCounterIndex);

/// \brief Checks whether or not a counter is enabled.
///
/// \param[in] sessionId Unique identifier of the session.
/// \param[in] counterIndex The index of the counter. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \return GPA_STATUS_OK is returned if the counter is enabled. GPA_STATUS_ERROR_COUNTER_NOT_FOUND is returned if it is not enabled.
GPALIB_DECL GPA_Status GPA_IsCounterEnabled(
    GPA_SessionId sessionId,
    gpa_uint32 counterIndex);

// Sample Handling

/// \brief Begins command list for sampling.
///
/// You will be unable to create samples on the specified command list before GPA_BeginCommandList is called.
/// Command list corresponds to ID3D12GraphicsCommandList in DirectX 12 and vkCommandBuffer in Vulkan.
/// In OpenCL/OpenGL/HSA/DirectX 11, use GPA_NULL_COMMAND_LIST for the pCommandList parameter and GPA_COMMAND_LIST_NONE for the commandListType parameter.
/// \param[in] sessionId unique identifier of the GPA Session Object.
/// \param[in] passIndex 0-based index of the pass.
/// \param[in] pCommandList the command list on which to begin sampling - ignored in OpenCL/HSA/OpenGL/DX11 applications.
/// \param[in] commandListType command list type.
/// \param[out] pCommandListId GPA-generated unique command list id.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_BeginCommandList(
    GPA_SessionId sessionId,
    gpa_uint32 passIndex,
    void* pCommandList,
    GPA_Command_List_Type commandListType,
    GPA_CommandListId* pCommandListId);

/// \brief Ends command list for sampling.
///
/// You will be unable to create samples on the specified command list after GPA_EndCommandList is called.
/// \param[in] commandListId the command list on which to end sampling - ignored in OpenCL/HSA/OpenGL applications.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EndCommandList(
    GPA_CommandListId commandListId);

/// \brief Begins a sample in a command list.
///
/// A sample is a particular workload for which counters will be collected.
/// If the owning session was created with GPA_SAMPLE_TYPE_DISCRETE_COUNTER and
/// one or more counters have been enabled, then those counters will be collected for this sample.
/// Each sample must be associated with a GPA command list.
/// Samples can be created by multiple threads provided no two threads are creating samples on same command list.
/// You must provide a unique Id for every new sample. When performing multiple passes, a sample must exist in all passes.
/// You may create as many samples as needed. However, nesting of samples is not allowed.
/// Each sample must be wrapped in sequence of GPA_BeginSample/GPA_EndSample before starting another one.
/// A sample can be started in one primary command list and continued/ended on another primary command list - See GPA_ContinueSampleOnCommandList.
/// \param[in] sampleId unique sample id.
/// \param[in] commandListId unique identifier of a previously-created GPA Command List Object.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_BeginSample(
    gpa_uint32 sampleId,
    GPA_CommandListId commandListId);

/// \brief Ends a sample in a command list.
///
/// A sample is a particular workload for which counters will be collected.
/// If the owning session was created with GPA_SAMPLE_TYPE_DISCRETE_COUNTER and
/// one or more counters have been enabled, then those counters will be collected for this sample.
/// Each sample must be associated with a GPA command list.
/// Samples can be created by using multiple threads provided no two threads are creating samples on same command list.
/// You must provide a unique Id for every new sample.
/// You may create as many samples as needed. However, nesting of samples is not allowed.
/// Each sample must be wrapped in sequence of GPA_BeginSample/GPA_EndSample before starting another one.
/// A sample can be started in one primary command list and continued/ended on another primary command list - See GPA_ContinueSampleOnCommandList.
/// \param[in] commandListId command list id on which the sample is ending - the command list may be different than the command list on which the sample was started.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EndSample(
    GPA_CommandListId commandListId);

/// \brief Continues a primary command list sample on another primary command list.
///
/// This function is only supported for DirectX 12 and Vulkan.
/// Samples can be started on one primary command list and continued/ended on another primary command list.
/// \param[in] srcSampleId The sample id of the sample being continued on a different command list.
/// \param[in] primaryCommandListId primary command list id on which the sample is continuing.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_ContinueSampleOnCommandList(
    gpa_uint32 srcSampleId,
    GPA_CommandListId primaryCommandListId);

/// \brief Copies a set of samples from a secondary command list back to the primary command list that executed the secondary command list.
///
/// This function is only supported for DirectX 12 and Vulkan.
/// GPA doesn't collect data for the samples created on secondary command lists unless they are copied to a new set of samples for the primary command list.
/// \param[in] secondaryCommandListId secondary command list where the secondary samples were created.
/// \param[in] primaryCommandListId primary command list to which the samples results should be copied. This should be the command list that executed the secondary command list.
/// \param[in] numSamples number of secondary samples.
/// \param[in] pNewSampleIds new sample ids on a primary command list.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_CopySecondarySamples(
    GPA_CommandListId secondaryCommandListId,
    GPA_CommandListId primaryCommandListId,
    gpa_uint32 numSamples,
    gpa_uint32* pNewSampleIds);

/// \brief Gets the number of samples created for the specified session.
///
/// This is useful if samples are conditionally created and a count is not kept.
/// \param[in] sessionId Unique identifier of the GPA Session Object.
/// \param[out] pSampleCount The value which will hold the number of samples contained within the session upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetSampleCount(
    GPA_SessionId sessionId,
    gpa_uint32* pSampleCount);

/// \brief Gets the sample id by index
///
/// This is useful if sample ids are either not zero-based or not consecutive.
/// \param[in] sessionId Unique identifier of the GPA Session Object.
/// \param[in] index The index of the sample. Must lie between 0 and (GPA_GetSampleCount result - 1).
/// \param[out] pSampleId The value that will hold the id of the sample upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetSampleId(
    GPA_SessionId sessionId,
    gpa_uint32 index,
    gpa_uint32* pSampleId);

// Query Results

/// \brief Checks whether or not a pass has finished.
///
/// After sampling a workload, results may be available immediately or take a certain amount of time to become available.
/// This function allows you to determine when the pass has finished and associated resources are no longer needed in the application.
/// The function does not block, permitting periodic polling.
/// The application must not free its resources until this function returns GPA_STATUS_OK.
/// \param[in] sessionId Unique identifier of the GPA Session Object.
/// \param[in] passIndex 0-based index of the pass.
/// \return GPA_STATUS_OK if pass is complete else GPA_STATUS_RESULT_NOT_READY.
GPALIB_DECL GPA_Status GPA_IsPassComplete(
    GPA_SessionId sessionId,
    gpa_uint32 passIndex);

/// \brief Checks if results for all samples within a session are available.
///
/// After a sampling session results may be available immediately or take a certain amount of time to become available.
/// This function allows you to determine when the results of a session can be read.
/// The function does not block, permitting periodic polling.
/// To block until a sample is ready use GPA_GetSampleResult instead.
/// \param[in] sessionId The value that will be set to the session identifier.
/// \return GPA_STATUS_OK if pass is complete else GPA_STATUS_RESULT_NOT_READY.
GPALIB_DECL GPA_Status GPA_IsSessionComplete(
    GPA_SessionId sessionId);

/// \brief Gets the result size (in bytes) for a given sample.
///
/// For discrete counter samples, the size will be the same for all samples, so it would be valid to retrieve the
/// result size for one sample and use that when retrieving results for all samples.
/// \param[in] sessionId Unique identifier of the GPA Session Object.
/// \param[in] sampleId The identifier of the sample to get the result size for.
/// \param[out] pSampleResultSizeInBytes The value that will be set to the result size upon successful execution  - this value needs to be passed to GetSampleResult.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetSampleResultSize(
    GPA_SessionId sessionId,
    gpa_uint32 sampleId,
    size_t* pSampleResultSizeInBytes);

/// \brief Gets the result data for a given sample.
///
/// This function will block until results are ready. Use GPA_IsSessionComplete to check if results are ready.
/// \param[in] sessionId The session identifier with the sample you wish to retrieve the result of.
/// \param[in] sampleId The identifier of the sample to get the result for.
/// \param[in] sampleResultSizeInBytes size of sample in bytes.
/// \param[out] pCounterSampleResults address to which the counter data for the sample will be copied to.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetSampleResult(
    GPA_SessionId sessionId,
    gpa_uint32 sampleId,
    size_t sampleResultSizeInBytes,
    void* pCounterSampleResults);

// Status / Error Query

/// \brief Gets a string representation of the specified GPA status value.
///
/// Provides a simple method to convert a status enum value into a string which can be used to display log messages.
/// \param[in] status The status whose string representation is needed.
/// \return A string which briefly describes the specified status.
GPALIB_DECL const char* GPA_GetStatusAsStr(
    GPA_Status status);

#else /// Not USE_GPA
#include "GPUPerfAPIStub.h"
#endif  // USE_GPA

#endif // _GPUPERFAPI_H_
