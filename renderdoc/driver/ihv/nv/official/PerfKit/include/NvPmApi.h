/*
 * Copyright 2014  NVIDIA Corporation.  All rights reserved.
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

///////////////////////////////////////////////////////////////////////////////
/// \file
/// NVPMAPI-Next Header files
///////////////////////////////////////////////////////////////////////////////

#ifndef _NVPMAPI_H_
#define _NVPMAPI_H_

#ifdef __cplusplus
extern "C" {
#endif // End of __cplusplus

#include <stdint.h>

/// Generic unsigned data types, 8-64 bits
typedef uint8_t NVPMUINT8;
typedef uint16_t NVPMUINT16;
typedef uint32_t NVPMUINT32;
typedef uint32_t NVPMUINT;
typedef uint64_t NVPMUINT64;
typedef double NVPMFLOAT64;

/// Context from NVPMAPI mapping back to the original API specific device/context
typedef NVPMUINT64 NVPMContext;

/// Abstract handle type for GL/CUDA, here to keep includes to a minimum
typedef NVPMUINT64 APIContextHandle;

/// Every counter has a unique ID
typedef NVPMUINT NVPMCounterID;

///////////////////////////////////////////////////////////////////////////////
/// Unified return code for all NVPMAPI-Next methods
/// The negative result values are thrown on init or if init failed
///////////////////////////////////////////////////////////////////////////////
#ifndef NVPMRESULT_DEFINED
typedef enum {
    /// Performance disabled in registry
    NVPM_FAILURE_DISABLED = -5,
    /// Mixed mode (32bit client 64bit kernel) unsupported
    NVPM_FAILURE_32BIT_ON_64BIT = -4,
    /// Returned when NVPMInit has not been called or failed
    NVPM_NO_IMPLEMENTATION = -3,
    /// nvpmapi.dll was not found
    NVPM_LIBRARY_NOT_FOUND = -2,
    /// General, internal failure when initializing
    NVPM_FAILURE = -1,
    /// Finished successfully
    NVPM_OK = 0,
    /// Invalid parameter found
    NVPM_ERROR_INVALID_PARAMETER,
    /// Driver version mismatch ?
    NVPM_ERROR_DRIVER_MISMATCH,
    /// Not initialized when trying to use
    NVPM_ERROR_NOT_INITIALIZED,
    /// Already initialized when trying to initialize
    NVPM_ERROR_ALREADY_INITIALIZED,
    /// Bad enumerator found
    NVPM_ERROR_BAD_ENUMERATOR,
    /// String is too small
    NVPM_ERROR_STRING_TOO_SMALL,
    /// Invalid counter found
    NVPM_ERROR_INVALID_COUNTER,
    /// No more memory to be allocated
    NVPM_ERROR_OUT_OF_MEMORY,
    ///
    NVPM_ERROR_EXPERIMENT_INCOMPLETE,
    ///
    NVPM_ERROR_INVALID_PASS,
    ///
    NVPM_ERROR_INVALID_OBJECT,
    ///
    NVPM_ERROR_COUNTER_NOT_ENABLED,
    ///
    NVPM_ERROR_COUNTER_NOT_FOUND,
    ///
    NVPM_ERROR_EXPERIMENT_NOT_RUN,
    ///
    NVPM_ERROR_32BIT_ON_64BIT,
    ///
    NVPM_ERROR_STATE_MACHINE,
    ///
    NVPM_ERROR_INTERNAL,
    ///
    NVPM_WARNING_ENDED_EARLY,
    ///
    NVPM_ERROR_TIME_OUT,
    ///
    NVPM_WARNING_DUPLICATE,
    ///
    NVPM_ERROR_COUNTERS_ENABLED,
    ///
    NVPM_ERROR_CONTEXT_NOT_SUPPORTED,
    ///
    NVPM_ERROR_INVALID_CONTEXT,
    ///
    NVPM_ERROR_GPU_UNSUPPORTED,
    ///
    NVPM_INCORRECT_VALUE_TYPE,
    NVPM_ERROR_MAX
} NVPMRESULT;
#define NVPMRESULT_DEFINED
#endif

#if defined(_WIN32) // Windows
#define NVCALL __stdcall
#else // Linux / Mac
#define NVCALL
#endif // End of _WIN32

///////////////////////////////////////////////////////////////////////////////
///  NVPMRESULT NVPMSetWarningLevel(NVPMUINT unLevel);
///
/// @brief Set warning output level to be set
/// @param[in] unLevel debug output levels
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMSetWarningLevel_Pfn)(NVPMUINT unLevel);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMGetExtendedError(NVPMUINT *pnError);
///
/// @brief Get extended error code
/// @param[out] pnError error code returned here
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMGetExtendedError_Pfn)(NVPMUINT *pnError);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMInit();
///
/// @brief Initialize NVPMAPI-Next
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMInit_Pfn)();

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMShutdown();
///
/// @brief Shutdown NVPMAPI-Next
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMShutdown_Pfn)();

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMCreateContextFromOGLContext(APIContextHandle hglrc, NVPMContext *perfCtx);
///
/// @brief Create NVPMContext from OpenGL context
/// @param[in] hglrc OpenGL context handle
/// @param[out] perfCtx pointer to the result NVPMContext
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMCreateContextFromOGLContext_Pfn)(
    APIContextHandle hglrc,
    NVPMContext *perfCtx
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMCreateContextFromCudaContext(APIContextHandle cuCtx, NVPMContext *perfCtx);
///
/// @brief Create NVPMContext from CUDA context
/// @param[in] cuCtx CUDA context handle
/// @param[out] perfCtx pointer to the result NVPMContext
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMCreateContextFromCudaContext_Pfn)(
    APIContextHandle cuCtx,
    NVPMContext *perfCtx
);

#if defined(_WIN32)
typedef struct IDirect3DDevice9 IDirect3DDevice9;
typedef struct ID3D10Device ID3D10Device;
typedef struct ID3D11Device ID3D11Device;
typedef struct ID3D11Device1 ID3D11Device1;

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMCreateContextFromD3D9Device(IDirect3DDevice9 *pD3DDevice, NVPMContext *perfCtx);
///
/// @brief Create NVPMContext from Direct3D9 device
/// @param[in] pD3DDevice Direct3D9 device handle
/// @param[out] perfCtx pointer to the result NVPMContext
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMCreateContextFromD3D9Device_Pfn)(
    IDirect3DDevice9 *pD3D9Device,
    NVPMContext *perfCtx
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMCreateContextFromD3D10Device(ID3D10Device *pD3DDevice, NVPMContext *perfCtx);
///
/// @brief Create NVPMContext from Direct3D10 device
/// @param[in] pD3DDevice Direct3D10 device handle
/// @param[out] perfCtx pointer to the result NVPMContext
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMCreateContextFromD3D10Device_Pfn)(
    ID3D10Device *pD3DDevice,
    NVPMContext *perfCtx
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMCreateContextFromD3D11Device(ID3D11Device *pD3DDevice, NVPMContext *perfCtx);
///
/// @brief Create NVPMContext from Direct3D11 device
/// @param[in] pD3DDevice Direct3D11 device handle
/// @param[out] perfCtx pointer to the result NVPMContext
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMCreateContextFromD3D11Device_Pfn)(
    ID3D11Device *pD3DDevice,
    NVPMContext *perfCtx
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMCreateContextFromD3D11Device1(ID3D11Device1 *pD3DDevice, NVPMContext *perfCtx);
///
/// @brief Create NVPMContext from Direct3D11_1 device
/// @param[in] pD3DDevice Direct3D11_1 device handle
/// @param[out] perfCtx pointer to the result NVPMContext
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMCreateContextFromD3D11Device1_Pfn)(
    ID3D11Device1 *pD3DDevice,
    NVPMContext *perfCtx
);
#endif // _WIN32

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMDestroyContext(NVPMContext perfCtx);
///
/// @brief Destroy existing NVPMContext
/// @param[in] perfCtx NVPMContext instance to be destroyed
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMDestroyContext_Pfn)(NVPMContext perfCtx);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMEnumCounters(NVPMCounterID unCounterID, const char *pcCounterName);
///
/// @brief Callback function for enumeration of counters/experiments.
/// @param[in] unCounterID Available counter's ID.
/// @param[in] pcCounterName Available counter's name.
/// @return NVPM_OK to continue enumerating available counters.
///////////////////////////////////////////////////////////////////////////////
typedef int (*NVPMEnumFunc)(NVPMCounterID unCounterID, const char *pcCounterName);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMEnumCountersByContext(NVPMContext perfCtx, NVPMEnumFunc pEnumFunction);
///
/// @brief Enumerate counters/experiments.
/// @param [in] perfCtx The perfCtx to enum counters from
/// @param [in] pEnumFunction function pointer to enum each available counter.
///     Prototype of callback function as #NVPMEnumFunc
/// @return unified return code #NVPMRESULT
/// @see NVPMEnumFunc
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMEnumCountersByContext_Pfn)(NVPMContext perfCtx, NVPMEnumFunc pEnumFunction);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMEnumCountersUserData(NVPMCounterID unCounterID, const char *pcCounterName, void *pUserData);
///
/// @brief Callback function for enumeration of counters/experiments supporting user data.
/// @param[in] unCounterID Available counter's ID.
/// @param[in] pcCounterName Available counter's name.
/// @param[in] pUserData Pointer to user specified data passed into #NVPMEnumCountersByContextUserData.
/// @return NVPM_OK to continue enumerating available counters.
///////////////////////////////////////////////////////////////////////////////
typedef int (*NVPMEnumFuncUserData)(NVPMCounterID unCounterID, const char *pcCounterName, void *pUserData);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMEnumCountersByContextUserData(NVPMContext perfCtx, NVPMEnumFunc pEnumFunction, void *pUserData);
///
/// @brief Enumerate counters/experiments.
/// @param [in] perfCtx The perfCtx to enum counters from
/// @param [in] pEnumFunction function pointer to enum each available counter.
///     Prototype of callback function as #NVPMEnumFuncUserData
/// @param [in] pUserData pointer to user data passed to each call to #NVPMEnumFuncUserData.
/// @return unified return code #NVPMRESULT
/// @see NVPMEnumFunc
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMEnumCountersByContextUserData_Pfn)(NVPMContext perfCtx, NVPMEnumFuncUserData pEnumFunction, void *pUserData);

///////////////////////////////////////////////////////////////////////////////
/// @brief Get the name of a counter specified by ID
/// @param[in] unCounterID ID to the counter which is interested
/// @param[out] pcString returned name string
/// @param[in,out] punLen length of return string ??
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMGetCounterName_Pfn)(
    NVPMCounterID unCounterID,
    char *pcString,
    NVPMUINT *punLen
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMGetCounterDescription(NVPMCounterID unCounterID, char *pcString, NVPMUINT *punLen);
///
/// @brief Get the description of a counter specified by ID
/// @param[in] unCounterID ID to the counter which is interested
/// @param[out] pcString returned description string
/// @param[in,out] punLen length of return string ??
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMGetCounterDescription_Pfn)(
    NVPMCounterID unCounterID,
    char *pcString,
    NVPMUINT *punLen
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMAPI_INTERFACE NVPMGetCounterIDByContext(NVPMContext perfCtx, const char *pcString, NVPMCounterID *punCounterID);
///
/// @brief Get the ID of a counter specified by name for a given context
/// @param [in] perfCtx The perfCtx to get counters number from
/// @param[in] pcString name of the counter which is interested
/// @param[out] punCounterID returned ID to the counter which is
///     interested
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMGetCounterIDByContext_Pfn)(
    NVPMContext perfCtx,
    const char *pcString,
    NVPMCounterID *punCounterID
    );

///////////////////////////////////////////////////////////////////////////////
/// NVPMAPI_INTERFACE NVPMGetCounterClockRateByContext(NVPMContext perfCtx, const char *pcString, float *pfValue);
///
/// @brief Get the clock rate of a counter specified by name
/// @param [in] perfCtx The perfCtx to get counters number from
/// @param[in] pcString name of the counter which is interested
/// @param[out] pfValue returned clock rate in MHz
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMGetCounterClockRateByContext_Pfn)(
    NVPMContext perfCtx,
    const char *pcString,
    float *pfValue
);

///////////////////////////////////////////////////////////////////////////////
// Query attribute information for a given counter.  These can be called
// when enumerating with NVPMEnumCounters();
///////////////////////////////////////////////////////////////////////////////
/// Counter's type
typedef enum {
    /// GPU counter
    NVPM_CT_GPU,
    /// OpenGL counter
    NVPM_CT_OGL,
    /// Direct3D counter
    NVPM_CT_D3D,
    /// Simplified experiment counter (can only be used in Experiment mode)
    NVPM_CT_SIMEXP,
    /// User counter type
    NVPM_CT_USER,
    /// Aggregated experiment counter
    NVPM_CT_AGGREGATE,
} NVPMCOUNTERTYPE;

/// Counter display type
typedef enum {
    /// Counter should be displayed as a ratio of value/cycles
    NVPM_CD_RATIO,
    /// Counter should be displayed as the value only
    NVPM_CD_RAW
} NVPMCOUNTERDISPLAY;

/// Counter value type.
typedef enum {
    // 64b unsigned integer
    NVPM_VALUE_TYPE_UINT64,
    // 64b float (double)
    NVPM_VALUE_TYPE_FLOAT64,
} NVPMCOUNTERVALUETYPE;

/// Attribute type used in function NVPMGetCounterAttribute
typedef enum {
    /// The type of counter, see NVPMCOUNTERTYPE
    NVPMA_COUNTER_TYPE,
    /// The display hint for the counter, see NVPMCOUNTERDISPLAY
    NVPMA_COUNTER_DISPLAY,
    /// The domain of counter
    NVPMA_COUNTER_DOMAIN,
    /// The value type of the counter
    NVPMA_COUNTER_VALUE_TYPE,
    /// Get the maximum of counter value.
    NVPMA_COUNTER_MAX,  // Return the maximum counter value
} NVPMATTRIBUTE;
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMGetCounterAttribute(NVPMCounterID unCounterID, NVPMATTRIBUTE nvAttribute, NVPMUINT64 *punValue);
///
/// @brief Get the some attribute of a counter specified by ID
/// @param[in] unCounterID ID to the counter which is interested
/// @param[out] nvAttribute which attribute of that counter is interested, see
///     #NVPMATTRIBUTE for detail information.
/// @param[out] punValue attribute result
/// @return unified return code #NVPMRESULT
/// @see NVPMATTRIBUTE
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMGetCounterAttribute_Pfn)(
    NVPMCounterID unCounterID,
    NVPMATTRIBUTE nvAttribute,
    NVPMUINT64 *punValue
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMAddCounterByName(NVPMContext perfCtx, const char *pcName);
///
/// @brief Activate counter specified by name
/// @param[in] perfCtx In which NVPMContext instance we want to activate the
///     counter
/// @param[in] pcName pointer to a string of the counter name
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMAddCounterByName_Pfn)(
    NVPMContext perfCtx,
    const char *pcName
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMAddCounter(NVPMContext perfCtx, NVPMCounterID unCounterID);
///
/// @brief Activate counter specified by ID
/// @param[in] perfCtx In which NVPMContext instance we want to activate the
///     counter
/// @param[in] unCounterID counter ID
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMAddCounter_Pfn)(NVPMContext perfCtx, NVPMCounterID unCounterID);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMAddCounters(NVPMContext perfCtx, NVPMUINT unCount, NVPMCounterID *punCounterIDs);
///
/// @brief Activate multiple counters at a time specified by an ID array
/// @param[in] perfCtx In which NVPMContext instance we want to activate the
///     counter
/// @param[in] unCount size of counter ID array
/// @param[in] punCounterIDs pointer to the counter ID array
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMAddCounters_Pfn)(
    NVPMContext perfCtx,
    NVPMUINT unCount,
    NVPMCounterID *punCounterIDs
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMRemoveCounterByName(NVPMContext perfCtx, const char *pcName);
///
/// @brief Deactivate counter specified by name
/// @param[in] perfCtx In which NVPMContext instance we want to deactivate the
/// performance counter
/// @param[in] pcName pointer to a string of the counter name
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMRemoveCounterByName_Pfn)(
    NVPMContext perfCtx,
    const char *pcName
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMRemoveCounter(NVPMContext perfCtx, NVPMCounterID unCounterID);
///
/// @brief Deactivate counter specified by ID
/// @param[in] perfCtx In which NVPMContext instance we want to deactivate the
/// performance counter
/// @param[in] unCounterID counter ID
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMRemoveCounter_Pfn)(
    NVPMContext perfCtx,
    NVPMCounterID unCounterID
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMRemoveCounters(NVPMContext perfCtx, NVPMUINT unCount, NVPMCounterID *punCounterIDs);
///
/// @brief Deactivate multiple counters at a time specified by an ID array
/// @param[in] perfCtx In which NVPMContext instance we want to deactivate the
/// performance counter
/// @param[in] unCount size of counter ID array
/// @param[in] punCounterIDs pointer to the counter ID array
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMRemoveCounters_Pfn)(
    NVPMContext perfCtx,
    NVPMUINT unCount,
    NVPMCounterID *punCounterIDs
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMRemoveAllCounters(NVPMContext perfCtx);
///
/// @brief Deactivate all counters in the specified NVPMContext instance
/// @param[in] perfCtx In which NVPMContext instance we want to deactivate all
/// the performance counters
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMRemoveAllCounters_Pfn)(NVPMContext perfCtx);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMReserveObjects(NVPMContext perfCtx, NVPMUINT objNum);
///
/// @brief Reserve certain amount of NVPMPerfObjects
/// @param[in] perfCtx Specify which NVPMContext instance to operate
/// @param[in] objNum number of PerfObjects to be reserved
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMReserveObjects_Pfn)(
    NVPMContext perfCtx,
    NVPMUINT objNum
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMDeleteObjects(NVPMContext perfCtx);
///
/// @brief Delete all NVPMPerfObjects in a given NVPMPerfContext
/// @param[in] perfCtx Specify which NVPMContext instance to operate
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMDeleteObjects_Pfn)(NVPMContext perfCtx);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMBeginExperiment(NVPMContext perfCtx, NVPMUINT *pnNumPasses);
///
/// @brief Begin experiment
/// @param[in] perfCtx Specify which NVPMContext instance to operate
/// @param[out] pnNumPasses return how many passes needed to do this experiment
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMBeginExperiment_Pfn)(
    NVPMContext perfCtx,
    NVPMUINT *pnNumPasses
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMEndExperiment(NVPMContext perfCtx);
///
/// @brief Ending an experiment
/// @param[in] perfCtx Specify which NVPMContext instance to operate
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMEndExperiment_Pfn)(NVPMContext perfCtx);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMBeginPass(NVPMContext perfCtx, NVPMUINT nPass);
///
/// @brief Beginning a pass
/// @param[in] perfCtx Specify which NVPMContext instance to operate
/// @param[in] nPass specify which pass it's going to be run
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMBeginPass_Pfn)(
    NVPMContext perfCtx,
    NVPMUINT nPass
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMEndPass(NVPMContext perfCtx, NVPMUINT nPass);
///
/// @brief Ending a pass
/// @param[in] perfCtx Specify which NVPMContext instance to operate
/// @param[in] nPass specify which pass to be ended
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMEndPass_Pfn)(
    NVPMContext perfCtx,
    NVPMUINT nPass
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMBeginObject(NVPMContext perfCtx, NVPMUINT nObjectID);
///
/// @brief Beginning a NVPMPerfObject, make that NVPMPerfOject active
/// @param[in] perfCtx Specify which NVPMContext instance to operate
/// @param[in] nObjectID ID of the NVPMPerfObject to be used
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMBeginObject_Pfn)(
    NVPMContext perfCtx,
    NVPMUINT nObjectID
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMEndObject(NVPMContext perfCtx, NVPMUINT nObjectID);
///
/// @brief Ending of a NVPMPerfObject, make that NVPMPerfObject inactive
/// @param[in] perfCtx Specify which NVPMContext instance to operate
/// @param[in] nObjectID ID of the NVPMPerfObject to be used
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMEndObject_Pfn)(
    NVPMContext perfCtx,
    NVPMUINT nObjectID
);
///////////////////////////////////////////////////////////////////////////////
// Sample Methods
// This is the typical "sample now" based interface.  If you pass in an array
// of SampleValue's, it will return the currently active counters (NULL returns
// no counters).  Fill *punCount with the available entries in pucValues/pucCycles
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// @brief The NVPMSampleValue structure contains the counter ID, value and
///     cycle.
/// @remarks The NVPMSampleValue structure is mainly used in function
///     #NVPMSample to get active counter information.
/// @remarks If the value of counter is RAW type(Integer), it is equal to
///     ulValue.
/// @remarks If the value of counter is PERCENT type(Float), it is equal to
///     ulValue/ulCycles.
/// @see NVPMSample, NVPMSampleValueEx
///////////////////////////////////////////////////////////////////////////////
typedef struct _NVPMSampleValue {
    /// ID of counter
    NVPMCounterID unCounterID;
    /// Value of counter
    NVPMUINT64 ulValue;
    /// Cycles of counter
    NVPMUINT64 ulCycles;
} NVPMSampleValue;

///////////////////////////////////////////////////////////////////////////////
/// @brief The NVPMSampleValueEx structure contains the counter ID, value,
///     cycle and updated flag.
/// @remarks The NVPMSampleValueEx structure is mainly used in function
///     #NVPMSampleEx to get active counter information. It is the extension of
///     structure #NVPMSampleValue.
/// @remarks If the value of counter is RAW type(Integer), it is equal to
///     ulValue.
/// @remarks If the value of counter is RATIO type(Float), it is equal to
///     ulValue/ulCycles.
/// @remarks The member unCounterValueUpdated is only internal used, just set
///     it to 0.
/// @see NVPMSampleEx, NVPMSampleValue
///////////////////////////////////////////////////////////////////////////////
typedef struct _NVPMSampleValueEx {
    /// Version of struct
    NVPMUINT32 ulVersion;
    /// ID of counter
    NVPMCounterID unCounterID;
    /// Value of counter
    union {
        NVPMUINT64  ulValue;
        NVPMFLOAT64 dValue;
    };
    /// Cycles of counter
    NVPMUINT64 ulCycles;
    /// Various flags
    NVPMUINT64 ulFlags;
} NVPMSampleValueEx;

#define NVPMSAMPLEEX_FLAG_OVERFLOW          0x0000000000000001      /// Flag used to indicate if the counter value has overflowed.
#define NVPMSAMPLEEX_FLAG_UPDATED           0x0000000000000002      /// Flag used to check the counter value updated.

#define NVPMSAMPLEEX_FLAG_VALUE_TYPE_MASK   0x000000000000FF00
#define NVPMSAMPLEEX_FLAG_VALUE_TYPE_SHIFT  8
#define NVPMSAMPLEEX_FLAG_VALUE_TYPE(flag)  ((NVPMCOUNTERVALUETYPE) (((flag) & NVPMSAMPLEEX_FLAG_VALUE_TYPE_MASK) >> NVPMSAMPLEEX_FLAG_VALUE_TYPE_SHIFT))

#define MAKE_NVPMSAMPLEVALUEEX_VERSION(VERSION, STRUCT_SIZE) (NVPMUINT32)((VERSION << 16) | STRUCT_SIZE)

/// nvpm sample value ex version major */
#define NVPMSAMPLEVALUEEX_VER_1 1
#define NVPMSAMPLEVALUEEX_VER_2 2
#define NVPMSAMPLEVALUEEX_VER NVPMSAMPLEVALUEEX_VER_2               // The latest version is VER_2. VER_1 is still supported and
                                                                    // the old NVPMSampleValueEx is compatible with the new one.
#define NVPMSAMPLEVALUEEX_VERSION()                         MAKE_NVPMSAMPLEVALUEEX_VERSION( NVPMSAMPLEVALUEEX_VER, sizeof(NVPMSampleValueEx) )
#define NVPMSAMPLEVALUEEX_VERSION_GET_STRUCT_SIZE(X)        ( (X) & 0xFFFF )
#define NVPMSAMPLEVALUEEX_VERSION_GET_VERSION(X)            ( (X) >> 16 )

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMSample(NVPMContext perfCtx, NVPMSampleValue *pSamples, NVPMUINT *punCount);
///
/// @brief Sample active counters for a specified NVPMContext and output active
/// counter information
/// @param[in] perfCtx Specify which NVPMContext instance to operate
/// @param[out] pSamples The buffer which get the updated counter information
///     from the core.
/// @param[in,out] punCount Input as the item count of pSamples, and output as
///     the number of counter information which is actually saved to pSample.
/// @return If succeed, return NVPM_OK, else unified return code #NVPMRESULT.
/// @remarks When pSamples and punCount are both NULL, will not update the
///     active counter data.
/// @remarks When pSamples is NULL and punCount is not NULL, will update the
///     active counter data, but no counter data will be output and *punCount
///     will set to 0.
/// @remarks When pSamples is not NULL and punCount is NULL, will update the
///     active counter data, but no counter data will be output to pSamples.
/// @remarks When neither pSamples nor punCount is NULL, the following actions
///     will be taken in order:
///     *punCount = min(*punCount,number of active counters);
///     Update *punCount active counters' data into pSamples.
/// @remarks You can also get active counter value by function
///     #NVPMGetCounterValueByName
/// @see NVPMGetCounterValueByName, NVPMSampleValue, NVPMSampleValueEx,
///     NVPMSampleEx
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMSample_Pfn)(
    NVPMContext perfCtx,
    NVPMSampleValue *pSamples,
    NVPMUINT *punCount
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMSampleEx(NVPMContext perfCtx, NVPMSampleValueEx *pSamples, NVPMUINT *punCount, NVPMUINT unNVPMSampleValueExVersion);
///
/// @brief Extended version of NVPMSample
/// @param[in] perfCtx Specify which NVPMContext instance to operate
/// @param[out] pSamples The buffer which get the updated counter information
///     from the core.
/// @param[in,out] punCount Input as the item count of pSamples, and output as
///     the number of counter information which is actually saved to pSample.
/// @param[in] unNVPMSampleValueExVersion version of NVPMSampleValueEx
/// @return If succeed, return NVPM_OK, else unified return code #NVPMRESULT.
/// @remarks When pSamples and punCount are both NULL, will not update the
///     active counter data.
/// @remarks When pSamples is NULL and punCount is NULL, update the active
///     counter data.
/// @remarks When pSamples is NULL and punCount is not NULL, will update the
///     active counter data, but no counter data will be output and *punCount
///     will set to 0.
/// @remarks When pSamples is not NULL and punCount is NULL, will update the
///     active counter data, but no counter data will be output to pSamples.
/// @remarks When neither pSamples nor punCount is NULL, the following actions
///     will be taken in order:
///     *punCount = min(*punCount,number of active counters);
///     Update *punCount active counters' data into pSamples.
/// @remarks You can also get active counter value by function
///     #NVPMGetCounterValueByName
/// @see NVPMGetCounterValueByName, NVPMSample, NVPMSampleValue,
///     NVPMSampleValueEx,
///
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMSampleEx_Pfn)(
    NVPMContext perfCtx,
    NVPMSampleValueEx *pSamples,
    NVPMUINT *punCount
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMGetCounterValueByName(NVPMContext perfCtx, const char *pcName, NVPMUINT nObjectID, NVPMUINT64 *pulValue, NVPMUINT64 *pulCycles);
///
/// @brief Get value of a counter specified by name in a give NVPMPerfObject of
/// a given NVPMPerfContext
/// @param[in] perfCtx Specify which NVPMContext instance to operate
/// @param[in] pcName name of the target counter
/// @param[in] nObjectID ID of the NVPMPerfObject
/// @param[out] pulValue returned value of that counter
/// @param[out] pulCycles returned cycles number of that counter
/// @param[out] pOverflow returned overflow flags, if it's nozero, the counter has overflowed. Otherwise it hasn't overflowed.
/// @return unified return code #NVPMRESULT. If the counter's value type cannot be returned by the called function, NVPM_INCORRECT_VALUE_TYPE will be returned.
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMGetCounterValueByName_Pfn)(
    NVPMContext perfCtx,
    const char *pcName,
    NVPMUINT nObjectID,
    NVPMUINT64 *pulValue,
    NVPMUINT64 *pulCycles
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMGetCounterValue{Uint64,Float64}(NVPMContext perfCtx, NVPMCounterID unCounterID, NVPMUINT nObjectID, NVPM{UINT,FLOAT}64 *pulValue, NVPMUINT64 *pulCycles);
///
/// @brief Get value of a counter specified by ID in a give NVPMPerfObject
/// of a given NVPMPerfContext
/// @param[in] perfCtx Specify which NVPMContext instance to operate
/// @param[in] unCounterID ID of the target counter
/// @param[in] nObjectID ID of given NVPMPerfObject
/// @param[out] p{ul,d}Value returned value of that counter
/// @param[out] pulCycles returned cycles number of that counter
/// @param[out] pOverflow returned overflow flags, if it's nozero, the counter has overflowed. Otherwise it hasn't overflowed.
/// @return unified return code #NVPMRESULT. If the counter's value type cannot be returned by the called function, NVPM_INCORRECT_VALUE_TYPE will be returned.
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMGetCounterValue_Pfn)(
    NVPMContext perfCtx,
    NVPMCounterID unCounterID,
    NVPMUINT nObjectID,
    NVPMUINT64 *pulValue,
    NVPMUINT64 *pulCycles
);

typedef NVPMRESULT (NVCALL *NVPMGetCounterValueUint64_Pfn)(
    NVPMContext perfCtx,
    NVPMCounterID unCounterID,
    NVPMUINT nObjectID,
    NVPMUINT64 *pulValue,
    NVPMUINT64 *pulCycles,
    NVPMUINT8 *pOverflow
);

typedef NVPMRESULT (NVCALL *NVPMGetCounterValueFloat64_Pfn)(
    NVPMContext perfCtx,
    NVPMCounterID unCounterID,
    NVPMUINT nObjectID,
    NVPMFLOAT64 *pdValue,
    NVPMUINT64 *pulCycles,
    NVPMUINT8 *pOverflow
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMGetGPUBottleneckName(NVPMContext perfCtx, NVPMUINT64 ulValue, char *pcName);
///
/// @brief convert bottleneck pipeline stage from ID to meaningful name string
/// @param[in] perfCtx Specify which NVPMContext instance to operate
/// @param[in] ulValue pipeline stage id
/// @param[out] pcName returned name string of the given pipeline stage
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMGetGPUBottleneckName_Pfn)(
    NVPMContext perfCtx,
    NVPMUINT64 ulValue,
    char *pcName
);

///////////////////////////////////////////////////////////////////////////////
/// NVPMRESULT NVPMRegisterNewDataProviderCallback(FuncPtrNewDataProvider fpNewDP);
///
/// @brief register a callback function to be called when new data provider is
/// registered to the NVPMAPI module
/// @param[in] fpNewDP function pointer, return NVPMUINT64 and void parameter
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMUINT64 (*FuncPtrNewDataProvider)(void);

typedef NVPMRESULT (NVCALL *NVPMRegisterNewDataProviderCallback_Pfn)(
    FuncPtrNewDataProvider fpNewDP
);

///////////////////////////////////////////////////////////////////////////////
// This section defines the external/exported interface for NVPMAPI-Next via
// function pointers.
///////////////////////////////////////////////////////////////////////////////

#ifdef GUID_DEFINED
typedef GUID NVPM_UUID;
#else
/// The standard UUID definition.  sizeof(NVPM_UUID) *needs* to be 16.
/// If it isn't on a needed compilation platform, we need nasty Work-ARounds.
typedef struct NVPM_UUID {
    NVPMUINT   Data1;
    NVPMUINT16 Data2;
    NVPMUINT16 Data3;
    NVPMUINT8  Data4[8];
} NVPM_UUID;
#endif

#ifdef NVPM_INITGUID
    // MSVC seems to require the use of "extern" here, whereas every other
    // compiler seems require omitting it.
#if defined(_MSC_VER)
    #define NVPM_DEFINE_GUID(x__, a, b, c, d0,d1,d2,d3,d4,d5,d6,d7) \
        extern const NVPM_UUID x__ = {a, b, c, {d0,d1,d2,d3,d4,d5,d6,d7}}
#else // !defined(_MSC_VER)
    #define NVPM_DEFINE_GUID(x__, a, b, c, d0,d1,d2,d3,d4,d5,d6,d7) \
        const NVPM_UUID x__ = {a, b, c, {d0,d1,d2,d3,d4,d5,d6,d7}}
#endif // defined(_MSC_VER)
#else // !NVPM_INITGUID
    #define NVPM_DEFINE_GUID(x__, a, b, c, d0,d1,d2,d3,d4,d5,d6,d7) \
        extern const NVPM_UUID x__
#endif // NVPM_INITGUID

// {243E8DA1-4BF8-44B9-98C4-F984D06BDF46}
NVPM_DEFINE_GUID(ETID_NvPmApi,
    0x243e8da1, 0x4bf8, 0x44b9, 0x98, 0xc4, 0xf9, 0x84, 0xd0, 0x6b, 0xdf, 0x46);

typedef struct _NvPmApi
{
    // This export table supports versioning by adding to the end without changing
    // the ETID.  The struct_size field will always be set to the size in bytes of
    // the entire export table structure.
    NVPMUINT struct_size;
    NVPMSetWarningLevel_Pfn SetWarningLevel;
    NVPMGetExtendedError_Pfn GetExtendedError;
    NVPMInit_Pfn Init;
    NVPMShutdown_Pfn Shutdown;
    NVPMCreateContextFromOGLContext_Pfn CreateContextFromOGLContext;
    NVPMCreateContextFromCudaContext_Pfn CreateContextFromCudaContext;
#if defined(_WIN32)
    NVPMCreateContextFromD3D9Device_Pfn CreateContextFromD3D9Device;
    NVPMCreateContextFromD3D10Device_Pfn CreateContextFromD3D10Device;
    NVPMCreateContextFromD3D11Device_Pfn CreateContextFromD3D11Device;
    NVPMCreateContextFromD3D11Device1_Pfn CreateContextFromD3D11Device1;
#endif
    NVPMDestroyContext_Pfn DestroyContext;
    NVPMEnumCountersByContext_Pfn EnumCountersByContext;
    NVPMGetCounterName_Pfn GetCounterName;
    NVPMGetCounterDescription_Pfn GetCounterDescription;
    NVPMGetCounterIDByContext_Pfn GetCounterIDByContext;
    NVPMGetCounterClockRateByContext_Pfn GetCounterClockRateByContext;
    NVPMGetCounterAttribute_Pfn GetCounterAttribute;
    NVPMAddCounterByName_Pfn AddCounterByName;
    NVPMAddCounter_Pfn AddCounter;
    NVPMAddCounters_Pfn AddCounters;
    NVPMRemoveCounterByName_Pfn RemoveCounterByName;
    NVPMRemoveCounter_Pfn RemoveCounter;
    NVPMRemoveCounters_Pfn RemoveCounters;
    NVPMRemoveAllCounters_Pfn RemoveAllCounters;
    NVPMReserveObjects_Pfn ReserveObjects;
    NVPMDeleteObjects_Pfn DeleteObjects;
    NVPMBeginExperiment_Pfn BeginExperiment;
    NVPMEndExperiment_Pfn EndExperiment;
    NVPMBeginPass_Pfn BeginPass;
    NVPMEndPass_Pfn EndPass;
    NVPMBeginObject_Pfn BeginObject;
    NVPMEndObject_Pfn EndObject;
    NVPMSample_Pfn Sample;
    NVPMSampleEx_Pfn SampleEx;
    NVPMGetCounterValueByName_Pfn GetCounterValueByName;
    NVPMGetCounterValue_Pfn GetCounterValue;
    NVPMGetGPUBottleneckName_Pfn GetGPUBottleneckName;
    NVPMRegisterNewDataProviderCallback_Pfn RegisterNewDataProviderCallback;
    NVPMGetCounterValueUint64_Pfn GetCounterValueUint64;
    NVPMGetCounterValueFloat64_Pfn GetCounterValueFloat64;
    NVPMEnumCountersByContextUserData_Pfn EnumCountersByContextUserData;
} NvPmApi;

///////////////////////////////////////////////////////////////////////////////
/// NVPMAPI_INTERFACE NVPMGetExportTable(const NVPM_UUID* pExportTableId, void** ppExportTable);
///
/// @brief Get interface table
/// @param[in] const NVPM_UUID* pExportTableId GUID for the interface table
/// @param[out] void** ppExportTable table exported
/// @return unified return code #NVPMRESULT
///////////////////////////////////////////////////////////////////////////////
typedef NVPMRESULT (NVCALL *NVPMGetExportTable_Pfn)(
    const NVPM_UUID* pExportTableId,
    void** ppExportTable);

#ifdef __cplusplus
};
#endif // End of __cplusplus

#endif // End of _NVPMAPI_H_
