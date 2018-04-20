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
* @file  ddPosixPlatform.cpp
* @brief POSIX platform layer implementation
***********************************************************************************************************************
*/

#include "ddPlatform.h"

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <time.h>

// Make sure our timeout definition matches the Windows value.
static_assert(DevDriver::kInfiniteTimeout == ~(0u), "Infinite Timeout value does not match OS definition!");

namespace DevDriver
{
    namespace Platform
    {
        static Result GetAbsTime(uint32 offsetInMs, timespec *pOutput)
        {
            Result result = Result::Error;
            timespec timeValue = {};
            if (clock_gettime(CLOCK_REALTIME, &timeValue) == 0)
            {
                const uint64 timeInMs = (static_cast<uint64>(timeValue.tv_sec) * 1000) +
                                        (static_cast<uint64>(timeValue.tv_nsec) / 1000000) + offsetInMs;
                const uint64 sec = timeInMs / 1000;
                const uint64 nsec = (timeInMs - (sec * 1000)) * 1000000;
                pOutput->tv_sec = static_cast<time_t>(sec);
                pOutput->tv_nsec = static_cast<long>(nsec);
                result = Result::Success;
            }
            return result;
        }

        /////////////////////////////////////////////////////
        // Local routines.....
        //
        void DebugPrint(LogLevel lvl, const char* format, ...)
        {
            va_list args;
            va_start(args, format);
            char buffer[1024];
            vsnprintf(buffer,sizeof(buffer), format, args);
            va_end(args);

#ifdef DEVDRIVER_PRINT_TO_CONSOLE
            printf("%s\n", buffer);
#endif

        }

        int32 AtomicIncrement(Atomic *variable)
        {
            return __sync_add_and_fetch(variable, 1);
        }

        int32 AtomicDecrement(Atomic *variable)
        {
            return __sync_sub_and_fetch(variable, 1);
        }

        int32 AtomicAdd(Atomic *variable, int32 num)
        {
            return __sync_add_and_fetch(variable, num);
        }

        int32 AtomicSubtract(Atomic *variable, int32 num)
        {
            return __sync_sub_and_fetch(variable, num);
        }

        /////////////////////////////////////////////////////
        // Thread routines.....
        //

        void* PlatformThreadShim(void *pThreadParam)
        {
            DD_ASSERT(pThreadParam != nullptr);
            const ThreadStorage *thread = reinterpret_cast<const ThreadStorage *>(pThreadParam);
            thread->callback(thread->parameter);
            return nullptr;
        }

        Thread::Thread() : m_thread() {};

        Thread::~Thread() {};

        Result Thread::Start(void(*threadCallback)(void *), void *threadParameter)
        {
            DD_ASSERT(threadCallback != nullptr);

            Result result = Result::Error;

            if (m_thread.callback == nullptr)
            {
                m_thread.parameter = threadParameter;
                m_thread.callback = threadCallback;

                if (pthread_create(&m_thread.handle, nullptr, &PlatformThreadShim, &m_thread) == 0)
                {
                    result = Result::Success;
                }
                else
                {
                    memset(&m_thread, 0, sizeof(ThreadStorage));
                }

                DD_ALERT(result != Result::Error);
            }
            return result;
        }

        Result Thread::Join()
        {
            DD_ASSERT(m_thread.callback != nullptr);

            Result result = Result::Error;

            if (pthread_join(m_thread.handle, nullptr) == 0)
            {
                memset(&m_thread, 0, sizeof(ThreadStorage));
                result = Result::Success;
            }

            DD_ALERT(result != Result::Error);
            return result;
        }

        bool Thread::IsJoinable() const
        {
            return m_thread.callback != nullptr;
        }

        /////////////////////////////////////////////////////
        // Memory Management
        /////////////////////////////////////////////////////

        void* AllocateMemory(size_t size, size_t alignment, bool zero)
        {
            void* pMemory = nullptr;
            const int retVal = posix_memalign(&pMemory, alignment, size);
            if ((retVal == 0) && (pMemory != nullptr) && zero)
            {
                memset(pMemory, 0, size);
            }

            return pMemory;
        }

        void FreeMemory(void* pMemory)
        {
            free(pMemory);
        }

        /////////////////////////////////////////////////////
        // Synchronization primatives
        //

        void AtomicLock::Lock()
        {
            // TODO - implement timeout
            while (__sync_val_compare_and_swap(&m_lock, 0, 1) == 1)
            {
                // spin until the mutex is unlocked again
                while (m_lock != 0)
                {
                }
            }
        }

        void AtomicLock::Unlock()
        {
            const long result = __sync_val_compare_and_swap(&m_lock, 1, 0);
            DD_UNUSED(result);
            DD_ASSERT(result != 0);
        }

        Mutex::Mutex()
        {
            const int result = pthread_mutex_init(&m_mutex, nullptr);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        Mutex::~Mutex()
        {
            const int result = pthread_mutex_destroy(&m_mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        void Mutex::Lock()
        {
            const int result = pthread_mutex_lock(&m_mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        void Mutex::Unlock()
        {
            const int result = pthread_mutex_unlock(&m_mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

#if defined(DD_LINUX)
        Semaphore::Semaphore(uint32 initialCount, uint32 maxCount)
        {
            // linux doesn't enforce a max. Beware.
            DD_UNUSED(maxCount);

            int result = sem_init(&m_semaphore, 0, initialCount);
            DD_ASSERT(result == 0);
            DD_UNUSED(result);
        }

        Semaphore::~Semaphore()
        {
            int result = sem_destroy(&m_semaphore);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        Result Semaphore::Signal()
        {
            int result = sem_post(&m_semaphore);
            DD_ASSERT(result == 0);
            return (result == 0) ? Result::Success : Result::Error;
        }

        Result Semaphore::Wait(uint32 timeoutInMs)
        {
            Result result = Result::Error;

            timespec timeout = {};

            if (GetAbsTime(timeoutInMs, &timeout) == Result::Success)
            {
                int retVal = RetryTemporaryFailure(sem_timedwait,&m_semaphore, &timeout);
                if (retVal != -1)
                {
                    result = Result::Success;
                } else if (errno == ETIMEDOUT)
                {
                    result = Result::NotReady;
                }
            }
            return result;
        }
#endif

        Event::Event(bool signaled)
        {
            int result = pthread_mutex_init(&m_event.mutex, nullptr);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            result = pthread_cond_init(&m_event.condition, nullptr);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            m_event.isSet = signaled;
        }

        Event::~Event()
        {
            int result = pthread_cond_destroy(&m_event.condition);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            result = pthread_mutex_destroy(&m_event.mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        void Event::Clear()
        {
            int result = pthread_mutex_lock(&m_event.mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            m_event.isSet = false;

            result = pthread_mutex_unlock(&m_event.mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        void Event::Signal()
        {
            int result = pthread_mutex_lock(&m_event.mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            m_event.isSet = true;

            result = pthread_cond_signal(&m_event.condition);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            result = pthread_mutex_unlock(&m_event.mutex);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);
        }

        Result Event::Wait(uint32 timeoutInMs)
        {
            Result result = Result::Error;

            timespec timeout = {};

            if (GetAbsTime(timeoutInMs, &timeout) == Result::Success)
            {
                int retVal = pthread_mutex_lock(&m_event.mutex);
                DD_UNUSED(retVal);
                DD_ASSERT(retVal == 0);

                int waitResult = 0;
                while (!m_event.isSet && waitResult == 0)
                {
                    waitResult = pthread_cond_timedwait(&m_event.condition, &m_event.mutex, &timeout);
                }

                if (waitResult == 0)
                {
                    result = Result::Success;
                } else if (waitResult == ETIMEDOUT)
                {
                    result = Result::NotReady;
                }

                retVal = pthread_mutex_unlock(&m_event.mutex);
                DD_UNUSED(retVal);
                DD_ASSERT(retVal == 0);
            }
            return result;
        }

#if defined(DD_LINUX)
        Random::Random()
        {
            timespec timeValue = {};
            int result = clock_gettime(CLOCK_MONOTONIC, &timeValue);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            // Use the current time to generate a random seed. Has to be 64 bit because seed48_r expects to get
            // unsigned short seed[3] as a parameter.
            uint64 seed = static_cast<uint64>(timeValue.tv_sec * 1000000000 + timeValue.tv_nsec);

            // Seed our internal state
            seed48_r(reinterpret_cast<unsigned short*>(&seed), &m_randState);
        }

        Random::~Random()
        {
        }

        uint32 Random::Generate()
        {
            long int value;
            // generate a random number
            const int result = mrand48_r(&m_randState, &value);
            DD_UNUSED(result);
            DD_ASSERT(result >= 0);
            // return the result, downcast to a uint32 if necessary
            return static_cast<uint32>(value);
        }

#endif
        uint32 Random::Max()
        {
            return -1;
        }

        ProcessId GetProcessId()
        {
            return static_cast<ProcessId>(getpid());
        }

        uint64 GetCurrentTimeInMs()
        {
            timespec timeValue = {};

            const int result = clock_gettime(CLOCK_MONOTONIC, &timeValue);
            DD_UNUSED(result);
            DD_ASSERT(result == 0);

            return static_cast<uint64>(timeValue.tv_sec * 1000 + timeValue.tv_nsec / 1000000);
        }

        void Sleep(uint32 millisecTimeout)
        {
            timespec relativeTime;
            const time_t sec = static_cast<time_t>(millisecTimeout / 1000);
            relativeTime.tv_sec = sec;
            relativeTime.tv_nsec = (millisecTimeout - (sec * 1000)) * 1000000;
            RetryTemporaryFailure(nanosleep, &relativeTime, &relativeTime);
        }

        void GetProcessName(char* buffer, size_t bufferSize)
        {
            DD_ASSERT(buffer != nullptr);
#if defined(DD_LINUX)
            const char* pProcessName = program_invocation_short_name;
#else
            const char* pProcessName = getprogname();
#endif
            Strncpy(buffer, (pProcessName != nullptr) ? pProcessName : "Unknown", bufferSize);

        }

        void Strncpy(char* pDst, const char* pSrc, size_t dstSize)
        {
            DD_ASSERT(pDst != nullptr);
            DD_ASSERT(pSrc != nullptr);
            DD_ALERT(strlen(pSrc) < dstSize);

            strncpy(pDst, pSrc, (dstSize - 1));
            pDst[dstSize - 1] = '\0';
        }

        void Snprintf(char* pDst, size_t dstSize, const char* format, ...)
        {
            va_list args;
            va_start(args, format);
            vsnprintf(pDst, dstSize, format, args);
            pDst[dstSize - 1] = '\0';
            va_end(args);
        }

        void Vsnprintf(char* pDst, size_t dstSize, const char* format, va_list args)
        {
            vsnprintf(pDst, dstSize, format, args);
            pDst[dstSize - 1] = '\0';
        }
    }
}
