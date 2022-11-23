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

#include <utility>
#include "NvPerfMetricsEvaluator.h"

namespace nv { namespace perf {

    class MetricsConfigBuilder
    {
    protected:
        NVPW_MetricsEvaluator* m_pMetricsEvaluator;         // not owned
        NVPA_RawMetricsConfig* m_pRawMetricsConfig;         // owned
        NVPA_CounterDataBuilder* m_pCounterDataBuilder;     // owned
        bool m_configuring;

    protected:
        void MoveAssign(MetricsConfigBuilder&& rhs)
        {
            Reset();
            m_pMetricsEvaluator = rhs.m_pMetricsEvaluator;
            m_pRawMetricsConfig = rhs.m_pRawMetricsConfig;
            m_pCounterDataBuilder = rhs.m_pCounterDataBuilder;
            m_configuring = rhs.m_configuring;

            rhs.m_pMetricsEvaluator = nullptr;
            rhs.m_pRawMetricsConfig = nullptr;
            rhs.m_pCounterDataBuilder = nullptr;
        }

    public:
        ~MetricsConfigBuilder()
        {
            Reset();
        }
        MetricsConfigBuilder() : m_pMetricsEvaluator(nullptr), m_pRawMetricsConfig(nullptr), m_pCounterDataBuilder(nullptr), m_configuring(false)
        {
        }
        MetricsConfigBuilder(MetricsConfigBuilder&& rhs) : m_pMetricsEvaluator(nullptr), m_pRawMetricsConfig(nullptr), m_pCounterDataBuilder(nullptr), m_configuring(false)
        {
            MoveAssign(std::forward<MetricsConfigBuilder>(rhs));
        }
        MetricsConfigBuilder& operator=(MetricsConfigBuilder&& rhs)
        {
            MoveAssign(std::forward<MetricsConfigBuilder>(rhs));
            return *this;
        }

        void Reset()
        {
            NVPW_RawMetricsConfig_Destroy_Params rawMetricsConfigParams = { NVPW_RawMetricsConfig_Destroy_Params_STRUCT_SIZE };
            rawMetricsConfigParams.pRawMetricsConfig = m_pRawMetricsConfig;
            NVPW_RawMetricsConfig_Destroy(&rawMetricsConfigParams);

            NVPW_CounterDataBuilder_Destroy_Params counterDataBuilderParams = { NVPW_CounterDataBuilder_Destroy_Params_STRUCT_SIZE };
            counterDataBuilderParams.pCounterDataBuilder = m_pCounterDataBuilder;
            NVPW_CounterDataBuilder_Destroy(&counterDataBuilderParams);

            m_pMetricsEvaluator     = nullptr;
            m_pRawMetricsConfig     = nullptr;
            m_pCounterDataBuilder   = nullptr;
        }

        bool Initialize(NVPW_MetricsEvaluator* pMetricsEvaluator, NVPA_RawMetricsConfig* pRawMetricsConfig, const char* chipName)
        {
            NVPA_Status nvpaStatus;

            Reset(); // destroy any existing objects
            m_pMetricsEvaluator = pMetricsEvaluator;
            m_pRawMetricsConfig = pRawMetricsConfig;
            NVPW_CounterDataBuilder_Create_Params counterDataBuilderParams = { NVPW_CounterDataBuilder_Create_Params_STRUCT_SIZE };
            counterDataBuilderParams.pChipName = chipName;
            nvpaStatus = NVPW_CounterDataBuilder_Create(&counterDataBuilderParams);
            if (nvpaStatus)
            {
                return false;
            }

            m_pCounterDataBuilder = counterDataBuilderParams.pCounterDataBuilder;

            NVPW_RawMetricsConfig_BeginPassGroup_Params beginPassGroupParams = { NVPW_RawMetricsConfig_BeginPassGroup_Params_STRUCT_SIZE };
            beginPassGroupParams.pRawMetricsConfig = m_pRawMetricsConfig;
            nvpaStatus = NVPW_RawMetricsConfig_BeginPassGroup(&beginPassGroupParams);
            if (nvpaStatus)
            {
                return false;
            }
            m_configuring = true;

            return true;
        }

        bool AddMetrics(const NVPW_MetricEvalRequest* pMetricEvalRequests, size_t numMetricEvalRequests, bool keepInstances = true)
        {
            NVPA_Status nvpaStatus;
            NVPW_MetricsEvaluator_GetMetricRawDependencies_Params getMetricRawDependenciesParams = { NVPW_MetricsEvaluator_GetMetricRawDependencies_Params_STRUCT_SIZE };
            getMetricRawDependenciesParams.pMetricsEvaluator = m_pMetricsEvaluator;
            getMetricRawDependenciesParams.pMetricEvalRequests = pMetricEvalRequests;
            getMetricRawDependenciesParams.numMetricEvalRequests = numMetricEvalRequests;
            getMetricRawDependenciesParams.metricEvalRequestStructSize = NVPW_MetricEvalRequest_STRUCT_SIZE;
            getMetricRawDependenciesParams.metricEvalRequestStrideSize = sizeof(NVPW_MetricEvalRequest);
            nvpaStatus = NVPW_MetricsEvaluator_GetMetricRawDependencies(&getMetricRawDependenciesParams);
            if (nvpaStatus)
            {
                NV_PERF_LOG_ERR(50, "NVPW_MetricsEvaluator_GetMetricRawDependencies failed\n");
                return false;
            }

            std::vector<const char*> rawDependencies(getMetricRawDependenciesParams.numRawDependencies);
            std::vector<const char*> optionalRawDependencies(getMetricRawDependenciesParams.numOptionalRawDependencies);
            getMetricRawDependenciesParams.ppRawDependencies = rawDependencies.data();
            getMetricRawDependenciesParams.ppOptionalRawDependencies = optionalRawDependencies.data();
            nvpaStatus = NVPW_MetricsEvaluator_GetMetricRawDependencies(&getMetricRawDependenciesParams);
            if (nvpaStatus)
            {
                NV_PERF_LOG_ERR(50, "NVPW_MetricsEvaluator_GetMetricRawDependencies failed\n");
                return false;
            }

            auto addRawMetric = [&](const char* const pRawMetricName, bool emitError) {
                NVPA_RawMetricRequest rawMetricRequest = { NVPA_RAW_METRIC_REQUEST_STRUCT_SIZE };
                rawMetricRequest.pMetricName = pRawMetricName;
                rawMetricRequest.keepInstances = keepInstances;

                NVPW_CounterDataBuilder_AddMetrics_Params addMetricParams = { NVPW_CounterDataBuilder_AddMetrics_Params_STRUCT_SIZE };
                addMetricParams.numMetricRequests = 1;
                addMetricParams.pCounterDataBuilder = m_pCounterDataBuilder;
                addMetricParams.pRawMetricRequests = &rawMetricRequest;
                nvpaStatus = NVPW_CounterDataBuilder_AddMetrics(&addMetricParams);
                if (nvpaStatus)
                {
                    const char* pString = "NVPW_CounterDataBuilder_AddMetrics failed for raw metric: %s\n";
                    if (emitError)
                    {
                        NV_PERF_LOG_ERR(50, pString, pRawMetricName);
                    }
                    else
                    {
                        NV_PERF_LOG_WRN(50, pString, pRawMetricName);
                    }
                    return false;
                }

                NVPW_RawMetricsConfig_AddMetrics_Params configAddMetricParams = { NVPW_RawMetricsConfig_AddMetrics_Params_STRUCT_SIZE };
                configAddMetricParams.numMetricRequests = 1;
                configAddMetricParams.pRawMetricRequests = &rawMetricRequest;
                configAddMetricParams.pRawMetricsConfig = m_pRawMetricsConfig;
                nvpaStatus = NVPW_RawMetricsConfig_AddMetrics(&configAddMetricParams);
                if (nvpaStatus)
                {
                    const char* pString = "NVPW_RawMetricsConfig_AddMetrics failed for raw metric: %s\n";
                    if (emitError)
                    {
                        NV_PERF_LOG_ERR(50, pString, pRawMetricName);
                    }
                    else
                    {
                        NV_PERF_LOG_WRN(50, pString, pRawMetricName);
                    }
                    return false;
                }
                return true;
            };

            for (const char* const pRawMetricName : rawDependencies)
            {
                if (!addRawMetric(pRawMetricName, true))
                {
                    return false;
                }
            }
            for (const char* const pRawMetricName : optionalRawDependencies)
            {
                addRawMetric(pRawMetricName, false); // fail in scheduling an optional raw metric may not be considered a failure
            }

            return true;
        }

        bool AddMetric(const char* pMetricName)
        {
            NVPW_MetricEvalRequest metricEvalRequest{};
            bool success = ToMetricEvalRequest(m_pMetricsEvaluator, pMetricName, metricEvalRequest);
            if (!success)
            {
                NV_PERF_LOG_ERR(50, "ToMetricEvalRequest failed for metric: %s\n", pMetricName);
                return false;
            }
            success = AddMetrics(&metricEvalRequest, 1);
            if (!success)
            {
                NV_PERF_LOG_ERR(50, "AddMetrics failed for metric: %s\n", pMetricName);
                return false;
            }
            return true;
        }

        bool AddMetrics(const char* const pMetricNames[], size_t numMetrics)
        {
            bool success = true;
            for (size_t metricIdx = 0; metricIdx < numMetrics; ++metricIdx)
            {
                const bool addMetricSuccess = AddMetric(pMetricNames[metricIdx]);
                if (!addMetricSuccess)
                {
                    success = false;
                }
            }
            if (!success)
            {
                return false;
            }
            return true;
        }

        bool PrepareConfigImage()
        {
            NVPA_Status nvpaStatus;
            m_configuring = false;

            NVPW_RawMetricsConfig_EndPassGroup_Params endPassGroupParam = { NVPW_RawMetricsConfig_EndPassGroup_Params_STRUCT_SIZE };
            endPassGroupParam.pRawMetricsConfig = m_pRawMetricsConfig;
            nvpaStatus = NVPW_RawMetricsConfig_EndPassGroup(&endPassGroupParam);
            if (nvpaStatus)
            {
                return false;
            }

            NVPW_RawMetricsConfig_GenerateConfigImage_Params generateConfigImageParam = { NVPW_RawMetricsConfig_GenerateConfigImage_Params_STRUCT_SIZE };
            generateConfigImageParam.pRawMetricsConfig = m_pRawMetricsConfig;
            nvpaStatus = NVPW_RawMetricsConfig_GenerateConfigImage(&generateConfigImageParam);
            if (nvpaStatus)
            {
                return false;
            }

            // Start a new PassGroup so that subsequent AddMetrics() calls will succeed.
            // This will not result in optimal scheduling, but it obeys the principle of least surprise.
            NVPW_RawMetricsConfig_BeginPassGroup_Params beginPassGroupParams = { NVPW_RawMetricsConfig_BeginPassGroup_Params_STRUCT_SIZE };
            beginPassGroupParams.pRawMetricsConfig = m_pRawMetricsConfig;
            nvpaStatus = NVPW_RawMetricsConfig_BeginPassGroup(&beginPassGroupParams);
            if (nvpaStatus)
            {
                return false;
            }
            m_configuring = true;

            return true;
        }

        // Returns the buffer size needed for the ConfigImage, or zero on error.
        size_t GetConfigImageSize() const
        {
            NVPW_RawMetricsConfig_GetConfigImage_Params getConfigImageParam = { NVPW_RawMetricsConfig_GetConfigImage_Params_STRUCT_SIZE };
            getConfigImageParam.pBuffer = nullptr;
            getConfigImageParam.bytesAllocated = 0;
            getConfigImageParam.pRawMetricsConfig = m_pRawMetricsConfig;
            NVPA_Status nvpaStatus = NVPW_RawMetricsConfig_GetConfigImage(&getConfigImageParam);
            if (nvpaStatus)
            {
                return 0;
            }

            return getConfigImageParam.bytesCopied;
        }

        // Copies the generated ConfigImage into pBuffer.
        bool GetConfigImage(size_t bufferSize, uint8_t* pBuffer) const
        {
            NVPW_RawMetricsConfig_GetConfigImage_Params getConfigImageParam = { NVPW_RawMetricsConfig_GetConfigImage_Params_STRUCT_SIZE };
            getConfigImageParam.pRawMetricsConfig = m_pRawMetricsConfig;
            getConfigImageParam.bytesAllocated = bufferSize;
            getConfigImageParam.pBuffer = pBuffer;
            NVPA_Status nvpaStatus = NVPW_RawMetricsConfig_GetConfigImage(&getConfigImageParam);
            if (nvpaStatus)
            {
                return false;
            }
            return true;
        }

        // Returns the buffer size needed for the CounterDataPrefix, or zero on error.
        size_t GetCounterDataPrefixSize() const
        {
            NVPW_CounterDataBuilder_GetCounterDataPrefix_Params getCounterDataPrefixParams = { NVPW_CounterDataBuilder_GetCounterDataPrefix_Params_STRUCT_SIZE };
            getCounterDataPrefixParams.bytesAllocated = 0;
            getCounterDataPrefixParams.pBuffer = nullptr;
            getCounterDataPrefixParams.pCounterDataBuilder = m_pCounterDataBuilder;
            NVPA_Status nvpaStatus = NVPW_CounterDataBuilder_GetCounterDataPrefix(&getCounterDataPrefixParams);
            if (nvpaStatus)
            {
                return 0;
            }

            return getCounterDataPrefixParams.bytesCopied;
        }

        // Copies the generated CounterDataPrefix into pBuffer.
        bool GetCounterDataPrefix(size_t bufferSize, uint8_t* pBuffer) const
        {
            NVPW_CounterDataBuilder_GetCounterDataPrefix_Params getCounterDataPrefixParams = { NVPW_CounterDataBuilder_GetCounterDataPrefix_Params_STRUCT_SIZE };
            getCounterDataPrefixParams.bytesAllocated = bufferSize;
            getCounterDataPrefixParams.pBuffer = pBuffer;
            getCounterDataPrefixParams.pCounterDataBuilder = m_pCounterDataBuilder;
            NVPA_Status nvpaStatus = NVPW_CounterDataBuilder_GetCounterDataPrefix(&getCounterDataPrefixParams);
            if (nvpaStatus)
            {
                return false;
            }
            return true;
        }

        // Returns the number of replay passes for this metrics config.
        // Total num passes = numPasses * numNestingLevels
        size_t GetNumPasses() const
        {
            NVPW_RawMetricsConfig_GetNumPasses_V2_Params getNumPassesParams;
            getNumPassesParams.structSize = sizeof(getNumPassesParams);
            getNumPassesParams.pPriv = NULL;
            getNumPassesParams.pRawMetricsConfig = m_pRawMetricsConfig;
            NVPA_Status nvpaStatus = NVPW_RawMetricsConfig_GetNumPasses_V2(&getNumPassesParams);
            if (nvpaStatus)
            {
                return 0u;
            }
            return getNumPassesParams.numPasses;
        }
    };

}}
