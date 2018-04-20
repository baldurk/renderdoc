/*
 *******************************************************************************
 *
 * Copyright (c) 2018 Advanced Micro Devices, Inc. All rights reserved.
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

#include "protocols/ddTransferClient.h"
#include <cstring>

#define TRANSFER_CLIENT_MIN_MAJOR_VERSION 1
#define TRANSFER_CLIENT_MAX_MAJOR_VERSION 2

namespace DevDriver
{
    namespace TransferProtocol
    {
        // ============================================================================================================
        TransferClient::TransferClient(IMsgChannel* pMsgChannel)
            : BaseProtocolClient(pMsgChannel,
                                 Protocol::Transfer,
                                 TRANSFER_CLIENT_MIN_MAJOR_VERSION,
                                 TRANSFER_CLIENT_MAX_MAJOR_VERSION)
        {
            memset(&m_transferContext, 0, sizeof(m_transferContext));
        }

        // ============================================================================================================
        TransferClient::~TransferClient()
        {
        }

        // ============================================================================================================
        Result TransferClient::RequestPullTransfer(BlockId blockId, size_t* pTransferSizeInBytes)
        {
            Result result = Result::Error;

            if ((m_transferContext.state == TransferState::Idle) &&
                (pTransferSizeInBytes != nullptr))
            {
                SizedPayloadContainer container = {};
                container.CreatePayload<TransferRequest>(blockId, TransferType::Pull, 0);

                result = TransactTransferPayload(&container);

                if ((result == Result::Success) &&
                    (container.GetPayload<TransferHeader>().command == TransferMessage::TransferDataHeader))
                {
                    // We've successfully received the transfer data header. Check if the transfer request was successful.
                    if (m_pSession->GetVersion() >= TRANSFER_REFACTOR_VERSION)
                    {
                        const TransferDataHeaderV2& receivedHeader = container.GetPayload<TransferDataHeaderV2>();
                        m_transferContext.state = TransferState::TransferInProgress;
                        m_transferContext.totalBytes = receivedHeader.sizeInBytes;
                        m_transferContext.crc32 = 0;
                        m_transferContext.dataChunkSizeInBytes = 0;
                        m_transferContext.dataChunkBytesTransfered = 0;

                        *pTransferSizeInBytes = receivedHeader.sizeInBytes;
                    }
                    else
                    {
                        const TransferDataHeader& receivedHeader = container.GetPayload<TransferDataHeader>();
                        result = receivedHeader.result;
                        if (result == Result::Success)
                        {
                            m_transferContext.state = TransferState::TransferInProgress;
                            m_transferContext.totalBytes = receivedHeader.sizeInBytes;
                            m_transferContext.crc32 = 0;
                            m_transferContext.dataChunkSizeInBytes = 0;
                            m_transferContext.dataChunkBytesTransfered = 0;
                            m_transferContext.type = TransferType::Pull;

                            *pTransferSizeInBytes = receivedHeader.sizeInBytes;
                        }
                        else
                        {
                            // The transfer failed on the remote server.
                            m_transferContext.state = TransferState::Error;
                            result = Result::Error;
                        }
                    }
                }
                else
                {
                    // We either didn't receive a response, or we received an invalid response.
                    m_transferContext.state = TransferState::Error;
                    result = Result::Error;
                }
            }

            return result;
        }

        // ============================================================================================================
        Result TransferClient::ReadPullTransferData(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead)
        {
            Result result = Result::Error;

            if ((m_transferContext.state == TransferState::TransferInProgress) && (pBytesRead != nullptr))
            {
                result = Result::Success;

                // There is no remaining data to read
                if ((m_transferContext.totalBytes == 0) &&
                    (m_transferContext.dataChunkSizeInBytes == m_transferContext.dataChunkBytesTransfered))
                {
                    result = Result::EndOfStream;
                    m_transferContext.state = TransferState::Idle;
                    *pBytesRead = 0;
                }
                else if (bufferSize > 0)
                {
                    SizedPayloadContainer& scratchPayload = m_transferContext.scratchPayload;

                    // There's space available in the caller's buffer, attempt to write data into it.
                    size_t remainingBufferSize = bufferSize;
                    while ((remainingBufferSize > 0) &&
                           (m_transferContext.state == TransferState::TransferInProgress))
                    {
                        // If we have local data, read from that.
                        const size_t dataChunkBytesAvailable =
                            (m_transferContext.dataChunkSizeInBytes - m_transferContext.dataChunkBytesTransfered);

                        if (dataChunkBytesAvailable > 0)
                        {
                            DD_ASSERT(scratchPayload.GetPayload<TransferHeader>().command == TransferMessage::TransferDataChunk);
                            const TransferDataChunk& chunk =
                                scratchPayload.GetPayload<TransferDataChunk>();

                            const size_t bytesToRead = Platform::Min(remainingBufferSize, dataChunkBytesAvailable);
                            const uint8* pData = (chunk.data + m_transferContext.dataChunkBytesTransfered);
                            memcpy(pDstBuffer + (bufferSize - remainingBufferSize), pData, bytesToRead);
                            m_transferContext.dataChunkBytesTransfered += bytesToRead;
                            remainingBufferSize -= bytesToRead;

                            // If this is the last of the data for the transfer, return end of stream and return to the idle state.
                            if ((m_transferContext.dataChunkBytesTransfered == m_transferContext.dataChunkSizeInBytes) &&
                                (m_transferContext.totalBytes == 0))
                            {
                                result = Result::EndOfStream;
                                m_transferContext.state = TransferState::Idle;
                            }
                        }
                        else if (m_transferContext.totalBytes > 0)
                        {
                            // Attempt to fetch a new chunk if we're out of data.
                            result = ReceiveTransferPayload(&m_transferContext.scratchPayload, kTransferChunkTimeoutInMs);

                            const TransferDataChunk& chunk =
                                scratchPayload.GetPayload<TransferDataChunk>();

                            if ((result == Result::Success) &&
                                (chunk.command == TransferMessage::TransferDataChunk))
                            {
                                // Calculate the remaining payload size. We clamp this to the minimum of the payload
                                // size specified and how many bytes are remaining. This works on the V1 protocol
                                // as all packets are guaranteed to be a full payload size, except for the last
                                // packet. That packet should be equal to the number of total bytes remaining.
                                // On V2 sessions, a server is free to send arbitrary sized chunks in situations
                                // that require it
                                const size_t receivedSize = scratchPayload.payloadSize - sizeof(TransferHeader);
                                const size_t payloadSize = Platform::Min(receivedSize, kMaxTransferDataChunkSize);
                                const uint32 adjustedPayloadSize = Platform::Min(static_cast<uint32>(payloadSize),
                                                                                 m_transferContext.totalBytes);

                                // Adjust global state
                                m_transferContext.dataChunkSizeInBytes = adjustedPayloadSize;
                                m_transferContext.dataChunkBytesTransfered = 0;
                                m_transferContext.totalBytes -= adjustedPayloadSize;

                                // Update the calculated CRC using the chunk we just received. The existing CRC value
                                // is used as an input, ensuring that we calculate the same value as the server.
                                m_transferContext.crc32 = CRC32(&chunk.data[0],
                                                                adjustedPayloadSize,
                                                                m_transferContext.crc32);

                                // If that was the last chunk we consume and verify the sentinel
                                if (m_transferContext.totalBytes == 0)
                                {
                                    SizedPayloadContainer sentinelPayload = {};
                                    result = ReceiveTransferPayload(&sentinelPayload, kTransferChunkTimeoutInMs);

                                    TransferDataSentinel& sentinel = sentinelPayload.GetPayload<TransferDataSentinel>();

                                    // If we didn't receive a sentinel or the read failed we return an error, otherwise
                                    if ((result != Result::Success) ||
                                        (sentinel.command != TransferMessage::TransferDataSentinel) ||
                                        (sentinel.result != Result::Success))
                                    {
                                        // Failed to receive the sentinel. Fail the transfer.
                                        m_transferContext.state = TransferState::Error;
                                    }
                                    else
                                    {
                                        // Check CRC
                                        if ((m_pSession->GetVersion() >= TRANSFER_REFACTOR_VERSION) &&
                                            (sentinel.crc32 != m_transferContext.crc32))
                                        {
                                            m_transferContext.state = TransferState::Error;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                // Failed to receive a transfer data chunk. Fail the transfer.
                                DD_ALERT_REASON("Pull transfer session received invalid data");
                                m_transferContext.state = TransferState::Error;
                            }
                        }
                    }

                    *pBytesRead = (bufferSize - remainingBufferSize);
                }
                else
                {
                    // No space available for writing in the caller's buffer.
                    *pBytesRead = 0;
                }
            }

            return result;
        }

        // ============================================================================================================
        Result TransferClient::RequestPushTransfer(BlockId blockId, size_t transferSizeInBytes)
        {
            Result result = Result::Error;
            if ((m_transferContext.state == TransferState::Idle) &&
                (m_pSession->GetVersion() >= TRANSFER_REFACTOR_VERSION) &&
                (blockId != kInvalidBlockId) &&
                (transferSizeInBytes != 0))
            {
                SizedPayloadContainer container = {};
                container.CreatePayload<TransferRequest>(blockId,
                                                         TransferType::Push,
                                                         static_cast<uint32>(transferSizeInBytes));
                Result transactResult = TransactTransferPayload(&container);
                if ((transactResult == Result::Success) &&
                    (container.GetPayload<TransferStatus>().command == TransferMessage::TransferStatus) &&
                    (container.GetPayload<TransferStatus>().result == Result::Success))
                {
                    m_transferContext.type = TransferType::Push;
                    m_transferContext.state = TransferState::TransferInProgress;
                    m_transferContext.totalBytes = static_cast<uint32>(transferSizeInBytes);
                    m_transferContext.crc32 = 0;
                    m_transferContext.dataChunkSizeInBytes = 0;
                    m_transferContext.dataChunkBytesTransfered = 0;
                    result = Result::Success;
                }
            }
            return result;
        }

        // ============================================================================================================
        Result TransferClient::WritePushTransferData(const uint8* pSrcBuffer, size_t bufferSize)
        {
            Result result = Result::Error;
            if ((m_transferContext.state == TransferState::TransferInProgress) &&
                (m_transferContext.type == TransferType::Push))
            {
                while ((m_transferContext.totalBytes > 0) & (bufferSize > 0))
                {
                    const size_t maxBytesInChunk = Platform::Min(kMaxTransferDataChunkSize,
                                                                 static_cast<size_t>(m_transferContext.totalBytes));
                    const size_t bytesToSend = Platform::Min(maxBytesInChunk, bufferSize);

                    m_transferContext.crc32 = CRC32(pSrcBuffer,
                                                    bytesToSend,
                                                    m_transferContext.crc32);

                    TransferDataChunk::WritePayload(pSrcBuffer, bytesToSend, &m_transferContext.scratchPayload);
                    result = SendTransferPayload(m_transferContext.scratchPayload);
                    if (result == Result::Success)
                    {
                        bufferSize -= bytesToSend;
                        pSrcBuffer += bytesToSend;
                    }
                    else if (result != Result::NotReady)
                    {
                        break;
                    }
                }

                if ((m_transferContext.totalBytes == 0) & (bufferSize > 0))
                {
                    result = Result::EndOfStream;
                }
            }

            return result;
        }

        // ============================================================================================================
        Result TransferClient::ClosePushTransfer(bool discard)
        {
            Result result = Result::Error;
            if ((m_transferContext.state == TransferState::TransferInProgress) &&
                (m_transferContext.type == TransferType::Push))
            {
                SizedPayloadContainer container = {};
                container.CreatePayload<TransferDataSentinel>(discard ? Result::Aborted : Result::Success,
                                                              m_transferContext.crc32);

                if ((TransactTransferPayload(&container) == Result::Success) &&
                    (container.GetPayload<TransferStatus>().command == TransferMessage::TransferStatus))
                {
                    result = container.GetPayload<TransferStatus>().result;
                    m_transferContext.state = TransferState::Idle;
                }
            }

            if ((result != Result::Success) && (result != Result::Aborted))
            {
                m_transferContext.state = TransferState::Error;
            }

            return result;
        }

        // ============================================================================================================
        Result TransferClient::AbortPullTransfer()
        {
            Result result = Result::Error;

            if ((m_transferContext.state == TransferState::TransferInProgress) &&
                (m_transferContext.type == TransferType::Pull))
            {
                SizedPayloadContainer container = {};

                container.CreatePayload<TransferStatus>(Result::Aborted);
                Result transferResult = SendTransferPayload(container);
                if (transferResult == Result::Success)
                {
                    // Discard all messages until we find the sentinel.
                    while ((transferResult == Result::Success) &&
                        (container.GetPayload<TransferHeader>().command != TransferMessage::TransferDataSentinel))
                    {
                        transferResult = ReceiveTransferPayload(&container);
                    }

                    if ((transferResult == Result::Success) &&
                        (container.GetPayload<TransferHeader>().command == TransferMessage::TransferDataSentinel))
                    {
                        // We've successfully aborted the transfer.
                        const Result finalResult = container.GetPayload<TransferDataSentinel>().result;
                        // We've either reached the original sentinel that indicates the end of the transfer or we've
                        // received a sentinel in response to calling abort. Sanity check the results with an assert.
                        DD_ASSERT((finalResult == Result::Aborted) ||
                                  (finalResult == Result::Success));
                        DD_UNUSED(finalResult);

                        m_transferContext.state = TransferState::Idle;
                        result = Result::Success;
                    }
                }
            }

            if (result != Result::Success)
            {
                m_transferContext.state = TransferState::Error;
            }

            return result;
        }

        // ============================================================================================================
        void TransferClient::ResetState()
        {
            memset(&m_transferContext, 0, sizeof(m_transferContext));
        }

        // ============================================================================================================
        // Helper method to send a payload, handling backwards compatibility and retrying.
        Result TransferClient::SendTransferPayload(
            const SizedPayloadContainer& container,
            uint32 timeoutInMs,
            uint32 retryInMs)
        {
            // Use the legacy size for the container if we're connected to an older client, otherwise use the real size.
            const Version sessionVersion = (m_pSession.IsNull() == false) ? m_pSession->GetVersion() : 0;
            const uint32 payloadSize = (sessionVersion >= TRANSFER_REFACTOR_VERSION) ? container.payloadSize : kMaxPayloadSizeInBytes;

            return SendSizedPayload(container.payload,
                                    payloadSize,
                                    timeoutInMs,
                                    retryInMs);
        }

        // ============================================================================================================
        // Helper method to handle sending a payload from a SizedPayloadContainer, including retrying if busy.
        Result TransferClient::ReceiveTransferPayload(
            SizedPayloadContainer* pContainer,
            uint32 timeoutInMs,
            uint32 retryInMs)
        {
            return ReceiveSizedPayload(pContainer->payload,
                                       sizeof(pContainer->payload),
                                       &pContainer->payloadSize,
                                       timeoutInMs,
                                       retryInMs);
        }

        // ============================================================================================================
        // Helper method to send and then receive using a SizedPayloadContainer object.
        Result TransferClient::TransactTransferPayload(
            SizedPayloadContainer* pContainer,
            uint32 timeoutInMs,
            uint32 retryInMs)
        {
            Result result = SendTransferPayload(*pContainer, timeoutInMs, retryInMs);
            if (result == Result::Success)
            {
                result = ReceiveTransferPayload(pContainer, timeoutInMs, retryInMs);
            }
            return result;
        }
    }

} // DevDriver
