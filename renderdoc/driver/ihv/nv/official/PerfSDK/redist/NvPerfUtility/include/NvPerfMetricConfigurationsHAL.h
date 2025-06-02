/*
* Copyright 2021-2025 NVIDIA Corporation.  All rights reserved.
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

#include <cstring>

#include "NvPerfInit.h"
#include "NvPerfMetricConfigurationsTU10X.h"
#include "NvPerfMetricConfigurationsGA10X.h"
#include "NvPerfMetricConfigurationsAD10X.h"
#include "NvPerfMetricConfigurationsGB20X.h"
#include "NvPerfCounterConfiguration.h"
#include "NvPerfMetricsConfigBuilder.h"
#include "NvPerfMetricsConfigLoader.h"
#include "NvPerfMetricsEvaluator.h"

namespace nv { namespace perf {

    namespace MetricConfigurations {

        inline size_t GetMetricConfigurationsSize(const char* pChipName)
        {
            if (!strcmp(pChipName, "TU102") || !strcmp(pChipName, "TU104") || !strcmp(pChipName, "TU106"))
            {
                return tu10x::MetricConfigurations::GetMetricConfigurationsSize();
            }
            else if (!strcmp(pChipName, "TU116") || !strcmp(pChipName, "TU117"))
            {
                return tu10x::MetricConfigurations::GetMetricConfigurationsSize();
            }
            else if (!strcmp(pChipName, "GA102") || !strcmp(pChipName, "GA103") ||!strcmp(pChipName, "GA104") || !strcmp(pChipName, "GA106") || !strcmp(pChipName, "GA107"))
            {
                return ga10x::MetricConfigurations::GetMetricConfigurationsSize();
            }
            else if (!strcmp(pChipName, "GA10B"))
            {
                return ga10x::MetricConfigurations::GetMetricConfigurationsSize();
            }
            else if (!strcmp(pChipName, "AD102") || !strcmp(pChipName, "AD103") || !strcmp(pChipName, "AD104") || !strcmp(pChipName, "AD106") || !strcmp(pChipName, "AD107"))    
            {
                return ad10x::MetricConfigurations::GetMetricConfigurationsSize();
            }
            else if (false
                     || !strcmp(pChipName, "GB202")
                     || !strcmp(pChipName, "GB203")
                     || !strcmp(pChipName, "GB205")
                     || !strcmp(pChipName, "GB206")
                )
            {
                return gb20x::MetricConfigurations::GetMetricConfigurationsSize();
            }
            else
            {
                NV_PERF_LOG_ERR(20, "Unknown chip \"%s\"\n", pChipName);
                return 0;
            }
        }

        inline const char** GetMetricConfigurationsFileNames(const char* pChipName)
        {
            if (!strcmp(pChipName, "TU102") || !strcmp(pChipName, "TU104") || !strcmp(pChipName, "TU106"))
            {
                return tu10x::MetricConfigurations::GetMetricConfigurationsFileNames();
            }
            else if (!strcmp(pChipName, "TU116") || !strcmp(pChipName, "TU117"))
            {
                return tu10x::MetricConfigurations::GetMetricConfigurationsFileNames();
            }
            else if (!strcmp(pChipName, "GA102") || !strcmp(pChipName, "GA103") ||!strcmp(pChipName, "GA104") || !strcmp(pChipName, "GA106") || !strcmp(pChipName, "GA107"))
            {
                return ga10x::MetricConfigurations::GetMetricConfigurationsFileNames();
            }
            else if (!strcmp(pChipName, "GA10B"))
            {
                return ga10x::MetricConfigurations::GetMetricConfigurationsFileNames();
            }
            else if (!strcmp(pChipName, "AD102") || !strcmp(pChipName, "AD103") || !strcmp(pChipName, "AD104") || !strcmp(pChipName, "AD106") || !strcmp(pChipName, "AD107"))    
            {
                return ad10x::MetricConfigurations::GetMetricConfigurationsFileNames();
            }
            else if (false
                     || !strcmp(pChipName, "GB202")
                     || !strcmp(pChipName, "GB203")
                     || !strcmp(pChipName, "GB205")
                     || !strcmp(pChipName, "GB206")
                )
            {
                return gb20x::MetricConfigurations::GetMetricConfigurationsFileNames();
            }
            else
            {
                NV_PERF_LOG_ERR(20, "Unknown chip \"%s\"\n", pChipName);
                return nullptr;
            }
        }

        inline const char** GetMetricConfigurations(const char* pChipName)
        {
            if (!strcmp(pChipName, "TU102") || !strcmp(pChipName, "TU104") || !strcmp(pChipName, "TU106"))
            {
                return tu10x::MetricConfigurations::GetMetricConfigurations();
            }
            else if (!strcmp(pChipName, "TU116") || !strcmp(pChipName, "TU117"))
            {
                return tu10x::MetricConfigurations::GetMetricConfigurations();
            }
            else if (!strcmp(pChipName, "GA102") || !strcmp(pChipName, "GA103") ||!strcmp(pChipName, "GA104") || !strcmp(pChipName, "GA106") || !strcmp(pChipName, "GA107"))
            {
                return ga10x::MetricConfigurations::GetMetricConfigurations();
            }
            else if (!strcmp(pChipName, "GA10B"))
            {
                return ga10x::MetricConfigurations::GetMetricConfigurations();
            }
            else if (!strcmp(pChipName, "AD102") || !strcmp(pChipName, "AD103") || !strcmp(pChipName, "AD104") || !strcmp(pChipName, "AD106") || !strcmp(pChipName, "AD107"))    
            {
                return ad10x::MetricConfigurations::GetMetricConfigurations();
            }
            else if (false
                || !strcmp(pChipName, "GB202")
                || !strcmp(pChipName, "GB203")
                || !strcmp(pChipName, "GB205")
                || !strcmp(pChipName, "GB206")
                )
            {
                return gb20x::MetricConfigurations::GetMetricConfigurations();
            }
            else
            {
                NV_PERF_LOG_ERR(20, "Unknown chip \"%s\"\n", pChipName);
                return nullptr;
            }
        }

        inline bool GetMetricConfigNameBasedOnHudConfigurationName(std::string& metricConfigName, const char* pChipName, const std::string& hudConfigurationName)
        {
            if (false)
            {
            }
            else if (false
                || !strcmp(pChipName, "GB202")
                || !strcmp(pChipName, "GB203")
                || !strcmp(pChipName, "GB205")
                || !strcmp(pChipName, "GB206")
                )
            {
                const std::map<std::string, std::string> namesMappingSinceBlackwell =
                {
                    { "Graphics High Speed Triage", "Top_Level_Triage" },
                    { "Graphics General Triage", "Top_Level_Triage" }
                };
                if (namesMappingSinceBlackwell.find(hudConfigurationName) != namesMappingSinceBlackwell.end())
                {
                    metricConfigName = namesMappingSinceBlackwell.at(hudConfigurationName);
                    return true;
                }
            }
            else
            {
                const std::map<std::string, std::string> namesMappingSinceTuring =
                {
                    { "Graphics High Speed Triage", "Top_Level_Triage" }
            };
                if (namesMappingSinceTuring.find(hudConfigurationName) != namesMappingSinceTuring.end())
                {
                    metricConfigName = namesMappingSinceTuring.at(hudConfigurationName);
                    return true;
                }
            }
            return false;
        }

        inline bool LoadMetricConfigObject(MetricConfigObject& metricConfigObject, const std::string& chipName, const std::string& triageGroupName)
        {
            size_t bakedConfigurationsSize = MetricConfigurations::GetMetricConfigurationsSize(chipName.c_str());
            const char** bakedConfigurationsFileNames = MetricConfigurations::GetMetricConfigurationsFileNames(chipName.c_str());
            const char** bakedConfigurations = MetricConfigurations::GetMetricConfigurations(chipName.c_str());

            if (bakedConfigurationsSize == 0)
            {
                NV_PERF_LOG_ERR(20, "No available metric configuration file\n");
                return false;
            }
            for (size_t index = 0; index < bakedConfigurationsSize; ++index)
            {
                bool success = metricConfigObject.InitializeFromYaml(bakedConfigurations[index]);
                if (!success)
                {
                    NV_PERF_LOG_WRN(100, "Failed loading baked file \"%s\"\n", bakedConfigurationsFileNames[index]);
                    continue;
                }
                if (metricConfigObject.GetTriageGroupName() == triageGroupName)
                {
                    return true;
                }
            }
            NV_PERF_LOG_ERR(20, "No available metric configuration with triage group: %s \n", triageGroupName.c_str());
            return false;
        }
    } // namespace MetricConfigurations

} }
