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
* @file  ddPosixPlatform.h
* @brief POSIX Platform layer definition
***********************************************************************************************************************
*/

#pragma once

#include <pthread.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <assert.h>

#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#define DD_DARWIN
#elif defined(__linux__)
#include <semaphore.h>
#define DD_LINUX
#else
static_assert(false, "Unknown platform detected")
#endif

#define DD_RESTRICT __restrict__

namespace DevDriver
{
    namespace Platform
    {
        template <typename T, typename... Args>
        int RetryTemporaryFailure(T& func, Args&&... args)
        {
            int retval;
            do
            {
                retval = func(args...);
            } while (retval == -1 && errno == EINTR);
            return retval;
        }

        typedef volatile int32 Atomic;

        struct EmptyStruct
        {
        };

        struct EventStorage
        {
            pthread_mutex_t mutex;
            pthread_cond_t condition;
            volatile bool isSet;
        };

        typedef pthread_mutex_t MutexStorage;

#if defined(DD_LINUX)
        typedef sem_t SemaphoreStorage;
        typedef drand48_data RandomStorage;
#endif

        struct ThreadStorage
        {
            void(*callback)(void *);
            void *parameter;
            pthread_t handle;
        };

        inline static void DebugBreak(
            const char* pFile,
            unsigned int line,
            const char* pFunction,
            const char* pAssertion) __attribute__((noreturn));

        static void DebugBreak(
            const char* pFile,
            unsigned int line,
            const char* pFunction,
            const char* pAssertion)
        {
            DD_UNUSED(pFile);
            DD_UNUSED(line);
            DD_UNUSED(pFunction);
            DD_UNUSED(pAssertion);

#if defined(DEVDRIVER_HARD_ASSERT)
#if !defined(NDEBUG)
#if defined(DD_LINUX)
            __assert_fail(pAssertion, pFile, line, pFunction);
#endif
#else
#if defined(SIGTRAP)
            raise(SIGTRAP);
#else
            raise(SIGINT);
#endif
#endif
#endif
            __builtin_unreachable();
        }
    }
}
