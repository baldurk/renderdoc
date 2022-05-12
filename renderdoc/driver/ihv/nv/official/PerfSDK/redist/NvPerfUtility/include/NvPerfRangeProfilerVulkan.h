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
#include "NvPerfVulkan.h"

namespace nv { namespace perf { namespace profiler {

    class RangeProfilerVulkan
    {
    protected:
        struct ProfilerApi : RangeProfilerStateMachine::IProfilerApi
        {
            VkQueue queue;
            VkDevice device;
            VkCommandPool commandPool;
            size_t maxQueueRangesPerPass;
            std::vector<VkCommandBuffer> rangeCommandBuffers;
            std::vector<VkFence> rangeFences;
            size_t nextCommandBufferIdx;
            SessionOptions sessionOptions;

            ProfilerApi()
                : queue()
                , device()
                , commandPool()
                , maxQueueRangesPerPass(1)
                , nextCommandBufferIdx()
                , sessionOptions()
            {
            }

            virtual bool CreateCounterData(const SetConfigParams& config, std::vector<uint8_t>& counterDataImage, std::vector<uint8_t>& counterDataScratch) const override
            {
                NVPA_Status nvpaStatus;

                NVPW_VK_Profiler_CounterDataImageOptions counterDataImageOptions = { NVPW_VK_Profiler_CounterDataImageOptions_STRUCT_SIZE };
                counterDataImageOptions.pCounterDataPrefix = config.pCounterDataPrefix;
                counterDataImageOptions.counterDataPrefixSize = config.counterDataPrefixSize;
                counterDataImageOptions.maxNumRanges = static_cast<uint32_t>(sessionOptions.maxNumRanges);
                counterDataImageOptions.maxNumRangeTreeNodes = static_cast<uint32_t>(2 * sessionOptions.maxNumRanges);
                counterDataImageOptions.maxRangeNameLength = static_cast<uint32_t>(sessionOptions.avgRangeNameLength);

                NVPW_VK_Profiler_CounterDataImage_CalculateSize_Params calculateSizeParams = { NVPW_VK_Profiler_CounterDataImage_CalculateSize_Params_STRUCT_SIZE };
                calculateSizeParams.pOptions = &counterDataImageOptions;
                calculateSizeParams.counterDataImageOptionsSize = NVPW_VK_Profiler_CounterDataImageOptions_STRUCT_SIZE;
                nvpaStatus = NVPW_VK_Profiler_CounterDataImage_CalculateSize(&calculateSizeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                counterDataImage.resize(calculateSizeParams.counterDataImageSize);

                NVPW_VK_Profiler_CounterDataImage_Initialize_Params initializeParams = { NVPW_VK_Profiler_CounterDataImage_Initialize_Params_STRUCT_SIZE };
                initializeParams.counterDataImageOptionsSize = NVPW_VK_Profiler_CounterDataImageOptions_STRUCT_SIZE;
                initializeParams.pOptions = &counterDataImageOptions;
                initializeParams.counterDataImageSize = calculateSizeParams.counterDataImageSize;
                initializeParams.pCounterDataImage = &counterDataImage[0];
                nvpaStatus = NVPW_VK_Profiler_CounterDataImage_Initialize(&initializeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize_Params scratchBufferSizeParams = { NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize_Params_STRUCT_SIZE };
                scratchBufferSizeParams.counterDataImageSize = calculateSizeParams.counterDataImageSize;
                scratchBufferSizeParams.pCounterDataImage = initializeParams.pCounterDataImage;
                nvpaStatus = NVPW_VK_Profiler_CounterDataImage_CalculateScratchBufferSize(&scratchBufferSizeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                counterDataScratch.resize(scratchBufferSizeParams.counterDataScratchBufferSize);

                NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer_Params initScratchBufferParams = { NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer_Params_STRUCT_SIZE };
                initScratchBufferParams.counterDataImageSize = calculateSizeParams.counterDataImageSize;
                initScratchBufferParams.pCounterDataImage = initializeParams.pCounterDataImage;
                initScratchBufferParams.counterDataScratchBufferSize = scratchBufferSizeParams.counterDataScratchBufferSize;
                initScratchBufferParams.pCounterDataScratchBuffer = &counterDataScratch[0];

                nvpaStatus = NVPW_VK_Profiler_CounterDataImage_InitializeScratchBuffer(&initScratchBufferParams);
                if (nvpaStatus)
                {
                    return false;
                }

                return true;
            }

            virtual bool SetConfig(const SetConfigParams& config) const override
            {
                NVPW_VK_Profiler_Queue_SetConfig_Params setConfigParams = { NVPW_VK_Profiler_Queue_SetConfig_Params_STRUCT_SIZE };
                setConfigParams.queue = queue;
                setConfigParams.pConfig = config.pConfigImage;
                setConfigParams.configSize = config.configImageSize;
                setConfigParams.minNestingLevel = 1;
                setConfigParams.numNestingLevels = config.numNestingLevels;
                setConfigParams.passIndex = 0;
                setConfigParams.targetNestingLevel = 1;
                NVPA_Status nvpaStatus = NVPW_VK_Profiler_Queue_SetConfig(&setConfigParams);
                if (nvpaStatus)
                {
                    return false;
                }

                return true;
            }

            virtual bool BeginPass() const override
            {
                NVPW_VK_Profiler_Queue_BeginPass_Params beginPassParams = { NVPW_VK_Profiler_Queue_BeginPass_Params_STRUCT_SIZE };
                beginPassParams.queue = queue;
                NVPA_Status nvpaStatus = NVPW_VK_Profiler_Queue_BeginPass(&beginPassParams);
                if (nvpaStatus)
                {
                    return false;
                }
                return true;
            }

            virtual bool EndPass() const override
            {
                NVPW_VK_Profiler_Queue_EndPass_Params endPassParams = { NVPW_VK_Profiler_Queue_EndPass_Params_STRUCT_SIZE };
                endPassParams.queue = queue;
                NVPA_Status nvpaStatus = NVPW_VK_Profiler_Queue_EndPass(&endPassParams);
                if (nvpaStatus)
                {
                    return false;
                }
                return true;
            }

            template <typename Functor>
            bool SubmitRangeCommandBufferFunctor(Functor&& functor)
            {
                VkFence fence = rangeFences[nextCommandBufferIdx];
                VkResult vkResult = vkWaitForFences(device, 1, &fence, false, 0);
                if (vkResult == VK_TIMEOUT)
                {
                    NV_PERF_LOG_ERR(10, "No more command buffer available for queue level ranges, consider increasing sessionOptions.maxNumRange\n");
                    return false;
                }

                if (vkResult)
                {
                    NV_PERF_LOG_ERR(10, "vkWaitForFences failed, VkResult = %d\n", vkResult);
                    return false;
                }

                VkCommandBuffer commandBuffer = rangeCommandBuffers[nextCommandBufferIdx];
                ++nextCommandBufferIdx;
                if (nextCommandBufferIdx >= rangeCommandBuffers.size())
                {
                    nextCommandBufferIdx = 0;
                }

                vkResult = vkResetCommandBuffer(commandBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
                if (vkResult)
                {
                    NV_PERF_LOG_ERR(10, "vkResetCommandBuffer failed, VkResult = %d\n", vkResult);
                    return false;
                }

                VkCommandBufferBeginInfo commandBufferBeginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
                vkResult = vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);
                if (vkResult)
                {
                    NV_PERF_LOG_ERR(10, "vkBeginCommandBuffer failed, VkResult = %d\n", vkResult);
                    return false;
                }
                if (!functor(commandBuffer))
                {
                    return false;
                }

                vkResult = vkEndCommandBuffer(commandBuffer);
                if (vkResult)
                {
                    NV_PERF_LOG_ERR(10, "vkEndCommandBuffer failed, VkResult = %d\n", vkResult);
                    return false;
                }

                vkResult = vkResetFences(device, 1, &fence);
                if (vkResult)
                {
                    NV_PERF_LOG_ERR(10, "vkResetFences failed, VkResult = %d\n", vkResult);
                    return false;
                }

                VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commandBuffer;
                vkResult = vkQueueSubmit(queue, 1, &submitInfo, fence);
                if (vkResult)
                {
                    NV_PERF_LOG_ERR(10, "vkQueueSubmit failed, VkResult = %d\n", vkResult);
                    return false;
                }
                return true;
            }

            virtual bool PushRange(const char* pRangeName) override
            {
                return SubmitRangeCommandBufferFunctor([&](VkCommandBuffer commandBuffer)
                {
                    NVPW_VK_Profiler_CommandBuffer_PushRange_Params pushRangeParams = {NVPW_VK_Profiler_CommandBuffer_PushRange_Params_STRUCT_SIZE};
                    pushRangeParams.commandBuffer = commandBuffer;
                    pushRangeParams.pRangeName = pRangeName;
                    NVPA_Status nvpaStatus = NVPW_VK_Profiler_CommandBuffer_PushRange(&pushRangeParams);
                    if (nvpaStatus)
                    {
                        NV_PERF_LOG_ERR(10, "NVPW_VK_Profiler_CommandBuffer_PushRange failed, nvpaStatus = %d\n", nvpaStatus);
                        return false;
                    }
                    return true;
                });
            }

            virtual bool PopRange() override
            {
                return SubmitRangeCommandBufferFunctor([&](VkCommandBuffer commandBuffer)
                {
                    NVPW_VK_Profiler_CommandBuffer_PopRange_Params popRangeParams = {NVPW_VK_Profiler_CommandBuffer_PopRange_Params_STRUCT_SIZE};
                    popRangeParams.commandBuffer = commandBuffer;
                    NVPA_Status nvpaStatus = NVPW_VK_Profiler_CommandBuffer_PopRange(&popRangeParams);
                    if (nvpaStatus)
                    {
                        NV_PERF_LOG_ERR(10, "NVPW_VK_Profiler_CommandBuffer_PopRange failed, nvpaStatus = %d\n", nvpaStatus);
                        return false;
                    }
                    return true;
                });
            }
            virtual bool DecodeCounters(std::vector<uint8_t>& counterDataImage, std::vector<uint8_t>& counterDataScratch, bool& onePassDecoded, bool& allPassesDecoded) const
            {
                NVPW_VK_Profiler_Queue_DecodeCounters_Params decodeParams = { NVPW_VK_Profiler_Queue_DecodeCounters_Params_STRUCT_SIZE };
                decodeParams.queue = queue;
                decodeParams.counterDataImageSize = counterDataImage.size();
                decodeParams.pCounterDataImage = counterDataImage.data();
                decodeParams.counterDataScratchBufferSize = counterDataScratch.size();
                decodeParams.pCounterDataScratchBuffer = counterDataScratch.data();
                NVPA_Status nvpaStatus = NVPW_VK_Profiler_Queue_DecodeCounters(&decodeParams);
                if (nvpaStatus)
                {
                    return false;
                }

                onePassDecoded = decodeParams.onePassCollected;
                allPassesDecoded = decodeParams.allPassesCollected;
                return true;
            }

            bool Initialize(VkDevice device_, VkQueue queue_, uint32_t queueFamilyIndex, const SessionOptions& sessionOptions_)
            {
                device = device_;
                queue = queue_;
                sessionOptions = sessionOptions_;

                VkCommandPoolCreateInfo commandPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
                commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex;
                commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
                VkResult vkResult = vkCreateCommandPool(device, &commandPoolCreateInfo, nullptr, &commandPool);
                if (vkResult)
                {
                    return false;
                }

                const size_t maxRangeCommandBuffers = maxQueueRangesPerPass * 2 * sessionOptions.numTraceBuffers;
                rangeCommandBuffers.resize(maxRangeCommandBuffers);
                VkCommandBufferAllocateInfo commandBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
                commandBufferAllocateInfo.commandPool = commandPool;
                commandBufferAllocateInfo.commandBufferCount = (uint32_t)maxRangeCommandBuffers;
                vkResult = vkAllocateCommandBuffers(device, &commandBufferAllocateInfo, rangeCommandBuffers.data());
                if (vkResult)
                {
                    return false;
                }

                rangeFences.resize(maxRangeCommandBuffers);
                VkFenceCreateInfo fenceCreateInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
                fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
                for (auto& rangeFence : rangeFences)
                {
                    vkResult = vkCreateFence(device, &fenceCreateInfo, nullptr, &rangeFence);
                    if (vkResult)
                    {
                        return false;
                    }
                }

                return true;
            }

            void Reset()
            {
                NVPW_VK_Profiler_Queue_EndSession_Params endSessionParams = {NVPW_VK_Profiler_Queue_EndSession_Params_STRUCT_SIZE};
                endSessionParams.queue = queue;
                endSessionParams.timeout = 0xFFFFFFFF;
                NVPA_Status nvpaStatus = NVPW_VK_Profiler_Queue_EndSession(&endSessionParams);
                if (nvpaStatus)
                {
                    NV_PERF_LOG_ERR(10, "NVPW_VK_Profiler_Queue_EndSession failed, nvpaStatus = %d\n", nvpaStatus);
                }

                sessionOptions = {};
                nextCommandBufferIdx = 0;

                vkFreeCommandBuffers(device, commandPool, (uint32_t)rangeCommandBuffers.size(), rangeCommandBuffers.data());
                rangeCommandBuffers.clear();

                vkDestroyCommandPool(device, commandPool, nullptr);
                commandPool = VK_NULL_HANDLE;

                for (auto fence : rangeFences)
                {
                    vkDestroyFence(device, fence, nullptr);
                }
                queue = VK_NULL_HANDLE;
                device = VK_NULL_HANDLE;
            }
        };

    protected: // members
        ProfilerApi m_profilerApi;
        RangeProfilerStateMachine m_stateMachine;
        std::thread m_spgoThread;
        volatile bool m_spgoThreadExited;

    private:
        // non-copyable
        RangeProfilerVulkan(const RangeProfilerVulkan&);

        static void SpgoThreadProc(RangeProfilerVulkan* pRangeProfiler, VkQueue queue)
        {
            // Run continuously in the background, handling all BeginPass and EndPass GPU operations until EndSession().
            NVPW_VK_Queue_ServicePendingGpuOperations_Params serviceGpuOpsParams = { NVPW_VK_Queue_ServicePendingGpuOperations_Params_STRUCT_SIZE };
            serviceGpuOpsParams.queue = queue;
            serviceGpuOpsParams.numOperations = 0; // run until EndSession()
            serviceGpuOpsParams.timeout = 0xFFFFFFFF;
            NVPA_Status nvpaStatus = NVPW_VK_Queue_ServicePendingGpuOperations(&serviceGpuOpsParams);
            if (nvpaStatus)
            {
                // TODO: log an error
            }

            pRangeProfiler->m_spgoThreadExited = true;
        }

    public:
        ~RangeProfilerVulkan()
        {
        }

        RangeProfilerVulkan()
            : m_profilerApi()
            , m_stateMachine(m_profilerApi)
            , m_spgoThread()
            , m_spgoThreadExited()
        {
        }
        // TODO: make this move friendly

        bool IsInSession() const
        {
            return !!m_profilerApi.queue;
        }

        bool IsInPass() const
        {
            return m_stateMachine.IsInPass();
        }

        VkQueue GetVkQueue() const
        {
            return m_profilerApi.queue;
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
            VkInstance instance,
            VkPhysicalDevice physicalDevice,
            VkDevice device,
            VkQueue queue,
            uint32_t queueFamilyIndex,
            const SessionOptions& sessionOptions)
        {
            if (IsInSession())
            {
                NV_PERF_LOG_ERR(10, "already in a session\n");
                return false;
            }
            if (!VulkanIsNvidiaDevice(physicalDevice) || !VulkanIsGpuSupported(instance, physicalDevice, device))
            {
                // TODO: error - device is not supported for profiling
                return false;
            }

            NVPA_Status nvpaStatus;

            NVPW_VK_Profiler_CalcTraceBufferSize_Params calcTraceBufferSizeParam = { NVPW_VK_Profiler_CalcTraceBufferSize_Params_STRUCT_SIZE };
            calcTraceBufferSizeParam.maxRangesPerPass = sessionOptions.maxNumRanges;
            calcTraceBufferSizeParam.avgRangeNameLength = sessionOptions.avgRangeNameLength;
            nvpaStatus = NVPW_VK_Profiler_CalcTraceBufferSize(&calcTraceBufferSizeParam);
            if (nvpaStatus)
            {
                return false;
            }

            NVPW_VK_Profiler_Queue_BeginSession_Params beginSessionParams = { NVPW_VK_Profiler_Queue_BeginSession_Params_STRUCT_SIZE };
            beginSessionParams.instance = instance;
            beginSessionParams.physicalDevice = physicalDevice;
            beginSessionParams.device = device;
            beginSessionParams.queue = queue;
            beginSessionParams.pfnGetInstanceProcAddr = (void*)vkGetInstanceProcAddr;
            beginSessionParams.pfnGetDeviceProcAddr = (void*)vkGetDeviceProcAddr;
            beginSessionParams.numTraceBuffers = sessionOptions.numTraceBuffers;
            beginSessionParams.traceBufferSize = calcTraceBufferSizeParam.traceBufferSize;
            beginSessionParams.maxRangesPerPass = sessionOptions.maxNumRanges;
            beginSessionParams.maxLaunchesPerPass = sessionOptions.maxNumRanges;
            nvpaStatus = NVPW_VK_Profiler_Queue_BeginSession(&beginSessionParams);
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
            m_spgoThread = std::thread(SpgoThreadProc, this, queue);
            if(!m_profilerApi.Initialize(device, queue, queueFamilyIndex, sessionOptions))
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

        // Convenience method to start a Queue-level range.  For CommandLists, use VulkanRangeCommands::PushRange.
        bool PushRange(const char* pRangeName)
        {
            const bool status = m_stateMachine.PushRange(pRangeName);
            return status;
        }

        // Convenience method to end a Queue-level range.  For CommandLists, use VulkanRangeCommands::PopRange.
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
