/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2017 Baldur Karlsson
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

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <string>
#include "os/os_specific.h"
#include "strings/string_utils.h"

// defined in foo/foo_network.cpp
Network::Socket *CreateServerSocket(const char *, uint16_t, int);

using std::string;

// because strerror_r is a complete mess...
static std::string errno_string(int err)
{
  switch(err)
  {
#if EAGAIN != EWOULDBLOCK
    case EAGAIN:
#endif
    case EWOULDBLOCK: return "EWOULDBLOCK: Operation would block.";
    case EINVAL: return "EINVAL: Invalid argument.";
    case EADDRINUSE: return "EADDRINUSE: Address already in use.";
    case ECONNRESET: return "ECONNRESET: A connection was forcibly closed by a peer.";
    case EINPROGRESS: return "EINPROGRESS: Operation now in progress.";
    case EINTR:
      return "EINTR: The function was interrupted by a signal that was caught, before any data was "
             "available.";
    case ETIMEDOUT: return "ETIMEDOUT: A socket operation timed out.";
    case ECONNABORTED: return "ECONNABORTED: A connection has been aborted.";
    case ECONNREFUSED: return "ECONNREFUSED: A connection was refused.";
    case EHOSTDOWN: return "EHOSTDOWN: Host is down.";
    case EHOSTUNREACH: return "EHOSTUNREACH: No route to host.";
    default: break;
  }

  return StringFormat::Fmt("Unknown error %d", err);
}

namespace Network
{
void Init()
{
}

void Shutdown()
{
}

Socket::~Socket()
{
  Shutdown();
}

void Socket::Shutdown()
{
  if(Connected())
  {
    shutdown((int)socket, SHUT_RDWR);
    close((int)socket);
    socket = -1;
  }
}

bool Socket::Connected() const
{
  return (int)socket != -1;
}

uint32_t Socket::GetRemoteIP() const
{
  sockaddr_in addr = {};
  socklen_t len = sizeof(addr);

  getpeername((int)socket, (sockaddr *)&addr, &len);

 if (addr.sin_family == AF_UNIX)
    return Network::MakeIP(127, 0, 0, 1);

  return ntohl(addr.sin_addr.s_addr);
}

Socket *Socket::AcceptClient(bool wait)
{
  do
  {
    int s = accept(socket, NULL, NULL);

    if(s != -1)
    {
      int flags = fcntl(s, F_GETFL, 0);
      fcntl(s, F_SETFL, flags | O_NONBLOCK);

      int nodelay = 1;
      setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay, sizeof(nodelay));

      return new Socket((ptrdiff_t)s);
    }

    int err = errno;

    if(err != EWOULDBLOCK && err != EAGAIN)
    {
      RDCWARN("accept: %s", errno_string(err).c_str());
      Shutdown();
    }

    Threading::Sleep(4);
  } while(wait);

  return NULL;
}

bool Socket::SendDataBlocking(const void *buf, uint32_t length)
{
  if(length == 0)
    return true;

  uint32_t sent = 0;

  char *src = (char *)buf;

  int flags = fcntl(socket, F_GETFL, 0);
  fcntl(socket, F_SETFL, flags & ~O_NONBLOCK);

  timeval oldtimeout = {0};
  socklen_t len = sizeof(oldtimeout);
  getsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (char *)&oldtimeout, &len);

  timeval timeout = {0};
  timeout.tv_sec = (timeoutMS / 1000);
  timeout.tv_usec = (timeoutMS % 1000) * 1000;
  setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));

  while(sent < length)
  {
    int ret = send(socket, src, length - sent, 0);

    if(ret <= 0)
    {
      int err = errno;

      if(err == EWOULDBLOCK || err == EAGAIN)
      {
        RDCWARN("Timeout in send");
        Shutdown();
        return false;
      }
      else
      {
        RDCWARN("send: %s", errno_string(err).c_str());
        Shutdown();
        return false;
      }
    }

    sent += ret;
    src += ret;
  }

  flags = fcntl(socket, F_GETFL, 0);
  fcntl(socket, F_SETFL, flags | O_NONBLOCK);

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
    int err = errno;

    if(err == EWOULDBLOCK || err == EAGAIN)
    {
      ret = 0;
    }
    else
    {
      RDCWARN("recv: %s", errno_string(err).c_str());
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
    int err = errno;

    if(err == EWOULDBLOCK || err == EAGAIN)
    {
      return true;
    }
    else
    {
      RDCWARN("recv: %s", errno_string(err).c_str());
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

  int flags = fcntl(socket, F_GETFL, 0);
  fcntl(socket, F_SETFL, flags & ~O_NONBLOCK);

  timeval oldtimeout = {0};
  socklen_t len = sizeof(oldtimeout);
  getsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&oldtimeout, &len);

  timeval timeout = {0};
  timeout.tv_sec = (timeoutMS / 1000);
  timeout.tv_usec = (timeoutMS % 1000) * 1000;
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
      int err = errno;

      if(err == EWOULDBLOCK || err == EAGAIN)
      {
        RDCWARN("Timeout in recv");
        Shutdown();
        return false;
      }
      else
      {
        RDCWARN("recv: %s", errno_string(err).c_str());
        Shutdown();
        return false;
      }
    }

    received += ret;
    dst += ret;
  }

  flags = fcntl(socket, F_GETFL, 0);
  fcntl(socket, F_SETFL, flags | O_NONBLOCK);

  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&oldtimeout, sizeof(oldtimeout));

  RDCASSERT(received == length);

  return true;
}

Socket *CreateClientSocket(const char *host, uint16_t port, int timeoutMS)
{
  char portstr[7] = {0};
  StringFormat::snprintf(portstr, 6, "%d", port);

  addrinfo hints;
  RDCEraseEl(hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;

  addrinfo *result = NULL;
  getaddrinfo(host, portstr, &hints, &result);

  for(addrinfo *ptr = result; ptr != NULL; ptr = ptr->ai_next)
  {
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if(s == -1)
      return NULL;

    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);

    int result = connect(s, ptr->ai_addr, (int)ptr->ai_addrlen);
    if(result == -1)
    {
      fd_set set;
      FD_ZERO(&set);
      FD_SET(s, &set);

      int err = errno;

      if(err == EWOULDBLOCK || err == EINPROGRESS)
      {
        timeval timeout;
        timeout.tv_sec = (timeoutMS / 1000);
        timeout.tv_usec = (timeoutMS % 1000) * 1000;
        result = select(s + 1, NULL, &set, NULL, &timeout);

        if(result <= 0)
        {
          RDCDEBUG("Timed out");
          close(s);
          continue;
        }

        socklen_t len = sizeof(err);
        getsockopt(s, SOL_SOCKET, SO_ERROR, &err, &len);
      }

      if(err != 0)
      {
        RDCDEBUG("%s", errno_string(err).c_str());
        close(s);
        continue;
      }
    }

    int nodelay = 1;
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay, sizeof(nodelay));

    return new Socket((ptrdiff_t)s);
  }

  RDCDEBUG("Failed to connect to %s:%d", host, port);
  return NULL;
}

bool ParseIPRangeCIDR(const char *str, uint32_t &ip, uint32_t &mask)
{
  uint32_t a = 0, b = 0, c = 0, d = 0, num = 0;

  int ret = sscanf(str, "%u.%u.%u.%u/%u", &a, &b, &c, &d, &num);

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
