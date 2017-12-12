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

#include <fcntl.h>
#include <netdb.h>
#include <unistd.h>
#include "os/os_specific.h"

namespace Network
{

Socket *CreateServerSocket(const char *bindaddr, uint16_t port, int queuesize)
{
  int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  int yes = 1;
  setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

  if(s == -1)
    return NULL;

  sockaddr_in addr;
  RDCEraseEl(addr);

  hostent *hp = gethostbyname(bindaddr);

  addr.sin_family = AF_INET;
  memcpy(&addr.sin_addr, hp->h_addr, hp->h_length);
  addr.sin_port = htons(port);

  int result = bind(s, (sockaddr *)&addr, sizeof(addr));
  if(result == -1)
  {
    RDCWARN("Failed to bind to %s:%d - %d", bindaddr, port, errno);
    close(s);
    return NULL;
  }

  result = listen(s, queuesize);
  if(result == -1)
  {
    RDCWARN("Failed to listen on %s:%d - %d", bindaddr, port, errno);
    close(s);
    return NULL;
  }

  int flags = fcntl(s, F_GETFL, 0);
  fcntl(s, F_SETFL, flags | O_NONBLOCK);

  return new Socket((ptrdiff_t)s);
}

};
