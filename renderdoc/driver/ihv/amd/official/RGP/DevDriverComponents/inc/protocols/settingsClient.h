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
* @file  settingsClient.h
* @brief Protocol Client for the Settings Protocol
***********************************************************************************************************************
*/

#pragma once

#define SETTINGS_CLIENT_MIN_MAJOR_VERSION 1
#define SETTINGS_CLIENT_MAX_MAJOR_VERSION 2

#include "settingsProtocol.h"
#include "baseProtocolClient.h"

namespace DevDriver
{
    namespace SettingsProtocol
    {
        class SettingsClient : public BaseProtocolClient
        {
        public:
            explicit SettingsClient(IMsgChannel* pMsgChannel);
            ~SettingsClient();

            Result QueryNumCategories(uint32* pNumCategories);
            Result QueryCategories(SettingCategory* pCategoriesBuffer, uint32 bufferSizeInCategories);

            Result QueryNumSettings(uint32* pNumSettings);
            Result QuerySettings(Setting* pSettingsBuffer, uint32 bufferSizeInSettings);

            Result QuerySetting(const char* pName, Setting* pSetting);

            Result SetSetting(const char* pName, const SettingValue* pValue);

        private:
        };
    }
} // DevDriver
