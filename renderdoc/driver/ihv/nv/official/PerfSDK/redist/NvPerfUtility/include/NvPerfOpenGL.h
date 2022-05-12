/*
* Copyright 2014-2022 NVIDIA Corporation.  All rights reserved.
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
#include "nvperf_opengl_host.h"
#include "nvperf_opengl_target.h"
#include "GL/gl.h"
#include <string.h>
namespace nv { namespace perf {

    // OpenGL Only Utilities
    //
    inline std::string OpenGLGetDeviceName()
    {
        const GLubyte* pRenderer = glGetString(GL_RENDERER);
        if (!pRenderer)
        {
            return "";
        }

        return (const char*) pRenderer;
    }

    inline bool OpenGLIsNvidiaDevice()
    {
        const GLubyte* pVendor = glGetString(GL_VENDOR);
        if (!pVendor)
        {
            return false;
        }

        if (strstr((const char*)pVendor, "NVIDIA"))
        {
            return true;
        }
        return false;
    }

    inline bool OpenGLLoadDriver()
    {
        NVPW_OpenGL_LoadDriver_Params loadDriverParams = { NVPW_OpenGL_LoadDriver_Params_STRUCT_SIZE };
        NVPA_Status nvpaStatus = NVPW_OpenGL_LoadDriver(&loadDriverParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_OpenGL_LoadDriver failed\n");
            return false;
        }
        return true;
    }

    inline size_t OpenGLGetNvperfDeviceIndex(size_t sliIndex = 0)
    {
        NVPW_OpenGL_GraphicsContext_GetDeviceIndex_Params getDeviceIndexParams = { NVPW_OpenGL_GraphicsContext_GetDeviceIndex_Params_STRUCT_SIZE };
        getDeviceIndexParams.sliIndex = sliIndex;

        NVPA_Status nvpaStatus = NVPW_OpenGL_GraphicsContext_GetDeviceIndex(&getDeviceIndexParams);
        if (nvpaStatus)
        {
            return ~size_t(0);
        }

        return getDeviceIndexParams.deviceIndex;
    }

    inline DeviceIdentifiers OpenGLGetDeviceIdentifiers(size_t sliIndex = 0)
    {
        const size_t deviceIndex = OpenGLGetNvperfDeviceIndex(sliIndex);

        DeviceIdentifiers deviceIdentifiers = GetDeviceIdentifiers(deviceIndex);
        return deviceIdentifiers;
    }

    inline NVPW_Device_ClockStatus OpenGLGetDeviceClockState()
    {
        size_t nvperfDeviceIndex = OpenGLGetNvperfDeviceIndex();
        return GetDeviceClockState(nvperfDeviceIndex);
    }

    inline bool OpenGLSetDeviceClockState(NVPW_Device_ClockSetting clockStatus)
    {
        size_t nvperfDeviceIndex = OpenGLGetNvperfDeviceIndex();
        return SetDeviceClockState(nvperfDeviceIndex, clockStatus);
    }

    inline bool OpenGLSetDeviceClockState(NVPW_Device_ClockStatus clockStatus)
    {
        size_t nvperfDeviceIndex = OpenGLGetNvperfDeviceIndex();
        return SetDeviceClockState(nvperfDeviceIndex, clockStatus);
    }

    inline size_t OpenGLCalculateMetricsEvaluatorScratchBufferSize(const char* pChipName)
    {
        NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize_Params calculateScratchBufferSizeParams = { NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize_Params_STRUCT_SIZE };
        calculateScratchBufferSizeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize(&calculateScratchBufferSizeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_OpenGL_MetricsEvaluator_CalculateScratchBufferSize failed\n");
            return 0;
        }
        return calculateScratchBufferSizeParams.scratchBufferSize;
    }

    inline NVPW_MetricsEvaluator* OpenGLCreateMetricsEvaluator(uint8_t* pScratchBuffer, size_t scratchBufferSize, const char* pChipName)
    {
        NVPW_OpenGL_MetricsEvaluator_Initialize_Params initializeParams = { NVPW_OpenGL_MetricsEvaluator_Initialize_Params_STRUCT_SIZE };
        initializeParams.pScratchBuffer = pScratchBuffer;
        initializeParams.scratchBufferSize = scratchBufferSize;
        initializeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_OpenGL_MetricsEvaluator_Initialize(&initializeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_OpenGL_MetricsEvaluator_Initialize failed\n");
            return nullptr;
        }
        return initializeParams.pMetricsEvaluator;
    }

}}

namespace nv { namespace perf { namespace profiler {

    inline NVPA_RawMetricsConfig* OpenGLCreateRawMetricsConfig(const char* pChipName)
    {
        NVPW_OpenGL_RawMetricsConfig_Create_Params configParams = { NVPW_OpenGL_RawMetricsConfig_Create_Params_STRUCT_SIZE };
        configParams.activityKind = NVPA_ACTIVITY_KIND_PROFILER;
        configParams.pChipName = pChipName;

        NVPA_Status nvpaStatus = NVPW_OpenGL_RawMetricsConfig_Create(&configParams);
        if (nvpaStatus)
        {
            return nullptr;
        }

        return configParams.pRawMetricsConfig;
    }

    inline bool OpenGLIsGpuSupported(size_t sliIndex = 0)
    {
        const size_t deviceIndex = OpenGLGetNvperfDeviceIndex(sliIndex);

        NVPW_OpenGL_Profiler_IsGpuSupported_Params params = { NVPW_OpenGL_Profiler_IsGpuSupported_Params_STRUCT_SIZE };
        params.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_OpenGL_Profiler_IsGpuSupported(&params);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_OpenGL_Profiler_IsGpuSupported failed on %s\n", OpenGLGetDeviceName().c_str());
            return false;
        }

        if (!params.isSupported)
        {
            NV_PERF_LOG_ERR(10, "%s is not supported for profiling\n", OpenGLGetDeviceName().c_str());
            if (params.gpuArchitectureSupportLevel != NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_SUPPORTED)
            {
                const DeviceIdentifiers deviceIdentifiers = OpenGLGetDeviceIdentifiers(sliIndex);
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
