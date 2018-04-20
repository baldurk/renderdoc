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
  * @file  hashFunc.h
  * @brief Hashing and comparison functors used by HashMap and HashSet
  ***********************************************************************************************************************
  */

#pragma once

#include "gpuopen.h"
#include "ddPlatform.h"
#include "memory.h"
#include <cstring>

namespace DevDriver
{
    /// Default hash functor.
    ///
    /// Just directly returns bits 31-6 of the key's first dword.  This is a decent hash if the key is a pointer.
    template<typename Key>
    struct DefaultHashFunc
    {
        /// Shifts the key to the right and use the resulting bits as a uint hash.
        ///
        /// @param [in] pKey     Pointer to the key to be hashed.  If the key is a pointer, which is the best use case for
        ///                      this hash function, then this is really a pointer to a pointer.
        /// @returns 32-bit uint hash value.
        uint32 operator()(const Key* pKey) const;

        DefaultHashFunc(uint32 minNumBits) {
            // Calculate how many bits of precision are left over after accounting for the number of buckets.
            const uint32 remainingPrecision = (uint32)Platform::Max(((kLength * 8) - (size_t)minNumBits), (size_t)0);
            // We are going to shift by either how much precision is remaining, or the default shift value of 6 bits.
            DD_STATIC_CONST uint32 kDefaultShiftNum = 6;  ///< Right shift bit number
            m_shiftNum = Platform::Min(remainingPrecision, kDefaultShiftNum);
        };

    private:
        uint32 m_shiftNum;
        DD_STATIC_CONST size_t kLength = Platform::Min(sizeof(Key), sizeof(uint32)); ///< How many bytes we read.
    };

    /// Null hash functor.
    ///
    /// Simply returns up to the first 4 bytes of the key. This works well for small, consecutive values (e.g., uint8)
    template<typename Key>
    struct NullHashFunc
    {
        /// Returns up to the first 4 bytes of the key
        ///
        /// @param [in] pKey     Pointer to the key to be hashed.  If the key is a pointer, which is the best use case for
        ///                      this hash function, then this is really a pointer to a pointer.
        /// @returns 32-bit uint hash value.
        uint32 operator()(const Key* pKey) const;

        NullHashFunc(uint32 minNumBits)
        {
            DD_UNUSED(minNumBits);
        };

    private:
        DD_STATIC_CONST size_t kLength = Platform::Min(sizeof(Key), sizeof(uint32)); ///< How many bytes we read.
    };

    /// Jenkins hash functor.
    ///
    /// Compute hash value according to the Jenkins algorithm.  A description of the algorithm is found here:
    /// http://burtleburtle.net/bob/hash/doobs.html
    /// By Bob Jenkins, 1996. bob_jenkins@compuserve.com. You may use this
    /// code any way you wish, private, educational, or commercial. It's free.
    /// See http:\\ourworld.compuserve.com\homepages\bob_jenkins\evahash.htm
    /// Use for hash table lookup, or anything where one collision in 2^^32 is
    /// acceptable. Do NOT use for cryptographic purposes.
    ///
    template<typename Key>
    struct JenkinsHashFunc
    {
        /// Hashes the specified key value via the Jenkins hash algorithm.
        ///
        /// @param [in] pKey     Pointer to the key to be hashed.
        /// @param [in] keyLen   Amount of data at pVoidKey to hash, in bytes.
        ///
        /// @returns 32-bit uint hash value.
        uint32 operator()(const Key* pKey, uint32 len = sizeof(Key)) const;

        /// Initialize local state. Does nothing for Jenkins
        JenkinsHashFunc(uint32 minNumBits)
        {
            DD_UNUSED(minNumBits);
        };
    };

    /// Jenkins hash functor for C-style strings.
    ///
    /// Compute hash value according to the Jenkins algorithm.  A description of the algorithm is found here:
    /// http://burtleburtle.net/bob/hash/doobs.html
    /// By Bob Jenkins, 1996. bob_jenkins@compuserve.com. You may use this
    /// code any way you wish, private, educational, or commercial. It's free.
    /// See http:\\ourworld.compuserve.com\homepages\bob_jenkins\evahash.htm
    /// Use for hash table lookup, or anything where one collision in 2^^32 is
    /// acceptable. Do NOT use for cryptographic purposes.
    ///
    /// @note This hash function is for char* keys only, since the regular JenkinsHashFunc will attempt to do a hash on the
    /// address of the pointer, as opposed to the actual string.
    template<typename Key>
    struct StringJenkinsHashFunc : JenkinsHashFunc<Key>
    {
        /// Hashes the specified C-style string key via the Jenkins hash algorithm.
        ///
        /// @param [in] ppKey    Pointer to the key string (i.e., this is a char**) to be hashed.
        /// @param [in] keyLen   Amount of data at pVoidKey to hash, in bytes.  Should always be sizeof(char*).
        ///
        /// @returns 32-bit uint hash value.
        uint32 operator()(const char** ppKey) const;

        /// Initialize local state. Does nothing.
        StringJenkinsHashFunc(uint32 minNumBits)
        {
            DD_UNUSED(minNumBits);
        };
    };

    /// Generic compare functor for types that have defined the comparison operator
    ///
    /// Used by @ref HashBase to prevent defining compare functions for each type.
    template<typename Key>
    struct DefaultEqualFunc
    {
        /// Returns true if key1 and key2 are equal according to the equality operator
        bool operator()(const Key& key1, const Key& key2) const
        {
            return (key1 == key2);
        }
    };

    /// Generic compare functor for types with arbitrary size.
    ///
    /// Used by @ref HashBase to prevent defining compare functions for each type.
    template<typename Key>
    struct BitwiseEqualFunc
    {
        /// Returns true if key1 and key2 are equal (have identical memory contents).
        bool operator()(const Key& key1, const Key& key2) const
        {
            return (memcmp(&key1, &key2, sizeof(Key)) == 0);
        }
    };

    /// String compare functor for use with C-style strings.  memcmp doesn't work well for strings, so this uses strcmp.
    template<typename Key>
    struct StringEqualFunc
    {
        /// Returns true if the strings in key1 and key2 are equal.
        bool operator()(const Key& key1, const Key& key2) const;
    };

    // =====================================================================================================================
    // Reads up to 4 bytes of the object passed as a value
    template<typename Key>
    inline uint32 ReadAsUint32(Key* pKey)
    {
        uint32 key = *reinterpret_cast<const uint32*>(pKey);

        switch (Platform::Min(sizeof(Key), sizeof(uint32)))
        {
            case 1:
                key &= 0x000000FF;
                break;
            case 2:
                key &= 0x0000FFFF;
                break;
            case 3:
                key &= 0x00FFFFFF;
                break;
            default:
                break;
        }
        return key;
    }

    // =====================================================================================================================
    // Default hash function implementation.  Simply shift the key to the right and use the resulting bits as the hash.
    template<typename Key>
    uint32 DefaultHashFunc<Key>::operator()(
        const Key* pKey
        ) const
    {
         return (ReadAsUint32(pKey) >> m_shiftNum);
    }

    // =====================================================================================================================
    // Null hash function implementation. Simply returns up to the first 4 bytes of the key.
    template<typename Key>
    uint32 NullHashFunc<Key>::operator()(
        const Key* pKey
        ) const
    {
        return ReadAsUint32(pKey);
    }

    // =====================================================================================================================
    // Hashes the specified key value with the Jenkins hash algorithm.  Implementation based on the algorithm description
    // found here: http://burtleburtle.net/bob/hash/doobs.html.
    // By Bob Jenkins, 1996. bob_jenkins@compuserve.com. You may use this
    // code any way you wish, private, educational, or commercial. It's free.
    // See http:\\ourworld.compuserve.com\homepages\bob_jenkins\evahash.htm
    // Use for hash table lookup, or anything where one collision in 2^^32 is
    // acceptable. Do NOT use for cryptographic purposes.
    template<typename Key>
    uint32 JenkinsHashFunc<Key>::operator()(
        const Key* pKey,
        uint32 len
        ) const
    {
        // Mixing table.
        DD_STATIC_CONST uint8 MixTable[256] =
        {
            251, 175, 119, 215,  81,  14,  79, 191, 103,  49, 181, 143, 186, 157,   0, 232,
            31,  32,  55,  60, 152,  58,  17, 237, 174,  70, 160, 144, 220,  90,  57, 223,
            59,   3,  18, 140, 111, 166, 203, 196, 134, 243, 124,  95, 222, 179, 197,  65,
            180,  48,  36,  15, 107,  46, 233, 130, 165,  30, 123, 161, 209,  23,  97,  16,
            40,  91, 219,  61, 100,  10, 210, 109, 250, 127,  22, 138,  29, 108, 244,  67,
            207,   9, 178, 204,  74,  98, 126, 249, 167, 116,  34,  77, 193, 200, 121,   5,
            20, 113,  71,  35, 128,  13, 182,  94,  25, 226, 227, 199,  75,  27,  41, 245,
            230, 224,  43, 225, 177,  26, 155, 150, 212, 142, 218, 115, 241,  73,  88, 105,
            39, 114,  62, 255, 192, 201, 145, 214, 168, 158, 221, 148, 154, 122,  12,  84,
            82, 163,  44, 139, 228, 236, 205, 242, 217,  11, 187, 146, 159,  64,  86, 239,
            195,  42, 106, 198, 118, 112, 184, 172,  87,   2, 173, 117, 176, 229, 247, 253,
            137, 185,  99, 164, 102, 147,  45,  66, 231,  52, 141, 211, 194, 206, 246, 238,
            56, 110,  78, 248,  63, 240, 189,  93,  92,  51,  53, 183,  19, 171,  72,  50,
            33, 104, 101,  69,   8, 252,  83, 120,  76, 135,  85,  54, 202, 125, 188, 213,
            96, 235, 136, 208, 162, 129, 190, 132, 156,  38,  47,   1,   7, 254,  24,   4,
            216, 131,  89,  21,  28, 133,  37, 153, 149,  80, 170,  68,   6, 169, 234, 151
        };

        const uint8* pByteKey = static_cast<const uint8*>(pKey);

        uint32 a = 0x9e3779b9;         // The golden ratio; an arbitrary value.
        uint32 b = a;
        uint32 c = MixTable[pByteKey[0]];  // Arbitrary value.

        // Handle most of the key.
        while (len >= 12)
        {
            a = a + (pByteKey[0] + (static_cast<uint32>(pByteKey[1]) << 8) +
                (static_cast<uint32>(pByteKey[2]) << 16) +
                     (static_cast<uint32>(pByteKey[3]) << 24));
            b = b + (pByteKey[4] + (static_cast<uint32>(pByteKey[5]) << 8) +
                (static_cast<uint32>(pByteKey[6]) << 16) +
                     (static_cast<uint32>(pByteKey[7]) << 24));
            c = c + (pByteKey[8] + (static_cast<uint32>(pByteKey[9]) << 8) +
                (static_cast<uint32>(pByteKey[10]) << 16) +
                     (static_cast<uint32>(pByteKey[11]) << 24));

            a = a - b;  a = a - c;  a = a ^ (c >> 13);
            b = b - c;  b = b - a;  b = b ^ (a << 8);
            c = c - a;  c = c - b;  c = c ^ (b >> 13);
            a = a - b;  a = a - c;  a = a ^ (c >> 12);
            b = b - c;  b = b - a;  b = b ^ (a << 16);
            c = c - a;  c = c - b;  c = c ^ (b >> 5);
            a = a - b;  a = a - c;  a = a ^ (c >> 3);
            b = b - c;  b = b - a;  b = b ^ (a << 10);
            c = c - a;  c = c - b;  c = c ^ (b >> 15);

            pByteKey = pByteKey + 12;
            len = len - 12;
        }

        // Handle last 11 bytes.
        c = c + sizeof(Key);
        switch (len)
        {
            case 11: c = c + (static_cast<uint32>(pByteKey[10]) << 24);  // fall through
            case 10: c = c + (static_cast<uint32>(pByteKey[9]) << 16);  // fall through
            case  9: c = c + (static_cast<uint32>(pByteKey[8]) << 8);   // fall through
                                                                    // the first byte of c is reserved for the length
            case  8: b = b + (static_cast<uint32>(pByteKey[7]) << 24);  // fall through
            case  7: b = b + (static_cast<uint32>(pByteKey[6]) << 16);  // fall through
            case  6: b = b + (static_cast<uint32>(pByteKey[5]) << 8);   // fall through
            case  5: b = b + pByteKey[4];                                // fall through
            case  4: a = a + (static_cast<uint32>(pByteKey[3]) << 24);  // fall through
            case  3: a = a + (static_cast<uint32>(pByteKey[2]) << 16);  // fall through
            case  2: a = a + (static_cast<uint32>(pByteKey[1]) << 8);   // fall through
            case  1: a = a + pByteKey[0];                                // fall through
                                                                     // case 0: nothing left to add
        }

        a = a - b;  a = a - c;  a = a ^ (c >> 13);
        b = b - c;  b = b - a;  b = b ^ (a << 8);
        c = c - a;  c = c - b;  c = c ^ (b >> 13);
        a = a - b;  a = a - c;  a = a ^ (c >> 12);
        b = b - c;  b = b - a;  b = b ^ (a << 16);
        c = c - a;  c = c - b;  c = c ^ (b >> 5);
        a = a - b;  a = a - c;  a = a ^ (c >> 3);
        b = b - c;  b = b - a;  b = b ^ (a << 10);
        c = c - a;  c = c - b;  c = c ^ (b >> 15);

        return c;
    }

    // =====================================================================================================================
    // Hashes the specified C-style string key with the Jenkins hash algorithm.
    template<typename Key>
    uint32 StringJenkinsHashFunc<Key>::operator()(
        const char** ppKey
        ) const
    {
        const char* pKey = *ppKey;
        const uint32 keyLen = static_cast<uint32>(strlen(pKey));

        return JenkinsHashFunc<Key>::operator()(pKey, keyLen);
    }

    // =====================================================================================================================
    // Returns true if the strings in key1 and key2 are the same.
    template<typename Key>
    bool StringEqualFunc<Key>::operator()(
        const Key& key1,
        const Key& key2
        ) const
    {
        bool ret = false;

        // Can't do strcmp on null.
        if ((key1 != nullptr) & (key2 != nullptr))
        {
            ret = (strcmp(key1, key2) == 0);
        }
        else if ((key1 == nullptr) & (key2 == nullptr))
        {
            ret = true;
        }

        return ret;
    }
}
