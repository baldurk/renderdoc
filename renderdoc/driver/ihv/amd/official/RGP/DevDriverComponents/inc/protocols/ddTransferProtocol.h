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
* @file  ddTransferProtocol.h
* @brief Protocol header for the block transfer protocol.
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "protocolSession.h"
#include <cstring>

/*
***********************************************************************************************************************
* Transfer Protocol
***********************************************************************************************************************
*/

#define TRANSFER_PROTOCOL_MAJOR_VERSION 2
#define TRANSFER_PROTOCOL_MINOR_VERSION 0

#define TRANSFER_INTERFACE_VERSION ((TRANSFER_INTERFACE_MAJOR_VERSION << 16) | TRANSFER_INTERFACE_MINOR_VERSION)

#define TRANSFER_PROTOCOL_MINIMUM_MAJOR_VERSION 1

/*
***********************************************************************************************************************
*| Version | Change Description                                                                                       |
*| ------- | ---------------------------------------------------------------------------------------------------------|
*|  2.0    | Refactor for variably sized messages + push transfers                                                    |
*|  1.0    | Initial version                                                                                          |
***********************************************************************************************************************
*/

#define TRANSFER_REFACTOR_VERSION 2
#define TRANSFER_INITIAL_VERSION 1

namespace DevDriver
{
    namespace TransferProtocol
    {
        ///////////////////////
        // GPU Open Transfer Protocol
        enum struct TransferMessage : uint32
        {
            Unknown = 0,
            TransferRequest,
            TransferDataHeader,
            TransferDataChunk,
            TransferDataSentinel,
            TransferStatus,
            Count,
        };

        enum struct TransferType : uint32
        {
            Pull = 0,
            Push,
            Count,
        };

        // @note: We currently subtract sizeof(uint32) instead of sizeof(TransferMessage) to work around struct packing issues.
        //        The compiler pads out TransferMessage to 4 bytes when it's included in the payload struct.
        DD_STATIC_CONST size_t kMaxTransferDataChunkSize = (kMaxPayloadSizeInBytes - sizeof(uint32));

        ///////////////////////
        // Transfer Types
        typedef uint32 BlockId;
        DD_STATIC_CONST BlockId kInvalidBlockId = 0;

        ///////////////////////
        // Transfer Payloads

        DD_NETWORK_STRUCT(TransferHeader, 4)
        {
            TransferMessage command;
            constexpr TransferHeader(TransferMessage message = TransferMessage::Unknown)
                : command(message)
            {
            }
        };

        DD_CHECK_SIZE(TransferHeader, 4);

        DD_NETWORK_STRUCT(TransferRequest, 4)
        {
            TransferMessage command;
            BlockId         blockId;
            TransferType    type;
            uint32          sizeInBytes;

            constexpr TransferRequest(BlockId blockId, TransferType type, uint32 size)
                : command(TransferMessage::TransferRequest)
                , blockId(blockId)
                , type(type)
                , sizeInBytes(size)
            {
            }
        };

        DD_CHECK_SIZE(TransferRequest, 16);

        DD_NETWORK_STRUCT(TransferDataHeader, 4)
        {
            TransferMessage command;
            Result result;
            uint32 sizeInBytes;

            constexpr TransferDataHeader(Result result, uint32 size)
                : command(TransferMessage::TransferDataHeader)
                , result(result)
                , sizeInBytes(size)
            {
            }
        };

        DD_CHECK_SIZE(TransferDataHeader, 12);

        DD_NETWORK_STRUCT(TransferDataHeaderV2, 4)
        {
            TransferMessage command;
            uint32 sizeInBytes;

            constexpr TransferDataHeaderV2(uint32 size)
                : command(TransferMessage::TransferDataHeader)
                , sizeInBytes(size)
            {}
        };

        DD_CHECK_SIZE(TransferDataHeaderV2, 8);

        DD_NETWORK_STRUCT(TransferDataChunk, 4)
        {
            TransferMessage command;
            uint8           data[kMaxTransferDataChunkSize];

            static void WritePayload(
                const void *pData,
                size_t bytesToSend,
                SizedPayloadContainer* pContainer)
            {
                pContainer->payloadSize = static_cast<uint32>(bytesToSend + offsetof(TransferDataChunk, data));
                DD_ASSERT(pContainer->payloadSize <= kMaxPayloadSizeInBytes);
                TransferDataChunk& payload = pContainer->GetPayload<TransferDataChunk>();
                payload.command = TransferMessage::TransferDataChunk;
                memcpy(&payload.data[0], pData, bytesToSend);
            }
        };

        DD_CHECK_SIZE(TransferDataChunk, kMaxPayloadSizeInBytes);

        DD_NETWORK_STRUCT(TransferDataSentinel, 4)
        {
            TransferMessage command;
            Result          result;
            uint32          crc32;

            constexpr TransferDataSentinel(Result result, uint32 crc32)
                : command(TransferMessage::TransferDataSentinel)
                , result(result)
                , crc32(crc32)
            {
            }
        };

        DD_CHECK_SIZE(TransferDataSentinel, 12);

        DD_NETWORK_STRUCT(TransferStatus, 4)
        {
            TransferMessage command;
            Result result;

            constexpr TransferStatus(Result result)
                : command(TransferMessage::TransferStatus)
                , result(result)
            {}
        };

        DD_CHECK_SIZE(TransferStatus, 8);
    }
}
