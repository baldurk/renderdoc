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

#include <cstdint>
#include "NvPerfInit.h"
#include "NvPerfCounterConfiguration.h"
#include "NvPerfCounterData.h"
#include "NvPerfDeviceProperties.h"
#include "NvPerfMiniTraceVulkan.h"
#include "NvPerfPeriodicSamplerGpu.h"
#include "NvPerfVulkan.h"
#include "NvPerfUtilities.h"
#include <chrono>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <functional>
#if defined(__linux__)
#include <sys/stat.h>
#endif

namespace nv { namespace perf { namespace sampler {

    class PeriodicSamplerTimeHistoryVulkan
    {
    private:
        enum class SamplerStatus
        {
            Uninitialized,
            Failed,
            InitializedButNotInSession,
            InSession,
        };

        GpuPeriodicSampler m_periodicSamplerGpu;
        RingBufferCounterData m_counterData;
        mini_trace::MiniTracerVulkan m_tracer;
        uint32_t m_maxTriggerLatency;
        bool m_isFirstFrame;
        SamplerStatus m_status;

    public:
        struct FrameDelimiter
        {
            uint64_t frameEndTime;
        };

    public:
        PeriodicSamplerTimeHistoryVulkan()
            : m_maxTriggerLatency()
            , m_isFirstFrame(true)
            , m_status(SamplerStatus::Uninitialized)
        {
        }
        PeriodicSamplerTimeHistoryVulkan(const PeriodicSamplerTimeHistoryVulkan& sampler) = delete;
        PeriodicSamplerTimeHistoryVulkan(PeriodicSamplerTimeHistoryVulkan&& sampler)
            : m_periodicSamplerGpu(std::move(sampler.m_periodicSamplerGpu))
            , m_counterData(std::move(sampler.m_counterData))
            , m_tracer(std::move(sampler.m_tracer))
            , m_maxTriggerLatency(sampler.m_maxTriggerLatency)
            , m_isFirstFrame(sampler.m_isFirstFrame)
            , m_status(sampler.m_status)
        {
            sampler.m_status = SamplerStatus::Uninitialized;
        }
        ~PeriodicSamplerTimeHistoryVulkan()
        {
            Reset();
        }
        PeriodicSamplerTimeHistoryVulkan& operator=(const PeriodicSamplerTimeHistoryVulkan& sampler) = delete;
        PeriodicSamplerTimeHistoryVulkan& operator=(PeriodicSamplerTimeHistoryVulkan&& sampler)
        {
            Reset();
            m_periodicSamplerGpu = std::move(sampler.m_periodicSamplerGpu);
            m_counterData = std::move(sampler.m_counterData);
            m_tracer = std::move(sampler.m_tracer);
            m_maxTriggerLatency = sampler.m_maxTriggerLatency;
            m_isFirstFrame = sampler.m_isFirstFrame;
            m_status = sampler.m_status;
            sampler.m_status = SamplerStatus::Uninitialized;
            return *this;
        }

        static void AppendDeviceRequiredExtensions(uint32_t vulkanApiVersion, std::vector<const char*>& deviceExtensionNames)
        {
            mini_trace::MiniTracerVulkan::AppendDeviceRequiredExtensions(vulkanApiVersion, deviceExtensionNames);
        }

        bool Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device
#if defined(VK_NO_PROTOTYPES)
                       , PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr
                       , PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr
#endif
                       )
        {
            if (m_status != SamplerStatus::Uninitialized)
            {
                NV_PERF_LOG_ERR(50, "Not in an initializable state\n");
                return false;
            }
            auto setStatusToFailed = ScopeExitGuard([&]() {
                m_status = SamplerStatus::Failed;
            });
            if (!InitializeNvPerf())
            {
                NV_PERF_LOG_ERR(10, "InitializeNvPerf failed\n");
                return false;
            }

            if (!VulkanLoadDriver(instance))
            {
                NV_PERF_LOG_ERR(10, "Could not load driver\n");
                return false;
            }

            const size_t deviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                                                 , pfnVkGetInstanceProcAddr
                                                                 , pfnVkGetDeviceProcAddr
#endif
                                                                 );
            if (deviceIndex == (size_t)~0)
            {
                NV_PERF_LOG_ERR(20, "VulkanGetNvperfDeviceIndex failed\n");
                return false;
            }

            bool success = m_periodicSamplerGpu.Initialize(deviceIndex);
            if (!success)
            {
                return false;
            }
            success = m_tracer.Initialize(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                         , pfnVkGetInstanceProcAddr
                                         , pfnVkGetDeviceProcAddr
#endif
                                         );
            if (!success)
            {
                return false;
            }
            m_isFirstFrame = true;
            setStatusToFailed.Dismiss();
            m_status = SamplerStatus::InitializedButNotInSession;
            return true;
        }

        void Reset()
        {
            m_periodicSamplerGpu.Reset();
            m_counterData.Reset();
            m_tracer.Reset();
            m_maxTriggerLatency = 0;
            m_isFirstFrame = true;
            m_status = SamplerStatus::Uninitialized;
        }

        bool IsInitialized() const
        {
            return (m_status == SamplerStatus::InitializedButNotInSession) || (m_status == SamplerStatus::InSession);
        }

        size_t GetGpuDeviceIndex() const
        {
            return m_periodicSamplerGpu.GetDeviceIndex();
        }

        const DeviceIdentifiers& GetGpuDeviceIdentifiers() const
        {
            return m_periodicSamplerGpu.GetDeviceIdentifiers();
        }

        RingBufferCounterData& GetRingBufferCounterData()
        {
            return m_counterData;
        }

        const RingBufferCounterData& GetRingBufferCounterData() const
        {
            return m_counterData;
        }

        const std::vector<uint8_t>& GetCounterData() const
        {
            return m_counterData.GetCounterData();
        }

        std::vector<uint8_t>& GetCounterData()
        {
            return m_counterData.GetCounterData();
        }

        bool BeginSession(VkQueue queue, uint32_t queueFamilyIndex, uint32_t samplingIntervalInNanoSeconds, uint32_t maxDecodeLatencyInNanoSeconds, size_t maxFrameLatency)
        {
            if (m_status != SamplerStatus::InitializedButNotInSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a state ready for BeginSession\n");
                return false;
            }
            auto setStatusToFailed = ScopeExitGuard([&]() {
                m_status = SamplerStatus::Failed;
            });
            const GpuPeriodicSampler::GpuPulseSamplingInterval samplingInterval = m_periodicSamplerGpu.GetGpuPulseSamplingInterval(samplingIntervalInNanoSeconds);
            m_maxTriggerLatency = maxDecodeLatencyInNanoSeconds / samplingIntervalInNanoSeconds;
            size_t recordBufferSize = 0;
            bool success = GpuPeriodicSamplerCalculateRecordBufferSize(GetGpuDeviceIndex(), std::vector<uint8_t>(), m_maxTriggerLatency, recordBufferSize);
            if (!success)
            {
                return false;
            }
            const size_t maxNumUndecodedSamplingRanges = 1;
            success = m_periodicSamplerGpu.BeginSession(recordBufferSize, maxNumUndecodedSamplingRanges, { samplingInterval.triggerSource }, samplingInterval.samplingInterval);
            if (!success)
            {
                return false;
            }
            success = m_tracer.BeginSession(queue, queueFamilyIndex, maxFrameLatency);
            if (!success)
            {
                return false;
            }
            setStatusToFailed.Dismiss();
            m_status = SamplerStatus::InSession;
            return true;
        }
        
        bool EndSession()
        {
            bool success = m_periodicSamplerGpu.EndSession();
            if (!success)
            {
                // continue
            }
            m_tracer.EndSession();
            m_maxTriggerLatency = 0;
            if (m_status == SamplerStatus::InSession)
            {
                m_status = SamplerStatus::InitializedButNotInSession;
            }
            return success;
        }

        bool SetConfig(const CounterConfiguration* pCounterConfiguration)
        {
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return false;
            }
            if (!pCounterConfiguration || pCounterConfiguration->numPasses != 1)
            {
                NV_PERF_LOG_ERR(30, "Invalid counter config\n");
                return false;
            }

            const size_t passIndex = 0;
            bool success = m_periodicSamplerGpu.SetConfig(pCounterConfiguration->configImage, passIndex);
            if (!success)
            {
                return false;
            }

            m_counterData.Reset();
            const bool validate = false; // set this to true to perform extra validation, useful for debugging
            success = m_counterData.Initialize(m_maxTriggerLatency, validate, [&](uint32_t maxSamples, NVPW_PeriodicSampler_CounterData_AppendMode appendMode, std::vector<uint8_t>& counterData) {
                return GpuPeriodicSamplerCreateCounterData(
                    GetGpuDeviceIndex(),
                    pCounterConfiguration->counterDataPrefix.data(),
                    pCounterConfiguration->counterDataPrefix.size(),
                    maxSamples,
                    appendMode,
                    counterData);
            });
            if (!success)
            {
                return false;
            }
            return true;
        }

        bool StartSampling()
        {
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return false;
            }
            bool success = m_periodicSamplerGpu.StartSampling();
            if (!success)
            {
                return false;
            }
            return true;
        }

        bool StopSampling()
        {
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return false;
            }
            bool success = m_periodicSamplerGpu.StopSampling();
            if (!success)
            {
                return false;
            }
            return true;
        }

        bool OnFrameEnd()
        {
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return false;
            }
            bool success = true;
            if (m_isFirstFrame)
            {
                success = StartSampling();
                if (!success)
                {
                    return false;
                }
                m_isFirstFrame = false;
            }
            success = m_tracer.OnFrameEnd();
            if (!success)
            {
                return false;
            }
            return true;
        }

        std::vector<FrameDelimiter> GetFrameDelimiters()
        {
            std::vector<FrameDelimiter> delimiters;
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return delimiters;
            }
            while (true)
            {
                bool isDataReady = false;
                mini_trace::MiniTracerVulkan::FrameData frameData{};
                if (!m_tracer.GetOldestFrameData(isDataReady, frameData))
                {
                    break;
                }
                if (!isDataReady)
                {
                    break;
                }
                delimiters.push_back(FrameDelimiter{ frameData.frameEndTime });
                if (!m_tracer.ReleaseOldestFrame())
                {
                    break;
                }
            }
            return delimiters;
        }

        bool DecodeCounters()
        {
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return false;
            }
            GpuPeriodicSampler::GetRecordBufferStatusParams getRecordBufferStatusParams = {};
            getRecordBufferStatusParams.queryOverflow = true;
            getRecordBufferStatusParams.queryNumUnreadBytes = true;
            bool success = m_periodicSamplerGpu.GetRecordBufferStatus(getRecordBufferStatusParams);
            if (!success)
            {
                return false;
            }
            if (!!getRecordBufferStatusParams.overflow)
            {
                NV_PERF_LOG_ERR(20, "Record buffer overflow has been detected! Please try to 1) reduce sampling frequency 2) increase record buffer size "
"3) call DecodeCounters() more frequently\n");
                return false;
            }
            if (getRecordBufferStatusParams.numUnreadBytes == 0)
            {
                return true;
            }

            NVPW_GPU_PeriodicSampler_DecodeStopReason decodeStopReason = NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_OTHER;
            size_t numSamplesMerged = 0;
            size_t numBytesConsumed = 0;
            success = m_periodicSamplerGpu.DecodeCounters(
                m_counterData.GetCounterData(),
                getRecordBufferStatusParams.numUnreadBytes,
                decodeStopReason,
                numSamplesMerged,
                numBytesConsumed);
            if (!success)
            {
                return false;
            }
            if (decodeStopReason != NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_ALL_GIVEN_BYTES_READ)
            {
                NV_PERF_LOG_WRN(100, "DecodeCounters stopped unexpectedly. Stop Reason: %d \n", decodeStopReason);
            }
            if (numSamplesMerged)
            {
                NV_PERF_LOG_WRN(100, "Merged samples have been detected! This may lead to reduced accuracy. please try to reduce the sampling frequency.\n");
            }

            success = m_periodicSamplerGpu.AcknowledgeRecordBuffer(numBytesConsumed);
            if (!success)
            {
                return false;
            }

            success = m_counterData.UpdatePut();
            if (!success)
            {
                return false;
            }
            return true;
        }

        // TConsumeRangeDataFunc should be in the form of bool(const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex, bool& stop),
        // return false to indicate something went wrong; set "stop" to true to early break from iterating succeeding unread ranges, "this" range will not be recycled
        template <typename TConsumeRangeDataFunc>
        bool ConsumeSamples(TConsumeRangeDataFunc&& consumeRangeDataFunc)
        {
            if (m_status != SamplerStatus::InSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session, this function is skipped\n");
                return false;
            }
            uint32_t numRangesConsumed = 0;
            bool success = m_counterData.ConsumeData([&, userFunc = std::forward<TConsumeRangeDataFunc>(consumeRangeDataFunc)](const uint8_t* pCounterDataImage, size_t counterDataImageSize, uint32_t rangeIndex, bool& stop) {
                bool userFuncSuccess = userFunc(pCounterDataImage, counterDataImageSize, rangeIndex, stop);
                if (!userFuncSuccess)
                {
                    return false;
                }
                if (!stop)
                {
                    ++numRangesConsumed;
                }
                return true; // simply pass "stop" along, m_counterData.ConsumeData will further process it and early break
            });
            if (!success)
            {
                return false;
            }

            success = m_counterData.UpdateGet(numRangesConsumed);
            if (!success)
            {
                return false;
            }
            return true;
        }
    };

    class PeriodicSamplerOneShotVulkan
    {
    public:
        enum class SamplerStatus
        {
            Uninitialized,
            Failed,
            Initialized,
            Sampling,
        };
        enum class AppendDateTime
        {
            no,
            yes
        };
        struct OutputOption
        {
            std::string directoryName = "OneShot";
            AppendDateTime appendDateTime = AppendDateTime::yes;
        };
        OutputOption m_outputOption;
        CounterConfiguration m_counterConfiguration;
        sampler::GpuPeriodicSampler m_periodicSamplerGpu;
        std::vector<uint8_t> m_counterData;              // This is used to store the counter values collected during profiling.
        std::function<void(const char*)> m_onStopSampling;
        std::string m_outputDirectory;
        SamplerStatus m_status;
        bool m_inCollection;
        size_t m_samplingIntervalNs;
        size_t m_maxDurationNs;
        size_t m_numFramesToSample;
        size_t m_numFramesSampled;

        PeriodicSamplerOneShotVulkan()
            : m_status(SamplerStatus::Uninitialized)
            , m_inCollection(false)
            , m_samplingIntervalNs(0)
            , m_maxDurationNs(0)
            , m_numFramesToSample(0)
            , m_numFramesSampled(0)
        {
        }
        PeriodicSamplerOneShotVulkan(const PeriodicSamplerOneShotVulkan& sampler) = delete;
        PeriodicSamplerOneShotVulkan(PeriodicSamplerOneShotVulkan&& sampler) = default;
        ~PeriodicSamplerOneShotVulkan()
        {
            Reset();
        }
        PeriodicSamplerOneShotVulkan& operator=(const PeriodicSamplerOneShotVulkan& sampler) = delete;
        PeriodicSamplerOneShotVulkan& operator=(PeriodicSamplerOneShotVulkan&& sampler) = default;

        SamplerStatus GetSamplerStatus()
        {
            return m_status;
        }

        bool Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
                        size_t samplingIntervalNs, size_t maxDurationPerFrameNs, size_t numFramesToSample,
                        std::function<void(const char*)> onStopSampling,
#if defined(VK_NO_PROTOTYPES)
                        PFN_vkGetInstanceProcAddr pfnVkGetInstanceProcAddr,
                        PFN_vkGetDeviceProcAddr pfnVkGetDeviceProcAddr,
#endif
                        const CounterConfiguration& counterConfiguration
                        )
        {
            if (m_status != SamplerStatus::Uninitialized)
            {
                NV_PERF_LOG_ERR(50, "Not in an initializable state\n");
                return false;
            }
            auto setStatusToFailed = ScopeExitGuard([&]() {
                m_status = SamplerStatus::Failed;
            });

            if (counterConfiguration.numPasses != 1)
            {
                NV_PERF_LOG_ERR(20, "Number of passes is not 1.\n");
                return false;
            }
            m_counterConfiguration = counterConfiguration;

            if (!InitializeNvPerf())
            {
                NV_PERF_LOG_ERR(10, "InitializeNvPerf failed\n");
                return false;
            }

            if (!VulkanLoadDriver(instance))
            {
                NV_PERF_LOG_ERR(10, "Could not load driver\n");
                return false;
            }

            if (!VulkanIsNvidiaDevice(physicalDevice
#if defined(VK_NO_PROTOTYPES)
                                    , instance
                                    , pfnVkGetInstanceProcAddr
#endif
            ))
            {
                NV_PERF_LOG_ERR(10, "Not running on an NVIDIA device\n");
                return false;
            }

            const size_t deviceIndex = VulkanGetNvperfDeviceIndex(instance, physicalDevice, device
#if defined(VK_NO_PROTOTYPES)
                                                                 , pfnVkGetInstanceProcAddr
                                                                 , pfnVkGetDeviceProcAddr
#endif
                                                                 );
            if (deviceIndex == (size_t)~0)
            {
                NV_PERF_LOG_ERR(20, "VulkanGetNvperfDeviceIndex failed\n");
                return false;
            }

            bool success = m_periodicSamplerGpu.Initialize(deviceIndex);
            if (!success)
            {
                NV_PERF_LOG_ERR(20, "GpuPeriodicSampler::Initialize failed\n");
                return false;
            }

            const DeviceIdentifiers deviceIdentifiers = m_periodicSamplerGpu.GetDeviceIdentifiers();
            const char* pChipName = deviceIdentifiers.pChipName;

            m_samplingIntervalNs = samplingIntervalNs;
            m_maxDurationNs = maxDurationPerFrameNs * numFramesToSample;
            success = InitializeCounterData();
            if (!success)
            {
                NV_PERF_LOG_ERR(20, "InitializeCounterData failed\n");
                return false;
            }

            m_numFramesToSample = numFramesToSample;
            m_numFramesSampled = 0;
            const size_t maxSamples = m_maxDurationNs / m_samplingIntervalNs;

            size_t recordBufferSize = 0;
            success = GpuPeriodicSamplerCalculateRecordBufferSize(deviceIndex, m_counterConfiguration.configImage, maxSamples, recordBufferSize);
            if (!success)
            {
                NV_PERF_LOG_ERR(20, "GpuPeriodicSamplerCalculateRecordBufferSize failed\n");
                return false;
            }
    
            const GpuPeriodicSampler::GpuPulseSamplingInterval samplingInterval = m_periodicSamplerGpu.GetGpuPulseSamplingInterval((uint32_t)m_samplingIntervalNs);
            success = m_periodicSamplerGpu.BeginSession(
                recordBufferSize,
                1,
                {samplingInterval.triggerSource},
                samplingInterval.samplingInterval
            );
            if (!success)
            {
                NV_PERF_LOG_ERR(20, "GpuPeriodicSampler::BeginSession failed\n");
                return false;
            }

            success = m_periodicSamplerGpu.SetConfig(m_counterConfiguration.configImage, 0);
            if (!success)
            {
                NV_PERF_LOG_ERR(20, "GpuPeriodicSampler::SetConfig failed\n");
                return false;
            }
            m_onStopSampling = onStopSampling;
            setStatusToFailed.Dismiss();
            m_status = SamplerStatus::Initialized;
            return true;
        }

        void StartCollectionOnFrameEnd()
        {
            m_inCollection = true;
        }

        bool OnFrameEnd()
        {
            if (m_inCollection && m_status == SamplerStatus::Sampling)
            {
                ++m_numFramesSampled;
                if (m_numFramesSampled == m_numFramesToSample)
                {
                    m_onStopSampling(m_outputDirectory.c_str());
                    bool success = m_periodicSamplerGpu.StopSampling();
                    if (!success)
                    {
                        NV_PERF_LOG_ERR(20, "GpuPeriodicSampler::StopSampling failed\n");
                        return false;
                    }
                    m_inCollection = false;
                    m_status = SamplerStatus::Initialized;

                    nv::perf::sampler::GpuPeriodicSampler::GetRecordBufferStatusParams getRecordBufferStatusParams = {};
                    getRecordBufferStatusParams.queryOverflow = true;
                    getRecordBufferStatusParams.queryNumUnreadBytes = true;
                    success = m_periodicSamplerGpu.GetRecordBufferStatus(getRecordBufferStatusParams);
                    if (!success)
                    {
                        NV_PERF_LOG_ERR(20, "GpuPeriodicSampler::GetRecordBufferStatus failed\n");
                        return false;
                    }

                    if (!!getRecordBufferStatusParams.overflow)
                    {
                        NV_PERF_LOG_WRN(20, "Record buffer overflow has been detected! Please try to 1) reduce sampling frequency 2) increase record buffer size ");
                    }

                    if (getRecordBufferStatusParams.numUnreadBytes == 0)
                    {
                        NV_PERF_LOG_ERR(20, "Nothing to decode\n");
                        return false;
                    }

                    // Decode the record buffer and store the decoded counters into the counter data.
                    NVPW_GPU_PeriodicSampler_DecodeStopReason decodeStopReason = NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_OTHER;
                    size_t numSamplesMerged = 0;
                    size_t numBytesConsumed = 0;
                    success = m_periodicSamplerGpu.DecodeCounters(m_counterData, getRecordBufferStatusParams.numUnreadBytes, decodeStopReason, numSamplesMerged, numBytesConsumed);
                    if (!success)
                    {
                        NV_PERF_LOG_ERR(20, "GpuPeriodicSampler::DecodeCounters failed\n");
                        return false;
                    }

                    if (!numBytesConsumed)
                    {
                        NV_PERF_LOG_ERR(20, "GpuPeriodicSampler::DecodeCounters: No bytes consumed\n");
                        return false;
                    }

                    if (decodeStopReason != NVPW_GPU_PERIODIC_SAMPLER_DECODE_STOP_REASON_ALL_GIVEN_BYTES_READ)
                    {
                        NV_PERF_LOG_WRN(20, "DecodeCounters stopped unexpectedly. Stop Reason: %d \n", decodeStopReason);
                    }

                    success = m_periodicSamplerGpu.AcknowledgeRecordBuffer(numBytesConsumed);
                    if (!success)
                    {
                        NV_PERF_LOG_ERR(20, "GpuPeriodicSampler::AcknowledgeRecordBuffer failed\n");
                        return false;
                    }
                    size_t counterDataSize = 0;
                    success = CounterDataTrimInPlace(m_counterData.data(), m_counterData.size(), counterDataSize);
                    {
                        std::string counterDataFileName = utilities::JoinDriectoryAndFileName(m_outputDirectory, "counterData.bin");
                        std::ofstream counterFile(counterDataFileName, std::ios::binary);
                        if (!counterFile.is_open())
                        {
                            NV_PERF_LOG_ERR(20, "Failed to open %s for writing\n", counterDataFileName.c_str());
                            return false;
                        }
                        counterFile.write(reinterpret_cast<const char*>(m_counterData.data()), counterDataSize);
                        if (!counterFile.good())
                        {
                            NV_PERF_LOG_ERR(20, "Failed to write to %s\n", counterDataFileName.c_str());
                            return false;
                        }
                    }

                    success = InitializeCounterData();
                    if (!success)
                    {
                        NV_PERF_LOG_ERR(20, "InitializeCounterData failed\n");
                        return false;
                    }
                    m_status = SamplerStatus::Initialized;
                    m_inCollection = false;
                    m_numFramesSampled = 0;
                    return true;
                }
                return true;
            }
            else if (m_inCollection && m_status == SamplerStatus::Initialized)
            {
                std::string outputDirName = m_outputOption.directoryName;
                if (m_outputOption.appendDateTime == AppendDateTime::yes)
                {
                    auto formatTime = [](const time_t& secondsSinceEpoch) -> std::string{
                        struct tm now_time;
#ifdef WIN32
                        const errno_t ret = localtime_s(&now_time, &secondsSinceEpoch);
                        if (ret)
                        {
                            return std::string();
                        }
#else
                        const tm* pRet = localtime_r(&secondsSinceEpoch, &now_time);
                        if (!pRet)
                        {
                            return std::string();
                        }
#endif
                        std::stringstream sstream;
                        sstream << std::setfill('0') << 1900 + now_time.tm_year << std::setw(2) << 1 + now_time.tm_mon << std::setw(2) << now_time.tm_mday << "_"
                                << std::setw(2) << now_time.tm_hour << std::setw(2) << now_time.tm_min << std::setw(2) << now_time.tm_sec;
                        return sstream.str();
                    };

                    auto now = std::chrono::system_clock::now();
                    const uint64_t secondsSinceEpoch = static_cast<uint64_t>(std::chrono::system_clock::to_time_t(now));
                    const std::string formattedTime = formatTime(secondsSinceEpoch);
                    outputDirName += std::string(1, NV_PERF_PATH_SEPARATOR) + formattedTime;
                }
                if (outputDirName.length() > 0 && outputDirName.back() != NV_PERF_PATH_SEPARATOR)
                {
                    outputDirName += NV_PERF_PATH_SEPARATOR;
                }
                for (size_t di = 0; di < outputDirName.length(); ++di)
                {
                    if (outputDirName[di] == NV_PERF_PATH_SEPARATOR)
                    {
                        std::string parentDir(outputDirName, 0, di);
#ifdef WIN32
                        BOOL dirCreated = CreateDirectoryA(parentDir.c_str(), NULL);
#else
                        bool dirCreated = !mkdir(parentDir.c_str(), 0777);
#endif
                        if (!dirCreated)
                        {
                            /* it probably already exists */
                        }
                    }
                }
                m_outputDirectory = outputDirName;

                bool success = m_periodicSamplerGpu.StartSampling();
                if (!success)
                {
                    m_inCollection = false;
                    NV_PERF_LOG_ERR(20, "GpuPeriodicSampler::StartSampling failed\n");
                    return false;
                }
                m_numFramesSampled = 0;
                m_status = SamplerStatus::Sampling;
            }
            return true;
        }
        bool Reset()
        {
            m_periodicSamplerGpu.Reset();
            m_counterData.clear();
            m_counterConfiguration = {};
            m_outputDirectory.clear();
            m_status = SamplerStatus::Uninitialized;
            m_inCollection = false;
            m_samplingIntervalNs = 0;
            m_maxDurationNs = 0;
            return true;
        }
    private:
        bool InitializeCounterData()
        {
            const uint32_t maxSamples = uint32_t(m_maxDurationNs / m_samplingIntervalNs);
            m_counterData.clear();
            size_t deviceIndex = m_periodicSamplerGpu.GetDeviceIndex();
            bool success = GpuPeriodicSamplerCreateCounterData(
                    deviceIndex, m_counterConfiguration.counterDataPrefix.data(), m_counterConfiguration.counterDataPrefix.size(),
                    maxSamples, NVPW_PERIODIC_SAMPLER_COUNTER_DATA_APPEND_MODE_LINEAR, m_counterData
            );
            if (!success)
            {
                NV_PERF_LOG_ERR(20, "GpuPeriodicSamplerCreateCounterData failed\n");
                return false;
            }
            return true;
        }
    };
}}}
