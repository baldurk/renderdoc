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
  * @file  hashMap.h
  * @brief Templated class implementing key/value map semantics
  ***********************************************************************************************************************
  */

#pragma once

#include "hashBase.h"

namespace DevDriver
{

    /// Encapsulates one key/value pair in a hash map.
    template<typename Key, typename Value>
    struct HashMapEntry
    {
        Key   key;    ///< Hash map entry key.
        Value value;  ///< Hash map entry value.
    };

    /**
     ***********************************************************************************************************************
     * @brief Templated hash map container.
     *
     * This container is meant for storing elements of an arbitrary (but uniform) key/value type.  Supported operations:
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
     * @warning This class is not thread-safe for Insert, FindAllocate, Erase, or iteration!
     * @warning Init() must be called before using this container. Begin() and Reset() can be safely called before
     *          initialization and Begin() will always return an iterator that points to null.
     *
     * For more details please refer to @ref HashBase.
     ***********************************************************************************************************************
     */
    template<typename Key,
        typename Value,
        size_t NumBuckets = (sizeof(Key) * 8),
        template<typename> class HashFunc = DefaultHashFunc,
        template<typename> class EqualFunc = DefaultEqualFunc>
        class HashMap : public HashBase<Key, HashMapEntry<Key, Value>, HashFunc<Key>, EqualFunc<Key>, NumBuckets>
    {
    public:
        // Forward declare Iterator class
        class Iterator;

        /// Convenience typedef for a templated entry of this hash map.
        using Entry = HashMapEntry<Key, Value>;

        /// @internal Constructor
        ///
        /// @param [in] allocCb Allocator callback struct used to allocate memory
        explicit constexpr HashMap(const AllocCb& allocCb) :
            Base::HashBase(allocCb)
        {
        }

        /// @internal Move Constructor
        ///
        /// @param [in] other HashMap object to move data from
        HashMap(HashMap&& other) :
            Base::HashBase(Platform::Forward<HashMap>(other))
        {
        }

        /// @internal Destructor
        ///
        ~HashMap() {
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
        HashMap& operator=(HashMap rhs)
        {
            Base::Swap(rhs);
            return *this;
        }

        /// Swaps the contents of who HashMap objects
        ///
        /// @param [in] other HashMap object to swap contents with
        void Swap(HashMap& other)
        {
            Base::Swap(other);
        }

        /// Gets a pointer to the value that matches the specified key.
        ///
        /// @param [in] key Key to search for.
        ///
        /// @returns A pointer to the value that matches the specified key or null if an entry for the key does not
        /// exist.
        Value* FindValue(const Key& key) const
        {
            // Get the bucket base address.
            Entry* pMatchingEntry = Base::FindEntry(key);
            return (pMatchingEntry != nullptr) ? &(pMatchingEntry->value) : nullptr;
        }

        /// Gets the pointer associated with the specified key. Only valid if Value is a pointer type.
        ///
        /// @param [in] key Key to search for.
        ///
        /// @returns A pointer that matches the specified key or null if an entry for the key does not exist.
        template <typename T = Value, typename = typename Platform::EnableIf<Platform::IsPointer<T>::Value>::Type>
        T FindPointer(const Key& key) const
        {
            // Get the bucket base address.
            Entry* pMatchingEntry = Base::FindEntry(key);
            return (pMatchingEntry != nullptr) ? pMatchingEntry->value : nullptr;
        }

        /// Finds a given entry; if no entry was found, allocate it.
        ///
        /// @param [in]  key      Key to search for.
        /// @param [out] pExisted True if an entry for the specified key existed before this call was made.  False indicates
        ///                       that a new entry was allocated as a result of this call.
        /// @returns @ref Readable/writeable value in the hash map corresponding to the specified key, or null if
        ///                       the value didn't exist and an allocation could not be created.
        template<typename U = Value, typename = typename Platform::EnableIf<Platform::IsPod<U>::Value>::Type>
        U* FindAllocate(const Key& key,
                        bool*      pExisted)  // [out] True if a matching key was found.
        {
            Entry* pEntry = Base::FindOrAllocate(key, pExisted);
            return (pEntry != nullptr) ? &(pEntry->value) : nullptr;
        }

        /// Inserts or updates a key/value pair, overwriting the previous value if it existed.
        ///
        /// @param [in] key   Key of the new entry to insert.
        /// @param [in] args  Parameters to be passed to the constructor of Value.
        ///
        /// @returns @ref Success if the operation completed successfully, or @ref InsufficientMemory if the operation
        ///          failed because an internal memory allocation failure.
        template <class... Args>
        Result Insert(const Key& key, Args&&... args)
        {
            bool   existed = true;
            Entry* pEntry = Base::FindOrAllocate(key, &existed);
            // Ensure that that it found or allocated memory, and update the value.
            if (pEntry != nullptr)
            {
                if (existed)
                {
                    pEntry->value.~Value();
                }
                new(&pEntry->value) Value(Platform::Forward<Args>(args)...);
            }
            return (pEntry != nullptr) ? Result::Success : Result::InsufficientMemory;
        }

        /// Inserts a key/value pair entry if the key doesn't already exist in the hash map.
        ///
        /// @warning No action will be taken if an entry matching this key already exists, even if the specified value
        ///          differs from the current value stored in the entry matching the specified key.
        ///
        /// @param [in] key   Key of the new entry to insert.
        /// @param [in] value Value of the new entry to insert.
        ///
        /// @returns @ref Success if the operation completed successfully, @ref Error if the operation failed
        ///          because the object already exists, or @ref InsufficientMemory if an internal memory allocation
        ///          failed.
        template <class... Args>
        Result Create(const Key& key, Args&&... args)
        {
            bool   existed = true;
            Entry* pEntry = Base::FindOrAllocate(key, &existed);
            Result result = Result::InsufficientMemory;

            // Add the new value if it did not exist already.
            if (pEntry != nullptr)
            {
                if (existed == false)
                {
                    new(&pEntry->value) Value(Platform::Forward<Args>(args)...);
                    result = Result::Success;
                }
                else
                {
                    result = Result::Error;
                }
            }
            return result;
        }

        /// Subscript operator
        ///
        /// @warning This operator will attempt to de-reference the pointer regardless of if it is valid or not.
        ///          Do not use this if you are concerned about handling memory allocation failures gracefully.
        ///
        /// @param [in] key   Key of the new entry to insert.
        ///
        /// @returns Reference to the value field associated with the key.
        Value& operator[](const Key& key)
        {
            Entry* pEntry = FindOrCreate(key);
            DD_ASSERT(pEntry != nullptr);
            return pEntry->value;
        }

        /// Finds and entry and returns an iterator to it
        ///
        /// @param [in] key key to look up in the HashMap
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

    protected:
        // Typedef for the specialized 'HashBase' object we're inheriting from so we can use properly qualified names when
        // accessing members of HashBase.
        using Base = HashBase<Key, Entry, HashFunc<Key>, EqualFunc<Key>, NumBuckets>;

        /// Finds an existing element, or allocates and constructs one to take it's place
        ///
        /// @param [in] key Key that you want to retrieve or create.
        ///
        /// @returns A pointer to the value that matches the specified key or null if an entry for the key does not
        ///          exist.
        template<typename... Args,
                 typename = typename Platform::EnableIf<Platform::IsConstructible<Value, Args...>::Value>::Type>
        Entry* FindOrCreate(const Key& key, Args&&... args)
        {
            bool   existed = true;
            Entry* pEntry = Base::FindOrAllocate(key, &existed);
            // Ensure that that it found or allocated memory, and update the value.
            if ((pEntry != nullptr) && !existed)
            {
                new(&pEntry->value) Value(Platform::Forward<Args>(args)...);
            }
            return pEntry;
        }
    };

    // Class declaration for HashMap::Iterator
    template<typename Key,
        typename Value,
        size_t NumBuckets,
        template<typename> class HashFunc,
        template<typename> class EqualFunc>
        class HashMap<Key, Value, NumBuckets, HashFunc, EqualFunc>::Iterator : protected Base::BaseIterator
    {
        friend HashMap;
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
        Entry& operator*() const
        {
            DD_ASSERT(Base::BaseIterator::m_pContainer != nullptr);
            return *Base::BaseIterator::Get();
        }

        // Use base access method to implement member of pointer operator
        Entry* operator->() const
        {
            DD_ASSERT(Base::BaseIterator::m_pContainer != nullptr);
            return Base::BaseIterator::Get();
        }

    private:
        // Constructor is private to ensure it cannot be created by anything other than the HashMap itself
        Iterator(const HashMap* pContainer, uint32 startBucket) :
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
        typename Value,
        size_t NumBuckets,
        template<typename> class HashFunc,
        template<typename> class EqualFunc>
        inline typename HashMap<Key, Value, NumBuckets, HashFunc, EqualFunc>::Iterator
            begin(HashMap<Key, Value, NumBuckets, HashFunc, EqualFunc> &rhs)
    {
        return rhs.Begin();
    }

    // Implement end() function for range-based for loops
    template<typename Key,
        typename Value,
        size_t NumBuckets,
        template<typename> class HashFunc,
        template<typename> class EqualFunc>
        inline constexpr typename HashMap<Key, Value, NumBuckets, HashFunc, EqualFunc>::Iterator
            end(const HashMap<Key, Value, NumBuckets, HashFunc, EqualFunc> &rhs)
    {
        return rhs.End();
    }
}
