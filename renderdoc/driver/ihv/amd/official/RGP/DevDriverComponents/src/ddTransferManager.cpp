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

#include "ddTransferManager.h"
#include "protocols/ddTransferServer.h"
#include "messageChannel.h"

namespace DevDriver
{
    namespace TransferProtocol
    {
        // ============================================================================================================
        TransferManager::TransferManager(const AllocCb& allocCb)
            : m_pMessageChannel(nullptr)
            , m_pSessionManager(nullptr)
            , m_pTransferServer(nullptr)
            , m_allocCb(allocCb)
            , m_rng()
            , m_mutex()
            , m_registeredServerBlocks(allocCb)
        {}

        // ============================================================================================================
        TransferManager::~TransferManager()
        {
            Destroy();
        }

        // ============================================================================================================
        Result TransferManager::Init(IMsgChannel* pMsgChannel, SessionManager* pSessionManager)
        {
            DD_ASSERT(pMsgChannel != nullptr);
            DD_ASSERT(pSessionManager != nullptr);

            m_pMessageChannel = pMsgChannel;
            m_pSessionManager = pSessionManager;

            m_pTransferServer = DD_NEW(TransferServer, m_allocCb)(m_pMessageChannel, this);
            if (m_pTransferServer != nullptr)
            {
                m_pSessionManager->RegisterProtocolServer(m_pTransferServer);
            }

            return (m_pTransferServer != nullptr) ? Result::Success : Result::Error;
        }

        // ============================================================================================================
        void TransferManager::Destroy()
        {
            if (m_pTransferServer != nullptr)
            {
                m_pSessionManager->UnregisterProtocolServer(m_pTransferServer);
                DD_DELETE(m_pTransferServer, m_allocCb);
                m_pTransferServer = nullptr;
            }
        }

        // ============================================================================================================
        SharedPointer<ServerBlock> TransferManager::OpenServerBlock()
        {
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);

            BlockId newBlockId = kInvalidBlockId;
            do
            {
                newBlockId = m_rng.Generate();
            } while ((newBlockId == kInvalidBlockId) && m_registeredServerBlocks.Contains(newBlockId));

            // Attempt to allocate a new server block
            SharedPointer<ServerBlock> pBlock = SharedPointer<ServerBlock>::Create(m_allocCb,
                                                                                   m_allocCb,
                                                                                   newBlockId);
            if (!pBlock.IsNull())
            {
                m_registeredServerBlocks.Create(newBlockId, pBlock);
            }

            return pBlock;
        }

        // ============================================================================================================
        SharedPointer<ServerBlock> TransferManager::GetServerBlock(BlockId serverBlockId)
        {
            SharedPointer<ServerBlock> pBlock = SharedPointer<ServerBlock>();
            Platform::LockGuard<Platform::Mutex> lock(m_mutex);
            const auto blockIter = m_registeredServerBlocks.Find(serverBlockId);
            if (blockIter != m_registeredServerBlocks.End())
            {
                pBlock = blockIter->value;
            }
            return pBlock;
        }

        // ============================================================================================================
        void TransferManager::CloseServerBlock(SharedPointer<ServerBlock>& pBlock)
        {
            if (!pBlock.IsNull())
            {
                Platform::LockGuard<Platform::Mutex> lock(m_mutex);

                m_registeredServerBlocks.Erase(pBlock->GetBlockId());

                // Clear the external shared pointer to the block.
                pBlock.Clear();
            }
        }

        // ============================================================================================================
        PullBlock* TransferManager::OpenPullBlock(ClientId clientId, BlockId blockId)
        {
            PullBlock* pBlock = DD_NEW(PullBlock, m_allocCb)(m_pMessageChannel, blockId);
            if (pBlock != nullptr)
            {
                // Connect to the remote client and request a transfer.
                Result result = pBlock->m_transferClient.Connect(clientId);
                if (result == Result::Success)
                {
                    result = pBlock->m_transferClient.RequestPullTransfer(blockId, &pBlock->m_blockDataSize);
                }

                // If we fail the transfer or connection, destroy the block.
                if (result != Result::Success)
                {
                    pBlock->m_transferClient.Disconnect();
                    DD_DELETE(pBlock, m_allocCb);
                    pBlock = nullptr;
                }
            }
            return pBlock;
        }

        // ============================================================================================================
        void TransferManager::ClosePullBlock(PullBlock** ppBlock)
        {
            DD_ASSERT(ppBlock != nullptr);

            TransferProtocol::TransferClient& transferClient = (*ppBlock)->m_transferClient;
            if (transferClient.IsTransferInProgress())
            {
                // Attempt to abort the transfer if there's currently one in progress.
                const Result result = transferClient.AbortPullTransfer();
                DD_UNUSED(result);
            }

            transferClient.Disconnect();
            DD_DELETE((*ppBlock), m_allocCb);
            *ppBlock = nullptr;
        }

        // ============================================================================================================
        PushBlock* TransferManager::OpenPushBlock(ClientId clientId, BlockId blockId, size_t blockSize)
        {
            PushBlock* pBlock = DD_NEW(PushBlock, m_allocCb)(m_pMessageChannel, blockId);
            if (pBlock != nullptr)
            {
                // Connect to the remote client and request a transfer.
                Result result = pBlock->m_transferClient.Connect(clientId);
                if (result == Result::Success)
                {
                    result = pBlock->m_transferClient.RequestPushTransfer(blockId, blockSize);
                }

                // If we fail the transfer or connection, destroy the block.
                if (result != Result::Success)
                {
                    pBlock->m_transferClient.Disconnect();
                    DD_DELETE(pBlock, m_allocCb);
                    pBlock = nullptr;
                }
            }
            return pBlock;
        }

        // ============================================================================================================
        void TransferManager::ClosePushBlock(PushBlock** ppBlock)
        {
            DD_ASSERT(ppBlock != nullptr);

            TransferProtocol::TransferClient& transferClient = (*ppBlock)->m_transferClient;
            if (transferClient.IsTransferInProgress())
            {
                // Attempt to abort the transfer if there's currently one in progress.
                const Result result = transferClient.ClosePushTransfer(true);
                DD_ASSERT(result == Result::Aborted);
                DD_UNUSED(result);
            }

            transferClient.Disconnect();
            DD_DELETE((*ppBlock), m_allocCb);
            *ppBlock = nullptr;
        }

        // ============================================================================================================
        void ServerBlock::Write(const uint8* pSrcBuffer, size_t numBytes)
        {
            // Writes can only be performed on blocks that are not closed.
            DD_ASSERT(m_isClosed == false);

            if (numBytes > 0)
            {
                // Calculate how many bytes we have available.
                const size_t blockCapacityInBytes = (m_chunks.Size() * kTransferChunkSizeInBytes);
                const size_t bytesAvailable = (blockCapacityInBytes - m_blockDataSize);

                // Allocate more chunks if necessary.
                if (bytesAvailable < numBytes)
                {
                    const size_t additionalBytesRequired = (numBytes - bytesAvailable);
                    const size_t numChunksRequired =
                        (Platform::Pow2Align(additionalBytesRequired, kTransferChunkSizeInBytes) / kTransferChunkSizeInBytes);
                    m_chunks.Resize(m_chunks.Size() + numChunksRequired);
                }

                // Copy the new data into the block
                uint8* pData = (reinterpret_cast<uint8*>(m_chunks.Data()) + m_blockDataSize);
                memcpy(pData, pSrcBuffer, numBytes);
                m_crc32 = CRC32(pData, numBytes, m_crc32);
                m_blockDataSize += numBytes;
            }
        }

        // ============================================================================================================
        void ServerBlock::Close()
        {
            DD_ASSERT(m_isClosed == false);

            m_isClosed = true;
        }

        // ============================================================================================================
        void ServerBlock::Reset()
        {
            m_isClosed = false;
            m_blockDataSize = 0;
            m_crc32 = 0;
        }

        // ============================================================================================================
        void ServerBlock::Reserve(size_t bytes)
        {
            if (!m_isClosed)
            {
                m_chunks.Reserve(Platform::Pow2Align(bytes, kTransferChunkSizeInBytes) / kTransferChunkSizeInBytes);
            }
        }

        // ============================================================================================================
        void ServerBlock::BeginTransfer()
        {
            m_pendingTransfersMutex.Lock();

            // Increment the number of pending transfers.
            ++m_numPendingTransfers;

            // Reset the event if this is the first transfer that's starting on this block.
            if (m_numPendingTransfers == 1)
            {
                m_transfersCompletedEvent.Clear();
            }

            m_pendingTransfersMutex.Unlock();
        }

        // ============================================================================================================
        void ServerBlock::EndTransfer()
        {
            m_pendingTransfersMutex.Lock();

            // We should always have pending transfers when end is called.
            DD_ASSERT(m_numPendingTransfers > 0);

            // Decrement the number of pending transfers.
            --m_numPendingTransfers;

            // Signal the event if this was the last transfer that was pending on this block.
            if (m_numPendingTransfers == 0)
            {
                m_transfersCompletedEvent.Signal();
            }

            m_pendingTransfersMutex.Unlock();
        }

        // ============================================================================================================
        Result ServerBlock::WaitForPendingTransfers(uint32 timeoutInMs)
        {
            return m_transfersCompletedEvent.Wait(timeoutInMs);
        }

        // ============================================================================================================
        Result PullBlock::Read(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead)
        {
            return m_transferClient.ReadPullTransferData(pDstBuffer, bufferSize, pBytesRead);
        }

        // ============================================================================================================
        Result PushBlock::Write(const uint8* pDstBuffer, size_t bufferSize)
        {
            return m_transferClient.WritePushTransferData(pDstBuffer, bufferSize);
        }

        // ============================================================================================================
        Result PushBlock::Finalize()
        {
            return m_transferClient.ClosePushTransfer(false);
        }

        // ============================================================================================================
        Result PushBlock::Discard()
        {
            return m_transferClient.ClosePushTransfer(true);
        }
    } // TransferProtocol
} // DevDriver
