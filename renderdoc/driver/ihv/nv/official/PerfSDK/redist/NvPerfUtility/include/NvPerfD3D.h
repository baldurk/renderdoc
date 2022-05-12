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

#include <dxgi.h>

namespace nv { namespace perf {

    inline bool DxgiIsNvidiaDevice(IDXGIAdapter* pAdapter)
    {
        DXGI_ADAPTER_DESC adapterDesc = {};
        HRESULT hr = pAdapter->GetDesc(&adapterDesc);
        if (FAILED(hr))
        {
            return false;
        }

        if (adapterDesc.VendorId != 0x10de)
        {
            return false;
        }

        return true;
    }

    inline size_t D3DGetNvperfDeviceIndex(IDXGIAdapter* pDXGIAdapter, size_t sliIndex = 0)
    {
        NVPW_Adapter_GetDeviceIndex_Params getDeviceIndexParams = { NVPW_Adapter_GetDeviceIndex_Params_STRUCT_SIZE };
        getDeviceIndexParams.pAdapter = pDXGIAdapter;
        getDeviceIndexParams.sliIndex = sliIndex;
        NVPA_Status nvpaStatus = NVPW_Adapter_GetDeviceIndex(&getDeviceIndexParams);
        if (nvpaStatus)
        {
            return ~size_t(0);
        }

        return getDeviceIndexParams.deviceIndex;
    }

    inline DeviceIdentifiers D3DGetDeviceIdentifiers(IDXGIAdapter* pDXGIAdapter, size_t sliIndex = 0)
    {
        const size_t deviceIndex = D3DGetNvperfDeviceIndex(pDXGIAdapter, sliIndex);

        DeviceIdentifiers deviceIdentifiers = GetDeviceIdentifiers(deviceIndex);
        return deviceIdentifiers;
    }

    inline NVPW_Device_ClockStatus D3DGetDeviceClockState(IDXGIAdapter* pDXGIAdapter)
    {
        size_t nvperfDeviceIndex = D3DGetNvperfDeviceIndex(pDXGIAdapter);
        return GetDeviceClockState(nvperfDeviceIndex);
    }

    inline bool D3DSetDeviceClockState(IDXGIAdapter* pDXGIAdapter, NVPW_Device_ClockSetting clockSetting)
    {
        size_t nvperfDeviceIndex = D3DGetNvperfDeviceIndex(pDXGIAdapter);
        return SetDeviceClockState(nvperfDeviceIndex, clockSetting);
    }

    inline bool D3DSetDeviceClockState(IDXGIAdapter* pDXGIAdapter, NVPW_Device_ClockStatus clockStatus)
    {
        size_t nvperfDeviceIndex = D3DGetNvperfDeviceIndex(pDXGIAdapter);
        return SetDeviceClockState(nvperfDeviceIndex, clockStatus);
    }
}}
