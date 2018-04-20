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
* @file  listenerCore.cpp
* @brief Class definition for RouterCore
***********************************************************************************************************************
*/

#include "transportThread.h"
#include "routerCore.h"
#include "../inc/ddPlatform.h"

namespace DevDriver
{
    void TransportThread::ReceiveThreadFunc(RouterCore *pRouter, IListenerTransport *pTransport)
    {
        if ((pRouter != nullptr) & (pTransport != nullptr))
        {
            RoutingCache cache(pRouter);
            MessageContext recvMsgContext = {};
            std::deque<MessageContext> recvQueue;
            std::deque<MessageContext> retryQueue;

            while (m_active)
            {
                size_t firstNewMessageIndex = recvQueue.size();
                // Check for new local messages.
                Result readResult = pTransport->ReceiveMessage(recvMsgContext.connectionInfo, recvMsgContext.message, kReceiveDelayInMs);
                if (readResult == Result::Success)
                {
                    do
                    {
                        recvQueue.emplace_back(recvMsgContext);
                        readResult = pTransport->ReceiveMessage(recvMsgContext.connectionInfo, recvMsgContext.message, kNoWait);
                    } while (readResult == Result::Success);
                }

                size_t messageNumber = 0;
                for (const auto &message : recvQueue)
                {
                    messageNumber++;
                    // only requeue messages if it's the first time we've tried to send them
                    if ((cache.RouteMessage(message) == Result::NotReady) & (messageNumber > firstNewMessageIndex))
                    {
                        retryQueue.emplace_back(message);
                    }
                }
                recvQueue.clear();
                recvQueue.swap(retryQueue);
            }
        }
    }

    void TransportThread::Start(RouterCore *pRouter, IListenerTransport *pTransport)
    {
        DD_ASSERT(m_active == false);
        m_active = true;
        m_thread = std::thread(&DevDriver::TransportThread::ReceiveThreadFunc, this, pRouter, pTransport);
        DD_ASSERT(m_thread.joinable());
    }

    void TransportThread::Stop()
    {
        if (m_active)
        {
            m_active = false;
            if (m_thread.joinable())
                m_thread.join();
        }
    }

    TransportThread::TransportThread() :
        m_active(false)
    {
    }

    TransportThread::~TransportThread()
    {
        if (m_active)
            Stop();
    }
} // DevDriver
