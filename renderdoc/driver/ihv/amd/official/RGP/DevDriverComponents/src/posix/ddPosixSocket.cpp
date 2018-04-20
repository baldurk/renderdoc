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
* @file  ddPosixSocket.cpp
* @brief POSIX socket implementation
***********************************************************************************************************************
*/

#include "../ddSocket.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "ddPlatform.h"

namespace DevDriver
{
    Result GetDataError(bool nonBlocking)
    {
        Result result = Result::Error;
        const int err = errno;
        switch (err)
        {
        case EAGAIN:
            if (nonBlocking)
                result = Result::NotReady;
            break;
        case ENOBUFS:
            result = Result::NotReady;
            break;
        case ECONNRESET:
        case ENOTCONN:
        case ENOENT:
        case ENOTDIR:
        case ECONNREFUSED:
        case EHOSTUNREACH:
        case EADDRINUSE:
        case EACCES:
        case ENETDOWN:
            result = Result::Unavailable;
            break;
        }
        return result;
    }

    bool IsRWOperationPending()
    {
        return ((errno == EAGAIN) || (errno == EWOULDBLOCK));
    }

    // =====================================================================================================================
    // Constructs the socket this object encapsulates.
    Socket::Socket()
        : m_osSocket(-1)
        , m_isNonBlocking(false)
        , m_socketType(SocketType::Unknown)
        , m_hints()
        , m_address()
        , m_addressSize(0)
    {
    }

    // =====================================================================================================================
    // Frees the socket this object encapsulates.
    Socket::~Socket()
    {
        if (m_osSocket != -1)
        {
            Close();
        }
    }

    // =====================================================================================================================
    // Initializes socket this object encapsulates.
    Result Socket::Init(bool isNonBlocking, SocketType socketType)
    {
        Result result = Result::Error;

        m_isNonBlocking = isNonBlocking;
        m_socketType = socketType;

        if (m_osSocket == -1)
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
                case SocketType::Local:
                    m_osSocket = socket(AF_UNIX, SOCK_DGRAM, 0);
                    m_hints.ai_family = AF_UNIX;
                    m_hints.ai_socktype = SOCK_DGRAM;
                    m_hints.ai_protocol = 0;
                    break;
                default:
                    break;
            }

            result = (m_osSocket != -1) ? Result::Success : Result::Error;
        }

        if ((result == Result::Success) & m_isNonBlocking)
        {
            // Enable non blocking mode for the socket.
            if (fcntl(m_osSocket, F_SETFL, O_NONBLOCK) != 0)
            {
                result = Result::Error;
            }
        }

        DD_ASSERT(result != Result::Error);
        return result;
    }

    Result Socket::Connect(const char* pAddress, uint32 port)
    {
        char sockAddress[kMaxStringLength] = {};
        size_t addressSize = 0;
        Result result = LookupAddressInfo(pAddress, port, sizeof(sockAddress), &sockAddress[0], &addressSize);
        if (result == Result::Success)
        {
            const int retVal = Platform::RetryTemporaryFailure(connect,
                                                               m_osSocket,
                                                               reinterpret_cast<sockaddr*>(&sockAddress[0]),
                                                               addressSize);

            if (retVal == 0)
            {
                result = Result::Success;
            }
            else if (retVal == -1)
            {
                result = GetDataError(m_isNonBlocking);
            }
            else
            {
                result = Result::Error;
            }
        }
        DD_ASSERT(result != Result::Error);
        return result;
    }

    Result Socket::Select(bool* pReadState, bool* pWriteState, bool* pExceptState, uint32 timeoutInMs)
    {
        Result result = Result::Error;

        fd_set readSet = {};
        fd_set writeSet = {};
        fd_set exceptSet = {};

        FD_SET(m_osSocket, &readSet);
        FD_SET(m_osSocket, &writeSet);
        FD_SET(m_osSocket, &exceptSet);

        timeval timeoutValue = {};
        timeoutValue.tv_sec = static_cast<int32>(timeoutInMs) / 1000;
        timeoutValue.tv_usec = (static_cast<int32>(timeoutInMs) % 1000) * 1000;

        fd_set* pReadSet = ((pReadState != nullptr) ? &readSet : nullptr);
        fd_set* pWriteSet = ((pWriteState != nullptr) ? &writeSet : nullptr);
        fd_set* pExceptSet = ((pExceptState != nullptr) ? &exceptSet : nullptr);

        const int retval = Platform::RetryTemporaryFailure(select,
                                                           m_osSocket + 1,
                                                           pReadSet,
                                                           pWriteSet,
                                                           pExceptSet,
                                                           &timeoutValue);

        if (retval > 0)
        {
            result = Result::Success;
        }
        else
        {
            result = (retval == 0) ? Result::NotReady : Result::Error;
        }

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

        DD_ASSERT(result != Result::Error);
        return result;
    }

    Result Socket::Bind(const char* pAddress, uint32 port)
    {
        Result result = Result::Error;

        if (m_socketType == SocketType::Local)
        {
            DD_ASSERT(sizeof(m_address) >= sizeof(sockaddr_un));

            m_addressSize = 0;

            sockaddr_un* DD_RESTRICT pAddr = reinterpret_cast<sockaddr_un *>(&m_address[0]);
            pAddr->sun_family = AF_UNIX;
            pAddr->sun_path[0] = '\0';

#if defined(DD_LINUX)
            if (pAddress != nullptr)
            {
                // Start the path with a null byte to bind as an abstract socket.
                Platform::Strncpy(pAddr->sun_path + 1, pAddress, sizeof(pAddr->sun_path) - 2);
                m_addressSize = sizeof(sockaddr_un);
            }
            else
            {
                m_addressSize = sizeof(sa_family_t);
            }
#else
            if (pAddress != nullptr)
            {
                Platform::Strncpy(pAddr->sun_path, pAddress, sizeof(pAddr->sun_path) - 1);
            }
            else
            {
                // If pAddress is null we attempt to generate a random filesystem path to bind to
                DD_STATIC_CONST char kUnixSocketTemplate[] =  "/tmp/com.amd.gpuopen-XXXXXX";
                Platform::Strncpy(pAddr->sun_path, kUnixSocketTemplate, sizeof(pAddr->sun_path) - 1);
                const char* pPath = mktemp(pAddr->sun_path);
                DD_ASSERT(pPath != nullptr);
            }
            m_addressSize = SUN_LEN(pAddr);
#endif

            // As a precaution, we unlink the address before we attempt to bind to it
            // We have to do this because we have no way of determining whether or not the file has been orphaned
            // by another process or not. It is *extremely* important to ensure that the bind address passed into
            // the socket is not a file as this can cause data loss.
            if (pAddr->sun_path[0] != 0)
            {
                 unlink(&pAddr->sun_path[0]);
            }

            // We bind the socket to the address that was either provided or generated.
            if (bind(m_osSocket, reinterpret_cast<sockaddr*>(pAddr), m_addressSize) != -1)
            {
                result = Result::Success;
            }
            else
            {
                const int err = errno;
                DD_UNUSED(err);
                DD_ASSERT_REASON("Bind failed");
            }
        }
        else
        {
            addrinfo hints = m_hints;
            hints.ai_flags = AI_PASSIVE;

            char portBuffer[16];
            snprintf(portBuffer, sizeof(portBuffer), "%d", port);

            addrinfo* pResult = nullptr;
            int retVal = getaddrinfo(pAddress, portBuffer, &hints, &pResult);

            if (retVal == 0)
            {
                sockaddr* addr = pResult->ai_addr;
                size_t addrSize = pResult->ai_addrlen;

                if (bind(m_osSocket, addr, static_cast<int>(addrSize)) != -1)
                {
                    result = Result::Success;
                }

                freeaddrinfo(pResult);
            }
        }

        DD_ASSERT(result != Result::Error);
        return result;
    }

    Result Socket::Listen(uint32 backlog)
    {
        DD_ASSERT(m_socketType == SocketType::Tcp);

        Result result = Result::Error;

        if (listen(m_osSocket, backlog) != -1)
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
        socklen_t addrSize = sizeof(addr);

        const int clientSocket = Platform::RetryTemporaryFailure(accept,
                                                                 m_osSocket,
                                                                 &addr,
                                                                 &addrSize);
        if (clientSocket != -1)
        {
            sockaddr_in* pSocket = reinterpret_cast<sockaddr_in*>(&addr);

            const unsigned int addressBufSize = 256;
            char addressBuf[addressBufSize];
            const char* pAddress = inet_ntop(AF_INET, reinterpret_cast<void*>(&pSocket->sin_addr), addressBuf, addressBufSize);

            unsigned int port = ntohs(pSocket->sin_port);

            result = pClientSocket->InitAsClient(clientSocket, pAddress, port, m_isNonBlocking);
        }

        return result;
    }

    Result Socket::LookupAddressInfo(const char* pAddress, uint32 port, size_t addressInfoSize, char* pAddressInfo, size_t *pAddressSize)
    {
        Result result = Result::Error;
        switch (m_socketType)
        {
            case SocketType::Tcp:
            case SocketType::Udp:
            {
                DD_ASSERT(addressInfoSize >= sizeof(sockaddr));

                char portBuffer[16];
                snprintf(portBuffer, sizeof(portBuffer), "%d", port);

                addrinfo* pResult = nullptr;
                int retVal = getaddrinfo(pAddress, portBuffer, &m_hints, &pResult);

                if (retVal == 0)
                {
                    sockaddr* addr = pResult->ai_addr;
                    const size_t& addrSize = pResult->ai_addrlen;
                    if (addressInfoSize >= addrSize)
                    {
                        memcpy(pAddressInfo, addr, addrSize);
                        *pAddressSize = addrSize;
                        result = Result::Success;
                    }

                    freeaddrinfo(pResult);
                }
                break;
            }
            case SocketType::Local:
            {
                DD_ASSERT(addressInfoSize >= sizeof(sockaddr_un));

                sockaddr_un* DD_RESTRICT pAddr = reinterpret_cast<sockaddr_un *>(pAddressInfo);
                pAddr->sun_family = AF_UNIX;

                // Start the path with a null byte to bind as an abstract socket.
                pAddr->sun_path[0] = '\0';
                Platform::Strncpy(pAddr->sun_path + 1, pAddress, sizeof(pAddr->sun_path) - 2);
                *pAddressSize = sizeof(sockaddr_un);
                result = Result::Success;
                break;
            }
            default:
                DD_UNREACHABLE();
                break;
        }
        DD_ASSERT(result != Result::Error);
        return result;
    }

    Result Socket::Send(const uint8* pData, size_t dataSize, size_t* pBytesSent)
    {
        Result result = Result::Error;

        const int retVal = Platform::RetryTemporaryFailure(send,
                                                           m_osSocket,
                                                           reinterpret_cast<const char*>(pData),
                                                           static_cast<int>(dataSize),
                                                           0);
        if (retVal != -1)
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
        DD_ASSERT((m_socketType == SocketType::Udp) || (m_socketType == SocketType::Local));

        Result result = Result::Error;

        const int retVal = Platform::RetryTemporaryFailure(sendto,
                                                           m_osSocket,
                                                           reinterpret_cast<const char*>(pData),
                                                           static_cast<int>(dataSize),
                                                           0,
                                                           reinterpret_cast<const sockaddr *>(pSockAddr),
                                                           addrSize);

        if (static_cast<size_t>(retVal) == dataSize)
        {
            result = Result::Success;
        }
        else if (retVal == 0)
        {
            result = Result::Unavailable;
        }
        else
        {
            result = GetDataError(m_isNonBlocking);
        }

        return result;
    }

    Result Socket::Receive(uint8* pBuffer, size_t bufferSize, size_t* pBytesReceived)
    {
        Result result = Result::Error;

        const int retVal = Platform::RetryTemporaryFailure(recv,
                                                           m_osSocket,
                                                           reinterpret_cast<char*>(pBuffer),
                                                           static_cast<int>(bufferSize),
                                                           0);
        if (retVal > 0)
        {
            *pBytesReceived = retVal;
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

    Result Socket::ReceiveFrom(void *pSockAddr, size_t *addrSize, uint8 *pBuffer, size_t bufferSize)
    {
        DD_ASSERT((m_socketType == SocketType::Udp) || (m_socketType == SocketType::Local));
        DD_ASSERT(*addrSize >= sizeof(sockaddr));

        Result result = Result::Error;

        const int retVal = Platform::RetryTemporaryFailure(recvfrom,
                                                           m_osSocket,
                                                           reinterpret_cast<char*>(pBuffer),
                                                           static_cast<int>(bufferSize),
                                                           0,
                                                           reinterpret_cast<sockaddr *>(pSockAddr),
                                                           reinterpret_cast<socklen_t*>(addrSize));

        if (retVal > 0)
        {
            result = Result::Success;
        }
        else if (retVal == 0)
        {
            result = Result::Unavailable;
        }
        else
        {
            result = GetDataError(m_isNonBlocking);
        }

        return result;
    }

    Result Socket::Close()
    {
        Result result = Result::Error;

        // Shut down the socket before closing it.
        // The result doesn't matter since we're closing it anyways.
        shutdown(m_osSocket, SHUT_RDWR);

        if (close(m_osSocket) != -1)
        {
            m_osSocket = -1;
            if (m_socketType == SocketType::Local)
            {
                sockaddr_un* DD_RESTRICT pAddr = reinterpret_cast<sockaddr_un *>(&m_address[0]);
                // If the socket wasn't in the abstract namespace, unlink it from the filesystem
                if (pAddr->sun_path[0] != 0)
                {
                    unlink(&pAddr->sun_path[0]);
                }
            }
            result = Result::Success;
        }

        return result;
    }

    Result Socket::GetSocketName(char *pAddress, size_t addrLen, uint32 *pPort)
    {
        Result result = Result::Error;
        socklen_t len = static_cast<socklen_t>(sizeof(sockaddr));
        sockaddr addr;
        if (getsockname(m_osSocket, &addr, &len) == 0)
        {
            sockaddr_in* pAddr = reinterpret_cast<sockaddr_in*>(&addr);
            const char* pResult = inet_ntop(AF_INET, reinterpret_cast<void*>(&pAddr->sin_addr), pAddress, addrLen);

            if (pResult != NULL)
            {
                unsigned int port = ntohs(pAddr->sin_port);
                *pPort = port;
                result = Result::Success;
            }
        }
        return result;
    }

    Result Socket::InitAsClient(OsSocketType socket, const char* pAddress, uint32 port, bool isNonBlocking)
    {

        DD_ASSERT(m_socketType == SocketType::Tcp);
        DD_UNUSED(pAddress);
        DD_UNUSED(port);

        Result result = Result::Success;

        m_isNonBlocking = isNonBlocking;

        m_osSocket = socket;

        result = (m_osSocket != -1) ? Result::Success : Result::Error;

        if (result == Result::Success)
        {
            if (m_isNonBlocking)
            {
                // Enable non blocking mode for the socket.
                result = (fcntl(m_osSocket, F_SETFL, O_NONBLOCK) == 0) ? Result::Success : Result::Error;
            }
        }

        return result;
    }

} // DevDriver
