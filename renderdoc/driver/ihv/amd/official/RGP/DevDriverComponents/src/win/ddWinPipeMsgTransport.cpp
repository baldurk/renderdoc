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
* @file  ddWinPipeMsgTransport.cpp
* @brief Class declaration for WinPipeMsgTransport
***********************************************************************************************************************
*/

#include "ddWinPipeMsgTransport.h"
#include "protocols/systemProtocols.h"
#include "ddPlatform.h"
#include <cstring>

using namespace DevDriver::ClientManagementProtocol;

namespace DevDriver
{
    inline static Result GetLastConnectError()
    {
        Result result = Result::Error;
        DWORD error = GetLastError();
        switch (error)
        {
        case ERROR_SEM_TIMEOUT:
            result = Result::NotReady;
            break;
        case ERROR_FILE_NOT_FOUND:
            result = Result::Unavailable;
            break;
        }
        return result;
    }
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
                result = Result::Success;
            else
                result = (GetLastError() == ERROR_IO_INCOMPLETE) ? Result::NotReady : Result::Error;
        }
        else if (waitResult != WAIT_TIMEOUT)
        {
            result = Result::Error;
        }
        return result;
    }

    WinPipeMsgTransport::WinPipeMsgTransport(const HostInfo& hostInfo) :
        m_hostInfo(hostInfo),
        m_pipeHandle(INVALID_HANDLE_VALUE),
        m_readTransaction(),
        m_writeTransaction()
    {}

    WinPipeMsgTransport::~WinPipeMsgTransport()
    {
        if (m_pipeHandle != INVALID_HANDLE_VALUE)
        {
            Disconnect();
        }
    }

    Result WinPipeMsgTransport::Connect(ClientId* pClientId, uint32 timeoutInMs)
    {
        DD_UNUSED(pClientId);
        DD_UNUSED(timeoutInMs);

        Result result = Result::Error;

        if (m_pipeHandle == INVALID_HANDLE_VALUE && WaitNamedPipe(m_hostInfo.hostname, timeoutInMs))
        {
            m_pipeHandle = CreateFileA(m_hostInfo.hostname,          // Pipe name
                                       GENERIC_READ | GENERIC_WRITE, // Read and write access
                                       0,                            // No sharing
                                       nullptr,                      // Default security attributes
                                       OPEN_EXISTING,                // Opens existing pipe
                                       FILE_FLAG_OVERLAPPED,         // Default attributes
                                       nullptr);                     // No template file

            // CreateFile returns INVALID_HANDLE_VALUE on failure
            if (m_pipeHandle != INVALID_HANDLE_VALUE)
            {
                DWORD dwMode = PIPE_READMODE_MESSAGE | PIPE_WAIT;
                BOOL success = SetNamedPipeHandleState(
                    m_pipeHandle,    // pipe handle
                    &dwMode,  // new pipe mode
                    nullptr,     // don't set maximum bytes
                    nullptr);    // don't set maximum time
                DD_ASSERT(success == TRUE);
                if (success == TRUE)
                {
                    m_readTransaction.oOverlap.Offset = 0;
                    m_readTransaction.oOverlap.OffsetHigh = 0;
                    m_readTransaction.oOverlap.hEvent = CreateEvent(
                        nullptr,    // default security attribute
                        TRUE,    // manual-reset event
                        FALSE,    // initial state = unsignaled
                        nullptr);   // unnamed event object

                    m_writeTransaction.oOverlap.Offset = 0;
                    m_writeTransaction.oOverlap.OffsetHigh = 0;
                    m_writeTransaction.oOverlap.hEvent = CreateEvent(
                        nullptr,    // default security attribute
                        TRUE,    // manual-reset event
                        FALSE,    // initial state = unsignaled
                        nullptr);   // unnamed event object

                    // CreateFile returns NULL on failure
                    DD_ASSERT((m_readTransaction.oOverlap.hEvent != NULL) & (m_writeTransaction.oOverlap.hEvent != NULL));

                    if ((m_readTransaction.oOverlap.hEvent != NULL) & (m_writeTransaction.oOverlap.hEvent != NULL))
                    {
                        result = Result::Success;
                    }
                }
            }
            else
            {
                result = GetLastConnectError();
            }
        }
        else
        {
            result = GetLastConnectError();
        }
        return result;
    }

    Result WinPipeMsgTransport::Disconnect()
    {
        Result result = Result::Error;
        if (m_pipeHandle != INVALID_HANDLE_VALUE)
        {
            CancelIoEx(m_pipeHandle, &m_readTransaction.oOverlap);
            CancelIoEx(m_pipeHandle, &m_writeTransaction.oOverlap);
            CloseHandle(m_writeTransaction.oOverlap.hEvent);
            CloseHandle(m_readTransaction.oOverlap.hEvent);
            CloseHandle(m_pipeHandle);
            m_writeTransaction.oOverlap.hEvent = 0;
            m_readTransaction.oOverlap.hEvent = 0;
            m_pipeHandle = INVALID_HANDLE_VALUE;
            result = Result::Success;
        }
        return result;
    }

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
    Result WinPipeMsgTransport::UpdateClientStatus(ClientId clientId, StatusFlags flags)
    {
        // not implemented
        DD_UNUSED(clientId);
        DD_UNUSED(flags);
        return Result::Unavailable;
    }
#endif

    Result WinPipeMsgTransport::ReadMessage(MessageBuffer &messageBuffer, uint32 timeoutInMs)
    {
        Result result = Result::Error;
        DWORD receivedSize = 0;

        if (!m_readTransaction.ioPending)
        {

            if (ReadFile(m_pipeHandle, &m_readTransaction.message, sizeof(MessageBuffer), &receivedSize, &m_readTransaction.oOverlap))
            {
                result = Result::Success;
            }
            else if (GetLastError() == ERROR_IO_PENDING)
            {
                m_readTransaction.ioPending = true;
            }
        }

        if (m_readTransaction.ioPending)
        {
            result = WaitOverlapped(m_pipeHandle, &m_readTransaction.oOverlap, &receivedSize, timeoutInMs);
        }

        if (result == Result::Success)
        {
            m_readTransaction.ioPending = false;

            memcpy(&messageBuffer, &m_readTransaction.message, receivedSize);
        }
        else if (result != Result::NotReady)
        {
            m_readTransaction.ioPending = false;
            result = Result::Error;
        }

        return result;
    }

    Result WinPipeMsgTransport::WriteMessage(const MessageBuffer &messageBuffer)
    {
        Result result = Result::Error;

        //ResetEvent(m_writeTransaction.oOverlap.hEvent);

        const DWORD totalMsgSize = (sizeof(MessageHeader) + messageBuffer.header.payloadSize);
        m_writeTransaction.cbSize = totalMsgSize;
        DWORD bytesWritten = 0;

        BOOL fSuccess = WriteFile(m_pipeHandle, &messageBuffer, totalMsgSize, &bytesWritten, &m_writeTransaction.oOverlap);

        if (!fSuccess)
        {
            DWORD dwErr = GetLastError();
            if (dwErr == ERROR_IO_PENDING)
            {
                result = WaitOverlapped(m_pipeHandle, &m_writeTransaction.oOverlap, &bytesWritten, kInfiniteTimeout);
            }
        }
        else
        {
            result = Result::Success;
        }

        return result;
    }

#if !DD_VERSION_SUPPORTS(GPUOPEN_DISTRIBUTED_STATUS_FLAGS_VERSION)
    Result WinPipeMsgTransport::QueryStatus(const HostInfo& hostInfo,
                                            uint32          timeoutInMs,
                                            StatusFlags*    pFlags)
    {
        Result result = Result::Error;

        MessageBuffer message = kOutOfBandMessage;
        message.header.messageId = static_cast<MessageCode>(ManagementMessage::QueryStatus);

        MessageBuffer responseMessage = {};
        DWORD bytesRead = 0;
        const BOOL success = CallNamedPipe(hostInfo.hostname,
                                           &message,
                                           sizeof(message.header),
                                           &responseMessage,
                                           sizeof(responseMessage),
                                           &bytesRead,
                                           timeoutInMs);
        if ((success != FALSE) & (bytesRead > sizeof(responseMessage.header)))
        {
            result = Result::VersionMismatch;
            if (IsOutOfBandMessage(responseMessage) &
                IsValidOutOfBandMessage(responseMessage) &
                (responseMessage.header.messageId == static_cast<MessageCode>(ManagementMessage::QueryStatusResponse)))
            {
                QueryStatusResponsePayload *pResponse =
                    reinterpret_cast<QueryStatusResponsePayload *>(&responseMessage.payload[0]);
                result = pResponse->result;
                *pFlags = pResponse->flags;
            }
        }
        else
        {
            result = GetLastConnectError();
        }

        return result;
    }
#endif

    // ================================================================================================================
    // Tests to see if the client can connect to RDS through this transport
    Result WinPipeMsgTransport::TestConnection(const HostInfo& hostInfo,
                                               uint32          timeoutInMs)
    {
        Result result = Result::Error;

        // In order to test connectivity we are going to manually send a KeepAlive message. This message is discarded
        // by both clients and RDS, making it safe to use for this purpose
        MessageBuffer message = kOutOfBandMessage;
        message.header.messageId = static_cast<MessageCode>(ManagementMessage::KeepAlive);

        MessageBuffer responseMessage = {};

        // Use CallNamedPipe to connect, send, receive, and disconnect to the named pipe
        DWORD bytesRead = 0;
        const BOOL success = CallNamedPipe(hostInfo.hostname,
                                           &message,
                                           sizeof(message.header),
                                           &responseMessage,
                                           sizeof(responseMessage),
                                           &bytesRead,
                                           timeoutInMs);
        // Check to make sure we got the response + that the response is the expected size
        // KeepAlive is defined as having no additional payload, so it will only ever be the size of a header
        if ((success != FALSE) & (bytesRead == sizeof(responseMessage.header)))
        {
            // Since we received a response, we know there is a server. An invalid packet here means that either
            // the remote server didn't understand the request or that there was a logical bug on the server.
            // In either case we treat this as a version mismatch since we can't tell the difference.
            result = Result::VersionMismatch;

            // check packet validity and set success if true
            if (IsOutOfBandMessage(responseMessage) &
                IsValidOutOfBandMessage(responseMessage) &
                (responseMessage.header.messageId == static_cast<MessageCode>(ManagementMessage::KeepAlive)))
            {
                result = Result::Success;
            }
        }
        else
        {
            // if the call failed, try to return a meaningful status result
            result = GetLastConnectError();
        }

        return result;
    }
} // DevDriver
