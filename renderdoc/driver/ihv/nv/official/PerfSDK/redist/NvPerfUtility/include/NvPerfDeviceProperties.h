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

    struct ClockInfo
    {
        NVPW_Device_ClockStatus clockStatus = NVPW_DEVICE_CLOCK_STATUS_UNKNOWN;
        NVPW_Device_ClockLevel clockLevel= NVPW_DEVICE_CLOCK_LEVEL_INVALID;

        ClockInfo()
            : clockStatus(NVPW_DEVICE_CLOCK_STATUS_UNKNOWN), clockLevel(NVPW_DEVICE_CLOCK_LEVEL_INVALID) {}

        ClockInfo(NVPW_Device_ClockStatus status, NVPW_Device_ClockLevel level)
            : clockStatus(status), clockLevel(level) {}
    };

    inline DeviceIdentifiers GetDeviceIdentifiers(size_t deviceIndex)
    {
        NVPW_Device_GetNames_Params getNamesParams = { NVPW_Device_GetNames_Params_STRUCT_SIZE };
        getNamesParams.deviceIndex = deviceIndex;
        NVPA_Status nvpaStatus = NVPW_Device_GetNames(&getNamesParams);
        if (nvpaStatus)
        {
            NV_PERF_LOG_ERR(10, "NVPW_Device_GetNames failed, nvpaStatus = %s\n", FormatStatus(nvpaStatus).c_str());
            return {};
        }

        DeviceIdentifiers deviceIdentifiers = {};
        deviceIdentifiers.pDeviceName = getNamesParams.pDeviceName;
        deviceIdentifiers.pChipName = getNamesParams.pChipName;

        return deviceIdentifiers;
    }

    inline ClockInfo GetDeviceClockState(size_t nvperfDeviceIndex)
    {
        NVPW_Device_GetClockStatus_Params getClockStatusParams = { NVPW_Device_GetClockStatus_Params_STRUCT_SIZE };
        getClockStatusParams.deviceIndex = nvperfDeviceIndex;
        NVPA_Status nvpaStatus = NVPW_Device_GetClockStatus(&getClockStatusParams);
        ClockInfo clockInfo;
        if (nvpaStatus)
        {
#ifdef __aarch64__
            uint32_t level = 100;
#else
            uint32_t level = 10;
#endif
            NV_PERF_LOG_ERR(level, "NVPW_Device_GetClockStatus() failed on %s, nvpaStatus = %s\n", GetDeviceIdentifiers(nvperfDeviceIndex).pDeviceName, FormatStatus(nvpaStatus).c_str());
            return clockInfo;
        }
        return ClockInfo(getClockStatusParams.clockStatus, getClockStatusParams.clockLevel);
    }

    inline const char* ToCString(NVPW_Device_ClockSetting clockSetting)
    {
        switch(clockSetting)
        {
            case NVPW_DEVICE_CLOCK_SETTING_INVALID:             return "Invalid";
            case NVPW_DEVICE_CLOCK_SETTING_DEFAULT:             return "Default";
            case NVPW_DEVICE_CLOCK_SETTING_LOCK_TO_RATED_TDP:   return "Locked to rated TDP";
            case NVPW_DEVICE_CLOCK_SETTING_LOCK_TO_TURBO_BOOST: return "Locked to turbo boost";
            default:                                            return "Unknown";
        }
    }

    inline const char* ToCString(const ClockInfo& clockInfo)
    {
        switch(clockInfo.clockStatus)
        {
            case NVPW_DEVICE_CLOCK_STATUS_LOCKED:
                switch (clockInfo.clockLevel)
                {
                    case NVPW_DEVICE_CLOCK_LEVEL_RATED_TDP:     return "Locked to rated TDP";
                    case NVPW_DEVICE_CLOCK_LEVEL_TURBO_BOOST:   return "Locked to turbo boost";
                    default:                                    return "Invalid";
                }
            case NVPW_DEVICE_CLOCK_STATUS_BOOST_ENABLED:        return "Unlocked, turbo boost enabled";
            case NVPW_DEVICE_CLOCK_STATUS_BOOST_DISABLED:       return "Unlocked, turbo boost disabled";
            case NVPW_DEVICE_CLOCK_STATUS_UNLOCKED:             return "Unlocked";
            default:                                            return "Uknown";
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
#ifdef __aarch64__
            uint32_t level = 100;
#else
            uint32_t level = 10;
#endif
            NV_PERF_LOG_ERR(level, "NVPW_Device_SetClockSetting( %s ) failed on %s, nvpaStatus = %s\n", ToCString(clockSetting), GetDeviceIdentifiers(nvperfDeviceIndex).pDeviceName, FormatStatus(nvpaStatus).c_str());
            return false;
        }
        return true;
    }

    inline bool SetDeviceClockState(size_t nvperfDeviceIndex, const ClockInfo& clockInfo)
    {
        // convert to NVPW_Device_ClockSetting
        NVPW_Device_ClockSetting clockSetting = NVPW_DEVICE_CLOCK_SETTING_INVALID;
        switch (clockInfo.clockStatus)
        {
            case NVPW_DEVICE_CLOCK_STATUS_UNKNOWN:
            case NVPW_DEVICE_CLOCK_STATUS_BOOST_ENABLED:
            case NVPW_DEVICE_CLOCK_STATUS_BOOST_DISABLED:
                // default driver setting (normally unlocked and not boosted, but could be unlocked boosted, or locked to rated TDP)
                clockSetting = NVPW_DEVICE_CLOCK_SETTING_DEFAULT;
                break;
            case NVPW_DEVICE_CLOCK_STATUS_LOCKED:
                switch (clockInfo.clockLevel)
                {
                    case NVPW_DEVICE_CLOCK_LEVEL_RATED_TDP:
                        clockSetting = NVPW_DEVICE_CLOCK_SETTING_LOCK_TO_RATED_TDP;
                        break;
                    case NVPW_DEVICE_CLOCK_LEVEL_TURBO_BOOST:
                        clockSetting = NVPW_DEVICE_CLOCK_SETTING_LOCK_TO_TURBO_BOOST;
                        break;
                    default:
                        clockSetting = NVPW_DEVICE_CLOCK_SETTING_DEFAULT;
                        break;
                }
            default:
                NV_PERF_LOG_ERR(10, "Invalid clockStatus: %s\n", ToCString(clockInfo));
                return false;
        }
        return SetDeviceClockState(nvperfDeviceIndex, clockSetting);
    }
}}
