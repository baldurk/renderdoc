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

#include <cassert>
#include <iterator>
#include <vector>
#include <set>
#include <algorithm>
#include "nvperf_device_target.h"
#include "NvPerfInit.h"
#include "NvPerfDeviceProperties.h"
#include "NvPerfPeriodicSamplerCommon.h"

namespace nv { namespace perf { namespace sampler {

    inline bool GpuPeriodicSamplerIsGpuSupported(size_t deviceIndex)
    {
        NVPW_GPU_PeriodicSampler_IsGpuSupported_Params isGpuSupportedParams = { NVPW_GPU_PeriodicSampler_IsGpuSupported_Params_STRUCT_SIZE };
        isGpuSupportedParams.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_IsGpuSupported(&isGpuSupportedParams);
        if (nvpaStatus != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "NVPW_GPU_PeriodicSampler_IsGpuSupported failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, deviceIndex);
            return false;
        }

        if (!isGpuSupportedParams.isSupported)
        {
            const DeviceIdentifiers deviceIdentifiers = GetDeviceIdentifiers(deviceIndex);
            NV_PERF_LOG_ERR(10, "%s is not supported for profiling\n", deviceIdentifiers.pDeviceName ? deviceIdentifiers.pDeviceName : "Unknown device");
            if (isGpuSupportedParams.gpuArchitectureSupportLevel != NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_SUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Unsupported GPU architecture %s\n", deviceIdentifiers.pChipName ? deviceIdentifiers.pChipName : "");
            }
            if (isGpuSupportedParams.sliSupportLevel == NVPW_SLI_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Devices in SLI configuration are not supported.\n");
            }
            if (isGpuSupportedParams.cmpSupportLevel == NVPW_CMP_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Cryptomining GPUs (NVIDIA CMP) are not supported.\n");
            }
            return false;
        }
        return true;
    }

    inline bool GpuPeriodicSamplerIsKeepLatestModeSupported(size_t deviceIndex)
    {
        NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Params isSupportedParams = { NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported_Params_STRUCT_SIZE };
        isSupportedParams.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported(&isSupportedParams);
        if (nvpaStatus != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "NVPW_GPU_PeriodicSampler_IsRecordBufferKeepLatestModeSupported failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, deviceIndex);
            return false;
        }
        if (!isSupportedParams.isSupported)
        {
            return false;
        }
        return true;
    }

    inline bool GpuPeriodicSamplerGetSupportedTriggers(size_t deviceIndex, std::set<NVPW_GPU_PeriodicSampler_TriggerSource>& supportedTriggers)
    {
        NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Params getSupportedTriggerSourcesParams = { NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources_Params_STRUCT_SIZE };
        getSupportedTriggerSourcesParams.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources(&getSupportedTriggerSourcesParams);
        if (nvpaStatus != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, deviceIndex);
            return false;
        }
        std::vector<uint32_t> supportedTriggersU32(getSupportedTriggerSourcesParams.numTriggerSources);
        getSupportedTriggerSourcesParams.pTriggerSources = supportedTriggersU32.data();
        nvpaStatus = NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources(&getSupportedTriggerSourcesParams);
        if (nvpaStatus != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(10, "NVPW_GPU_PeriodicSampler_GetSupportedTriggerSources failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, deviceIndex);
            return false;
        }
        supportedTriggers.clear();
        std::transform(supportedTriggersU32.begin(), supportedTriggersU32.end(), std::inserter(supportedTriggers, supportedTriggers.end()), [](uint32_t trigger) {
            return static_cast<NVPW_GPU_PeriodicSampler_TriggerSource>(trigger);
        });
        return true;
    }

    // if configImage is empty, then it will be calculated based on the maximum number of counter collection units in the system
    inline bool GpuPeriodicSamplerCalculateRecordBufferSize(size_t deviceIndex, const std::vector<uint8_t>& configImage, size_t maxNumUndecodedSamples, size_t& recordBufferSize)
    {
        NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Params calculateRecordBufferSizeParams = { NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize_Params_STRUCT_SIZE };
        calculateRecordBufferSizeParams.deviceIndex = deviceIndex;
        if (!configImage.empty())
        {
            calculateRecordBufferSizeParams.pConfig = &configImage[0];
            calculateRecordBufferSizeParams.configSize = configImage.size();
        }
        calculateRecordBufferSizeParams.maxNumUndecodedSamples = maxNumUndecodedSamples;
        const NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize(&calculateRecordBufferSizeParams);
        if (nvpaStatus != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(20, "NVPW_GPU_PeriodicSampler_CalculateRecordBufferSize failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, deviceIndex);
            return false;
        }
        recordBufferSize = calculateRecordBufferSizeParams.recordBufferSize;
        return true;
    }

    inline bool GpuPeriodicSamplerGetCounterAvailability(size_t deviceIndex, std::vector<uint8_t>& counterAvailabilityImage)
    {
        NVPW_GPU_PeriodicSampler_GetCounterAvailability_Params getCounterAvailabilityParams = { NVPW_GPU_PeriodicSampler_GetCounterAvailability_Params_STRUCT_SIZE };
        getCounterAvailabilityParams.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_GetCounterAvailability(&getCounterAvailabilityParams);
        if (nvpaStatus != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(20, "NVPW_GPU_PeriodicSampler_GetCounterAvailability failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, deviceIndex);
            return false;
        }
        counterAvailabilityImage.clear();
        counterAvailabilityImage.resize(getCounterAvailabilityParams.counterAvailabilityImageSize);
        getCounterAvailabilityParams.pCounterAvailabilityImage = counterAvailabilityImage.data();
        nvpaStatus = NVPW_GPU_PeriodicSampler_GetCounterAvailability(&getCounterAvailabilityParams);
        if (nvpaStatus != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(20, "NVPW_GPU_PeriodicSampler_GetCounterAvailability failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, deviceIndex);
            return false;
        }
        return true;
    }

    inline bool GpuPeriodicSamplerCreateCounterData(
        size_t deviceIndex,
        const uint8_t* pCounterDataPrefix,
        size_t counterDataPrefixSize,
        uint32_t maxSamples,
        NVPW_PeriodicSampler_CounterData_AppendMode appendMode,
        std::vector<uint8_t>& counterData)
    {
        NVPW_GPU_PeriodicSampler_CounterDataImageOptions options = { NVPW_GPU_PeriodicSampler_CounterDataImageOptions_STRUCT_SIZE };
        options.pCounterDataPrefix = pCounterDataPrefix;
        options.counterDataPrefixSize = counterDataPrefixSize;
        options.maxSamples = maxSamples;
        options.appendMode = static_cast<uint32_t>(appendMode);

        NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Params calculateSizeParams = { NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize_Params_STRUCT_SIZE };
        calculateSizeParams.deviceIndex = deviceIndex;
        calculateSizeParams.pOptions = &options;
        NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize(&calculateSizeParams);
        if (nvpaStatus != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(30, "NVPW_GPU_PeriodicSampler_CounterDataImage_CalculateSize failed, nvpaStatus = %d\n", nvpaStatus);
            return false;
        }
        counterData.resize(calculateSizeParams.counterDataImageSize);

        NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Params initializeParams = { NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize_Params_STRUCT_SIZE };
        initializeParams.deviceIndex = deviceIndex;
        initializeParams.pOptions = &options;
        initializeParams.pCounterDataImage = counterData.data();
        initializeParams.counterDataImageSize = counterData.size();
        nvpaStatus = NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize(&initializeParams);
        if (nvpaStatus != NVPA_STATUS_SUCCESS)
        {
            NV_PERF_LOG_ERR(30, "NVPW_GPU_PeriodicSampler_CounterDataImage_Initialize failed, nvpaStatus = %d\n", nvpaStatus);
            return false;
        }
        return true;
    }

    class GpuPeriodicSampler
    {
    public:
        struct GpuPulseSamplingInterval
        {
            uint32_t samplingInterval;
            NVPW_GPU_PeriodicSampler_TriggerSource triggerSource;

            GpuPulseSamplingInterval()
                : samplingInterval()
                , triggerSource(NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_INVALID)
            {
            }
        };

    private:
        size_t m_deviceIndex;
        DeviceIdentifiers m_deviceIdentifiers;
        std::set<NVPW_GPU_PeriodicSampler_TriggerSource> m_supportedTriggers;
        bool m_inSession;
        bool m_isInitialized;

    public:
        GpuPeriodicSampler()
            : m_deviceIndex(size_t(~0))
            , m_deviceIdentifiers({ "", "" })
            , m_inSession()
            , m_isInitialized()
        {
        }
        GpuPeriodicSampler(const GpuPeriodicSampler& sampler) = delete;
        GpuPeriodicSampler(GpuPeriodicSampler&& sampler)
            : m_deviceIndex(sampler.m_deviceIndex)
            , m_deviceIdentifiers(sampler.m_deviceIdentifiers)
            , m_supportedTriggers(std::move(sampler.m_supportedTriggers))
            , m_inSession(sampler.m_inSession)
            , m_isInitialized(sampler.m_isInitialized)
        {
            sampler.m_isInitialized = false;
        }
        ~GpuPeriodicSampler()
        {
            Reset();
        }
        GpuPeriodicSampler& operator=(const GpuPeriodicSampler& sampler) = delete;
        GpuPeriodicSampler& operator=(GpuPeriodicSampler&& sampler)
        {
            Reset();
            m_deviceIndex = sampler.m_deviceIndex;
            m_deviceIdentifiers = sampler.m_deviceIdentifiers;
            m_supportedTriggers = std::move(sampler.m_supportedTriggers);
            m_inSession = sampler.m_inSession;
            m_isInitialized = sampler.m_isInitialized;
            sampler.m_isInitialized = false;
            return *this;
        }

        bool Initialize(size_t deviceIndex)
        {
            if (!GpuPeriodicSamplerIsGpuSupported(deviceIndex))
            {
                NV_PERF_LOG_ERR(10, "GPU Periodic Sampler is not supported on the current device, deviceIndex = %llu\n", deviceIndex);
                return false;
            }
            if (!GpuPeriodicSamplerGetSupportedTriggers(deviceIndex, m_supportedTriggers))
            {
                return false;
            }
            m_deviceIndex = deviceIndex;
            m_deviceIdentifiers = nv::perf::GetDeviceIdentifiers(deviceIndex);
            m_isInitialized = true;
            return true;
        }

        void Reset()
        {
            if (m_isInitialized)
            {
                if (m_inSession)
                {
                    EndSession();
                    m_inSession = false;
                }
                m_deviceIndex = size_t(~0);
                m_deviceIdentifiers = { "", "" };
                m_supportedTriggers.clear();
                m_isInitialized = false;
            }
        }

        bool IsInitialized() const
        {
            return m_isInitialized;
        }

        size_t GetDeviceIndex() const
        {
            return m_deviceIndex;
        };

        const DeviceIdentifiers& GetDeviceIdentifiers() const
        {
            return m_deviceIdentifiers;
        }

        const std::set<NVPW_GPU_PeriodicSampler_TriggerSource>& GetSupportedTriggers() const
        {
            return m_supportedTriggers;
        }

        bool IsTriggerSupported(NVPW_GPU_PeriodicSampler_TriggerSource trigger) const
        {
            const auto itr = std::find(m_supportedTriggers.begin(), m_supportedTriggers.end(), trigger);
            const bool isSupported = (itr != m_supportedTriggers.end());
            return isSupported;
        }

        GpuPulseSamplingInterval GetGpuPulseSamplingInterval(uint32_t samplingIntervalInNanoSeconds) const
        {
            GpuPulseSamplingInterval samplingInterval;
            if (IsTriggerSupported(NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_TIME_INTERVAL))
            {
                samplingInterval.triggerSource = NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_TIME_INTERVAL;
                samplingInterval.samplingInterval = samplingIntervalInNanoSeconds;
            }
            else
            {
                assert(IsTriggerSupported(NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_SYSCLK_INTERVAL));
                samplingInterval.triggerSource = NVPW_GPU_PERIODIC_SAMPLER_TRIGGER_SOURCE_GPU_SYSCLK_INTERVAL;
                const uint32_t MAX_SYS_FREQ = 3u * 1000 * 1000 * 1000; // 3 GHZ
                samplingInterval.samplingInterval = samplingIntervalInNanoSeconds * (MAX_SYS_FREQ / (1000 * 1000 * 1000));
            }
            return samplingInterval;
        }

        bool BeginSession(
            size_t recordBufferSize,
            size_t maxNumUndecodedSamplingRanges, // must be 1
            const std::vector<NVPW_GPU_PeriodicSampler_TriggerSource>& enabledTriggerSources,
            uint64_t samplingInterval,
            NVPW_GPU_PeriodicSampler_RecordBuffer_AppendMode recordBufferAppendMode = NVPW_GPU_PERIODIC_SAMPLER_RECORD_BUFFER_APPEND_MODE_KEEP_OLDEST)
        {
            if (!m_isInitialized)
            {
                NV_PERF_LOG_ERR(20, "GpuPeriodicSampler is not initialized\n");
                return false;
            }
            for (NVPW_GPU_PeriodicSampler_TriggerSource triggerSource : enabledTriggerSources)
            {
                if (!IsTriggerSupported(triggerSource))
                {
                    NV_PERF_LOG_ERR(20, "Trigger source is not supported on the current GPU, triggerSource = %u, deviceIndex = %llu\n", triggerSource, m_deviceIndex);
                    return false;
                }
            }
            if ((recordBufferAppendMode == NVPW_GPU_PERIODIC_SAMPLER_RECORD_BUFFER_APPEND_MODE_KEEP_LATEST) && !GpuPeriodicSamplerIsKeepLatestModeSupported(m_deviceIndex))
            {
                NV_PERF_LOG_ERR(10, "Record buffer keep latest mode is not supported on the current GPU, deviceIndex = %llu\n", m_deviceIndex);
                return false;
            }
            std::vector<uint32_t> enabledTriggerSourcesU32;
            std::transform(enabledTriggerSources.begin(), enabledTriggerSources.end(), std::back_inserter(enabledTriggerSourcesU32), [](NVPW_GPU_PeriodicSampler_TriggerSource triggerSource) {
                return static_cast<uint32_t>(triggerSource);
            });
            NVPW_GPU_PeriodicSampler_BeginSession_V2_Params beginSessionParams = { NVPW_GPU_PeriodicSampler_BeginSession_V2_Params_STRUCT_SIZE };
            beginSessionParams.deviceIndex = m_deviceIndex;
            beginSessionParams.maxNumUndecodedSamplingRanges = maxNumUndecodedSamplingRanges;
            beginSessionParams.pTriggerSources = enabledTriggerSourcesU32.data();
            beginSessionParams.numTriggerSources = enabledTriggerSourcesU32.size();
            beginSessionParams.samplingInterval = samplingInterval;
            beginSessionParams.recordBufferSize = recordBufferSize;
            beginSessionParams.recordBufferAppendMode = recordBufferAppendMode;
            const NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_BeginSession_V2(&beginSessionParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(20, "NVPW_GPU_PeriodicSampler_BeginSession_V2 failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, m_deviceIndex);
                return false;
            }
            m_inSession = true;
            return true;
        }

        bool EndSession()
        {
            if (!m_inSession)
            {
                NV_PERF_LOG_ERR(20, "EndSession() called, but not in a session\n");
                return false;
            }
            NVPW_GPU_PeriodicSampler_EndSession_Params endSessionParams = { NVPW_GPU_PeriodicSampler_EndSession_Params_STRUCT_SIZE };
            endSessionParams.deviceIndex = m_deviceIndex;
            const NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_EndSession(&endSessionParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(20, "NVPW_GPU_PeriodicSampler_EndSession failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, m_deviceIndex);
                return false;
            }
            m_inSession = false;
            return true;
        }

        bool SetConfig(const std::vector<uint8_t>& configImage, size_t passIndex)
        {
            if (!m_inSession)
            {
                NV_PERF_LOG_ERR(20, "SetConfig() called, but not in a session\n");
                return false;
            }
            NVPW_GPU_PeriodicSampler_SetConfig_Params setConfigParams = { NVPW_GPU_PeriodicSampler_SetConfig_Params_STRUCT_SIZE };
            setConfigParams.deviceIndex = m_deviceIndex;
            setConfigParams.pConfig = &configImage[0];
            setConfigParams.configSize = configImage.size();
            setConfigParams.passIndex = passIndex;
            const NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_SetConfig(&setConfigParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(20, "NVPW_GPU_PeriodicSampler_SetConfig failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, m_deviceIndex);
                return false;
            }
            return true;
        }

        bool StartSampling()
        {
            if (!m_inSession)
            {
                NV_PERF_LOG_ERR(20, "StartSampling() called, but not in a session\n");
                return false;
            }
            NVPW_GPU_PeriodicSampler_StartSampling_Params startSamplingParams = { NVPW_GPU_PeriodicSampler_StartSampling_Params_STRUCT_SIZE };
            startSamplingParams.deviceIndex = m_deviceIndex;
            const NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_StartSampling(&startSamplingParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(20, "NVPW_GPU_PeriodicSampler_StartSampling failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, m_deviceIndex);
                return false;
            }
            return true;
        }

        bool StopSampling()
        {
            if (!m_inSession)
            {
                NV_PERF_LOG_ERR(20, "StopSampling() called, but not in a session\n");
                return false;
            }
            NVPW_GPU_PeriodicSampler_StopSampling_Params stopSamplingParams = { NVPW_GPU_PeriodicSampler_StopSampling_Params_STRUCT_SIZE };
            stopSamplingParams.deviceIndex = m_deviceIndex;
            const NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_StopSampling(&stopSamplingParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(20, "NVPW_GPU_PeriodicSampler_StopSampling failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, m_deviceIndex);
                return false;
            }
            return true;
        }

        bool CpuTrigger()
        {
            if (!m_inSession)
            {
                NV_PERF_LOG_ERR(20, "CpuTrigger() called, but not in a session\n");
                return false;
            }
            NVPW_GPU_PeriodicSampler_CpuTrigger_Params cpuTriggerParams = { NVPW_GPU_PeriodicSampler_CpuTrigger_Params_STRUCT_SIZE };
            cpuTriggerParams.deviceIndex = m_deviceIndex;
            const NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_CpuTrigger(&cpuTriggerParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(20, "NVPW_GPU_PeriodicSampler_CpuTrigger failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, m_deviceIndex);
                return false;
            }
            return true;
        }

        bool GetRecordBufferStatus(size_t& totalSize, size_t& usedSize, bool& overflow)
        {
            if (!m_inSession)
            {
                NV_PERF_LOG_ERR(20, "GetRecordBufferStatus() called, but not in a session\n");
                return false;
            }
            NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Params getRecordBufferStatusParams = { NVPW_GPU_PeriodicSampler_GetRecordBufferStatus_Params_STRUCT_SIZE };
            getRecordBufferStatusParams.deviceIndex = m_deviceIndex;
            const NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_GetRecordBufferStatus(&getRecordBufferStatusParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(20, "NVPW_GPU_PeriodicSampler_GetRecordBufferStatus failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, m_deviceIndex);
                return false;
            }
            totalSize = getRecordBufferStatusParams.totalSize;
            usedSize = getRecordBufferStatusParams.usedSize;
            overflow = !!getRecordBufferStatusParams.overflow;
            return true;
        }

        bool DecodeCounters(
            std::vector<uint8_t>& counterDataImage,
            size_t numSamplingRangesToDecode, // must be 1
            size_t& numSamplingRangesDecoded,
            bool& recordBufferOverflow,
            size_t& numSamplesDropped,
            size_t& numSamplesMerged,
            bool doNotDropSamples = false)
        {
            if (!m_inSession)
            {
                NV_PERF_LOG_ERR(20, "DecodeCounters() called, but not in a session\n");
                return false;
            }
            NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Params decodeCountersParams = { NVPW_GPU_PeriodicSampler_DecodeCounters_V2_Params_STRUCT_SIZE };
            decodeCountersParams.deviceIndex = m_deviceIndex;
            decodeCountersParams.pCounterDataImage = counterDataImage.data();
            decodeCountersParams.counterDataImageSize = counterDataImage.size();
            decodeCountersParams.numRangesToDecode = numSamplingRangesToDecode;
            decodeCountersParams.doNotDropSamples = doNotDropSamples;
            const NVPA_Status nvpaStatus = NVPW_GPU_PeriodicSampler_DecodeCounters_V2(&decodeCountersParams);
            if (nvpaStatus)
            {
                NV_PERF_LOG_ERR(20, "NVPW_GPU_PeriodicSampler_DecodeCounters_V2 failed, nvpaStatus = %d, deviceIndex = %llu\n", nvpaStatus, m_deviceIndex);
                return false;
            }
            numSamplingRangesDecoded = decodeCountersParams.numRangesDecoded;
            recordBufferOverflow = !!decodeCountersParams.recordBufferOverflow;
            numSamplesDropped = decodeCountersParams.numSamplesDropped;
            numSamplesMerged = decodeCountersParams.numSamplesMerged;
            return true;
        }
    };

}}}
