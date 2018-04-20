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
* @file  msgChannel.h
* @brief Interface declaration for IMsgChannel
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"

namespace DevDriver
{
    class IMsgTransport
    {
    public:
        virtual ~IMsgTransport() {}

        // Connect and disconnect from the transport.
        virtual Result Connect(ClientId* pClientId, uint32 timeoutInMs) = 0;
        virtual Result Disconnect() = 0;

        // Read and Write messages from a connected transport
        virtual Result WriteMessage(const MessageBuffer &messageBuffer) = 0;
        virtual Result ReadMessage(MessageBuffer &messageBuffer, uint32 timeoutInMs) = 0;

        // Get a human-readable string describing the connection type.
        virtual const char* GetTransportName() const = 0;

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
        virtual Result UpdateClientStatus(ClientId clientId, StatusFlags flags) = 0;
#endif

        // Static method to be implemented by individual transports
        // true indicates that the transport is incapable of detecting
        //   dropped connections and some form of keep-alive is required
        // false indicates that the transport can properly detect dropped
        //   connections
        DD_STATIC_CONST bool RequiresKeepAlive()
        {
            return false;
        }

        // Static method to be implemented by individual transports
        // true indicates that Connect is expected to also negotiate a client ID
        // false indicates that the MessageChannel needs to do it's own client ID
        //   negotiation, e.g. in the case of network connections
        DD_STATIC_CONST bool RequiresClientRegistration()
        {
            return false;
        }
    protected:
        IMsgTransport() {}
    };

} // DevDriver
