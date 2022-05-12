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
#include <vector>
#include "NvPerfInit.h"
#include "NvPerfCounterConfiguration.h"
#include "NvPerfRangeProfiler.h"
#include "NvPerfOpenGL.h"

namespace nv { namespace perf { namespace profiler {

    class RangeProfilerOpenGL
    {
    protected:
        struct ProfilerApi : RangeProfilerStateMachine::IProfilerApi
        {
            size_t maxQueueRangesPerPass;
            size_t nextCommandBufferIdx;
            SessionOptions sessionOptions;
            NVPW_OpenGL_GraphicsContext* pGraphicsContext;

            ProfilerApi()
                : maxQueueRangesPerPass(1)
                , nextCommandBufferIdx()
                , sessionOptions()
                , pGraphicsContext()
            {
            }

            virtual bool CreateCounterData(const SetConfigParams& config, std::vector<uint8_t>& counterDataImage, std::vector<uint8_t>& counterDataScratch) const override
            {
                NVPA_Status nvpaStatus;

                NVPW_OpenGL_Profiler_CounterDataImageOptions counterDataImageOption = { NVPW_OpenGL_Profiler_CounterDataImageOptions_STRUCT_SIZE };
                counterDataImageOption.pCounterDataPrefix = config.pCounterDataPrefix;
                counterDataImageOption.counterDataPrefixSize = config.counterDataPrefixSize;
                counterDataImageOption.maxNumRanges = static_cast<uint32_t>(sessionOptions.maxNumRanges);
                counterDataImageOption.maxNumRangeTreeNodes = static_cast<uint32_t>(2 * sessionOptions.maxNumRanges);
                counterDataImageOption.maxRangeNameLength = static_cast<uint32_t>(sessionOptions.avgRangeNameLength);

                NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize_Params calculateSizeParams = { NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize_Params_STRUCT_SIZE };
                calculateSizeParams.pOptions = &counterDataImageOption;
                calculateSizeParams.counterDataImageOptionsSize = NVPW_OpenGL_Profiler_CounterDataImageOptions_STRUCT_SIZE;
                nvpaStatus = NVPW_OpenGL_Profiler_CounterDataImage_CalculateSize(&calculateSizeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                NVPW_OpenGL_Profiler_CounterDataImage_Initialize_Params initializeParams = { NVPW_OpenGL_Profiler_CounterDataImage_Initialize_Params_STRUCT_SIZE };
                initializeParams.counterDataImageOptionsSize = NVPW_OpenGL_Profiler_CounterDataImageOptions_STRUCT_SIZE;
                initializeParams.pOptions = &counterDataImageOption;
                initializeParams.counterDataImageSize = calculateSizeParams.counterDataImageSize;

                counterDataImage.resize(calculateSizeParams.counterDataImageSize);
                initializeParams.pCounterDataImage = &counterDataImage[0];
                nvpaStatus = NVPW_OpenGL_Profiler_CounterDataImage_Initialize(&initializeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize_Params scratchBufferSizeParams = { NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize_Params_STRUCT_SIZE };
                scratchBufferSizeParams.counterDataImageSize = calculateSizeParams.counterDataImageSize;
                scratchBufferSizeParams.pCounterDataImage = initializeParams.pCounterDataImage;
                nvpaStatus = NVPW_OpenGL_Profiler_CounterDataImage_CalculateScratchBufferSize(&scratchBufferSizeParams);
                if (nvpaStatus)
                {
                    return false;
                }
                counterDataScratch.resize(scratchBufferSizeParams.counterDataScratchBufferSize);

                NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer_Params initScratchBufferParams = { NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer_Params_STRUCT_SIZE };
                initScratchBufferParams.counterDataImageSize = calculateSizeParams.counterDataImageSize;
                initScratchBufferParams.pCounterDataImage = initializeParams.pCounterDataImage;
                initScratchBufferParams.counterDataScratchBufferSize = scratchBufferSizeParams.counterDataScratchBufferSize;
                initScratchBufferParams.pCounterDataScratchBuffer = &counterDataScratch[0];

                nvpaStatus = NVPW_OpenGL_Profiler_CounterDataImage_InitializeScratchBuffer(&initScratchBufferParams);
                if (nvpaStatus)
                {
                    return false;
                }

                return true;
            }

            virtual bool SetConfig(const SetConfigParams& config) const override
            {
                NVPW_OpenGL_Profiler_GraphicsContext_SetConfig_Params setConfigParams = { NVPW_OpenGL_Profiler_GraphicsContext_SetConfig_Params_STRUCT_SIZE };
                setConfigParams.pConfig = config.pConfigImage;
                setConfigParams.configSize = config.configImageSize;
                setConfigParams.minNestingLevel = 1;
                setConfigParams.numNestingLevels = config.numNestingLevels;
                setConfigParams.passIndex = 0;
                setConfigParams.targetNestingLevel = 1;
                NVPA_Status nvpaStatus = NVPW_OpenGL_Profiler_GraphicsContext_SetConfig(&setConfigParams);
                if (nvpaStatus)
                {
                    return false;
                }

                return true;
            }

            virtual bool BeginPass() const override
            {
                NVPW_OpenGL_Profiler_GraphicsContext_BeginPass_Params beginPassParams = { NVPW_OpenGL_Profiler_GraphicsContext_BeginPass_Params_STRUCT_SIZE };
                NVPA_Status nvpaStatus = NVPW_OpenGL_Profiler_GraphicsContext_BeginPass(&beginPassParams);
                if (nvpaStatus)
                {
                    return false;
                }
                return true;
            }

            virtual bool EndPass() const override
            {
                NVPW_OpenGL_Profiler_GraphicsContext_EndPass_Params endPassParams = { NVPW_OpenGL_Profiler_GraphicsContext_EndPass_Params_STRUCT_SIZE };
                NVPA_Status nvpaStatus = NVPW_OpenGL_Profiler_GraphicsContext_EndPass(&endPassParams);
                if (nvpaStatus)
                {
                    return false;
                }
                return true;
            }

            virtual bool PushRange(const char* pRangeName) override
            {
                NVPW_OpenGL_Profiler_GraphicsContext_PushRange_Params pushRangeParams = {NVPW_OpenGL_Profiler_GraphicsContext_PushRange_Params_STRUCT_SIZE};
                pushRangeParams.pRangeName = pRangeName;
                NVPA_Status nvpaStatus = NVPW_OpenGL_Profiler_GraphicsContext_PushRange(&pushRangeParams);
                if (nvpaStatus)
                {
                    NV_PERF_LOG_ERR(10, "NVPW_OpenGL_Profiler_GraphicsContext_PushRange failed, nvpaStatus = %d\n", nvpaStatus);
                    return false;
                }
                return true;
            }

            virtual bool PopRange() override
            {
                NVPW_OpenGL_Profiler_GraphicsContext_PopRange_Params popRangeParams = {NVPW_OpenGL_Profiler_GraphicsContext_PopRange_Params_STRUCT_SIZE};
                NVPA_Status nvpaStatus = NVPW_OpenGL_Profiler_GraphicsContext_PopRange(&popRangeParams);
                if (nvpaStatus)
                {
                    NV_PERF_LOG_ERR(10, "NVPW_OpenGL_Profiler_GraphicsContext_PopRange failed, nvpaStatus = %d\n", nvpaStatus);
                    return false;
                }
                return true;
            }
            virtual bool DecodeCounters(std::vector<uint8_t>& counterDataImage, std::vector<uint8_t>& counterDataScratch, bool& onePassDecoded, bool& allPassesDecoded) const
            {
                NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters_Params decodeParams = { NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters_Params_STRUCT_SIZE };
                decodeParams.counterDataImageSize = counterDataImage.size();
                decodeParams.pCounterDataImage = counterDataImage.data();
                decodeParams.counterDataScratchBufferSize = counterDataScratch.size();
                decodeParams.pCounterDataScratchBuffer = counterDataScratch.data();
                decodeParams.pGraphicsContext = pGraphicsContext;
                NVPA_Status nvpaStatus = NVPW_OpenGL_Profiler_GraphicsContext_DecodeCounters(&decodeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                onePassDecoded = decodeParams.onePassCollected;
                allPassesDecoded = decodeParams.allPassesCollected;
                return true;
            }

            bool Initialize(const SessionOptions& sessionOptions_)
            {
                NVPW_OpenGL_GetCurrentGraphicsContext_Params getCurrentGraphicsContextParams = {NVPW_OpenGL_GetCurrentGraphicsContext_Params_STRUCT_SIZE};
                NVPA_Status nvpaStatus = NVPW_OpenGL_GetCurrentGraphicsContext(&getCurrentGraphicsContextParams);
                if (nvpaStatus)
                {
                    return false;
                }
                pGraphicsContext = getCurrentGraphicsContextParams.pGraphicsContext;
                sessionOptions = sessionOptions_;
                return true;
            }

            void Reset()
            {
                NVPW_OpenGL_Profiler_GraphicsContext_EndSession_Params endSessionParams = {NVPW_OpenGL_Profiler_GraphicsContext_EndSession_Params_STRUCT_SIZE};
                NVPA_Status nvpaStatus = NVPW_OpenGL_Profiler_GraphicsContext_EndSession(&endSessionParams);
                if (nvpaStatus)
                {
                    NV_PERF_LOG_ERR(10, "NVPW_OpenGL_Profiler_GraphicsContext_EndSession failed, nvpaStatus = %d\n", nvpaStatus);
                }
                sessionOptions = {};
                pGraphicsContext = nullptr;
            }
        };

    protected: // members
        ProfilerApi m_profilerApi;
        RangeProfilerStateMachine m_stateMachine;

    private:
        // non-copyable
        RangeProfilerOpenGL(const RangeProfilerOpenGL&);

    public:
        ~RangeProfilerOpenGL()
        {
        }

        RangeProfilerOpenGL()
            : m_profilerApi()
            , m_stateMachine(m_profilerApi)
        {
        }

        bool IsInSession() const
        {
            return m_profilerApi.pGraphicsContext;
        }

        bool IsInPass() const
        {
            return m_stateMachine.IsInPass();
        }

        bool SetMaxQueueRangesPerPass(size_t maxQueueRangesPerPass)
        {
            if (IsInSession())
            {
                NV_PERF_LOG_ERR(10, "SetMaxQueueRangesPerPass must be called before the session starts.\n");
                return false;
            }
            m_profilerApi.maxQueueRangesPerPass = maxQueueRangesPerPass;
            return true;
        }

        bool BeginSession(
            const SessionOptions& sessionOptions)
        {
            if (IsInSession())
            {
                NV_PERF_LOG_ERR(10, "already in a session\n");
                return false;
            }
            if (!OpenGLIsNvidiaDevice() || !OpenGLIsGpuSupported())
            {
                // TODO: error - device is not supported for profiling
                return false;
            }

            NVPA_Status nvpaStatus;

            NVPW_OpenGL_Profiler_CalcTraceBufferSize_Params calcTraceBufferSizeParam = { NVPW_OpenGL_Profiler_CalcTraceBufferSize_Params_STRUCT_SIZE };
            calcTraceBufferSizeParam.maxRangesPerPass = sessionOptions.maxNumRanges;
            calcTraceBufferSizeParam.avgRangeNameLength = sessionOptions.avgRangeNameLength;
            nvpaStatus = NVPW_OpenGL_Profiler_CalcTraceBufferSize(&calcTraceBufferSizeParam);
            if (nvpaStatus)
            {
                return false;
            }

            NVPW_OpenGL_Profiler_GraphicsContext_BeginSession_Params beginSessionParams = { NVPW_OpenGL_Profiler_GraphicsContext_BeginSession_Params_STRUCT_SIZE };
            beginSessionParams.numTraceBuffers = sessionOptions.numTraceBuffers;
            beginSessionParams.traceBufferSize = calcTraceBufferSizeParam.traceBufferSize;
            beginSessionParams.maxRangesPerPass = sessionOptions.maxNumRanges;
            beginSessionParams.maxLaunchesPerPass = sessionOptions.maxNumRanges;
            nvpaStatus = NVPW_OpenGL_Profiler_GraphicsContext_BeginSession(&beginSessionParams);
            if (nvpaStatus)
            {
                if (nvpaStatus == NVPA_STATUS_INSUFFICIENT_PRIVILEGE)
                {
                    NV_PERF_LOG_ERR(10, "Failed to start profiler session: profiling permissions not enabled.  Please follow these instructions: https://developer.nvidia.com/nvidia-development-tools-solutions-ERR_NVGPUCTRPERM-permission-issue-performance-counters \n");
                }
                else if (nvpaStatus == NVPA_STATUS_INSUFFICIENT_DRIVER_VERSION)
                {
                    NV_PERF_LOG_ERR(10, "Failed to start profiler session: insufficient driver version.  Please install the latest NVIDIA driver from https://www.nvidia.com \n");
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

            if(!m_profilerApi.Initialize(sessionOptions))
            {
                return false;
            }

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
            if (!IsInPass())
            {
                return true;
            }

            const bool status = m_stateMachine.PushRange(pRangeName);
            return status;
        }

        bool PopRange()
        {
            if (!IsInPass())
            {
                return true;
            }

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
