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
* @file  platform.h
* @brief GPUOpen Platform Abstraction Layer
***********************************************************************************************************************
*/

#pragma once

#include "gpuopen.h"

#if !defined(NDEBUG)
#if !defined(DEVDRIVER_FORCE_ASSERT)
#define DEVDRIVER_FORCE_ASSERT
#endif
#if !defined(DEVDRIVER_HARD_ASSERT)
#define DEVDRIVER_HARD_ASSERT
#endif
#endif

#if defined(_WIN32)
#include "win/ddWinPlatform.h"
#elif defined(__APPLE__) || defined(__linux__)
#include "posix/ddPosixPlatform.h"
#endif

#if defined(__x86_64__) || defined(_M_X64)
#define DD_ARCH_STRING "x86-64"
#elif defined(__i386__) || defined(_M_IX86)
#define DD_ARCH_STRING "x86"
#else
#define DD_ARCH_STRING "Unk"
#endif

#if defined(DD_WINDOWS)
#define DD_OS_STRING "Windows"
#elif defined(DD_LINUX)
#define DD_OS_STRING "Linux"
#else
#define DD_OS_STRING "Unknown"
#endif

#include "util/template.h"
#include "util/memory.h"
#include <cstdarg>

// TODO: remove this and make kDebugLogLevel DD_STATIC_CONST when we use a version of visual studio that supports it
#ifdef DEVDRIVER_LOG_LEVEL
    #define DEVDRIVER_LOG_LEVEL_VALUE static_cast<LogLevel>(DEVDRIVER_LOG_LEVEL)
#else
    #if defined(NDEBUG)
        #define DEVDRIVER_LOG_LEVEL_VALUE LogLevel::Always
    #else
        #define DEVDRIVER_LOG_LEVEL_VALUE LogLevel::Verbose
    #endif
#endif

#define DD_WILL_PRINT(lvl) ((lvl >= DEVDRIVER_LOG_LEVEL_VALUE) & (lvl < DevDriver::LogLevel::Count))
#define DD_PRINT(lvl, ...) DevDriver::LogString<lvl>(__VA_ARGS__)

#if !defined(DEVDRIVER_FORCE_ASSERT)

#define DD_ALERT(statement)      (DD_UNUSED(0))
#define DD_ASSERT(statement)     (DD_UNUSED(0))
#define DD_ASSERT_REASON(reason) (DD_UNUSED(0))
#define DD_ALERT_REASON(reason)  (DD_UNUSED(0))

#else

#define DD_STRINGIFY(str) #str
#define DD_STRINGIFY_(x) DD_STRINGIFY(x)
#define DD_ALERT(statement)                                                                     \
{                                                                                               \
    if (!(statement))                                                                           \
    {                                                                                           \
        DD_PRINT(DevDriver::LogLevel::Alert, "%s (%d): Alert triggered in %s: %s\n",            \
            __FILE__, __LINE__, __func__, DD_STRINGIFY(statement));                             \
    }                                                                                           \
}

#define DD_ASSERT(statement)                                                                    \
{                                                                                               \
    if (!(statement))                                                                           \
    {                                                                                           \
        DD_PRINT(DevDriver::LogLevel::Error, "%s (%d): Assertion failed in %s: %s\n",           \
            __FILE__, __LINE__, __func__, DD_STRINGIFY(statement));                             \
        DevDriver::Platform::DebugBreak(__FILE__, __LINE__, __func__, DD_STRINGIFY(statement)); \
    }                                                                                           \
}

#define DD_ALERT_REASON(reason)                                                     \
{                                                                                   \
    DD_PRINT(DevDriver::LogLevel::Alert, "%s (%d): Alert triggered in %s: %s\n",    \
        __FILE__, __LINE__, __func__, reason);                                      \
}

#define DD_ASSERT_REASON(reason)                                                    \
{                                                                                   \
    DD_PRINT(DevDriver::LogLevel::Error, "%s (%d): Assertion failed in %s: %s\n",   \
        __FILE__, __LINE__, __func__, reason);                                      \
    DevDriver::Platform::DebugBreak(__FILE__, __LINE__, __func__, reason);          \
}

#endif

#if !defined(DD_RESTRICT)
#define DD_RESTRICT
#endif

/// Convenience macro that always asserts.
#define DD_ASSERT_ALWAYS() DD_ASSERT_REASON("Unconditional Assert")

/// Convenience macro that always alerts.
#define DD_ALERT_ALWAYS() DD_ALERT_REASON("Unconditional Alert")

/// Convenience macro that asserts if something has not been implemented.
#define DD_NOT_IMPLEMENTED() DD_ASSERT_REASON("Code not implemented!")

/// Convenience macro that asserts if an area of code that shouldn't be executed is reached.
#define DD_UNREACHABLE() DD_ASSERT_REASON("Unreachable code has been reached!")

namespace DevDriver
{
    namespace Platform
    {

        void DebugPrint(LogLevel lvl, const char* format, ...);

        /* platform functions for performing atomic operations */

        int32 AtomicIncrement(Atomic *variable);
        int32 AtomicDecrement(Atomic *variable);
        int32 AtomicAdd(Atomic *variable, int32 num);
        int32 AtomicSubtract(Atomic *variable, int32 num);

        class Thread
        {
        public:
            Thread();
            ~Thread();
            Result Start(void(*threadCallback)(void *), void *threadParameter);
            Result Join();
            bool IsJoinable() const;

        private:
            ThreadStorage m_thread;
        };

        void* AllocateMemory(size_t size, size_t alignment, bool zero);
        void FreeMemory(void* pMemory);

        /* fast locks */
        class AtomicLock
        {
        public:
            AtomicLock() : m_lock(0) {};
            ~AtomicLock() {};
            void Lock();
            void Unlock();
            bool IsLocked() { return m_lock != 0; };
        private:
            Atomic m_lock;
        };

        class Mutex
        {
        public:
            Mutex();
            ~Mutex();
            void Lock();
            void Unlock();
        private:
            MutexStorage m_mutex;
        };

        class Semaphore
        {
        public:
            explicit Semaphore(uint32 initialCount, uint32 maxCount);
            ~Semaphore();
            Result Signal();
            Result Wait(uint32 millisecTimeout);
        private:
            SemaphoreStorage m_semaphore;
        };

        class Event
        {
        public:
            explicit Event(bool signaled);
            ~Event();
            void Clear();
            void Signal();
            Result Wait(uint32 timeoutInMs);
        private:
            EventStorage m_event;
        };

        class Random
        {
        public:
            Random();
            ~Random();
            uint32 Generate();
            static uint32 Max();

        private:
            RandomStorage m_randState;
        };

        ProcessId GetProcessId();

        uint64 GetCurrentTimeInMs();

        // Todo: Remove Sleep() entirely from our platform API. It cannot be used in the KMD and should not be used
        // anywhere else either.
        void Sleep(uint32 millisecTimeout);

        void GetProcessName(char* buffer, size_t bufferSize);

        void Strncpy(char* pDst, const char* pSrc, size_t dstSize);

        void Snprintf(char* pDst, size_t dstSize, const char* format, ...);
        void Vsnprintf(char* pDst, size_t dstSize, const char* format, va_list args);
    }

#ifndef DD_PRINT_FUNC
#define DD_PRINT_FUNC Platform::DebugPrint
#else
    void DD_PRINT_FUNC(LogLevel logLevel, const char* format, ...);
#endif

    template <LogLevel logLevel = LogLevel::Info, class ...Ts>
    inline void LogString(const char *format, Ts&&... args)
    {
        if (DD_WILL_PRINT(logLevel))
        {
            DD_PRINT_FUNC(logLevel, format, Platform::Forward<Ts>(args)...);
        }
    }

    //---------------------------------------------------------------------
    // CRC32
    //
    // Calculate a 32bit crc using a the Sarwate look up table method. The original algorithm was created by
    // Dilip V. Sarwate, and is based off of Stephan Brumme's implementation. See also:
    // https://dl.acm.org/citation.cfm?doid=63030.63037
    // http://create.stephan-brumme.com/crc32/#sarwate
    //
    // Copyright (c) 2004-2006 Intel Corporation - All Rights Reserved
    //
    //
    //
    // This software program is licensed subject to the BSD License,
    // available at http://www.opensource.org/licenses/bsd-license.html.
    //
    // Abstract:
    //
    // Tables for software CRC generation
    //
    static inline uint32 CRC32(const void *pData, size_t length, uint32 lastCRC = 0)
    {
        DD_STATIC_CONST uint32_t lookupTable[256] =
        {
            0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
            0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
            0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
            0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
            0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
            0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
            0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
            0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
            0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
            0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
            0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
            0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
            0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
            0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
            0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
            0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
            0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
            0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
            0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
            0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
            0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
            0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
            0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
            0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
            0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
            0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
            0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
            0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
            0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
            0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
            0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
            0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D,
        };

        uint32 crc = ~lastCRC; // same as lastCRC ^ 0xFFFFFFFF
        const unsigned char* DD_RESTRICT pCurrent = (const unsigned char*)pData;
        while (length--)
            crc = (crc >> 8) ^ lookupTable[(crc & 0xFF) ^ *pCurrent++];
        return ~crc;
    }
}
