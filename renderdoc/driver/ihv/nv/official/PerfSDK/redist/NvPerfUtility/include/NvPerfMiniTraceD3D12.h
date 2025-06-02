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

#include "NvPerfD3D12.h"
#include "NvPerfInit.h"
#include "NvPerfMiniTrace.h"

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12GraphicsCommandList;

namespace nv { namespace perf { namespace mini_trace {

    template <class TPredicateFunc>
    NVPA_Bool PredicateFuncInvoker(void* self, ID3D12CommandQueue* pCommandQueue)
    {
        auto& predicateFunc = *(TPredicateFunc*)self;
        const NVPA_Bool predicate = predicateFunc(pCommandQueue);
        return predicate;
    }

    // The UserData is mutable and persistent across ECLs.
    template <class TMarkerFunc>
    void MarkerFuncInvoker(void* self, ID3D12CommandQueue* pCommandQueue, uint8_t* pUserData, size_t userDataSize)
    {
        auto& markerFunc = *(TMarkerFunc*)self;
        markerFunc(pCommandQueue, pUserData, userDataSize);
    }

    // Return 0 to skip the timestamp
    template <class TAddressFunc>
    uint64_t AddressFuncInvoker(void* self, ID3D12CommandQueue* pCommandQueue)
    {
        auto& addressFunc = *(TAddressFunc*)self;
        uint64_t gpuva = addressFunc(pCommandQueue);
        return gpuva;
    }

    class MiniTraceD3D12
    {
    private:
        ID3D12Device* m_pD3D12Device;
        NVPW_D3D12_MiniTrace_DeviceState* m_pDeviceState;
        bool m_isInitialized;
        // Doesn't own any resources(e.g. ID3D12Resource)

    public:
        MiniTraceD3D12()
            : m_pD3D12Device()
            , m_pDeviceState()
            , m_isInitialized()
        {
        }
        MiniTraceD3D12(const MiniTraceD3D12& trace) = delete;
        MiniTraceD3D12(MiniTraceD3D12&& trace)
            : m_pD3D12Device(trace.m_pD3D12Device)
            , m_pDeviceState(trace.m_pDeviceState)
            , m_isInitialized(trace.m_isInitialized)
        {
            trace.m_pDeviceState = nullptr;
            trace.m_isInitialized = false;
        }
        ~MiniTraceD3D12()
        {
            Reset();
        }
        MiniTraceD3D12& operator=(const MiniTraceD3D12& trace) = delete;
        MiniTraceD3D12& operator=(MiniTraceD3D12&& trace)
        {
            Reset();
            m_pD3D12Device = trace.m_pD3D12Device;
            m_pDeviceState = trace.m_pDeviceState;
            m_isInitialized = trace.m_isInitialized;
            trace.m_pDeviceState = nullptr;
            trace.m_isInitialized = false;
            return *this;
        }

        bool Initialize(ID3D12Device* pDevice)
        {
            if (!InitializeNvPerf())
            {
                NV_PERF_LOG_ERR(10, "InitializeNvPerf failed\n");
                return false;
            }
            if (!D3D12LoadDriver())
            {
                NV_PERF_LOG_ERR(10, "Could not load driver\n");
                return false;
            }
            if (!D3D12IsGpuSupported(pDevice))
            {
                return false;
            }

            NVPW_D3D12_MiniTrace_DeviceState_Create_Params createParams = { NVPW_D3D12_MiniTrace_DeviceState_Create_Params_STRUCT_SIZE };
            createParams.pDevice = pDevice;
            NVPA_Status nvpaStatus = NVPW_D3D12_MiniTrace_DeviceState_Create(&createParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(20, "NVPW_D3D12_MiniTrace_DeviceState_Create failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            m_pD3D12Device = pDevice;
            m_pDeviceState = createParams.pDeviceState;
            m_isInitialized = true;
            return true;
        }

        void Reset()
        {
            if (m_isInitialized)
            {
                NVPW_D3D12_MiniTrace_DeviceState_Destroy_Params destroyParams = { NVPW_D3D12_MiniTrace_DeviceState_Destroy_Params_STRUCT_SIZE };
                destroyParams.pDeviceState = m_pDeviceState;
                NVPA_Status nvpaStatus = NVPW_D3D12_MiniTrace_DeviceState_Destroy(&destroyParams);
                if (nvpaStatus != NVPA_STATUS_SUCCESS)
                {
                    NV_PERF_LOG_ERR(50, "NVPW_D3D12_MiniTrace_DeviceState_Destroy failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                }
                m_pD3D12Device = nullptr;
                m_pDeviceState = nullptr;
                m_isInitialized = false;
            }
        }

        bool IsInitialized() const
        {
            return m_isInitialized;
        }

        bool RegisterQueue(ID3D12CommandQueue* pCommandQueue)
        {
            NVPW_D3D12_MiniTrace_Queue_Register_Params queueRegisterParams = { NVPW_D3D12_MiniTrace_Queue_Register_Params_STRUCT_SIZE };
            queueRegisterParams.pDeviceState = m_pDeviceState;
            queueRegisterParams.pCommandQueue = pCommandQueue;
            NVPA_Status nvpaStatus = NVPW_D3D12_MiniTrace_Queue_Register(&queueRegisterParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(30, "NVPW_D3D12_MiniTrace_Queue_Register failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }

        bool UnregisterQueue(ID3D12CommandQueue* pCommandQueue)
        {
            NVPW_D3D12_MiniTrace_Queue_Unregister_Params queueUnregisterParams = { NVPW_D3D12_MiniTrace_Queue_Unregister_Params_STRUCT_SIZE };
            queueUnregisterParams.pCommandQueue = pCommandQueue;
            NVPA_Status nvpaStatus = NVPW_D3D12_MiniTrace_Queue_Unregister(&queueUnregisterParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(30, "NVPW_D3D12_MiniTrace_Queue_Unregister failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }

        // `TPredicateFunc` is expected to be in the form of bool(ID3D12CommandQueue*)
        template <class TPredicateFunc>
        bool FrontEndTrigger(ID3D12GraphicsCommandList* pCommandList, bool useComputeMethods, TPredicateFunc&& predicateFunc)
        {
            static_assert(std::is_trivially_destructible<TPredicateFunc>::value, "The PredicateFunc copied into the CommandList must have a trivial destructor.");
            static_assert(std::is_trivially_copyable<TPredicateFunc>::value, "The PredicateFunc copied into the CommandList must be trivially copyable.");

            NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger_Params frontEndTriggerParams = { NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger_Params_STRUCT_SIZE };
            frontEndTriggerParams.pDeviceState = m_pDeviceState;
            frontEndTriggerParams.pCommandList = pCommandList;
            frontEndTriggerParams.useComputeMethods = useComputeMethods;
            frontEndTriggerParams.predicateFuncInvoker = PredicateFuncInvoker<TPredicateFunc>;
            frontEndTriggerParams.pPredicateFunc = &predicateFunc;
            frontEndTriggerParams.predicateFuncSize = sizeof(predicateFunc);
            NVPA_Status nvpaStatus = NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger(&frontEndTriggerParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(30, "NVPW_D3D12_MiniTrace_CommandList_FrontEndTrigger failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }

        // `TMarkerFunc` is expected to be in the form of void(ID3D12CommandQueue* pCommandQueue, uint8_t* pUserData, size_t userDataSize)
        template <class TMarkerFunc>
        bool MarkerCpu(ID3D12GraphicsCommandList* pCommandList, const uint8_t* pUserData, size_t userDataSize, TMarkerFunc&& markerFunc)
        {
            static_assert(std::is_trivially_destructible<TMarkerFunc>::value, "The MarkerFunc copied into the CommandList must have a trivial destructor.");
            static_assert(std::is_trivially_copyable<TMarkerFunc>::value, "The MarkerFunc copied into the CommandList must be trivially copyable.");

            NVPW_D3D12_MiniTrace_CommandList_MarkerCpu_Params markerCpuParams = { NVPW_D3D12_MiniTrace_CommandList_MarkerCpu_Params_STRUCT_SIZE };
            markerCpuParams.pDeviceState = m_pDeviceState;
            markerCpuParams.pCommandList = pCommandList;
            markerCpuParams.markerFuncInvoker = MarkerFuncInvoker<TMarkerFunc>;
            markerCpuParams.pMarkerFunc = &markerFunc;
            markerCpuParams.markerFuncSize = sizeof(markerFunc);
            markerCpuParams.pUserData = pUserData;
            markerCpuParams.userDataSize = userDataSize;
            NVPA_Status nvpaStatus = NVPW_D3D12_MiniTrace_CommandList_MarkerCpu(&markerCpuParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(30, "NVPW_D3D12_MiniTrace_CommandList_MarkerCpu failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }

        // `TAddressFunc` is expected to be in the form of uint64_t(ID3D12CommandQueue* pCommandQueue)
        template <class TAddressFunc>
        bool HostTimestamp(ID3D12GraphicsCommandList* pCommandList, uint32_t payload, TAddressFunc&& addressFunc)
        {
            static_assert(std::is_trivially_destructible<TAddressFunc>::value, "The AddressFunc copied into the CommandList must have a trivial destructor.");
            static_assert(std::is_trivially_copyable<TAddressFunc>::value, "The AddressFunc copied into the CommandList must be trivially copyable.");

            NVPW_D3D12_MiniTrace_CommandList_HostTimestamp_Params hostTimestampParams = { NVPW_D3D12_MiniTrace_CommandList_HostTimestamp_Params_STRUCT_SIZE };
            hostTimestampParams.pDeviceState = m_pDeviceState;
            hostTimestampParams.pCommandList = pCommandList;
            hostTimestampParams.payload = payload;
            hostTimestampParams.addressFuncInvoker = AddressFuncInvoker<TAddressFunc>;
            hostTimestampParams.pAddressFunc = &addressFunc;
            hostTimestampParams.addressFuncSize = sizeof(addressFunc);
            NVPA_Status nvpaStatus = NVPW_D3D12_MiniTrace_CommandList_HostTimestamp(&hostTimestampParams);
            if (nvpaStatus != NVPA_STATUS_SUCCESS)
            {
                NV_PERF_LOG_ERR(30, "NVPW_D3D12_MiniTrace_CommandList_HostTimestamp failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
                return false;
            }
            return true;
        }
    };

    class MiniTracerD3D12
    {
    private:
        struct FrameResource
        {
            CommandBuffer commandBuffer;
            uint64_t traceBufferGpuVA;
            uint32_t payload;

            FrameResource()
                : traceBufferGpuVA()
                , payload()
            {
            }
        };
        // Workaround for closure type not being trivially copyable on certain compilers
        class AddressFunc
        {
        private:
            uint64_t m_gpuVA;
        public:
            AddressFunc(uint64_t gpuVA)
                : m_gpuVA(gpuVA)
            {
            }
            uint64_t operator() (ID3D12CommandQueue*)
            {
                return m_gpuVA;
            }
        };
    public:
        struct FrameData
        {
            uint64_t frameEndTime;
        };
    private:
        // determined at Initialize()
        ID3D12Device* m_pDevice;
        MiniTraceD3D12 m_trace;
        bool m_isInitialized;
        // determined at BeginSession()
        ID3D12CommandQueue* m_pCommandQueue;
        std::vector<FrameResource> m_frameResources;
        size_t m_putFrameIdx;
        size_t m_getFrameIdx;
        size_t m_numUnreadFrames;
        size_t m_perFrameBufferSize;
        ComPtr<ID3D12Resource> m_pTraceBuffer;
        ComPtr<ID3D12Resource> m_pReadbackBuffer;
        bool m_inSession;
    private:
        static size_t CircularIncrement(size_t index, size_t max)
        {
            return (++index >= max) ? 0 : index;
        }
    public:
        MiniTracerD3D12()
            : m_pDevice()
            , m_isInitialized()
            , m_pCommandQueue()
            , m_putFrameIdx()
            , m_getFrameIdx()
            , m_numUnreadFrames()
            , m_perFrameBufferSize()
            , m_inSession()
        {
        }
        MiniTracerD3D12(const MiniTracerD3D12& tracer) = delete;
        MiniTracerD3D12(MiniTracerD3D12&& tracer)
            : m_pDevice(tracer.m_pDevice)
            , m_trace(std::move(tracer.m_trace))
            , m_isInitialized(tracer.m_isInitialized)
            , m_pCommandQueue(tracer.m_pCommandQueue)
            , m_frameResources(std::move(tracer.m_frameResources))
            , m_putFrameIdx(tracer.m_putFrameIdx)
            , m_getFrameIdx(tracer.m_getFrameIdx)
            , m_numUnreadFrames(tracer.m_numUnreadFrames)
            , m_perFrameBufferSize(tracer.m_perFrameBufferSize)
            , m_pTraceBuffer(std::move(tracer.m_pTraceBuffer))
            , m_pReadbackBuffer(std::move(tracer.m_pReadbackBuffer))
            , m_inSession(tracer.m_inSession)
        {
            tracer.m_isInitialized = false;
            tracer.m_inSession = false;
        }
        ~MiniTracerD3D12()
        {
            Reset();
        }
        MiniTracerD3D12& operator=(const MiniTracerD3D12& tracer) = delete;
        MiniTracerD3D12& operator=(MiniTracerD3D12&& tracer)
        {
            Reset();
            m_pDevice = tracer.m_pDevice;
            m_trace = std::move(tracer.m_trace);
            m_isInitialized = tracer.m_isInitialized;
            m_pCommandQueue = tracer.m_pCommandQueue;
            m_frameResources = std::move(tracer.m_frameResources);
            m_putFrameIdx = tracer.m_putFrameIdx;
            m_getFrameIdx = tracer.m_getFrameIdx;
            m_numUnreadFrames = tracer.m_numUnreadFrames;
            m_perFrameBufferSize = tracer.m_perFrameBufferSize;
            m_pTraceBuffer = std::move(tracer.m_pTraceBuffer);
            m_pReadbackBuffer = std::move(tracer.m_pReadbackBuffer);
            m_inSession = tracer.m_inSession;
            tracer.m_isInitialized = false;
            tracer.m_inSession = false;
            return *this;
        }

        bool Initialize(ID3D12Device* pDevice)
        {
            bool success = m_trace.Initialize(pDevice);
            if (!success)
            {
                return false;
            }
            m_pDevice = pDevice;
            m_isInitialized = true;
            return true;
        }

        // Undo BeginSession + Initialize
        void Reset()
        {
            if (m_isInitialized)
            {
                EndSession();
                m_pDevice = nullptr;
                m_trace.Reset();
                m_isInitialized = false;
            }
        }

        bool IsInitialized() const
        {
            return m_isInitialized;
        }

        bool InSession() const
        {
            return m_inSession;
        }

        MiniTraceD3D12& GetMiniTrace()
        {
            return m_trace;
        }

        bool BeginSession(ID3D12CommandQueue* pCommandQueue, size_t maxFrameLatency)
        {
            const D3D12_COMMAND_LIST_TYPE type = pCommandQueue->GetDesc().Type;
            if (type != D3D12_COMMAND_LIST_TYPE_DIRECT)
            {
                NV_PERF_LOG_ERR(50, "Unsupported command queue type: %u\n", type);
                return false;   
            }
            if (!m_isInitialized)
            {
                NV_PERF_LOG_ERR(50, "Not initialized\n");
                return false;
            }
            if (m_inSession)
            {
                NV_PERF_LOG_ERR(50, "Already in a session\n");
                return false;
            }

            bool success = m_trace.RegisterQueue(pCommandQueue);
            if (!success)
            {
                return false;
            }
            m_pCommandQueue = pCommandQueue;

            m_perFrameBufferSize = sizeof(NVPW_TimestampReport); // only supports a single timestamp per-frame today
            {
                const size_t bufferSize = m_perFrameBufferSize * (maxFrameLatency + 1);
                D3D12_HEAP_PROPERTIES heapProperties{};
                {
                    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
                    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
                    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
                    heapProperties.CreationNodeMask = 1;
                    heapProperties.VisibleNodeMask = 1;
                }

                D3D12_RESOURCE_DESC resourceDesc{};
                {
                    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
                    resourceDesc.Alignment = 0;
                    resourceDesc.Width = bufferSize;
                    resourceDesc.Height = 1;
                    resourceDesc.DepthOrArraySize = 1;
                    resourceDesc.MipLevels = 1;
                    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
                    resourceDesc.SampleDesc = DXGI_SAMPLE_DESC{1, 0};
                    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
                    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
                }

                HRESULT hr = m_pDevice->CreateCommittedResource(
                    &heapProperties,
                    D3D12_HEAP_FLAG_NONE,
                    &resourceDesc,
                    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                    nullptr,
                    IID_PPV_ARGS(&m_pTraceBuffer));
                if (!SUCCEEDED(hr))
                {
                    NV_PERF_LOG_ERR(50, "Allocating trace buffer failed\n");
                    return false;
                }
                
                heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
                resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
                hr = m_pDevice->CreateCommittedResource(
                    &heapProperties,
                    D3D12_HEAP_FLAG_NONE,
                    &resourceDesc,
                    D3D12_RESOURCE_STATE_COPY_DEST,
                    nullptr,
                    IID_PPV_ARGS(&m_pReadbackBuffer));
                if (!SUCCEEDED(hr))
                {
                    NV_PERF_LOG_ERR(50, "Allocating readback buffer failed\n");
                    return false;
                }
            }

            m_frameResources.resize(maxFrameLatency + 1);
            const uint64_t baseGpuVA = m_pTraceBuffer->GetGPUVirtualAddress();
            for (size_t frameIdx = 0; frameIdx < m_frameResources.size(); ++frameIdx)
            {
                FrameResource& frameResource = m_frameResources[frameIdx];
                success = frameResource.commandBuffer.Initialize(m_pDevice, D3D12_COMMAND_LIST_TYPE_DIRECT);
                if (!success)
                {
                    NV_PERF_LOG_ERR(50, "Initializing command buffer failed\n");
                    return false;
                }
                frameResource.traceBufferGpuVA = baseGpuVA + m_perFrameBufferSize * frameIdx;
                frameResource.payload = 10000;
            }
            m_inSession = true;
            return true;
        }

        void EndSession()
        {
            if (m_inSession)
            {
                m_trace.UnregisterQueue(m_pCommandQueue);
                m_pCommandQueue = nullptr;
                m_pTraceBuffer.Reset();
                m_pReadbackBuffer.Reset();
                m_frameResources.clear();
                m_putFrameIdx = 0;
                m_getFrameIdx = 0;
                m_numUnreadFrames = 0;
                m_perFrameBufferSize = 0;
                m_inSession = false;
            }
        }

        bool OnFrameEnd()
        {
            if (!m_inSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session\n");
                return false;
            }
            if (m_numUnreadFrames == m_frameResources.size())
            {
                NV_PERF_LOG_ERR(50, "Maximum number of unread frames is reached\n");
                return false;
            }

            bool success = true;
            FrameResource& frameResource = m_frameResources[m_putFrameIdx];
            CommandBuffer& commandBuffer = frameResource.commandBuffer;
            if (commandBuffer.fenceValue && !commandBuffer.IsCompleted())
            {
                NV_PERF_LOG_ERR(50, "Cannot recycle the frame that has not completed, \"maxFrameLatency\" specified is not suffient to cover the latency\n");
                return false;
            }
            if (commandBuffer.fenceValue)
            {
                success = commandBuffer.ResetAllocator();
                if (!success)
                {
                    NV_PERF_LOG_ERR(50, "ResetAllocator() failed\n");
                    return false;
                }
                success = commandBuffer.ResetList();
                if (!success)
                {
                    NV_PERF_LOG_ERR(50, "ResetList() failed\n");
                    return false;
                }
            }

            // capture by reference is okay here because ECL is in the same scope
            success = m_trace.HostTimestamp(commandBuffer.pCommandList.Get(), frameResource.payload++, AddressFunc(frameResource.traceBufferGpuVA));
            if (!success)
            {
                return false;
            }
            D3D12_RESOURCE_BARRIER resourceBarrier{};
            {
                resourceBarrier.Transition.pResource = m_pTraceBuffer.Get();
                resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
                resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
                resourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            }
            commandBuffer.pCommandList->ResourceBarrier(1, &resourceBarrier);
            // only copy back the region blongs to the current frame
            commandBuffer.pCommandList->CopyBufferRegion(
                m_pReadbackBuffer.Get(),
                m_perFrameBufferSize * m_putFrameIdx,
                m_pTraceBuffer.Get(),
                m_perFrameBufferSize * m_putFrameIdx,
                m_perFrameBufferSize);
            resourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
            resourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
            commandBuffer.pCommandList->ResourceBarrier(1, &resourceBarrier);
            success = commandBuffer.CloseList();
            if (!success)
            {
                NV_PERF_LOG_ERR(50, "CloseList() failed\n");
                return false;
            }
            commandBuffer.Execute(m_pCommandQueue);
            success = commandBuffer.SignalFence(m_pCommandQueue);
            if (!success)
            {
                NV_PERF_LOG_ERR(50, "SignalFence() failed\n");
                return false;
            }
            m_putFrameIdx = CircularIncrement(m_putFrameIdx, m_frameResources.size());
            ++m_numUnreadFrames;
            return true;
        }

        bool GetOldestFrameData(bool& isDataReady, FrameData& frameData)
        {
            isDataReady = false;
            if (!m_inSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session\n");
                return false;
            }

            if (!m_numUnreadFrames)
            {
                return true; // no data to consume, not an error
            }
            FrameResource& oldestFrameResource = m_frameResources[m_getFrameIdx];
            assert(oldestFrameResource.commandBuffer.fenceValue);
            if (!oldestFrameResource.commandBuffer.IsCompleted())
            {
                return true; // no data to consume - not finished executing on GPU, not an error
            }
            isDataReady = true;
            {
                // only read back the range belongs to the current frame
                D3D12_RANGE readRange;
                readRange.Begin = m_perFrameBufferSize * m_getFrameIdx;
                readRange.End = m_perFrameBufferSize * (m_getFrameIdx + 1);
                NVPW_TimestampReport* pReportsBuffer = nullptr;
                HRESULT hr = m_pReadbackBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pReportsBuffer));
                if (!SUCCEEDED(hr))
                {
                    NV_PERF_LOG_ERR(50, "ID3D12Resource::Map() failed\n");
                    return false;
                }
                NVPW_TimestampReport& report = pReportsBuffer[m_getFrameIdx]; // note "pReportsBuffer" is not offset by value in D3D12_RANGE
                frameData = FrameData{ report.timestamp };
                readRange.Begin = 0;
                readRange.End = 0;
                m_pReadbackBuffer->Unmap(0, &readRange);
            }
            return true;
        }

        bool ReleaseOldestFrame()
        {
            if (!m_inSession)
            {
                NV_PERF_LOG_ERR(50, "Not in a session\n");
                return false;
            }
            if (!m_numUnreadFrames)
            {
                return false;
            }
            m_getFrameIdx = CircularIncrement(m_getFrameIdx, m_frameResources.size());
            --m_numUnreadFrames;
            return true;
        }
    };

    class APITracerD3D12
    {
    private:
        struct TraceData
        {
            std::string name;
            // Initialize to -1 to indicate that the value is not set.
            size_t startSlotInQueryHeap = size_t(-1);
            size_t endSlotInQueryHeap = size_t(-1);
            size_t nestingLevel = 0;
        };
        // Owned
        ComPtr<ID3D12QueryHeap> m_queryHeap;
        ComPtr<ID3D12Resource> m_queryResultBuffer;
        // Not Owned
        ID3D12Device* m_pDevice;

        size_t m_nextSlot = 0;
        size_t m_maxSlots = 0;

        std::vector<TraceData> m_traceData;
        std::vector<uint64_t> m_timestamps;
    public:

        bool ResolveQueries(std::vector<APITraceData>& traceData)
        {
            if (m_traceData.size() == 0)
            {
                return true;
            }
            m_timestamps.resize(m_nextSlot);

            void* outMappedPtr = nullptr;
            D3D12_RANGE range = { 0, m_nextSlot * sizeof(uint64_t) };
            HRESULT hr = m_queryResultBuffer->Map(0, &range, &outMappedPtr);
            if (FAILED(hr))
            {
                NV_PERF_LOG_ERR(20, "ID3D12Resource::Map() failed\n");
                return false;
            }

            memcpy(m_timestamps.data(), outMappedPtr, m_nextSlot * sizeof(uint64_t));

            const D3D12_RANGE emptyRange = {};
            m_queryResultBuffer->Unmap(0, &emptyRange);
            traceData.reserve(traceData.size() + m_traceData.size());
            for (size_t index = 0; index < m_traceData.size(); ++index)
            {
                uint64_t startTimestamp = 0;
                uint64_t endTimestamp = 0;
                if (m_traceData[index].startSlotInQueryHeap < m_timestamps.size())
                {
                    startTimestamp = m_timestamps[m_traceData[index].startSlotInQueryHeap];
                }
                if (m_traceData[index].endSlotInQueryHeap < m_timestamps.size())
                {
                    endTimestamp = m_timestamps[m_traceData[index].endSlotInQueryHeap];
                }
                traceData.emplace_back(APITraceData{ m_traceData[index].name, startTimestamp, endTimestamp, m_traceData[index].nestingLevel });
            }
            return true;
        }
        void ClearData()
        {
            m_nextSlot = 0;
            m_traceData.clear();
        }
        bool BeginRange(ID3D12GraphicsCommandList* pCommandList, const char* name, size_t nestingLevel, size_t& index)
        {
            size_t slot = m_nextSlot;
            if (slot >= m_maxSlots)
            {
                NV_PERF_LOG_ERR(50, "No more slots\n");
                return false;
            }
            m_traceData.emplace_back(TraceData{name, slot, size_t(-1), nestingLevel});
            index = m_traceData.size() - 1;
            ++m_nextSlot;
            pCommandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, (UINT)slot);
            return true;
        }
        bool EndRange(ID3D12GraphicsCommandList* pCommandList, size_t index)
        {
            if (index >= m_traceData.size())
            {
                NV_PERF_LOG_ERR(50, "Invalid index\n");
                return false;
            }
            size_t slot = m_nextSlot;
            if (slot >= m_maxSlots)
            {
                NV_PERF_LOG_ERR(50, "No more slots\n");
                return false;
            }
            m_traceData[index].endSlotInQueryHeap = slot;
            ++m_nextSlot;
            pCommandList->EndQuery(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, (UINT)slot);
            return true;
        }

        void ResolveQueryDataOnCmdList(ID3D12GraphicsCommandList* pCommandList)
        {
            pCommandList->ResolveQueryData(m_queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, (UINT)m_nextSlot, m_queryResultBuffer.Get(), 0);
        }

        bool Initialize(ID3D12Device* pDevice, size_t numRanges)
        {
            size_t maxSlots = 2 * numRanges;
            D3D12_QUERY_HEAP_DESC desc = {};
            desc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
            desc.Count = (UINT)maxSlots;
            HRESULT hr = pDevice->CreateQueryHeap(&desc, IID_PPV_ARGS(&m_queryHeap));
            if(FAILED(hr))
            {
                NV_PERF_LOG_ERR(20, "CreateQueryHeap failed\n");
                return false;
            }
            D3D12_RESOURCE_DESC bufferDesc = {};
            bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            bufferDesc.Alignment = 0;
            bufferDesc.Width = maxSlots * sizeof(uint64_t);
            bufferDesc.Height = 1;
            bufferDesc.DepthOrArraySize = 1;
            bufferDesc.MipLevels = 1;
            bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
            bufferDesc.SampleDesc.Count = 1;
            bufferDesc.SampleDesc.Quality = 0;
            bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            D3D12_HEAP_PROPERTIES heapProperties = {};
            heapProperties.Type = D3D12_HEAP_TYPE_READBACK;
            heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heapProperties.CreationNodeMask = 1;
            heapProperties.VisibleNodeMask = 1;

            hr = pDevice->CreateCommittedResource( &heapProperties, D3D12_HEAP_FLAG_NONE,
                &bufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_queryResultBuffer)
            );
            if(FAILED(hr))
            {
                NV_PERF_LOG_ERR(20, "CreateCommittedResource failed\n");
                return false;
            }

            m_maxSlots = maxSlots;
            m_pDevice = pDevice;
            return true;
        }

        void Destroy()
        {
            m_pDevice = nullptr;
            m_queryHeap = nullptr;
            m_queryResultBuffer = nullptr;
        }

        ~APITracerD3D12()
        {
            Destroy();
        }
    };
}}} // nv::perf::minitrace
