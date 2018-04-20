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
* @file  ddClientURIService.h
* @brief Class declaration for ClientURIService
***********************************************************************************************************************
*/

#pragma once

#include "ddUriInterface.h"

namespace DevDriver
{
    class IMsgChannel;

    // String used to identify the client URI service
    DD_STATIC_CONST char kClientURIServiceName[] = "client";

    class ClientURIService : public IService
    {
    public:
        ClientURIService();
        ~ClientURIService();

        // Returns the name of the service
        const char* GetName() const override final { return kClientURIServiceName; }

        // Binds a message channel to the service
        // All requests will be handled using the currently bound message channel
        void BindMessageChannel(IMsgChannel* pMsgChannel) { m_pMsgChannel = pMsgChannel; }

        // Handles an incoming URI request
        Result HandleRequest(URIRequestContext* pContext) override final;

    private:
        // Currently bound message channel
        IMsgChannel* m_pMsgChannel;
    };
} // DevDriver
