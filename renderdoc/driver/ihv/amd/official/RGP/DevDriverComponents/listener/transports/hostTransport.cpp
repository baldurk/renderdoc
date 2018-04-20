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

#include "hostTransport.h"
#include "ddPlatform.h"
#include "../routerCore.h"

#define BUFFER_LENGTH 16
#define RECV_BUFSIZE (sizeof(MessageBuffer) * 8)
#define SEND_BUFSIZE (sizeof(MessageBuffer) * 8)

namespace DevDriver
{

    HostListenerTransport::HostListenerTransport(const ListenerCreateInfo &createInfo)
        : m_transportHandle(0)
    {
        DD_UNUSED(createInfo);
    }

    HostListenerTransport::~HostListenerTransport()
    {
        Disable();
    }

    Result HostListenerTransport::ReceiveMessage(ConnectionInfo & connectionInfo, MessageBuffer & message, uint32 timeoutInMs)
    {
        Result result = Result::NotReady;
        const auto waitTime = std::chrono::milliseconds((int64)timeoutInMs);
        std::unique_lock<std::mutex> lock(m_inboundMessages.mutex);
        if (m_inboundMessages.signal.wait_for(lock, waitTime, [&] { return !m_inboundMessages.queue.empty(); }))
        {
            DD_ASSERT(m_transportHandle != 0);
            message = m_inboundMessages.queue.front();
            m_inboundMessages.queue.pop_front();
            result = Result::Success;
            connectionInfo.handle = m_transportHandle;
            connectionInfo.size = 0;
        }
        lock.unlock();
        return result;
    }
    Result HostListenerTransport::TransmitMessage(const ConnectionInfo & connectionInfo, const MessageBuffer & message)
    {
        DD_ASSERT(connectionInfo.handle == m_transportHandle);
        DD_UNUSED(connectionInfo);

        std::lock_guard<std::mutex> lock(m_outboundMessages.mutex);
        m_outboundMessages.queue.emplace_back(message);
        m_outboundMessages.signal.notify_one();
        return Result::Success;
    }

    Result HostListenerTransport::TransmitBroadcastMessage(const MessageBuffer & message)
    {
        std::lock_guard<std::mutex> lock(m_outboundMessages.mutex);
        m_outboundMessages.queue.emplace_back(message);
        m_outboundMessages.signal.notify_one();
        return Result::Success;
    }

    Result HostListenerTransport::Enable(RouterCore *pRouter, TransportHandle handle)
    {
        Result result = Result::Error;
        if (m_transportHandle == 0)
        {
            m_transportHandle = handle;
            m_transportThread.Start(pRouter, this);
            result = Result::Success;
        }
        return result;
    }

    Result HostListenerTransport::Disable()
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

    Result HostListenerTransport::HostReadMessage(MessageBuffer & messageBuffer, uint32 timeoutInMs)
    {
        const auto waitTime = std::chrono::milliseconds((int64)timeoutInMs);
        Result result = Result::NotReady;
        std::unique_lock<std::mutex> lock(m_outboundMessages.mutex);
        if (m_outboundMessages.signal.wait_for(lock, waitTime, [&] { return !m_outboundMessages.queue.empty(); }))
        {
            messageBuffer = m_outboundMessages.queue.front();
            m_outboundMessages.queue.pop_front();
            result = Result::Success;
        }
        lock.unlock();
        return result;
    }

    Result HostListenerTransport::HostWriteMessage(const MessageBuffer & messageBuffer)
    {
        DD_ASSERT(m_transportHandle != 0);
        std::lock_guard<std::mutex> lock(m_inboundMessages.mutex);
        m_inboundMessages.queue.emplace_back(messageBuffer);
        m_inboundMessages.signal.notify_one();
        return Result::Success;
    }
} // DevDriver
