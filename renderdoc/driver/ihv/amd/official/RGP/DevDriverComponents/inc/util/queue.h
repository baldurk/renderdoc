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
* @file  queue.h
* @brief Templated double ended queue class for gpuopen
***********************************************************************************************************************
*/

#pragma once

#include "ddPlatform.h"
#include "template.h"
#include <string.h>

namespace DevDriver
{
    template <typename T, size_t BlockSize = 8, size_t MinIndexCacheSize = 8>
    class Queue
    {
    public:
        class Iterator;

        // Constructor - initialize with no storage
        explicit constexpr Queue(const AllocCb& allocCb)
            : m_pBlockIndexCache(nullptr)
            , m_numBlocks(0)
            , m_offset(0)
            , m_size(0)
            , m_allocCb(allocCb)
        {
        }

        // Move constructor - exchange all key data with rhs
        Queue(Queue&& rhs)
            : m_pBlockIndexCache(Platform::Exchange(rhs.m_pBlockIndexCache, nullptr))
            , m_numBlocks(Platform::Exchange(rhs.m_numBlocks, 0))
            , m_offset(Platform::Exchange(rhs.m_offset, 0))
            , m_size(Platform::Exchange(rhs.m_size, 0))
            , m_allocCb(rhs.m_allocCb)
        {
        }

        // Destructor - destroy everything than quite
        ~Queue()
        {
            Clear();
        }

        // Copy elision assignment operator. The compiler will decide whether to initialize the object via the move
        // or copy constructor. This borders on "compiler magic" but is (alarmingly) valid since it relies on the fact
        // that the compiler is able to generate a temporary copy of the parameter, then swaps contents of the current
        // object with those from the temporary copy. The temporary copy is then destroyed, effectively destroying the
        // the existing instance. Where things get weird is that the compiler, if the parameter is bound to an rvalue,
        // is allowed to use it directly without making a temporary copy. This allows the compiler to simply treat the
        // assignment as a swap, as opposed to having to copy all resources. The compiler is also allowed to do other
        // optimizations based on context, so the actual behavior of this operator is somewhat squirrel. It does,
        // however, allow implementing both the copy and move assignment operators with a single operator, relying
        // on the compiler to pick a contextually appropriate behavior.
        Queue& operator=(Queue rhs)
        {
            Swap(rhs);
            return *this;
        }

        //
        // subscript operators
        //
        T& operator[] (size_t index)
        {
            return _PeekIndex(index);
        }

        const T& operator[] (size_t index) const
        {
            return _PeekIndex(index);
        }

        // return the current size
        size_t Size() const {
            return m_size;
        }

        // return the current capacity
        size_t Capacity() const {
            return kPaddedBlockSize * m_numBlocks;
        }

        // returns true if empty
        bool IsEmpty() const {
            return (m_size == 0);
        }

        //
        // Insertion methods
        //

        // Inserts the provided element at the front of the queue.
        // Uses a variadic template parameter to allow construction of an object straight from parameters.
        template <class... Args>
        bool PushFront(Args&&... args)
        {
            T* pData = _AllocateFront();
            if (pData != nullptr)
            {
                new(pData) T(Platform::Forward<Args>(args)...);
            }
            return (pData != nullptr);
        }

        // Inserts the provided element at the end of the queue.
        // Uses a variadic template parameter to allow construction of an object straight from parameters.
        template <class... Args>
        bool PushBack(Args&&... args)
        {
            T* pData = _AllocateBack();
            if (pData != nullptr)
            {
                new(pData) T(Platform::Forward<Args>(args)...);
            }
            return (pData != nullptr);
        }

        // Allocates memory for a new element at the front of the queue and returns a pointer.
        // This method is intended to be the fastest way possible to allocate memory, under the assumption that
        // the user will do either a memcpy or in place construction on it. Given that there is no way to ensure
        // that the user actually constructs an object in place we limit this method to POD types only.
        template<typename U = T, typename = typename Platform::EnableIf<Platform::IsPod<U>::Value>::Type>
        U* AllocateFront()
        {
            return _AllocateFront();
        }

        // Allocates memory for a new element at the back of the queue and returns a pointer.
        // This method is intended to be the fastest way possible to allocate memory, under the assumption that
        // the user will do either a memcpy or in place construction on it. Given that there is no way to ensure
        // that the user actually constructs an object in place we limit this method to POD types only.
        template<typename U = T, typename = typename Platform::EnableIf<Platform::IsPod<U>::Value>::Type>
        U* AllocateBack()
        {
            return _AllocateBack();
        }

        //
        // Removal methods
        //

        // Pop element from the front of the stack
        bool PopFront()
        {
            bool result = !IsEmpty();
            if (result)
            {
                // If the object is not a POD, destroy the instance before we deallocate it.
                if (!Platform::IsPod<T>::Value)
                {
                    _Front().~T();
                }
                // Update state so that it is not part of the queue anymore.
                _PopFront();
            }
            return result;
        }

        bool PopFront(T& output)
        {
            bool result = !IsEmpty();
            if (result)
            {
                // Move object out of the queue into object provided.
                output = Platform::Move(_Front());
                // Update state so that it is not part of the queue anymore.
                // If the object is not a POD, destroy the instance before we deallocate it.
                if (!Platform::IsPod<T>::Value)
                {
                    _Front().~T();
                }
                _PopFront();
            }
            return result;
        }

        // Pop element from the back
        bool PopBack()
        {
            bool result = !IsEmpty();
            if (result)
            {
                // If the object is not a POD, destroy the instance before we deallocate it.
                if (!Platform::IsPod<T>::Value)
                {
                    _Back().~T();
                }
                // Update state so that it is not part of the queue anymore.
                _PopBack();
            }
            return result;
        }

        bool PopBack(T& output)
        {
            bool result = !IsEmpty();
            if (result)
            {
                // Move object out of the queue into object provided.
                output = Platform::Move(_Back());

                // If the object is not a POD, destroy the instance before we deallocate it.
                if (!Platform::IsPod<T>::Value)
                {
                    _Back().~T();
                }

                // Update state so that it is not part of the queue anymore.
                _PopBack();
            }
            return result;
        }

        //
        // Deprecated functions
        //
        bool PopFront(T* output)
        {
            if (output != nullptr)
                return PopFront(*output);
            else
                return PopFront();
        }

        bool PopBack(T* output)
        {
            if (output != nullptr)
                return PopBack(*output);
            else
                return PopBack();
        }

        //
        // Maintenance methods
        //

        // Peek functions
        T* PeekBack() const
        {
            return (!IsEmpty())? &_Back() : nullptr;
        }

        T* PeekFront() const
        {
            return (!IsEmpty()) ? &_Front() : nullptr;
        }

        // Free all memory
        void Clear()
        {
            if (m_pBlockIndexCache != nullptr)
            {
                Reset();

                for (size_t i = 0; i < m_numBlocks; i++)
                {
                    const QueueBlockPtr& pBlock = m_pBlockIndexCache[i];
                    if (pBlock != nullptr)
                    {
                        DD_PRINT(LogLevel::Never, "[Queue::Clear] Freeing block at 0x%X", pBlock);
                        DD_FREE(pBlock, m_allocCb);
                    }
                }

                DD_FREE(m_pBlockIndexCache, m_allocCb);
                m_pBlockIndexCache = nullptr;
            }

            m_numBlocks = 0;
        }

        // Clears all objects stored, but doesn't free memory.
        void Reset()
        {

            while (!IsEmpty())
            {
                // If the object is not a POD, destroy the instance before we deallocate it.
                if (!Platform::IsPod<T>::Value)
                {
                    _Front().~T();
                }
                // Update state so that it is not part of the queue anymore.
                _PopFront();
            }
        }

        // Swap two queues
        void Swap(Queue& otherQueue)
        {
            m_pBlockIndexCache = Platform::Exchange(otherQueue.m_pBlockIndexCache, m_pBlockIndexCache);
            m_numBlocks = Platform::Exchange(otherQueue.m_numBlocks, m_numBlocks);
            m_offset = Platform::Exchange(otherQueue.m_offset, m_offset);
            m_size = Platform::Exchange(otherQueue.m_size, m_size);
            m_allocCb = Platform::Exchange(otherQueue.m_allocCb, m_allocCb);
        }

        // Iterator creation function
        Iterator Begin() const
        {
            return CreateIterator(0);
        }

        // Iterator creation function
        constexpr Iterator End() const
        {
            return Iterator(nullptr, 0);
        }

        // Iterator creation function
        Iterator CreateIterator(size_t index) const
        {
            if (index < m_size)
                return Iterator(this, index);
            return End();
        }

        // Finds the first index for the provided object
        Iterator Find(const T& object) const
        {
            auto it = Begin();
            for (; it != End(); ++it)
            {
                if (*it == object)
                {
                    break;
                }
            }
            return it;
        }
    private:
        DD_STATIC_CONST size_t kPaddedBlockSize = Platform::ConstPow2Pad(BlockSize);
        // private data structure
        struct QueueBlock
        {
            T data[kPaddedBlockSize];
        };

        using QueueBlockPtr = QueueBlock*;

        QueueBlockPtr* m_pBlockIndexCache;
        size_t m_numBlocks;
        size_t m_offset;
        size_t m_size;
        AllocCb m_allocCb;

        // Private copy constructor to prevent implicit copies
        Queue(const Queue& rhs)
            : Queue(rhs.allocCb)
        {
            GrowBlocks(rhs.m_numBlocks);
            for (int i = 0; i < rhs.m_size; i++)
            {
                *_AllocateBack() = rhs[i];
            }
        }

        // Helper function to calculate the pBlockIndex index for the given index
        size_t BlockForOffset(size_t offset) const
        {
            return ((offset / kPaddedBlockSize) & (m_numBlocks - 1));
        }

        // Helper function to calculate the index within a pBlockIndex for a given offset
        size_t IndexForOffset(size_t offset) const
        {
            return ((offset % kPaddedBlockSize) & (kPaddedBlockSize - 1));
        }

        // Helper function to grow internal allocation by numBlocks blocks
        void GrowBlocks(size_t numBlocks)
        {
            DD_STATIC_CONST size_t kPaddedCacheSize = Platform::ConstPow2Pad(MinIndexCacheSize);

            // Ensure the index offset is within the current capacity before resizing. While we treat the offset as
            // a modulus cycle and otherwise allow this behavior, changing the capacity effectively changes the size
            // of the modulus cycle and can invalidate offsets that are > Capacity(). Using a bitmask here only works
            // because Capacity() is guaranteed to be a power of two.
            m_offset &= (Capacity() - 1);

            // Calculate the new capacity as the next power of 2 above current number of blocks + numBlocks
            const size_t newCapacity = Platform::Pow2Pad(Platform::Max(m_numBlocks + numBlocks, kPaddedCacheSize));

            // Allocate a new pBlockIndex index cache
            QueueBlockPtr* pNewIndexCache =
                reinterpret_cast<QueueBlockPtr*>(DD_CALLOC(newCapacity * sizeof(QueueBlockPtr),
                                                           alignof(QueueBlockPtr),
                                                           m_allocCb));
            if (pNewIndexCache != nullptr)
            {

                // If we had previously allocated a cache
                if (m_numBlocks > 0)
                {
                    // Calculate the pBlockIndex index for the current head of the queue
                    const size_t blockIndex = BlockForOffset(m_offset);

                    // Calculate distance between the head and the end of the existing index cache
                    const size_t lengthToEnd = (m_numBlocks - blockIndex);

                    QueueBlockPtr* basePtr = &pNewIndexCache[blockIndex];

                    // Copy pointers from head to the end of the existing allocation + increment pointer
                    memcpy(basePtr, &m_pBlockIndexCache[blockIndex], sizeof(QueueBlockPtr) * lengthToEnd);
                    basePtr += lengthToEnd;

                    // Calculate difference in new capacity and previous capacity
                    const size_t count = newCapacity - m_numBlocks;

                    // If there is enough room we copy objects from the beginning to the head after the tail
                    if (blockIndex <= count)
                    {
                        memcpy(basePtr, &m_pBlockIndexCache[0], sizeof(QueueBlockPtr) * blockIndex);
                        basePtr += blockIndex;
                    }
                    else
                    {
                        // Otherwise we need to split it into two memcpy's - the number that can fit at the end
                        // and the number that will have to go at the beginning
                        memcpy(basePtr, &m_pBlockIndexCache[0], sizeof(QueueBlockPtr) * count);
                        basePtr += count;
                        memcpy(&pNewIndexCache[0], &m_pBlockIndexCache[count], sizeof(QueueBlockPtr) * (blockIndex - count));
                    }
                    // Free the original
                    DD_FREE(m_pBlockIndexCache, m_allocCb);
                }
                // Update the cache and the capacity
                m_pBlockIndexCache = pNewIndexCache;
                m_numBlocks = newCapacity;
            }
        }

        // Helper function to allocate memory for an object at the end of the queue
        T* _AllocateBack()
        {
            T* pResult = nullptr;
            const size_t newOffset = m_offset + m_size;
            const size_t indexOffset = IndexForOffset(newOffset);

            // Check to see if the tail is at a pBlockIndex edge + grow as necessary
            if ((indexOffset == 0)
                & ((m_size + kPaddedBlockSize) >= Capacity()))
            {
                GrowBlocks(1);
            }

            // Only allocate if we have enough space
            if ((m_size + 1) <= Capacity())
            {
                // Calculate the new capacity
                const size_t blockOffset = BlockForOffset(newOffset);
                DD_ASSERT(blockOffset < m_numBlocks);
                QueueBlockPtr& pBlockIndex = m_pBlockIndexCache[blockOffset];

                // If the pBlockIndex hasn't been allocated yet, allocate it
                if (pBlockIndex == nullptr)
                {
                    pBlockIndex =
                        static_cast<QueueBlock*>(DD_MALLOC(sizeof(QueueBlock), alignof(QueueBlock), m_allocCb));
                    DD_PRINT(LogLevel::Never, "[Queue::AllocateBack] Allocated block at 0x%X", pBlockIndex);
                }

                // If the pBlockIndex has been allocated then get it's address + increment the size
                if (pBlockIndex != nullptr)
                {
                    DD_ASSERT(indexOffset < kPaddedBlockSize);
                    pResult = &pBlockIndex->data[indexOffset];
                    m_size++;
                }
            }
            return pResult;
        }

        // Helper function to allocate memory for an object at the beginning of the queue
        T* _AllocateFront()
        {
            T* pResult = nullptr;

            // Check to see if the head is at a pBlockIndex edge + grow as necessary
            if ((IndexForOffset(m_offset) == 0)
                & ((m_size + kPaddedBlockSize) >= Capacity()))
            {
                GrowBlocks(1);
            }

            const size_t capacity = Capacity();
            // Only allocate if we have enough space
            if ((m_size + 1) <= capacity)
            {
                // We need to wrap the index around if necessary
                const size_t newOffset = (m_offset != 0 ? m_offset : capacity) - 1;

                // Calculate the pBlockIndex index
                const size_t blockOffset = BlockForOffset(newOffset);
                DD_ASSERT(blockOffset < m_numBlocks);
                QueueBlockPtr& pBlockIndex = m_pBlockIndexCache[blockOffset];

                // If the pBlockIndex hasn't been allocated yet, allocate it
                if (pBlockIndex == nullptr)
                {
                    pBlockIndex =
                        static_cast<QueueBlock*>(DD_MALLOC(sizeof(QueueBlock), alignof(QueueBlock), m_allocCb));
                    DD_PRINT(LogLevel::Never, "[Queue::AllocateFront] Allocated block at 0x%X", pBlockIndex);
                }

                // If the pBlockIndex has been allocated then get it's address + increment the size
                if (pBlockIndex != nullptr)
                {
                    m_size++;
                    m_offset = newOffset;
                    const size_t indexOffset = IndexForOffset(newOffset);
                    DD_ASSERT(indexOffset < kPaddedBlockSize);
                    pResult = &pBlockIndex->data[indexOffset];
                }
            }
            return pResult;
        }

        //
        // EXTREMELY DANGEROUS INTERNAL CALLS
        //
        // These have no safety checking as they are assumed to only be used internally
        //

        // Returns a reference to the object at the specified index.
        // This *will* dereference an invalid pointer if you give it an invalid index.
        T& _PeekIndex(size_t offset) const
        {
            const size_t index = m_offset + offset;
            const size_t blockOffset = BlockForOffset(index);
            DD_ASSERT(blockOffset < m_numBlocks);
            const size_t indexOffset = IndexForOffset(index);
            DD_ASSERT(indexOffset < kPaddedBlockSize);

            return m_pBlockIndexCache[blockOffset]->data[indexOffset];
        }

        // Returns a reference to the object at the back of the queue.
        // This *will* dereference an invalid pointer if there are no objects in the queue.
        T& _Back()  const
        {
            return _PeekIndex(m_size - 1);
        }

        // Returns a reference to the object at the front of the queue.
        // This *will* dereference an invalid pointer if there are no objects in the queue.
        T& _Front() const
        {
            return _PeekIndex(0);
        }

        // Updates state so as to remove the last item in the queue. This does not actually destroy the object,
        // and should only be called after that has occured.
        // This *will* put the queue in an invalid state if called while the queue is empty.
        void _PopBack()
        {
            DD_ASSERT(!IsEmpty());

            if ((--m_size) == 0)
            {
                m_offset = 0;
            }
        }

        // Updates state so as to remove the first item in the queue. This does not actually destroy the object,
        // and should only be called after that has occured.
        // This *will* put the queue in an invalid state if called while the queue is empty.
        void _PopFront()
        {
            DD_ASSERT(!IsEmpty());

            m_offset++;

            if ((--m_size) == 0)
            {
                m_offset = 0;
            }
        }
    };

    // Iterator class for the Queue type
    template <typename T, size_t BlockSize, size_t MinIndexCacheSize>
    class Queue<T, BlockSize, MinIndexCacheSize>::Iterator
    {
        friend Queue;
    public:
        // Comparison operator
        bool operator!=(const Iterator& rhs) const
        {
            return ((m_pContainer != rhs.m_pContainer) | (m_index != rhs.m_index));
        }

        // Prefix operator to increment the iterator
        Iterator& operator++()
        {
            if (m_pContainer != nullptr)
            {
                m_index += 1;
                if (m_index >= m_pContainer->m_size)
                {
                    m_index = 0;
                    m_pContainer = nullptr;
                }
            }
            return *this;
        }

        // Indirection operator
        T& operator*() const
        {
            DD_ASSERT(m_pContainer != nullptr);
            return m_pContainer->_PeekIndex(m_index);
        }

        // Member of pointer operator. Returns a pointer to the object in the shared container.
        T* operator->() const
        {
            DD_ASSERT(m_pContainer != nullptr);
            return &m_pContainer->_PeekIndex(m_index);
        }
    private:
        // Private constructor so that an object cannot be created by the user
        Iterator(const Queue* pContainer, size_t index) :
            m_pContainer(pContainer),
            m_index(index)
        {
        };

        const Queue* m_pContainer;
        size_t m_index;
    };

    //
    // functions necessary for C++ ranged based for loop support
    //

    // Implement begin() function for range-based for loops
    template <typename T, size_t BlockSize, size_t MinIndexCacheSize>
    inline typename Queue<T, BlockSize, MinIndexCacheSize>::Iterator
        begin(Queue<T, BlockSize, MinIndexCacheSize>& rhs)
    {
        return rhs.Begin();
    }

    // Implement end() function for range-based for loops
    template <typename T, size_t BlockSize, size_t MinIndexCacheSize>
    inline constexpr typename Queue<T, BlockSize, MinIndexCacheSize>::Iterator
        end(const Queue<T, BlockSize, MinIndexCacheSize>& rhs)
    {
        return rhs.End();
    }
} // DevDriver
