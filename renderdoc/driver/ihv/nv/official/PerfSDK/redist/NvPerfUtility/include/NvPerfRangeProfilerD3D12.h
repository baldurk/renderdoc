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

#include <stdio.h>
#include <thread>
#include <vector>
#include "NvPerfInit.h"
#include "NvPerfCounterConfiguration.h"
#include "NvPerfRangeProfiler.h"
#include "NvPerfD3D12.h"

struct ID3D12CommandQueue;

namespace nv { namespace perf { namespace profiler {

    class RangeProfilerD3D12
    {
    protected:
        struct ProfilerApi : RangeProfilerStateMachine::IProfilerApi
        {
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> pCommandQueue;
            SessionOptions sessionOptions;

            ProfilerApi()
                : pCommandQueue(nullptr)
                , sessionOptions()
            {
            }

            virtual bool CreateCounterData(const SetConfigParams& config, std::vector<uint8_t>& counterDataImage, std::vector<uint8_t>& counterDataScratch) const override
            {
                NVPA_Status nvpaStatus;

                NVPW_D3D12_Profiler_CounterDataImageOptions counterDataImageOptions = { NVPW_D3D12_Profiler_CounterDataImageOptions_STRUCT_SIZE };
                counterDataImageOptions.pCounterDataPrefix = config.pCounterDataPrefix;
                counterDataImageOptions.counterDataPrefixSize = config.counterDataPrefixSize;
                counterDataImageOptions.maxNumRanges = static_cast<uint32_t>(sessionOptions.maxNumRanges);
                counterDataImageOptions.maxNumRangeTreeNodes = static_cast<uint32_t>(2 * sessionOptions.maxNumRanges);
                counterDataImageOptions.maxRangeNameLength = static_cast<uint32_t>(sessionOptions.avgRangeNameLength);

                NVPW_D3D12_Profiler_CounterDataImage_CalculateSize_Params calculateSizeParams = { NVPW_D3D12_Profiler_CounterDataImage_CalculateSize_Params_STRUCT_SIZE };
                calculateSizeParams.pOptions = &counterDataImageOptions;
                calculateSizeParams.counterDataImageOptionsSize = NVPW_D3D12_Profiler_CounterDataImageOptions_STRUCT_SIZE;
                nvpaStatus = NVPW_D3D12_Profiler_CounterDataImage_CalculateSize(&calculateSizeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                counterDataImage.resize(calculateSizeParams.counterDataImageSize);

                NVPW_D3D12_Profiler_CounterDataImage_Initialize_Params initializeParams = { NVPW_D3D12_Profiler_CounterDataImage_Initialize_Params_STRUCT_SIZE };
                initializeParams.counterDataImageOptionsSize = NVPW_D3D12_Profiler_CounterDataImageOptions_STRUCT_SIZE;
                initializeParams.pOptions = &counterDataImageOptions;
                initializeParams.counterDataImageSize = calculateSizeParams.counterDataImageSize;
                initializeParams.pCounterDataImage = &counterDataImage[0];
                nvpaStatus = NVPW_D3D12_Profiler_CounterDataImage_Initialize(&initializeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize_Params scratchBufferSizeParams = { NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize_Params_STRUCT_SIZE };
                scratchBufferSizeParams.counterDataImageSize = calculateSizeParams.counterDataImageSize;
                scratchBufferSizeParams.pCounterDataImage = initializeParams.pCounterDataImage;
                nvpaStatus = NVPW_D3D12_Profiler_CounterDataImage_CalculateScratchBufferSize(&scratchBufferSizeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                counterDataScratch.resize(scratchBufferSizeParams.counterDataScratchBufferSize);

                NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer_Params initScratchBufferParams = { NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer_Params_STRUCT_SIZE };
                initScratchBufferParams.counterDataImageSize = calculateSizeParams.counterDataImageSize;
                initScratchBufferParams.pCounterDataImage = initializeParams.pCounterDataImage;
                initScratchBufferParams.counterDataScratchBufferSize = scratchBufferSizeParams.counterDataScratchBufferSize;
                initScratchBufferParams.pCounterDataScratchBuffer = &counterDataScratch[0];

                nvpaStatus = NVPW_D3D12_Profiler_CounterDataImage_InitializeScratchBuffer(&initScratchBufferParams);
                if (nvpaStatus)
                {
                    return false;
                }

                return true;
            }

            virtual bool SetConfig(const SetConfigParams& config) const override
            {
                NVPW_D3D12_Profiler_Queue_SetConfig_Params setConfigParams = { NVPW_D3D12_Profiler_Queue_SetConfig_Params_STRUCT_SIZE };
                setConfigParams.pCommandQueue = pCommandQueue.Get();
                setConfigParams.pConfig = config.pConfigImage;
                setConfigParams.configSize = config.configImageSize;
                setConfigParams.minNestingLevel = 1;
                setConfigParams.numNestingLevels = config.numNestingLevels;
                setConfigParams.passIndex = 0;
                setConfigParams.targetNestingLevel = 1;
                NVPA_Status nvpaStatus = NVPW_D3D12_Profiler_Queue_SetConfig(&setConfigParams);
                if (nvpaStatus)
                {
                    return false;
                }

                return true;
            }

            virtual bool BeginPass() const override
            {
                NVPW_D3D12_Profiler_Queue_BeginPass_Params beginPassParams = { NVPW_D3D12_Profiler_Queue_BeginPass_Params_STRUCT_SIZE };
                beginPassParams.pCommandQueue = pCommandQueue.Get();
                NVPA_Status nvpaStatus = NVPW_D3D12_Profiler_Queue_BeginPass(&beginPassParams);
                if (nvpaStatus)
                {
                    return false;
                }
                return true;
            }

            virtual bool EndPass() const override
            {
                NVPW_D3D12_Profiler_Queue_EndPass_Params endPassParams = { NVPW_D3D12_Profiler_Queue_EndPass_Params_STRUCT_SIZE };
                endPassParams.pCommandQueue = pCommandQueue.Get();
                NVPA_Status nvpaStatus = NVPW_D3D12_Profiler_Queue_EndPass(&endPassParams);
                if (nvpaStatus)
                {
                    return false;
                }
                return true;
            }

            virtual bool PushRange(const char* pRangeName) override
            {
                NVPW_D3D12_Profiler_Queue_PushRange_Params pushRangeParams = {NVPW_D3D12_Profiler_Queue_PushRange_Params_STRUCT_SIZE};
                pushRangeParams.pRangeName = pRangeName;
                pushRangeParams.rangeNameLength = 0;
                pushRangeParams.pCommandQueue = pCommandQueue.Get();
                NVPA_Status nvpaStatus = NVPW_D3D12_Profiler_Queue_PushRange(&pushRangeParams);
                if (nvpaStatus)
                {
                    return false;
                }
                return true;
            }

            virtual bool PopRange() override
            {
                NVPW_D3D12_Profiler_Queue_PopRange_Params popParams = {NVPW_D3D12_Profiler_Queue_PopRange_Params_STRUCT_SIZE};
                popParams.pCommandQueue = pCommandQueue.Get();
                NVPA_Status nvpaStatus = NVPW_D3D12_Profiler_Queue_PopRange(&popParams);
                if (nvpaStatus)
                {
                    return false;
                }
                return true;
            }

            virtual bool DecodeCounters(std::vector<uint8_t>& counterDataImage, std::vector<uint8_t>& counterDataScratch, bool& onePassDecoded, bool& allPassesDecoded) const
            {
                NVPW_D3D12_Profiler_Queue_DecodeCounters_Params decodeParams = { NVPW_D3D12_Profiler_Queue_DecodeCounters_Params_STRUCT_SIZE };
                decodeParams.pCommandQueue = pCommandQueue.Get();
                decodeParams.counterDataImageSize = counterDataImage.size();
                decodeParams.pCounterDataImage = counterDataImage.data();
                decodeParams.counterDataScratchBufferSize = counterDataScratch.size();
                decodeParams.pCounterDataScratchBuffer = counterDataScratch.data();
                NVPA_Status nvpaStatus = NVPW_D3D12_Profiler_Queue_DecodeCounters(&decodeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                onePassDecoded = decodeParams.onePassCollected;
                allPassesDecoded = decodeParams.allPassesCollected;
                return true;
            }

            bool Initialize(ID3D12CommandQueue* pCommandQueue_, const SessionOptions& sessionOptions_)
            {
                pCommandQueue = pCommandQueue_;
                sessionOptions = sessionOptions_;
                return true;
            }

            void Reset()
            {
                NVPW_D3D12_Profiler_Queue_EndSession_Params endSessionParams = {NVPW_D3D12_Profiler_Queue_EndSession_Params_STRUCT_SIZE};
                endSessionParams.pCommandQueue = pCommandQueue.Get();
                endSessionParams.timeout = INFINITE;
                NVPA_Status nvpaStatus = NVPW_D3D12_Profiler_Queue_EndSession(&endSessionParams);
                if (nvpaStatus)
                {
                    NV_PERF_LOG_ERR(10, "NVPW_D3D12_Profiler_Queue_EndSession failed, nvpaStatus = %d\n", nvpaStatus);
                }

                sessionOptions = {};
                pCommandQueue = nullptr;
            }
        };

    protected: // members
        ProfilerApi m_profilerApi;
        RangeProfilerStateMachine m_stateMachine;
        std::thread m_spgoThread;
        volatile bool m_spgoThreadExited;

    private:
        // non-copyable
        RangeProfilerD3D12(const RangeProfilerD3D12&);

        static void SpgoThreadProc(RangeProfilerD3D12* pRangeProfilerD3D12, ID3D12CommandQueue* pCommandQueue)
        {
            // Run continuously in the background, handling all BeginPass and EndPass GPU operations until EndSession().
            NVPW_D3D12_Queue_ServicePendingGpuOperations_Params serviceGpuOpsParams = { NVPW_D3D12_Queue_ServicePendingGpuOperations_Params_STRUCT_SIZE };
            serviceGpuOpsParams.pCommandQueue = pCommandQueue;
            serviceGpuOpsParams.numOperations = 0; // run until EndSession()
            serviceGpuOpsParams.timeout = INFINITE;
            NVPA_Status nvpaStatus = NVPW_D3D12_Queue_ServicePendingGpuOperations(&serviceGpuOpsParams);
            if (nvpaStatus)
            {
                // TODO: log an error
            }

            pRangeProfilerD3D12->m_spgoThreadExited = true;
        }

    public:
        ~RangeProfilerD3D12()
        {
        }

        RangeProfilerD3D12()
            : m_profilerApi()
            , m_stateMachine(m_profilerApi)
            , m_spgoThread()
            , m_spgoThreadExited()
        {
        }
        // TODO: make this move friendly

        bool IsInSession() const
        {
            return !!m_profilerApi.pCommandQueue;
        }

        bool IsInPass() const
        {
            return m_stateMachine.IsInPass();
        }

        ID3D12CommandQueue* GetCommandQueue() const
        {
            return m_profilerApi.pCommandQueue.Get();
        }

        bool BeginSession(
            ID3D12CommandQueue* pCommandQueue,
            const SessionOptions& sessionOptions)
        {
            if (IsInSession())
            {
                NV_PERF_LOG_ERR(10, "already in a session\n");
                return false;
            }
            if (!D3D12IsNvidiaDevice(pCommandQueue) || !D3D12IsGpuSupported(pCommandQueue))
            {
                // TODO: error - device is not supported for profiling
                return false;
            }

            NVPA_Status nvpaStatus;

            NVPW_D3D12_Profiler_CalcTraceBufferSize_Params calcTraceBufferSizeParam = { NVPW_D3D12_Profiler_CalcTraceBufferSize_Params_STRUCT_SIZE };
            calcTraceBufferSizeParam.maxRangesPerPass = sessionOptions.maxNumRanges;
            calcTraceBufferSizeParam.avgRangeNameLength = sessionOptions.avgRangeNameLength;
            nvpaStatus = NVPW_D3D12_Profiler_CalcTraceBufferSize(&calcTraceBufferSizeParam);
            if (nvpaStatus)
            {
                return false;
            }

            NVPW_D3D12_Profiler_Queue_BeginSession_Params beginSessionParams = { NVPW_D3D12_Profiler_Queue_BeginSession_Params_STRUCT_SIZE };
            beginSessionParams.pCommandQueue = pCommandQueue;
            beginSessionParams.numTraceBuffers = sessionOptions.numTraceBuffers;
            beginSessionParams.traceBufferSize = calcTraceBufferSizeParam.traceBufferSize;
            beginSessionParams.maxRangesPerPass = sessionOptions.maxNumRanges;
            beginSessionParams.maxLaunchesPerPass = sessionOptions.maxNumRanges;
            nvpaStatus = NVPW_D3D12_Profiler_Queue_BeginSession(&beginSessionParams);
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

            m_spgoThreadExited = false;
            m_spgoThread = std::thread(SpgoThreadProc, this, pCommandQueue);

            m_profilerApi.Initialize(pCommandQueue, sessionOptions);
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
            m_spgoThread.join();
            m_spgoThreadExited = false;

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

        // Convenience method to start a Queue-level range.  For CommandLists, use D3D12RangeCommands::PushRange.
        bool PushRange(const char* pRangeName)
        {
            const bool status = m_stateMachine.PushRange(pRangeName);
            return status;
        }

        // Convenience method to end a Queue-level range.  For CommandLists, use D3D12RangeCommands::PopRange.
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

            if (m_spgoThreadExited)
            {
                NV_PERF_LOG_ERR(10, "the background thread exited; possible hang on subsequent CPU-waiting-on-GPU calls\n");
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
