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
* @file  listenerCore.h
* @brief Class declaration for RouterCore
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"

#include "transports/abstractListenerTransport.h"

#include <thread>

namespace DevDriver
{
    class TransportThread
    {
    public:
        TransportThread();
        ~TransportThread();

        void Start(class RouterCore *pListener, IListenerTransport *pTransport);
        void Stop();
    private:
        void ReceiveThreadFunc(RouterCore *pRouter, IListenerTransport *pTransport);
        DD_STATIC_CONST uint32 kReceiveDelayInMs = 25;
        std::thread         m_thread;
        volatile bool       m_active;
    };
} // DevDriver
