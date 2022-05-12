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

#include <vector>
#include "NvPerfInit.h"
#include "nvperf_device_host.h"

namespace nv { namespace perf { namespace sampler {

    inline size_t DeviceCalculateMetricsEvaluatorScratchBufferSize(const char* pChipName)
    {
        NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize_Params calculateScratchBufferSizeParams = { NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize_Params_STRUCT_SIZE };
        calculateScratchBufferSizeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize(&calculateScratchBufferSizeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_Device_MetricsEvaluator_CalculateScratchBufferSize failed\n");
            return 0;
        }
        return calculateScratchBufferSizeParams.scratchBufferSize;
    }

    inline NVPW_MetricsEvaluator* DeviceCreateMetricsEvaluator(uint8_t* pScratchBuffer, size_t scratchBufferSize, const char* pChipName)
    {
        NVPW_Device_MetricsEvaluator_Initialize_Params initializeParams = { NVPW_Device_MetricsEvaluator_Initialize_Params_STRUCT_SIZE };
        initializeParams.pScratchBuffer = pScratchBuffer;
        initializeParams.scratchBufferSize = scratchBufferSize;
        initializeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_Device_MetricsEvaluator_Initialize(&initializeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_Devicee_MetricsEvaluator_Initialize failed\n");
            return nullptr;
        }
        return initializeParams.pMetricsEvaluator;
    }

    inline NVPW_MetricsEvaluator* DeviceCreateMetricsEvaluator(std::vector<uint8_t>& scratchBuffer, const char* pChipName)
    {
        const size_t scratchBufferSize = DeviceCalculateMetricsEvaluatorScratchBufferSize(pChipName);
        if (!scratchBufferSize)
        {
            return (NVPW_MetricsEvaluator*)nullptr;
        }

        scratchBuffer.resize(scratchBufferSize);
        NVPW_MetricsEvaluator* pMetricsEvaluator = DeviceCreateMetricsEvaluator(scratchBuffer.data(), scratchBuffer.size(), pChipName);
        return pMetricsEvaluator;
    }

    inline NVPA_RawMetricsConfig* DeviceCreateRawMetricsConfig(const char* pChipName)
    {
        NVPW_Device_RawMetricsConfig_Create_Params configParams = { NVPW_Device_RawMetricsConfig_Create_Params_STRUCT_SIZE };
        configParams.activityKind = NVPA_ACTIVITY_KIND_REALTIME_SAMPLED;
        configParams.pChipName = pChipName;

        NVPA_Status nvpaStatus = NVPW_Device_RawMetricsConfig_Create(&configParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_Device_RawMetricsConfig_Create failed\n");
            return nullptr;
        }

        return configParams.pRawMetricsConfig;
    }

}}}
