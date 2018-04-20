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
* @file  socketMsgTransport.h
* @brief Class declaration for SocketMsgTransport
***********************************************************************************************************************
*/

#pragma once

#include "msgTransport.h"
#include "ddSocket.h"

namespace DevDriver
{
    class NetworkMsgTransport : IMsgTransport
    {
    public:
        NetworkMsgTransport(const TransportCreateInfo& createInfo);
        ~NetworkMsgTransport();

        Result Connect(ClientId* pClientId, uint32 timeoutInMs) override;
        Result Disconnect() override;

        Result ReadMessage(MessageBuffer &messageBuffer, uint32 timeoutInMs) override;
        Result WriteMessage(const MessageBuffer &messageBuffer) override;

#if GPUOPEN_CLIENT_INTERFACE_MAJOR_VERSION < GPUOPEN_CLIENT_REGISTRATION_VERSION
        Result RegisterExternalClient(Component componentType, ClientId* pExternalClientId) override
        {
            return RegisterExternalClient(componentType, 0, pExternalClientId);
        };
#endif
        Result RegisterExternalClient(Component componentType, ClientFlags flags, ClientId* pExternalClientId) override;
        Result UnregisterExternalClient(ClientId externalClientId) override;
        Result UpdateClientStatus(ClientId clientId, ClientFlags flags) override;

        static Result QueryStatus(TransportType type, ClientFlags *pFlags, uint32 timeoutInMs);

    private:
        Component       m_componentType;
        ClientFlags     m_initialClientFlags;
        ClientId        m_clientId;
    };

} // DevDriver
