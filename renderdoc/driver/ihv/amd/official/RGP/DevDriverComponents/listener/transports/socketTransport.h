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
* @file  socketTransport.h
* @brief Class declaration for SocketListenerTransport
***********************************************************************************************************************
*/

#pragma once

#include "abstractListenerTransport.h"
#include "../src/ddSocket.h"
#include "../transportThread.h"

namespace DevDriver
{
    static_assert(sizeof(sockaddr) <= kConnectionInfoDataSize, "ConnectionInfo struct not large enough to hold address information");

    class RouterCore;

    class SocketListenerTransport : public IListenerTransport
    {
    public:
        SocketListenerTransport(TransportType type, const char *pAddress, uint32 port);
        ~SocketListenerTransport() override;

        Result ReceiveMessage(ConnectionInfo &connectionInfo, MessageBuffer &message, uint32 timeoutInMs) override;
        Result TransmitMessage(const ConnectionInfo &connectionInfo, const MessageBuffer &message) override;
        Result TransmitBroadcastMessage(const MessageBuffer &message) override;

        Result Enable(RouterCore *pRouter, TransportHandle handle) override;
        Result Disable() override;

        TransportHandle GetHandle() override { return m_transportHandle; };
        bool ForwardingConnection() override { return false; };
        const char* GetTransportName() override { return m_hostDescription; };

    protected:
        char        m_hostAddress[kMaxStringLength];
        char        m_hostDescription[kMaxStringLength];
        Socket      m_clientSocket;
        SocketType  m_socketType;
        uint32      m_port;
        TransportHandle m_transportHandle;
        bool        m_listening;
        TransportThread m_transportThread;
    };
} // DevDriver
