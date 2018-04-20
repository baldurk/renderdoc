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

#include "protocols/settingsServer.h"
#include "msgChannel.h"
#include <cstring>

namespace DevDriver
{
    namespace SettingsProtocol
    {
        enum class SessionState
        {
            ReceivePayload = 0,
            ProcessPayload,
            SendPayload,
            SendSettingsNumResponse,
            SendSettingsDataResponse,
            SendCategoriesNumResponse,
            SendCategoriesDataResponse
        };

        struct SettingsSession
        {
            SessionState state;
            SettingsPayload payload;
            uint32 itemIndex;
            uint32 numItems;
        };

        SettingsServer::SettingsServer(IMsgChannel* pMsgChannel)
            : BaseProtocolServer(pMsgChannel, Protocol::Settings, SETTINGS_CLIENT_MIN_MAJOR_VERSION, SETTINGS_CLIENT_MAX_MAJOR_VERSION)
            , m_settings(pMsgChannel->GetAllocCb())
            , m_categories(pMsgChannel->GetAllocCb())
        {
            DD_ASSERT(m_pMsgChannel != nullptr);
        }

        SettingsServer::~SettingsServer()
        {
        }

        void SettingsServer::Finalize()
        {
            LockData();
            BaseProtocolServer::Finalize();
            UnlockData();
        }

        bool SettingsServer::AcceptSession(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);
            return true;
        }

        void SettingsServer::SessionEstablished(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);

            // Allocate session data for the newly established session
            SettingsSession* pSessionData = DD_NEW(SettingsSession, m_pMsgChannel->GetAllocCb())();
            pSessionData->state = SessionState::ReceivePayload;
            pSessionData->payload = {};
            pSession->SetUserData(pSessionData);
        }

        void SettingsServer::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            SettingsSession* pSessionData = reinterpret_cast<SettingsSession*>(pSession->GetUserData());

            switch (pSessionData->state)
            {
                case SessionState::ReceivePayload:
                {
                    uint32 bytesReceived = 0;
                    Result result = pSession->Receive(sizeof(pSessionData->payload), &pSessionData->payload, &bytesReceived, kNoWait);

                    if (result == Result::Success)
                    {
                        DD_ASSERT(sizeof(pSessionData->payload) == bytesReceived);
                        pSessionData->state = SessionState::ProcessPayload;
                    }

                    break;
                }

                case SessionState::ProcessPayload:
                {
                    switch (static_cast<SettingsMessage>(pSessionData->payload.command))
                    {
                        case SettingsMessage::QueryNumSettingsRequest:
                        {
                            LockData();
                            const uint32 numSettings = static_cast<uint32>(m_settings.Size());
                            UnlockData();

                            pSessionData->payload.command = SettingsMessage::QueryNumSettingsResponse;
                            pSessionData->payload.queryNumSettingsResponse.numSettings = numSettings;

                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }
                        case SettingsMessage::QuerySettingsRequest:
                        {
                            LockData();
                            const uint32 numSettings = static_cast<uint32>(m_settings.Size());
                            UnlockData();

                            pSessionData->payload.command = SettingsMessage::QuerySettingsNumResponse;
                            pSessionData->payload.querySettingsNumResponse.numSettings = numSettings;

                            pSessionData->state = SessionState::SendSettingsNumResponse;

                            break;
                        }
                        case SettingsMessage::QuerySettingRequest:
                        {
                            pSessionData->payload.command = SettingsMessage::QuerySettingResponse;
                            pSessionData->payload.querySettingResponse.success = false;

                            LockData();
                            int32 settingIndex = FindSetting(pSessionData->payload.querySettingRequest.name);
                            if (settingIndex != -1)
                            {
                                pSessionData->payload.querySettingResponse.success = true;
                                memcpy(&pSessionData->payload.querySettingResponse.setting, &m_settings[settingIndex], sizeof(Setting));
#if SETTINGS_PROTOCOL_SUPPORTS(SETTINGS_HEX_VERSION)
                                if ((pSession->GetVersion() < SETTINGS_HEX_VERSION) & (pSessionData->payload.querySettingResponse.setting.type == SettingType::Hex))
                                {
                                    pSessionData->payload.querySettingResponse.setting.type = SettingType::UnsignedInteger;
                                }
#endif
                            }
                            UnlockData();

                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }
                        case SettingsMessage::SetSettingRequest:
                        {
                            Setting* pSetting = nullptr;

                            LockData();

                            if (m_isFinalized == false)
                            {
                                for (auto &setting : m_settings)
                                {
                                    if (strcmp(setting.name, pSessionData->payload.setSettingRequest.name) == 0)
                                    {
                                        pSetting = &setting;
                                        break;
                                    }
                                }

                                if (pSetting != nullptr)
                                {
                                    memcpy(&pSetting->value, &pSessionData->payload.setSettingRequest.value, sizeof(SettingValue));
                                }
                            }
                            UnlockData();

                            pSessionData->payload.command = SettingsMessage::SetSettingResponse;
                            pSessionData->payload.setSettingResponse.success = (pSetting != nullptr);

                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }
                        case SettingsMessage::QueryNumCategoriesRequest:
                        {
                            LockData();
                            const uint32 numCategories = static_cast<uint32>(m_categories.Size());
                            UnlockData();

                            pSessionData->payload.command = SettingsMessage::QueryNumCategoriesResponse;
                            pSessionData->payload.queryNumCategoriesResponse.numCategories = numCategories;

                            pSessionData->state = SessionState::SendPayload;

                            break;
                        }
                        case SettingsMessage::QueryCategoriesRequest:
                        {
                            LockData();
                            const uint32 numCategories = static_cast<uint32>(m_categories.Size());
                            UnlockData();

                            pSessionData->payload.command = SettingsMessage::QueryCategoriesNumResponse;
                            pSessionData->payload.queryCategoriesNumResponse.numCategories = numCategories;

                            pSessionData->state = SessionState::SendCategoriesNumResponse;

                            break;
                        }

                        default:
                        {
                            DD_UNREACHABLE();
                            break;
                        }
                    }

                    break;
                }

                case SessionState::SendPayload:
                {
                    Result result = pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait);
                    if (result == Result::Success)
                    {
                        pSessionData->state = SessionState::ReceivePayload;
                    }

                    break;
                }

                case SessionState::SendSettingsNumResponse:
                {
                    Result result = pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait);
                    if (result == Result::Success)
                    {
                        pSessionData->itemIndex = 0;
                        pSessionData->numItems = pSessionData->payload.querySettingsNumResponse.numSettings;
                        pSessionData->state = SessionState::SendSettingsDataResponse;

                        // Prepare the payload for the first data response
                        pSessionData->payload.command = SettingsMessage::QuerySettingsDataResponse;
                        if (pSessionData->numItems > 0)
                        {
                            LockData();
                            memcpy(&pSessionData->payload.querySettingsDataResponse.setting, &m_settings[0], sizeof(Setting));
#if SETTINGS_PROTOCOL_SUPPORTS(SETTINGS_HEX_VERSION)
                            if ((pSession->GetVersion() < SETTINGS_HEX_VERSION) & (pSessionData->payload.querySettingResponse.setting.type == SettingType::Hex))
                            {
                                pSessionData->payload.querySettingsDataResponse.setting.type = SettingType::UnsignedInteger;
                            }
#endif
                            UnlockData();
                        }
                    }

                    break;
                }

                case SessionState::SendSettingsDataResponse:
                {
                    if (pSessionData->itemIndex < pSessionData->numItems)
                    {
                        while (pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait) == Result::Success)
                        {
                            ++pSessionData->itemIndex;

                            // Prepare the payload for the next data response if necessary
                            if (pSessionData->itemIndex < pSessionData->numItems)
                            {
                                LockData();
                                memcpy(&pSessionData->payload.querySettingsDataResponse.setting, &m_settings[pSessionData->itemIndex], sizeof(Setting));
#if SETTINGS_PROTOCOL_SUPPORTS(SETTINGS_HEX_VERSION)
                                if ((pSession->GetVersion() < SETTINGS_HEX_VERSION) & (pSessionData->payload.querySettingResponse.setting.type == SettingType::Hex))
                                {
                                    pSessionData->payload.querySettingsDataResponse.setting.type = SettingType::UnsignedInteger;
                                }
#endif
                                UnlockData();
                            }
                            else
                            {
                                // Break out of the send loop if we've finished sending all of the responses
                                break;
                            }
                        }
                    }
                    else
                    {
                        // We've sent all the responses. Return to normal operation.
                        pSessionData->state = SessionState::ReceivePayload;
                    }

                    break;
                }

                case SessionState::SendCategoriesNumResponse:
                {
                    Result result = pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait);
                    if (result == Result::Success)
                    {
                        pSessionData->itemIndex = 0;
                        pSessionData->numItems = pSessionData->payload.queryCategoriesNumResponse.numCategories;
                        pSessionData->state = SessionState::SendCategoriesDataResponse;

                        // Prepare the payload for the first data response
                        pSessionData->payload.command = SettingsMessage::QueryCategoriesDataResponse;
                        if (pSessionData->numItems > 0)
                        {
                            LockData();
                            memcpy(&pSessionData->payload.queryCategoriesDataResponse.category, &m_categories[0], sizeof(SettingCategory));
                            UnlockData();
                        }
                    }

                    break;
                }

                case SessionState::SendCategoriesDataResponse:
                {
                    if (pSessionData->itemIndex < pSessionData->numItems)
                    {
                        while (pSession->Send(sizeof(pSessionData->payload), &pSessionData->payload, kNoWait) == Result::Success)
                        {
                            ++pSessionData->itemIndex;

                            // Prepare the payload for the next data response if necessary
                            if (pSessionData->itemIndex < pSessionData->numItems)
                            {
                                LockData();
                                memcpy(&pSessionData->payload.queryCategoriesDataResponse.category, &m_categories[pSessionData->itemIndex], sizeof(SettingCategory));
                                UnlockData();
                            }
                            else
                            {
                                // Break out of the send loop if we've finished sending all of the responses
                                break;
                            }
                        }
                    }
                    else
                    {
                        // We've sent all the responses. Return to normal operation.
                        pSessionData->state = SessionState::ReceivePayload;
                    }

                    break;
                }

                default:
                {
                    DD_UNREACHABLE();
                    break;
                }
            }
        }

        void SettingsServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            DD_UNUSED(terminationReason);
            SettingsSession *pSettingsSession = reinterpret_cast<SettingsSession*>(pSession->SetUserData(nullptr));

            // Free the session data
            if (pSettingsSession != nullptr)
            {
                DD_DELETE(pSettingsSession, m_pMsgChannel->GetAllocCb());
            }
        }

        void SettingsServer::AddCategory(const char* pName, const char* pParentName)
        {
            int32 categoryIndex = FindCategory(pName);

            // Make sure we don't already have this category
            if (categoryIndex == -1)
            {
                // Default to the root category if no parent name is provided
                int32 parentCategoryIndex = -1;
                if (pParentName != nullptr)
                {
                    parentCategoryIndex = FindCategory(pParentName);

                    // Make sure the parent category name is valid
                    DD_ASSERT(parentCategoryIndex != -1);
                }

                SettingCategory category = {};
                Platform::Strncpy(category.name, pName, sizeof(category.name));
                category.parentIndex = parentCategoryIndex;

                m_categories.PushBack(category);
            }
        }

        int32 SettingsServer::QueryCategoryIndex(const char* pName) const
        {
            return FindCategory(pName);
        }

        void SettingsServer::AddSetting(const Setting* pSetting)
        {
            DD_ASSERT(pSetting != nullptr);

            // Make sure the setting refers to a valid category index
            DD_ASSERT(pSetting->categoryIndex < static_cast<uint32>(m_categories.Size()));

            LockData();
            m_settings.PushBack(*pSetting);
            UnlockData();
        }

        bool SettingsServer::QuerySetting(const char* pName, Setting* pSetting)
        {
            LockData();
            int32 settingIndex = FindSetting(pName);
            if (settingIndex != -1)
            {
                memcpy(pSetting, &m_settings[settingIndex], sizeof(Setting));
            }
            UnlockData();

            return (settingIndex != -1);
        }

        bool SettingsServer::QuerySettingByIndex(uint32 settingIndex, Setting* pSetting)
        {
            bool result = false;

            LockData();
            if (settingIndex < m_settings.Size())
            {
                memcpy(pSetting, &m_settings[settingIndex], sizeof(Setting));
                result = true;
            }
            UnlockData();

            return result;
        }

        bool SettingsServer::UpdateSetting(const char* pName, const SettingValue* pValue)
        {
            LockData();
            int32 settingIndex = FindSetting(pName);
            if (settingIndex != -1)
            {
                memcpy(&m_settings[settingIndex].value, pValue, sizeof(SettingValue));
            }
            UnlockData();

            return (settingIndex != -1);
        }

        bool SettingsServer::UpdateSettingByIndex(uint32 settingIndex, const SettingValue* pValue)
        {
            bool result = false;

            LockData();
            if (settingIndex < m_settings.Size())
            {
                memcpy(&m_settings[settingIndex].value, pValue, sizeof(SettingValue));
                result = true;
            }
            UnlockData();

            return result;
        }

        uint32 SettingsServer::GetNumSettings()
        {
            LockData();
            uint32 numSettings = static_cast<uint32>(m_settings.Size());
            UnlockData();

            return numSettings;
        }

        uint32 SettingsServer::GetNumCategories()
        {
            LockData();
            uint32 numCategories = static_cast <uint32>(m_categories.Size());
            UnlockData();

            return numCategories;
        }

        void SettingsServer::LockData()
        {
            m_mutex.Lock();
        }

        void SettingsServer::UnlockData()
        {
            m_mutex.Unlock();
        }

        int32 SettingsServer::FindCategory(const char* pCategoryName) const
        {
            DD_ASSERT(pCategoryName != nullptr);

            int32 targetCategoryIndex = -1;

            for (uint32 categoryIndex = 0; categoryIndex < static_cast<uint32>(m_categories.Size()); ++categoryIndex)
            {
                if (strcmp(m_categories[categoryIndex].name, pCategoryName) == 0)
                {
                    targetCategoryIndex = static_cast<int32>(categoryIndex);
                    break;
                }
            }

            return targetCategoryIndex;
        }

        int32 SettingsServer::FindSetting(const char* pSettingName) const
        {
            DD_ASSERT(pSettingName != nullptr);

            int32 targetSettingIndex = -1;

            for (uint32 settingIndex = 0; settingIndex < static_cast<uint32>(m_settings.Size()); ++settingIndex)
            {
                if (strcmp(m_settings[settingIndex].name, pSettingName) == 0)
                {
                    targetSettingIndex = static_cast<int32>(settingIndex);
                    break;
                }
            }

            return targetSettingIndex;
        }
    }
} // DevDriver
