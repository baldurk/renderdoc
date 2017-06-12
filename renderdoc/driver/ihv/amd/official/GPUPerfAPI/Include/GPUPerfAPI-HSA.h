//==============================================================================
// Copyright (c) 2015-2016 Advanced Micro Devices, Inc. All rights reserved.
/// \author AMD Developer Tools Team
/// \file
/// \brief  This file can be included by an application that wishes to use the HSA
///         version of GPUPerfAPI. It defines a structure that can be passed to the
///         GPA_OpenContext call when using GPUPerfAPI with HSA.
//==============================================================================


#ifndef _GPUPERFAPI_HSA_H_
#define _GPUPERFAPI_HSA_H_

#include "GPUPerfAPITypes.h"
#include "GPUPerfAPI.h"

#include "hsa.h"

// NOTE: When using the HSA version of GPUPerfAPI, you can initialize and call
//       GPUPerfAPI in one of two ways:
//         1) You must call GPA_Initialize prior to the application initializing
//            the HSA runtime with a call to hsa_init.  You can then simply pass
//            in a hsa_queue_t* instance when calling GPA_OpenContext.  When doing
//            this, GPUPerfAPI will set up the HSA runtime correctly to use the
//            AQL-emulation mode and the pre/post-dispatch callbacks.
//         2) You can perform all initialization yourself to ensure that AQL-emulation
//            mode is used and the pre/post-dispatch callbacks are used.  In that case,
//            you can then call GPA_OpenContext with an instance of the below structure
//            (whose members you would initialize with data provided by the pre-dispatch
//            callback). Note: this second method is currently used by the CodeXL GPU
//            Profiler, though in the future, it may be modified to use the first method.
//
//       It is recommended to use the first method above when using GPUPerfAPI directly
//       from an HSA application.

/// an instance of this structure can be passed to GPA_OpenContext for the HSA
/// version of GPUPerfAPI.
typedef struct
{
    const hsa_agent_t* m_pAgent;                ///< the agent
    const hsa_queue_t* m_pQueue;                ///< the queue
    void*              m_pAqlTranslationHandle; ///< the AQL translation handle (an opaque pointer) supplied by the pre-dispatch callback
} GPA_HSA_Context;


#endif // _GPUPERFAPI_HSA_H_
