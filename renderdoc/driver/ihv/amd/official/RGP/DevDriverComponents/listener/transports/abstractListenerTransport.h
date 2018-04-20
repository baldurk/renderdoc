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
* @file  abstractListenerTransport.h
* @brief Class declaration for IListenerTransport
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"

namespace DevDriver
{
    const uint32 kConnectionInfoDataSize = 128;
    typedef uint32 TransportHandle;

    struct ConnectionInfo
    {
        char data[kConnectionInfoDataSize];
        size_t size;
        TransportHandle handle;
    };

    class RouterCore;

    class IListenerTransport
    {
    public:
        virtual ~IListenerTransport() {}

        virtual Result Enable(RouterCore* pRouter, TransportHandle handle) = 0;
        virtual Result ReceiveMessage(ConnectionInfo &connectionInfo, MessageBuffer &message, uint32 timeoutInMs) = 0;
        virtual Result TransmitMessage(const ConnectionInfo &connectionInfo, const MessageBuffer &message) = 0;
        virtual Result TransmitBroadcastMessage(const MessageBuffer &message) = 0;
        virtual Result Disable() = 0;

        virtual TransportHandle GetHandle() = 0;
        virtual bool ForwardingConnection() = 0;
        virtual const char* GetTransportName() = 0;

    protected:
        IListenerTransport() {}
    };
} // DevDriver
