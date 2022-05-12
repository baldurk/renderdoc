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

#include "NvPerfD3D.h"

#include "nvperf_d3d11_host.h"
#include "nvperf_d3d11_target.h"
#include <D3D11.h>
#include <wrl/client.h>

namespace nv { namespace perf {

    //
    // D3D11 Only Utilities
    //

    inline bool D3D11FindAdapterForDevice(ID3D11Device* pDevice, IDXGIAdapter** ppDXGIAdapter, DXGI_ADAPTER_DESC* pAdapterDesc = nullptr)
    {
        Microsoft::WRL::ComPtr<IDXGIDevice> pDXGIDevice;
        HRESULT hr = pDevice->QueryInterface(IID_PPV_ARGS(&pDXGIDevice));
        if (FAILED(hr))
        {
            return false;
        }

        hr = pDXGIDevice->GetAdapter(ppDXGIAdapter);
        if (FAILED(hr))
        {
            return false;
        }

        if (pAdapterDesc)
        {
            hr = (*ppDXGIAdapter)->GetDesc(pAdapterDesc);
            if (FAILED(hr))
            {
                return false;
            }
        }

        return true;
    }

    inline std::wstring D3D11GetDeviceName(ID3D11Device* pDevice)
    {
      DXGI_ADAPTER_DESC adapterDesc = {};
      Microsoft::WRL::ComPtr<IDXGIAdapter> pDXGIAdapter;
      if (!D3D11FindAdapterForDevice(pDevice, &pDXGIAdapter, &adapterDesc))
      {
          return L"";
      }

      return adapterDesc.Description;
    }

    inline bool D3D11IsNvidiaDevice(ID3D11Device* pDevice)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> pDXGIAdapter;
        if (!D3D11FindAdapterForDevice(pDevice, &pDXGIAdapter))
        {
            return false;
        }

        const bool isNvidiaDevice = DxgiIsNvidiaDevice(pDXGIAdapter.Get());

        return isNvidiaDevice;
    }

    inline bool D3D11IsNvidiaDevice(ID3D11DeviceContext* pDeviceContext)
    {
        Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
        pDeviceContext->GetDevice(&pDevice);
        if (!pDevice)
        {
            return false;
        }

        const bool isNvidiaDevice = D3D11IsNvidiaDevice(pDevice.Get());
        return isNvidiaDevice;
    }

    inline bool D3D11Finish(ID3D11Device* pDevice, ID3D11DeviceContext* pDeviceContext)
    {
        D3D11_QUERY_DESC queryDesc = { D3D11_QUERY_EVENT, 0 };
        Microsoft::WRL::ComPtr<ID3D11Query> pQuery;
        HRESULT hr = pDevice->CreateQuery(&queryDesc, &pQuery);
        if (FAILED(hr))
        {
            return false;
        }
        pDeviceContext->End(pQuery.Get());

        BOOL isDone = FALSE;
        do {
            hr = pDeviceContext->GetData(pQuery.Get(), &isDone, sizeof(isDone), 0);
            if (FAILED(hr))
            {
                return false;
            }
        } while (!isDone);

        return true;
    }

    //
    // D3D11 NvPerf Utilities
    //

    inline bool D3D11LoadDriver()
    {
        NVPW_D3D11_LoadDriver_Params loadDriverParams = { NVPW_D3D11_LoadDriver_Params_STRUCT_SIZE };
        NVPA_Status nvpaStatus = NVPW_D3D11_LoadDriver(&loadDriverParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_D3D11_LoadDriver failed\n");
            return false;
        }
        return true;
    }

    inline size_t D3D11GetNvperfDeviceIndex(ID3D11Device* pDevice, size_t sliIndex = 0)
    {
        NVPW_D3D11_Device_GetDeviceIndex_Params getDeviceIndexParams = { NVPW_D3D11_Device_GetDeviceIndex_Params_STRUCT_SIZE };
        getDeviceIndexParams.pDevice = pDevice;
        getDeviceIndexParams.sliIndex = sliIndex;
        NVPA_Status nvpaStatus = NVPW_D3D11_Device_GetDeviceIndex(&getDeviceIndexParams);
        if (nvpaStatus)
        {
            return ~size_t(0);
        }

        return getDeviceIndexParams.deviceIndex;
    }

    inline DeviceIdentifiers D3D11GetDeviceIdentifiers(ID3D11Device* pDevice, size_t sliIndex = 0)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter> pDXGIAdapter;
        if (!D3D11FindAdapterForDevice(pDevice, &pDXGIAdapter))
        {
            return {};
        }

        return D3DGetDeviceIdentifiers(pDXGIAdapter.Get(), sliIndex);
    }

    inline NVPW_Device_ClockStatus D3D11GetDeviceClockState(ID3D11Device* pDevice)
    {
        size_t nvperfDeviceIndex = D3D11GetNvperfDeviceIndex(pDevice);
        return GetDeviceClockState(nvperfDeviceIndex);
    }

    inline bool D3D11SetDeviceClockState(ID3D11Device* pDevice, NVPW_Device_ClockSetting clockSetting)
    {
        size_t nvperfDeviceIndex = D3D11GetNvperfDeviceIndex(pDevice);
        return SetDeviceClockState(nvperfDeviceIndex, clockSetting);
    }

    inline bool D3D11SetDeviceClockState(ID3D11Device* pDevice, NVPW_Device_ClockStatus clockStatus)
    {
        size_t nvperfDeviceIndex = D3D11GetNvperfDeviceIndex(pDevice);
        return SetDeviceClockState(nvperfDeviceIndex, clockStatus);
    }

    inline size_t D3D11CalculateMetricsEvaluatorScratchBufferSize(const char* pChipName)
    {
        NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize_Params calculateScratchBufferSizeParams = { NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize_Params_STRUCT_SIZE };
        calculateScratchBufferSizeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize(&calculateScratchBufferSizeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_D3D11_MetricsEvaluator_CalculateScratchBufferSize failed\n");
            return 0;
        }
        return calculateScratchBufferSizeParams.scratchBufferSize;
    }

    inline NVPW_MetricsEvaluator* D3D11CreateMetricsEvaluator(uint8_t* pScratchBuffer, size_t scratchBufferSize, const char* pChipName)
    {
        NVPW_D3D11_MetricsEvaluator_Initialize_Params initializeParams = { NVPW_D3D11_MetricsEvaluator_Initialize_Params_STRUCT_SIZE };
        initializeParams.pScratchBuffer = pScratchBuffer;
        initializeParams.scratchBufferSize = scratchBufferSize;
        initializeParams.pChipName = pChipName;
        NVPA_Status nvpaStatus = NVPW_D3D11_MetricsEvaluator_Initialize(&initializeParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_D3D11_MetricsEvaluator_Initialize failed\n");
            return nullptr;
        }
        return initializeParams.pMetricsEvaluator;
    }
}}

namespace nv { namespace perf { namespace profiler {

    inline NVPA_RawMetricsConfig* D3D11CreateRawMetricsConfig(const char* pChipName)
    {
        NVPW_D3D11_RawMetricsConfig_Create_Params configParams = { NVPW_D3D11_RawMetricsConfig_Create_Params_STRUCT_SIZE };
        configParams.activityKind = NVPA_ACTIVITY_KIND_PROFILER;
        configParams.pChipName = pChipName;

        NVPA_Status nvpaStatus = NVPW_D3D11_RawMetricsConfig_Create(&configParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(20, "NVPW_D3D11_RawMetricsConfig_Create failed\n");
            return nullptr;
        }

        return configParams.pRawMetricsConfig;
    }

    inline bool D3D11IsGpuSupported(ID3D11Device* pDevice, size_t sliIndex = 0)
    {
        const size_t deviceIndex = D3D11GetNvperfDeviceIndex(pDevice, sliIndex);
        if (deviceIndex == ~size_t(0))
        {
            NV_PERF_LOG_ERR(10, "D3D11GetNvperfDeviceIndex failed on %ls\n", D3D11GetDeviceName(pDevice).c_str());
            return false;
        }

        NVPW_D3D11_Profiler_IsGpuSupported_Params params = { NVPW_D3D11_Profiler_IsGpuSupported_Params_STRUCT_SIZE };
        params.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_D3D11_Profiler_IsGpuSupported(&params);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_D3D11_Profiler_IsGpuSupported failed on %ls\n", D3D11GetDeviceName(pDevice).c_str());
            return false;
        }

        if (!params.isSupported)
        {
            NV_PERF_LOG_ERR(10, "%ls is not supported for profiling\n", D3D11GetDeviceName(pDevice).c_str());
            if (params.gpuArchitectureSupportLevel != NVPW_GPU_ARCHITECTURE_SUPPORT_LEVEL_SUPPORTED)
            {
                const DeviceIdentifiers deviceIdentifiers = D3D11GetDeviceIdentifiers(pDevice, sliIndex);
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

    inline bool D3D11IsGpuSupported(ID3D11DeviceContext* pDeviceContext, size_t sliIndex = 0)
    {
        Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
        pDeviceContext->GetDevice(&pDevice);
        if (!pDevice)
        {
            return false;
        }

        const bool isGpuSupported = D3D11IsGpuSupported(pDevice.Get(), sliIndex);
        return isGpuSupported;
    }

}}}
