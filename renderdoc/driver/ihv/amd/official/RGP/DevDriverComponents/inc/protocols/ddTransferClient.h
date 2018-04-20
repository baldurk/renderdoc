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
* @file  ddTransferClient.h
* @brief Class declaration for TransferClient.
***********************************************************************************************************************
*/

#pragma once

#include "baseProtocolClient.h"
#include "protocols/ddTransferProtocol.h"

namespace DevDriver
{
    class IMsgChannel;

    namespace TransferProtocol
    {
        class TransferClient final : public BaseProtocolClient
        {
        public:
            explicit TransferClient(IMsgChannel* pMsgChannel);
            ~TransferClient();

            // Requests a transfer on the remote client. Returns Success if the request was successful and data
            // is being sent to the client. Returns the size in bytes of the data being transferred in
            // pTransferSizeInBytes.
            Result RequestPullTransfer(BlockId blockId, size_t* pTransferSizeInBytes);

            // Reads transfer data from a previous transfer that completed successfully.
            Result ReadPullTransferData(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead);

            // Aborts a pull transfer in progress.
            Result AbortPullTransfer();

            // Requests a transfer on the remote client. Returns Success if the request was successful and data
            // can be written to the server.
            Result RequestPushTransfer(BlockId blockId, size_t transferSizeInBytes);

            // Writes transfer data to the remote server.
            Result WritePushTransferData(const uint8* pSrcBuffer, size_t bufferSize);

            // Closes the push transfer session, optionally discarding any data already transmitted
            Result ClosePushTransfer(bool discard = false);

            // Returns true if there's currently a transfer in progress.
            bool IsTransferInProgress() const
            {
                return IsConnected() && (m_transferContext.state == TransferState::TransferInProgress);
            }

            // Backwards compatibility
            Result RequestTransfer(BlockId blockId, size_t* pTransferSizeInBytes)
            {
                return RequestPullTransfer(blockId, pTransferSizeInBytes);
            }

            Result ReadTransferData(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead)
            {
                return ReadPullTransferData(pDstBuffer, bufferSize, pBytesRead);
            }

        private:
            void ResetState() override;

            // Helper method to send a payload, handling backwards compatibility and retrying.
            Result SendTransferPayload(const SizedPayloadContainer& container,
                                       uint32                       timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                       uint32                       retryInMs   = kDefaultRetryTimeoutInMs);
            // Helper method to handle sending a payload from a SizedPayloadContainer, including retrying if busy.
            Result ReceiveTransferPayload(SizedPayloadContainer* pContainer,
                                          uint32                 timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                          uint32                 retryInMs   = kDefaultRetryTimeoutInMs);
            // Helper method to send and then receive using a SizedPayloadContainer object.
            Result TransactTransferPayload(SizedPayloadContainer* pContainer,
                                           uint32                 timeoutInMs = kDefaultCommunicationTimeoutInMs,
                                           uint32                 retryInMs   = kDefaultRetryTimeoutInMs);

            enum class TransferState : uint32
            {
                Idle = 0,
                TransferInProgress,
                Error
            };

            // Context structure for tracking all state specific to a transfer.
            struct ClientTransferContext
            {
                TransferState state;
                TransferType  type;
                uint32 totalBytes;
                uint32 crc32;
                size_t dataChunkSizeInBytes;
                size_t dataChunkBytesTransfered;
                SizedPayloadContainer scratchPayload;
            };

            ClientTransferContext m_transferContext;

            DD_STATIC_CONST uint32 kTransferChunkTimeoutInMs = 3000;
        };
    }
} // DevDriver
