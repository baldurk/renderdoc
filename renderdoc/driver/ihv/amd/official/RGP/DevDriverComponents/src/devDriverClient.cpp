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
* @file devDriverClient.cpp
* @brief Class definition for DevDriverClient.
***********************************************************************************************************************
*/

#include "devDriverClient.h"
#include "messageChannel.h"
#include "protocolClient.h"
#include "socketMsgTransport.h"
#include "protocols/loggingClient.h"
#include "protocols/settingsClient.h"
#include "protocols/driverControlClient.h"
#include "protocols/rgpClient.h"
#include "protocols/etwClient.h"

// The local transport implementation is only available on Windows.
#if defined(_WIN32)
#include "win/ddWinPipeMsgTransport.h"
#endif

namespace DevDriver
{
    DevDriverClient::DevDriverClient(const AllocCb&          allocCb,
                                     const ClientCreateInfo& createInfo)
        : m_pMsgChannel(nullptr)
        , m_pClients(allocCb)
        , m_pUnusedClients(allocCb)
        , m_allocCb(allocCb)
        , m_createInfo(createInfo)
    {}

#if !DD_VERSION_SUPPORTS(GPUOPEN_CREATE_INFO_CLEANUP_VERSION)
    DevDriverClient::DevDriverClient(const DevDriverClientCreateInfo& createInfo)
        : m_pMsgChannel(nullptr)
        , m_pClients(createInfo.transportCreateInfo.allocCb)
        , m_pUnusedClients(createInfo.transportCreateInfo.allocCb)
        , m_allocCb(createInfo.transportCreateInfo.allocCb)
        , m_createInfo()
    {
        m_createInfo.initialFlags = createInfo.transportCreateInfo.initialFlags;
        m_createInfo.componentType = createInfo.transportCreateInfo.componentType;
        m_createInfo.createUpdateThread = createInfo.transportCreateInfo.createUpdateThread;
        Platform::Strncpy(&m_createInfo.clientDescription[0],
                          &createInfo.transportCreateInfo.clientDescription[0],
                          sizeof(m_createInfo.clientDescription));
        switch (createInfo.transportCreateInfo.type)
        {
        case TransportType::Local:
        {
            m_createInfo.connectionInfo = kDefaultNamedPipe;
            break;
        }
        case TransportType::Remote:
        {
            m_createInfo.connectionInfo = createInfo.transportCreateInfo.hostInfo;
            // Explicitly overwrite connectionInfo.type since it didn't exist originally.
            m_createInfo.connectionInfo.type = createInfo.transportCreateInfo.type;
            break;
        }
        default:
            DD_UNREACHABLE();
            break;
        }
    }
#endif

    DevDriverClient::~DevDriverClient()
    {
        Destroy();
    }

    Result DevDriverClient::Initialize()
    {
        Result result = Result::Error;
#if defined(DD_WINDOWS)
        if (m_createInfo.connectionInfo.type == TransportType::Local)
        {
            using MsgChannelPipe = MessageChannel<WinPipeMsgTransport>;
            m_pMsgChannel = DD_NEW(MsgChannelPipe, m_allocCb)(m_allocCb,
                                                              m_createInfo,
                                                              m_createInfo.connectionInfo);
        }
        else if (m_createInfo.connectionInfo.type == TransportType::Remote)
        {
            using MsgChannelSocket = MessageChannel<SocketMsgTransport>;
            m_pMsgChannel = DD_NEW(MsgChannelSocket, m_allocCb)(m_allocCb,
                                                                m_createInfo,
                                                                m_createInfo.connectionInfo);
        }
#else
        if ((m_createInfo.connectionInfo.type == TransportType::Remote) |
            (m_createInfo.connectionInfo.type == TransportType::Local))
        {
            using MsgChannelSocket = MessageChannel<SocketMsgTransport>;
            m_pMsgChannel = DD_NEW(MsgChannelSocket, m_allocCb)(m_allocCb,
                                                                m_createInfo,
                                                                m_createInfo.connectionInfo);
        }
#endif
        else
        {
            // Invalid transport type
            DD_ALERT_REASON("Invalid transport type specified");
        }

        if (m_pMsgChannel != nullptr)
        {
            result = m_pMsgChannel->Register(kRegistrationTimeoutInMs);
            if (result != Result::Success)
            {
                // We failed to initialize so we need to destroy the message channel.
                DD_DELETE(m_pMsgChannel, m_allocCb);
                m_pMsgChannel = nullptr;
            }
        }

        return result;
    }

    void DevDriverClient::Destroy()
    {
        if (m_pMsgChannel != nullptr)
        {
            Result result = m_pMsgChannel->Unregister();
            DD_ASSERT(result == Result::Success);
            DD_UNUSED(result);

            for (auto &pClient : m_pClients)
            {
                DD_DELETE(pClient, m_allocCb);
            }
            m_pClients.Clear();

            for (auto &pClient : m_pUnusedClients)
            {
                DD_DELETE(pClient, m_allocCb);
            }
            m_pUnusedClients.Clear();

            DD_DELETE(m_pMsgChannel, m_allocCb);
            m_pMsgChannel = nullptr;
        }
    }

    bool DevDriverClient::IsConnected() const
    {
        if (m_pMsgChannel)
            return m_pMsgChannel->IsConnected();
        return false;
    }

    IMsgChannel* DevDriverClient::GetMessageChannel() const
    {
        return m_pMsgChannel;
    }
} // DevDriver
