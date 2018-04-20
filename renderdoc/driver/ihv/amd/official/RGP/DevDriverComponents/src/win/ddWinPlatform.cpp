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
* @file  ddWinPlatform.cpp
* @brief User mode Windows platform layer implementation
***********************************************************************************************************************
*/

#include "ddPlatform.h"

////////////////////////////////
// UMD

#include <Psapi.h>

#include <cstdio>
#include <cstdlib>

// Make sure our timeout definition matches the Windows value.
static_assert(DevDriver::kInfiniteTimeout == INFINITE, "Infinite Timeout value does not match OS definition!");

namespace DevDriver
{
    namespace Platform
    {
        inline Result WaitObject(HANDLE hObject, uint32 millisecTimeout)
        {
            DD_ASSERT(hObject != NULL);
            DWORD status = WaitForSingleObject(hObject, millisecTimeout);
            Result result = Result::Error;
            switch (status)
            {
                case WAIT_OBJECT_0:
                    result = Result::Success;
                    break;
                case WAIT_TIMEOUT:
                    result = Result::NotReady;
                    break;
            }
            DD_ALERT(result != Result::Error);
            return result;
        }

        ////////////////////////
        // Open an event create in user space
        // If hObject is not nullptr, the passed in handle is opened.
        // If it is nullptr, we use the passed in name string.
        inline HANDLE CopyHandleFromProcess(ProcessId processId, HANDLE hObject)
        {
            DD_ASSERT(hObject != NULL);

            HANDLE outputObject = nullptr;

            HANDLE hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, TRUE /*bInheritHandle*/, static_cast<DWORD>(processId));

            if (hProcess != nullptr)
            {
                // Just Duplicate the handle for UMD test
                DuplicateHandle(
                    hProcess,
                    hObject,
                    GetCurrentProcess(),
                    &outputObject,
                    EVENT_ALL_ACCESS,
                    TRUE, /*Inherit Handle*/
                    0 /*options*/);

                CloseHandle(hProcess);
            }

            DD_ALERT(outputObject != NULL);
            return outputObject;
        }

        /////////////////////////////////////////////////////
        // Local routines.....
        //
        void DebugPrint(LogLevel lvl, const char* format, ...)
        {
            DD_UNUSED(lvl);

            va_list args;
            va_start(args, format);
            char buffer[1024];
            vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
            va_end(args);
            strcat_s(buffer, "\n");
            OutputDebugString(buffer);
#ifdef DEVDRIVER_PRINT_TO_CONSOLE
            printf("%s", buffer);
#endif
        }

        int32 AtomicIncrement(Atomic *variable)
        {
            return static_cast<int32>(InterlockedIncrementAcquire(variable));
        }

        int32 AtomicAdd(Atomic *variable, int32 num)
        {
            return static_cast<int32>(InterlockedAddAcquire(variable, static_cast<long>(num)));
        }

        int32 AtomicDecrement(Atomic *variable)
        {
            return static_cast<int32>(InterlockedDecrementAcquire(variable));
        }

        int32 AtomicSubtract(Atomic *variable, int32 num)
        {
            return static_cast<int32>(InterlockedAddAcquire(variable, -static_cast<long>(num)));
        }

        /////////////////////////////////////////////////////
        // Thread routines.....
        //

        DWORD WINAPI PlatformThreadShim(void *pThreadParam)
        {
            DD_ASSERT(pThreadParam != nullptr);
            const ThreadStorage *thread = reinterpret_cast<const ThreadStorage *>(pThreadParam);
            thread->callback(thread->parameter);
            return 0;
        }

        Thread::Thread() : m_thread() {};
        Thread::~Thread() {};

        Result Thread::Start(void(*threadCallback)(void *), void *threadParameter)
        {
            Result result = Result::Error;

            if (m_thread.handle == NULL)
            {
                m_thread.parameter = threadParameter;
                m_thread.callback = threadCallback;

                m_thread.handle = ::CreateThread(
                    nullptr, // Thread Attributes
                    0, // Stack size (use default)
                    PlatformThreadShim,
                    &m_thread,
                    0, // Creation flags - start immediately
                    nullptr);

                if (m_thread.handle != NULL)
                {
                    result = Result::Success;
                }
                DD_ALERT(result != Result::Error);
            }
            return result;
        };

        Result Thread::Join()
        {
            DD_ASSERT(m_thread.handle != NULL);
            Result result = WaitObject(m_thread.handle, kInfiniteTimeout);
            if (result == Result::Success)
            {
                memset(&m_thread, 0, sizeof(ThreadStorage));
            }
            DD_ALERT(result != Result::Error);
            return result;
        };

        bool Thread::IsJoinable() const
        {
            return (m_thread.handle != NULL);
        };

        /////////////////////////////////////////////////////
        // Memory Management
        /////////////////////////////////////////////////////

        void* AllocateMemory(size_t size, size_t alignment, bool zero)
        {
            void* pMemory = _aligned_malloc(size, alignment);
            if ((pMemory != nullptr) && zero)
            {
                memset(pMemory, 0, size);
            }

            return pMemory;
        }

        void FreeMemory(void* pMemory)
        {
            _aligned_free(pMemory);
        }

        /////////////////////////////////////////////////////
        // Synchronization primatives...
        //

        void AtomicLock::Lock()
        {
            Result result = Windows::AcquireFastLock(&m_lock);
            DD_UNUSED(result);
            DD_ASSERT(result == Result::Success);

        }

        void AtomicLock::Unlock()
        {
            Result result = Windows::ReleaseFastLock(&m_lock);
            DD_UNUSED(result);
            DD_ASSERT(result == Result::Success);
        }

        Mutex::Mutex()
            : m_mutex()
        {
            InitializeCriticalSection(&m_mutex.criticalSection);
        }

        Mutex::~Mutex()
        {
#if !defined(NDEBUG)
            // This mutex was destroyed while locked. Potentially hazardous due to possibility of a pending wait on
            // the lock
            DD_ASSERT(m_mutex.lockCount == 0);
#endif
            DeleteCriticalSection(&m_mutex.criticalSection);
        }

        void Mutex::Lock()
        {
            EnterCriticalSection(&m_mutex.criticalSection);
#if !defined(NDEBUG)
            const int32 count = AtomicIncrement(&m_mutex.lockCount);
            // This lock was successfully locked twice. This indicates recursive lock usage, which is non supported
            // on all platforms
            DD_ASSERT(count == 1);
            DD_UNUSED(count);
#endif
        }

        void Mutex::Unlock()
        {
#if !defined(NDEBUG)
            AtomicDecrement(&m_mutex.lockCount);
#endif
            LeaveCriticalSection(&m_mutex.criticalSection);
        }

        Semaphore::Semaphore(uint32 initialCount, uint32 maxCount)
        {
            m_semaphore = Windows::CreateSharedSemaphore(initialCount, maxCount);
        }

        Semaphore::~Semaphore()
        {
            Windows::CloseSharedSemaphore(m_semaphore);
        }

        Result Semaphore::Signal()
        {
            return Windows::SignalSharedSemaphore(m_semaphore);
        }

        Result Semaphore::Wait(uint32 millisecTimeout)
        {
            return WaitObject(reinterpret_cast<HANDLE>(m_semaphore), millisecTimeout);
        }

        Event::Event(bool signaled)
        {
            m_event = CreateEvent(nullptr, TRUE, static_cast<BOOL>(signaled), nullptr);
        }

        Event::~Event()
        {
            CloseHandle(m_event);
        }

        void Event::Clear()
        {
            ResetEvent(m_event);
        }

        void Event::Signal()
        {
            SetEvent(m_event);
        }

        Result Event::Wait(uint32 timeoutInMs)
        {
            return WaitObject(m_event,timeoutInMs);
        }

        Random::Random()
        {
        }

        Random::~Random()
        {
        }

        uint32 Random::Generate()
        {
            uint32 value = 0;
            errno_t result = rand_s(&value);
            DD_ASSERT(result == 0);
            DD_UNUSED(result);
            return value;
        }

        uint32 Random::Max()
        {
            return UINT_MAX;
        }

        ProcessId GetProcessId()
        {
            return static_cast<ProcessId>(GetCurrentProcessId());
        }

        uint64 GetCurrentTimeInMs()
        {
            return GetTickCount64();
        }

        void Sleep(uint32 millisecTimeout)
        {
            ::Sleep(millisecTimeout);
        }

        void GetProcessName(char* buffer, size_t bufferSize)
        {
            char path[1024] = {};
            size_t  numChars = 0;

            numChars = GetModuleFileNameExA(GetCurrentProcess(), nullptr, path, 1024);

            buffer[0] = 0;
            if (numChars > 0)
            {
                char fname[256] = {};
                char ext[256] = {};
                _splitpath_s(path, nullptr, 0, nullptr, 0, fname, sizeof(fname), ext, sizeof(ext));
                strcat_s(buffer, bufferSize, fname);
                strcat_s(buffer, bufferSize, ext);
            }
        }

        void Strncpy(char* pDst, const char* pSrc, size_t dstSize)
        {
            DD_ASSERT(pDst != nullptr);
            DD_ASSERT(pSrc != nullptr);
            DD_ALERT(strlen(pSrc) < dstSize);

            // Clamp the copy to the size of the dst buffer (1 char reserved for the null terminator).
            strcpy_s(pDst, dstSize, pSrc);
        }

        void Snprintf(char* pDst, size_t dstSize, const char* format, ...)
        {
            va_list args;
            va_start(args, format);
            vsnprintf_s(pDst, dstSize, _TRUNCATE, format, args);
            va_end(args);
        }

        void Vsnprintf(char* pDst, size_t dstSize, const char* format, va_list args)
        {
            vsnprintf_s(pDst, dstSize, _TRUNCATE, format, args);
        }

        namespace Windows
        {
            // Windows specific functions required for in-memory communication
            Result AcquireFastLock(Atomic *mutex)
            {
                // TODO - implement timeout
                while (InterlockedCompareExchangeAcquire(mutex, 1, 0) == 1)
                {
                    // spin until the mutex is unlocked again
                    while (*mutex != 0)
                    {
                    }
                }
                return Result::Success;
            }

            Result ReleaseFastLock(Atomic *mutex)
            {
                if (InterlockedCompareExchangeRelease(mutex, 0, 1) == 0)
                {
                    // tried to unlock an already unlocked mutex
                    return Result::Error;
                }
                return Result::Success;
            }

            /////////////////////////////////////////////////////
            // Local routines.....
            //

            Handle CreateSharedSemaphore(uint32 initialCount, uint32 maxCount)
            {
                // Create original object in the current process
                return DD_PTR_TO_HANDLE(CreateSemaphore(
                    nullptr,
                    initialCount,
                    maxCount,
                    nullptr /* Not Named*/));
            }

            Handle CopySemaphoreFromProcess(ProcessId processId, Handle hObject)
            {
                return DD_PTR_TO_HANDLE(CopyHandleFromProcess(processId, reinterpret_cast<HANDLE>(hObject)));
            }

            Result SignalSharedSemaphore(Handle pSemaphore)
            {
                DD_ASSERT(pSemaphore != NULL);
                BOOL result = ReleaseSemaphore(reinterpret_cast<HANDLE>(pSemaphore), 1, nullptr);
                return (result != 0) ? Result::Success : Result::Error;
            }

            Result WaitSharedSemaphore(Handle pSemaphore, uint32 millisecTimeout)
            {
                return WaitObject(reinterpret_cast<HANDLE>(pSemaphore), millisecTimeout);
            }

            void CloseSharedSemaphore(Handle pSemaphore)
            {
                if (pSemaphore != NULL)
                {
                    CloseHandle(reinterpret_cast<HANDLE>(pSemaphore));
                }
            }

            Handle CreateSharedBuffer(Size bufferSizeInBytes)
            {
                HANDLE hSharedBuffer =
                    CreateFileMapping(
                        INVALID_HANDLE_VALUE,    // use paging file
                        nullptr,                    // default security
                        PAGE_READWRITE,          // read/write access
                        0,                       // maximum object size (high-order DWORD)
                        bufferSizeInBytes, // maximum object size (low-order DWORD)
                        nullptr); // name of mapping object

                DD_ALERT(hSharedBuffer != nullptr);
                return DD_PTR_TO_HANDLE(hSharedBuffer);
            }

            Handle MapSystemBufferView(Handle hBuffer, Size bufferSizeInBytes)
            {
                DD_ASSERT(hBuffer != kNullPtr);
                LPVOID pSharedBufferView = MapViewOfFile(
                    reinterpret_cast<HANDLE>(hBuffer),
                    FILE_MAP_ALL_ACCESS, // read/write permission
                    0, // File offset high dword
                    0, // File offset low dword
                    bufferSizeInBytes);
                DD_ALERT(pSharedBufferView != nullptr);
                return DD_PTR_TO_HANDLE(pSharedBufferView);
            }

            void UnmapBufferView(Handle hSharedBuffer, Handle hSharedBufferView)
            {
                DD_UNUSED(hSharedBuffer);
                DD_ASSERT(hSharedBufferView != kNullPtr);
                BOOL result = UnmapViewOfFile(reinterpret_cast<HANDLE>(hSharedBufferView));
                DD_UNUSED(result);
                DD_ALERT(result == TRUE);
            }

            void CloseSharedBuffer(Handle hSharedBuffer)
            {
                if (hSharedBuffer != kNullPtr)
                {
                    BOOL result = CloseHandle(reinterpret_cast<HANDLE>(hSharedBuffer));
                    DD_ALERT(result == TRUE);
                    DD_UNUSED(result);
                }
            }

            Handle MapProcessBufferView(Handle hBuffer, ProcessId processId)
            {
                Handle sharedBuffer = kNullPtr;

                HANDLE hProcess = ::OpenProcess(PROCESS_ALL_ACCESS, TRUE /*bInheritHandle*/, static_cast<DWORD>(processId));

                if (hProcess != nullptr)
                {
                    // Just Duplicate the handle for UMD test
                    DuplicateHandle(
                        GetCurrentProcess(),
                        (HANDLE)hBuffer,
                        hProcess,
                        reinterpret_cast<LPHANDLE>(&sharedBuffer),
                        0,
                        TRUE, /*Inherit Handle*/
                        DUPLICATE_SAME_ACCESS /*options*/);

                    CloseHandle(hProcess);
                }

                DD_ALERT(sharedBuffer != kNullPtr);

                return sharedBuffer;
            }
        }
    }
}
