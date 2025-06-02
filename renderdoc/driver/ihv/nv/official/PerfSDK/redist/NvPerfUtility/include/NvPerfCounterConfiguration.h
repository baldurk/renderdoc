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

#include <stdint.h>
#include <vector>
#include <fstream>
#include "NvPerfInit.h"
#include "NvPerfMetricsConfigBuilder.h"
#include "NvPerfScopeExitGuard.h"

namespace nv { namespace perf {

    struct CounterConfiguration
    {
        // `configImage` stores the configuration settings for the counters, informing the profiler which counters to collect and how to collect them.
        // These settings are typically applied to a profiling session through the SetConfig API.
        // Note that a profiling session only stores a pointer to the counter config image, so it must remain valid for the entire duration of the profiling session,
        // or until the next SetConfig call with a different counter config image.
        std::vector<uint8_t> configImage;
        // `counterDataPrefix` stores the header information for counter data, which includes the counters present.
        // This header is used to initialize the actual counter data, where counter values are stored.
        // It can be safely destroyed if no further counter data initialization is needed.
        std::vector<uint8_t> counterDataPrefix;
        size_t numPasses = 0;
    };

    /// Transforms configBuilder into configuration.
    inline bool CreateConfiguration(
        MetricsConfigBuilder& configBuilder,
        CounterConfiguration& configuration)
    {
        bool res = false;
        res = configBuilder.PrepareConfigImage();
        if (!res)
        {
            NV_PERF_LOG_ERR(10, "PrepareConfigImage failed\n");
            return false;
        }

        const size_t configImageSize = configBuilder.GetConfigImageSize();
        if (!configImageSize)
        {
            NV_PERF_LOG_ERR(10, "GetConfigImageSize failed\n");
            return false;
        }
        configuration.configImage.resize(configImageSize);
        if (!configBuilder.GetConfigImage(configuration.configImage.size(), &configuration.configImage[0]))
        {
            NV_PERF_LOG_ERR(10, "GetConfigImage failed\n");
            return false;
        }

        const size_t counterDataPrefixSize = configBuilder.GetCounterDataPrefixSize();
        if (!counterDataPrefixSize)
        {
            NV_PERF_LOG_ERR(10, "GetCounterDataPrefixSize failed\n");
            return false;
        }
        configuration.counterDataPrefix.resize(counterDataPrefixSize);
        if (!configBuilder.GetCounterDataPrefix(configuration.counterDataPrefix.size(), &configuration.counterDataPrefix[0]))
        {
            NV_PERF_LOG_ERR(10, "GetCounterDataPrefix failed\n");
            return false;
        }

        NVPW_Config_GetNumPasses_V2_Params getNumPassesParams = { NVPW_Config_GetNumPasses_V2_Params_STRUCT_SIZE };
        getNumPassesParams.pConfig = &configuration.configImage[0];
        NVPA_Status nvpaStatus = NVPW_Config_GetNumPasses_V2(&getNumPassesParams);
        if (nvpaStatus != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "NVPW_Config_GetNumPasses_V2 failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return false;
        }
        configuration.numPasses = getNumPassesParams.numPasses;

        return true;
    }


    /// Adds pMetricNames[0..numMetrics-1] into configBuilder, then transforms configBuilder into configuration.
    inline bool CreateConfiguration(
        MetricsConfigBuilder& configBuilder,
        size_t numMetrics,
        const char* const pMetricNames[],
        CounterConfiguration& configuration)
    {
        bool succeeded = configBuilder.AddMetrics(pMetricNames, numMetrics);
        if (!succeeded)
        {
            return false;
        }

        succeeded = CreateConfiguration(configBuilder, configuration);
        if (!succeeded)
        {
            return false;
        }
        return true;
    }

    /// Load the configuration and counter data prefix from files.
    inline bool LoadConfiguration(
        const char* pConfigImageFileName,
        const char* pCounterDataPrefixFileName,
        CounterConfiguration& configuration)
    {
        // Load the configuration from file.
        std::vector<uint8_t> configImage;
        {
            std::ifstream configImageFile(pConfigImageFileName, std::ios::binary);
            if (configImageFile.fail())
            {
                NV_PERF_LOG_ERR(10, "Failed to open file %s\n", pConfigImageFileName);
                return false;
            }
            configImage.assign(std::istreambuf_iterator<char>(configImageFile), std::istreambuf_iterator<char>());
        }

        // Load the counter data prefix from file.
        std::vector<uint8_t> counterDataPrefix;
        {
            std::ifstream counterDataPrefixFile(pCounterDataPrefixFileName, std::ios::binary);
            if (counterDataPrefixFile.fail())
            {
                NV_PERF_LOG_ERR(10, "Failed to open file %s\n", pCounterDataPrefixFileName);
                return false;
            }
            counterDataPrefix.assign(std::istreambuf_iterator<char>(counterDataPrefixFile), std::istreambuf_iterator<char>());
        }

        NVPW_Config_GetNumPasses_V2_Params getNumPassesParams = { NVPW_Config_GetNumPasses_V2_Params_STRUCT_SIZE };
        getNumPassesParams.pConfig = configImage.data();
        NVPA_Status nvpaStatus = NVPW_Config_GetNumPasses_V2(&getNumPassesParams);
        if (nvpaStatus != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "NVPW_Config_GetNumPasses_V2 failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return false;
        }

        configuration.configImage = std::move(configImage);
        configuration.counterDataPrefix = std::move(counterDataPrefix);
        configuration.numPasses = getNumPassesParams.numPasses;
        return true;
    }
}}
