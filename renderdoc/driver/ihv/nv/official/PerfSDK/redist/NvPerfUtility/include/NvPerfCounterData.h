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

#include "nvperf_host.h"
#include "nvperf_target.h"
#include "NvPerfInit.h"
#include <algorithm>
#include <string>
#include <vector>

namespace nv { namespace perf {

    class CounterDataCombiner
    {
    private:
        std::vector<uint8_t> m_counterData;
        NVPA_CounterDataCombiner* m_pCounterDataCombiner;

    public:
        CounterDataCombiner()
            : m_pCounterDataCombiner()
        {
        }
        CounterDataCombiner(const CounterDataCombiner& combiner) = delete;
        CounterDataCombiner& operator=(const CounterDataCombiner& combiner) = delete;
        CounterDataCombiner(CounterDataCombiner&& combiner)
            : m_counterData(std::move(combiner.m_counterData))
            , m_pCounterDataCombiner(combiner.m_pCounterDataCombiner)
        {
            combiner.m_pCounterDataCombiner = nullptr;
        }
        CounterDataCombiner& operator=(CounterDataCombiner&& combiner)
        {
            Reset();
            m_counterData = std::move(combiner.m_counterData);
            m_pCounterDataCombiner = combiner.m_pCounterDataCombiner;
            combiner.m_pCounterDataCombiner = nullptr;
            return *this;
        }
        ~CounterDataCombiner()
        {
            Reset();
        }

        // The session type of "this" counter data is determined by the session type of the counter data and the prefix that are passed in.
        bool Initialize(
            const uint8_t* pCounterDataPrefix,
            size_t counterDataPrefixSize,
            uint32_t maxNumRanges,
            uint32_t maxNumRangeTreeNodes,
            uint32_t maxRangeNameLength,
            const uint8_t* pCounterDataSrc)
        {
            {
                NVPW_CounterData_CalculateCounterDataImageCopySize_Params calculateCounterDataImageCopySizeParams = { NVPW_CounterData_CalculateCounterDataImageCopySize_Params_STRUCT_SIZE };
                calculateCounterDataImageCopySizeParams.pCounterDataPrefix = pCounterDataPrefix;
                calculateCounterDataImageCopySizeParams.counterDataPrefixSize = counterDataPrefixSize;
                calculateCounterDataImageCopySizeParams.maxNumRanges = maxNumRanges;
                calculateCounterDataImageCopySizeParams.maxNumRangeTreeNodes = maxNumRangeTreeNodes;
                calculateCounterDataImageCopySizeParams.maxRangeNameLength = maxRangeNameLength;
                calculateCounterDataImageCopySizeParams.pCounterDataSrc = pCounterDataSrc;
                NVPA_Status nvpaStatus = NVPW_CounterData_CalculateCounterDataImageCopySize(&calculateCounterDataImageCopySizeParams);
                if (nvpaStatus != NVPA_STATUS_SUCCESS)
                {
                    NV_PERF_LOG_ERR(50, "NVPW_CounterData_CalculateCounterDataImageCopySize failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                    return false;
                }
                m_counterData.resize(calculateCounterDataImageCopySizeParams.copyDataImageCounterSize);
            }
            {
                NVPW_CounterData_InitializeCounterDataImageCopy_Params initializeCounterDataImageCopyParams = { NVPW_CounterData_InitializeCounterDataImageCopy_Params_STRUCT_SIZE };
                initializeCounterDataImageCopyParams.pCounterDataPrefix = pCounterDataPrefix;
                initializeCounterDataImageCopyParams.counterDataPrefixSize = counterDataPrefixSize;
                initializeCounterDataImageCopyParams.maxNumRanges = maxNumRanges;
                initializeCounterDataImageCopyParams.maxNumRangeTreeNodes = maxNumRangeTreeNodes;
                initializeCounterDataImageCopyParams.maxRangeNameLength = maxRangeNameLength;
                initializeCounterDataImageCopyParams.pCounterDataSrc = pCounterDataSrc;
                initializeCounterDataImageCopyParams.pCounterDataDst = m_counterData.data();
                NVPA_Status nvpaStatus = NVPW_CounterData_InitializeCounterDataImageCopy(&initializeCounterDataImageCopyParams);
                if (nvpaStatus != NVPA_STATUS_SUCCESS)
                {
                    NV_PERF_LOG_ERR(50, "NVPW_CounterData_InitializeCounterDataImageCopy failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                    return false;
                }
            }
            {
                NVPW_CounterDataCombiner_Create_Params counterDataCombinerCreateParams = { NVPW_CounterDataCombiner_Create_Params_STRUCT_SIZE };
                counterDataCombinerCreateParams.pCounterDataDst = m_counterData.data();
                NVPA_Status nvpaStatus = NVPW_CounterDataCombiner_Create(&counterDataCombinerCreateParams);
                if (nvpaStatus != NVPA_STATUS_SUCCESS)
                {
                    NV_PERF_LOG_ERR(50, "NVPW_CounterDataCombiner_Create failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                    return false;
                }
                m_pCounterDataCombiner = counterDataCombinerCreateParams.pCounterDataCombiner;
            }
            return true;            
        }

        // This particular version of the Initialize() function is designed for use with periodic sampler sessions, which do not require the `maxNumRangeTreeNodes`
        // and `maxRangeNameLength` parameters.
        bool Initialize(
            const uint8_t* pCounterDataPrefix,
            size_t counterDataPrefixSize,
            uint32_t maxNumRanges,
            const uint8_t* pCounterDataSrc)
        {
            return Initialize(pCounterDataPrefix, counterDataPrefixSize, maxNumRanges, 0, 0, pCounterDataSrc);   
        }

        void Reset()
        {
            if (m_pCounterDataCombiner)
            {
                NVPW_CounterDataCombiner_Destroy_Params counterDataCombinerDestroyParams = {NVPW_CounterDataCombiner_Destroy_Params_STRUCT_SIZE};
                counterDataCombinerDestroyParams.pCounterDataCombiner = m_pCounterDataCombiner;
                NVPA_Status nvpaStatus = NVPW_CounterDataCombiner_Destroy(&counterDataCombinerDestroyParams);
                if (nvpaStatus != NVPA_STATUS_SUCCESS)
                {
                    NV_PERF_LOG_ERR(50, "NVPW_CounterDataCombiner_Destroy failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                }
                m_pCounterDataCombiner = nullptr;
            }
            m_counterData.clear();
        }

        std::vector<uint8_t>& GetCounterData()
        {
            return m_counterData;
        }

        const std::vector<uint8_t>& GetCounterData() const
        {
            return m_counterData;
        }

        bool CreateRange(const char* const* ppDescriptions, size_t numDescriptions, size_t& rangeIndexDst)
        {
            NVPW_CounterDataCombiner_CreateRange_Params createRangeParams = { NVPW_CounterDataCombiner_CreateRange_Params_STRUCT_SIZE };
            createRangeParams.pCounterDataCombiner = m_pCounterDataCombiner;
            createRangeParams.ppDescriptions = ppDescriptions;
            createRangeParams.numDescriptions = numDescriptions;
            NVPA_Status nvpaStatus = NVPW_CounterDataCombiner_CreateRange(&createRangeParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "NVPW_CounterDataCombiner_CreateRange failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            rangeIndexDst = createRangeParams.rangeIndexDst;
            return true;
        }

        // If the session type is periodic sampler, the description of the range is not required as ranges are anonymous there.
        bool CreateRange(size_t& rangeIndexDst)
        {
            return CreateRange(nullptr, 0, rangeIndexDst);
        }

        bool CopyIntoRange(size_t rangeIndexDst, const uint8_t* pCounterDataSrc, size_t rangeIndexSrc)
        {
            NVPW_CounterDataCombiner_CopyIntoRange_Params copyIntoRangeParams = { NVPW_CounterDataCombiner_CopyIntoRange_Params_STRUCT_SIZE };
            copyIntoRangeParams.pCounterDataCombiner = m_pCounterDataCombiner;
            copyIntoRangeParams.rangeIndexDst = rangeIndexDst;
            copyIntoRangeParams.pCounterDataSrc = pCounterDataSrc;
            copyIntoRangeParams.rangeIndexSrc = rangeIndexSrc;
            NVPA_Status nvpaStatus = NVPW_CounterDataCombiner_CopyIntoRange(&copyIntoRangeParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "NVPW_CounterDataCombiner_CopyIntoRange failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }

        // This function is for range profiler session type only.
        bool AccumulateIntoRange(
            size_t rangeIndexDst,
            uint32_t dstMultiplier,
            const uint8_t* pCounterDataSrc,
            size_t rangeIndexSrc,
            uint32_t srcMultiplier)
        {
            NVPW_CounterDataCombiner_AccumulateIntoRange_Params accumulateIntoRangeParams = { NVPW_CounterDataCombiner_AccumulateIntoRange_Params_STRUCT_SIZE };
            accumulateIntoRangeParams.pCounterDataCombiner = m_pCounterDataCombiner;
            accumulateIntoRangeParams.rangeIndexDst = rangeIndexDst;
            accumulateIntoRangeParams.dstMultiplier = dstMultiplier;
            accumulateIntoRangeParams.pCounterDataSrc = pCounterDataSrc;
            accumulateIntoRangeParams.rangeIndexSrc = rangeIndexSrc;
            accumulateIntoRangeParams.srcMultiplier = srcMultiplier;
            NVPA_Status nvpaStatus = NVPW_CounterDataCombiner_AccumulateIntoRange(&accumulateIntoRangeParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "NVPW_CounterDataCombiner_AccumulateIntoRange failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }

        bool SumIntoRange(
            size_t rangeIndexDst,
            const uint8_t* pCounterDataSrc,
            size_t rangeIndexSrc)
        {
            NVPW_CounterDataCombiner_SumIntoRange_Params sumIntoRangeParams = { NVPW_CounterDataCombiner_SumIntoRange_Params_STRUCT_SIZE };
            sumIntoRangeParams.pCounterDataCombiner = m_pCounterDataCombiner;
            sumIntoRangeParams.rangeIndexDst = rangeIndexDst;
            sumIntoRangeParams.pCounterDataSrc = pCounterDataSrc;
            sumIntoRangeParams.rangeIndexSrc = rangeIndexSrc;
            NVPA_Status nvpaStatus = NVPW_CounterDataCombiner_SumIntoRange(&sumIntoRangeParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "NVPW_CounterDataCombiner_SumIntoRange failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }

        bool WeightedSumIntoRange(
            size_t rangeIndexDst,
            double dstMultiplier,
            const uint8_t* pCounterDataSrc,
            size_t rangeIndexSrc,
            double srcMultiplier)
        {
            NVPW_CounterDataCombiner_WeightedSumIntoRange_Params weightedSumIntoRangeParams = { NVPW_CounterDataCombiner_WeightedSumIntoRange_Params_STRUCT_SIZE };
            weightedSumIntoRangeParams.pCounterDataCombiner = m_pCounterDataCombiner;
            weightedSumIntoRangeParams.rangeIndexDst = rangeIndexDst;
            weightedSumIntoRangeParams.dstMultiplier = dstMultiplier;
            weightedSumIntoRangeParams.pCounterDataSrc = pCounterDataSrc;
            weightedSumIntoRangeParams.rangeIndexSrc = rangeIndexSrc;
            weightedSumIntoRangeParams.srcMultiplier = srcMultiplier;
            NVPA_Status nvpaStatus = NVPW_CounterDataCombiner_WeightedSumIntoRange(&weightedSumIntoRangeParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "NVPW_CounterDataCombiner_WeightedSumIntoRange failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }

        static double GetOverlapFactor(
            uint64_t sampleBeginTime,
            uint64_t sampleEndTime,
            uint64_t targetBeginTime,
            uint64_t targetEndTime)
        {
            assert(sampleBeginTime < sampleEndTime);
            assert(targetBeginTime < targetEndTime);
            if (sampleBeginTime >= targetEndTime || sampleEndTime <= targetBeginTime)
            {
                return 0.0;
            }
            if (sampleBeginTime >= targetBeginTime && sampleEndTime <= targetEndTime)
            {
                return 1.0;
            }
            const uint64_t overlapDuration = [&]() {
                if (sampleBeginTime >= targetBeginTime)
                {
                    return targetEndTime - sampleBeginTime;
                }
                else if (sampleEndTime <= targetEndTime)
                {
                    return sampleEndTime - targetBeginTime;
                }
                else
                {
                    return targetEndTime - targetBeginTime;
                }
            }();
            return (double)overlapDuration / double(sampleEndTime - sampleBeginTime);
        }

        // This is a hybrid function that smartly determines which code path to invoke based on the timestamps.
        // If a sample fully resides within the interval defined by the target begin and end timestamps, the function will use the fast path
        // by directly invoking SumIntoRange(). This avoids the overhead of interpolation and provides optimal performance.
        // On the other hand, if a sample only partially overlaps with the interval, the function will first use interpolation to estimate
        // the portion of the sample that falls within the interval and then invoke WeightedSumIntoRange().
        bool SumIntoRange(
            size_t rangeIndexDst,
            const uint8_t* pCounterDataSrc,
            size_t rangeIndexSrc,
            uint64_t sampleBeginTime,
            uint64_t sampleEndTime,
            uint64_t targetBeginTime,
            uint64_t targetEndTime)
        {
            bool success = true;
            if (sampleBeginTime >= targetBeginTime && sampleEndTime <= targetEndTime)
            {
                success = SumIntoRange(rangeIndexDst, pCounterDataSrc, rangeIndexSrc);
            }
            else
            {
                const double srcMultiplier = GetOverlapFactor(sampleBeginTime, sampleEndTime, targetBeginTime, targetEndTime);
                const double dstMultiplier = 1.0;
                success = WeightedSumIntoRange(
                    rangeIndexDst,
                    dstMultiplier,
                    pCounterDataSrc,
                    rangeIndexSrc,
                    srcMultiplier);
            }
            if (!success)
            {
                return false;
            }
            return true;
        }
    };

    inline size_t CounterDataGetNumRanges(const uint8_t* pCounterDataImage)
    {
        NVPW_CounterData_GetNumRanges_Params getNumRangeParams = { NVPW_CounterData_GetRangeDescriptions_Params_STRUCT_SIZE };
        getNumRangeParams.pCounterDataImage = pCounterDataImage;
        NVPA_Status nvpaStatus = NVPW_CounterData_GetNumRanges(&getNumRangeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(50, "NVPW_CounterData_GetNumRanges failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return 0;
        }
        return getNumRangeParams.numRanges;
    }

    inline bool ExtractCounterDataPrefixFromCounterData(const uint8_t* pCounterDataImage, size_t counterDataImageSize, std::vector<uint8_t>& counterDataPrefixExtracted)
    {
        NVPW_CounterData_ExtractCounterDataPrefix_Params extractCounterDataPrefixParams = { NVPW_CounterData_ExtractCounterDataPrefix_Params_STRUCT_SIZE };
        extractCounterDataPrefixParams.pCounterDataSrc = pCounterDataImage;
        extractCounterDataPrefixParams.counterDataSrcSize = counterDataImageSize;
        NVPA_Status nvpaStatus = NVPW_CounterData_ExtractCounterDataPrefix(&extractCounterDataPrefixParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(50, "NVPW_CounterData_ExtractCounterDataPrefix failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return false;
        }
        counterDataPrefixExtracted.resize(extractCounterDataPrefixParams.counterDataPrefixSize);
        extractCounterDataPrefixParams.pCounterDataPrefix = counterDataPrefixExtracted.data();
        extractCounterDataPrefixParams.counterDataPrefixSize = counterDataPrefixExtracted.size();
        nvpaStatus = NVPW_CounterData_ExtractCounterDataPrefix(&extractCounterDataPrefixParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(50, "NVPW_CounterData_ExtractCounterDataPrefix failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return false;
        }
        return true;
    }

    namespace profiler {

        // TODO: this function performs dynamic allocations; either need a non-malloc'ing variant, or move this to an appropriate place
        inline std::string CounterDataGetRangeName(const uint8_t* pCounterDataImage, size_t rangeIndex, char delimiter, const char** ppLeafName = nullptr)
        {
            std::string rangeName;

            NVPW_CounterData_GetRangeDescriptions_Params params = { NVPW_CounterData_GetRangeDescriptions_Params_STRUCT_SIZE };
            params.pCounterDataImage = pCounterDataImage;
            params.rangeIndex = rangeIndex;
            NVPA_Status nvpaStatus = NVPW_CounterData_GetRangeDescriptions(&params);
            if (nvpaStatus)
            {
                return "";
            }

            if (!params.numDescriptions)
            {
                return "";
            }

            std::vector<const char*> descriptions;
            descriptions.resize(params.numDescriptions);
            params.ppDescriptions = descriptions.data();
            nvpaStatus = NVPW_CounterData_GetRangeDescriptions(&params);
            if (nvpaStatus)
            {
                return "";
            }

            rangeName += descriptions[0];
            for (size_t descriptionIdx = 1; descriptionIdx < params.numDescriptions; ++descriptionIdx)
            {
                const char* pDescription = params.ppDescriptions[descriptionIdx];
                rangeName += delimiter;
                rangeName += pDescription;
            }

            if (ppLeafName)
            {
                *ppLeafName = descriptions.back();
            }

            return rangeName;
        }

    } // namespace profiler


    namespace sampler {

        struct SampleTimestamp
        {
            uint64_t start = 0;
            uint64_t end = 0;
        };

        struct CounterDataInfo
        {
            uint32_t numTotalRanges = 0;
            uint32_t numPopulatedRanges = 0;
            uint32_t numCompletedRanges = 0;
        };

        inline bool CounterDataGetSampleTime(const uint8_t* pCounterDataImage, size_t rangeIndex, SampleTimestamp& timestamp);
        inline bool CounterDataGetTriggerCount(const uint8_t* pCounterDataImage, size_t counterDataImageSize, size_t rangeIndex, uint32_t& triggerCount);
        inline bool CounterDataGetInfo(const uint8_t* pCounterDataImage, size_t counterDataImageSize, CounterDataInfo& counterDataInfo);

        // this class assumes trigger counts are continuous(in order to quickly determine whether wraparound has occured), this is true if all triggers are in the same sampling range
        class RingBufferCounterData
        {
        protected:
            struct RangeDataIndexDescriptor
            {
                uint32_t rangeDataIndex;
                uint32_t triggerCount;
            };

            enum class InvalidValues: uint32_t
            {
                TriggerCount = 0,
                RangeDataIndex = 0xFFFFFFFF,
            };
            std::vector<uint8_t> m_counterDataImage;
            uint32_t m_numTotalRanges; // total number of allocated ranges in counter data
            RangeDataIndexDescriptor m_get; // the last consumed range data index
            RangeDataIndexDescriptor m_put; // the last produced range data index
            bool m_validate; // perform additional validation at the cost of perf, useful for debugging

        public:
            RingBufferCounterData()
                : m_numTotalRanges()
                , m_get({ (uint32_t)InvalidValues::RangeDataIndex, (uint32_t)InvalidValues::TriggerCount })
                , m_put({ (uint32_t)InvalidValues::RangeDataIndex, (uint32_t)InvalidValues::TriggerCount })
                , m_validate(false)
            {
            }
            RingBufferCounterData(const RingBufferCounterData& counterData) = delete;
            RingBufferCounterData& operator=(const RingBufferCounterData& counterData) = delete;
            RingBufferCounterData(RingBufferCounterData&& counterData) = default;
            RingBufferCounterData& operator=(RingBufferCounterData&& counterData) = default;
            virtual ~RingBufferCounterData() = default;

            // set "validate" to true for additional validation, at the cost of perf, useful for debugging
            // "createCounterDataFn" is expected to be in the form of bool(uint32_t, NVPW_PeriodicSampler_CounterData_AppendMode, std::vector<uint8_>&)
            template <class TCreateCounterDataFn>
            bool Initialize(uint32_t maxTriggerLatency, bool validate, TCreateCounterDataFn&& createCounterDataFn)
            {
                if (!createCounterDataFn(maxTriggerLatency, NVPW_PERIODIC_SAMPLER_COUNTER_DATA_APPEND_MODE_CIRCULAR, m_counterDataImage))
                {
                    return false;
                }
                m_numTotalRanges = maxTriggerLatency;
                m_validate = validate;
                return true;
            }

            void Reset()
            {
                m_counterDataImage.clear();
                m_numTotalRanges = 0;
                m_get = { (uint32_t)InvalidValues::RangeDataIndex, (uint32_t)InvalidValues::TriggerCount };
                m_put = { (uint32_t)InvalidValues::RangeDataIndex, (uint32_t)InvalidValues::TriggerCount };
                m_validate = false;
            }

            const std::vector<uint8_t>& GetCounterData() const
            {
                return m_counterDataImage;
            }

            std::vector<uint8_t>& GetCounterData()
            {
                return m_counterDataImage;
            }

            bool UpdatePut()
            {
                uint32_t numPopulatedRanges = 0;
                uint32_t numCompletedRanges = 0;
                bool success = GetLatestInfo(numPopulatedRanges, numCompletedRanges);
                if (!success)
                {
                    return false;
                }
                if (!numCompletedRanges)
                {
                    return true; // no range completed yet, early return
                }

                const uint32_t lastAcquiredRangeIndex = numPopulatedRanges - 1;
                const uint32_t lastCompletedRangeIndex = numCompletedRanges - 1;

                if (m_validate)
                {
                    if (!numPopulatedRanges)
                    {
                        NV_PERF_LOG_ERR(50, "Internal error: 'numCompletedRanges' is non-zero but 'numPopulatedRanges' is zero\n");
                        return false;
                    }
                    // check PUT has not beaten GET for one round, otherwise we've lost data
                    uint32_t lastAcquiredRangeTriggerCount = 0;
                    success = GetTriggerCount(lastAcquiredRangeIndex, lastAcquiredRangeTriggerCount);
                    if (!success)
                    {
                        return false;
                    }
                    const bool dataLost = (m_get.triggerCount != (uint32_t)InvalidValues::TriggerCount) && ((lastAcquiredRangeTriggerCount - m_get.triggerCount) > m_numTotalRanges);
                    if (dataLost)
                    {
                        NV_PERF_LOG_ERR(50, "PUT has beaten GET for one round, 'maxTriggerLatency' specified is not sufficient to cover the latency\n");
                        return false;
                    }
                }

                m_put.rangeDataIndex = lastCompletedRangeIndex;
                success = GetTriggerCount(lastCompletedRangeIndex, m_put.triggerCount);
                if (!success)
                {
                    return false;
                }
                return true;
            }

            bool UpdateGet(uint32_t numRangesConsumed)
            {
                if (m_validate)
                {
                    if (numRangesConsumed > GetNumUnreadRanges())
                    {
                        return false;
                    }
                }
                if (m_get.triggerCount == (uint32_t)InvalidValues::TriggerCount)
                {
                    m_get.rangeDataIndex = numRangesConsumed - 1;
                    m_get.triggerCount = m_put.triggerCount - Distance(m_get.rangeDataIndex, m_put.rangeDataIndex);
                }
                else
                {
                    m_get.rangeDataIndex = CircularIncrement(m_get.rangeDataIndex, numRangesConsumed);
                    m_get.triggerCount += numRangesConsumed;
                    if (m_validate)
                    {
                        uint32_t queriedGetTriggerCount = 0;
                        bool success = GetTriggerCount(m_get.rangeDataIndex, queriedGetTriggerCount);
                        if (!success)
                        {
                            return false;
                        }
                        if (queriedGetTriggerCount != m_get.triggerCount)
                        {
                            return false;
                        }
                        if (m_put.triggerCount < m_get.triggerCount)
                        {
                            return false;
                        }
                    }
                }
                return true;
            }

            uint32_t GetNumUnreadRanges() const
            {
                if (m_put.triggerCount == (uint32_t)InvalidValues::TriggerCount)
                {
                    return 0;
                }
                else if (m_get.triggerCount == (uint32_t)InvalidValues::TriggerCount)
                {
                    return m_put.rangeDataIndex + 1;
                }
                else
                {                
                    return m_put.triggerCount - m_get.triggerCount;
                }
            }

            // TConsumeRangeDataFunc should be in the form of bool(const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex, bool& stop),
            // return false to indicate something went wrong; set "stop" to true to early break from iterating succeeding unread ranges
            // note this function doesn’t update GET in the end, it requires client calling UpdateGet()
            template <typename TConsumeRangeDataFunc>
            bool ConsumeData(TConsumeRangeDataFunc&& consumeRangeDataFunc) const
            {
                const uint32_t numUnreadRanges = GetNumUnreadRanges();
                for (uint32_t ii = 0; ii < numUnreadRanges; ++ii)
                {
                    const uint32_t rangeIndex = CircularIncrement(m_get.rangeDataIndex, ii + 1);
                    bool stop = false;
                    const bool success = consumeRangeDataFunc(m_counterDataImage.data(), m_counterDataImage.size(), rangeIndex, stop);
                    if (!success)
                    {
                        return false;
                    }
                    if (stop)
                    {
                        return true;
                    }
                }
                return true;
            }

            uint32_t CircularIncrement(uint32_t current, uint32_t stepSize) const
            {
                if (!m_numTotalRanges)
                {
                    return 0;
                }
                if (stepSize > m_numTotalRanges)
                {
                    stepSize = stepSize % m_numTotalRanges;
                }

                if (m_numTotalRanges - current > stepSize)
                {
                    return current + stepSize;
                }
                else
                {
                    return stepSize - (m_numTotalRanges - current);
                }
            }

            uint32_t Distance(uint32_t first, uint32_t last) const
            {
                if (!m_numTotalRanges)
                {
                    return 0;
                }
                if (last >= first)
                {
                    return last - first;
                }
                else
                {
                    return last + m_numTotalRanges - first;
                }
            }

            virtual bool GetTriggerCount(uint32_t rangeIndex, uint32_t& triggerCount) const
            {
                return CounterDataGetTriggerCount(m_counterDataImage.data(), m_counterDataImage.size(), rangeIndex, triggerCount);
            }

            virtual bool GetLatestInfo(uint32_t& numPopulatedRanges, uint32_t& numCompletedRanges) const
            {
                CounterDataInfo counterDataInfo{};
                const bool success = CounterDataGetInfo(m_counterDataImage.data(), m_counterDataImage.size(), counterDataInfo);
                if (!success)
                {
                    return false;
                }
                numPopulatedRanges = counterDataInfo.numPopulatedRanges;
                numCompletedRanges = counterDataInfo.numCompletedRanges;
                return true;
            }
        };

        // The `CombinedCounterDataMaxNumRanges` parameter defines the maximum number of ranges that can be stored in the combined counter data.
        // This parameter is used to determine how frequently a new combined counter data object should be initialized.
        template <size_t CombinedCounterDataMaxNumRanges = 1024>
        class SampleCombiner
        {
        public:
            struct SampleInfo
            {
                uint64_t beginTimestamp;
                uint64_t endTimestamp;
                const uint8_t* pCounterData;
                uint32_t rangeIndex;
            };

        protected:
            CounterDataCombiner m_combiner;
            uint32_t m_combinedCounterDataRangeIndex;
            std::vector<uint8_t> m_counterDataTemplate; // used for fast initialization(memcpy)

        public:
            SampleCombiner()
                : m_combinedCounterDataRangeIndex((uint32_t)-1)
            {
            }
            SampleCombiner(const SampleCombiner& combiner) = delete;
            SampleCombiner& operator=(const SampleCombiner& combiner) = delete;
            SampleCombiner(SampleCombiner&& combiner) = default;
            SampleCombiner& operator=(SampleCombiner&& combiner) = default;
            ~SampleCombiner() = default;

            bool Initialize(const std::vector<uint8_t>& counterDataPrefix, const std::vector<uint8_t>& counterDataSource)
            {
                bool success = m_combiner.Initialize(
                    counterDataPrefix.data(),
                    counterDataPrefix.size(), 
                    CombinedCounterDataMaxNumRanges,
                    counterDataSource.data());
                if (!success)
                {
                    return false;
                }
                for (size_t rangeIndex = 0; rangeIndex < CombinedCounterDataMaxNumRanges; ++rangeIndex)
                {
                    if (!m_combiner.CreateRange(rangeIndex))
                    {
                        return false;
                    }
                }
                m_counterDataTemplate = m_combiner.GetCounterData();
                return true;
            }

            void Reset()
            {
                m_combiner.Reset();
                m_combinedCounterDataRangeIndex = (uint32_t)-1;
                m_counterDataTemplate.clear();
            }

            // This function takes a list of samples, a begin timestamp, and an end timestamp. The objective is to iterate through each sample and sum 
            // its value if it lies within the interval defined by the begin and end timestamps. If only a portion of the sample falls within this
            // interval, interpolation will be applied.
            // Once the sample combination is complete, the function will call the user-provided callback with the combined sample.
            // The samples passed to this function are expected to be in ascending order, with each subsequent sample having a timestamp greater than
            // its predecessor. 
            // TConsumeDataFunc should be in the form of bool(const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex),
            // return false to indicate something went wrong. Note if there are no overlapping samples, `consumeDataFunc` will not be invoked.
            template <typename TConsumeDataFunc>
            bool MergeSamples(const std::vector<SampleInfo>& samples, uint64_t beginTime, uint64_t endTime, TConsumeDataFunc&& consumeDataFunc)
            {
                if (samples.empty() || samples.back().endTimestamp <= beginTime || endTime <= samples.front().beginTimestamp)
                {
                    return true; // no overlapping samples
                }

                ++m_combinedCounterDataRangeIndex;
                if (m_combinedCounterDataRangeIndex == CombinedCounterDataMaxNumRanges)
                {
                    m_combiner.GetCounterData() = m_counterDataTemplate;
                    m_combinedCounterDataRangeIndex = 0;
                }

                auto overlapSampleBeginItr = std::upper_bound(samples.begin(), samples.end(), beginTime, [&](uint64_t timestamp, const SampleInfo& sample) {
                    return sample.endTimestamp > timestamp;
                });
                auto overlapSampleEndItr = std::upper_bound(samples.begin(), samples.end(), endTime, [&](uint64_t timestamp, const SampleInfo& sample) {
                    return sample.beginTimestamp > timestamp;
                });
                for (auto itr = overlapSampleBeginItr; itr < overlapSampleEndItr; ++itr)
                {
                    if (!m_combiner.SumIntoRange(
                            m_combinedCounterDataRangeIndex,
                            itr->pCounterData,
                            itr->rangeIndex,
                            itr->beginTimestamp,
                            itr->endTimestamp,
                            beginTime,
                            endTime))
                    {
                        return false;
                    }
                }

                if (!consumeDataFunc(m_combiner.GetCounterData().data(), m_combiner.GetCounterData().size(), m_combinedCounterDataRangeIndex))
                {
                    return false;
                }
                return true;
            }
        };

        class FrameLevelSampleCombiner
        {
        private:
            struct SampleInfo
            {
                uint64_t beginTimestamp;
                uint64_t endTimestamp;
                const uint8_t* pCounterData;
                uint32_t rangeIndex;
            };
        public:
            struct FrameInfo
            {
                uint64_t beginTimestamp;
                uint64_t endTimestamp;
                size_t numSamplesInFrame;
                const uint8_t* pCombinedCounterData;
                size_t combinedCounterDataSize;
                uint32_t combinedCounterDataRangeIndex;
            };
        private:
            enum {
                // The `CombinedCounterDataMaxNumRanges` parameter defines the maximum number of ranges that can be stored in the combined counter data.
                // This parameter is used to determine how frequently a new combined counter data object should be initialized.
                CombinedCounterDataMaxNumRanges = 1024,
            };
            CounterDataCombiner m_combiner;
            uint32_t m_combinedCounterDataRangeIndex;
            std::vector<uint8_t> m_counterDataTemplate; // used for fast initialization(memcpy)

            // sample desc ring buffer
            std::vector<SampleInfo> m_sampleInfoRingBuffer;
            size_t m_putIndex;
            size_t m_getIndex;
            size_t m_numUnreadSamples;

            uint64_t m_frameBeginTime;
        public:
            FrameLevelSampleCombiner()
                : m_combinedCounterDataRangeIndex()
                , m_putIndex()
                , m_getIndex()
                , m_numUnreadSamples()
                , m_frameBeginTime()
            {
            }
            FrameLevelSampleCombiner(const FrameLevelSampleCombiner& combiner) = delete;
            FrameLevelSampleCombiner& operator=(const FrameLevelSampleCombiner& combiner) = delete;
            FrameLevelSampleCombiner(FrameLevelSampleCombiner&& combiner) = default;
            FrameLevelSampleCombiner& operator=(FrameLevelSampleCombiner&& combiner) = default;
            ~FrameLevelSampleCombiner() = default;
        protected:
            static size_t CircularIncrement(size_t index, size_t max)
            {
                return (++index >= max) ? 0 : index;
            }

        public:
            bool Initialize(
                const std::vector<uint8_t>& counterDataPrefix,
                const std::vector<uint8_t>& counterDataSource,
                size_t maxSampleLatency)
            {
                bool success = m_combiner.Initialize(
                    counterDataPrefix.data(),
                    counterDataPrefix.size(), 
                    CombinedCounterDataMaxNumRanges,
                    counterDataSource.data());
                if (!success)
                {
                    return false;
                }
                for (size_t rangeIndex = 0; rangeIndex < CombinedCounterDataMaxNumRanges; ++rangeIndex)
                {
                    if (!m_combiner.CreateRange(rangeIndex))
                    {
                        return false;
                    }
                }
                m_counterDataTemplate = m_combiner.GetCounterData();
                m_sampleInfoRingBuffer.resize(maxSampleLatency);
                return true;
            }

            void Reset()
            {
                m_combiner.Reset();
                m_combinedCounterDataRangeIndex = 0;
                m_counterDataTemplate.clear();
                m_sampleInfoRingBuffer.clear();
                m_putIndex = 0;
                m_getIndex = 0;
                m_numUnreadSamples = 0;
                m_frameBeginTime = 0;
            }

            bool AddSample(const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex)
            {
                if (m_numUnreadSamples == m_sampleInfoRingBuffer.size())
                {
                    NV_PERF_LOG_ERR(50, "Buffer is full, specified \"maxSampleLatency\" is insufficient\n");
                    return false;
                }
                static_cast<void>(counterDataImageSize); // unused at this moment
                SampleTimestamp timestamp{};
                bool success = CounterDataGetSampleTime(pCounterDataImage, rangeIndex, timestamp);
                if (!success)
                {
                    return false;
                }

                assert(timestamp.start < timestamp.end);
                SampleInfo& sampleInfo = m_sampleInfoRingBuffer[m_putIndex];
                sampleInfo.beginTimestamp = timestamp.start;
                sampleInfo.endTimestamp = timestamp.end;
                sampleInfo.pCounterData = pCounterDataImage;
                sampleInfo.rangeIndex = rangeIndex;
                m_putIndex = CircularIncrement(m_putIndex, m_sampleInfoRingBuffer.size());
                ++m_numUnreadSamples;
                return true;
            }

            bool IsDataComplete(uint64_t frameEndTime) const
            {
                if (!m_numUnreadSamples)
                {
                    return false;
                }

                size_t lastSampleIdx = (m_getIndex + m_numUnreadSamples - 1);
                if (lastSampleIdx >= m_sampleInfoRingBuffer.size())
                {
                    lastSampleIdx -= m_sampleInfoRingBuffer.size();
                }
                const SampleInfo& sampleInfo = m_sampleInfoRingBuffer[lastSampleIdx];
                return (sampleInfo.endTimestamp >= frameEndTime);
            }

            // Note this doesn't depend on frame data's completeness, but will do best effort.
            // If completeness of frame data is desired, call this only after IsDataComplete()
            bool GetCombinedSamples(uint64_t frameEndTime, FrameInfo& frameInfo)
            {
                if (frameEndTime <= m_frameBeginTime)
                {
                    return false;
                }
                // if all the ranges have been occupied in the combined counter data, reset it
                if (m_combinedCounterDataRangeIndex == CombinedCounterDataMaxNumRanges)
                {
                    m_combiner.GetCounterData() = m_counterDataTemplate;
                    m_combinedCounterDataRangeIndex = 0;
                }
                const uint32_t combinedCounterDataRangeIndex = m_combinedCounterDataRangeIndex;
                ++m_combinedCounterDataRangeIndex;

                size_t numSamplesInFrame = 0;
                while (m_numUnreadSamples)
                {
                    const SampleInfo& sampleInfo = m_sampleInfoRingBuffer[m_getIndex];
                    if (sampleInfo.beginTimestamp >= frameEndTime)
                    {
                        //                  +----------+
                        //                  |  Sample  |
                        //                  +----------+
                        // +------------+
                        // | This Frame |
                        // +------------+
                        // belongs to a future frame => early break
                        break;
                    }
                    else if (sampleInfo.endTimestamp <= m_frameBeginTime)
                    {
                        // +----------+
                        // |  Sample  |
                        // +----------+
                        //                  +------------+
                        //                  | This Frame |
                        //                  +------------+
                        // belongs to a prior frame => ignore it
                        m_getIndex = CircularIncrement(m_getIndex, m_sampleInfoRingBuffer.size());
                        --m_numUnreadSamples;
                        continue;
                    }
                    else if (sampleInfo.endTimestamp > frameEndTime)
                    {
                        //                  +----------------------+
                        //                  |        Sample        |
                        //                  +----------------------+
                        //       +------------+
                        //       | This Frame |
                        //       +------------+
                        //                         +------------+
                        //          OR             | This Frame |
                        //                         +------------+
                        // partly falls into this frame, but partly belongs to a future frame => consume it but do not recycle it
                        ++numSamplesInFrame;
                        bool success = m_combiner.SumIntoRange(
                            combinedCounterDataRangeIndex,
                            sampleInfo.pCounterData,
                            sampleInfo.rangeIndex,
                            sampleInfo.beginTimestamp,
                            sampleInfo.endTimestamp,
                            m_frameBeginTime,
                            frameEndTime);
                        if (!success)
                        {
                            return false;
                        }
                        break;
                    }
                    else
                    {
                        //            +----------------------+
                        //            |        Sample        |
                        //            +----------------------+
                        //                            +------------+
                        //                            | This Frame |
                        //                            +------------+
                        //        +------------------------------------+
                        //  OR    |         This Frame                 |
                        //        +------------------------------------+
                        // consume and recycle it
                        ++numSamplesInFrame;
                        m_getIndex = CircularIncrement(m_getIndex, m_sampleInfoRingBuffer.size());
                        --m_numUnreadSamples;
                        bool success = m_combiner.SumIntoRange(
                            combinedCounterDataRangeIndex,
                            sampleInfo.pCounterData,
                            sampleInfo.rangeIndex,
                            sampleInfo.beginTimestamp,
                            sampleInfo.endTimestamp,
                            m_frameBeginTime,
                            frameEndTime);
                        if (!success)
                        {
                            return false;
                        }
                    }
                }

                frameInfo.beginTimestamp = m_frameBeginTime;
                frameInfo.endTimestamp = frameEndTime;
                frameInfo.numSamplesInFrame = numSamplesInFrame;
                frameInfo.pCombinedCounterData = m_combiner.GetCounterData().data();
                frameInfo.combinedCounterDataSize = m_combiner.GetCounterData().size();
                frameInfo.combinedCounterDataRangeIndex = combinedCounterDataRangeIndex; 
                m_frameBeginTime = frameEndTime;
                return true;
            }
        };

        inline bool CounterDataTrimInPlace(uint8_t* pCounterDataImage, size_t counterDataImageSize, size_t& counterDataImageTrimmedSize)
        {
            NVPW_PeriodicSampler_CounterData_TrimInPlace_Params params = { NVPW_PeriodicSampler_CounterData_TrimInPlace_Params_STRUCT_SIZE };
            params.pCounterDataImage = pCounterDataImage;
            params.counterDataImageSize = counterDataImageSize;
            NVPA_Status nvpaStatus = NVPW_PeriodicSampler_CounterData_TrimInPlace(&params);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "NVPW_PeriodicSampler_CounterData_TrimInPlace failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            counterDataImageTrimmedSize = params.counterDataImageTrimmedSize;
            return true;
        }

        inline bool CounterDataGetSampleTime(const uint8_t* pCounterDataImage, size_t rangeIndex, SampleTimestamp& timestamp)
        {
            NVPW_PeriodicSampler_CounterData_GetSampleTime_Params params = { NVPW_PeriodicSampler_CounterData_GetSampleTime_Params_STRUCT_SIZE };
            params.pCounterDataImage = pCounterDataImage;
            params.rangeIndex = rangeIndex;
            NVPA_Status nvpaStatus = NVPW_PeriodicSampler_CounterData_GetSampleTime(&params);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "NVPW_PeriodicSampler_CounterData_GetSampleTime failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            timestamp.start = params.timestampStart;
            timestamp.end = params.timestampEnd;
            return true;
        }

        inline bool CounterDataGetTriggerCount(const uint8_t* pCounterDataImage, size_t counterDataImageSize, size_t rangeIndex, uint32_t& triggerCount)
        {
            NVPW_PeriodicSampler_CounterData_GetTriggerCount_Params params = { NVPW_PeriodicSampler_CounterData_GetTriggerCount_Params_STRUCT_SIZE };
            params.pCounterDataImage = pCounterDataImage;
            params.counterDataImageSize = counterDataImageSize;
            params.rangeIndex = rangeIndex;
            NVPA_Status nvpaStatus = NVPW_PeriodicSampler_CounterData_GetTriggerCount(&params);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "NVPW_PeriodicSampler_CounterData_GetTriggerCount failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            triggerCount = params.triggerCount;
            return true;
        }

        inline bool CounterDataGetInfo(const uint8_t* pCounterDataImage, size_t counterDataImageSize, CounterDataInfo& counterDataInfo)
        {
            NVPW_PeriodicSampler_CounterData_GetInfo_Params params = { NVPW_PeriodicSampler_CounterData_GetInfo_Params_STRUCT_SIZE };
            params.pCounterDataImage = pCounterDataImage;
            params.counterDataImageSize = counterDataImageSize;
            NVPA_Status nvpaStatus = NVPW_PeriodicSampler_CounterData_GetInfo(&params);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(50, "NVPW_PeriodicSampler_CounterData_GetInfo failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            counterDataInfo.numTotalRanges = static_cast<uint32_t>(params.numTotalRanges);
            counterDataInfo.numPopulatedRanges = static_cast<uint32_t>(params.numPopulatedRanges);
            counterDataInfo.numCompletedRanges = static_cast<uint32_t>(params.numCompletedRanges);
            return true;
        }
    
    } // namespace sampler
}}
