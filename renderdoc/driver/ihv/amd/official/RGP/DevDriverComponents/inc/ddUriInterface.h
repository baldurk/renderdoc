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
* @file  ddUriInterface.h
* @brief Declaration for public URI interfaces.
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "ddPlatform.h"
#include "util/sharedptr.h"

namespace DevDriver
{
    namespace TransferProtocol
    {
        class ServerBlock;
    }

    enum struct URIDataFormat : uint32
    {
        Unknown = 0,
        Text,
        Binary,
        Count
    };

    // A struct that represents a unique URI request
    struct URIRequestContext
    {
        // Mutable arguments passed to the request
        char* pRequestArguments;

        // A server block to write the response data into.
        SharedPointer<TransferProtocol::ServerBlock> pResponseBlock;

        // The format of the data written into the response block.
        URIDataFormat responseDataFormat;
    };

    struct URIResponseHeader
    {
        // The size of the response data in bytes
        size_t responseDataSizeInBytes;

        // The format of the response data
        URIDataFormat responseDataFormat;
    };

    // Base class for URI services
    class IService
    {
    public:
        virtual ~IService() {}

        // Returns the name of the service
        virtual const char* GetName() const = 0;

#if !DD_VERSION_SUPPORTS(GPUOPEN_URI_RESPONSE_FORMATS_VERSION)
        // Attempts to handle a request from a client
        // Deprecated
        virtual Result HandleRequest(char*                                        pArguments,
                                     SharedPointer<TransferProtocol::ServerBlock> pBlock)
        {
            DD_UNUSED(pArguments);
            DD_UNUSED(pBlock);
            DD_NOT_IMPLEMENTED();
            return Result::Error;
        }

        // Attempts to handle a request from a client
        virtual Result HandleRequest(URIRequestContext* pContext)
        {
            DD_ASSERT(pContext != nullptr);

            const Result result = HandleRequest(pContext->pRequestArguments,
                                                pContext->pResponseBlock);
            if (result == Result::Success)
            {
                pContext->responseDataFormat = URIDataFormat::Text;
            }

            return result;
        }
#else
        // Attempts to handle a request from a client
        virtual Result HandleRequest(URIRequestContext* pContext) = 0;
#endif
    protected:
        IService() {};
    };
} // DevDriver
