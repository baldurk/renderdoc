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

#include "NvPerfInit.h"
#include "NvPerfDeviceProperties.h"
#include "NvPerfPeriodicSamplerGpu.h"
#include "NvPerfD3D.h"
#include "nvperf_d3d12_host.h"
#include "nvperf_d3d12_target.h"
#include <D3D12.h>
#include <wrl/client.h>

namespace nv { namespace perf {

    //
    // D3D Only Utilities
    //
    using Microsoft::WRL::ComPtr;

    struct CommandBuffer
    {
        ComPtr<ID3D12CommandAllocator> pCommandAllocator;
        ComPtr<ID3D12GraphicsCommandList> pCommandList;
        ComPtr<ID3D12Fence> pFence;
        uint64_t fenceValue;

        CommandBuffer()
            : fenceValue(0)
        {
        }
        CommandBuffer(const CommandBuffer& counterData) = delete;
        CommandBuffer& operator=(const CommandBuffer& counterData) = delete;
        CommandBuffer(CommandBuffer&& counterData) = default;
        CommandBuffer& operator=(CommandBuffer&& counterData) = default;

        bool Initialize(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type)
        {
            HRESULT hr = pDevice->CreateCommandAllocator(type, IID_PPV_ARGS(&pCommandAllocator));
            if (FAILED(hr))
            {
                return false;
            }
            hr = pDevice->CreateCommandList(0, type, pCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&pCommandList));
            if (FAILED(hr))
            {
                return false;
            }

            hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pFence));
            if (FAILED(hr))
            {
                return false;
            }
            return true;
        }

        bool CloseList()
        {
            HRESULT hr = pCommandList->Close();
            if (FAILED(hr))
            {
                return false;
            }
            return true;
        }

        bool ResetList()
        {
            HRESULT hr = pCommandList->Reset(pCommandAllocator.Get(), nullptr);
            if (FAILED(hr))
            {
                return false;
            }
            return true;
        }

        bool ResetAllocator()
        {
            HRESULT hr = pCommandAllocator->Reset();
            if (FAILED(hr))
            {
                return false;
            }
            return true;
        }

        bool WaitForCompletion(uint32_t milliseconds)
        {
            if (!IsCompleted())
            {
                const HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
                if (event == nullptr)
                {
                    return false;
                }

                const HRESULT hr = pFence->SetEventOnCompletion(fenceValue, event);
                if (FAILED(hr))
                {
                    return false;
                }

                const DWORD result = WaitForSingleObject(event, milliseconds);
                const BOOL closeSucceeded = CloseHandle(event);
                if (result != WAIT_OBJECT_0)
                {
                    return false;
                }
                if (!closeSucceeded)
                {
                    return false;
                }
            }
            return true;
        }

        bool WaitAndResetAllocator()
        {
            HRESULT hr = WaitForCompletion(INFINITE);
            if (FAILED(hr))
            {
                return false;
            }

            hr = ResetAllocator();
            if (FAILED(hr))
            {
                return false;
            }
            return true;
        }

        bool IsCompleted()
        {
            return pFence->GetCompletedValue() >= fenceValue;
        }

        void Execute(ID3D12CommandQueue* pQueue)
        {
            ID3D12CommandList* const ppCommandLists[] = {pCommandList.Get()};
            pQueue->ExecuteCommandLists(1, ppCommandLists);
        }

        bool SignalFence(ID3D12CommandQueue* pQueue)
        {
            const HRESULT hr = pQueue->Signal(pFence.Get(), ++fenceValue);
            if (FAILED(hr))
            {
                return false;
            }
            return true;
        }

        ~CommandBuffer()
        {
            assert(IsCompleted());
        }
    };

    inline bool D3D12FindAdapterForDevice(ID3D12Device* pDevice, IDXGIAdapter1** ppDXGIAdapter, DXGI_ADAPTER_DESC1* pAdapterDesc = nullptr)
    {
        const LUID deviceLuid = pDevice->GetAdapterLuid();

        ComPtr<IDXGIFactory1> pDXGIFactory;
        HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&pDXGIFactory));
        if (FAILED(hr))
        {
            return false;
        }

        for (UINT adapterIndex = 0; ; ++adapterIndex)
        {
            ComPtr<IDXGIAdapter1> pDXGIAdapter;
            hr = pDXGIFactory->EnumAdapters1(adapterIndex, &pDXGIAdapter);
            if (FAILED(hr))
            {
                break; // the intended loop termination
            }

            DXGI_ADAPTER_DESC1 adapterDesc = {};
            hr = pDXGIAdapter->GetDesc1(&adapterDesc);
            if (FAILED(hr))
            {
                continue;
            }

            if (!memcmp(&adapterDesc.AdapterLuid, &deviceLuid, sizeof(deviceLuid)))
            {
                *ppDXGIAdapter = pDXGIAdapter.Detach();
                if (pAdapterDesc)
                {
                    *pAdapterDesc = adapterDesc;
                }
                return true;
            }
        }

        return false;
    }

    inline std::wstring D3D12GetDeviceName(ID3D12Device* pDevice)
    {
        DXGI_ADAPTER_DESC1 adapterDesc = {};
        ComPtr<IDXGIAdapter1> pDXGIAdapter;
        if (!D3D12FindAdapterForDevice(pDevice, &pDXGIAdapter, &adapterDesc))
        {
            return L"";
        }

        return adapterDesc.Description;
    }


    inline bool D3D12IsNvidiaDevice(ID3D12Device* pDevice)
    {
        ComPtr<IDXGIAdapter1> pDXGIAdapter;
        if (!D3D12FindAdapterForDevice(pDevice, &pDXGIAdapter))
        {
            return false;
        }

        const bool isNvidiaDevice = DxgiIsNvidiaDevice(pDXGIAdapter.Get());
        return isNvidiaDevice;
    }

    inline bool D3D12IsNvidiaDevice(ID3D12CommandQueue* pCommandQueue)
    {
        ComPtr<ID3D12Device> pDevice;
        HRESULT hr = pCommandQueue->GetDevice(IID_PPV_ARGS(&pDevice));
        if (FAILED(hr))
        {
            return false;
        }

        const bool isNvidiaDevice = D3D12IsNvidiaDevice(pDevice.Get());
        return isNvidiaDevice;
    }

    //
    // D3D12 NvPerf Utilities
    //

    inline bool D3D12LoadDriver()
    {
        NVPW_D3D12_LoadDriver_Params loadDriverParams = { NVPW_D3D12_LoadDriver_Params_STRUCT_SIZE };
        NVPA_Status nvpaStatus = NVPW_D3D12_LoadDriver(&loadDriverParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_D3D12_LoadDriver failed\n");
            return false;
        }
        return true;
    }


    inline size_t D3D12GetNvperfDeviceIndex(ID3D12Device* pDevice, size_t sliIndex = 0)
    {
        NVPW_D3D12_Device_GetDeviceIndex_Params getDeviceIndexParams = { NVPW_D3D12_Device_GetDeviceIndex_Params_STRUCT_SIZE };
        getDeviceIndexParams.pDevice = pDevice;
        getDeviceIndexParams.sliIndex = sliIndex;
        NVPA_Status nvpaStatus = NVPW_D3D12_Device_GetDeviceIndex(&getDeviceIndexParams);
        if (nvpaStatus)
        {
            return ~size_t(0);
        }

        return getDeviceIndexParams.deviceIndex;
    }

    inline DeviceIdentifiers D3D12GetDeviceIdentifiers(ID3D12Device* pDevice, size_t sliIndex = 0)
    {
        ComPtr<IDXGIAdapter1> pDXGIAdapter;
        if (!D3D12FindAdapterForDevice(pDevice, &pDXGIAdapter))
        {
            return {};
        }

        return D3DGetDeviceIdentifiers(pDXGIAdapter.Get(), sliIndex);
    }

    inline NVPW_Device_ClockStatus D3D12GetDeviceClockState(ID3D12Device* pDevice)
    {
        size_t nvperfDeviceIndex = D3D12GetNvperfDeviceIndex(pDevice);
        return GetDeviceClockState(nvperfDeviceIndex);
    }

    inline bool D3D12SetDeviceClockState(ID3D12Device* pDevice, NVPW_Device_ClockSetting clockSetting)
    {
        size_t nvperfDeviceIndex = D3D12GetNvperfDeviceIndex(pDevice);
        return SetDeviceClockState(nvperfDeviceIndex, clockSetting);
    }

    inline bool D3D12SetDeviceClockState(ID3D12Device* pDevice, NVPW_Device_ClockStatus clockStatus)
    {
        size_t nvperfDeviceIndex = D3D12GetNvperfDeviceIndex(pDevice);
        return SetDeviceClockState(nvperfDeviceIndex, clockStatus);
    }

    inline size_t D3D12CalculateMetricsEvaluatorScratchBufferSize(const char* pChipName)
    {
        NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Params calculateScratchBufferSizeParams = { NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize_Params_STRUCT_SIZE };
        calculateScratchBufferSizeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize(&calculateScratchBufferSizeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_D3D12_MetricsEvaluator_CalculateScratchBufferSize failed\n");
            return 0;
        }
        return calculateScratchBufferSizeParams.scratchBufferSize;
    }

    inline NVPW_MetricsEvaluator* D3D12CreateMetricsEvaluator(uint8_t* pScratchBuffer, size_t scratchBufferSize, const char* pChipName)
    {
        NVPW_D3D12_MetricsEvaluator_Initialize_Params initializeParams = { NVPW_D3D12_MetricsEvaluator_Initialize_Params_STRUCT_SIZE };
        initializeParams.pScratchBuffer = pScratchBuffer;
        initializeParams.scratchBufferSize = scratchBufferSize;
        initializeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_D3D12_MetricsEvaluator_Initialize(&initializeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_D3D12_MetricsEvaluator_Initialize failed\n");
            return nullptr;
        }
        return initializeParams.pMetricsEvaluator;
    }

}}

namespace nv { namespace perf { namespace profiler {

    inline NVPA_RawMetricsConfig* D3D12CreateRawMetricsConfig(const char* pChipName)
    {
        NVPW_D3D12_RawMetricsConfig_Create_Params configParams = { NVPW_D3D12_RawMetricsConfig_Create_Params_STRUCT_SIZE };
        configParams.activityKind = NVPA_ACTIVITY_KIND_PROFILER;
        configParams.pChipName = pChipName;

        NVPA_Status nvpaStatus = NVPW_D3D12_RawMetricsConfig_Create(&configParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_D3D12_RawMetricsConfig_Create failed\n");
            return nullptr;
        }

        return configParams.pRawMetricsConfig;
    }

    inline bool D3D12IsGpuSupported(ID3D12Device* pDevice, size_t sliIndex = 0)
    {
        const size_t deviceIndex = D3D12GetNvperfDeviceIndex(pDevice, sliIndex);
        if (deviceIndex == ~size_t(0))
        {
            NV_PERF_LOG_ERR(10, "D3D12GetNvperfDeviceIndex failed on %ls\n", D3D12GetDeviceName(pDevice).c_str());
            return false;
        }

        NVPW_D3D12_Profiler_IsGpuSupported_Params params = { NVPW_D3D12_Profiler_IsGpuSupported_Params_STRUCT_SIZE };
        params.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_D3D12_Profiler_IsGpuSupported(&params);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_D3D12_Profiler_IsGpuSupported failed on %ls\n", D3D12GetDeviceName(pDevice).c_str());
            return false;
        }

        if (!params.isSupported)
        {
            NV_PERF_LOG_ERR(10, "%ls is not supported for profiling\n", D3D12GetDeviceName(pDevice).c_str());
            if (params.gpuArchitectureSupportLevel != NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_SUPPORTED)
            {
                const DeviceIdentifiers deviceIdentifiers = D3D12GetDeviceIdentifiers(pDevice, sliIndex);
                NV_PERF_LOG_ERR(10, "Unsupported GPU architecture %s\n", deviceIdentifiers.pChipName);
            }
            if (params.sliSupportLevel == NVPW_SLI_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Devices in SLI configuration are not supported.\n");
            }
            if (params.cmpSupportLevel == NVPW_CMP_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Cryptomining GPUs (NVIDIA CMP) are not supported.\n");
            }
            return false;
        }

        return true;
    }

    inline bool D3D12IsGpuSupported(ID3D12CommandQueue* pCommandQueue, size_t sliIndex = 0)
    {
        ComPtr<ID3D12Device> pDevice;
        HRESULT hr = pCommandQueue->GetDevice(IID_PPV_ARGS(&pDevice));
        if (FAILED(hr))
        {
            return false;
        }

        const bool isGpuSupported = D3D12IsGpuSupported(pDevice.Get(), sliIndex);
        return isGpuSupported;
    }


    inline bool D3D12PushRange(ID3D12GraphicsCommandList* pCommandList, const char* pRangeName)
    {
        NVPW_D3D12_Profiler_CommandList_PushRange_Params pushRangeParams = { NVPW_D3D12_Profiler_CommandList_PushRange_Params_STRUCT_SIZE };
        pushRangeParams.pRangeName = pRangeName;
        pushRangeParams.rangeNameLength = 0;
        pushRangeParams.pCommandList = pCommandList;
        NVPA_Status nvpaStatus = NVPW_D3D12_Profiler_CommandList_PushRange(&pushRangeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(50, "NVPW_D3D12_Profiler_CommandList_PushRange failed\n");
            return false;
        }
        return true;
    }

    inline bool D3D12PopRange(ID3D12GraphicsCommandList* pCommandList)
    {
        NVPW_D3D12_Profiler_CommandList_PopRange_Params popParams = { NVPW_D3D12_Profiler_CommandList_PopRange_Params_STRUCT_SIZE };
        popParams.pCommandList = pCommandList;
        NVPA_Status nvpaStatus = NVPW_D3D12_Profiler_CommandList_PopRange(&popParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(50, "NVPW_D3D12_Profiler_CommandList_PopRange failed\n");
            return false;
        }
        return true;
    }

    inline bool D3D12PushRange_Nop(ID3D12GraphicsCommandList* pCommandList, const char* pRangeName)
    {
        return false;
    }

    inline bool D3D12PopRange_Nop(ID3D12GraphicsCommandList* pCommandList)
    {
        return false;
    }

    // 
    struct D3D12RangeCommands
    {
        bool isNvidiaDevice;
        bool(*PushRange)(ID3D12GraphicsCommandList* pCommandList, const char* pRangeName);
        bool(*PopRange)(ID3D12GraphicsCommandList* pCommandList);

    public:
        D3D12RangeCommands()
            : isNvidiaDevice(false)
            , PushRange(&D3D12PushRange_Nop)
            , PopRange(&D3D12PopRange_Nop)
        {
        }

        void Initialize(bool isNvidiaDevice_)
        {
            isNvidiaDevice = isNvidiaDevice_;
            if (isNvidiaDevice_)
            {
                PushRange = &D3D12PushRange;
                PopRange = &D3D12PopRange;
            }
            else
            {
                PushRange = &D3D12PushRange_Nop;
                PopRange = &D3D12PopRange_Nop;
            }
        }
    
        void Initialize(IDXGIAdapter* pDXGIAdapter)
        {
            const bool isNvidiaDevice_ = DxgiIsNvidiaDevice(pDXGIAdapter);
            return Initialize(isNvidiaDevice_);
        }

        void Initialize(ID3D12Device* pDevice)
        {
            const bool isNvidiaDevice_ = D3D12IsNvidiaDevice(pDevice);
            return Initialize(isNvidiaDevice_);
        }
    };

}}} // nv::perf::profiler

namespace nv { namespace perf { namespace mini_trace {

    inline bool D3D12IsGpuSupported(ID3D12Device* pDevice, size_t sliIndex = 0)
    {
        const size_t deviceIndex = D3D12GetNvperfDeviceIndex(pDevice, sliIndex);
        if (deviceIndex == ~size_t(0))
        {
            NV_PERF_LOG_ERR(10, "D3D12GetNvperfDeviceIndex failed on %ls\n", D3D12GetDeviceName(pDevice).c_str());
            return false;
        }

        NVPW_D3D12_MiniTrace_IsGpuSupported_Params params = { NVPW_D3D12_MiniTrace_IsGpuSupported_Params_STRUCT_SIZE };
        params.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_D3D12_MiniTrace_IsGpuSupported(&params);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_D3D12_MiniTrace_IsGpuSupported failed on %ls\n", D3D12GetDeviceName(pDevice).c_str());
            return false;
        }

        if (!params.isSupported)
        {
            NV_PERF_LOG_ERR(10, "%ls is not supported for profiling\n", D3D12GetDeviceName(pDevice).c_str());
            if (params.gpuArchitectureSupportLevel != NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_SUPPORTED)
            {
                const DeviceIdentifiers deviceIdentifiers = D3D12GetDeviceIdentifiers(pDevice, sliIndex);
                NV_PERF_LOG_ERR(10, "Unsupported GPU architecture %s\n", deviceIdentifiers.pChipName);
            }
            if (params.sliSupportLevel == NVPW_SLI_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Devices in SLI configuration are not supported.\n");
            }
            if (params.cmpSupportLevel == NVPW_CMP_SUPPORT_LEVEL_UNSUPPORTED)
            {
                NV_PERF_LOG_ERR(10, "Cryptomining GPUs (NVIDIA CMP) are not supported.\n");
            }
            return false;
        }

        return true;
    }

}}} // nv::perf::mini_trace

namespace nv { namespace perf { namespace sampler {

    inline bool D3D12IsGpuSupported(ID3D12Device* pDevice, size_t sliIndex = 0)
    {
        const size_t deviceIndex = D3D12GetNvperfDeviceIndex(pDevice, sliIndex);
        if (deviceIndex == ~size_t(0))
        {
            NV_PERF_LOG_ERR(10, "D3D12GetNvperfDeviceIndex failed on %ls\n", D3D12GetDeviceName(pDevice).c_str());
            return false;
        }
        if (!GpuPeriodicSamplerIsGpuSupported(deviceIndex))
        {
            return false;
        }
        if (!mini_trace::D3D12IsGpuSupported(pDevice, sliIndex))
        {
            return false;
        }
        return true;
    }

}}} // nv::perf::sampler
