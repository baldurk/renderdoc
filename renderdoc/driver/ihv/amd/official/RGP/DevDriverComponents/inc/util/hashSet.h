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
  *********************************************************************************************************************
  * @file  hashSet.h
  * @brief Templated class implementing set semantics
  *********************************************************************************************************************
  */

#pragma once

#include "hashBase.h"

namespace DevDriver
{

    /// Encapsulates one entry of a hash set.
    template<typename Key>
    struct HashSetEntry
    {
        Key key;  ///< Hash set entry key.
    };

    /**
     ******************************************************************************************************************
     * @brief Templated hash set container.
     *
     * This is meant for storing elements of an arbitrary (but uniform) key type. Supported operations:
     *
     * - Searching
     * - Insertion
     * - Deletion
     * - Iteration
     *
     * HashFunc is a functor for hashing keys.  Built-in choices for HashFunc are:
     *
     * - DefaultHashFunc: Good choice when the key is a pointer.
     * - JenkinsHashFunc: Good choice when the key is arbitrary binary data.
     * - StringJenkinsHashFunc: Good choice when the key is a C-style string.
     *
     * EqualFunc is a functor for comparing keys.  Built-in choices for EqualFunc are:
     *
     * - DefaultEqualFunc: Determines keys are equal using teh equality operator.
     * - BitwiseEqualFunc: Determines keys are equal by bitwise comparison.
     * - StringEqualFunc: Treats keys as a char* and compares them as C-style strings.
     *
     * @warning This class is not thread-safe for Insert, Erase, or iteration!
     * @warning Init() must be called before using this container. Begin() and Reset() can be safely called before
     *          initialization and Begin() will always return an iterator that points to null.
     *
     * For more details please refer to @ref HashBase.
     ***********************************************************************************************************************
     */
    template<typename Key,
        size_t NumBuckets = (sizeof(Key) * 8),
        template<typename> class HashFunc = DefaultHashFunc,
        template<typename> class EqualFunc = DefaultEqualFunc>
        class HashSet : public HashBase<Key,
        HashSetEntry<Key>,
        HashFunc<Key>,
        EqualFunc<Key>,
        NumBuckets>
    {
    public:
        // Forward declare Iterator class
        class Iterator;

        /// Convenience typedef for a templated entry of this hash set.
        typedef HashSetEntry<Key> Entry;

        /// @internal Constructor
        ///
        /// @param [in] allocCb Allocator callback struct used to allocate memory
        explicit constexpr HashSet(const AllocCb& allocCb) :
            Base::HashBase(allocCb)
        {
        }

        /// @internal Move Constructor
        ///
        /// @param [in] other HashSet object to move data from
        HashSet(HashSet&& other) :
            Base::HashBase(Platform::Forward<HashSet>(other))
        {
        }

        /// @internal Destructor
        ///
        ~HashSet()
        {
        }

        /// @internal Unifying copy operator
        ///
        /// Copy elision assignment operator. The compiler will decide whether to initialize the object via the move
        /// or copy constructor. This borders on "compiler magic" but is (alarmingly) valid since it relies on the fact
        /// that the compiler is able to generate a temporary copy of the parameter, then swaps contents of the current
        /// object with those from the temporary copy. The temporary copy is then destroyed, effectively destroying the
        /// the existing instance. Where things get weird is that the compiler, if the parameter is bound to an rvalue,
        /// is allowed to use it directly without making a temporary copy. This allows the compiler to simply treat the
        /// assignment as a swap, as opposed to having to copy all resources. The compiler is also allowed to do other
        /// optimizations based on context, so the actual behavior of this operator is somewhat squirrel. It does,
        /// however, allow implementing both the copy and move assignment operators with a single operator, relying
        /// on the compiler to pick a contextually appropriate behavior.
        HashSet& operator=(HashSet rhs)
        {
            Base::Swap(rhs);
            return *this;
        }

        /// Swaps the contents of who HashSet objects
        ///
        /// @param [in] other HashSet to swap contents with
        void Swap(HashSet& other)
        {
            Base::Swap(other);
        }

        /// Inserts an entry.
        ///
        /// No action will be taken if an entry matching this key already exists in the set.
        ///
        /// @param [in] key New entry to insert.
        ///
        /// @returns @ref Success if the operation completed successfully, or @ref InsufficientMemory if the operation
        ///          failed because an internal memory allocation failed.
        Result Insert(const Key& key)
        {
            const Entry* pEntry = Base::FindOrAllocate(key);
            return (pEntry != nullptr) ? Result::Success : Result::InsufficientMemory;
        }

        /// Finds and entry and returns an iterator to it
        ///
        /// @param [in] key key to look up in the HashSet
        ///
        /// @returns Iterator to the element, or Iterator equal to End().
        ///          Warning: this iterator is not guaranteed to be able to iterate across the entire HashMap.
        Iterator Find(const Key& key) const
        {
            return Iterator(Base::FindIterator(key));
        }

        /// Removes an entry using the provided iterator
        ///
        /// @param [in] iterator Iterator for the entry to be removed.
        ///
        /// @returns Iterator to the next element in the set
        Iterator Remove(const Iterator& iterator)
        {
            Iterator result = iterator;
            const bool didRemove = Base::RemoveIterator(result);
            DD_ASSERT(didRemove);
            DD_UNUSED(didRemove);
            return result;
        }

        // Creates a new iterator representing the beginning of the HashMap
        Iterator Begin() const
        {
            return Iterator(this, Base::GetFirstBucket());
        }

        // Creates a new iterator representing the end of the HashMap
        constexpr Iterator End() const
        {
            return Iterator(this, static_cast<uint32>(Base::kPaddedNumBuckets));
        }
    private:
        // Typedef for the specialized 'HashBase' object we're inheriting from so we can use properly qualified names
        //  when accessing members of HashBase.
        typedef HashBase<Key, HashSetEntry<Key>, HashFunc<Key>, EqualFunc<Key>, NumBuckets> Base;
    };

    template<typename Key,
        size_t NumBuckets,
        template<typename> class HashFunc,
        template<typename> class EqualFunc>
        class HashSet<Key, NumBuckets, HashFunc, EqualFunc>::Iterator : protected Base::BaseIterator
    {
        friend HashSet;
    public:
        // Use base comparison method to implement !=
        bool operator!=(const Iterator &rhs) const
        {
            return !Base::BaseIterator::Equals(rhs);
        }

        // Use base increment method to implement prefix operator
        Iterator& operator++()
        {
            Base::BaseIterator::Next();
            return *this;
        }

        // Use base access method to implement indirection operator
        Key& operator*() const
        {
            DD_ASSERT(Base::BaseIterator::m_pContainer != nullptr);
            return Base::BaseIterator::Get()->key;
        }

        // Use base access method to implement member of pointer operator
        Key* operator->() const
        {
            DD_ASSERT(Base::BaseIterator::m_pContainer != nullptr);
            return &Base::BaseIterator::Get()->key;
        }

    private:
        // Constructor is private to ensure it cannot be created by anything other than the HashSet itself
        Iterator(const HashSet* pContainer, uint32 startBucket) :
            Base::BaseIterator(pContainer, startBucket)
        {
        }

        // Creates an Iterator object from a BaseIterator object.
        // Constructor is private to ensure it cannot be created by anything other than the HashMap itself
        Iterator(typename Base::BaseIterator&& iterator) :
            Base::BaseIterator(iterator)
        {
        }
    };

    //
    // functions necessary for C++ ranged based for loop support
    //

    // Implement begin() function for range-based for loops
    template<typename Key,
        size_t NumBuckets,
        template<typename> class HashFunc,
        template<typename> class EqualFunc>
        inline typename HashSet<Key, NumBuckets, HashFunc, EqualFunc>::Iterator
            begin(HashSet<Key, NumBuckets, HashFunc, EqualFunc> &rhs)
    {
        return rhs.Begin();
    }

    // Implement end() function for range-based for loops
    template<typename Key,
        size_t NumBuckets,
        template<typename> class HashFunc,
        template<typename> class EqualFunc>
        inline constexpr typename HashSet<Key, NumBuckets, HashFunc, EqualFunc>::Iterator
            end(const HashSet<Key, NumBuckets, HashFunc, EqualFunc> &rhs)
    {
        return rhs.End();
    }
}
