/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Samsung Electronics (UK) Limited
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

#include "socket.hpp"

#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>

#include <arpa/inet.h>
#include <sys/socket.h>

namespace lizard
{
using namespace std;

Socket *Socket::createConnection(const char *host, uint32_t port)
{
  struct sockaddr_in serv_addr;
  int sock;
  if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    return NULL;
  }
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(port);

  if(inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0)
  {
    return NULL;
  }
  int connectResult = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  if(connectResult < 0)
  {
    return NULL;
  }
  return new lizard::Socket(sock);
}

void Socket::destroyConnection(Socket *connection)
{
  delete connection;
}

static Socket::Result toSocketResult(ssize_t value, int lastErrno)
{
  if(value > 0)
    return Socket::SUCCESS;
  if(value == 0)
    return Socket::CONNECTION_CLOSED;

  switch(lastErrno)
  {
    case EAGAIN: return Socket::WOULD_BLOCK;
    case ECONNABORTED:
    case ECONNRESET: return Socket::CONNECTION_TERMINATED;
    default: return Socket::ERROR;
  }
}

Socket::Socket(int fd) : m_fd(fd)
{
}

Socket::~Socket()
{
  close();
}

Socket::Result Socket::send(const void *buffer, size_t bufferSize, size_t *bytesSent)
{
  ssize_t bytesWritten = ::send(m_fd, (const char *)buffer, bufferSize, 0);
  Socket::Result result = toSocketResult(bytesWritten, errno);

  if(bytesSent != NULL)
    *bytesSent = bytesWritten > 0 ? (size_t)bytesWritten : 0;

  return result;
}

Socket::Result Socket::receive(void *buffer, size_t bufferSize, size_t *bytesRecv)
{
  ssize_t bytesRead = ::recv(m_fd, (char *)buffer, bufferSize, 0);
  Result result = toSocketResult(bytesRead, errno);

  if(bytesRecv != NULL)
    *bytesRecv = bytesRead > 0 ? (size_t)bytesRead : 0;

  return result;
}

Socket::Result Socket::receiveAll(void *buffer, size_t bufferSize, size_t *bytesRecv)
{
  size_t bytesRead = 0;
  Result result;
  while(bytesRead < bufferSize)
  {
    bytesRead += ::recv(m_fd, (char *)buffer + bytesRead, bufferSize - bytesRead, 0);
    result = toSocketResult(bytesRead, errno);
    if(result != SUCCESS)
    {
      return result;
    }
  }
  if(bytesRecv != NULL)
    *bytesRecv = bytesRead > 0 ? (size_t)bytesRead : 0;
  return result;
}

Socket::Result Socket::shutdown()
{
  int shutdownResult = ::shutdown(m_fd, SHUT_RDWR);
  return shutdownResult == 0 ? SUCCESS : ERROR;
}

Socket::Result Socket::close()
{
  int closeResult = ::close(m_fd);
  return closeResult == 0 ? SUCCESS : ERROR;
}

string Socket::resultstr(Socket::Result result)
{
  switch(result)
  {
    case SUCCESS: return "SUCCESS";
    case WOULD_BLOCK: return "WOULD_BLOCK";
    case CONNECTION_TERMINATED: return "CONNECTION_TERMINATED";
    case CONNECTION_CLOSED: return "CONNECTION_CLOSED";
    case ERROR: return "ERROR";
    default: return "UNSPECIFIED RESULT";
  }
}

} /* namespace lizard */
