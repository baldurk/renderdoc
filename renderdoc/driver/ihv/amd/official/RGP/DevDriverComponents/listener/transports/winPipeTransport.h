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
* @file  udpListenerTransport.h
* @brief Class declaration for BaseMsgChannel
***********************************************************************************************************************
*/

#pragma once

#include "abstractListenerTransport.h"
#include <deque>
#include <unordered_set>
#include <condition_variable>
#include <mutex>
#include <unordered_map>
#include <thread>
#include "../transportThread.h"
#include "msgTransport.h"

namespace DevDriver
{
    class RouterCore;

    struct PipeInfo
    {
        std::mutex      lock;
        std::thread     thread;
        volatile bool   active;
        Handle          pipeHandle;
        Handle          writeEvent;
        Handle          readEvent;
        bool ioPending;
    };

    class PipeListenerTransport : public IListenerTransport
    {
    public:
        PipeListenerTransport(const char* pPipeName);
        ~PipeListenerTransport() override;

        Result ReceiveMessage(ConnectionInfo &connectionInfo, MessageBuffer &message, uint32 timeoutInMs) override;
        Result TransmitMessage(const ConnectionInfo &connectionInfo, const MessageBuffer &message) override;
        Result TransmitBroadcastMessage(const MessageBuffer &message) override;

        Result Enable(RouterCore *pRouter, TransportHandle handle) override;
        Result Disable() override;

        TransportHandle GetHandle() override { return m_transportHandle; };
        bool ForwardingConnection() override { return false; };
        const char* GetTransportName() override { return "Local Pipe"; };
    protected:
        char            m_pipeName[kMaxStringLength];
        TransportHandle m_transportHandle;

        bool m_listening;

        struct
        {
            std::unordered_map<Handle, PipeInfo*> threadMap;
            std::unordered_set<PipeInfo*> deleteSet;
            std::mutex mutex;
        } m_threadPool;

        PipeInfo m_listenThread;
        RouterCore* m_pRouter;

        void ListeningThreadFunc(PipeInfo *threadInfo);
        void ReceivingThreadFunc(RouterCore *pRouter, PipeInfo *threadInfo);
        DD_STATIC_CONST uint32 kWaitTimeoutInMs = 100;
    };
} // DevDriver
