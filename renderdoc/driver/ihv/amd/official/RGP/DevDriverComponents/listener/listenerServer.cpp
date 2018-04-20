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
* @file listenerServer.cpp
* @brief Class definition for ListenerServer.
***********************************************************************************************************************
*/

#include "listenerServer.h"
#include "../src/messageChannel.h"
#include "protocolServer.h"
#include "protocols/loggingServer.h"
#include "protocols/etwServer.h"

namespace DevDriver
{
    ListenerServer::ListenerServer(const ListenerServerCreateInfo& createInfo, IMsgChannel *pMsgChannel)
        : m_createInfo(createInfo)
        , m_pMsgChannel(pMsgChannel)
    {
    }

    ListenerServer::~ListenerServer()
    {
        Destroy();
    }

    Result ListenerServer::Initialize()
    {
        Result result = Result::Error;

        if (m_pMsgChannel != nullptr)
        {
            result = m_pMsgChannel->Register(kInfiniteTimeout);

            if (result == Result::Success)
            {
                ClientMetadata filter = {};

                m_pMsgChannel->Send(kBroadcastClientId,
                                    Protocol::System,
                                    static_cast<MessageCode>(SystemProtocol::SystemMessage::ClientConnected),
                                    filter,
                                    0,
                                    nullptr);

                result = InitializeProtocols();

                if (result != Result::Success)
                {
                    // Unregister the message channel since we failed to initialize the protocols.
                    m_pMsgChannel->Unregister();
                }
            }

            if (result != Result::Success)
            {
                // We failed to initialize so we need to destroy the message channel.
                m_pMsgChannel = nullptr;
            }
        }
        return result;
    }

    void ListenerServer::Destroy()
    {
        if (m_pMsgChannel != nullptr)
        {
            Result result = m_pMsgChannel->Unregister();
            DD_ASSERT(result == Result::Success);
            DD_UNUSED(result);
            DestroyProtocols();

            // We don't own the message channel so we set this to nullptr
            m_pMsgChannel = nullptr;
        }
    }

    IMsgChannel* ListenerServer::GetMessageChannel()
    {
        return m_pMsgChannel;
    }

    LoggingProtocol::LoggingServer* ListenerServer::GetLoggingServer()
    {
        return GetServer<Protocol::Logging>();
    }

    ETWProtocol::ETWServer * ListenerServer::GetEtwServer()
    {
        return GetServer<Protocol::ETW>();
    }

    Result ListenerServer::InitializeProtocols()
    {
        Result result = Result::Success;

        if (m_createInfo.enabledProtocols.logging)
        {
            result = RegisterProtocol<Protocol::Logging>();
        }

#if defined(_WIN32)
        if (m_createInfo.enabledProtocols.etw)
        {
            result = RegisterProtocol<Protocol::ETW>();
        }
#endif

        return result;
    }

    void ListenerServer::DestroyProtocols()
    {
        if (m_createInfo.enabledProtocols.logging)
        {
            UnregisterProtocol(Protocol::Logging);
        }

        if (m_createInfo.enabledProtocols.etw)
        {
            UnregisterProtocol(Protocol::ETW);
        }
    }

    Result ListenerServer::RegisterProtocol(Protocol protocol)
    {
        Result result = Result::Error;
        if (m_pMsgChannel->GetProtocolServer(protocol) == nullptr)
        {
            switch (protocol)
            {
            case Protocol::Logging:
            {
                result = RegisterProtocol<Protocol::Logging>();
                break;
            }
            default:
            {
                // This shouldn't happen. Ever.
                DD_UNREACHABLE();
                break;
            }
            }
        }
        return result;
    }

    void ListenerServer::UnregisterProtocol(Protocol protocol)
    {
        IProtocolServer *pProtocolServer = m_pMsgChannel->GetProtocolServer(protocol);
        if (pProtocolServer != nullptr)
        {
            const Result result = m_pMsgChannel->UnregisterProtocolServer(pProtocolServer);
            DD_ASSERT(result == Result::Success);
            DD_UNUSED(result);
            delete pProtocolServer;
        }
    }

} // DevDriver
