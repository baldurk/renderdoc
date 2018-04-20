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

#include "../ddSocket.h"

#pragma comment(lib, "ws2_32.lib")

#include <ws2tcpip.h>
#include <stdlib.h>
#include "ddPlatform.h"

#include "../session.h"

namespace DevDriver
{
    static inline Result GetDataError(bool nonBlocking)
    {
        Result result = Result::Error;
        int error = WSAGetLastError();
        switch (error)
        {
            case WSAEWOULDBLOCK:
                if (nonBlocking)
                    result = Result::NotReady;
                break;
            case WSAECONNRESET:
            case WSAENETUNREACH:
            case WSAETIMEDOUT:
                result = Result::Unavailable;
                break;
        }
        return result;
    }

    // =====================================================================================================================
    // Constructs the Win32 socket this object encapsulates.
    Socket::Socket()
        : m_osSocket(INVALID_SOCKET)
        , m_isNonBlocking(false)
        , m_socketType(SocketType::Unknown)
        , m_hints()
    {
    }

    // =====================================================================================================================
    // Frees the Win32 socket this object encapsulates.
    Socket::~Socket()
    {
        // On Windows, there is no "DestroySocket" or equivalent call.
        Close();
    }

    // =====================================================================================================================
    // Initializes the Win32 condition variable this object encapsulates.
    Result Socket::Init(bool isNonBlocking, SocketType socketType)
    {
        Result result = Result::Error;

        // Initialize the winsock library.
        const WORD requestedVersion = MAKEWORD(2, 2);
        WSAData wsaData = {};
        if (WSAStartup(requestedVersion, &wsaData) == 0)
        {
            m_isNonBlocking = isNonBlocking;
            m_socketType = socketType;

            if (m_osSocket == INVALID_SOCKET)
            {
                switch (socketType)
                {
                    case SocketType::Tcp:
                        m_osSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                        m_hints.ai_family = AF_INET;
                        m_hints.ai_socktype = SOCK_STREAM;
                        m_hints.ai_protocol = IPPROTO_TCP;
                        break;
                    case SocketType::Udp:
                        m_osSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                        m_hints.ai_family = AF_INET;
                        m_hints.ai_socktype = SOCK_DGRAM;
                        m_hints.ai_protocol = IPPROTO_UDP;
                        break;
                    default:
                        break;
                }

                result = (m_osSocket != INVALID_SOCKET) ? Result::Success : Result::Error;
            }

            if (result == Result::Success)
            {
                // Set the exclusive address option
                int opt = 1;
                int retval = setsockopt(m_osSocket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&opt, sizeof(opt));
                if (retval == SOCKET_ERROR)
                {
                    result = Result::Error;
                }

                // magic number. 2x default window size seems to yield good results
                DD_STATIC_CONST int bufferMultiple = 2;

                // set the buffer to 256K
                int bufferSize = bufferMultiple * kDefaultWindowSize * kMaxMessageSizeInBytes;
                retval = setsockopt(m_osSocket, SOL_SOCKET, SO_SNDBUF, (char *)&bufferSize, sizeof(bufferSize));
                if (retval == SOCKET_ERROR)
                {
                    result = Result::Error;
                }

                retval = setsockopt(m_osSocket, SOL_SOCKET, SO_RCVBUF, (char *)&bufferSize, sizeof(bufferSize));
                if (retval == SOCKET_ERROR)
                {
                    result = Result::Error;
                }

                if ((result == Result::Success) & m_isNonBlocking)
                {
                    // Enable non blocking mode for the socket.
                    u_long arg = 1;
                    result = (ioctlsocket(m_osSocket, FIONBIO, &arg) != SOCKET_ERROR) ? Result::Success
                        : Result::Error;
                }
            }

            // Clean up winsock if the socket initialization failed for some reason.
            if (result != Result::Success)
            {
                WSACleanup();
            }
        }

        return result;
    }

    Result Socket::Connect(const char* pAddress, uint32 port)
    {
        Result result = Result::Success;

        char portBuffer[16] = {};
        _itoa_s(port, portBuffer, 10);

        addrinfo* pResult = nullptr;
        int retVal = getaddrinfo(pAddress, portBuffer, &m_hints, &pResult);

        if (retVal == 0)
        {
            sockaddr* addr = pResult->ai_addr;
            size_t addrSize = pResult->ai_addrlen;

            retVal = connect(m_osSocket, addr, static_cast<int>(addrSize));

            if (retVal == 0)
            {
                result = Result::Success;
            }
            else
            {
                result = GetDataError(m_isNonBlocking);
            }

            freeaddrinfo(pResult);
        }
        else
        {
            result = Result::Error;
        }

        return result;
    }

    Result Socket::Select(bool* pReadState, bool* pWriteState, bool* pExceptState, uint32 timeoutInMs)
    {
        Result result = Result::Error;

        fd_set readSet = {};
        fd_set writeSet = {};
        fd_set exceptSet = {};

#pragma warning(push)
#pragma warning(disable : 4548)
        FD_SET(m_osSocket, &readSet);
        FD_SET(m_osSocket, &writeSet);
        FD_SET(m_osSocket, &exceptSet);
#pragma warning(pop)

        timeval timeoutValue = {};
        timeoutValue.tv_sec = timeoutInMs / 1000;
        timeoutValue.tv_usec = (timeoutInMs % 1000) * 1000;

        fd_set* pReadSet = ((pReadState != nullptr) ? &readSet : nullptr);
        fd_set* pWriteSet = ((pWriteState != nullptr) ? &writeSet : nullptr);
        fd_set* pExceptSet = ((pExceptState != nullptr) ? &exceptSet : nullptr);

        int retval = select(0, pReadSet, pWriteSet, pExceptSet, &timeoutValue);

        if (pReadState != nullptr)
        {
            *pReadState = (FD_ISSET(m_osSocket, pReadSet) != 0);
        }

        if (pWriteState != nullptr)
        {
            *pWriteState = (FD_ISSET(m_osSocket, pWriteSet) != 0);
        }

        if (pExceptState != nullptr)
        {
            *pExceptState = (FD_ISSET(m_osSocket, pExceptSet) != 0);
        }

        if (retval > 0)
        {
            result = Result::Success;
        }
        else
        {
            result = (retval == 0) ? Result::NotReady : Result::Error;
        }
        return result;
    }

    Result Socket::Bind(const char* pAddress, uint32 port)
    {
        Result result = Result::Error;

        addrinfo hints = m_hints;

        hints.ai_flags = AI_PASSIVE;

        char portBuffer[16] = {};
        _itoa_s(port, portBuffer, 10);

        addrinfo* pResult = nullptr;
        int retVal = getaddrinfo(pAddress, portBuffer, &hints, &pResult);

        if (retVal == 0)
        {
            sockaddr* addr = pResult->ai_addr;
            size_t addrSize = pResult->ai_addrlen;

            if (bind(m_osSocket, addr, static_cast<int>(addrSize)) != SOCKET_ERROR)
            {
                result = Result::Success;
            }

            freeaddrinfo(pResult);
        }

        return result;
    }

    Result Socket::Listen(uint32 backlog)
    {
        DD_ASSERT(m_socketType == SocketType::Tcp);

        Result result = Result::Error;

        if (listen(m_osSocket, backlog) != SOCKET_ERROR)
        {
            result = Result::Success;
        }

        return result;
    }

    Result Socket::Accept(Socket* pClientSocket)
    {
        DD_ASSERT(m_socketType == SocketType::Tcp);

        Result result = Result::Error;

        sockaddr addr = {};
        int addrSize = sizeof(addr);

        SOCKET clientSocket = accept(m_osSocket, &addr, &addrSize);
        if (clientSocket != INVALID_SOCKET)
        {
            sockaddr_in* pSocket = reinterpret_cast<sockaddr_in*>(&addr);

            const UINT addressBufSize = 256;
            char addressBuf[addressBufSize];
            const char* pAddress = inet_ntop(AF_INET, reinterpret_cast<void*>(&pSocket->sin_addr), addressBuf, addressBufSize);

            UINT port = ntohs(pSocket->sin_port);

            result = pClientSocket->InitAsClient(clientSocket, pAddress, port, m_isNonBlocking);
        }

        return result;
    }

    Result Socket::LookupAddressInfo(const char* pAddress, uint32 port, size_t addressInfoSize, char* pAddressInfo, size_t *pAddressSize)
    {
        DD_ASSERT(addressInfoSize >= sizeof(sockaddr));
        Result result = Result::Error;

        char portBuffer[16] = {};
        _itoa_s(port, portBuffer, 10);

        addrinfo* pResult = nullptr;
        int retVal = getaddrinfo(pAddress, portBuffer, &m_hints, &pResult);

        if (retVal == 0)
        {
            sockaddr* addr = pResult->ai_addr;
            size_t addrSize = pResult->ai_addrlen;
            if (addressInfoSize >= addrSize)
            {
                memcpy(pAddressInfo, addr, addrSize);
                *pAddressSize = addrSize;
                result = Result::Success;
            }

            freeaddrinfo(pResult);
        }

        return result;
    }

    Result Socket::Send(const uint8* pData, size_t dataSize, size_t* pBytesSent)
    {
        Result result = Result::Error;

        int retVal = send(m_osSocket, reinterpret_cast<const char*>(pData), static_cast<int>(dataSize), 0);
        if (retVal > 0)
        {
            *pBytesSent = retVal;
            result = Result::Success;
        }
        else
        {
            *pBytesSent = 0;
            if (retVal == 0)
            {
                result = Result::Unavailable;
            }
            else
            {
                result = GetDataError(m_isNonBlocking);
            }
        }

        return result;
    }

    Result Socket::SendTo(const void *pSockAddr, size_t addrSize, const uint8 *pData, size_t dataSize)
    {
        DD_ASSERT(m_socketType == SocketType::Udp);
        // DD_ASSERT(addrSize >= sizeof(sockaddr));

        Result result = Result::Error;

        int retVal = sendto(m_osSocket,
            reinterpret_cast<const char*>(pData),
            static_cast<int>(dataSize),
            0,
            reinterpret_cast<const sockaddr *>(pSockAddr),
            static_cast<int>(addrSize));

        if (retVal > 0)
        {
            DD_ASSERT(static_cast<size_t>(retVal) == dataSize);
            result = Result::Success;
        }
        else
        {
            if (retVal == 0)
            {
                result = Result::Unavailable;
            }
            else
            {
                result = GetDataError(m_isNonBlocking);
            }
        }

        return result;
    }

    Result Socket::Receive(uint8* pBuffer, size_t bufferSize, size_t* pBytesReceived)
    {
        //DD_ASSERT(m_socketType == SocketType::Tcp);

        Result result = Result::Error;

        int retVal = recv(m_osSocket, reinterpret_cast<char*>(pBuffer), static_cast<int>(bufferSize), 0);
        if (retVal > 0)
        {
            *pBytesReceived = retVal;
            result = Result::Success;
        }
        else
        {
            *pBytesReceived = 0;
            if (retVal == 0)
            {
                result = Result::Unavailable;
            }
            else
            {
                result = GetDataError(m_isNonBlocking);
            }
        }
        return result;
    }

    Result Socket::ReceiveFrom(void *pSockAddr, size_t *addrSize, uint8 *pBuffer, size_t bufferSize)
    {
        DD_ASSERT(m_socketType == SocketType::Udp);
        DD_ASSERT(*addrSize >= sizeof(sockaddr));

        Result result = Result::Error;

        int retVal = recvfrom(m_osSocket,
            reinterpret_cast<char*>(pBuffer),
            static_cast<int>(bufferSize),
            0,
            reinterpret_cast<sockaddr *>(pSockAddr),
            reinterpret_cast<int*>(addrSize));

        if (retVal > 0)
        {
            result = Result::Success;
        }
        else
        {
            if (retVal == 0)
            {
                result = Result::Unavailable;
            }
            else
            {
                result = GetDataError(m_isNonBlocking);
            }
        }

        return result;
    }

    Result Socket::Close()
    {
        Result result = Result::Error;
        if (m_osSocket != INVALID_SOCKET)
        {

            // Shut down the socket before closing it.
            // The result doesn't matter since we're closing it anyways.
            shutdown(m_osSocket, SD_BOTH);

            int retVal = closesocket(m_osSocket);
            if (retVal != SOCKET_ERROR)
            {
                result = Result::Success;
            }

            // Clean up the winsock library.
            WSACleanup();
            m_osSocket = INVALID_SOCKET;
        }
        return result;
    }

    Result Socket::GetSocketName(char *pAddress, size_t addrLen, uint32 *pPort)
    {
        Result result = Result::Error;
        int len = sizeof(sockaddr);
        sockaddr addr;
        if (getsockname(m_osSocket, &addr, &len) == 0)
        {
            sockaddr_in* pAddr = reinterpret_cast<sockaddr_in*>(&addr);
            const char* pResult = inet_ntop(AF_INET, reinterpret_cast<void*>(&pAddr->sin_addr), pAddress, addrLen);

            if (pResult != NULL)
            {
                UINT port = ntohs(pAddr->sin_port);
                *pPort = port;
                result = Result::Success;
            }
        }
        return result;
    }

    Result Socket::InitAsClient(OsSocketType socket, const char* pAddress, uint32 port, bool isNonBlocking)
    {
        DD_ASSERT(m_socketType == SocketType::Tcp);

        DD_UNUSED(port);
        DD_UNUSED(pAddress);

        Result result = Result::Success;

        m_isNonBlocking = isNonBlocking;

        m_osSocket = socket;

        result = (m_osSocket != INVALID_SOCKET) ? Result::Success : Result::Error;

        if (result == Result::Success)
        {
            if (m_isNonBlocking)
            {
                // Enable non blocking mode for the socket.
                u_long arg = 1;
                result = (ioctlsocket(m_osSocket, FIONBIO, &arg) != SOCKET_ERROR) ? Result::Success
                    : Result::Error;
            }
        }

        return result;
    }

} // DevDriver
