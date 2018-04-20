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
* @file  udpListenerTransport.h
* @brief Class declaration for BaseMsgChannel
***********************************************************************************************************************
*/

#pragma once

#include "transports/abstractListenerTransport.h"
#include "../inc/msgTransport.h"
#include <memory>

namespace DevDriver
{
    class HostListenerTransport;

    class HostMsgTransport : public IMsgTransport
    {
    public:
        HostMsgTransport(const std::shared_ptr<HostListenerTransport> &pHostTransport,
                         ClientId hostClientId);
        ~HostMsgTransport();

        Result Connect(ClientId* pClientId, uint32 timeoutInMs) override;
        Result Disconnect() override;

        Result ReadMessage(MessageBuffer &messageBuffer, uint32 timeoutInMs) override;
        Result WriteMessage(const MessageBuffer &messageBuffer) override;

        const char* GetTransportName() const override
        {
            return "Direct Connection";
        }

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
        Result UpdateClientStatus(ClientId clientId, StatusFlags flags) override;
#endif

        DD_STATIC_CONST bool RequiresKeepAlive()
        {
            return false;
        }

        DD_STATIC_CONST bool RequiresClientRegistration()
        {
            return true;
        }
    private:
        ClientId m_clientId;
        std::shared_ptr<HostListenerTransport> m_pHostTransport;
    };
} // DevDriver
