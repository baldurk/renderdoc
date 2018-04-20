/*
 *******************************************************************************
 *
 * Copyright (c) 2017-2018 Advanced Micro Devices, Inc. All rights reserved.
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
* @file  ddTransferManager.h
* @brief Class declaration for TransferManager
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"
#include "util/sharedptr.h"
#include "util/vector.h"
#include "util/hashMap.h"
#include "protocols/systemProtocols.h"
#include "protocols/ddTransferClient.h"

namespace DevDriver
{
    class IMsgChannel;
    class SessionManager;

    namespace TransferProtocol
    {
        class TransferManager;
        class TransferServer;

        // Size of an individual "chunk" within a transfer operation.
        static const size_t kTransferChunkSizeInBytes = 4096;

        // A struct that represents a single transfer chunk
        struct TransferChunk
        {
            uint8 Data[kTransferChunkSizeInBytes];
        };

        // Base class for transfer blocks.
        // A "block" is a binary blob of data associated with a unique id. Blocks can be created locally via the
        // transfer manager's OpenServerBlock function. Once a server block is closed, it can be accessed remotely
        // by other clients on the message bus. Remote clients can simply use their own transfer manager to open
        // the desired block over the bus via OpenPullBlock.
        class TransferBlock
        {
        public:
            explicit TransferBlock(BlockId blockId)
                : m_blockDataSize(0)
                , m_blockId(blockId) {}

            // Returns the unique id associated with this block
            BlockId GetBlockId() const { return m_blockId; }

            // Returns the size of the data contained within this block in bytes
            size_t GetBlockDataSize() const { return m_blockDataSize; }

        protected:
            size_t  m_blockDataSize; // The size of the data held by the block
            BlockId m_blockId;       // The id associated with this block
        };

        // A server transfer block.
        // Only supports writes and must be closed before the data can be accessed.
        // Writes can only be performed on blocks that have not been closed.
        class ServerBlock final : public TransferBlock
        {
            friend class TransferServer;
        public:
            explicit ServerBlock(const AllocCb& allocCb, BlockId blockId)
                : TransferBlock(blockId)
                , m_isClosed(false)
                , m_chunks(allocCb)
                , m_numPendingTransfers(0)
                , m_transfersCompletedEvent(true)
                , m_crc32(0)
                {}

            // Writes numBytes bytes from pSrcBuffer into the block.
            void Write(const uint8* pSrcBuffer, size_t numBytes);

            // Closes the block which exposes it to external clients and prevents further writes.
            void Close();

            // Resets the block to its initial state. Does not return allocated memory.
            void Reset();

            // Returns true if this block has been closed.
            bool IsClosed() const { return m_isClosed; }

            // Returns a const pointer to the underlying data contained within the block, or null if it contains
            // no data.
            const uint8* GetBlockData() const {
                return (m_blockDataSize > 0) ? reinterpret_cast<const uint8*>(m_chunks.Data()) : nullptr;
            }

            // Waits for all pending transfers to complete or for the timeout to expire.
            Result WaitForPendingTransfers(uint32 timeoutInMs);

            // Returns a CRC32 for the current block
            uint32 GetCrc32() const { return m_crc32; };

            // Reserves at least the specified number of bytes in the internal storage.
            void Reserve(size_t bytes);

        private:
            // Notifies the block that a new transfer has begun.
            void BeginTransfer();

            // Notifies the block that an existing transfer has ended.
            void EndTransfer();

            bool                  m_isClosed;                // A bool that indicates if the block is closed
            Vector<TransferChunk> m_chunks;                  // A list of transfer chunks used to store data
            Platform::Mutex       m_pendingTransfersMutex;   // A mutex used to control access to the pending transfers counter
            uint32                m_numPendingTransfers;     // A counter used to track the number of pending transfers
            Platform::Event       m_transfersCompletedEvent; // An event that is signaled when all pendings transfers are completed
            uint32                m_crc32;                   // CRC covering all data stored in this block
        };

        // Backwards compatibility type alias. This will be removed with a future interface version change.
        using LocalBlock = ServerBlock;

        // A transfer block for reading data from a remote client.
        class PullBlock final : public TransferBlock
        {
            friend class TransferManager;
        public:
            // Reads up to bufferSize bytes into pDstBuffer from the block.
            // Returns the number of bytes read in pBytesRead.
            Result Read(uint8* pDstBuffer, size_t bufferSize, size_t* pBytesRead);

        private:
            explicit PullBlock(IMsgChannel* pMsgChannel, BlockId blockId)
                : TransferBlock(blockId)
                , m_transferClient(pMsgChannel)
            {}

            TransferClient m_transferClient;
        };

        // A transfer block for sending data to a remote server block
        class PushBlock final : public TransferBlock
        {
            friend class TransferManager;
        public:
            // Reads up to bufferSize bytes from pDstBuffer and writes it into the remote block.
            Result Write(const uint8* pDstBuffer, size_t bufferSize);

            // Closes the block, telling the server to save the data already transfered.
            Result Finalize();

            // Closes the block, telling the server to discard any data already transfered.
            Result Discard();
        private:
            explicit PushBlock(IMsgChannel* pMsgChannel, BlockId blockId)
                : TransferBlock(blockId)
                , m_transferClient(pMsgChannel)
            {}

            TransferClient m_transferClient;
        };

        // Transfer manager class.
        // Manages interactions with server/remote transfer blocks.
        class TransferManager
        {
        public:
            explicit TransferManager(const AllocCb& allocCb);
            ~TransferManager();

            Result Init(IMsgChannel* pMsgChannel, SessionManager* pSessionManager);
            void Destroy();

            // Returns a shared pointer to a server block or nullptr in the case of an error.
            // Shared pointers are always used with server blocks to make sure they aren't destroyed
            // while a remote download is in progress.
            SharedPointer<ServerBlock> OpenServerBlock();

            // Returns a shared pointer to a server block matching the requested block ID, or nullptr if it does
            // not exist.
            SharedPointer<ServerBlock> GetServerBlock(BlockId serverBlockId);

            // Releases a server block. This prevents new remote transfer requests from succeeding.
            // This will clear the server block pointer inside the shared pointer object.
            void CloseServerBlock(SharedPointer<ServerBlock>& pBlock);

            // Attempts to open a block exposed by a remote client over the message bus.
            // Returns a valid PullBlock pointer on success and nullptr on failure.
            PullBlock* OpenPullBlock(ClientId clientId, BlockId blockId);

            // Closes a pull block and deletes the underlying resources.
            // This will null out the pull block pointer that is passed in as ppBlock.
            void ClosePullBlock(PullBlock** ppBlock);

            // Attempts to open a block exposed by a remote client over the message bus.
            // Returns a valid PushBlock pointer on success and nullptr on failure.
            PushBlock* OpenPushBlock(ClientId clientId, BlockId blockId, size_t blockSize);

            // Closes a push block and deletes the underlying resources.
            // This will null out the push block pointer that is passed in as ppBlock.
            void ClosePushBlock(PushBlock** ppBlock);

            // Backwards compatibility - replaced by OpenServerBlock()
            SharedPointer<ServerBlock> AcquireLocalBlock()
            {
                return OpenServerBlock();
            }

            // Backwards compatibility - replaced by CloseServerBlock()
            void ReleaseLocalBlock(SharedPointer<ServerBlock>& pBlock)
            {
                CloseServerBlock(pBlock);
            }

        private:
            IMsgChannel*     m_pMessageChannel;
            SessionManager*  m_pSessionManager;
            TransferServer*  m_pTransferServer;
            AllocCb          m_allocCb;
            Platform::Random m_rng;
            Platform::Mutex  m_mutex;

            // A list of all the server blocks that are currently available to the TransferManager.
            HashMap<BlockId, SharedPointer<ServerBlock>, 16> m_registeredServerBlocks;
        };
    } // TransferProtocol
} // DevDriver
