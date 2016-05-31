/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include <unistd.h>
#include "os/os_specific.h"

extern char **environ;

char **GetCurrentEnvironment()
{
  return environ;
}

int GetIdentPort(pid_t childPid)
{
  int ret = 0;

  string procfile = StringFormat::Fmt("/proc/%d/net/tcp", (int)childPid);

  // try for a little while for the /proc entry to appear
  for(int retry = 0; retry < 10; retry++)
  {
    // back-off for each retry
    usleep(1000 + 500 * retry);

    FILE *f = FileIO::fopen(procfile.c_str(), "r");

    if(f == NULL)
    {
      // try again in a bit
      continue;
    }

    // read through the proc file to check for an open listen socket
    while(ret == 0 && !feof(f))
    {
      const size_t sz = 512;
      char line[sz];
      line[sz - 1] = 0;
      fgets(line, sz - 1, f);

      int socketnum = 0, hexip = 0, hexport = 0;
      int num = sscanf(line, " %d: %x:%x", &socketnum, &hexip, &hexport);

      // find open listen socket on 0.0.0.0:port
      if(num == 3 && hexip == 0 && hexport >= RenderDoc_FirstCaptureNetworkPort &&
         hexport <= RenderDoc_LastCaptureNetworkPort)
      {
        ret = hexport;
        break;
      }
    }

    FileIO::fclose(f);
  }

  return ret;
}
