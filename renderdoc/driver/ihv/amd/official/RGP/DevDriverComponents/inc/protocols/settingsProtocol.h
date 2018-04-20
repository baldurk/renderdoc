/*
 *******************************************************************************
 *
 * Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/
/**
***********************************************************************************************************************
* @file  settingsProtocol.h
* @brief Protocol header for the Settings Protocol
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"

#define SETTINGS_PROTOCOL_MAJOR_VERSION 2
#define SETTINGS_PROTOCOL_MINOR_VERSION 0

#define SETTINGS_INTERFACE_VERSION ((GPUOPEN_INTERFACE_MAJOR_VERSION << 16) | GPUOPEN_INTERFACE_MINOR_VERSION)

#define SETTINGS_PROTOCOL_MINIMUM_MAJOR_VERSION 1

#ifndef SETTINGS_CLIENT_MIN_MAJOR_VERSION
static_assert(false, "Client must define SETTINGS_CLIENT_MIN_MAJOR_VERSION.");
#else
static_assert((SETTINGS_CLIENT_MIN_MAJOR_VERSION >= SETTINGS_PROTOCOL_MINIMUM_MAJOR_VERSION) &&
(SETTINGS_CLIENT_MIN_MAJOR_VERSION <= SETTINGS_PROTOCOL_MAJOR_VERSION),
"The specified SETTINGS_CLIENT_MIN_MAJOR_VERSION is not supported.");
#endif

#ifndef SETTINGS_CLIENT_MAX_MAJOR_VERSION
static_assert(false, "Client must define SETTINGS_CLIENT_MAX_MAJOR_VERSION.");
#else
static_assert((SETTINGS_CLIENT_MAX_MAJOR_VERSION >= SETTINGS_PROTOCOL_MINIMUM_MAJOR_VERSION) &&
(SETTINGS_CLIENT_MAX_MAJOR_VERSION <= SETTINGS_PROTOCOL_MAJOR_VERSION),
"The specified SETTINGS_CLIENT_MAX_MAJOR_VERSION is not supported.");
#endif

/*
***********************************************************************************************************************
*| Version | Change Description                                                                                       |
*| ------- | ---------------------------------------------------------------------------------------------------------|
*|  2.0    | Add hex setting type to protocol.                                                                        |
*|  1.0    | Initial version.                                                                                         |
***********************************************************************************************************************
*/

#define SETTINGS_HEX_VERSION 2
#define SETTINGS_INITIAL_VERSION 1

#define SETTINGS_PROTOCOL_SUPPORTS(x) ((SETTINGS_CLIENT_MAX_MAJOR_VERSION >= x) && (x >= SETTINGS_CLIENT_MIN_MAJOR_VERSION))

namespace DevDriver
{

    namespace SettingsProtocol
    {
        ///////////////////////
        // Settings Constants
        DD_STATIC_CONST uint32 kSmallStringSize = 64;
        DD_STATIC_CONST uint32 kLargeStringSize = 256;

        ///////////////////////
        // Settings Protocol
        enum struct SettingsMessage : MessageCode
        {
            Unknown = 0,
            QueryNumSettingsRequest,
            QueryNumSettingsResponse,
            QuerySettingsRequest,
            QuerySettingsNumResponse,
            QuerySettingsDataResponse,
            QuerySettingRequest,
            QuerySettingResponse,
            SetSettingRequest,
            SetSettingResponse,
            QueryNumCategoriesRequest,
            QueryNumCategoriesResponse,
            QueryCategoriesRequest,
            QueryCategoriesNumResponse,
            QueryCategoriesDataResponse,
            Count
        };

        ///////////////////////
        // Setting Types
        enum struct SettingType : uint32
        {
            Unknown = 0,
            Boolean,
            Integer,
            UnsignedInteger,
            Float,
            String,
#if SETTINGS_PROTOCOL_SUPPORTS(SETTINGS_HEX_VERSION)
            Hex,
#endif
            Count
        };

        ///////////////////////
        // Setting Value Structure
        DD_NETWORK_STRUCT(SettingValue, 4)
        {
            union
            {
                bool boolValue;
                int32 integerValue;
                uint32 unsignedIntegerValue;
#if SETTINGS_PROTOCOL_SUPPORTS(SETTINGS_HEX_VERSION)
                uint32 hexValue;
#endif
                float floatValue;
                char stringValue[kSmallStringSize];
            };
        };

        DD_CHECK_SIZE(SettingValue, 64);

        ///////////////////////
        // Setting Structure
        DD_NETWORK_STRUCT(Setting, 4)
        {
            char description[kLargeStringSize];
            char name[kSmallStringSize];
            SettingValue value;
            SettingValue defaultValue;
            SettingType type;
            uint32 categoryIndex;
        };

        DD_CHECK_SIZE(Setting, 456);

        ///////////////////////
        // Category Structure
        DD_NETWORK_STRUCT(SettingCategory, 4)
        {
            char name[kSmallStringSize];
            int32 parentIndex;
        };

        DD_CHECK_SIZE(SettingCategory, 68);

        ///////////////////////
        // Payloads
        DD_NETWORK_STRUCT(QueryNumSettingsResponsePayload, 4)
        {
            uint32 numSettings;
        };
        DD_CHECK_SIZE(QueryNumSettingsResponsePayload, 4);

        DD_NETWORK_STRUCT(QuerySettingsNumResponsePayload, 4)
        {
            uint32 numSettings;
        };
        DD_CHECK_SIZE(QuerySettingsNumResponsePayload, 4);

        DD_NETWORK_STRUCT(QuerySettingsDataResponsePayload, 4)
        {
            Setting setting;
        };

        DD_CHECK_SIZE(QuerySettingsDataResponsePayload, 456);

        DD_NETWORK_STRUCT(QuerySettingRequestPayload, 4)
        {
            char name[kSmallStringSize];
        };
        DD_CHECK_SIZE(QuerySettingRequestPayload, 64);

        DD_NETWORK_STRUCT(QuerySettingResponsePayload, 4)
        {
            Setting setting;
            // todo: replace this with something that is more verbose at a later date
            //       avoided for the time being as it would cause a breaking API change
            uint8 success;
            // pad out for alignment requirements
            uint8 padding[3];
        };

        DD_CHECK_SIZE(QuerySettingResponsePayload, 460);

        DD_NETWORK_STRUCT(SetSettingRequestPayload, 4)
        {
            char name[kSmallStringSize];
            SettingValue value;
        };

        DD_CHECK_SIZE(SetSettingRequestPayload, 128);

        DD_NETWORK_STRUCT(SetSettingResponsePayload, 4)
        {
            // todo: replace this with something that is more verbose at a later date
            //       avoided for the time being as it would cause a breaking API change
            uint8 success;
            // pad out to 4 bytes for alignment requirements
            uint8 padding[3];
        };

        DD_CHECK_SIZE(SetSettingResponsePayload, 4);

        DD_NETWORK_STRUCT(QueryNumCategoriesResponsePayload, 4)
        {
            uint32 numCategories;
        };

        DD_CHECK_SIZE(QueryNumCategoriesResponsePayload, 4);

        DD_NETWORK_STRUCT(QueryCategoriesNumResponsePayload, 4)
        {
            uint32 numCategories;
        };

        DD_CHECK_SIZE(QueryCategoriesNumResponsePayload, 4);

        DD_NETWORK_STRUCT(QueryCategoriesDataResponsePayload, 4)
        {
            SettingCategory category;
        };

        DD_CHECK_SIZE(QueryCategoriesDataResponsePayload, 68);

        DD_NETWORK_STRUCT(SettingsPayload, 4)
        {
            SettingsMessage command;
            // pad out to 4 bytes for alignment requirements
            char            padding[3];
            union
            {
                QueryNumSettingsResponsePayload    queryNumSettingsResponse;
                QuerySettingsNumResponsePayload    querySettingsNumResponse;
                QuerySettingsDataResponsePayload   querySettingsDataResponse;
                QuerySettingRequestPayload         querySettingRequest;
                QuerySettingResponsePayload        querySettingResponse;
                SetSettingRequestPayload           setSettingRequest;
                SetSettingResponsePayload          setSettingResponse;
                QueryNumCategoriesResponsePayload  queryNumCategoriesResponse;
                QueryCategoriesNumResponsePayload  queryCategoriesNumResponse;
                QueryCategoriesDataResponsePayload queryCategoriesDataResponse;
            };
        };

        DD_CHECK_SIZE(SettingsPayload, 464);
    }
}
