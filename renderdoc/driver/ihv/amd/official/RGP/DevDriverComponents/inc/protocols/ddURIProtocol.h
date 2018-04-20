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
* @file  ddURIProtocol.h
* @brief Protocol header for the URI protocol.
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "ddTransferProtocol.h"

/*
***********************************************************************************************************************
* URI Protocol
***********************************************************************************************************************
*/

#define URI_PROTOCOL_MAJOR_VERSION 2
#define URI_PROTOCOL_MINOR_VERSION 0

#define URI_INTERFACE_VERSION ((URI_INTERFACE_MAJOR_VERSION << 16) | URI_INTERFACE_MINOR_VERSION)

#define URI_PROTOCOL_MINIMUM_MAJOR_VERSION 1

/*
***********************************************************************************************************************
*| Version | Change Description                                                                                       |
*| ------- | ---------------------------------------------------------------------------------------------------------|
*|  2.0    | Added support for response data formats.                                                                 |
*|  1.0    | Initial version                                                                                          |
***********************************************************************************************************************
*/

#define URI_RESPONSE_FORMATS_VERSION 2
#define URI_INITIAL_VERSION 1

namespace DevDriver
{
    namespace URIProtocol
    {
        ///////////////////////
        // GPU Open URI Protocol
        enum struct URIMessage : MessageCode
        {
            Unknown = 0,
            URIRequest,
            URIResponse,
            Count,
        };

        ///////////////////////
        // URI Types
        enum struct ResponseDataFormat : uint32
        {
            Unknown = 0,
            Text,
            Binary,
            Count
        };

        ///////////////////////
        // URI Constants
        DD_STATIC_CONST uint32 kURIStringSize = 256;

        ///////////////////////
        // URI Payloads

        DD_NETWORK_STRUCT(URIRequestPayload, 4)
        {
            char uriString[kURIStringSize];
        };

        DD_CHECK_SIZE(URIRequestPayload, kURIStringSize);

        DD_NETWORK_STRUCT(URIResponsePayload, 4)
        {
            Result result;
            TransferProtocol::BlockId blockId;
            ResponseDataFormat format; // format is only valid in v2 sessions or higher.
        };

        DD_CHECK_SIZE(URIResponsePayload, 12);

        DD_NETWORK_STRUCT(URIPayload, 4)
        {
            URIMessage  command;
            // pad out to 4 bytes for alignment requirements
            char        padding[3];
            union
            {
                URIRequestPayload  uriRequest;
                URIResponsePayload uriResponse;
            };
        };

        DD_CHECK_SIZE(URIPayload, 260);
    }
}
