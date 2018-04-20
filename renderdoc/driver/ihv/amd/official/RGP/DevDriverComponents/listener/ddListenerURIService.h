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
* @file  ddListenerURIService.h
* @brief Class declaration for ListenerURIService
***********************************************************************************************************************
*/

#pragma once

#include "protocols/ddURIService.h"

namespace DevDriver
{
    class ListenerCore;

    // String used to identify the listener URI service
    DD_STATIC_CONST char kListenerURIServiceName[] = "listener";

    class ListenerURIService : public IService
    {
    public:
        ListenerURIService();
        ~ListenerURIService();

        // Returns the name of the service
        const char* GetName() const override final { return kListenerURIServiceName; }

        // Binds a listener core to the service
        // All requests will be handled using the currently bound listener core
        void BindListenerCore(ListenerCore* pListenerCore) { m_pListenerCore = pListenerCore; }

        // Handles an incoming URI request
        Result HandleRequest(URIRequestContext* pContext) override final;

    private:
        // Currently bound listener core
        ListenerCore* m_pListenerCore;
    };
} // DevDriver
