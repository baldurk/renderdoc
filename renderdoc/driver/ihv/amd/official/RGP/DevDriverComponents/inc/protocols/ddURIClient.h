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
* @file  ddURIClient.h
* @brief Class declaration for URIClient.
***********************************************************************************************************************
*/

#pragma once

#include "baseProtocolClient.h"
#include "ddUriInterface.h"

namespace DevDriver
{
    class IMsgChannel;

    namespace TransferProtocol
    {
        class PullBlock;
    }

    namespace URIProtocol
    {
        // We alias these types for backwards compatibility
        using ResponseHeader = DevDriver::URIResponseHeader;

        class URIClient final : public BaseProtocolClient
        {
        public:
            explicit URIClient(IMsgChannel* pMsgChannel);
            ~URIClient();

            // Sends a URI request to the connected server.
            Result RequestURI(const char* pRequestString, ResponseHeader* pResponseHeader = nullptr);

#if !DD_VERSION_SUPPORTS(GPUOPEN_URI_RESPONSE_FORMATS_VERSION)
            // Sends a URI request to the connected server.
            // Deprecated
            Result RequestURI(const char* pRequestString, size_t* pResponseSizeInBytes);
#endif

            // Reads response data that was returned by a prior request.
            Result ReadResponse(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead);

            // Aborts the currently active request.
            Result AbortRequest();

        private:
            void ResetState() override;

            enum class State : uint32
            {
                Idle = 0,
                ReadResponse
            };

            // Context structure for tracking all state specific to a request.
            struct Context
            {
                State                        state;
                TransferProtocol::PullBlock* pBlock;
            };

            Context m_context;
        };
    }
} // DevDriver
