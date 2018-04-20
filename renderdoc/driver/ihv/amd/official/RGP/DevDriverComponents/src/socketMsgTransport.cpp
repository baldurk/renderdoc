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

#include "socketMsgTransport.h"
#include "protocols/systemProtocols.h"
#include "ddPlatform.h"
#include <cstring>

using namespace DevDriver::ClientManagementProtocol;

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

    SocketMsgTransport::SocketMsgTransport(const HostInfo& hostInfo) :
        m_connected(false),
        m_hostInfo(hostInfo),
        m_socketType(TransportToSocketType(hostInfo.type))
    {
        if ((m_socketType != SocketType::Udp) && (m_socketType != SocketType::Local))
        {
            DD_ASSERT_REASON("Unsupported socket type provided");
        }
    }

    SocketMsgTransport::~SocketMsgTransport()
    {
        Disconnect();
    }

    Result SocketMsgTransport::Connect(ClientId* pClientId, uint32 timeoutInMs)
    {
        DD_UNUSED(timeoutInMs);
        DD_UNUSED(pClientId);

        // Attempt to connect to the remote host.
        Result result = Result::Error;

        if (!m_connected)
        {
            result = m_clientSocket.Init(true, m_socketType);

            if (result == Result::Success)
            {
                result = m_clientSocket.Bind(nullptr, 0);
            }

            if (result == Result::Success)
            {
                result = m_clientSocket.Connect(m_hostInfo.hostname, m_hostInfo.port);
            }
            m_connected = (result == Result::Success);
        }
        return result;
    }

    Result SocketMsgTransport::Disconnect()
    {
        Result result = Result::Error;
        if (m_connected)
        {
            m_connected = false;
            result = m_clientSocket.Close();
        }
        return result;
    }

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
    Result SocketMsgTransport::UpdateClientStatus(ClientId clientId, StatusFlags flags)
    {
        // not implemented
        DD_UNUSED(clientId);
        DD_UNUSED(flags);
        return Result::Unavailable;
    }
#endif

    Result SocketMsgTransport::ReadMessage(MessageBuffer &messageBuffer, uint32 timeoutInMs)
    {
        bool canRead = m_connected;
        bool exceptState = false;
        Result result = Result::Success;

        if (canRead & (timeoutInMs > 0))
        {
            result = m_clientSocket.Select(&canRead, nullptr, &exceptState, timeoutInMs);
        }

        if (result == Result::Success)
        {
            if (canRead)
            {
                size_t bytesReceived;
                result = m_clientSocket.Receive(reinterpret_cast<uint8*>(&messageBuffer), sizeof(MessageBuffer), &bytesReceived);
            }
            else if (exceptState)
            {
                result = Result::Error;
            }
            else
            {
                result = Result::NotReady;
            }
        }
        return result;
    }

    Result SocketMsgTransport::WriteMessage(const MessageBuffer &messageBuffer)
    {
        DD_ASSERT(m_connected);
        const size_t totalMsgSize = (sizeof(MessageHeader) + messageBuffer.header.payloadSize);
        size_t bytesSent = 0;
        return m_clientSocket.Send(reinterpret_cast<const uint8*>(&messageBuffer), totalMsgSize, &bytesSent);
    }

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
    Result SocketMsgTransport::QueryStatus(const HostInfo& hostInfo,
                                           uint32          timeoutInMs,
                                           StatusFlags*    pFlags)
    {
        Result result = Result::Error;
        Socket clientSocket;
        const SocketType sType = TransportToSocketType(hostInfo.type);

        if (sType != SocketType::Unknown)
        {
            result = clientSocket.Init(true, sType);

            if (result == Result::Success)
            {
                result = clientSocket.Bind(nullptr, 0);

                if (result == Result::Success)
                {
                    result = clientSocket.Connect(hostInfo.hostname, hostInfo.port);
                }

                if (result == Result::Success)
                {
                    MessageBuffer message = kOutOfBandMessage;
                    message.header.messageId = static_cast<MessageCode>(ManagementMessage::QueryStatus);

                    size_t bytesWritten = 0;
                    result = clientSocket.Send(
                        reinterpret_cast<const uint8 *>(&message),
                        sizeof(message.header),
                        &bytesWritten);

                    if (result == Result::Success)
                    {
                        bool canRead = false;
                        bool exceptState = false;
                        result = clientSocket.Select(&canRead, nullptr, &exceptState, timeoutInMs);
                        if ((result == Result::Success) & (canRead) & (!exceptState))
                        {
                            MessageBuffer responseMessage = {};
                            size_t bytesReceived;
                            result = clientSocket.Receive(reinterpret_cast<uint8 *>(&responseMessage), sizeof(MessageBuffer), &bytesReceived);
                            if (result == Result::Success)
                            {
                                result = Result::VersionMismatch;
                                if (IsOutOfBandMessage(responseMessage) &
                                    IsValidOutOfBandMessage(responseMessage) &
                                    (responseMessage.header.messageId == static_cast<MessageCode>(ManagementMessage::QueryStatusResponse)))
                                {
                                    const QueryStatusResponsePayload *pResponse =
                                        reinterpret_cast<QueryStatusResponsePayload *>(&responseMessage.payload[0]);
                                    result = pResponse->result;
                                    *pFlags = pResponse->flags;
                                }
                            }
                        }
                    }
                }
                clientSocket.Close();
            }
        }
        return result;
    }
#endif

    // ================================================================================================================
    // Tests to see if the client can connect to RDS through this transport
    Result SocketMsgTransport::TestConnection(const HostInfo& hostInfo, uint32 timeoutInMs)
    {
        Result result = Result::Error;
        Socket clientSocket;
        const SocketType sType = TransportToSocketType(hostInfo.type);

        if (sType != SocketType::Unknown)
        {
            result = clientSocket.Init(true, sType);

            if (result == Result::Success)
            {
                // Bind with no host info will bind our local side of the socket to a random port that is capable of
                // receiving from any address.
                result = clientSocket.Bind(nullptr, 0);

                // If we were able to bind to a socket we the connect to the remote host/port specified
                if (result == Result::Success)
                {
                    result = clientSocket.Connect(hostInfo.hostname, hostInfo.port);
                }

                // If we made it this far we need to actually make sure we can actually communicate with the remote host
                if (result == Result::Success)
                {
                    // In order to test connectivity we are going to manually send a KeepAlive message. This message
                    // is discarded by both clients and RDS, making it safe to use for this purpose
                    MessageBuffer message = kOutOfBandMessage;
                    message.header.messageId = static_cast<MessageCode>(ManagementMessage::KeepAlive);

                    // Transmit the KeepAlive packet
                    size_t bytesWritten = 0;
                    result = clientSocket.Send(
                        reinterpret_cast<const uint8 *>(&message),
                        sizeof(message.header),
                        &bytesWritten);

                    if (result == Result::Success)
                    {
                        // Wait until a response is waiting
                        bool canRead = false;
                        bool exceptState = false;
                        result = clientSocket.Select(&canRead, nullptr, &exceptState, timeoutInMs);
                        if ((result == Result::Success) & (canRead) & (!exceptState))
                        {
                            // read the response
                            MessageBuffer responseMessage = {};
                            size_t bytesReceived;
                            result = clientSocket.Receive(reinterpret_cast<uint8 *>(&responseMessage), sizeof(MessageBuffer), &bytesReceived);

                            // Check to make sure we got the response + that the response is the expected size
                            // KeepAlive is defined as having no additional payload, so it will only ever be the size of a header
                            if ((result == Result::Success) & (bytesReceived == sizeof(responseMessage.header)))
                            {
                                // Since we received a response, we know there is a server. An invalid packet here means that either
                                // the remote server didn't understand the request or that there was a logical bug on the server.
                                // In either case we treat this as a version mismatch since we can't tell the difference.
                                result = Result::VersionMismatch;

                                // check packet validity and set success if true
                                if (IsOutOfBandMessage(responseMessage) &
                                    IsValidOutOfBandMessage(responseMessage) &
                                    (responseMessage.header.messageId == static_cast<MessageCode>(ManagementMessage::KeepAlive)))
                                {
                                    result = Result::Success;
                                }
                            }
                        }
                    }
                }
                clientSocket.Close();
            }
        }
        return result;
    }
} // DevDriver
