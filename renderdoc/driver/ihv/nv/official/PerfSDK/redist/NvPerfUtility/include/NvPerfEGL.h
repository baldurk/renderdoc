/*
* Copyright 2014-2025 NVIDIA Corporation.  All rights reserved.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#pragma once

#include "NvPerfInit.h"
#include "NvPerfDeviceProperties.h"
#include "nvperf_egl_host.h"
#include "nvperf_egl_target.h"
#include <EGL/egl.h>
#include <string.h>
namespace nv { namespace perf {
    typedef void *EGLDeviceEXT;
    typedef EGLBoolean (*PFNEGLQUERYDISPLAYATTRIBEXTPROC)(EGLDisplay dpy, EGLint attribute, EGLAttrib *value);
    typedef const char * ( * PFNEGLQUERYDEVICESTRINGEXTPROC) (EGLDeviceEXT  device, EGLint  name);
#define EGL_DEVICE_EXT 0x322C
#define EGL_RENDERER_EXT 0x335F

    // EGL Only Utilities
    //
    inline std::string EGLGetDeviceName()
    {
        EGLDisplay display = eglGetCurrentDisplay();
        // No current display
        if (display == EGL_NO_DISPLAY)
        {
            return "";
        }

        PFNEGLQUERYDISPLAYATTRIBEXTPROC pEglQueryDisplayAttribEXT = (PFNEGLQUERYDISPLAYATTRIBEXTPROC)eglGetProcAddress("eglQueryDisplayAttribEXT");
        if (!pEglQueryDisplayAttribEXT)
        {
            return "";
        }

        PFNEGLQUERYDEVICESTRINGEXTPROC pEglQueryDeviceStringExt = (PFNEGLQUERYDEVICESTRINGEXTPROC)eglGetProcAddress("eglQueryDeviceStringEXT");
        if (!pEglQueryDeviceStringExt)
        {
            return "";
        }

        EGLDeviceEXT eglDeviceExt = nullptr;
        if (!pEglQueryDisplayAttribEXT(display, EGL_DEVICE_EXT,(EGLAttrib*)&eglDeviceExt) || !eglDeviceExt)
        {
            return "";
        }

        const char* pRenderer = pEglQueryDeviceStringExt(eglDeviceExt, EGL_RENDERER_EXT);
        if (!pRenderer)
        {
            return "";
        }
        return pRenderer;
    }

    inline bool EGLIsNvidiaDevice()
    {
        EGLDisplay display = eglGetCurrentDisplay();
        // No current display
        if (display == EGL_NO_DISPLAY)
        {
            return false;
        }
        const char* pVendor = eglQueryString(display, EGL_VENDOR);
        if (!pVendor)
        {
            return false;
        }

        if (strstr(pVendor, "NVIDIA"))
        {
            return true;
        }
        return false;
    }

    inline bool EGLLoadDriver()
    {
        NVPW_EGL_LoadDriver_Params loadDriverParams = { NVPW_EGL_LoadDriver_Params_STRUCT_SIZE };
        NVPA_Status nvpaStatus = NVPW_EGL_LoadDriver(&loadDriverParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_EGL_LoadDriver failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return false;
        }
        return true;
    }

    inline size_t EGLGetNvperfDeviceIndex(size_t sliIndex = 0)
    {
        NVPW_EGL_GraphicsContext_GetDeviceIndex_Params getDeviceIndexParams = { NVPW_EGL_GraphicsContext_GetDeviceIndex_Params_STRUCT_SIZE };
        getDeviceIndexParams.sliIndex = sliIndex;

        NVPA_Status nvpaStatus = NVPW_EGL_GraphicsContext_GetDeviceIndex(&getDeviceIndexParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_EGL_GraphicsContext_GetDeviceIndex failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return ~size_t(0);
        }

        return getDeviceIndexParams.deviceIndex;
    }

    inline DeviceIdentifiers EGLGetDeviceIdentifiers(size_t sliIndex = 0)
    {
        const size_t deviceIndex = EGLGetNvperfDeviceIndex(sliIndex);

        DeviceIdentifiers deviceIdentifiers = GetDeviceIdentifiers(deviceIndex);
        return deviceIdentifiers;
    }

    inline ClockInfo EGLGetDeviceClockState()
    {
        size_t nvperfDeviceIndex = EGLGetNvperfDeviceIndex();
        return GetDeviceClockState(nvperfDeviceIndex);
    }

    inline bool EGLSetDeviceClockState(NVPW_Device_ClockSetting clockSetting)
    {
        size_t nvperfDeviceIndex = EGLGetNvperfDeviceIndex();
        return SetDeviceClockState(nvperfDeviceIndex, clockSetting);
    }

    inline bool EGLSetDeviceClockState(const ClockInfo& clockInfo)
    {
        size_t nvperfDeviceIndex = EGLGetNvperfDeviceIndex();
        return SetDeviceClockState(nvperfDeviceIndex, clockInfo);
    }

    inline size_t EGLCalculateMetricsEvaluatorScratchBufferSize(const char* pChipName)
    {
        NVPW_EGL_MetricsEvaluator_CalculateScratchBufferSize_Params calculateScratchBufferSizeParams = { NVPW_EGL_MetricsEvaluator_CalculateScratchBufferSize_Params_STRUCT_SIZE };
        calculateScratchBufferSizeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_EGL_MetricsEvaluator_CalculateScratchBufferSize(&calculateScratchBufferSizeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_EGL_MetricsEvaluator_CalculateScratchBufferSize failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return 0;
        }
        return calculateScratchBufferSizeParams.scratchBufferSize;
    }

    inline NVPW_MetricsEvaluator* EGLCreateMetricsEvaluator(uint8_t* pScratchBuffer, size_t scratchBufferSize, const char* pChipName)
    {
        NVPW_EGL_MetricsEvaluator_Initialize_Params initializeParams = { NVPW_EGL_MetricsEvaluator_Initialize_Params_STRUCT_SIZE };
        initializeParams.pScratchBuffer = pScratchBuffer;
        initializeParams.scratchBufferSize = scratchBufferSize;
        initializeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_EGL_MetricsEvaluator_Initialize(&initializeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_EGL_MetricsEvaluator_Initialize failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return nullptr;
        }
        return initializeParams.pMetricsEvaluator;
    }

}}

namespace nv { namespace perf { namespace profiler {

    inline NVPW_RawCounterConfig* EGLCreateRawCounterConfig(const char* pChipName)
    {
        NVPW_EGL_RawCounterConfig_Create_Params configParams = { NVPW_EGL_RawCounterConfig_Create_Params_STRUCT_SIZE };
        configParams.activityKind = NVPA_ACTIVITY_KIND_PROFILER;
        configParams.pChipName = pChipName;

        NVPA_Status nvpaStatus = NVPW_EGL_RawCounterConfig_Create(&configParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_EGL_RawCounterConfig_Create failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return nullptr;
        }

        return configParams.pRawCounterConfig;
    }

    inline bool EGLIsGpuSupported(size_t sliIndex = 0)
    {
        const size_t deviceIndex = EGLGetNvperfDeviceIndex(sliIndex);

        NVPW_EGL_Profiler_IsGpuSupported_Params params = { NVPW_EGL_Profiler_IsGpuSupported_Params_STRUCT_SIZE };
        params.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_EGL_Profiler_IsGpuSupported(&params);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_EGL_Profiler_IsGpuSupported failed on %s, nvpaStatus = %s\n", EGLGetDeviceName().c_str(), FormatStatus(nvpaStatus).c_str());
            return false;
        }

        if (!params.isSupported)
        {
            NV_PERF_LOG_ERR(10, "%s is not supported for profiling\n", EGLGetDeviceName().c_str());
            if (params.gpuArchitectureSupportLevel != NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_SUPPORTED)
            {
                const DeviceIdentifiers deviceIdentifiers = EGLGetDeviceIdentifiers(sliIndex);
                NV_PERF_LOG_ERR(10, "Unsupported GPU architecture %s\n", deviceIdentifiers.pChipName);
            }
            if (params.sliSupportLevel == NVPW_SLI_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Devices in SLI configuration are not supported.\n");
            }
            if (params.cmpSupportLevel == NVPW_CMP_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Cryptomining GPUs (NVIDIA CMP) are not supported.\n");
            }
            return false;
        }

        return true;
    }

}}}
