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

#include "hostMsgTransport.h"
#include "transports/hostTransport.h"
#include "ddPlatform.h"

#define BUFFER_LENGTH 16
#define RECV_BUFSIZE (sizeof(MessageBuffer) * 8)
#define SEND_BUFSIZE (sizeof(MessageBuffer) * 8)

namespace DevDriver
{
    HostMsgTransport::HostMsgTransport(const std::shared_ptr<HostListenerTransport> &pHostTransport,
                                       ClientId hostClientId)
        : m_clientId(hostClientId)
        , m_pHostTransport(pHostTransport)
    {
    }

    HostMsgTransport::~HostMsgTransport()
    {
    }

    Result HostMsgTransport::Connect(ClientId *pClientId, uint32 timeoutInMs)
    {
        DD_UNUSED(timeoutInMs);

        Result result = Result::Error;
        if (m_clientId != kBroadcastClientId)
        {
            // we don't have to register the client, as client registration for the is done HostMsgTransport elsewhere
            result = Result::Success;
            *pClientId = m_clientId;
        }
        return result;
    }

    Result HostMsgTransport::Disconnect()
    {
        Result result = Result::Error;
        if (m_clientId != kBroadcastClientId)
        {
            // we don't have to unregister the client, as client registration for the HostMsgTransport elsewhere
            result = Result::Success;
            m_clientId = kBroadcastClientId;
        }
        return result;
    }

    Result HostMsgTransport::ReadMessage(MessageBuffer &messageBuffer, uint32 timeoutInMs)
    {
        return m_pHostTransport->HostReadMessage(messageBuffer, timeoutInMs);
    }

    Result HostMsgTransport::WriteMessage(const MessageBuffer &messageBuffer)
    {
        return m_pHostTransport->HostWriteMessage(messageBuffer);
    }

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
    Result HostMsgTransport::UpdateClientStatus(ClientId clientId, StatusFlags flags)
    {
        DD_UNUSED(clientId);
        DD_UNUSED(flags);
        return Result::Error;
    }
#endif
} // DevDriver
