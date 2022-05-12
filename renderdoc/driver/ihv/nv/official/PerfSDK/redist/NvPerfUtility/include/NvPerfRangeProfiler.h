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
#include <list>
#include <utility>
#include <vector>

#ifdef __linux__
#include <sys/stat.h>
#endif

#include "NvPerfCounterData.h"
#include "NvPerfCounterConfiguration.h"

namespace nv { namespace perf { namespace profiler {

    // safe defaults for realtime
    struct SessionOptions
    {
        size_t maxNumRanges = 16;
        size_t avgRangeNameLength = 128;
        size_t numTraceBuffers = 5;                 // recommended: SwapChainDepth + 2
    };

    struct SetConfigParams
    {
        const uint8_t* pConfigImage;
        size_t configImageSize;
        const uint8_t* pCounterDataPrefix;
        size_t counterDataPrefixSize;
        size_t numPasses;
        uint16_t numNestingLevels;
        size_t numStatisticalSamples;

        SetConfigParams()
            : pConfigImage()
            , configImageSize()
            , pCounterDataPrefix()
            , counterDataPrefixSize()
            , numPasses()
            , numNestingLevels()
            , numStatisticalSamples()
        {
        }

        SetConfigParams(const CounterConfiguration& configuration, uint16_t numNestingLevels = 1, size_t numStatisticalSamples = 1)
            : pConfigImage(configuration.configImage.data())
            , configImageSize(configuration.configImage.size())
            , pCounterDataPrefix(configuration.counterDataPrefix.data())
            , counterDataPrefixSize(configuration.counterDataPrefix.size())
            , numPasses(configuration.numPasses)
            , numNestingLevels(numNestingLevels)
            , numStatisticalSamples(numStatisticalSamples)
        {
        }
    };

    // out-param from DecodeCounters
    struct DecodeResult
    {
        bool onePassDecoded;
        bool allPassesDecoded;
        bool allStatisticalSamplesCollected;
        std::vector<uint8_t> counterDataImage;      // if allPassesDecoded is true, this will be non-empty
    };

    class RangeProfilerStateMachine
    {
    public: // types
        struct IProfilerApi
        {
            virtual bool CreateCounterData(const SetConfigParams& config, std::vector<uint8_t>& counterDataImage, std::vector<uint8_t>& counterDataScratch) const = 0;
            virtual bool SetConfig(const SetConfigParams& config) const = 0;
            virtual bool BeginPass() const = 0;
            virtual bool EndPass() const = 0;
            virtual bool PushRange(const char* pRangeName) = 0;
            virtual bool PopRange() = 0;
            virtual bool DecodeCounters(std::vector<uint8_t>& counterDataImage, std::vector<uint8_t>& counterDataScratch, bool& onePassDecoded, bool& allPassesDecoded) const = 0;
        };

    protected: // types
        struct CounterStateMachine
        {
            // state updated per-pass
            size_t numPassesSubmitted;                          /// number of passes submitted (incremented at EndPass)
            size_t numStatisticalSamplesCollected;              /// number of times all passes were collected

            // state derived from the configuration
            size_t numPassesPerStatisticalSample;               /// number of passes required by the {ConfigImage, numNestingLevels}
            size_t numStatisticalSamplesRequired;               /// number of repeated samplings required by SetConfig
            std::vector<uint8_t> counterDataImage;              /// opaque buffer containing HW counter data; updated in DecodeCounters on each frame
            std::vector<uint8_t> counterDataScratch;            /// opaque buffer needed by DecodeCounters

            bool AllPassesSubmitted() const
            {
                const bool allPassesSubmitted = (numPassesSubmitted == numPassesPerStatisticalSample * numStatisticalSamplesRequired);
                return allPassesSubmitted;
            }
        };

    protected: // members
        IProfilerApi& m_profilerApi;
        bool m_inPass;

        // Use std::list for stable iterators and a guarantee of no-copy.
        typedef std::list<SetConfigParams> ConfigQueue;
        typedef std::list<CounterStateMachine> CountersQueue;
        bool m_needSetConfig;
        ConfigQueue m_configQueue;                      // m_configQueue.front() is the active configuration (by SetConfig), and is popped after all passes are submitted
        CountersQueue m_countersQueue;                  // queued CounterData, which may lag the configQueue when frames are rendered asynchronously
        CountersQueue::iterator m_submitCounterItr;     // points at the CounterData corresponding to m_configQueue.front()

    private:
        // non-copyable
        RangeProfilerStateMachine(const RangeProfilerStateMachine&);

    public:
        ~RangeProfilerStateMachine()
        {
            Reset();
        }

        RangeProfilerStateMachine(IProfilerApi& profilerApi)
            : m_profilerApi(profilerApi)
            , m_inPass(false)
            , m_needSetConfig()
            , m_configQueue()
            , m_countersQueue()
            , m_submitCounterItr()
        {
        }

        void Reset()
        {
            m_submitCounterItr = {};
            m_countersQueue.clear();
            m_configQueue.clear();
            m_needSetConfig = false;
            m_inPass = false;
        }

        bool IsInPass() const
        {
            return m_inPass;
        }

        bool EnqueueCounterCollection(const SetConfigParams& config)
        {
            CounterStateMachine counterStateMachine = {};
            counterStateMachine.numPassesPerStatisticalSample = config.numPasses * config.numNestingLevels;
            counterStateMachine.numStatisticalSamplesRequired = config.numStatisticalSamples;
            if (!m_profilerApi.CreateCounterData(config, counterStateMachine.counterDataImage, counterStateMachine.counterDataScratch))
            {
                return false;
            }

            if (m_configQueue.empty())
            {
                m_needSetConfig = true;
            }
            m_configQueue.push_back(config);

            const bool countersQueueWasEmpty = m_countersQueue.empty();
            m_countersQueue.emplace_back(std::move(counterStateMachine));
            if (countersQueueWasEmpty)
            {
                m_submitCounterItr = m_countersQueue.begin();
            }

            return true;
        }

        bool BeginPass()
        {
            if (m_inPass)
            {
                // TODO: error - must be called in session, but outside of a pass
                return false;
            }
            if (m_configQueue.empty())
            {
                // Do not enqueue additional HW data collection.
                return true;
            }

            if (m_needSetConfig)
            {
                if (!m_profilerApi.SetConfig(m_configQueue.front()))
                {
                    return false;
                }
                m_needSetConfig = false;
            }

            if (!m_profilerApi.BeginPass())
            {
                return false;
            }

            m_inPass = true;
            return true;
        }

        bool EndPass()
        {
            if (!m_inPass)
            {
                // TODO: error - must be called in session, and inside of a pass
                return false;
            }

            if (m_configQueue.empty())
            {
                // Do not enqueue additional HW data collection.
                return true;
            }

            if (!m_profilerApi.EndPass())
            {
                return false;
            }

            CounterStateMachine& counterStateMachine = *m_submitCounterItr;
            counterStateMachine.numPassesSubmitted += 1;
            if (counterStateMachine.AllPassesSubmitted())
            {
                ++m_submitCounterItr;
                m_configQueue.pop_front();
                if (!m_configQueue.empty())
                {
                    m_needSetConfig = true;
                }
            }

            m_inPass = false;
            return true;
        }

        bool PushRange(const char* pRangeName)
        {
            if (!m_inPass)
            {
                // TODO: error - must be called in session, and inside of a pass
                return false;
            }

            if (m_configQueue.empty())
            {
                // Do not enqueue additional HW data collection.
                return true;
            }
            
            if (!m_profilerApi.PushRange(pRangeName))
            {
                return false;
            }

            return true;
        }

        bool PopRange()
        {
            if (!m_inPass)
            {
                // TODO: error - must be called in session, and inside of a pass
                return false;
            }

            if (m_configQueue.empty())
            {
                // Do not enqueue additional HW data collection.
                return true;
            }
            
            if (!m_profilerApi.PopRange())
            {
                return false;
            }

            return true;
        }

        bool DecodeCounters(DecodeResult& decodeResult)
        {
            if (m_countersQueue.empty())
            {
                // TODO: error - nothing is queued for collection.  see SetConfig ...
                return false;
            }

            CounterStateMachine& counterStateMachine = m_countersQueue.front();

            decodeResult = {};
            if (!m_profilerApi.DecodeCounters(counterStateMachine.counterDataImage, counterStateMachine.counterDataScratch, decodeResult.onePassDecoded, decodeResult.allPassesDecoded))
            {
                // TODO: error - the session must be torn down
                return false;
            }

            if (decodeResult.allPassesDecoded)
            {
                counterStateMachine.numStatisticalSamplesCollected += 1;
                if (counterStateMachine.numStatisticalSamplesCollected == counterStateMachine.numStatisticalSamplesRequired)
                {
                    decodeResult.allStatisticalSamplesCollected = true;
                    decodeResult.counterDataImage = std::move(counterStateMachine.counterDataImage);
                    m_countersQueue.pop_front();
                }
            }
            return true;
        }

        bool AllPassesSubmitted() const
        {
            const bool allPassesSubmitted = m_configQueue.empty();
            return allPassesSubmitted;
        }
    };

}}}