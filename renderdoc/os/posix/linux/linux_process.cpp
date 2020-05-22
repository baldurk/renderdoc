/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include <elf.h>
#include <sys/ptrace.h>
#include <sys/types.h>
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

RDOC_CONFIG(bool, Linux_PtraceChildProcesses, true,
            "Use ptrace(2) to trace child processes at startup to ensure connection is made as "
            "early as possible.");

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
  FileIO::GetFilesInDirectory(dirPath.c_str(), files);
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

  rdcstr procfile = StringFormat::Fmt("/proc/%d/net/tcp", (int)childPid);

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

    if(!FileIO::exists(procfile.c_str()))
    {
      RDCWARN("Process %u is no longer running - did it exit during initialisation or fail to run?",
              childPid);
    }
  }

  return ret;
}

static bool ptrace_scope_ok()
{
  if(!Linux_PtraceChildProcesses())
    return false;

  rdcstr contents;
  FileIO::ReadAll("/proc/sys/kernel/yama/ptrace_scope", contents);
  contents.trim();
  if(!contents.empty())
  {
    int ptrace_scope = atoi(contents.c_str());
    if(ptrace_scope > 1)
    {
      if(RenderDoc::Inst().IsReplayApp())
      {
        static bool warned = false;
        if(!warned)
        {
          warned = true;
          RDCWARN(
              "ptrace_scope value %d means ptrace can't be used to pause child processes while "
              "attaching.",
              ptrace_scope);
        }
      }
      return false;
    }
  }

  return true;
}

static uint64_t get_nanotime()
{
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  uint64_t ret = uint64_t(ts.tv_sec) * 1000000000ULL + uint32_t(ts.tv_nsec & 0xffffffff);
  return ret;
}

#if ENABLED(RDOC_X64)
#define INST_PTR_REG rip
#else
#define INST_PTR_REG eip
#endif

static uint64_t get_child_ip(pid_t childPid)
{
  user_regs_struct regs = {};

  long ptraceRet = ptrace(PTRACE_GETREGS, childPid, NULL, &regs);
  if(ptraceRet == 0)
    return uint64_t(regs.INST_PTR_REG);

  return 0;
}

static bool wait_traced_child(pid_t childPid, uint32_t timeoutMS, int &status)
{
  // spin waiting for the traced child, with a 100ms timeout
  status = 0;
  uint64_t start_nano = get_nanotime();
  uint64_t end_nano = 0;
  int ret = 0;

  const uint64_t timeoutNanoseconds = uint64_t(timeoutMS) * 1000 * 1000;

  while((ret = waitpid(childPid, &status, WNOHANG)) == 0)
  {
    status = 0;

    // if we're in a capturing process then the process itself might have done waitpid(-1) and
    // swallowed the wait for our child. So as an alternative we check to see if we can query the
    // instruction pointer, which is only possible if the child is stopped.
    uint64_t ip = get_child_ip(childPid);
    if(ip != 0)
    {
      // do waitpid again in case we raced and the child stopped in between the call to waitpid and
      // get_child_ip.
      ret = waitpid(childPid, &status, WNOHANG);

      // if it still didn't succeed, set status to 0 so we know we're earlying out and don't check
      // the status codes.
      if(ret == 0)
        status = 0;
      return true;
    }

    usleep(10);

    // check the timeout
    end_nano = get_nanotime();
    if(end_nano - start_nano > timeoutNanoseconds)
      break;
  }

  return WIFSTOPPED(status);
}

bool StopChildAtMain(pid_t childPid)
{
  // don't do this unless the ptrace scope is OK.
  if(!ptrace_scope_ok())
    return false;

  int childStatus = 0;

  // we have a low timeout for this stop since it should happen almost immediately (right after the
  // fork). If it didn't then we want to fail relatively fast.
  if(!wait_traced_child(childPid, 100, childStatus))
  {
    RDCERR("Didn't get initial stop from child PID %u", childPid);
    return false;
  }

  if(childStatus > 0 && WSTOPSIG(childStatus) != SIGSTOP)
  {
    RDCERR("Initial signal from child PID %u was %x, expected %x", childPid, WSTOPSIG(childStatus),
           SIGSTOP);
    return false;
  }

  long ptraceRet = 0;

  // continue until exec
  ptraceRet = ptrace(PTRACE_SETOPTIONS, childPid, NULL, PTRACE_O_TRACEEXEC);
  RDCASSERTEQUAL(ptraceRet, 0);

  // continue
  ptraceRet = ptrace(PTRACE_CONT, childPid, NULL, NULL);
  RDCASSERTEQUAL(ptraceRet, 0);

  // we're not under control of when the application calls exec() after fork() in the case of child
  // processes, so be a little more generous with the timeout
  if(!wait_traced_child(childPid, 250, childStatus))
  {
    RDCERR("Didn't get to execve in child PID %u", childPid);
    return false;
  }

  if(childStatus > 0 && (childStatus >> 8) != (SIGTRAP | (PTRACE_EVENT_EXEC << 8)))
  {
    RDCERR("Exec wait event from child PID %u was status %x, expected %x", childPid,
           (childStatus >> 8), (SIGTRAP | (PTRACE_EVENT_EXEC << 8)));
    return false;
  }

  rdcstr exepath;
  long basePointer = 0;
  uint32_t sectionOffset = 0;

  rdcstr mapsName = StringFormat::Fmt("/proc/%u/maps", childPid);

  FILE *maps = FileIO::fopen(mapsName.c_str(), "r");

  if(!maps)
  {
    RDCERR("Couldn't open %s", mapsName.c_str());
    return false;
  }

  while(!feof(maps))
  {
    char line[512] = {0};
    if(fgets(line, 511, maps))
    {
      if(strstr(line, "r-xp"))
      {
        RDCCOMPILE_ASSERT(sizeof(long) == sizeof(void *), "Expected long to be pointer sized");
        int pathOffset = 0;
        int num = sscanf(line, "%lx-%*x r-xp %x %*x:%*x %*u %n", &basePointer, &sectionOffset,
                         &pathOffset);

        if(num != 2 || pathOffset == 0)
        {
          RDCERR("Couldn't parse first executable mapping '%s'", rdcstr(line).trimmed().c_str());
          return false;
        }

        exepath = line + pathOffset;
        exepath.trim();
        break;
      }
    }
  }

  if(basePointer == 0)
  {
    RDCERR("Couldn't find executable mapping in maps file");
    return false;
  }

  FileIO::fclose(maps);

  FILE *elf = FileIO::fopen(exepath.c_str(), "r");

  if(!elf)
  {
    RDCERR("Couldn't open %s to parse ELF header", exepath.c_str());
    return false;
  }

  Elf64_Ehdr elf_header;
  size_t read = FileIO::fread(&elf_header, sizeof(elf_header), 1, elf);
  FileIO::fclose(elf);

  if(read != 1)
  {
    RDCERR("Couldn't read ELF header from %s", exepath.c_str());
    return false;
  }

  void *entry = (void *)(basePointer + elf_header.e_entry - sectionOffset);

  long origEntryWord = ptrace(PTRACE_PEEKTEXT, childPid, entry, 0);

  long breakpointWord = (origEntryWord & 0xffffff00) | 0xcc;
  ptraceRet = ptrace(PTRACE_POKETEXT, childPid, entry, breakpointWord);
  RDCASSERTEQUAL(ptraceRet, 0);

  // continue
  ptraceRet = ptrace(PTRACE_CONT, childPid, NULL, NULL);
  RDCASSERTEQUAL(ptraceRet, 0);

  // it could take a long time to hit main so we have a large timeout here
  if(!wait_traced_child(childPid, 2000, childStatus))
  {
    RDCERR("Didn't hit breakpoint in PID %u (%x)", childPid, childStatus);
    return false;
  }

  // we're now at main! now just need to clean up after ourselves

  user_regs_struct regs = {};

  ptraceRet = ptrace(PTRACE_GETREGS, childPid, NULL, &regs);
  RDCASSERTEQUAL(ptraceRet, 0);

  // step back past the byte we inserted the breakpoint on
  regs.INST_PTR_REG--;
  ptraceRet = ptrace(PTRACE_SETREGS, childPid, NULL, &regs);
  RDCASSERTEQUAL(ptraceRet, 0);

  // restore the function
  ptraceRet = ptrace(PTRACE_POKETEXT, childPid, entry, origEntryWord);
  RDCASSERTEQUAL(ptraceRet, 0);

  // we'll resume after reading the ident port in the calling function
  return true;
}

void StopAtMainInChild()
{
  // don't do this unless the ptrace scope is OK.
  if(!ptrace_scope_ok())
    return;

  // allow parent tracing, and immediately stop so the parent process can attach
  ptrace(PTRACE_TRACEME, 0, 0, 0);
  raise(SIGSTOP);
}

void ResumeProcess(pid_t childPid)
{
  if(childPid != 0)
  {
    // try to detach and resume the process, ignoring any errors if we weren't tracing
    ptrace(PTRACE_DETACH, childPid, NULL, NULL);
  }
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
