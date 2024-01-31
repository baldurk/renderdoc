/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include <arpa/inet.h>
#include <unistd.h>
#include "os/os_specific.h"
#include "os/posix/posix_network.h"

namespace Network
{
void SocketPostSend()
{
  // we need to throttle android sending to ensure it never gets ahead of the PC otherwise the
  // forwarded port may encounter a EWOULDBLOCK error.
  // adb is buggy and will scompletely drop all writes as soon as a write blocks:
  // https://issuetracker.google.com/issues/139078301
  //
  // Throttling the device end is a hack but is reasonably reliable as we assume the PC side is fast
  // enough to read it. Since we batch most sends, sleeping on each send is not too costly, but this
  // may have some impact especially around small packets (we force a flush on the end of each
  // chunk/packet).
  usleep(1500);
}

uint32_t Socket::GetRemoteIP() const
{
  // Android uses abstract sockets which are only "localhost" accessible
  return MakeIP(127, 0, 0, 1);
}

Socket *CreateServerSocket(const rdcstr &, uint16_t port, int queuesize)
{
  return CreateAbstractServerSocket(port, queuesize);
}
};
