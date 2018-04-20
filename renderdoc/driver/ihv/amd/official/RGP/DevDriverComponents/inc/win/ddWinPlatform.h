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
* @file  ddWinPlatform.h
* @brief Windows platform layer definition
***********************************************************************************************************************
*/

#pragma once

#ifdef BUILDING_KD
#include "precomp.hpp"
#define DD_WINDOWS_KMD
#else
#define _CRT_RAND_S
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#undef WIN32_LEAN_AND_MEAN
#else
#include <Windows.h>
#endif
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <csignal>
#endif

#define DD_RESTRICT __restrict

#define DD_WINDOWS

namespace DevDriver
{
    namespace Platform
    {
        /* platform functions for performing atomic operations */
        typedef volatile long Atomic;
        DD_CHECK_SIZE(Atomic, sizeof(int32));

        struct EmptyStruct {};

#ifdef DD_WINDOWS_KMD

        struct MutexStorage
        {
            FAST_MUTEX mutex;
#if !defined(NDEBUG)
            Atomic     lockCount;
#endif
        };
        typedef KSEMAPHORE SemaphoreStorage;
        typedef KEVENT EventStorage;
        typedef ULONG RandomStorage;
#else
        struct MutexStorage
        {
            CRITICAL_SECTION criticalSection;
#if !defined(NDEBUG)
            Atomic           lockCount;
#endif
        };
        typedef Handle SemaphoreStorage;
        typedef HANDLE EventStorage;
        typedef EmptyStruct RandomStorage;
#endif
        struct ThreadStorage
        {
            void(*callback)(void *);
            void *parameter;
            volatile HANDLE handle;
        };

        // Either invoke the debugger or raise the TRAP
        __declspec(noreturn) inline static void DebugBreak(
            const char* pFile,
            unsigned int line,
            const char* pFunction,
            const char* pAssertion)
        {
            DD_UNUSED(pFile);
            DD_UNUSED(line);
            DD_UNUSED(pFunction);
            DD_UNUSED(pAssertion);
#ifdef DEVDRIVER_HARD_ASSERT
#if defined(_MSC_VER)
            __debugbreak();
#else
            raise(SIGTRAP);
#endif
#endif
        }

        namespace Windows
        {

            // Windows specific functions required for in-memory communication
            Result AcquireFastLock(Atomic *mutex);
            Result ReleaseFastLock(Atomic *mutex);

            Handle CreateSharedSemaphore(uint32 initialCount, uint32 maxCount);
            Handle CopySemaphoreFromProcess(ProcessId processId, Handle hObject);
            Result SignalSharedSemaphore(Handle pSemaphore);
            Result WaitSharedSemaphore(Handle pSemaphore, uint32 millisecTimeout);
            void CloseSharedSemaphore(Handle pSemaphore);

            Handle CreateSharedBuffer(Size bufferSizeInBytes);
            void CloseSharedBuffer(Handle hSharedBuffer);

            Handle MapSystemBufferView(Handle hBuffer, Size bufferSizeInBytes);
            Handle MapProcessBufferView(Handle hBuffer, ProcessId processId);
            void UnmapBufferView(Handle hSharedBuffer, Handle hSharedBufferView);

            class AtomicLockGuard
            {
            public:
                explicit AtomicLockGuard(Atomic &lock) : m_lock(lock) { AcquireFastLock(&m_lock); };
                ~AtomicLockGuard() { ReleaseFastLock(&m_lock); };
            private:
                Atomic &m_lock;
            };
        }
    }
}
