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
  * @file  hashBase.h
  * @brief Templated class to support common hash table functions.
  ***********************************************************************************************************************
  */

#pragma once

#include "gpuopen.h"
#include "ddPlatform.h"
#include "memory.h"
#include "hashFunc.h"
#include <cstring>

namespace DevDriver
{
    // For the i-th block, it will hold Pow(2,i) groups, the whole array could have 4G groups.
    DD_STATIC_CONST int32 kNumHashBlocks = 32; // Maximum number of blocks the container can implement

    /**
     ***********************************************************************************************************************
     * @brief Templated base class for HashMap and HashSet, supporting the ability to store, find, and remove entries.
     *
     * The hash container has a fixed number of buckets.  These buckets contain a growable number of entry groups.  Each
     * entry group contains a fixed number of entries and a pointer to the next entry group in the bucket.
     *
     * This class aims to be very efficient when looking up the key and storing small attached items is the primary concern.
     * It's therefore not desired to have the key associated with a pointer to the attached data, because the attached data
     * may be of similar or smaller size than the pointer anyway, it would also introduce much unnecessary memory
     * management, and it would imply a minimum of two cache misses in the typical lookup case.
     *
     * The idea is that these enty groups can be exactly the size of a cache line, so an entry group can be scanned with
     * only a single cache miss.  This extends the load factor that the hash-map can manage before performance begins to
     * degrade.  For the very small items that we expect, this should be a significant advantage; we expect one cache miss
     * pretty much always, so packing the items together would not be a significant gain, and the cost in memory usage is
     * (relatively) small.
     ***********************************************************************************************************************
     */
    template<
        typename Key,
        typename Entry,
        typename HashFunc,
        typename EqualFunc,
        size_t NumBuckets,
        size_t MinimumBucketSize = DD_CACHE_LINE_BYTES * 2>
        class HashBase
    {
    public:
        /// Returns number of entries in the container.
        size_t Size() const { return m_numEntries; }

        /// Empty the hash container without freeing the underlying allocations.
        void Reset()
        {
            if (m_curBlock >= 0)
            {
                // Reset the bucket head pointers
                memset(&m_buckets[0], 0, sizeof(m_buckets));

                // We need to destroy all object instances inside the hash map, so we iterate over all blocks
                for (int32 i = 0; i <= m_curBlock; ++i)
                {
                    MemBlock& block = m_blocks[i];

                    DD_ASSERT(block.pMemory != nullptr);

                    // The number of buckets depends on which block it is
                    for (int32 j = 0; j < Platform::Pow2(i); j++)
                    {
                        // For every bucket in the block we need to destroy all object instances
                        Bucket& currentBucket = block.pMemory[j];
                        if (!Platform::IsPod<Bucket>::Value)
                        {
                            for (uint32 k = 0; k < currentBucket.footer.numEntries; k++)
                            {
                                currentBucket.entries[k].~Entry();
                            }
                        }

                        // We then overwrite the footer
                        currentBucket.footer = Footer();

                        // Ensure that the footer was correctly zero initialized
                        DD_ASSERT(currentBucket.footer.pNextBucket == nullptr);
                        DD_ASSERT(currentBucket.footer.numEntries == 0);
                    }

                    block.curBucket = 0;
                }

                // Reset the object count
                m_numEntries = 0;
                m_curBlock = -1;
            }
        }

        /// Empty the hash container and dispose of all underlying allocations.
        void Clear()
        {
            if (m_curBlock >= 0)
            {
                // Reset the bucket head pointers
                memset(&m_buckets[0], 0, sizeof(m_buckets));

                // Deallocate all blocks that have been allocated
                for (int32 i = 0; i <= m_curBlock; ++i)
                {
                    MemBlock& block = m_blocks[i];
                    if (block.pMemory != nullptr)
                    {
                        // If this was not a POD type we want to explicitly destroy the object
                        if (!Platform::IsPod<Bucket>::Value)
                        {
                            for (int32 j = 0; j < Platform::Pow2(i); j++)
                            {
                                Bucket& currentBucket = block.pMemory[j];
                                for (uint32 k = 0; k < currentBucket.footer.numEntries; k++)
                                {
                                    currentBucket.entries[k].~Entry();
                                }
                            }
                        }

                        DD_FREE(block.pMemory, m_allocCb);
                        block.pMemory = nullptr;
                        block.curBucket = 0;
                    }
                }

                // Reset the object count
                m_numEntries = 0;
                m_curBlock = -1;
            }
        }

        /// Returns true if the specified key exists in the set.
        ///
        /// @param [in] key Key to search for.
        ///
        /// @returns True if the specified key exists in the set.
        bool Contains(const Key& key) const
        {
            // Get the bucket base address.
            const Bucket* pBucket = FindBucket(key);
            const Entry* pMatchingEntry = nullptr;

            while ((pBucket != nullptr) && (pMatchingEntry == nullptr))
            {
                const Bucket& currentBucket = *pBucket;

                // Search this entry group.
                for (uint32 i = 0; i < currentBucket.footer.numEntries; i++)
                {
                    const Entry& entry = currentBucket.entries[i];
                    if (this->m_equalFunc(entry.key, key))
                    {
                        // We've found the entry.
                        pMatchingEntry = &entry;
                        break;
                    }
                }

                // Chain to the next entry group.
                pBucket = pBucket->footer.pNextBucket;
            }

            return (pMatchingEntry != nullptr);
        }

        /// Removes an entry that matches the specified key.
        ///
        /// @param [in] key Key of the entry to erase.
        ///
        /// @returns True if the erase completed successfully, false if an entry for this key did not exist.
        Result Erase(const Key& key)
        {
            // Get the bucket base address.
            Bucket* pBucket = this->FindBucket(key);
            Entry* pFoundEntry = nullptr;

            // Find the entry to delete
            while (pBucket != nullptr && (pBucket->footer.numEntries > 0))
            {
                Bucket& currentBucket = *pBucket;

                // Search each group
                for (uint32 i = 0; i < currentBucket.footer.numEntries; i++)
                {
                    Entry& entry = currentBucket.entries[i];
                    if (this->m_equalFunc(entry.key, key) == true)
                    {
                        // We shouldn't find the same key twice.
                        DD_ASSERT(pFoundEntry == nullptr);

                        pFoundEntry = &entry;
                        break;
                    }
                }

                if (pFoundEntry != nullptr)
                    break;

                // Chain to the next entry group.
                pBucket = currentBucket.footer.pNextBucket;
            }

            // Copy the last entry's data into the entry that we are removing and invalidate the last entry as it now appears
            // earlier in the list.  This also handles the case where the entry to be removed is the last entry.

            if (pFoundEntry != nullptr)
            {
                Entry* pLastEntry = nullptr;
                Bucket* pLastBucket = nullptr;

                while (pBucket != nullptr && (pBucket->footer.numEntries > 0))
                {
                    Bucket& currentBucket = *pBucket;

                    // keep track of last entry of all groups in bucket
                    pLastEntry = &(currentBucket.entries[currentBucket.footer.numEntries - 1]);
                    pLastBucket = pBucket;

                    // Chain to the next entry group.
                    pBucket = currentBucket.footer.pNextBucket;
                }

                DD_ASSERT(pLastEntry != nullptr);

                // If this item isn't the last entry, then we move the last entry into it.
                if (pFoundEntry != pLastEntry)
                {
                    *pFoundEntry = Platform::Move(*pLastEntry);
                }

                // If it wasn't a POD type we need to explicitly invoke the destructor on the last entry
                if (!Platform::IsPod<Entry>::Value)
                {
                    pLastEntry->~Entry();
                }

                DD_ASSERT(this->m_numEntries > 0);
                this->m_numEntries--;
                pLastBucket->footer.numEntries--;
            }
            return (pFoundEntry != nullptr) ? Result::Success : Result::Error;
        }
    protected:
        class BaseIterator;

        /// @internal Constructor
        ///
        /// @param [in] numBuckets Number of buckets to allocate for this hash container.  The initial hash container
        ///                         will take (buckets * DD_CACHE_LINE_BYTES) bytes.
        /// @param [in] pAllocator The allocator that will allocate memory if required.
        explicit HashBase(const AllocCb& allocCb) :
            m_hashFunc(HashFunc(Platform::ConstLog2(kPaddedNumBuckets))),
            m_equalFunc(EqualFunc()),
            m_allocCb(allocCb),
            m_numEntries(0),
            m_blocks(),
            m_curBlock(-1),
            m_buckets()
        {
        }

        ///// @internal Constructor
        /////
        ///// @param [in] numBuckets Number of buckets to allocate for this hash container.  The initial hash container
        /////                         will take (buckets * DD_CACHE_LINE_BYTES) bytes.
        ///// @param [in] pAllocator The allocator that will allocate memory if required.
        HashBase(HashBase&& rhs) :
            m_hashFunc(HashFunc(Platform::ConstLog2(kPaddedNumBuckets))),
            m_equalFunc(EqualFunc()),
            m_allocCb(rhs.m_allocCb),
            m_numEntries(Platform::Exchange(rhs.m_numEntries, 0)),
            m_curBlock(Platform::Exchange(rhs.m_curBlock, -1))
        {
            // Copy pointers to allocations made by the other container
            memcpy(m_blocks, rhs.m_blocks, sizeof(m_blocks));
            // Reset the object on the right hand side
            memset(rhs.m_blocks, 0, sizeof(m_blocks));

            // Copy the pointers from the right hand container
            memcpy(m_buckets, rhs.m_buckets, sizeof(m_buckets));
            // Reset the object on the right hand side
            memset(rhs.m_buckets, 0, sizeof(m_buckets));
        }

        virtual ~HashBase()
        {
            Clear();
        }

        // ============================================================================================================
        // Gets a pointer to the value that matches the key.  Returns null if no entry is present matching the
        // specified key.
        Entry* FindEntry(const Key& key) const
        {
            // Get the bucket base address.
            Bucket* pBucket = this->FindBucket(key);
            Entry* pMatchingEntry = nullptr;

            while (pBucket != nullptr)
            {
                Bucket& currentBucket = *pBucket;

                // Search this entry group
                for (uint32 i = 0; i < currentBucket.footer.numEntries; i++)
                {
                    Entry& entry = currentBucket.entries[i];

                    if (this->m_equalFunc(entry.key, key))
                    {
                        // We've found the entry.
                        pMatchingEntry = &entry;
                        break;
                    }
                }

                if (pMatchingEntry != nullptr)
                {
                    break;
                }

                // Chain to the next entry group.
                pBucket = currentBucket.footer.pNextBucket;
            }

            return pMatchingEntry;
        }

        // ============================================================================================================
        // Gets a BaseIterator to the value that matches the key.  Returns invalid Iterator if the value is not found
        BaseIterator FindIterator(const Key& key) const
        {
            // Get the bucket base address.
            const uint32 bucket = m_hashFunc(&key) & (kPaddedNumBuckets - 1);
            Bucket* pBucket = m_buckets[bucket];

            while (pBucket != nullptr)
            {
                Bucket& currentBucket = *pBucket;

                // Search this entry group
                for (uint32 i = 0; i < currentBucket.footer.numEntries; i++)
                {
                    if (this->m_equalFunc(currentBucket.entries[i].key, key))
                    {
                        // We've found the entry.
                        return BaseIterator(this, bucket, bucket, pBucket, i);
                    }
                }

                // Chain to the next entry group.
                pBucket = currentBucket.footer.pNextBucket;
            }

            return BaseIterator(this, static_cast<uint32>(kPaddedNumBuckets));
        }

        /// Finds a given entry; if no entry was found, allocate it.
        ///
        /// @param [in]  key      Key to search for.
        /// @param [out] pExisted True if an entry for the specified key existed before this call was made.  False
        ///                       indicates that a new entry was allocated as a result of this call.
        /// @returns @ref Readable/writeable value in the hash map corresponding to the specified key.
        Entry* FindOrAllocate(const Key& key,
                              bool*      pExisted = nullptr)  // [out] True if a matching key was found.
        {
            // Get the bucket base address.
            const uint32 bucket = m_hashFunc(&key) & (kPaddedNumBuckets - 1);
            Bucket** ppBucket = &m_buckets[bucket];
            bool existed = false;

            Entry* pMatchingEntry = nullptr;

            while (pMatchingEntry == nullptr)
            {
                Bucket* pBucket = *ppBucket;

                if (pBucket == nullptr)
                {
                    pBucket = AllocateBucket();
                    DD_ASSERT(pBucket != nullptr);

                    *ppBucket = pBucket;
                }

                Bucket& currentBucket = *pBucket;

                // Search this entry group.
                for (uint32 i = 0; i < currentBucket.footer.numEntries; i++)
                {
                    Entry& entry = currentBucket.entries[i];
                    if (this->m_equalFunc(entry.key, key))
                    {
                        // We've found the entry.
                        pMatchingEntry = &entry;
                        existed = true;
                        break;
                    }
                }

                if ((pMatchingEntry == nullptr) & (currentBucket.footer.numEntries < kEntriesInBucket))
                {
                    // We've reached the end of the bucket and the entry was not found.  Allocate this entry for the key.
                    Entry& entry = currentBucket.entries[currentBucket.footer.numEntries];
                    new(&entry.key) Key(key);
                    pMatchingEntry = &entry;
                    currentBucket.footer.numEntries++;
                    this->m_numEntries++;
                }

                if (pMatchingEntry != nullptr)
                {
                    break;
                }

                ppBucket = &currentBucket.footer.pNextBucket;
            }

            if (pExisted != nullptr)
            {
                *pExisted = existed;
            }
            return pMatchingEntry;
        }

        /// Returns the bucket index containing the first element or an invalid bucket index if no buckets have been
        /// allocated yet.
        uint32 GetFirstBucket() const
        {
            uint32 bucket = 0;

            if (m_numEntries != 0)
            {
                for (; bucket < kPaddedNumBuckets; ++bucket)
                {
                    if ((m_buckets[bucket] != nullptr) && (m_buckets[bucket]->footer.numEntries > 0))
                    {
                        break;
                    }
                }
            }
            else
            {
                // If the backing memory does not exist we should return an invalid bucket.
                // This can be done by setting the start bucket such that it is off the end of the bucket list.
                bucket = kPaddedNumBuckets;
            }

            return bucket;
        }

        /// Swaps the contents of this object with those of the object specified as a parameter.
        void Swap(HashBase& other)
        {
            // exchange the stateful information
            m_allocCb = Platform::Exchange(other.m_allocCb, m_allocCb);
            m_numEntries = Platform::Exchange(other.m_numEntries, m_numEntries);
            m_curBlock = Platform::Exchange(other.m_curBlock, m_curBlock);

            // calculate how many blocks need to be swapped
            const int32 numBlocks = Platform::Min(Platform::Max(m_curBlock, other.m_curBlock) + 1, kNumHashBlocks);

            // exchange block pointers
            for (int32 i = 0; i < numBlocks; i++)
            {
                m_blocks[i] = Platform::Exchange(other.m_blocks[i], m_blocks[i]);
            }

            // exchange bucket pointers
            for (size_t i = 0; i < kPaddedNumBuckets; i++)
            {
                m_buckets[i] = Platform::Exchange(other.m_buckets[i], m_buckets[i]);
            }
        }

        /// Removes an entry associated with the provided iterator
        ///
        /// @param [in] iterator BaseIterator created by the current object
        ///
        /// @returns True if the erase completed successfully, false if the iterator was invalid.
        bool RemoveIterator(BaseIterator& iterator)
        {
            Entry* pFoundEntry = nullptr;
            if ((iterator.m_pCurrentBucket != nullptr) & (iterator.m_pContainer == this))
            {
                // Get the bucket base address.
                Bucket* pBucket = iterator.m_pCurrentBucket;

                DD_ASSERT(this->m_numEntries > 0);

                pFoundEntry = iterator.Get();

                DD_ASSERT(pFoundEntry != nullptr);

                // We need to look for the last entry in the current chain. We will then either swap this entry with
                // last entry and destroy the object
                Entry* pLastEntry = nullptr;
                Bucket* pLastBucket = nullptr;

                while (pBucket != nullptr && (pBucket->footer.numEntries > 0))
                {
                    Bucket& currentBucket = *pBucket;

                    // keep track of last entry of all groups in bucket
                    pLastEntry = &(currentBucket.entries[currentBucket.footer.numEntries - 1]);
                    pLastBucket = pBucket;

                    // Chain to the next entry group.
                    pBucket = currentBucket.footer.pNextBucket;
                }

                DD_ASSERT(pLastEntry != nullptr);
                DD_ASSERT(pLastBucket != nullptr);

                this->m_numEntries--;
                pLastBucket->footer.numEntries--;

                if (pFoundEntry != pLastEntry)
                {
                    *pFoundEntry = Platform::Move(*pLastEntry);
                }
                else
                {
                    // This was the last entry in the current bucket, so we need to advance the iterator
                    iterator.Next();
                }

                // If the element we removed wasn't a POD type we need to explicitly invoke the destructor
                if (!Platform::IsPod<Entry>::Value)
                {
                    pLastEntry->~Entry();
                }

            }
            return (pFoundEntry != nullptr);
        }

        DD_STATIC_CONST size_t kPaddedNumBuckets = Platform::ConstPow2Pad(NumBuckets);
    private:
        struct Bucket;
        struct Footer
        {
            Bucket* pNextBucket;
            uint32  numEntries;
        };

        /// The native bucket size is going to be, at minimum, the size of an entry + footer,
        /// aligned to the cache line.
        DD_STATIC_CONST size_t kAlignedBucketSize =
        Platform::Pow2Align(sizeof(Entry) + sizeof(Footer), DD_CACHE_LINE_BYTES);

        /// We pick the larger of the native bucket size and the minimum bucket size to ensure
        /// that we always have enough room for at least one object.
        DD_STATIC_CONST size_t kBucketSize = Platform::Max(kAlignedBucketSize, MinimumBucketSize);

        /// Number of entries in a single group.
        DD_STATIC_CONST uint32 kEntriesInBucket = ((kBucketSize - sizeof(Footer)) / sizeof(Entry));

        /// There must be at least one entry in each group.
        static_assert((kEntriesInBucket >= 1), "Hash container entry is too big.");

        struct Bucket
        {
            Entry  entries[kEntriesInBucket];
            Footer footer;
        };

        static_assert(sizeof(Bucket) <= kBucketSize, "Hash container entry is too big.");

        struct MemBlock
        {
            Bucket* pMemory = nullptr;  // Pointer to the memory allocated for this block.
            int32   curBucket = 0;      // Current group index to be allocated.
        };

        const HashFunc  m_hashFunc;                     ///< @internal Hash functor object.
        const EqualFunc m_equalFunc;                    ///< @internal Key compare function object.
        AllocCb         m_allocCb;                      ///< @internal Allocator for this hash allocation function.

        uint32          m_numEntries;                   ///< @internal Entries in the table.

        MemBlock        m_blocks[kNumHashBlocks];       ///< @internal Memory blocks for memory allocations.
        int32           m_curBlock;                     ///< @internal Current block index buckets can be allocated
                                                        ///< from. -1 indicates no memory has been allocated yet

        Bucket*         m_buckets[kPaddedNumBuckets];   ///< @internal Base address as allocated
                                                        ///< (before alignment).

        /// @internal Finds the bucket that matches the specified key
        ///
        /// @param [in] key Key to find matching bucket for.
        ///
        /// @returns Pointer to the bucket corresponding to the specified key.
        Bucket* FindBucket(const Key& key) const
        {
            const uint32 bucket = m_hashFunc(&key) & (kPaddedNumBuckets - 1);
            return m_buckets[bucket];
        }

        /// Allocates a new block of memory.
        ///
        /// No size parameter, the size of allocation is fixed to the groupSize parameter specified in the constructor.
        ///
        /// @returns A pointer to the allocate memory, or null if the allocation failed.
        Bucket* AllocateBucket()
        {
            Bucket* pMemory = nullptr;

            // Leave pBlock null if this is the first allocation made with this object.
            MemBlock* pBlock = (m_curBlock >= 0) ? &m_blocks[m_curBlock] : nullptr;

            // If current block is used up (or we haven't allocated one yet), go to next.
            if ((pBlock == nullptr) || (pBlock->curBucket >= Platform::Pow2(m_curBlock)))
            {
                // Only advance to the next block if the current one had memory allocated to it (which implies that it's
                // full).
                int32 nextBlock = m_curBlock;

                if ((pBlock == nullptr) || (pBlock->pMemory != nullptr))
                {
                    nextBlock++;
                }

                DD_ASSERT(nextBlock < kNumHashBlocks);

                pBlock = &m_blocks[nextBlock];

                DD_ASSERT(pBlock->curBucket == 0);

                // Allocate memory if needed (note that this may rarely fail)
                if (pBlock->pMemory == nullptr)
                {
                    const size_t numBuckets = static_cast<size_t>(Platform::Pow2(nextBlock));
                    const size_t allocSize = sizeof(Bucket) * numBuckets;
                    pBlock->pMemory = reinterpret_cast<Bucket*>(DD_CALLOC(allocSize, alignof(Bucket), m_allocCb));
                }

                // If we successfully allocated memory (or the block already had some), make it current
                if (pBlock->pMemory != nullptr)
                {
                    m_curBlock = nextBlock;
                }
            }

            if (pBlock->pMemory != nullptr)
            {
                pMemory = &pBlock->pMemory[pBlock->curBucket++];
            }

            return pMemory;
        }
    };

    /**
    ***********************************************************************************************************************
    * @brief  BaseIterator for traversal of elements in a Hash container.
    *
    * Backward iterating is not supported since there is no "footer" or "header" for a hash container.
    ***********************************************************************************************************************
    */
    template<
        typename Key,
        typename Entry,
        typename HashFunc,
        typename EqualFunc,
        size_t   NumBuckets,
        size_t   MinimumBucketSize>
        class HashBase<Key, Entry, HashFunc, EqualFunc, NumBuckets, MinimumBucketSize>::BaseIterator
    {
    protected:
        /// Returns a pointer to current entry.  Will return null if the iterator has been advanced off the end of the
        /// container.
        Entry* Get() const { return &m_pCurrentBucket->entries[m_indexInBucket]; }

        // In order for an iterator to be considered equal it has to have the same:
        //  1) Container
        //  2) Current bucket
        //  3) Index inside the bucket
        // Note: this is not publicly exposed to prevent HashMap and HashSet from ever being directly comparable
        bool Equals(const BaseIterator& rhs) const
        {
            return ((m_pContainer == rhs.m_pContainer) &
                    (m_currentBucket == rhs.m_currentBucket) &
                    (m_indexInBucket == rhs.m_indexInBucket));
        }

        /// Advances the iterator to the next position (move forward).
        void Next()
        {
            if (m_pCurrentBucket != nullptr)
            {
                Bucket* pNextBucket = m_pCurrentBucket->footer.pNextBucket;

                // We're in the middle of a group.
                if ((m_indexInBucket < kEntriesInBucket) &&
                    (m_indexInBucket + 1 < m_pCurrentBucket->footer.numEntries))
                {
                    m_indexInBucket++;
                }
                // We're in the last entry of a group.
                // Considering that the next chained group could be an empty group already, it is better to check the
                // next group's footer->numEntries before jump to the next group. If the numEntry of the next chained
                // group is 0 (invalid), we need to jump to the next bucket directly to avoid returning invalid entry.
                else if ((pNextBucket != nullptr) &&
                         (m_indexInBucket == m_pCurrentBucket->footer.numEntries - 1) &&
                         (pNextBucket->footer.numEntries > 0))
                {
                    m_pCurrentBucket = pNextBucket;
                    m_indexInBucket = 0;
                }
                // The current bucket is done, step to the next.
                else
                {
                    do
                    {
                        m_currentBucket = (m_currentBucket + 1) % kPaddedNumBuckets;

                        pNextBucket = m_pContainer->m_buckets[m_currentBucket];

                        if (pNextBucket != nullptr && pNextBucket->footer.numEntries > 0)
                        {
                            m_indexInBucket = 0;
                            break;
                        }
                    } while (m_currentBucket != m_startBucket);

                    if (m_currentBucket != m_startBucket)
                    {
                        m_pCurrentBucket = pNextBucket;
                        m_indexInBucket = 0;
                    }
                    else
                    {
                        m_currentBucket = kPaddedNumBuckets;
                        m_pCurrentBucket = nullptr;
                        m_indexInBucket = 0;
                    }
                }
            }
        }

        // Constructor. BaseIterator objects should never be directly created by anything.
        BaseIterator(
            const HashBase*   pContainer,   ///< [retained] The hash container to iterate over
            uint32            startBucket)  ///< The beginning bucket
            :
            m_pContainer(pContainer),
            m_startBucket(startBucket),
            m_currentBucket(startBucket),
            m_pCurrentBucket(nullptr),
            m_indexInBucket(0)
        {
            if (startBucket < kPaddedNumBuckets)
            {
                m_pCurrentBucket = m_pContainer->m_buckets[m_startBucket];
            }
        }

        // Constructor that allows returning an Iterator to a specific object.
        // BaseIterator objects should never be directly created by anything.
        constexpr BaseIterator(
            const HashBase*   pContainer,   ///< [retained] The hash container to iterate over
            uint32            startBucket,
            uint32            currentBucket,
            Bucket*           pCurrentBucket,
            uint32            bucketIndex)  ///< The beginning bucket
            :
            m_pContainer(pContainer),
            m_startBucket(startBucket),
            m_currentBucket(currentBucket),
            m_pCurrentBucket(pCurrentBucket),
            m_indexInBucket(bucketIndex)
        {
        }

        const HashBase* m_pContainer;     // Hash container that we're iterating over.
        uint32          m_startBucket;    // Bucket where we start iterating.
        uint32          m_currentBucket;  // Current bucket we're iterating.
        Bucket*         m_pCurrentBucket;  // Current group we're iterating (belongs to the current bucket).
        uint32          m_indexInBucket;   // Index of current entry in the group.

        // Although this is a transgression of coding standards, it means that Container does not need to have a public
        // interface specifically to implement this class. The added encapsulation this provides is worthwhile.
        friend HashBase<Key, Entry, HashFunc, EqualFunc, NumBuckets>;
    };
}
