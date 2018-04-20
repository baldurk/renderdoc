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
* @file  abstractClientManager.h
* @brief Class declaration for IClientManager
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "../transports/abstractListenerTransport.h"
#include <memory>

namespace DevDriver
{
    class IClientManager
    {
    public:
        virtual ~IClientManager() {}

        // TODO: explicit initialize and destroy?
        // anything else missing?
        virtual Result RegisterHost(ClientId* pClientId) = 0;
        virtual std::shared_ptr<IListenerTransport> GetHostTransport() = 0;
        virtual Result UnregisterHost() = 0;

#if DD_VERSION_SUPPORTS(GPUOPEN_DEPRECATE_LEGACY_KMD_VERSION)
        virtual Result RegisterClient(ClientId* pClientId) = 0;
#else
        virtual Result RegisterClient(Component componentType, StatusFlags flags, ClientId* pClientId) = 0;
#endif
        virtual Result UnregisterClient(ClientId clientId) = 0;

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
        virtual Result UpdateHostStatus(StatusFlags flags) = 0;
        virtual Result UpdateClientStatus(ClientId clientId, StatusFlags flags) = 0;
        virtual Result QueryStatus(StatusFlags *pFlags) = 0;
#endif

        virtual const char *GetClientManagerName() const = 0;
        virtual ClientId GetHostClientId() const = 0;

    protected:
        IClientManager() {}
    };

} // DevDriver
