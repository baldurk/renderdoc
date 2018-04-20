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
    class SocketMsgTransport : public IMsgTransport
    {
    public:
        explicit SocketMsgTransport(const HostInfo& hostInfo);
        ~SocketMsgTransport();

        Result Connect(ClientId* pClientId, uint32 timeoutInMs) override;
        Result Disconnect() override;

        Result ReadMessage(MessageBuffer& messageBuffer, uint32 timeoutInMs) override;
        Result WriteMessage(const MessageBuffer& messageBuffer) override;

        const char* GetTransportName() const override
        {
            const char *pName = "Unknown";
            switch (m_socketType)
            {
                case SocketType::Tcp:
                    pName = "TCP Socket";
                    break;
                case SocketType::Udp:
                    pName = "UDP Socket";
                    break;
                case SocketType::Local:
#if !defined(DD_WINDOWS)
                    pName = "Unix Domain Socket";
                    break;
#endif
                default:
                    break;
            }

            return pName;
        }

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
        Result UpdateClientStatus(ClientId clientId, StatusFlags flags) override;
        static Result QueryStatus(const HostInfo& hostInfo, uint32 timeoutInMs, StatusFlags *pFlags);
#endif

        static Result TestConnection(const HostInfo& connectionInfo, uint32 timeoutInMs);

        DD_STATIC_CONST bool RequiresKeepAlive()
        {
            return true;
        }

        DD_STATIC_CONST bool RequiresClientRegistration()
        {
            return true;
        }

    private:
        Socket              m_clientSocket;
        bool                m_connected;
        const HostInfo      m_hostInfo;
        const SocketType    m_socketType;
    };

} // DevDriver
