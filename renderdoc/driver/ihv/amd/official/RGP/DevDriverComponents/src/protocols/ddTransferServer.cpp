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

#include "protocols/ddTransferServer.h"
#include "ddTransferManager.h"
#include "msgChannel.h"

#define TRANSFER_SERVER_MIN_MAJOR_VERSION 1
#define TRANSFER_SERVER_MAX_MAJOR_VERSION 2

namespace DevDriver
{
    namespace TransferProtocol
    {
        enum class SessionState
        {
            Idle = 0,
            SendPayload,
            StartPullTransfer,
            ProcessPullTransfer,
            StartPushTransfer,
            ReceivePushTransferData,
        };

        class TransferServer::TransferSession
        {
        public:
            // ========================================================================================================
            TransferSession(TransferManager* pTransferManager, const SharedPointer<ISession>& pSession)
                : m_scratchPayload()
                , m_pTransferManager(pTransferManager)
                , m_pSession(pSession)
                , m_pBlock()
                , m_totalBytes(0)
                , m_bytesTransferred(0)
                , m_crc32(0)
                , m_state(SessionState::Idle)
            {
            }

            // ========================================================================================================
            ~TransferSession()
            {
                // If we're terminating a session with a valid block, then that means the transfer did not finish properly.
                // Make sure to notify the block that the transfer is now ending so we don't throw off the internal counter.
                if (m_pBlock.IsNull() == false)
                {
                    m_pBlock->EndTransfer();
                }
            }

            // Helper functions for working with SizedPayloadContainers and managing back-compat.

            // ========================================================================================================
            Result SendPayload(const SizedPayloadContainer& payload, uint32 timeoutInMs)
            {
                // If we're running an older transfer version, always write the fixed container size.
                // Otherwise, write the real size.
                const uint32 payloadSize =
                    (m_pSession->GetVersion() >= TRANSFER_REFACTOR_VERSION) ? payload.payloadSize : kMaxPayloadSizeInBytes;

                return m_pSession->Send(payloadSize, payload.payload, timeoutInMs);
            }

            // ========================================================================================================
            Result ReceivePayload(SizedPayloadContainer* pPayload, uint32 timeoutInMs)
            {
                DD_ASSERT(pPayload != nullptr);
                return m_pSession->Receive(sizeof(pPayload->payload), pPayload->payload, &pPayload->payloadSize, timeoutInMs);
            }

            // ========================================================================================================
            void ProcessPayload()
            {
                DD_ASSERT(m_state == SessionState::Idle);

                switch (m_scratchPayload.GetPayload<TransferHeader>().command)
                {
                case TransferMessage::TransferRequest:
                {
                    const TransferRequest request = m_scratchPayload.GetPayload<TransferRequest>();

                    switch (request.type)
                    {
                        // It is invalid for sessions of version less than TRANSFER_REFACTOR_VERSION to set a non-zero
                        // value for request.type
                    case TransferType::Pull:
                    {
                        // Determine if the requested block is available. Available, in this context, means that
                        // the block exists and has been closed.
                        // If the block is available, start the transfer process.
                        // If the block is not available, return an error response.
                        SharedPointer<ServerBlock> pBlock = m_pTransferManager->GetServerBlock(request.blockId);
                        const bool blockIsAvailable = (!pBlock.IsNull() && pBlock->IsClosed());
                        if (blockIsAvailable && (m_state == SessionState::Idle))
                        {
                            // Increments the number of pending transfers to prevent the block from being destroyed
                            // in the middle of a transfer.
                            pBlock->BeginTransfer();

                            // Use the block information to populate our transfer context.
                            m_pBlock = pBlock;
                            m_totalBytes = pBlock->GetBlockDataSize();
                            m_bytesTransferred = 0;
                            m_crc32 = pBlock->GetCrc32();
                            m_state = SessionState::StartPullTransfer;

                            const uint32 blockSizeInBytes = static_cast<uint32>(m_pBlock->GetBlockDataSize());
                            if (m_pSession->GetVersion() >= TRANSFER_REFACTOR_VERSION)
                            {
                                m_scratchPayload.CreatePayload<TransferDataHeaderV2>(blockSizeInBytes);
                            }
                            else
                            {
                                m_scratchPayload.CreatePayload<TransferDataHeader>(Result::Success, blockSizeInBytes);
                            }

                            SendPullTransferHeader();
                        }
                        else
                        {
                            m_scratchPayload.CreatePayload<TransferStatus>(Result::Error);
                            m_state = SessionState::SendPayload;
                            SendScratchPayloadAndMoveToIdle();
                        }
                        break;
                    }
                    case TransferType::Push:
                    {
                        DD_ASSERT(m_pSession->GetVersion() >= TRANSFER_REFACTOR_VERSION);
                        // Determine if the requested block is available. Available, in this context, means that
                        // the block exists and has not been closed yet.
                        // If the block is available, start the transfer process.
                        // If the block is not available, return an error response.
                        SharedPointer<ServerBlock> pBlock = m_pTransferManager->GetServerBlock(request.blockId);
                        const bool blockIsAvailable = (!pBlock.IsNull() && !pBlock->IsClosed());
                        if (blockIsAvailable && (m_state == SessionState::Idle))
                        {
                            // Increments the number of pending transfers to prevent the block from being destroyed
                            // in the middle of a transfer.
                            pBlock->BeginTransfer();

                            // Increments the number of pending transfers to prevent the block from being destroyed
                            // in the middle of a transfer.
                            m_pBlock = pBlock;
                            m_bytesTransferred = 0;
                            m_crc32 = 0;

                            m_totalBytes = request.sizeInBytes;
                            if (m_totalBytes > 0)
                            {
                                m_pBlock->Reserve(m_totalBytes);
                                m_state = SessionState::StartPushTransfer;
                                m_scratchPayload.CreatePayload<TransferStatus>(Result::Success);
                                StartPushTransferSession();
                            }
                            else
                            {
                                m_state = SessionState::SendPayload;
                                m_scratchPayload.CreatePayload<TransferStatus>(Result::Error);
                                SendScratchPayloadAndMoveToIdle();
                            }
                        }
                        else
                        {
                            m_scratchPayload.CreatePayload<TransferStatus>(Result::Error);
                            m_state = SessionState::SendPayload;
                            SendScratchPayloadAndMoveToIdle();
                        }
                        break;
                    }
                    default:
                    {
                        m_scratchPayload.CreatePayload<TransferStatus>(Result::Error);
                        m_state = SessionState::SendPayload;
                        SendScratchPayloadAndMoveToIdle();
                        break;
                    }
                    }
                    break;
                }

                case TransferMessage::TransferStatus:
                {
                    DD_ALERT(m_scratchPayload.GetPayload<TransferStatus>().result == Result::Aborted);
                    // It's possible that we may receive a transfer abort request after we've already sent
                    // all the transfer data to the remote client successfully. This can happen when the
                    // remaining amount of data for the transfer fits into the entire send window.
                    // In this case, we still need to respond correctly and send the client an abort sentinel.
                    SendSentinel(Result::Aborted);
                    break;
                }

                default:
                {
                    // Invalid command
                    DD_UNREACHABLE();
                    break;
                }
                }
            }

            // ========================================================================================================
            void SendScratchPayloadAndMoveToIdle()
            {
                DD_ASSERT(m_state == SessionState::SendPayload);
                if (SendPayload(m_scratchPayload, kNoWait) == Result::Success)
                {
                    m_state = SessionState::Idle;
                }
            }

            // ========================================================================================================
            void SendSentinel(Result status, uint32 crc32 = 0)
            {
                m_scratchPayload.CreatePayload<TransferDataSentinel>(status, crc32);
                m_state = SessionState::SendPayload;
                SendScratchPayloadAndMoveToIdle();
            }

            // ========================================================================================================
            void ProcessPullSession()
            {
                DD_ASSERT(m_state == SessionState::ProcessPullTransfer);

                // Look for an abort request.
                const Result result = ReceivePayload(&m_scratchPayload, kNoWait);

                // If we haven't received any messages from the client, then continue transferring data to them.
                if (result == Result::NotReady)
                {
                    while (m_bytesTransferred < m_totalBytes)
                    {
                        const uint8* pData = (m_pBlock->GetBlockData() + m_bytesTransferred);
                        const size_t bytesRemaining = (m_totalBytes - m_bytesTransferred);
                        const size_t bytesToSend = Platform::Min(kMaxTransferDataChunkSize, bytesRemaining);

                        TransferDataChunk::WritePayload(pData, bytesToSend, &m_scratchPayload);

                        const Result sendResult = SendPayload(m_scratchPayload, kNoWait);
                        if (sendResult == Result::Success)
                        {
                            m_bytesTransferred += bytesToSend;
                        }
                        else
                        {
                            break;
                        }
                    }

                    // If we've finished transferring all block data, send the sentinel and free the block.
                    if (m_bytesTransferred == m_totalBytes)
                    {
                        // Notify the block that a transfer is completing.
                        m_pBlock->EndTransfer();
                        m_pBlock.Clear();
                        SendSentinel(Result::Success, m_crc32);
                    }
                }
                else if (result == Result::Success)
                {
                    if (m_scratchPayload.GetPayload<TransferHeader>().command == TransferMessage::TransferStatus)
                    {
                        // This should only be received for an abort
                        DD_ALERT(m_scratchPayload.GetPayload<TransferStatus>().result == Result::Aborted);
                        SendSentinel(Result::Aborted);
                    }
                    else
                    {
                        // We should only ever receive abort requests in this state. Send back an error.
                        SendSentinel(Result::Error);
                        DD_ALERT_REASON("Invalid response received");
                    }
                }
                else
                {
                    // We've encountered an error while receiving. Do nothing. The session will close itself soon.
                }
            }

            // ========================================================================================================
            void SendPullTransferHeader()
            {
                DD_ASSERT(m_state == SessionState::StartPullTransfer);
                if (SendPayload(m_scratchPayload, kNoWait) == Result::Success)
                {
                    m_state = SessionState::ProcessPullTransfer;
                    ProcessPullSession();
                }
            }

            // ========================================================================================================
            void StartPushTransferSession()
            {
                DD_ASSERT(m_pSession->GetVersion() >= TRANSFER_REFACTOR_VERSION);
                DD_ASSERT(m_state == SessionState::StartPushTransfer);
                if (SendPayload(m_scratchPayload, kNoWait) == Result::Success)
                {
                    m_state = SessionState::ReceivePushTransferData;
                }
            }

            // ========================================================================================================
            void ReceivePushTransferData()
            {
                DD_ASSERT(m_state == SessionState::ReceivePushTransferData);
                DD_ASSERT(m_pSession->GetVersion() >= TRANSFER_REFACTOR_VERSION);
                Result result = Result::Success;
                do
                {
                    result = ReceivePayload(&m_scratchPayload, kNoWait);
                    if (result == Result::Success)
                    {
                        switch (m_scratchPayload.GetPayload<TransferHeader>().command)
                        {
                        case TransferMessage::TransferDataChunk:
                        {
                            if (m_bytesTransferred < m_totalBytes)
                            {
                                const size_t bytesRemaining = (m_totalBytes - m_bytesTransferred);
                                const size_t numBytes = m_scratchPayload.payloadSize - offsetof(TransferDataChunk, data);
                                const size_t bytesToWrite = Platform::Min(numBytes, bytesRemaining);
                                m_pBlock->Write(&m_scratchPayload.GetPayload<TransferDataChunk>().data[0], bytesToWrite);
                                m_bytesTransferred += bytesToWrite;
                            }
                            else
                            {
                                DD_ALERT_REASON("Client tried to write more than requested bytes to the server");
                                CancelTransfer(Result::InsufficientMemory);
                            }
                            break;
                        }
                        case TransferMessage::TransferDataSentinel:
                        {
                            const TransferDataSentinel& sentinel = m_scratchPayload.GetPayload<TransferDataSentinel>();
                            if ((sentinel.result == Result::Success) && (m_pBlock->GetCrc32() == sentinel.crc32))
                            {
                                m_pBlock->Close();
                                m_pBlock->EndTransfer();
                                m_pBlock.Clear();
                                // Transfer succeeded
                                m_state = SessionState::SendPayload;
                                m_scratchPayload.CreatePayload<TransferStatus>(Result::Success);
                                SendScratchPayloadAndMoveToIdle();
                            }
                            else
                            {
                                const Result abortReason = (sentinel.result != Result::Aborted) ? Result::Error : Result::Aborted;
                                CancelTransfer(abortReason);
                            }
                            break;
                        }
                        default:
                        {
                            DD_ALERT("Push transfer received unexpected packet from client");
                            CancelTransfer(Result::Error);
                            break;
                        }
                        }
                    }
                } while ((result == Result::Success) && (m_state == SessionState::ReceivePushTransferData));
            }

            // ========================================================================================================
            void CancelTransfer(Result reason)
            {
                if (!m_pBlock.IsNull())
                {
                    m_pBlock->Reset();
                    m_pBlock->EndTransfer();
                    m_pBlock.Clear();
                }
                m_state = SessionState::SendPayload;
                m_scratchPayload.CreatePayload<TransferStatus>(reason);
                SendScratchPayloadAndMoveToIdle();
            }

            // ========================================================================================================
            void UpdateSession()
            {
                // Identify which state the session is currently in and perform associated update.
                switch (m_state)
                {
                case SessionState::Idle:
                {
                    const Result result = ReceivePayload(&m_scratchPayload, kNoWait);
                    if (result == Result::Success)
                    {
                        ProcessPayload();
                    }
                    break;
                }

                case SessionState::ProcessPullTransfer:
                {
                    ProcessPullSession();
                    break;
                }
                case SessionState::StartPullTransfer:
                {
                    SendPullTransferHeader();
                    break;
                }

                case SessionState::StartPushTransfer:
                {
                    StartPushTransferSession();
                    break;
                }

                case SessionState::ReceivePushTransferData:
                {
                    ReceivePushTransferData();
                    break;
                }

                case SessionState::SendPayload:
                {
                    SendScratchPayloadAndMoveToIdle();
                    break;
                }

                default:
                {
                    DD_UNREACHABLE();
                    break;
                }
                }
            }

        private:
            SizedPayloadContainer      m_scratchPayload;
            TransferManager*           m_pTransferManager;
            SharedPointer<ISession>    m_pSession;
            SharedPointer<ServerBlock> m_pBlock;
            size_t                     m_totalBytes;
            size_t                     m_bytesTransferred;
            uint32                     m_crc32;
            SessionState               m_state;
        };

        // =====================================================================================================================
        TransferServer::TransferServer(IMsgChannel* pMsgChannel, TransferManager* pTransferManager)
            : BaseProtocolServer(pMsgChannel, Protocol::Transfer, TRANSFER_SERVER_MIN_MAJOR_VERSION, TRANSFER_SERVER_MAX_MAJOR_VERSION)
            , m_pTransferManager(pTransferManager)
        {
            DD_ASSERT(m_pMsgChannel != nullptr);
        }

        // =====================================================================================================================
        TransferServer::~TransferServer()
        {}

        // =====================================================================================================================
        void TransferServer::Finalize()
        {
            BaseProtocolServer::Finalize();
        }

        // =====================================================================================================================
        bool TransferServer::AcceptSession(const SharedPointer<ISession>& pSession)
        {
            DD_UNUSED(pSession);
            return true;
        }

        // =====================================================================================================================
        void TransferServer::SessionEstablished(const SharedPointer<ISession>& pSession)
        {
            // Allocate session data for the newly established session
            TransferSession* pSessionData = DD_NEW(TransferSession, m_pMsgChannel->GetAllocCb())(m_pTransferManager, pSession);
            pSession->SetUserData(pSessionData);
        }

        // =====================================================================================================================
        void TransferServer::UpdateSession(const SharedPointer<ISession>& pSession)
        {
            TransferSession* pSessionData = reinterpret_cast<TransferSession*>(pSession->GetUserData());
            pSessionData->UpdateSession();
        }

        // =====================================================================================================================
        void TransferServer::SessionTerminated(const SharedPointer<ISession>& pSession, Result terminationReason)
        {
            DD_UNUSED(terminationReason);
            TransferSession* pSessionData = reinterpret_cast<TransferSession*>(pSession->SetUserData(nullptr));

            // Free the session data
            if (pSessionData != nullptr)
            {
                DD_DELETE(pSessionData, m_pMsgChannel->GetAllocCb());
            }
        }
    }
} // DevDriver
