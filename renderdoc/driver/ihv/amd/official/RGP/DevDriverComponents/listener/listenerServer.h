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
* @file  listenerServer.h
* @brief Class declaration for ListenerServer.
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "msgChannel.h"
#include "msgTransport.h"
#include "protocols/systemProtocols.h"
#include "protocols/typemap.h"

namespace DevDriver
{
    class IProtocolServer;

    struct ListenerServerCreateInfo
    {
        ProtocolFlags       enabledProtocols;
    };

    class ListenerServer
    {
    public:
        ListenerServer(const ListenerServerCreateInfo& createInfo, IMsgChannel *pMsgChannel);
        ~ListenerServer();

        Result Initialize();
        void Destroy();

        IMsgChannel* GetMessageChannel();

        LoggingProtocol::LoggingServer* GetLoggingServer();

        ETWProtocol::ETWServer* GetEtwServer();

    private:
        Result InitializeProtocols();
        void DestroyProtocols();
        Result RegisterProtocol(Protocol protocol);
        void UnregisterProtocol(Protocol protocol);

        ListenerServerCreateInfo    m_createInfo;
        IMsgChannel*                m_pMsgChannel;

        template <Protocol protocol, class ...Args>
        Result RegisterProtocol(Args... args)
        {
            Result result = Result::Error;
            using T = ProtocolServerType<protocol>;
            T* pProtocolServer = nullptr;
            if (m_pMsgChannel->GetProtocolServer(protocol) == nullptr)
            {
                pProtocolServer = new T(m_pMsgChannel, args...);
                result = m_pMsgChannel->RegisterProtocolServer(pProtocolServer);
            }
            return result;
        };

        template <Protocol protocol>
        ProtocolServerType<protocol>* GetServer()
        {
            using T = ProtocolServerType<protocol>;
            return static_cast<T*>(m_pMsgChannel->GetProtocolServer(protocol));
        }

    };

} // DevDriver
