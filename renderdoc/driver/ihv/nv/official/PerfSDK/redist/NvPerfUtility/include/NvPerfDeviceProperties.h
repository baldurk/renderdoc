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

#include "nvperf_host.h"
#include "nvperf_target.h"
#include "NvPerfInit.h"
#include <vector>

namespace nv { namespace perf {
    enum
    {
        NVIDIA_VENDOR_ID = 0x10de
    };

    struct DeviceIdentifiers
    {
        const char* pDeviceName;
        const char* pChipName;
    };

    inline DeviceIdentifiers GetDeviceIdentifiers(size_t deviceIndex)
    {
        NVPW_Device_GetNames_Params getNamesParams = { NVPW_Device_GetNames_Params_STRUCT_SIZE };
        getNamesParams.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_Device_GetNames(&getNamesParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_Device_GetNames failed\n");
            return {};
        }

        DeviceIdentifiers deviceIdentifiers = {};
        deviceIdentifiers.pDeviceName = getNamesParams.pDeviceName;
        deviceIdentifiers.pChipName = getNamesParams.pChipName;

        return deviceIdentifiers;
    }

    inline NVPW_Device_ClockStatus GetDeviceClockState(size_t nvperfDeviceIndex)
    {
        NVPW_Device_GetClockStatus_Params getClockStatusParams = { NVPW_Device_GetClockStatus_Params_STRUCT_SIZE };
        getClockStatusParams.deviceIndex = nvperfDeviceIndex;
        NVPA_Status nvpaStatus = NVPW_Device_GetClockStatus(&getClockStatusParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_Device_GetClockStatus() failed on %s\n", GetDeviceIdentifiers(nvperfDeviceIndex).pDeviceName);
            return NVPW_DEVICE_CLOCK_STATUS_UNKNOWN;
        }
        return getClockStatusParams.clockStatus;
    }

    inline const char* ToCString(NVPW_Device_ClockSetting clockSetting)
    {
        switch(clockSetting)
        {
            case NVPW_DEVICE_CLOCK_SETTING_INVALID:             return "Invalid";
            case NVPW_DEVICE_CLOCK_SETTING_DEFAULT:             return "Default";
            case NVPW_DEVICE_CLOCK_SETTING_LOCK_TO_RATED_TDP:   return "Locked to rated TDP";
            default:                                            return "Unknown";
        }
    }

    inline bool SetDeviceClockState(size_t nvperfDeviceIndex, NVPW_Device_ClockSetting clockSetting)
    {
        NVPW_Device_SetClockSetting_Params setClockSettingParams = { NVPW_Device_SetClockSetting_Params_STRUCT_SIZE };
        setClockSettingParams.deviceIndex = nvperfDeviceIndex;
        setClockSettingParams.clockSetting = clockSetting;
        NVPA_Status nvpaStatus = NVPW_Device_SetClockSetting(&setClockSettingParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_Device_SetClockSetting( %s ) failed on %s\n", ToCString(clockSetting), GetDeviceIdentifiers(nvperfDeviceIndex).pDeviceName);
            return false;
        }
        return true;
    }

    inline const char* ToCString(NVPW_Device_ClockStatus clockStatus)
    {
        switch(clockStatus)
        {
            case NVPW_DEVICE_CLOCK_STATUS_LOCKED_TO_RATED_TDP:  return "Locked to rated TDP";
            case NVPW_DEVICE_CLOCK_STATUS_BOOST_ENABLED:        return "Boost enabled";
            case NVPW_DEVICE_CLOCK_STATUS_BOOST_DISABLED:       return "Boost disabled";
            default:                                            return "Unknown";
        }
    }

    inline bool SetDeviceClockState(size_t nvperfDeviceIndex, NVPW_Device_ClockStatus clockStatus)
    {
        // convert to NVPW_Device_ClockSetting
        NVPW_Device_ClockSetting clockSetting = NVPW_DEVICE_CLOCK_SETTING_INVALID;
        switch (clockStatus)
        {
            case NVPW_DEVICE_CLOCK_STATUS_UNKNOWN:
            case NVPW_DEVICE_CLOCK_STATUS_BOOST_ENABLED:
            case NVPW_DEVICE_CLOCK_STATUS_BOOST_DISABLED:
                // default driver setting (normally unlocked and not boosted, but could be unlocked boosted, or locked to rated TDP)
                clockSetting = NVPW_DEVICE_CLOCK_SETTING_DEFAULT;
                break;
            case NVPW_DEVICE_CLOCK_STATUS_LOCKED_TO_RATED_TDP:
                clockSetting = NVPW_DEVICE_CLOCK_SETTING_LOCK_TO_RATED_TDP;
                break;
            default:
                NV_PERF_LOG_ERR(10, "Invalid clockStatus: %s\n", ToCString(clockStatus));
                return false;
        }
        return SetDeviceClockState(nvperfDeviceIndex, clockSetting);
    }
}}