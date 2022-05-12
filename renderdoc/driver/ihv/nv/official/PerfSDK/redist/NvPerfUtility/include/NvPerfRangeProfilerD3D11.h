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

#include "NvPerfRangeProfiler.h"
#include "NvPerfD3D11.h"

namespace nv { namespace perf { namespace profiler {

    class RangeProfilerD3D11
    {
    private:
        struct ProfilerApi : RangeProfilerStateMachine::IProfilerApi
        {
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> pDeviceContext;
            SessionOptions sessionOptions;

            ProfilerApi()
                : pDeviceContext(nullptr)
                , sessionOptions()
            {
            }

            virtual bool CreateCounterData(const SetConfigParams& config, std::vector<uint8_t>& counterDataImage, std::vector<uint8_t>& counterDataScratch) const override
            {
                NVPA_Status nvpaStatus;

                NVPW_D3D11_Profiler_CounterDataImageOptions counterDataImageOptions = { NVPW_D3D11_Profiler_CounterDataImageOptions_STRUCT_SIZE };
                counterDataImageOptions.pCounterDataPrefix = config.pCounterDataPrefix;
                counterDataImageOptions.counterDataPrefixSize = config.counterDataPrefixSize;
                counterDataImageOptions.maxNumRanges = static_cast<uint32_t>(sessionOptions.maxNumRanges);
                counterDataImageOptions.maxNumRangeTreeNodes = static_cast<uint32_t>(2 * sessionOptions.maxNumRanges);
                counterDataImageOptions.maxRangeNameLength = static_cast<uint32_t>(sessionOptions.avgRangeNameLength);

                NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Params calculateSizeParams = { NVPW_D3D11_Profiler_CounterDataImage_CalculateSize_Params_STRUCT_SIZE };
                calculateSizeParams.counterDataImageOptionsSize = NVPW_D3D11_Profiler_CounterDataImageOptions_STRUCT_SIZE;
                calculateSizeParams.pOptions = &counterDataImageOptions;
                nvpaStatus = NVPW_D3D11_Profiler_CounterDataImage_CalculateSize(&calculateSizeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                counterDataImage.resize(calculateSizeParams.counterDataImageSize);

                NVPW_D3D11_Profiler_CounterDataImage_Initialize_Params initializeParams = { NVPW_D3D11_Profiler_CounterDataImage_Initialize_Params_STRUCT_SIZE };
                initializeParams.counterDataImageOptionsSize = NVPW_D3D11_Profiler_CounterDataImageOptions_STRUCT_SIZE;
                initializeParams.pOptions = &counterDataImageOptions;
                initializeParams.counterDataImageSize = calculateSizeParams.counterDataImageSize;
                initializeParams.pCounterDataImage = &counterDataImage[0];
                nvpaStatus = NVPW_D3D11_Profiler_CounterDataImage_Initialize(&initializeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Params scratchBufferSizeParams = { NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize_Params_STRUCT_SIZE };
                scratchBufferSizeParams.counterDataImageSize = calculateSizeParams.counterDataImageSize;
                scratchBufferSizeParams.pCounterDataImage = initializeParams.pCounterDataImage;
                nvpaStatus = NVPW_D3D11_Profiler_CounterDataImage_CalculateScratchBufferSize(&scratchBufferSizeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                counterDataScratch.resize(scratchBufferSizeParams.counterDataScratchBufferSize);

                NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Params initScratchBufferParams = { NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer_Params_STRUCT_SIZE };
                initScratchBufferParams.counterDataImageSize = calculateSizeParams.counterDataImageSize;
                initScratchBufferParams.pCounterDataImage = initializeParams.pCounterDataImage;
                initScratchBufferParams.counterDataScratchBufferSize = scratchBufferSizeParams.counterDataScratchBufferSize;
                initScratchBufferParams.pCounterDataScratchBuffer = &counterDataScratch[0];
                nvpaStatus = NVPW_D3D11_Profiler_CounterDataImage_InitializeScratchBuffer(&initScratchBufferParams);
                if (nvpaStatus)
                {
                    return false;
                }

                return true;
            }

            virtual bool SetConfig(const SetConfigParams& config) const override
            {
                NVPW_D3D11_Profiler_DeviceContext_SetConfig_Params setConfigParams = { NVPW_D3D11_Profiler_DeviceContext_SetConfig_Params_STRUCT_SIZE };
                setConfigParams.pDeviceContext = pDeviceContext.Get();
                setConfigParams.pConfig = config.pConfigImage;
                setConfigParams.configSize = config.configImageSize;
                setConfigParams.minNestingLevel = 1;
                setConfigParams.numNestingLevels = config.numNestingLevels;
                setConfigParams.passIndex = 0;
                setConfigParams.targetNestingLevel = 1;
                NVPA_Status nvpaStatus = NVPW_D3D11_Profiler_DeviceContext_SetConfig(&setConfigParams);
                if (nvpaStatus)
                {
                    return false;
                }

                return true;
            }

            virtual bool BeginPass() const override
            {
                NVPW_D3D11_Profiler_DeviceContext_BeginPass_Params beginPassParams = { NVPW_D3D11_Profiler_DeviceContext_BeginPass_Params_STRUCT_SIZE };
                beginPassParams.pDeviceContext = pDeviceContext.Get();
                NVPA_Status nvpaStatus = NVPW_D3D11_Profiler_DeviceContext_BeginPass(&beginPassParams);
                if (nvpaStatus)
                {
                    return false;
                }
                return true;
            }

            virtual bool EndPass() const override
            {
                NVPW_D3D11_Profiler_DeviceContext_EndPass_Params endPassParams = { NVPW_D3D11_Profiler_DeviceContext_EndPass_Params_STRUCT_SIZE };
                endPassParams.pDeviceContext = pDeviceContext.Get();
                NVPA_Status nvpaStatus = NVPW_D3D11_Profiler_DeviceContext_EndPass(&endPassParams);
                if (nvpaStatus)
                {
                    return false;
                }
                return true;
            }

            virtual bool PushRange(const char* pRangeName) override
            {
                NVPW_D3D11_Profiler_DeviceContext_PushRange_Params pushRangeParams = { NVPW_D3D11_Profiler_DeviceContext_PushRange_Params_STRUCT_SIZE };
                pushRangeParams.pDeviceContext = pDeviceContext.Get();
                pushRangeParams.pRangeName = pRangeName;
                pushRangeParams.rangeNameLength = 0;
                NVPA_Status nvpaStatus = NVPW_D3D11_Profiler_DeviceContext_PushRange(&pushRangeParams);
                if (nvpaStatus)
                {
                    return false;
                }
                return true;
            }

            virtual bool PopRange() override
            {
                NVPW_D3D11_Profiler_DeviceContext_PopRange_Params popParams = { NVPW_D3D11_Profiler_DeviceContext_PopRange_Params_STRUCT_SIZE };
                popParams.pDeviceContext = pDeviceContext.Get();
                NVPA_Status nvpaStatus = NVPW_D3D11_Profiler_DeviceContext_PopRange(&popParams);
                if (nvpaStatus)
                {
                    return false;
                }
                return true;
            }

            virtual bool DecodeCounters(std::vector<uint8_t>& counterDataImage, std::vector<uint8_t>& counterDataScratch, bool& onePassDecoded, bool& allPassesDecoded) const
            {
                NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Params decodeParams = { NVPW_D3D11_Profiler_DeviceContext_DecodeCounters_Params_STRUCT_SIZE };
                decodeParams.pDeviceContext = pDeviceContext.Get();
                decodeParams.counterDataImageSize = counterDataImage.size();
                decodeParams.pCounterDataImage = counterDataImage.data();
                decodeParams.counterDataScratchBufferSize = counterDataScratch.size();
                decodeParams.pCounterDataScratchBuffer = counterDataScratch.data();
                NVPA_Status nvpaStatus = NVPW_D3D11_Profiler_DeviceContext_DecodeCounters(&decodeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                onePassDecoded = decodeParams.onePassCollected;
                allPassesDecoded = decodeParams.allPassesCollected;
                return true;
            }

            bool Initialize(ID3D11DeviceContext* pDeviceContext_, SessionOptions sessionOptions_)
            {
                pDeviceContext = pDeviceContext_;
                sessionOptions = sessionOptions_;
            }

            void Reset()
            {
                NVPW_D3D11_Profiler_DeviceContext_EndSession_Params endSessionParams = {NVPW_D3D11_Profiler_DeviceContext_EndSession_Params_STRUCT_SIZE};
                endSessionParams.pDeviceContext = pDeviceContext.Get();
                NVPA_Status nvpaStatus = NVPW_D3D11_Profiler_DeviceContext_EndSession(&endSessionParams);
                if (nvpaStatus)
                {
                    NV_PERF_LOG_ERR(10, "NVPW_D3D11_Profiler_DeviceContext_EndSession failed, nvpaStatus = %d\n", nvpaStatus);
                }

                sessionOptions = {};
                pDeviceContext = nullptr;
            }
        };

    private:
        ProfilerApi m_profilerApi;
        RangeProfilerStateMachine m_stateMachine;

    public:
        ~RangeProfilerD3D11()
        {
        }

        RangeProfilerD3D11(const RangeProfilerD3D11&) = delete;

        RangeProfilerD3D11()
            : m_profilerApi()
            , m_stateMachine(m_profilerApi)
        {
        }
        // TODO: make this move friendly

        RangeProfilerD3D11& operator=(const RangeProfilerD3D11&) = delete;

        bool IsInSession() const
        {
            return !!m_profilerApi.pDeviceContext;
        }

        bool IsInPass() const
        {
            return m_stateMachine.IsInPass();
        }

        ID3D11DeviceContext* GetDeviceContext() const
        {
            return m_profilerApi.pDeviceContext.Get();
        }

        bool BeginSession(ID3D11DeviceContext* pDeviceContext, const SessionOptions& sessionOptions)
        {
            if (IsInSession())
            {
                NV_PERF_LOG_ERR(10, "already in a session\n");
                return false;
            }
            if (pDeviceContext->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE)
            {
                NV_PERF_LOG_ERR(10, "profiling is only supported on immediate device contexts\n");
                return false;
            }
            if (!nv::perf::D3D11IsNvidiaDevice(pDeviceContext) || !nv::perf::profiler::D3D11IsGpuSupported(pDeviceContext))
            {
                NV_PERF_LOG_ERR(10, "device is not supported for profiling\n");
                return false;
            }

            NVPA_Status nvpaStatus;

            NVPW_D3D11_Profiler_CalcTraceBufferSize_Params calcTraceBufferSizeParam = { NVPW_D3D11_Profiler_CalcTraceBufferSize_Params_STRUCT_SIZE };
            calcTraceBufferSizeParam.maxRangesPerPass = sessionOptions.maxNumRanges;
            calcTraceBufferSizeParam.avgRangeNameLength = sessionOptions.avgRangeNameLength;
            nvpaStatus = NVPW_D3D11_Profiler_CalcTraceBufferSize(&calcTraceBufferSizeParam);
            if (nvpaStatus)
            {
                return false;
            }

            NVPW_D3D11_Profiler_DeviceContext_BeginSession_Params beginSessionParams = { NVPW_D3D11_Profiler_DeviceContext_BeginSession_Params_STRUCT_SIZE };
            beginSessionParams.pDeviceContext = pDeviceContext;
            beginSessionParams.numTraceBuffers = sessionOptions.numTraceBuffers;
            beginSessionParams.traceBufferSize = calcTraceBufferSizeParam.traceBufferSize;
            beginSessionParams.maxRangesPerPass = sessionOptions.maxNumRanges;
            beginSessionParams.maxLaunchesPerPass = sessionOptions.maxNumRanges;
            nvpaStatus = NVPW_D3D11_Profiler_DeviceContext_BeginSession(&beginSessionParams);
            if (nvpaStatus)
            {
                if (nvpaStatus == NVPA_STATUS_INSUFFICIENT_PRIVILEGE)
                {
                    NV_PERF_LOG_ERR(10, "Failed to start profiler session: profiling permissions not enabled.  Please follow these instructions: https://developer.nvidia.com/ERR_NVGPUCTRPERM\n");
                }
                else if (nvpaStatus == NVPA_STATUS_INSUFFICIENT_DRIVER_VERSION)
                {
                    NV_PERF_LOG_ERR(10, "Failed to start profiler session: insufficient driver version.  Please install the latest NVIDIA driver from https://www.nvidia.com\n");
                }
                else if(nvpaStatus == NVPA_STATUS_RESOURCE_UNAVAILABLE)
                {
                    NV_PERF_LOG_ERR(10, "Failed to start profiler session: resource conflict - only one profiler session can run at a time per GPU.\n");
                }
                else if(nvpaStatus == NVPA_STATUS_INVALID_OBJECT_STATE)
                {
                    NV_PERF_LOG_ERR(10, "Failed to start profiler session: a profiler session already exists.\n");
                }
                else
                {
                    NV_PERF_LOG_ERR(10, "Failed to start profiler session: unknown error.");
                }
                return false;
            }

            m_profilerApi.sessionOptions = sessionOptions;
            m_profilerApi.pDeviceContext = pDeviceContext;
            return true;
        }

        bool EndSession()
        {
            if (!IsInSession())
            {
                NV_PERF_LOG_ERR(10, "must be called in a session\n");
                return false;
            }

            m_stateMachine.Reset();
            m_profilerApi.Reset();
            return true;
        }

        bool EnqueueCounterCollection(const SetConfigParams& config)
        {
            const bool status = m_stateMachine.EnqueueCounterCollection(config);
            return status;
        }

        bool EnqueueCounterCollection(const CounterConfiguration& configuration, uint16_t numNestingLevels = 1, size_t numStatisticalSamples = 1)
        {
            const bool status = m_stateMachine.EnqueueCounterCollection(SetConfigParams(configuration, numNestingLevels, numStatisticalSamples));
            return status;
        }

        bool BeginPass()
        {
            if (!IsInSession())
            {
                NV_PERF_LOG_ERR(10, "must be called in a session\n");
                return false;
            }

            const bool status = m_stateMachine.BeginPass();
            return status;
        }

        bool EndPass()
        {
            if (!IsInSession())
            {
                NV_PERF_LOG_ERR(10, "must be called in a session\n");
                return false;
            }

            const bool status = m_stateMachine.EndPass();
            return status;
        }

        bool PushRange(const char* pRangeName)
        {
            const bool status = m_stateMachine.PushRange(pRangeName);
            return status;
        }

        bool PopRange()
        {
            const bool status = m_stateMachine.PopRange();
            return status;
        }

        bool DecodeCounters(DecodeResult& decodeResult)
        {
            if (!IsInSession())
            {
                NV_PERF_LOG_ERR(10, "must be called in a session\n");
                return false;
            }

            const bool status = m_stateMachine.DecodeCounters(decodeResult);
            return status;
        }

        bool AllPassesSubmitted() const
        {
            const bool allPassesSubmitted = m_stateMachine.AllPassesSubmitted();
            return allPassesSubmitted;
        }
    };
}}}
