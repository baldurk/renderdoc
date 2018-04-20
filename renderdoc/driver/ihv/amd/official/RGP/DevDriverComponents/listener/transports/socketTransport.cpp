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

#include "socketTransport.h"
#include "ddPlatform.h"
#include <cstring>
#include <cstdio>
#include <cassert>
#include "../routerCore.h"

namespace DevDriver
{
    // Take a TransportType and find the associated SocketType for the current platform
    inline static SocketType TransportToSocketType(TransportType type)
    {
        SocketType result = SocketType::Unknown;
        switch (type)
        {
#ifndef _WIN32
        case TransportType::Local:
            result = SocketType::Local;
            break;
#endif
        case TransportType::Remote:
            result = SocketType::Udp;
            break;
        default:
            DD_ALERT_REASON("Invalid transport type specified");
            break;
        }
        return result;
    }

    SocketListenerTransport::SocketListenerTransport(TransportType type, const char *pAddress, uint32 port) :
        m_socketType(TransportToSocketType(type)),
        m_port(port),
        m_listening(false)
    {
        if (pAddress != nullptr)
        {
            Platform::Strncpy(m_hostAddress, pAddress, sizeof(m_hostAddress));
        }
        else
        {
            Platform::Strncpy(m_hostAddress, "0.0.0.0", sizeof(m_hostAddress));
        }

        if (type == TransportType::Local)
        {
            Platform::Snprintf(m_hostDescription, sizeof(m_hostDescription), "%s", m_hostAddress);
        }
        else if (type == TransportType::Remote)
        {
            Platform::Snprintf(m_hostDescription, sizeof(m_hostDescription), "%s:%u", m_hostAddress, m_port);
        }
        else
        {
            // Invalid type specified
            Platform::Snprintf(m_hostDescription, sizeof(m_hostDescription), "Unknown");
            DD_ALERT_REASON("Unknown socket type requested");
        }
    }

    SocketListenerTransport::~SocketListenerTransport()
    {
        if (m_listening)
            Disable();
    }

    Result SocketListenerTransport::ReceiveMessage(ConnectionInfo& connectionInfo, MessageBuffer& message, uint32 timeoutInMs)
    {
        bool canRead = false;
        bool exceptState = false;
        connectionInfo.handle = m_transportHandle;
        Result result = m_clientSocket.Select(&canRead, nullptr, &exceptState, timeoutInMs);
        if (result == Result::Success)
        {
            if (exceptState)
            {
                result = Result::Error;
            }
            else if (canRead)
            {
                connectionInfo.size = sizeof(connectionInfo.data);
                result = m_clientSocket.ReceiveFrom(reinterpret_cast<void *>(&connectionInfo.data[0]),
                    &connectionInfo.size,
                    reinterpret_cast<uint8*>(&message),
                    sizeof(MessageBuffer));
            }
            else
            {
                result = Result::NotReady;
            }
        }
        return result;
    }

    Result SocketListenerTransport::TransmitMessage(const ConnectionInfo& connectionInfo, const MessageBuffer& message)
    {
        DD_ASSERT(connectionInfo.handle == m_transportHandle);
        Result result = m_clientSocket.SendTo(reinterpret_cast<const void *>(&connectionInfo.data[0]),
            connectionInfo.size,
            reinterpret_cast<const uint8*>(&message),
            sizeof(MessageHeader) + message.header.payloadSize);
        return result;
    }

    Result SocketListenerTransport::TransmitBroadcastMessage(const MessageBuffer& message)
    {
        DD_UNUSED(message);

        return Result::Error;
    }

    Result SocketListenerTransport::Enable(RouterCore *pRouter, TransportHandle handle)
    {
        Result result = Result::Error;

        if (m_clientSocket.Init(true, m_socketType) == DevDriver::Result::Success)
        {
            char *address = nullptr;
            if (m_hostAddress[0] != 0)
                address = m_hostAddress;
            if (m_clientSocket.Bind(address, m_port) == Result::Success)
            {
                result = Result::Success;
                m_transportHandle = handle;
                m_transportThread.Start(pRouter, this);
            }
        }
        return result;
    }

    Result SocketListenerTransport::Disable()
    {
        Result result = Result::Error;
        if (m_transportHandle != 0)
        {
            m_transportHandle = 0;
            m_transportThread.Stop();
            result = Result::Success;
        }
        return result;
    }
} // DevDriver
