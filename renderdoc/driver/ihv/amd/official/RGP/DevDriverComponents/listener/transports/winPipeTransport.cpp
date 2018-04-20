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

#include "winPipeTransport.h"
#include "ddPlatform.h"
#include <cstring>
#include <Windows.h>
#include "../routerCore.h"

#define BUFFER_LENGTH 16
#define RECV_BUFSIZE (sizeof(MessageBuffer) * 8)
#define SEND_BUFSIZE (sizeof(MessageBuffer) * 8)

namespace DevDriver
{
    static Result WaitOverlapped(HANDLE hPipe, OVERLAPPED* pOverlapped, DWORD *pBytesTransferred, DWORD waitTimeMs)
    {
        DWORD waitResult = WAIT_OBJECT_0;
        Result result = Result::NotReady;

        //BOOL result = GetOverlappedResultEx(hPipe, pOverlapped, pBytesTransferred, waitTimeMs, FALSE);

        if (waitTimeMs > 0)
            waitResult = WaitForSingleObject(pOverlapped->hEvent, waitTimeMs);

        if (waitResult == WAIT_OBJECT_0)
        {
            if (GetOverlappedResult(hPipe, pOverlapped, pBytesTransferred, FALSE))
            {
                result = Result::Success;
            }
            else
            {
                result = (GetLastError() == ERROR_IO_INCOMPLETE) ? Result::NotReady : Result::Error;
            }
        }
        else if (waitResult != WAIT_TIMEOUT)
        {
            result = Result::Error;
        }
        return result;
    }

    void SetDebugPriviledges(bool enabled)
    {
        HANDLE              hToken;
        LUID                SeDebugNameValue;
        TOKEN_PRIVILEGES    TokenPrivileges;

        if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
        {
            if (LookupPrivilegeValue(nullptr, SE_DEBUG_NAME, &SeDebugNameValue))
            {
                TokenPrivileges.PrivilegeCount = 1;
                TokenPrivileges.Privileges[0].Luid = SeDebugNameValue;
                TokenPrivileges.Privileges[0].Attributes = enabled ? SE_PRIVILEGE_ENABLED : 0;

                if (AdjustTokenPrivileges(hToken, FALSE, &TokenPrivileges, sizeof(TOKEN_PRIVILEGES), nullptr, nullptr))
                {
                    CloseHandle(hToken);
                }
                else
                {
                    CloseHandle(hToken);
                    DD_PRINT(LogLevel::Error, "Couldn't adjust token privileges!");
                }
            }
            else
            {
                CloseHandle(hToken);
                DD_PRINT(LogLevel::Error, "Couldn't look up privilege value!");
            }
        }
        else
        {
            DD_PRINT(LogLevel::Error, "Couldn't open process token!");
        }
    }

    Result ReadMessage(PipeInfo &threadInfo, OVERLAPPED &oOverlap, MessageContext &messageContext, uint32 timeoutInMs)
    {
        Result result = Result::Error;
        HANDLE hPipe = reinterpret_cast<HANDLE>(threadInfo.pipeHandle);
        DWORD receivedSize = 0;

        if (!threadInfo.ioPending)
        {

            if (ReadFile(hPipe, &messageContext.message, sizeof(MessageBuffer), &receivedSize, &oOverlap))
            {
                result = Result::Success;
            }
            else if (GetLastError() == ERROR_IO_PENDING)
            {
                threadInfo.ioPending = true;
            }
        }

        if (threadInfo.ioPending)
        {
            result = WaitOverlapped(hPipe, &oOverlap, &receivedSize, timeoutInMs);
            //DD_ASSERT(result != Result::Error);
        }

        if (result == Result::Success)
        {
            threadInfo.ioPending = false;
        }
        else if (result != Result::NotReady)
        {
            threadInfo.ioPending = false;
            result = Result::Error;
        }

        return result;
    }

    void PipeListenerTransport::ListeningThreadFunc(PipeInfo *pipeInfo)
    {
        DD_ASSERT(pipeInfo != nullptr);

        OVERLAPPED oOverlap = {};
        oOverlap.Offset = 0;
        oOverlap.OffsetHigh = 0;
        oOverlap.hEvent = CreateEvent(
            nullptr,    // default security attribute
            TRUE,    // manual-reset event
            FALSE,    // initial state = unsignaled
            nullptr);   // unnamed event object

        pipeInfo->active = true;

        while (pipeInfo->active)
        {
            // Wait for the client to connect; if it succeeds,
            // the function returns a nonzero value. If the function
            // returns zero, GetLastError returns ERROR_PIPE_CONNECTED.

            HANDLE hPipe = INVALID_HANDLE_VALUE;
            hPipe = CreateNamedPipeA(
                m_pipeName,                                             // Pipe name
                PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,              // Can read/write and uses overlapped i/o
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,  // Message oriented and blocking reads/writes
                PIPE_UNLIMITED_INSTANCES,                               // Max. instances
                SEND_BUFSIZE,                                           // Output buffer size
                RECV_BUFSIZE,                                           // Input buffer size
                0,                                                      // Client time-out
                nullptr);                                               // Default security attribute

            if (hPipe == INVALID_HANDLE_VALUE)
            {
                DD_PRINT(LogLevel::Error, "[winPipeTransport] CreateNamedPipe failed, GLE=%d.", GetLastError());
                return;
            }

            Result result = (ConnectNamedPipe(hPipe, &oOverlap) != FALSE) ? Result::Success : Result::Error;

            if (result != Result::Success)
            {
                DWORD dwErr = GetLastError();
                if (dwErr == ERROR_IO_PENDING)
                {
                    DD_PRINT(LogLevel::Debug, "[winPipeTransport] Waiting for new client");

                    DWORD numBytes = 0;

                    result = Result::NotReady;

                    while ((pipeInfo->active) & (result == Result::NotReady))
                    {
                        if (m_threadPool.deleteSet.size() > 0)
                        {
                            std::lock_guard<std::mutex> lock(m_threadPool.mutex);
                            for (auto &threadInfo : m_threadPool.deleteSet)
                            {
                                // should already be inactive
                                threadInfo->active = false;
                                if (threadInfo->thread.joinable())
                                    threadInfo->thread.join();
                                DisconnectNamedPipe(reinterpret_cast<HANDLE>(threadInfo->pipeHandle));
                                CloseHandle(reinterpret_cast<HANDLE>(threadInfo->pipeHandle));
                                CloseHandle(reinterpret_cast<HANDLE>(threadInfo->readEvent));
                                CloseHandle(reinterpret_cast<HANDLE>(threadInfo->writeEvent));
                                delete threadInfo;
                            }
                            m_threadPool.deleteSet.clear();
                        }
                        result = WaitOverlapped(hPipe, &oOverlap, &numBytes, kWaitTimeoutInMs);
                    }
                }
                else if (dwErr == ERROR_PIPE_CONNECTED)
                {
                    result = Result::Success;
                }
            }

            if (result == Result::Success)
            {
                DD_PRINT(LogLevel::Debug, "[winPipeTransport] New client connected, starting new thread");
                PipeInfo *newThread = new PipeInfo();
                newThread->pipeHandle = DD_PTR_TO_HANDLE(hPipe);
                newThread->active = true;

                newThread->writeEvent = DD_PTR_TO_HANDLE(CreateEvent(
                    nullptr,    // default security attribute
                    TRUE,    // manual-reset event
                    FALSE,    // initial state = unsignaled
                    nullptr));   // unnamed event object

                newThread->readEvent = DD_PTR_TO_HANDLE(CreateEvent(
                    nullptr,    // default security attribute
                    TRUE,    // manual-reset event
                    FALSE,    // initial state = unsignaled
                    nullptr));   // unnamed event object

                newThread->thread = std::thread(&PipeListenerTransport::ReceivingThreadFunc, this, m_pRouter, newThread);

                if (!newThread->thread.joinable())
                {
                    DD_PRINT(LogLevel::Error, "[winPipeTransport] Thread creation failed!");
                    DisconnectNamedPipe(hPipe);
                    CloseHandle(hPipe);
                    delete newThread;
                }
            }
            else
            {
                if (result == Result::Error)
                {
                    DD_PRINT(LogLevel::Error, "[winPipeTransport] Connection failed!");
                }
                CloseHandle(hPipe);
            }
                // The client could not connect, so close the pipe.
        }
    }

    void PipeListenerTransport::ReceivingThreadFunc(RouterCore *pRouter, PipeInfo *pThreadInfo)
    {
        if ((pThreadInfo == nullptr) | (pRouter == nullptr))
        {
            DD_PRINT(LogLevel::Error, "ERROR - Pipe Server Failure");
            return;
        }
        else
        {
            std::lock_guard<std::mutex> lock(m_threadPool.mutex);
            m_threadPool.threadMap.emplace(pThreadInfo->pipeHandle, pThreadInfo);
        }

        pThreadInfo->active = true;

        DD_PRINT(LogLevel::Debug, "[winPipeTransport] New client thread started");

        OVERLAPPED oOverlap = {};
        oOverlap.Offset = 0;
        oOverlap.OffsetHigh = 0;
        oOverlap.hEvent = reinterpret_cast<HANDLE>(pThreadInfo->readEvent);

        MessageContext recvContext = {};
        recvContext.connectionInfo.handle = m_transportHandle;
        recvContext.connectionInfo.size = sizeof(HANDLE);
        memcpy(&recvContext.connectionInfo.data[0], &pThreadInfo->pipeHandle, sizeof(HANDLE));

        RoutingCache cache(pRouter);
        std::deque<MessageContext> recvQueue;
        std::deque<MessageContext> retryQueue;

        // Loop until done reading
        while (pThreadInfo->active)
        {
            size_t firstNewMessageIndex = recvQueue.size();
            DD_STATIC_CONST uint32 kReceiveDelayInMs = 10;

            // Check for new local messages.
            Result result = ReadMessage(*pThreadInfo, oOverlap, recvContext, kReceiveDelayInMs);
            while (result == Result::Success)
            {
                recvQueue.emplace_back(recvContext);
                result = ReadMessage(*pThreadInfo, oOverlap, recvContext, kNoWait);
            }

            size_t messageNumber = 0;
            for (const auto &message : recvQueue)
            {
                messageNumber++;
                // only requeue messages if it's the first time we've tried to send them
                if ((cache.RouteMessage(message) == Result::NotReady) & (messageNumber > firstNewMessageIndex))
                {
                    retryQueue.emplace_back(message);
                }
            }
            recvQueue.clear();
            recvQueue.swap(retryQueue);

            if (result == Result::Error)
            {
                pThreadInfo->active = false;
                std::lock_guard<std::mutex> threadLock(m_threadPool.mutex);
                if (m_threadPool.threadMap.erase(pThreadInfo->pipeHandle) > 0)
                {
                    m_threadPool.deleteSet.emplace(pThreadInfo);
                }
            }
        }
    }

    PipeListenerTransport::PipeListenerTransport(const char* pPipeName) :
        m_listening(false),
        m_listenThread(),
        m_pRouter(nullptr)
    {
        DD_ASSERT(pPipeName != nullptr);
        Platform::Strncpy(&m_pipeName[0], pPipeName, sizeof(m_pipeName));
    }

    PipeListenerTransport::~PipeListenerTransport()
    {
        if (m_listening)
            Disable();
    }

    Result PipeListenerTransport::ReceiveMessage(ConnectionInfo &connectionInfo, MessageBuffer &message, uint32 timeoutInMs)
    {
        DD_UNUSED(connectionInfo);
        DD_UNUSED(message);
        DD_UNUSED(timeoutInMs);
        return Result::Error;
    }

    Result PipeListenerTransport::TransmitMessage(const ConnectionInfo & connectionInfo, const MessageBuffer & message)
    {
        Result result = Result::Error;
        DD_ASSERT(connectionInfo.handle == m_transportHandle);
        DD_ASSERT(connectionInfo.size == sizeof(HANDLE));

        const HANDLE &hPipe = *reinterpret_cast<const HANDLE *>(&connectionInfo.data[0]);
        PipeInfo *pThreadInfo = nullptr;

        std::unique_lock<std::mutex> lock(m_threadPool.mutex);
        auto find = m_threadPool.threadMap.find(DD_PTR_TO_HANDLE(hPipe));
        if (find != m_threadPool.threadMap.end())
        {
            pThreadInfo = find->second;
        }
        lock.unlock();

        if (pThreadInfo != nullptr)
        {

            DWORD cbWritten = 0;
            DWORD totalMessageSize = sizeof(MessageHeader) + message.header.payloadSize;
            OVERLAPPED oOverlap = {};

            std::lock_guard<std::mutex> guard(pThreadInfo->lock);
            HANDLE hEvent = reinterpret_cast<HANDLE>(pThreadInfo->writeEvent);

            oOverlap.hEvent = hEvent;

            BOOL fSuccess = WriteFile(hPipe,
                &message,     // buffer to write from
                totalMessageSize, // number of bytes to write
                &cbWritten,   // number of bytes written
                &oOverlap);        // not overlapped I/O

            if (!fSuccess)
            {
                DWORD dwErr = GetLastError();
                if (dwErr == ERROR_IO_PENDING)
                {
                    Result waitResult = WaitOverlapped(hPipe, &oOverlap, &cbWritten, kInfiniteTimeout);
                    if (waitResult == Result::Success)
                    {
                        fSuccess = TRUE;
                    }
                    else
                    {
                        DD_ALERT_REASON("Wait on pipe write failed.");
                    }
                }
            }

            if (fSuccess != FALSE)
            {
                result = Result::Success;
            } else
            {

                pThreadInfo->active = false;
                lock.lock();
                if (m_threadPool.threadMap.erase(pThreadInfo->pipeHandle) > 0)
                {
                    m_threadPool.deleteSet.emplace(pThreadInfo);
                }
                lock.unlock();
            }

        }

        //DD_ASSERT(result != Result::Error);
        return result;
    }

    Result PipeListenerTransport::TransmitBroadcastMessage(const MessageBuffer & message)
    {
        DD_UNUSED(message);
        return Result::Error;
    }

    Result PipeListenerTransport::Enable(RouterCore *pRouter, TransportHandle handle)
    {
        Result result = Result::Error;
        SetDebugPriviledges(true);

        HANDLE hPipe = INVALID_HANDLE_VALUE;
        hPipe = CreateNamedPipeA(
            m_pipeName,                                             // Pipe name
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,              // Can read/write and uses overlapped i/o
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,  // Message oriented and blocking reads/writes
            PIPE_UNLIMITED_INSTANCES,                               // Max. instances
            SEND_BUFSIZE,                                           // Output buffer size
            RECV_BUFSIZE,                                           // Input buffer size
            0,                                                      // Client time-out
            nullptr);                                               // Default security attribute

        if (hPipe != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hPipe);

            m_transportHandle = handle;
            m_pRouter = pRouter;

            m_listenThread.active = true;
            m_listenThread.thread = std::thread(&PipeListenerTransport::ListeningThreadFunc, this, &m_listenThread);

            if (m_listenThread.thread.joinable())
            {
                result = Result::Success;
                m_listening = true;
            }
            else
            {
                m_transportHandle = 0;
                m_pRouter = nullptr;
                SetDebugPriviledges(false);
            }
        }
        else if (GetLastError() == ERROR_ACCESS_DENIED)
        {
            result = Result::Unavailable;
        }
        return result;
    }

    Result PipeListenerTransport::Disable()
    {
        Result result = Result::Error;
        if (m_listening)
        {
            m_listenThread.active = false;
            if (m_listenThread.thread.joinable())
                m_listenThread.thread.join();

            std::lock_guard<std::mutex> lock(m_threadPool.mutex);
            for (auto &pThreadInfo : m_threadPool.deleteSet)
            {
                // should already be inactive
                pThreadInfo->active = false;
                if (pThreadInfo->thread.joinable())
                    pThreadInfo->thread.join();
                DisconnectNamedPipe(reinterpret_cast<HANDLE>(pThreadInfo->pipeHandle));
                CloseHandle(reinterpret_cast<HANDLE>(pThreadInfo->pipeHandle));
                CloseHandle(reinterpret_cast<HANDLE>(pThreadInfo->readEvent));
                CloseHandle(reinterpret_cast<HANDLE>(pThreadInfo->writeEvent));
                delete pThreadInfo;
            }
            m_threadPool.deleteSet.clear();
            for (auto &pair : m_threadPool.threadMap)
            {
                pair.second->active = false;
                if (pair.second->thread.joinable())
                    pair.second->thread.join();
                DisconnectNamedPipe(reinterpret_cast<HANDLE>(pair.second->pipeHandle));
                CloseHandle(reinterpret_cast<HANDLE>(pair.second->pipeHandle));
                CloseHandle(reinterpret_cast<HANDLE>(pair.second->readEvent));
                CloseHandle(reinterpret_cast<HANDLE>(pair.second->writeEvent));
                delete pair.second;
            }
            m_transportHandle = 0;
            m_threadPool.threadMap.clear();
            m_listening = false;
            SetDebugPriviledges(false);
        }
        // todo: unregister all clients
        return result;
    }
} // DevDriver
