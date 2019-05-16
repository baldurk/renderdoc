/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include <winsock2.h>
#include <ws2tcpip.h>
#include "os/os_specific.h"

#ifndef WSA_FLAG_NO_HANDLE_INHERIT
#define WSA_FLAG_NO_HANDLE_INHERIT 0x80
#endif

#ifndef WSA_FLAG_OVERLAPPED
#define WSA_FLAG_OVERLAPPED 0x01
#endif

static std::string wsaerr_string(int err)
{
  switch(err)
  {
    case WSAENOTSOCK:
      return "WSAENOTSOCK: An operation was attempted on something that is not a socket";
    case WSAEWOULDBLOCK:
      return "WSAEWOULDBLOCK: A non-blocking socket operation could not be completed immediately";
    case WSAEADDRINUSE:
      return "WSAEADDRINUSE: Only one usage of each socket address (protocol/network address/port) "
             "is normally permitted.";
    case WSAENETDOWN: return "WSAENETDOWN: A socket operation encountered a dead network.";
    case WSAENETUNREACH:
      return "WSAENETUNREACH: A socket operation was attempted to an unreachable network.";
    case WSAENETRESET:
      return "WSAENETRESET: The connection has been broken due to keep-alive activity detecting a "
             "failure while the operation was in progress.";
    case WSAECONNABORTED:
      return "WSAECONNABORTED: An established connection was aborted by the software in your host "
             "machine.";
    case WSAECONNRESET:
      return "WSAECONNRESET: An existing connection was forcibly closed by the remote host.";
    case WSAETIMEDOUT: return "WSAETIMEDOUT: A socket operation timed out.";
    case WSAECONNREFUSED:
      return "WSAECONNREFUSED: No connection could be made because the target machine actively "
             "refused "
             "it.";
    case WSAEHOSTDOWN:
      return "WSAEHOSTDOWN: A socket operation failed because the destination host was down.";
    case WSAEHOSTUNREACH:
      return "WSAETIMEDOUT: A socket operation was attempted to an unreachable host.";
    case WSATRY_AGAIN: return "WSATRY_AGAIN: A temporary failure in name resolution occurred.";
    case WSAEINVAL:
      return "WSAEINVAL: An invalid value was provided for the ai_flags member of the pHints "
             "parameter.";
    case WSANO_RECOVERY:
      return "WSANO_RECOVERY: A nonrecoverable failure in name resolution occurred.";
    case WSAEAFNOSUPPORT:
      return "WSAEAFNOSUPPORT: The ai_family member of the pHints parameter is not supported.";
    case WSA_NOT_ENOUGH_MEMORY:
      return "WSA_NOT_ENOUGH_MEMORY: A memory allocation failure occurred.";
    case WSAHOST_NOT_FOUND:
      return "WSAHOST_NOT_FOUND: The name does not resolve for the supplied parameters or the "
             "pNodeName and pServiceName parameters were not provided.";
    case WSATYPE_NOT_FOUND:
      return "WSATYPE_NOT_FOUND: The pServiceName parameter is not supported for the specified "
             "ai_socktype member of the pHints parameter.";
    case WSAESOCKTNOSUPPORT:
      return "WSAESOCKTNOSUPPORT: The ai_socktype member of the pHints parameter is not supported.";
    case WSANO_DATA:
      return "WSANO_DATA: The requested name is valid, but no data of the requested type was "
             "found.";
    case WSANOTINITIALISED:
      return "WSANOTINITIALISED: A successful WSAStartup call must occur before using this "
             "function.";
    default: break;
  }

  return StringFormat::Fmt("Unknown error %d", err);
}

namespace Network
{
void Init()
{
  WSAData wsaData = {0};
  WSAStartup(MAKEWORD(2, 2), &wsaData);
}

void Shutdown()
{
  WSACleanup();
}

Socket::~Socket()
{
  Shutdown();
}

void Socket::Shutdown()
{
  if(Connected())
  {
    shutdown((SOCKET)socket, SD_BOTH);
    closesocket((SOCKET)socket);
    socket = -1;
  }
}

bool Socket::Connected() const
{
  return (SOCKET)socket != INVALID_SOCKET;
}

uint32_t Socket::GetRemoteIP() const
{
  sockaddr_in addr = {};
  socklen_t len = sizeof(addr);

  getpeername((SOCKET)socket, (sockaddr *)&addr, &len);

  return ntohl(addr.sin_addr.s_addr);
}

Socket *Socket::AcceptClient(uint32_t timeoutMilliseconds)
{
  do
  {
    SOCKET s = accept(socket, NULL, NULL);

    if(s != INVALID_SOCKET)
    {
      u_long enable = 1;
      ioctlsocket(s, FIONBIO, &enable);

      BOOL nodelay = TRUE;
      setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));

      return new Socket((ptrdiff_t)s);
    }

    int err = WSAGetLastError();

    if(err != WSAEWOULDBLOCK)
    {
      RDCWARN("accept: %s", wsaerr_string(err).c_str());
      Shutdown();
    }

    const uint32_t sleeptime = 4;

    Threading::Sleep(sleeptime);

    if(sleeptime < timeoutMilliseconds)
      timeoutMilliseconds -= sleeptime;
    else
      timeoutMilliseconds = 0U;
  } while(timeoutMilliseconds);

  return NULL;
}

bool Socket::SendDataBlocking(const void *buf, uint32_t length)
{
  if(length == 0)
    return true;

  uint32_t sent = 0;

  char *src = (char *)buf;

  u_long enable = 0;
  ioctlsocket(socket, FIONBIO, &enable);

  DWORD oldtimeout = 0;
  int len = sizeof(oldtimeout);
  getsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&oldtimeout, &len);

  DWORD timeout = timeoutMS;
  setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));

  while(sent < length)
  {
    int ret = send(socket, src, length - sent, 0);

    if(ret <= 0)
    {
      int err = WSAGetLastError();

      if(err == WSAEWOULDBLOCK || err == WSAETIMEDOUT)
      {
        RDCWARN("Timeout in send");
        Shutdown();
        return false;
      }
      else
      {
        RDCWARN("send: %s", wsaerr_string(err).c_str());
        Shutdown();
        return false;
      }
    }

    sent += ret;
    src += ret;
  }

  enable = 1;
  ioctlsocket(socket, FIONBIO, &enable);

  setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&oldtimeout, sizeof(oldtimeout));

  RDCASSERT(sent == length);

  return true;
}

bool Socket::IsRecvDataWaiting()
{
  char dummy;
  int ret = recv(socket, &dummy, 1, MSG_PEEK);

  if(ret == 0)
  {
    Shutdown();
    return false;
  }
  else if(ret <= 0)
  {
    int err = WSAGetLastError();

    if(err == WSAEWOULDBLOCK)
    {
      ret = 0;
    }
    else
    {
      RDCWARN("recv: %s", wsaerr_string(err).c_str());
      Shutdown();
      return false;
    }
  }

  return ret > 0;
}

bool Socket::RecvDataNonBlocking(void *buf, uint32_t &length)
{
  if(length == 0)
    return true;

  // socket is already blocking, don't have to change anything
  int ret = recv(socket, (char *)buf, length, 0);

  if(ret > 0)
  {
    length = (uint32_t)ret;
  }
  else
  {
    length = 0;
    int err = WSAGetLastError();

    if(err == WSAEWOULDBLOCK)
    {
      return true;
    }
    else
    {
      RDCWARN("recv: %s", wsaerr_string(err).c_str());
      Shutdown();
      return false;
    }
  }

  return true;
}

bool Socket::RecvDataBlocking(void *buf, uint32_t length)
{
  if(length == 0)
    return true;

  uint32_t received = 0;

  char *dst = (char *)buf;

  u_long enable = 0;
  ioctlsocket(socket, FIONBIO, &enable);

  DWORD oldtimeout = 0;
  int len = sizeof(oldtimeout);
  getsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&oldtimeout, &len);

  DWORD timeout = timeoutMS;
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));

  while(received < length)
  {
    int ret = recv(socket, dst, length - received, 0);

    if(ret == 0)
    {
      Shutdown();
      return false;
    }
    else if(ret <= 0)
    {
      int err = WSAGetLastError();

      if(err == WSAEWOULDBLOCK || err == WSAETIMEDOUT)
      {
        RDCWARN("Timeout in recv");
        Shutdown();
        return false;
      }
      else
      {
        RDCWARN("recv: %s", wsaerr_string(err).c_str());
        Shutdown();
        return false;
      }
    }

    received += ret;
    dst += ret;
  }

  enable = 1;
  ioctlsocket(socket, FIONBIO, &enable);

  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&oldtimeout, sizeof(oldtimeout));

  RDCASSERT(received == length);

  return true;
}

Socket *CreateServerSocket(const char *bindaddr, uint16_t port, int queuesize)
{
  SOCKET s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
                       WSA_FLAG_NO_HANDLE_INHERIT | WSA_FLAG_OVERLAPPED);

  if(s == INVALID_SOCKET)
    return NULL;

  sockaddr_in addr;
  RDCEraseEl(addr);

  addr.sin_family = AF_INET;
  inet_pton(AF_INET, bindaddr, &addr.sin_addr);
  addr.sin_port = htons(port);

  int result = bind(s, (SOCKADDR *)&addr, sizeof(addr));
  if(result == SOCKET_ERROR)
  {
    RDCWARN("Failed to bind to %s:%d - %d", bindaddr, port, WSAGetLastError());
    closesocket(s);
    return NULL;
  }

  result = listen(s, queuesize);
  if(result == SOCKET_ERROR)
  {
    RDCWARN("Failed to listen on %s:%d - %d", bindaddr, port, WSAGetLastError());
    closesocket(s);
    return NULL;
  }

  u_long nonblock = 1;
  ioctlsocket(s, FIONBIO, &nonblock);

  return new Socket((ptrdiff_t)s);
}

Socket *CreateClientSocket(const char *host, uint16_t port, int timeoutMS)
{
  wchar_t portwstr[7] = {0};

  {
    char buf[7] = {0};
    int n = StringFormat::snprintf(buf, 6, "%d", port);
    for(int i = 0; i < n && i < 6; i++)
      portwstr[i] = (wchar_t)buf[i];
  }

  addrinfoW hints;
  RDCEraseEl(hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  std::wstring whost = StringFormat::UTF82Wide(std::string(host));

  addrinfoW *addrResult = NULL;
  int res = GetAddrInfoW(whost.c_str(), portwstr, &hints, &addrResult);
  if(res != 0)
  {
    RDCDEBUG("%s", wsaerr_string(res).c_str());
    return NULL;
  }

  for(addrinfoW *ptr = addrResult; ptr != NULL; ptr = ptr->ai_next)
  {
    SOCKET s = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0,
                         WSA_FLAG_NO_HANDLE_INHERIT | WSA_FLAG_OVERLAPPED);

    if(s == INVALID_SOCKET)
    {
      FreeAddrInfoW(addrResult);
      return NULL;
    }

    u_long enable = 1;
    ioctlsocket(s, FIONBIO, &enable);

    int result = connect(s, ptr->ai_addr, (int)ptr->ai_addrlen);
    if(result == SOCKET_ERROR)
    {
      fd_set setW = {}, setE = {};
      FD_ZERO(&setW);
      FD_ZERO(&setE);

// macro FD_SET contains the do { } while(0) idiom, which warns
#pragma warning(push)
#pragma warning(disable : 4127)    // conditional expression is constant
      FD_SET(s, &setW);
      FD_SET(s, &setE);
#pragma warning(pop)

      int err = WSAGetLastError();

      if(err == WSAEWOULDBLOCK)
      {
        timeval timeout;
        timeout.tv_sec = (timeoutMS / 1000);
        timeout.tv_usec = (timeoutMS % 1000) * 1000;
        result = select((int)s + 1, NULL, &setW, &setE, &timeout);

        socklen_t len = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &len);

        // if select never returned, if the timeout is less than 1 second we treat it as a
        // connection refused. This is inaccurate but we don't want to have to wait a full second
        // for the connect to time out. On Winsock there seems to be a minimum of 1 second before
        // it will actually return connection refused.
        if(result <= 0 && timeoutMS <= 1000)
        {
          err = WSAECONNREFUSED;
        }
      }

      if(err != 0)
      {
        RDCDEBUG("%s", wsaerr_string(err).c_str());
        closesocket(s);
        continue;
      }
    }

    BOOL nodelay = TRUE;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));

    FreeAddrInfoW(addrResult);

    return new Socket((ptrdiff_t)s);
  }

  FreeAddrInfoW(addrResult);

  RDCDEBUG("Failed to connect to %s:%d", host, port);
  return NULL;
}

bool ParseIPRangeCIDR(const char *str, uint32_t &ip, uint32_t &mask)
{
  uint32_t a = 0, b = 0, c = 0, d = 0, num = 0;

  int ret = sscanf_s(str, "%u.%u.%u.%u/%u", &a, &b, &c, &d, &num);

  if(ret != 5 || a > 255 || b > 255 || c > 255 || d > 255 || num > 32)
  {
    ip = 0;
    mask = 0;
    return false;
  }

  ip = MakeIP(a, b, c, d);

  if(num == 0)
  {
    mask = 0;
  }
  else
  {
    num = 32 - num;
    mask = ((~0U) >> num) << num;
  }

  return true;
}
};
