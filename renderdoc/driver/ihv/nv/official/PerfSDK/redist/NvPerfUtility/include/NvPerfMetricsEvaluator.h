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

#include <sstream>
#include <utility>
#include <vector>
#include <string>
#include "NvPerfInit.h"

namespace nv { namespace perf {

    // Smart Pointer for NVPW_MetricsEvaluator
    class MetricsEvaluator
    {
    protected:
        NVPW_MetricsEvaluator* m_pMetricsEvaluator;
        std::vector<uint8_t> m_scratchBuffer;

    private:
        // Prevent accidental use of "delete" keyword on this class' implicit conversions.
        // Introducing a second 'operator CompileErrorOnOperatorDelete*()' triggers an 'ambiguous conversion to void*'
        // on the 'delete', which catches the usage error at compile time.  c.f. http://stackoverflow.com/a/3312507
        struct CompileErrorOnOperatorDelete;
        operator CompileErrorOnOperatorDelete*() const;

    private:
        // non-copyable
        MetricsEvaluator(const MetricsEvaluator& rhs);
        MetricsEvaluator& operator=(const MetricsEvaluator& rhs);

    public:
        ~MetricsEvaluator()
        {
            Reset();
        }

        MetricsEvaluator()
            : m_pMetricsEvaluator()
        {
        }

        // takes the ownership
        MetricsEvaluator(NVPW_MetricsEvaluator* pMetricsEvaluator, std::vector<uint8_t>&& scratchBuffer)
            : m_pMetricsEvaluator(pMetricsEvaluator)
            , m_scratchBuffer(std::move(scratchBuffer))
        {
            scratchBuffer.clear();
        }

        MetricsEvaluator(MetricsEvaluator&& evaluator)
            : m_pMetricsEvaluator(evaluator.m_pMetricsEvaluator)
            , m_scratchBuffer(std::move(evaluator.m_scratchBuffer))
        {
            evaluator.m_pMetricsEvaluator = nullptr;
            evaluator.m_scratchBuffer.clear();
        }

        MetricsEvaluator& operator=(MetricsEvaluator&& evaluator)
        {
            Reset();
            m_pMetricsEvaluator = evaluator.m_pMetricsEvaluator;
            m_scratchBuffer = std::move(evaluator.m_scratchBuffer);
            evaluator.m_pMetricsEvaluator = nullptr;
            evaluator.m_scratchBuffer.clear();
            return *this;
        }

        operator NVPW_MetricsEvaluator*() const
        {
            return m_pMetricsEvaluator;
        }

        void Reset()
        {
            if (m_pMetricsEvaluator != nullptr)
            {
                NVPW_MetricsEvaluator_Destroy_Params destroyParams = { NVPW_MetricsEvaluator_Destroy_Params_STRUCT_SIZE };
                destroyParams.pMetricsEvaluator = m_pMetricsEvaluator;
                NVPA_Status status = NVPW_MetricsEvaluator_Destroy(&destroyParams);
                if (status != NVPA_STATUS_SUCCESS)
                {
                    NV_PERF_LOG_ERR(80, "NVPW_MetricsEvaluator_Destroy failed\n");
                }
                m_pMetricsEvaluator = nullptr;
            }
            m_scratchBuffer.clear();
        }
    };

    class MetricsEnumerator
    {
    public:
        class Iterator
        {
        private:
            // note these are pointing to the .RO section of the library, so their lifetime are not bound to any particular metrics enumerator or metrics evaluator instance
            const char* m_pMetricNames;
            const size_t* m_pMetricNameBeginIndices;
            size_t m_numMetrics;
            size_t m_metricIndex;
        public:
            Iterator()
                : m_pMetricNames(nullptr)
                , m_pMetricNameBeginIndices(nullptr)
                , m_numMetrics(0)
                , m_metricIndex(0)
            {
            }

            Iterator(const char* pMetricNames, const size_t* pMetricNameBeginIndices, size_t numMetrics, size_t metricIndex)
                : m_pMetricNames(pMetricNames)
                , m_pMetricNameBeginIndices(pMetricNameBeginIndices)
                , m_numMetrics(numMetrics)
                , m_metricIndex(metricIndex)
            {
            }

            Iterator(const Iterator& iterator)
                : m_pMetricNames(iterator.m_pMetricNames)
                , m_pMetricNameBeginIndices(iterator.m_pMetricNameBeginIndices)
                , m_numMetrics(iterator.m_numMetrics)
                , m_metricIndex(iterator.m_metricIndex)
            {
            }

            Iterator& operator=(const Iterator& rhs)
            {
                m_pMetricNames = rhs.m_pMetricNames;
                m_pMetricNameBeginIndices = rhs.m_pMetricNameBeginIndices;
                m_numMetrics = rhs.m_numMetrics;
                m_metricIndex = rhs.m_metricIndex;
                return *this;
            }

            bool operator!=(const Iterator& rhs) const
            {
                return !(*this == rhs);
            }

            bool operator==(const Iterator& rhs) const
            {
                return m_pMetricNames == rhs.m_pMetricNames
                    && m_pMetricNameBeginIndices == rhs.m_pMetricNameBeginIndices
                    && m_numMetrics == rhs.m_numMetrics
                    && m_metricIndex == rhs.m_metricIndex;
            }

            Iterator operator++()
            {
                if (m_metricIndex < m_numMetrics)
                {
                    ++m_metricIndex;
                }
                return *this;
            }

            Iterator operator++(int)
            {
                Iterator prev = *this;
                ++*this;
                return prev;
            }

            // no validity check
            const char* operator*() const
            {
                const char* pMetricName = &m_pMetricNames[m_pMetricNameBeginIndices[m_metricIndex]];
                return pMetricName;
            }
        };

    private:
        // note these are pointing to the .RO section of the library, so their lifetime are not bound to any particular metrics evaluator instance
        const char* m_pMetricNames;
        const size_t* m_pMetricNameBeginIndices;
        size_t m_numMetrics;

    public:
        MetricsEnumerator()
            : m_pMetricNames(nullptr)
            , m_pMetricNameBeginIndices(nullptr)
            , m_numMetrics(0)
        {
        }

        MetricsEnumerator(const char* pMetricNames, const size_t* pMetricNameBeginIndices, size_t numMetrics)
            : m_pMetricNames(pMetricNames)
            , m_pMetricNameBeginIndices(pMetricNameBeginIndices)
            , m_numMetrics(numMetrics)
        {
        }

        MetricsEnumerator(const MetricsEnumerator& metricsEnumerator)
            : m_pMetricNames(metricsEnumerator.m_pMetricNames)
            , m_pMetricNameBeginIndices(metricsEnumerator.m_pMetricNameBeginIndices)
            , m_numMetrics(metricsEnumerator.m_numMetrics)
        {
        }

        MetricsEnumerator& operator=(const MetricsEnumerator& rhs)
        {
            m_pMetricNames = rhs.m_pMetricNames;
            m_pMetricNameBeginIndices = rhs.m_pMetricNameBeginIndices;
            m_numMetrics = rhs.m_numMetrics;
            return *this;
        }

        // no bounds check
        const char* operator[](size_t index) const
        {
            const char* pMetricName = &m_pMetricNames[m_pMetricNameBeginIndices[index]];
            return pMetricName;
        }

        Iterator begin() const
        {
            return Iterator(m_pMetricNames, m_pMetricNameBeginIndices, m_numMetrics, 0);
        }

        Iterator end() const
        {
            return Iterator(m_pMetricNames, m_pMetricNameBeginIndices, m_numMetrics, m_numMetrics);
        }

        size_t size() const
        {
            return m_numMetrics;
        }

        bool empty() const
        {
            return !m_numMetrics;
        }
    };

    inline MetricsEnumerator EnumerateMetrics(NVPW_MetricsEvaluator* pMetricsEvaluator, NVPW_MetricType metricType)
    {
        NVPW_MetricsEvaluator_GetMetricNames_Params metricsEvaluatorGetMetricNamesParams = { NVPW_MetricsEvaluator_GetMetricNames_Params_STRUCT_SIZE };
        metricsEvaluatorGetMetricNamesParams.pMetricsEvaluator = pMetricsEvaluator;
        metricsEvaluatorGetMetricNamesParams.metricType = static_cast<uint8_t>(metricType);
        const NVPA_Status status = NVPW_MetricsEvaluator_GetMetricNames(&metricsEvaluatorGetMetricNamesParams);
        if (status != NVPA_STATUS_SUCCESS)
        {
            return MetricsEnumerator();
        }
        return MetricsEnumerator(metricsEvaluatorGetMetricNamesParams.pMetricNames, metricsEvaluatorGetMetricNamesParams.pMetricNameBeginIndices, metricsEvaluatorGetMetricNamesParams.numMetrics);
    }

    inline MetricsEnumerator EnumerateCounters(NVPW_MetricsEvaluator* pMetricsEvaluator)
    {
        return EnumerateMetrics(pMetricsEvaluator, NVPW_METRIC_TYPE_COUNTER);
    }

    inline MetricsEnumerator EnumerateRatios(NVPW_MetricsEvaluator* pMetricsEvaluator)
    {
        return EnumerateMetrics(pMetricsEvaluator, NVPW_METRIC_TYPE_RATIO);
    }

    inline MetricsEnumerator EnumerateThroughputs(NVPW_MetricsEvaluator* pMetricsEvaluator)
    {
        return EnumerateMetrics(pMetricsEvaluator, NVPW_METRIC_TYPE_THROUGHPUT);
    }

    inline const char* ToCString(NVPW_MetricType metricType)
    {
        switch (metricType)
        {
            case NVPW_METRIC_TYPE_COUNTER:
                return "Counter";
            case NVPW_METRIC_TYPE_RATIO:
                return "Ratio";
            case NVPW_METRIC_TYPE_THROUGHPUT:
                return "Throughput";
            default:
                return "";
        }
    }

    inline const char* ToCString(NVPW_RollupOp rollupOp)
    {
        switch (rollupOp)
        {
            case NVPW_ROLLUP_OP_AVG:
                return ".avg";
            case NVPW_ROLLUP_OP_MAX:
                return ".max";
            case NVPW_ROLLUP_OP_MIN:
                return ".min";
            case NVPW_ROLLUP_OP_SUM:
                return ".sum";
            default:
                return "";
        }
    }

    inline const char* ToCString(NVPW_Submetric submetric)
    {
        switch (submetric)
        {
            case NVPW_SUBMETRIC_NONE:
                return "";
            case NVPW_SUBMETRIC_PEAK_SUSTAINED:
                return ".peak_sustained";
            case NVPW_SUBMETRIC_PEAK_SUSTAINED_ACTIVE:
                return ".peak_sustained_active";
            case NVPW_SUBMETRIC_PEAK_SUSTAINED_ACTIVE_PER_SECOND:
                return ".peak_sustained_active.per_second";
            case NVPW_SUBMETRIC_PEAK_SUSTAINED_ELAPSED:
                return ".peak_sustained_elapsed";
            case NVPW_SUBMETRIC_PEAK_SUSTAINED_ELAPSED_PER_SECOND:
                return ".peak_sustained_elapsed.per_second";
            case NVPW_SUBMETRIC_PEAK_SUSTAINED_FRAME:
                return ".peak_sustained_frame";
            case NVPW_SUBMETRIC_PEAK_SUSTAINED_FRAME_PER_SECOND:
                return ".peak_sustained_frame.per_second";
            case NVPW_SUBMETRIC_PEAK_SUSTAINED_REGION:
                return ".peak_sustained_region";
            case NVPW_SUBMETRIC_PEAK_SUSTAINED_REGION_PER_SECOND:
                return ".peak_sustained_region.per_second";
            case NVPW_SUBMETRIC_PER_CYCLE_ACTIVE:
                return ".per_cycle_active";
            case NVPW_SUBMETRIC_PER_CYCLE_ELAPSED:
                return ".per_cycle_elapsed";
            case NVPW_SUBMETRIC_PER_CYCLE_IN_FRAME:
                return ".per_cycle_in_frame";
            case NVPW_SUBMETRIC_PER_CYCLE_IN_REGION:
                return ".per_cycle_in_region";
            case NVPW_SUBMETRIC_PER_SECOND:
                return ".per_second";
            case NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_ACTIVE:
                return ".pct_of_peak_sustained_active";
            case NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_ELAPSED:
                return ".pct_of_peak_sustained_elapsed";
            case NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_FRAME:
                return ".pct_of_peak_sustained_frame";
            case NVPW_SUBMETRIC_PCT_OF_PEAK_SUSTAINED_REGION:
                return ".pct_of_peak_sustained_region";
            case NVPW_SUBMETRIC_MAX_RATE:
                return ".max_rate";
            case NVPW_SUBMETRIC_PCT:
                return ".pct";
            case NVPW_SUBMETRIC_RATIO:
                return ".ratio";
            default:
                return "";
        }
    }

    inline const char* ToCString(const MetricsEnumerator& countersEnumerator, const MetricsEnumerator& ratiosEnumerator, const MetricsEnumerator& throughputsEnumerator, NVPW_MetricType metricType, size_t metricIndex)
    {
        if (metricType == NVPW_METRIC_TYPE_COUNTER)
        {
            if (metricIndex < countersEnumerator.size())
            {
                return countersEnumerator[metricIndex];
            }
        }
        else if (metricType == NVPW_METRIC_TYPE_RATIO)
        {
            if (metricIndex < ratiosEnumerator.size())
            {
                return ratiosEnumerator[metricIndex];
            }
        }
        else if (metricType == NVPW_METRIC_TYPE_THROUGHPUT)
        {
            if (metricIndex < throughputsEnumerator.size())
            {
                return throughputsEnumerator[metricIndex];
            }
        }
        NV_PERF_LOG_WRN(50, "ToCString failed\n");
        return "";
    }

    inline const char* ToCString(NVPW_MetricsEvaluator* pMetricsEvaluator, NVPW_MetricType metricType, size_t metricIndex)
    {
        if (metricType == NVPW_METRIC_TYPE_COUNTER)
        {
            const MetricsEnumerator countersEnumerator = EnumerateCounters(pMetricsEvaluator);
            if (metricIndex < countersEnumerator.size())
            {
                return countersEnumerator[metricIndex];
            }
        }
        else if (metricType == NVPW_METRIC_TYPE_RATIO)
        {
            const MetricsEnumerator ratiosEnumerator = EnumerateRatios(pMetricsEvaluator);
            if (metricIndex < ratiosEnumerator.size())
            {
                return ratiosEnumerator[metricIndex];
            }
        }
        else if (metricType == NVPW_METRIC_TYPE_THROUGHPUT)
        {
            const MetricsEnumerator throughputsEnumerator = EnumerateThroughputs(pMetricsEvaluator);
            if (metricIndex < throughputsEnumerator.size())
            {
                return throughputsEnumerator[metricIndex];
            }
        }
        NV_PERF_LOG_WRN(50, "ToCString failed\n");
        return "";
    }

    inline std::string ToString(const MetricsEnumerator& countersEnumerator, const MetricsEnumerator& ratiosEnumerator, const MetricsEnumerator& throughputsEnumerator, const NVPW_MetricEvalRequest& metricEvalRequest)
    {
        std::string metricName(ToCString(countersEnumerator, ratiosEnumerator, throughputsEnumerator, static_cast<NVPW_MetricType>(metricEvalRequest.metricType), metricEvalRequest.metricIndex));
        if (metricEvalRequest.metricType == NVPW_METRIC_TYPE_COUNTER || metricEvalRequest.metricType == NVPW_METRIC_TYPE_THROUGHPUT)
        {
            metricName += ToCString(static_cast<NVPW_RollupOp>(metricEvalRequest.rollupOp));
        }
        metricName += ToCString(static_cast<NVPW_Submetric>(metricEvalRequest.submetric));
        return metricName;
    }

    inline std::string ToString(NVPW_MetricsEvaluator* pMetricsEvaluator, const NVPW_MetricEvalRequest& metricEvalRequest)
    {
        std::string metricName(ToCString(pMetricsEvaluator, static_cast<NVPW_MetricType>(metricEvalRequest.metricType), metricEvalRequest.metricIndex));
        if (metricEvalRequest.metricType == NVPW_METRIC_TYPE_COUNTER || metricEvalRequest.metricType == NVPW_METRIC_TYPE_THROUGHPUT)
        {
            metricName += ToCString(static_cast<NVPW_RollupOp>(metricEvalRequest.rollupOp));
        }
        metricName += ToCString(static_cast<NVPW_Submetric>(metricEvalRequest.submetric));
        return metricName;
    }

    inline bool ToMetricEvalRequest(NVPW_MetricsEvaluator* pMetricsEvaluator, const char* pMetricName, NVPW_MetricEvalRequest& metricEvalRequest)
    {
        NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params toMetricEvalRequestParams = { NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest_Params_STRUCT_SIZE };
        toMetricEvalRequestParams.pMetricsEvaluator = pMetricsEvaluator;
        toMetricEvalRequestParams.pMetricName = pMetricName;
        toMetricEvalRequestParams.pMetricEvalRequest = &metricEvalRequest;
        toMetricEvalRequestParams.metricEvalRequestStructSize = NVPW_MetricEvalRequest_STRUCT_SIZE;
        const NVPA_Status status = NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest(&toMetricEvalRequestParams);
        if (status != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_WRN(80, "NVPW_MetricsEvaluator_ConvertMetricNameToMetricEvalRequest failed\n");
            return false;
        }
        return true;
    }

    inline bool GetMetricTypeAndIndex(NVPW_MetricsEvaluator* pMetricsEvaluator, const char* pMetricName, NVPW_MetricType& metricType, size_t& metricIndex)
    {
        NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Params getMetricTypeAndIndexParams = { NVPW_MetricsEvaluator_GetMetricTypeAndIndex_Params_STRUCT_SIZE };
        getMetricTypeAndIndexParams.pMetricsEvaluator = pMetricsEvaluator;
        getMetricTypeAndIndexParams.pMetricName = pMetricName;
        NVPA_Status status = NVPW_MetricsEvaluator_GetMetricTypeAndIndex(&getMetricTypeAndIndexParams);
        if (status != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_WRN(80, "NVPW_MetricsEvaluator_GetMetricTypeAndIndex failed\n");
            return false;
        }
        metricType = static_cast<NVPW_MetricType>(getMetricTypeAndIndexParams.metricType);
        metricIndex = getMetricTypeAndIndexParams.metricIndex;
        return true;
    }

    inline bool GetSupportedSubmetrics(NVPW_MetricsEvaluator* pMetricsEvaluator, NVPW_MetricType metricType, std::vector<NVPW_Submetric>& submetrics)
    {
        NVPW_MetricsEvaluator_GetSupportedSubmetrics_Params getSupportedSubmetrics = { NVPW_MetricsEvaluator_GetSupportedSubmetrics_Params_STRUCT_SIZE };
        getSupportedSubmetrics.pMetricsEvaluator = pMetricsEvaluator;
        getSupportedSubmetrics.metricType = static_cast<uint8_t>(metricType);
        NVPA_Status status = NVPW_MetricsEvaluator_GetSupportedSubmetrics(&getSupportedSubmetrics);
        if (status != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(80, "NVPW_MetricsEvaluator_GetSupportedSubmetrics failed for metric type: %u\n", getSupportedSubmetrics.metricType);
            return false;
        }
        submetrics.reserve(getSupportedSubmetrics.numSupportedSubmetrics);
        for (size_t ii = 0; ii < getSupportedSubmetrics.numSupportedSubmetrics; ++ii)
        {
            submetrics.push_back(static_cast<NVPW_Submetric>(getSupportedSubmetrics.pSupportedSubmetrics[ii]));   
        }
        return true;
    }

    inline bool MetricsEvaluatorSetDeviceAttributes(NVPW_MetricsEvaluator* pMetricsEvaluator, const uint8_t* pCounterDataImage, size_t counterDataImageSize)
    {
        NVPW_MetricsEvaluator_SetDeviceAttributes_Params setDeviceAttributesParams = { NVPW_MetricsEvaluator_SetDeviceAttributes_Params_STRUCT_SIZE };
        setDeviceAttributesParams.pMetricsEvaluator = pMetricsEvaluator;
        setDeviceAttributesParams.pCounterDataImage = pCounterDataImage;
        setDeviceAttributesParams.counterDataImageSize = counterDataImageSize;
        const NVPA_Status status = NVPW_MetricsEvaluator_SetDeviceAttributes(&setDeviceAttributesParams);
        if (status != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(50, "NVPW_MetricsEvaluator_SetDeviceAttributes failed\n");
            return false;
        }
        return true;
    }

    // Evaluate the named metrics from (CounterDataImage, rangeIndex) and store them in pMetricValues.
    inline bool EvaluateToGpuValues(
        NVPW_MetricsEvaluator* pMetricsEvaluator,
        const uint8_t* pCounterDataImage,
        size_t counterDataImageSize,
        size_t rangeIndex,
        size_t numMetricEvalRequests,
        const NVPW_MetricEvalRequest* pMetricEvalRequests,
        double* pMetricValues)
    {
        NVPW_MetricsEvaluator_EvaluateToGpuValues_Params evaluateToGpuValuesParams = { NVPW_MetricsEvaluator_EvaluateToGpuValues_Params_STRUCT_SIZE };
        evaluateToGpuValuesParams.pMetricsEvaluator = pMetricsEvaluator;
        evaluateToGpuValuesParams.pMetricEvalRequests = pMetricEvalRequests;
        evaluateToGpuValuesParams.numMetricEvalRequests = numMetricEvalRequests;
        evaluateToGpuValuesParams.metricEvalRequestStructSize = NVPW_MetricEvalRequest_STRUCT_SIZE;
        evaluateToGpuValuesParams.metricEvalRequestStrideSize = sizeof(NVPW_MetricEvalRequest);
        evaluateToGpuValuesParams.pCounterDataImage = pCounterDataImage;
        evaluateToGpuValuesParams.counterDataImageSize = counterDataImageSize;
        evaluateToGpuValuesParams.rangeIndex = rangeIndex;
        evaluateToGpuValuesParams.pMetricValues = pMetricValues;
        NVPA_Status status = NVPW_MetricsEvaluator_EvaluateToGpuValues(&evaluateToGpuValuesParams);
        if (status != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(80, "NVPW_MetricsEvaluator_EvaluateToGpuValues failed\n");
            return false;
        }
        return true;
    }

    inline bool operator==(const NVPW_DimUnitFactor& lhs, const NVPW_DimUnitFactor& rhs)
    {
        return (lhs.dimUnit == rhs.dimUnit) && (lhs.exponent == rhs.exponent);
    }

    inline bool operator<(const NVPW_DimUnitFactor& lhs, const NVPW_DimUnitFactor& rhs)
    {
        if (lhs.dimUnit != rhs.dimUnit)
        {
            return lhs.dimUnit < rhs.dimUnit;
        }
        if (lhs.exponent != rhs.exponent)
        {
            return lhs.exponent < rhs.exponent;
        }
        return false;
    }

    inline bool GetMetricDimUnits(NVPW_MetricsEvaluator* pMetricsEvaluator, const NVPW_MetricEvalRequest& metricRequest, std::vector<NVPW_DimUnitFactor>& dimUnits)
    {
        NVPW_MetricsEvaluator_GetMetricDimUnits_Params getMetricDimUnitsParams = { NVPW_MetricsEvaluator_GetMetricDimUnits_Params_STRUCT_SIZE };
        getMetricDimUnitsParams.pMetricsEvaluator = pMetricsEvaluator;
        getMetricDimUnitsParams.pMetricEvalRequest = &metricRequest;
        getMetricDimUnitsParams.metricEvalRequestStructSize = NVPW_MetricEvalRequest_STRUCT_SIZE;
        getMetricDimUnitsParams.dimUnitFactorStructSize = NVPW_DimUnitFactor_STRUCT_SIZE;
        NVPA_Status status = NVPW_MetricsEvaluator_GetMetricDimUnits(&getMetricDimUnitsParams);
        if (status != NVPA_STATUS_SUCCESS || !getMetricDimUnitsParams.numDimUnits)
        {
            NV_PERF_LOG_WRN(80, "NVPW_MetricsEvaluator_GetMetricDimUnits failed for metric = %s\n", ToString(pMetricsEvaluator, metricRequest).c_str());
            return false;
        }
        dimUnits.resize(getMetricDimUnitsParams.numDimUnits);
        getMetricDimUnitsParams.pDimUnits = dimUnits.data();
        status = NVPW_MetricsEvaluator_GetMetricDimUnits(&getMetricDimUnitsParams);
        if (status != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_WRN(80, "NVPW_MetricsEvaluator_GetMetricDimUnits failed for metric = %s\n", ToString(pMetricsEvaluator, metricRequest).c_str());
            return false;
        }
        return true;
    }

    inline const char* GetMetricDescription(NVPW_MetricsEvaluator* pMetricsEvaluator, NVPW_MetricType metricType, size_t metricIndex)
    {
        if (metricType == NVPW_METRIC_TYPE_COUNTER)
        {
            NVPW_MetricsEvaluator_GetCounterProperties_Params params{ NVPW_MetricsEvaluator_GetCounterProperties_Params_STRUCT_SIZE };
            params.pMetricsEvaluator = pMetricsEvaluator;
            params.counterIndex = metricIndex;
            NVPA_Status status = NVPW_MetricsEvaluator_GetCounterProperties(&params);
            if (status == NVPA_STATUS_SUCCESS)
            {
                return params.pDescription;
            }
        }
        else if (metricType == NVPW_METRIC_TYPE_RATIO)
        {
            NVPW_MetricsEvaluator_GetRatioMetricProperties_Params params{ NVPW_MetricsEvaluator_GetRatioMetricProperties_Params_STRUCT_SIZE };
            params.pMetricsEvaluator = pMetricsEvaluator;
            params.ratioMetricIndex = metricIndex;
            NVPA_Status status = NVPW_MetricsEvaluator_GetRatioMetricProperties(&params);
            if (status == NVPA_STATUS_SUCCESS)
            {
                return params.pDescription;
            }
        }
        else if (metricType == NVPW_METRIC_TYPE_THROUGHPUT)
        {
            NVPW_MetricsEvaluator_GetThroughputMetricProperties_Params params{ NVPW_MetricsEvaluator_GetThroughputMetricProperties_Params_STRUCT_SIZE };
            params.pMetricsEvaluator = pMetricsEvaluator;
            params.throughputMetricIndex = metricIndex;
            NVPA_Status status = NVPW_MetricsEvaluator_GetThroughputMetricProperties(&params);
            if (status == NVPA_STATUS_SUCCESS)
            {
                return params.pDescription;
            }
        }
        NV_PERF_LOG_WRN(50, "GetMetricDescription failed for metricType = %u, metricIndex = %u\n", (uint32_t)metricType, (uint32_t)metricIndex);
        return nullptr;
    }

    inline const char* ToCString(NVPW_MetricsEvaluator* pMetricsEvaluator, NVPW_HwUnit hwUnit)
    {
        NVPW_MetricsEvaluator_HwUnitToString_Params params{ NVPW_MetricsEvaluator_HwUnitToString_Params_STRUCT_SIZE };
        params.pMetricsEvaluator = pMetricsEvaluator;
        params.hwUnit = hwUnit;
        NVPA_Status status = NVPW_MetricsEvaluator_HwUnitToString(&params);
        if (status != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_WRN(50, "NVPW_MetricsEvaluator_HwUnitToString failed for hwUnit: %u\n", hwUnit);
            return nullptr;
        }
        return params.pHwUnitName;
    }

    inline NVPW_HwUnit GetMetricHwUnit(NVPW_MetricsEvaluator* pMetricsEvaluator, NVPW_MetricType metricType, size_t metricIndex)
    {
        if (metricType == NVPW_METRIC_TYPE_COUNTER)
        {
            NVPW_MetricsEvaluator_GetCounterProperties_Params params{ NVPW_MetricsEvaluator_GetCounterProperties_Params_STRUCT_SIZE };
            params.pMetricsEvaluator = pMetricsEvaluator;
            params.counterIndex = metricIndex;
            NVPA_Status status = NVPW_MetricsEvaluator_GetCounterProperties(&params);
            if (status == NVPA_STATUS_SUCCESS)
            {
                return static_cast<NVPW_HwUnit>(params.hwUnit);
            }
        }
        else if (metricType == NVPW_METRIC_TYPE_RATIO)
        {
            NVPW_MetricsEvaluator_GetRatioMetricProperties_Params params{ NVPW_MetricsEvaluator_GetRatioMetricProperties_Params_STRUCT_SIZE };
            params.pMetricsEvaluator = pMetricsEvaluator;
            params.ratioMetricIndex = metricIndex;
            NVPA_Status status = NVPW_MetricsEvaluator_GetRatioMetricProperties(&params);
            if (status == NVPA_STATUS_SUCCESS)
            {
                return static_cast<NVPW_HwUnit>(params.hwUnit);
            }
        }
        else if (metricType == NVPW_METRIC_TYPE_THROUGHPUT)
        {
            NVPW_MetricsEvaluator_GetThroughputMetricProperties_Params params{ NVPW_MetricsEvaluator_GetThroughputMetricProperties_Params_STRUCT_SIZE };
            params.pMetricsEvaluator = pMetricsEvaluator;
            params.throughputMetricIndex = metricIndex;
            NVPA_Status status = NVPW_MetricsEvaluator_GetThroughputMetricProperties(&params);
            if (status == NVPA_STATUS_SUCCESS)
            {
                return static_cast<NVPW_HwUnit>(params.hwUnit);
            }
        }
        NV_PERF_LOG_WRN(50, "GetMetricHwUnit failed for metricType = %u, metricIndex = %u\n", (uint32_t)metricType, (uint32_t)metricIndex);
        return NVPW_HW_UNIT_INVALID;
    }

    inline const char* GetMetricHwUnitStr(NVPW_MetricsEvaluator* pMetricsEvaluator, NVPW_MetricType metricType, size_t metricIndex)
    {
        const NVPW_HwUnit hwUnit = GetMetricHwUnit(pMetricsEvaluator, metricType, metricIndex);
        const char* pHwUnitStr = ToCString(pMetricsEvaluator, hwUnit);
        return pHwUnitStr;
    }

    inline const char* ToCString(NVPW_MetricsEvaluator* pMetricsEvaluator, NVPW_DimUnitName dimUnit, bool plural)
    {
        NVPW_MetricsEvaluator_DimUnitToString_Params dimUnitToStringParams = { NVPW_MetricsEvaluator_DimUnitToString_Params_STRUCT_SIZE };
        dimUnitToStringParams.pMetricsEvaluator = pMetricsEvaluator;
        dimUnitToStringParams.dimUnit = static_cast<uint32_t>(dimUnit);
        NVPA_Status status = NVPW_MetricsEvaluator_DimUnitToString(&dimUnitToStringParams);
        if (status != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_WRN(80, "NVPW_MetricsEvaluator_DimUnitToString failed for dimUnit = %u\n", dimUnit);
            return "";
        }
        const char* pDimUnitStr = plural? dimUnitToStringParams.pPluralName : dimUnitToStringParams.pSingularName;
        return pDimUnitStr;
    }

    // `getDimUnitStrFunctor` must be in the form of const char*(NVPW_DimUnitName dimUnit, bool plural)
    template <typename GetDimUnitStrFunctor>
    inline std::string ToString(const std::vector<NVPW_DimUnitFactor>& dimUnitFactors, GetDimUnitStrFunctor&& getDimUnitStrFunctor)
    {
        if (dimUnitFactors.empty())
        {
            return "<unitless>";
        }

        std::stringstream sstream;
        size_t numeratorCount = 0;
        size_t denominatorCount = 0;
        auto isNumerator = [](const NVPW_DimUnitFactor& dimUnitFactor) {
            return dimUnitFactor.exponent > 0;
        };
        // if printNumerator == false, print the denominator
        auto printFormattedDimUnits = [&](size_t count, bool printNumerator) {
            if (count > 1)
            {
                sstream << "(";
            }
            bool isFirst = true;
            for (const NVPW_DimUnitFactor& dimUnitFactor : dimUnitFactors)
            {
                if (printNumerator != isNumerator(dimUnitFactor))
                {
                    continue;
                }

                if (!isFirst)
                {
                    sstream << " * ";
                }
                const bool plural = printNumerator;
                sstream << getDimUnitStrFunctor(static_cast<NVPW_DimUnitName>(dimUnitFactor.dimUnit), plural);
                if (std::abs(dimUnitFactor.exponent) != 1)
                {
                    sstream << "^" << (uint32_t)std::abs(dimUnitFactor.exponent);
                }
                isFirst = false;
            }
            if (count > 1)
            {
                sstream << ")";
            }
        };

        for (const NVPW_DimUnitFactor& dimUnitFactor : dimUnitFactors)
        {
            isNumerator(dimUnitFactor) ? ++numeratorCount : ++denominatorCount;
        }

        if (numeratorCount)
        {
            const bool printNumerator = true;
            printFormattedDimUnits(numeratorCount, printNumerator);
        }
        else
        {
            sstream << "1";
        }

        if (denominatorCount)
        {
            sstream << " / ";
            const bool printNumerator = false;
            printFormattedDimUnits(denominatorCount, printNumerator);
        }
        return sstream.str();
    }

}}
