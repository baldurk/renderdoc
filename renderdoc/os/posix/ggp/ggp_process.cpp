/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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
#include <algorithm>
#include "os/os_specific.h"

extern char **environ;

// we wait 1ns, then 2ns, then 4ns, etc so our total is 0xfff etc
// 0xfffff == ~1s
#define INITIAL_WAIT_TIME 1
#define MAX_WAIT_TIME 0xfffff

char **GetCurrentEnvironment()
{
  return environ;
}

std::vector<int> getSockets(pid_t childPid)
{
  std::vector<int> sockets;
  std::string dirPath = StringFormat::Fmt("/proc/%d/fd", (int)childPid);
  std::vector<PathEntry> files = FileIO::GetFilesInDirectory(dirPath.c_str());
  if(files.empty())
    return sockets;

  for(const PathEntry &file : files)
  {
    std::string target = StringFormat::Fmt("%s/%s", dirPath.c_str(), file.filename.c_str());
    char linkname[1024];
    ssize_t length = readlink(target.c_str(), linkname, 1023);
    if(length == -1)
      continue;

    linkname[length] = '\0';
    uint32_t inode = 0;
    int num = sscanf(linkname, "socket:[%u]", &inode);
    if(num == 1)
      sockets.push_back(inode);
  }
  return sockets;
}

int GetIdentPort(pid_t childPid)
{
  int ret = 0;

  std::string procfile = StringFormat::Fmt("/proc/%d/net/tcp", (int)childPid);

  int waitTime = INITIAL_WAIT_TIME;

  // try for a little while for the /proc entry to appear
  while(ret == 0 && waitTime <= MAX_WAIT_TIME)
  {
    // back-off for each retry
    usleep(waitTime);

    waitTime *= 2;

    FILE *f = FileIO::fopen(procfile.c_str(), "r");

    if(f == NULL)
    {
      // try again in a bit
      continue;
    }

    std::vector<int> sockets = getSockets(childPid);

    // read through the proc file to check for an open listen socket
    while(ret == 0 && !feof(f))
    {
      const size_t sz = 512;
      char line[sz];
      line[sz - 1] = 0;
      fgets(line, sz - 1, f);

      // an example for a line:
      // sl local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt  uid timeout inode
      // 0: 00000000:9808 00000000:0000 0A 00000000:00000000 00:00000000 00000000 1000    0   109747

      int socketnum = 0, hexip = 0, hexport = 0, inode = 0;
      int num = sscanf(line, " %d: %x:%x %*x:%*x %*x %*x:%*x %*x:%*x %*x %*d %*d %d", &socketnum,
                       &hexip, &hexport, &inode);

      // find open listen socket on 0.0.0.0:port
      if(num == 4 && hexip == 0 && hexport >= RenderDoc_FirstTargetControlPort &&
         hexport <= RenderDoc_LastTargetControlPort &&
         std::find(sockets.begin(), sockets.end(), inode) != sockets.end())
      {
        ret = hexport;
      }
    }

    FileIO::fclose(f);
  }

  if(ret == 0)
  {
    RDCWARN("Couldn't locate renderdoc target control listening port between %u and %u in %s",
            (uint32_t)RenderDoc_FirstTargetControlPort, (uint32_t)RenderDoc_LastTargetControlPort,
            procfile.c_str());
  }

  return ret;
}

// because OSUtility::DebuggerPresent is called often we want it to be
// cheap. Opening and parsing a file would cause high overhead on each
// call, so instead we just cache it at startup. This fails in the case
// of attaching to processes
bool debuggerPresent = false;

void CacheDebuggerPresent()
{
  FILE *f = FileIO::fopen("/proc/self/status", "r");

  if(f == NULL)
  {
    RDCWARN("Couldn't open /proc/self/status");
    return;
  }

  // read through the proc file to check for TracerPid
  while(!feof(f))
  {
    const size_t sz = 512;
    char line[sz];
    line[sz - 1] = 0;
    fgets(line, sz - 1, f);

    int tracerpid = 0;
    int num = sscanf(line, "TracerPid: %d", &tracerpid);

    // found TracerPid line
    if(num == 1)
    {
      debuggerPresent = (tracerpid != 0);
      break;
    }
  }

  FileIO::fclose(f);
}

bool OSUtility::DebuggerPresent()
{
  return debuggerPresent;
}

const char *Process::GetEnvVariable(const char *name)
{
  return getenv(name);
}

uint64_t Process::GetMemoryUsage()
{
  FILE *f = FileIO::fopen("/proc/self/statm", "r");

  if(f == NULL)
  {
    RDCWARN("Couldn't open /proc/self/statm");
    return 0;
  }

  char line[512] = {};
  fgets(line, 511, f);

  uint32_t vmPages = 0;
  int num = sscanf(line, "%u", &vmPages);

  if(num == 1 && vmPages > 0)
    return vmPages * (uint64_t)sysconf(_SC_PAGESIZE);

  return 0;
}
