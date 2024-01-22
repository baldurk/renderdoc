/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include <dlfcn.h>
#include <elf.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ptrace.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include "api/replay/data_types.h"
#include "common/common.h"
#include "common/formatting.h"
#include "core/core.h"
#include "core/settings.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

#include <asm/ptrace.h>

RDOC_CONFIG(bool, Linux_PtraceChildProcesses, true,
            "Use ptrace(2) to trace child processes at startup to ensure connection is made as "
            "early as possible.");
RDOC_CONFIG(bool, Linux_Debug_PtraceLogging, false, "Enable verbose debug logging of ptrace usage.");

extern char **environ;

// we wait 1ns, then 2ns, then 4ns, etc so our total is 0xfff etc
// 0xfffff == ~1s
#define INITIAL_WAIT_TIME 1
#define MAX_WAIT_TIME 0xfffff

char **GetCurrentEnvironment()
{
  return environ;
}

rdcarray<int> getSockets(pid_t childPid)
{
  rdcarray<int> sockets;
  rdcstr dirPath = StringFormat::Fmt("/proc/%d/fd", (int)childPid);
  rdcarray<PathEntry> files;
  FileIO::GetFilesInDirectory(dirPath, files);
  if(files.empty())
    return sockets;

  for(const PathEntry &file : files)
  {
    rdcstr target = StringFormat::Fmt("%s/%s", dirPath.c_str(), file.filename.c_str());
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

  rdcstr pidvalidfile = StringFormat::Fmt("/proc/%d/stat", (int)childPid);
  rdcstr procfile = StringFormat::Fmt("/proc/%d/net/tcp", (int)childPid);

  int waitTime = INITIAL_WAIT_TIME;

  // try for a little while for the /proc entry to appear
  while(ret == 0 && waitTime <= MAX_WAIT_TIME)
  {
    if(!FileIO::exists(pidvalidfile))
    {
      RDCWARN("Process %u is not running - did it exit during initialisation or fail to run?",
              childPid);
      return 0;
    }

    // back-off for each retry
    usleep(waitTime);

    waitTime *= 2;

    FILE *f = FileIO::fopen(procfile, FileIO::ReadText);

    if(f == NULL)
    {
      // try again in a bit
      continue;
    }

    rdcarray<int> sockets = getSockets(childPid);

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
         hexport <= RenderDoc_LastTargetControlPort && sockets.contains(inode))
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

    if(!FileIO::exists(procfile))
    {
      RDCWARN("Process %u is no longer running - did it exit during initialisation or fail to run?",
              childPid);
    }
  }

  return ret;
}

bool StopChildAtMain(pid_t childPid, bool *exitWithNoExec)
{
  int stat;
  pid_t pid;
  pid = waitpid(childPid, &stat, WUNTRACED);
  return pid == childPid && WIFSTOPPED(stat);
}

void ResumeProcess(pid_t childPid, uint32_t delay = 0)
{
  kill(childPid, SIGCONT);
}

void StopAtMainInChild(void)
{
  raise(SIGSTOP);
}

// because OSUtility::DebuggerPresent is called often we want it to be
// cheap. Opening and parsing a file would cause high overhead on each
// call, so instead we just cache it at startup. This fails in the case
// of attaching to processes
bool debuggerPresent = false;
bool debuggerCached = false;

void CacheDebuggerPresent()
{
  FILE *f = FileIO::fopen("/proc/self/status", FileIO::ReadText);

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
      // if no tracer is connected then that's fine, we cache no debugger being present. One could
      // attach later but that's the problem with caching, worst case we lose break-on-error.
      if(tracerpid == 0)
      {
        debuggerPresent = false;
        debuggerCached = true;
      }
      else
      {
        // this is REALLY ugly. There's no better way to communicate when we have a real debugger
        // attached and when it's a parent renderdoc process injecting hooks. So we look up the
        // parent PID and see if it has any executable pages mapped from our library (a real
        // debugger could have read-only pages, sadly).
        rdcstr tracermaps;
        if(FileIO::ReadAll(StringFormat::Fmt("/proc/%d/maps", tracerpid).c_str(), tracermaps))
        {
          // could be slightly more efficient than this, but we don't expect to do this more than a
          // couple of times on process startup and the file should only be a few kb so clarity wins
          // out.

          rdcarray<rdcstr> lines;
          split(tracermaps, lines, '\n');

          // remove any lines that don't reference librenderdoc.so
          lines.removeIf(
              [](const rdcstr &l) { return !l.contains("/lib" STRINGIZE(RDOC_BASE_NAME) ".so"); });
          merge(lines, tracermaps, '\n');

          if(tracermaps.contains("r-x"))
          {
            // if the tracer has librenderdoc.so loaded for execute assume that we're detecting
            // RenderDoc's ptrace usage. Don't treat it as a debugger but don't cache this result,
            // we'll check again soon and hopefully get a better result
            debuggerPresent = false;
            debuggerCached = false;
          }
          else
          {
            // tracer is present and doesn't have librenderdoc.so loaded (or only has it loaded
            // read-only), it must be a real debugger.
            debuggerPresent = true;
            debuggerCached = true;
          }
        }
        else
        {
          // can't read the tracer maps entry? Maybe a privilege issue, assume this isn't RenderDoc
          // and cache it as a debugger
          RDCWARN("Couldn't read /proc/%d/maps entry for tracer, assuming valid debugger", tracerpid);
          debuggerPresent = true;
          debuggerCached = true;
        }
      }

      break;
    }
  }

  FileIO::fclose(f);
}

bool OSUtility::DebuggerPresent()
{
  // recache the debugger's presence if it's not cached, e.g. if the previous debugger looked like
  // it was ourselves doing a ptrace over launch
  if(!debuggerCached)
    CacheDebuggerPresent();
  return debuggerPresent;
}

using PFN_getenv = decltype(&getenv);

rdcstr Process::GetEnvVariable(const rdcstr &name)
{
  const char *val = NULL;
  // try to bypass any hooks to ensure we don't break (looking at you bash)

  static PFN_getenv dyn_getenv = NULL;
  static bool checked = false;
  if(!checked)
  {
    checked = true;
    void *libc = dlopen("libc.so.6", RTLD_NOLOAD | RTLD_GLOBAL | RTLD_NOW);
    if(libc)
    {
      dyn_getenv = (PFN_getenv)dlsym(libc, "getenv");
    }
  }

  if(dyn_getenv)
    val = dyn_getenv(name.c_str());
  else
    val = getenv(name.c_str());

  return val ? val : rdcstr();
}

uint64_t Process::GetMemoryUsage()
{
  FILE *f = FileIO::fopen("/proc/self/statm", FileIO::ReadText);

  if(f == NULL)
  {
    RDCWARN("Couldn't open /proc/self/statm");
    return 0;
  }

  char line[512] = {};
  fgets(line, 511, f);

  FileIO::fclose(f);

  uint32_t rssPages = 0;
  int num = sscanf(line, "%*u %u", &rssPages);

  if(num == 1 && rssPages > 0)
    return rssPages * (uint64_t)sysconf(_SC_PAGESIZE);

  return 0;
}
