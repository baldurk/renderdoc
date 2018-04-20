/*
 *******************************************************************************
 *
 * Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
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

#include "protocols/settingsClient.h"
#include "msgChannel.h"
#include <cstring>

namespace DevDriver
{
    namespace SettingsProtocol
    {

        SettingsClient::SettingsClient(IMsgChannel* pMsgChannel)
            : BaseProtocolClient(pMsgChannel, Protocol::Settings, SETTINGS_CLIENT_MIN_MAJOR_VERSION, SETTINGS_CLIENT_MAX_MAJOR_VERSION)

        {
        }

        SettingsClient::~SettingsClient()
        {
        }

        Result SettingsClient::QueryNumSettings(uint32* pNumSettings)
        {
            Result result = Result::Error;

            if (IsConnected() && (pNumSettings != nullptr))
            {
                SettingsPayload payload = {};
                payload.command = SettingsMessage::QueryNumSettingsRequest;

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                }

                if (result == Result::Success)
                {
                    if (payload.command == SettingsMessage::QueryNumSettingsResponse)
                    {
                        *pNumSettings = payload.queryNumSettingsResponse.numSettings;
                    }
                    else
                    {
                        // Invalid response payload
                        result = Result::Error;
                    }
                }
            }

            return result;
        }

        Result SettingsClient::QuerySettings(Setting* pSettingsBuffer, uint32 bufferSizeInSettings)
        {
            Result result = Result::Error;

            if (IsConnected() && (pSettingsBuffer != nullptr) && (bufferSizeInSettings > 0))
            {
                SettingsPayload payload = {};
                payload.command = SettingsMessage::QuerySettingsRequest;

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                }

                if (result == Result::Success)
                {
                    if (payload.command == SettingsMessage::QuerySettingsNumResponse)
                    {
                        uint32 settingsSent = payload.querySettingsNumResponse.numSettings;
                        uint32 settingsReceived = 0;
                        for (uint32 settingIndex = 0; settingIndex < settingsSent; ++settingIndex)
                        {
                            result = ReceivePayload(payload);
                            if (result == Result::Success)
                            {
                                if (payload.command == SettingsMessage::QuerySettingsDataResponse)
                                {
                                    if (settingsReceived < bufferSizeInSettings)
                                    {
                                        memcpy(pSettingsBuffer + settingsReceived, &payload.querySettingsDataResponse.setting, sizeof(Setting));
                                        ++settingsReceived;
                                    }
                                }
                                else
                                {
                                    // Invalid response payload
                                    result = Result::Error;
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        // Invalid response payload
                        result = Result::Error;
                    }
                }
            }

            return result;
        }

        Result SettingsClient::QuerySetting(const char* pName, Setting* pSetting)
        {
            Result result = Result::Error;

            if (IsConnected() && (pName != nullptr) && (pSetting != nullptr))
            {
                SettingsPayload payload = {};
                payload.command = SettingsMessage::QuerySettingRequest;
                Platform::Strncpy(payload.querySettingRequest.name, pName, sizeof(payload.querySettingRequest.name));

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                }

                if (result == Result::Success)
                {
                    if (payload.command == SettingsMessage::QuerySettingResponse)
                    {
                        if (payload.querySettingResponse.success)
                        {
                            memcpy(pSetting, &payload.querySettingResponse.setting, sizeof(Setting));
                        }
                        else
                        {
                            // The connected client failed to set the requested setting
                            result = Result::Error;
                        }
                    }
                    else
                    {
                        // Invalid response payload
                        result = Result::Error;
                    }
                }
            }

            return result;
        }

        Result SettingsClient::SetSetting(const char* pName, const SettingValue* pValue)
        {
            Result result = Result::Error;

            if (IsConnected() && (pName != nullptr) && (pValue != nullptr))
            {
                SettingsPayload payload = {};
                payload.command = SettingsMessage::SetSettingRequest;
                Platform::Strncpy(payload.setSettingRequest.name, pName, sizeof(payload.setSettingRequest.name));
                memcpy(&payload.setSettingRequest.value, pValue, sizeof(SettingValue));

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                }

                if (result == Result::Success)
                {
                    if (payload.command == SettingsMessage::SetSettingResponse)
                    {
                        result = (payload.setSettingResponse.success ? Result::Success : Result::Error);
                    }
                    else
                    {
                        // Invalid response payload
                        result = Result::Error;
                    }
                }
            }

            return result;
        }

        Result SettingsClient::QueryNumCategories(uint32* pNumCategories)
        {
            Result result = Result::Error;

            if (IsConnected() && (pNumCategories != nullptr))
            {
                SettingsPayload payload = {};
                payload.command = SettingsMessage::QueryNumCategoriesRequest;

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                }

                if (result == Result::Success)
                {
                    if (payload.command == SettingsMessage::QueryNumCategoriesResponse)
                    {
                        *pNumCategories = payload.queryNumCategoriesResponse.numCategories;
                    }
                    else
                    {
                        // Invalid response payload
                        result = Result::Error;
                    }
                }
            }

            return result;
        }

        Result SettingsClient::QueryCategories(SettingCategory* pCategoriesBuffer, uint32 bufferSizeInCategories)
        {
            Result result = Result::Error;

            if (IsConnected() && (pCategoriesBuffer != nullptr) && (bufferSizeInCategories > 0))
            {
                SettingsPayload payload = {};
                payload.command = SettingsMessage::QueryCategoriesRequest;

                result = SendPayload(payload);
                if (result == Result::Success)
                {
                    result = ReceivePayload(payload);
                }

                if (result == Result::Success)
                {
                    if (payload.command == SettingsMessage::QueryCategoriesNumResponse)
                    {
                        uint32 categoriesSent = payload.queryCategoriesNumResponse.numCategories;
                        uint32 categoriesReceived = 0;
                        for (uint32 categoryIndex = 0; categoryIndex < categoriesSent; ++categoryIndex)
                        {
                            result = ReceivePayload(payload);
                            if (result == Result::Success)
                            {
                                if (payload.command == SettingsMessage::QueryCategoriesDataResponse)
                                {
                                    if (categoriesReceived < bufferSizeInCategories)
                                    {
                                        memcpy(pCategoriesBuffer + categoriesReceived, &payload.queryCategoriesDataResponse.category, sizeof(SettingCategory));
                                        ++categoriesReceived;
                                    }
                                }
                                else
                                {
                                    // Invalid response payload
                                    result = Result::Error;
                                    break;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                    }
                    else
                    {
                        // Invalid response payload
                        result = Result::Error;
                    }
                }
            }

            return result;
        }
    }
} // DevDriver
