//==============================================================================
// Copyright (c) 2010-2017 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  This file is the only header that must be included by an application that
///          wishes to use GPUPerfAPI. It defines all the available entrypoints.
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

#include <assert.h>
#include "GPUPerfAPITypes.h"
#include "GPUPerfAPIFunctionTypes.h"

/// Platform specific defintions
#ifdef _WIN32
    #include <windows.h>
    typedef HMODULE LibHandle; /// typedef for HMODULE for loading the library on windows
    typedef GUID GPA_API_UUID; /// typedef for Windows GUID definition
#else
    typedef void* LibHandle; /// typedef for void* for loading the library on linux

    /// Structure for holding UUID
    typedef struct GPA_API_UUID
    {
        unsigned long data1;
        unsigned short data2;
        unsigned short data3;
        unsigned char data4[8];

#ifdef __cplusplus
        /// operator overloaded function for equality comparison
        /// \return true if UUIDs are equal otherwise false
        bool operator==(GPA_API_UUID otherUUID)
        {
            bool isEqual = true;
            isEqual &= data1 == otherUUID.data1;
            isEqual &= data2 == otherUUID.data2;
            isEqual &= data3 == otherUUID.data3;
            isEqual &= data4[0] == otherUUID.data4[0];
            isEqual &= data4[1] == otherUUID.data4[1];
            isEqual &= data4[2] == otherUUID.data4[2];
            isEqual &= data4[3] == otherUUID.data4[3];
            return isEqual;
        }
#endif
    }GPA_API_UUID;
#endif

/// UUID value for the version specific GPA API
// UUID: 2696c8b4 - fd56 - 41fc - 9742 - af3c6aa34182
// This needs to be updated if the GPA API function table changes
#define GPA_API_3_0_UUID  { 0x2696c8b4,\
                          0xfd56,\
                          0x41fc,\
                          { 0x97, 0X42, 0Xaf, 0X3c, 0X6a, 0Xa3, 0X41, 0X82 } };

/// UUID value for the current GPA API
const GPA_API_UUID GPA_API_CURRENT_UUID = GPA_API_3_0_UUID;

/// \brief Register a callback function to receive log messages.
///
/// Only one callback function can be registered, so the implementation should be able
/// to handle the different types of messages. A parameter to the callback function will
/// indicate the message type being received. Messages will not contain a newline character
/// at the end of the message.
/// \param loggingType Identifies the type of messages to receive callbacks for.
/// \param pCallbackFuncPtr Pointer to the callback function
/// \return GPA_STATUS_OK, unless the callbackFuncPtr is nullptr and the loggingType is not
/// GPA_LOGGING_NONE, in which case GPA_STATUS_ERROR_NULL_POINTER is returned.
GPALIB_DECL GPA_Status GPA_RegisterLoggingCallback(GPA_Logging_Type loggingType, GPA_LoggingCallbackPtrType pCallbackFuncPtr);

// Init / destroy GPA

/// \brief Initializes the driver so that counters are exposed.
///
/// This function must be called before the rendering context or device is created.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_Initialize();

/// \brief Undo any initialization to ensure proper behavior in applications that are not being profiled.
///
/// This function must be called after the rendering context or device is released / destroyed.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_Destroy();

// Context Startup / Finish

/// \brief Opens the counters in the specified context for reading.
///
/// This function must be called before any other GPA functions.
/// \param pContext The context to open counters for. Typically a device pointer. Refer to GPA API specific documentation for further details.
/// \param flags Flags used to initialize the context. Should be a combination of GPA_OpenContext_Bits
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_OpenContext(void* pContext, GPA_OpenContextFlags flags);

/// \brief Closes the counters in the currently active context.
///
/// GPA functions should not be called again until the counters are reopened with GPA_OpenContext.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_CloseContext();

/// \brief Select another context to be the currently active context.
///
/// The selected context must have previously been opened with a call to GPA_OpenContext.
/// If the call is successful, all GPA functions will act on the currently selected context.
/// \param pContext The context to select. The same value that was passed to GPA_OpenContext.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_SelectContext(void* pContext);


// Counter Interrogation

/// \brief Get the number of counters available.
///
/// \param pCount The value which will hold the count upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetNumCounters(gpa_uint32* pCount);

/// \brief Get the name of a specific counter.
///
/// \param index The index of the counter name to query. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \param ppName The address which will hold the name upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterName(gpa_uint32 index, const char** ppName);

/// \brief Get category of the specified counter.
///
/// \param index The index of the counter to query. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \param ppCategory The address which will hold the category string upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterCategory(gpa_uint32 index, const char** ppCategory);

/// \brief Get description of the specified counter.
///
/// \param index The index of the counter to query. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \param ppDescription The address which will hold the description upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterDescription(gpa_uint32 index, const char** ppDescription);

/// \brief Get the counter data type of the specified counter.
///
/// \param index The index of the counter. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \param pCounterDataType The value which will hold the description upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterDataType(gpa_uint32 index, GPA_Type* pCounterDataType);

/// \brief Get the counter usage type of the specified counter.
///
/// \param index The index of the counter. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \param pCounterUsageType The value which will hold the description upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterUsageType(gpa_uint32 index, GPA_Usage_Type* pCounterUsageType);

/// \brief Get a string with the name of the specified counter data type.
///
/// Typically used to display counter types along with their name.
/// E.g. counterDataType of GPA_TYPE_UINT64 would return "gpa_uint64".
/// \param counterDataType The type to get the string for.
/// \param ppTypeStr The value that will be set to contain a reference to the name of the counter data type.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetDataTypeAsStr(GPA_Type counterDataType, const char** ppTypeStr);

/// \brief Get a string with the name of the specified counter usage type.
///
/// Converts the counter usage type to a string representation.
/// E.g. counterUsageType of GPA_USAGE_TYPE_PERCENTAGE would return "percentage".
/// \param counterUsageType The type to get the string for.
/// \param ppUsageTypeStr The value that will be set to contain a reference to the name of the counter usage type.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetUsageTypeAsStr(GPA_Usage_Type counterUsageType, const char** ppUsageTypeStr);

/// \brief Enable a specified counter.
///
/// Subsequent sampling sessions will provide values for any enabled counters.
/// Initially all counters are disabled, and must explicitly be enabled by calling this function.
/// \param index The index of the counter to enable. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EnableCounter(gpa_uint32 index);


/// \brief Disable a specified counter.
///
/// Subsequent sampling sessions will not provide values for any disabled counters.
/// Initially all counters are disabled, and must explicitly be enabled.
/// \param index The index of the counter to enable. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_DisableCounter(gpa_uint32 index);


/// \brief Get the number of enabled counters.
///
/// \param pCount The value that will be set to the number of counters that are currently enabled.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetEnabledCount(gpa_uint32* pCount);


/// \brief Get the counter index for an enabled counter.
///
/// For example, if GPA_GetEnabledIndex returns 3, and I wanted the counter index of the first enabled counter,
/// I would call this function with enabledNumber equal to 0.
/// \param enabledNumber The number of the enabled counter to get the counter index for. Must lie between 0 and (GPA_GetEnabledIndex result - 1).
/// \param pEnabledCounterIndex The value that will contain the index of the counter.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetEnabledIndex(gpa_uint32 enabledNumber, gpa_uint32* pEnabledCounterIndex);


/// \brief Check that a counter is enabled.
///
/// \param counterIndex The index of the counter. Must lie between 0 and (GPA_GetNumCounters result - 1).
/// \return GPA_STATUS_OK is returned if the counter is enabled, GPA_STATUS_ERROR_NOT_FOUND otherwise.
GPALIB_DECL GPA_Status GPA_IsCounterEnabled(gpa_uint32 counterIndex);


/// \brief Enable a specified counter using the counter name (case insensitive).
///
/// Subsequent sampling sessions will provide values for any enabled counters.
/// Initially all counters are disabled, and must explicitly be enabled by calling this function.
/// \param pCounter The name of the counter to enable.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EnableCounterStr(const char* pCounter);


/// \brief Disable a specified counter using the counter name (case insensitive).
///
/// Subsequent sampling sessions will not provide values for any disabled counters.
/// Initially all counters are disabled, and must explicitly be enabled.
/// \param pCounter The name of the counter to disable.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_DisableCounterStr(const char* pCounter);


/// \brief Enable all counters.
///
/// Subsequent sampling sessions will provide values for all counters.
/// Initially all counters are disabled, and must explicitly be enabled by calling a function which enables them.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EnableAllCounters();


/// \brief Disable all counters.
///
/// Subsequent sampling sessions will not provide values for any disabled counters.
/// Initially all counters are disabled, and must explicitly be enabled.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_DisableAllCounters();


/// \brief Get index of a counter given its name (case insensitive).
///
/// \param pCounter The name of the counter to get the index for.
/// \param pIndex The index of the requested counter.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetCounterIndex(const char* pCounter, gpa_uint32* pIndex);


/// \brief Get the number of passes required for the currently enabled set of counters.
///
/// This represents the number of times the same sequence must be repeated to capture the counter data.
/// On each pass a different (compatible) set of counters will be measured.
/// \param pNumPasses The value of the number of passes.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetPassCount(gpa_uint32* pNumPasses);


/// \brief Begin sampling with the currently enabled set of counters.
///
/// This must be called to begin the counter sampling process.
/// A unique sessionID will be returned which is later used to retrieve the counter values.
/// Session Identifiers are integers and always start from 1 on a newly opened context, upwards in sequence.
/// The set of enabled counters cannot be changed inside a BeginSession/EndSession sequence.
/// \param pSessionID The value that will be set to the session identifier.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_BeginSession(gpa_uint32* pSessionID);


/// \brief End sampling with the currently enabled set of counters.
///
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EndSession();


/// \brief Begin sampling pass.
///
/// Between BeginPass and EndPass calls it is expected that a sequence of repeatable operations exist.
/// If this is not the case only counters which execute in a single pass should be activated.
/// The number of required passes can be determined by enabling a set of counters and then calling GPA_GetPassCount.
/// The operations inside the BeginPass/EndPass calls should be looped over GPA_GetPassCount result number of times.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_BeginPass();


/// \brief End sampling pass.
///
/// Between BeginPass and EndPass calls it is expected that a sequence of repeatable operations exist.
/// If this is not the case only counters which execute in a single pass should be activated.
/// The number of required passes can be determined by enabling a set of counters and then calling GPA_GetPassCount.
/// The operations inside the BeginPass/EndPass calls should be looped over GPA_GetPassCount result number of times.
/// This is necessary to capture all counter values, since sometimes counter combinations cannot be captured simultaneously.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EndPass();


///
/// \brief Begin sampling for a sample list
///
/// For use with the explicit graphics APIs (DirectX 12 and Vulkan). Each sample must be associated with a sample list.
/// For DirectX 12, a sample list is a ID3D12GraphicsCommandList.  For Vulkan, it is a VkCommandBuffer
/// \param pSampleList the sample list on which to begin sampling
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_BeginSampleList(void* pSampleList);

/// \brief End sampling for a sample list
///
/// For use with the explicit graphics APIs (DirectX 12 and Vulkan). Each sample must be associated with a sample list.
/// For DirectX 12, a sample list is a ID3D12GraphicsCommandList.  For Vulkan, it is a VkCommandBuffer
/// \param pSampleList the sample list on which to end sampling
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EndSampleList(void* pSampleList);

/// \brief Begin a sample in a sample list
///
/// For use with the explicit graphics APIs (DirectX 12 and Vulkan). Each sample must be associated with a sample list.
/// For DirectX 12, a sample list is a ID3D12GraphicsCommandList.  For Vulkan, it is a VkCommandBuffer
/// Multiple samples can be performed inside a BeginSampleList/EndSampleList sequence.
/// Each sample computes the values of the counters between GPA_BeginSampleInSampleList and GPA_EndSampleInSampleList.
/// To identify each sample the user must provide a unique sampleID as a parameter to this function.
/// The number need only be unique within the same BeginSession/EndSession sequence.
/// GPA_BeginSampleInSampleList must be followed by a call to GPA_EndSampleInSampleList before GPA_BeginSampleInSampleList is called again.
/// \param sampleID Any integer, unique within the BeginSession/EndSession sequence, used to retrieve the sample results.
/// \param pSampleList the sample list on which to begin a sample
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_BeginSampleInSampleList(gpa_uint32 sampleID, void* pSampleList);

/// \brief End a sample in a sample list.
///
/// For use with the explicit graphics APIs (DirectX 12 and Vulkan). Each sample must be associated with a sample list.
/// For DirectX 12, a sample list is a ID3D12GraphicsCommandList.  For Vulkan, it is a VkCommandBuffer
/// GPA_BeginSampleInSampleList must be followed by a call to GPA_EndSampleInSampleList before GPA_BeginSampleInSampleList is called again.
/// \param pSampleList the sample list on which to end a sample
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EndSampleInSampleList(void* pSampleList);

/// \brief Begin a sample using the enabled counters.
///
/// Multiple samples can be performed inside a BeginSession/EndSession sequence.
/// Each sample computes the values of the counters between BeginSample and EndSample.
/// To identify each sample the user must provide a unique sampleID as a parameter to this function.
/// The number need only be unique within the same BeginSession/EndSession sequence.
/// Not supported when using DirectX 12 or Vulkan
/// BeginSample must be followed by a call to EndSample before BeginSample is called again.
/// \param sampleID Any integer, unique within the BeginSession/EndSession sequence, used to retrieve the sample results.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_BeginSample(gpa_uint32 sampleID);


/// \brief End sampling using the enabled counters.
///
/// BeginSample must be followed by a call to EndSample before BeginSample is called again.
/// Not supported when using DirectX 12 or Vulkan
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_EndSample();


/// \brief Get the number of samples a specified session contains.
///
/// This is useful if samples are conditionally created and a count is not kept.
/// \param sessionID The session to get the number of samples for.
/// \param pSampleCount The value that will be set to the number of samples contained within the session.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetSampleCount(gpa_uint32 sessionID, gpa_uint32* pSampleCount);


/// \brief Determine if an individual sample result is available.
///
/// After a sampling session results may be available immediately or take a certain amount of time to become available.
/// This function allows you to determine when a sample can be read.
/// The function does not block, permitting periodic polling.
/// To block until a sample is ready use a GetSample* function instead of this.
/// It can be more efficient to determine if a whole session's worth of data is available using GPA_IsSessionReady.
/// \param pReadyResult The value that will contain the result of the sample being ready. Non-zero if ready.
/// \param sessionID The session containing the sample to determine availability.
/// \param sampleID The sample identifier of the sample to query availability for.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_IsSampleReady(gpa_uint8* pReadyResult, gpa_uint32 sessionID, gpa_uint32 sampleID);


/// \brief Determine if all samples within a session are available.
///
/// After a sampling session results may be available immediately or take a certain amount of time to become available.
/// This function allows you to determine when the results of a session can be read.
/// The function does not block, permitting periodic polling.
/// To block until a sample is ready use a GetSample* function instead of this.
/// \param pReadyResult The value that will contain the result of the session being ready. Non-Zero if ready.
/// \param sessionID The session to determine availability for.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_IsSessionReady(gpa_uint8* pReadyResult, gpa_uint32 sessionID);


/// \brief Get a sample of type 64-bit unsigned integer.
///
/// This function will block until the value is available.
/// Use GPA_IsSampleReady if you do not wish to block.
/// \param sessionID The session identifier with the sample you wish to retrieve the result of.
/// \param sampleID The identifier of the sample to get the result for.
/// \param counterID The counter index to get the result for.
/// \param pResult The value which will contain the counter result upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetSampleUInt64(gpa_uint32 sessionID, gpa_uint32 sampleID, gpa_uint32 counterID, gpa_uint64* pResult);


/// \brief Get a sample of type 32-bit unsigned integer.
///
/// This function will block until the value is available.
/// Use GPA_IsSampleReady if you do not wish to block.
/// \param sessionID The session identifier with the sample you wish to retrieve the result of.
/// \param sampleID The identifier of the sample to get the result for.
/// \param counterIndex The counter index to get the result for.
/// \param pResult The value which will contain the counter result upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetSampleUInt32(gpa_uint32 sessionID, gpa_uint32 sampleID, gpa_uint32 counterIndex, gpa_uint32* pResult);


/// \brief Get a sample of type 64-bit float.
///
/// This function will block until the value is available.
/// Use GPA_IsSampleReady if you do not wish to block.
/// \param sessionID The session identifier with the sample you wish to retrieve the result of.
/// \param sampleID The identifier of the sample to get the result for.
/// \param counterIndex The counter index to get the result for.
/// \param pResult The value which will contain the counter result upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetSampleFloat64(gpa_uint32 sessionID, gpa_uint32 sampleID, gpa_uint32 counterIndex, gpa_float64* pResult);


/// \brief Get a sample of type 32-bit float.
///
/// This function will block until the value is available.
/// Use GPA_IsSampleReady if you do not wish to block.
/// \param sessionID The session identifier with the sample you wish to retrieve the result of.
/// \param sampleID The identifier of the sample to get the result for.
/// \param counterIndex The counter index to get the result for.
/// \param pResult The value which will contain the counter result upon successful execution.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetSampleFloat32(gpa_uint32 sessionID, gpa_uint32 sampleID, gpa_uint32 counterIndex, gpa_float32* pResult);


/// \brief Get a string translation of a GPA status value.
///
/// Provides a simple method to convert a status enum value into a string which can be used to display log messages.
/// \param status The status to convert into a string.
/// \return A string which describes the supplied status.
GPALIB_DECL const char* GPA_GetStatusAsStr(GPA_Status status);

/// \brief Get the GPU device id associated with the current context
///
/// \param pDeviceID The value that will be set to the device id.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetDeviceID(gpa_uint32* pDeviceID);

/// \brief Get the GPU device description associated with the current context
///
/// \param ppDesc The value that will be set to the device description.
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetDeviceDesc(const char** ppDesc);

/// \brief Internal function. Pass draw call counts to GPA for internal purposes.
///
/// \param iCounts[in] the draw counts for the current frame
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_InternalSetDrawCallCounts(const int iCounts);


/// \brief Get the GPA Api function table
///
/// \param ppGPAFuncTable[out] pointer to pointer of GPAApi - GPA Function table structure
/// \return The GPA result status of the operation. GPA_STATUS_OK is returned if the operation is successful.
GPALIB_DECL GPA_Status GPA_GetFuncTable(void** ppGPAFuncTable);

/// Structure to hold the function table of the exported GPA APIs
typedef struct _GPAApi
{
    GPA_API_UUID m_apiId;

#define GPA_FUNCTION_PREFIX(func) func##PtrType func;
    #include "GPAFunctions.h"
#undef GPA_FUNCTION_PREFIX

#ifdef __cplusplus
    _GPAApi()
    {
        m_apiId = GPA_API_CURRENT_UUID;
#define GPA_FUNCTION_PREFIX(func) func = nullptr;
    #include "GPAFunctions.h"
#undef GPA_FUNCTION_PREFIX
    }
#endif

} GPAApi;


#endif // _GPUPERFAPI_H_
