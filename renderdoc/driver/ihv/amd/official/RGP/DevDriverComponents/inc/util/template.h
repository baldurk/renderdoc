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
* @file  template.h
* @brief Templated utility functions
***********************************************************************************************************************
*/

#pragma once

#if !defined(_MSC_VER)
#include <type_traits>
#endif

namespace DevDriver
{
    namespace Platform
    {
        /// Templated LockGuard class. Works with any type that implements Lock() and Unlock()
        template <typename T>
        class LockGuard
        {
        public:
            explicit LockGuard(T &lock) : m_lock(lock) { lock.Lock(); }
            ~LockGuard() { m_lock.Unlock(); }
        private:
            T &m_lock;
        };

        /// Computes the base-2 logarithm of an unsigned 64-bit integer.
        ///
        /// If the given integer is not a power of 2, this function will not provide an exact answer.
        ///
        /// @returns log_2(u)
        template<typename T>
        inline uint32 Log2(T u)  ///< Value to compute the logarithm of.
        {
            uint32 logValue = 0;

            while (u > 1)
            {
                ++logValue;
                u >>= 1;
            }
            return logValue;
        }

        /// Computes the base-2 logarithm of an unsigned 64-bit integer.
        ///
        /// If the given integer is not a power of 2, this function will not provide an exact answer.
        ///
        /// @returns log_2(u)
        template<typename T>
        inline constexpr uint32 _ConstLog2(T u, uint32 logValue)  ///< Value to compute the logarithm of.
        {
            return (u > 1) ? _ConstLog2(u >> 1, logValue + 1) : logValue;
        }

        /// Computes the base-2 logarithm of an unsigned 64-bit integer.
        ///
        /// If the given integer is not a power of 2, this function will not provide an exact answer.
        ///
        /// @returns log_2(u)
        template<typename T>
        inline constexpr uint32 ConstLog2(T u)  ///< Value to compute the logarithm of.
        {
            return _ConstLog2(u, 0);
        }

        static_assert(ConstLog2(1) == 0, "ConstLog2 failure");
        static_assert(ConstLog2(2) == 1, "ConstLog2 failure");
        static_assert(ConstLog2(128) == 7, "ConstLog2 failure");
        static_assert(ConstLog2(255) == 7, "ConstLog2 failure");

        /// Computes 2 ^ value provided
        ///
        /// @returns 2 ^ (u)
        template<typename T>
        inline constexpr T Pow2(T u)
        {
            return ((T)1 << u);
        }

        static_assert(Pow2(0) == 1, "Pow2 failure");
        static_assert(Pow2(1) == 2, "Pow2 failure");
        static_assert(Pow2(7) == 128, "Pow2 failure");

        /// Determines if a value is a power of two.
        ///
        /// @returns True if it is a power of two, false otherwise.
        inline constexpr bool IsPowerOfTwo(uint64 value)
        {
            return (value == 0) ? false : ((value & (value - 1)) == 0);
        }

        /// Rounds the specified uint 'value' up to the nearest value meeting the specified 'alignment'.  Only power of 2
        /// alignments are supported by this function.
        ///
        /// returns Aligned value.
        template<typename T>
        inline constexpr T Pow2Align(
                            T      value,      ///< Value to align.
                            uint64 alignment)  ///< Desired alignment (must be a power of 2).
        {
            return ((value + static_cast<T>(alignment) - 1) & ~(static_cast<T>(alignment) - 1));
        }

        /// Rounds the specified uint 'value' up to the nearest power of 2
        ///
        /// @returns Power of 2 padded value.
        template<typename T>
        inline T Pow2Pad(T value)  ///< Value to pad.
        {
            T ret = 1;
            if (IsPowerOfTwo(value))
            {
                ret = value;
            }
            else
            {
                while (ret < value)
                {
                    ret <<= 1;
                }
            }

            return ret;
        }

        /// Rounds the specified uint 'value' up to the nearest power of 2. Constexpr varient.
        ///
        /// @returns Power of 2 padded value.
        template<typename T>
        inline constexpr T _ConstPow2Pad(T value, T padded)  ///< Value to pad.
        {
            return (padded < value) ? _ConstPow2Pad(value, padded << 1) : padded;
        }

        /// Rounds the specified uint 'value' up to the nearest power of 2. Constexpr varient.
        ///
        /// @returns Power of 2 padded value.
        template<typename T>
        inline constexpr T ConstPow2Pad(T value)  ///< Value to pad.
        {
            return (IsPowerOfTwo(value)) ? value : _ConstPow2Pad(value, (T)1);
        }

        static_assert(ConstPow2Pad(512) == 512, "ConstPow2Pad failure");
        static_assert(ConstPow2Pad(511) == 512, "ConstPow2Pad failure");
        static_assert(ConstPow2Pad(257) == 512, "ConstPow2Pad failure");

        /// Finds the smallest of two values
        ///
        /// @returns a if a < b, otherwise b.
        template <typename T>
        inline constexpr T Min(const T &a, const T &b)
        {
            return ((a < b) ? a : b);
        }

        /// Finds the larger of two values
        ///
        /// @returns a if a > b, otherwise b.
        template <typename T>
        inline constexpr T Max(const T &a, const T &b)
        {
            return ((a > b) ? a : b);
        }

        // Given a type T, set Type equal to T
        template <typename T>
        struct RemoveRef
        {
            typedef T Type;
        };

        // Given a type T&, set Type equal to T
        template <typename T>
        struct RemoveRef<T &>
        {
            typedef T Type;
        };

        // Given a type T&&, set Type equal to T
        template <typename T>
        struct RemoveRef<T &&>
        {
            typedef T Type;
        };

        // std::move equivalent
        template <typename T>
        inline typename RemoveRef<T>::Type&& Move(T&& obj)
        {
            return static_cast<typename RemoveRef<T>::Type&&>(obj);
        }

        // std::forward equivalent
        template <typename T>
        inline T&& Forward(typename RemoveRef<T>::Type&& args)
        {
            return static_cast<T&&>(args);
        }

        // std::forward equivalent
        template <typename T>
        inline T&& Forward(typename RemoveRef<T>::Type& args)
        {
            return static_cast<T&&>(args);
        }

        // Returns the contents of Value in a new variable, and assign newValue into the memory occupied by value.
        template <typename T, typename U = T>
        inline T Exchange(T& value, U&& newValue)
        {
            T oldValue = Move(value);
            value = Forward<U>(newValue);
            return (oldValue);
        }

        // Convenience structure that defined Value as either true or false, and Type as either TrueType or FalseType
        template <bool value>
        struct BoolType
        {
            static const bool Value = value;
            using Type = BoolType<value>;
        };

        using FalseType = BoolType<false>;
        using TrueType = BoolType<true>;

        // Struct whose ::Type member is undefined if the first condition is not true
        template<bool Enable,
            class Type = void>
            struct EnableIf
        {
        };

        // Struct whose ::Type member is equal to T if the first condition is true.
        template<class T>
        struct EnableIf<true, T>
        {
            typedef T Type;
        };

        template <class T>
        struct IsPointer : FalseType
        {
        };

        template <class T>
        struct IsPointer<T*> : TrueType
        {
        };

// If we are building with MSVC we want to use the compiler intrinsics here. This is primarily because building with
// the /kernel precludes the use of the C++ type traits library. For all other compilers we simply implement this
// using the standard C++ library.
#if defined(_MSC_VER)
        // Struct whose ::Value member is equal to true if you can cast from T to U, and false otherwise.
        template <class T, class U>
        struct IsConvertible : BoolType<__is_convertible_to(T, U)>
        {
        };

        // Struct whose ::Value member is equal to true if you can construct an object of type T using the arguments
        // provided.
        template<typename T, typename... Args>
        struct IsConstructible : BoolType<__is_constructible(T, Args...)>
        {

        };

        // Struct whose ::Value member is equal to true if T is an abstract class, and false otherwise.
        template<typename T>
        struct IsAbstract : BoolType<__is_abstract(T)>
        {

        };

        // Struct whose ::Value member is equal to true if T is an abstract class, and false otherwise.
        template<typename T>
        struct IsPod : BoolType<__is_pod(T)>
        {

        };

        // Struct whose ::Value member is equal to true if T is has a standard layout, and false otherwise.
        template<typename T>
        struct IsStandardLayout : BoolType<__is_standard_layout(T)>
        {

        };

        // Struct whose ::Value member is equal to true if T is trivially destructable, and false otherwise.
        template<typename T>
        struct IsTriviallyDestructible : BoolType<__has_trivial_destructor(T)>
        {

        };
#else
        // Struct whose ::Value member is equal to true if you can cast from T to U, and false otherwise.
        template <class T, class U>
        struct IsConvertible : BoolType<std::is_convertible<T, U>::value>
        {

        };

        // Struct whose ::Value member is equal to true if you can construct an object of type T using the arguments
        // provided.
        template<typename T, typename... Args>
        struct IsConstructible : BoolType<std::is_constructible<T, Args...>::value>
        {

        };

        // Struct whose ::Value member is equal to true if T is an abstract class, and false otherwise.
        template<typename T>
        struct IsAbstract : BoolType<std::is_abstract<T>::value>
        {

        };

        // Struct whose ::Value member is equal to true if T is an abstract class, and false otherwise.
        template<typename T>
        struct IsPod : BoolType<std::is_pod<T>::value>
        {

        };

        // Struct whose ::Value member is equal to true if T is has a standard layout, and false otherwise.
        template<typename T>
        struct IsStandardLayout : BoolType<std::is_standard_layout<T>::value>
        {

        };

        // Struct whose ::Value member is equal to true if T is trivially destructable, and false otherwise.
        template<typename T>
        struct IsTriviallyDestructible : BoolType<std::is_trivially_destructible<T>::value>
        {

        };
#endif
    }
} // DevDriver
