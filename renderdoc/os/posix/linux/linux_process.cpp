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

#if defined(__arm__)

// for some reason arm doesn't have the same struct name. Sigh :(
#define user_regs_struct user_regs

#define INST_PTR_REG ARM_pc

#define BREAK_INST 0xe7f001f0ULL
#define BREAK_INST_BYTES_SIZE 4
// on ARM seemingly the instruction isn't actually considered executed, so we don't have to modify
// the instruction pointer at all.
#define BREAK_INST_INST_PTR_ADJUST 0

#elif defined(__aarch64__)

#define INST_PTR_REG pc

#define BREAK_INST 0xd4200000ULL
#define BREAK_INST_BYTES_SIZE 4
// on ARM seemingly the instruction isn't actually considered executed, so we don't have to modify
// the instruction pointer at all.
#define BREAK_INST_INST_PTR_ADJUST 0

#elif defined(__riscv)

#define INST_PTR_REG pc

// ebreak
#define BREAK_INST 0x00100073ULL
#define BREAK_INST_BYTES_SIZE 4
#define BREAK_INST_INST_PTR_ADJUST 4

#else

#define BREAK_INST 0xccULL
#define BREAK_INST_BYTES_SIZE 1
// step back over the instruction
#define BREAK_INST_INST_PTR_ADJUST 1

#if ENABLED(RDOC_X64)
#define INST_PTR_REG rip
#else
#define INST_PTR_REG eip
#endif

#endif

static uint64_t get_child_ip(pid_t childPid)
{
  user_regs_struct regs = {};

  iovec regs_iovec = {&regs, sizeof(regs)};
  long ptraceRet = ptrace(PTRACE_GETREGSET, childPid, (void *)NT_PRSTATUS, &regs_iovec);
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

bool StopChildAtMain(pid_t childPid, bool *exitWithNoExec)
{
  // don't do this unless the ptrace scope is OK.
  if(!ptrace_scope_ok())
    return false;

  if(Linux_Debug_PtraceLogging())
    RDCLOG("Stopping child PID %u at main", childPid);

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

  if(Linux_Debug_PtraceLogging())
    RDCLOG("Child PID %u is stopped in StopAtMainInChild()", childPid);

  int64_t ptraceRet = 0;

  // continue until exec
  ptraceRet = ptrace(PTRACE_SETOPTIONS, childPid, NULL, PTRACE_O_TRACEEXEC | PTRACE_O_TRACEEXIT);
  RDCASSERTEQUAL(ptraceRet, 0);

  if(Linux_Debug_PtraceLogging())
    RDCLOG("Child PID %u configured to trace exec(). Continuing child", childPid);

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

  int statusResult = childStatus >> 8;

  if(childStatus > 0 &&
     (statusResult == SIGCHLD || statusResult == (SIGTRAP | (PTRACE_EVENT_EXIT << 8))))
  {
    if(Linux_Debug_PtraceLogging())
      RDCLOG("Child PID %u exited while waiting for exec() 0x%x", childPid, childStatus);
    if(exitWithNoExec)
      *exitWithNoExec = true;

    if(statusResult == SIGCHLD)
      ptrace(PTRACE_DETACH, childPid, NULL, SIGCHLD);
    else
      ptrace(PTRACE_DETACH, childPid, NULL, NULL);
    return false;
  }

  if(childStatus > 0 && statusResult != (SIGTRAP | (PTRACE_EVENT_EXEC << 8)))
  {
    RDCERR("Exec wait event from child PID %u was status 0x%x, expected 0x%x", childPid,
           statusResult, (SIGTRAP | (PTRACE_EVENT_EXEC << 8)));

    return false;
  }

  if(Linux_Debug_PtraceLogging())
    RDCLOG("Child PID %u is stopped at execve() 0x%x", childPid, childStatus);

  rdcstr exepath;
  long baseVirtualPointer = 0;
  uint32_t sectionOffset = 0;

  rdcstr mapsName = StringFormat::Fmt("/proc/%u/maps", childPid);

  FILE *maps = FileIO::fopen(mapsName, FileIO::ReadText);

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
        int num = sscanf(line, "%lx-%*x r-xp %x %*x:%*x %*u %n", &baseVirtualPointer,
                         &sectionOffset, &pathOffset);

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

  if(baseVirtualPointer == 0)
  {
    RDCERR("Couldn't find executable mapping in maps file");
    return false;
  }

  if(Linux_Debug_PtraceLogging())
    RDCLOG("Child PID %u has exepath %s basePointer 0x%llx and sectionOffset 0x%x", childPid,
           exepath.c_str(), (uint64_t)baseVirtualPointer, (uint32_t)sectionOffset);

  FileIO::fclose(maps);

  FILE *elf = FileIO::fopen(exepath, FileIO::ReadText);

  if(!elf)
  {
    RDCERR("Couldn't open %s to parse ELF header", exepath.c_str());
    return false;
  }

  Elf64_Ehdr elf_header;
  size_t read = FileIO::fread(&elf_header, sizeof(elf_header), 1, elf);

  if(read != 1)
  {
    FileIO::fclose(elf);
    RDCERR("Couldn't read ELF header from %s", exepath.c_str());
    return false;
  }

  size_t entryVirtual = (size_t)elf_header.e_entry;
  // if the section doesn't shift between file offset and virtual address this will be the same
  size_t entryFileOffset = entryVirtual;

  if(elf_header.e_shoff)
  {
    if(Linux_Debug_PtraceLogging())
      RDCLOG("exepath %s contains sections, rebasing to correct section", exepath.c_str());

    FileIO::fseek64(elf, elf_header.e_shoff, SEEK_SET);

    RDCASSERTEQUAL(elf_header.e_shentsize, sizeof(Elf64_Shdr));

    for(Elf64_Half s = 0; s < elf_header.e_shnum; s++)
    {
      Elf64_Shdr section_header;
      read = FileIO::fread(&section_header, sizeof(section_header), 1, elf);

      if(read != 1)
      {
        FileIO::fclose(elf);
        RDCERR("Couldn't read section header from %s", exepath.c_str());
        return false;
      }

      if(section_header.sh_addr <= entryVirtual &&
         entryVirtual < section_header.sh_addr + section_header.sh_size)
      {
        if(Linux_Debug_PtraceLogging())
          RDCLOG(
              "Found section in %s from 0x%llx - 0x%llx at offset 0x%llx containing entry 0x%llx.",
              exepath.c_str(), (uint64_t)section_header.sh_addr,
              uint64_t(section_header.sh_addr + section_header.sh_size),
              (uint64_t)section_header.sh_offset, (uint64_t)entryVirtual);

        entryFileOffset =
            (entryVirtual - (size_t)section_header.sh_addr) + (size_t)section_header.sh_offset;

        break;
      }
    }
  }

  FileIO::fclose(elf);

  void *entry = (void *)(baseVirtualPointer + entryFileOffset - sectionOffset);

  if(Linux_Debug_PtraceLogging())
    RDCLOG("child process %u executable %s has entry %p at 0x%llx + (0x%llx - 0x%x)", childPid,
           exepath.c_str(), entry, (uint64_t)baseVirtualPointer, (uint64_t)entryFileOffset,
           (uint32_t)sectionOffset);

  // this reads a 'word' and returns as long, upcast (if needed) to uint64_t
  uint64_t origEntryWord = (uint64_t)ptrace(PTRACE_PEEKTEXT, childPid, entry, 0);

  if(Linux_Debug_PtraceLogging())
    RDCLOG("Read word %llx from %p in child process %u running executable %s",
           (uint64_t)origEntryWord, entry, childPid, exepath.c_str());

  uint64_t breakpointWord =
      (origEntryWord & (0xffffffffffffffffULL << (BREAK_INST_BYTES_SIZE * 8))) | BREAK_INST;
  // downcast back to long, if that means truncating
  ptraceRet = ptrace(PTRACE_POKETEXT, childPid, entry, (long)breakpointWord);
  RDCASSERTEQUAL(ptraceRet, 0);

  if(Linux_Debug_PtraceLogging())
    RDCLOG("Changed word to %llx and re-poked in process %u. Continuing child",
           (uint64_t)breakpointWord, childPid);

  // continue
  ptraceRet = ptrace(PTRACE_CONT, childPid, NULL, NULL);
  RDCASSERTEQUAL(ptraceRet, 0);

  // it could take a long time to hit main so we have a large timeout here
  if(!wait_traced_child(childPid, 2000, childStatus))
  {
    RDCERR("Didn't hit breakpoint in PID %u (%x)", childPid, childStatus);
    return false;
  }

  if(Linux_Debug_PtraceLogging())
    RDCLOG("Process %u hit entry point", childPid);

  // we're now at main! now just need to clean up after ourselves

  user_regs_struct regs = {};

  iovec regs_iovec = {&regs, sizeof(regs)};
  ptraceRet = ptrace(PTRACE_GETREGSET, childPid, (void *)NT_PRSTATUS, &regs_iovec);
  RDCASSERTEQUAL(ptraceRet, 0);

  if(Linux_Debug_PtraceLogging())
    RDCLOG("Process %u instruction pointer is at %llx, for entry point %p", childPid,
           (uint64_t)(regs.INST_PTR_REG), entry);

  // step back past the byte(s) we inserted the breakpoint on
  regs.INST_PTR_REG -= BREAK_INST_INST_PTR_ADJUST;
  ptraceRet = ptrace(PTRACE_SETREGSET, childPid, (void *)NT_PRSTATUS, &regs_iovec);
  RDCASSERTEQUAL(ptraceRet, 0);

  // restore the function
  ptraceRet = ptrace(PTRACE_POKETEXT, childPid, entry, origEntryWord);
  RDCASSERTEQUAL(ptraceRet, 0);

  if(Linux_Debug_PtraceLogging())
    RDCLOG("Process %u instruction pointer adjusted and breakpoint removed.", childPid);

  // we'll resume after reading the ident port in the calling function
  return true;
}

void StopAtMainInChild()
{
  // don't do this unless the ptrace scope is OK.
  if(!ptrace_scope_ok())
    return;

  if(Linux_Debug_PtraceLogging())
    RDCLOG("Stopping in main at child for ptracing");

  // allow parent tracing, and immediately stop so the parent process can attach
  ptrace(PTRACE_TRACEME, 0, 0, 0);

  if(Linux_Debug_PtraceLogging())
    RDCLOG("Done PTRACE_TRACEME, raising SIGSTOP");

  raise(SIGSTOP);

  if(Linux_Debug_PtraceLogging())
    RDCLOG("Resumed after SIGSTOP");
}

void ResumeProcess(pid_t childPid, uint32_t delaySeconds)
{
  if(!ptrace_scope_ok())
    return;

  if(childPid != 0)
  {
    // if we have a delay, see if the process is paused. If so then detach it but keep it stopped
    // and wait to see if someone attaches
    if(delaySeconds > 0)
    {
      uint64_t ip = get_child_ip(childPid);

      if(ip != 0)
      {
        if(Linux_Debug_PtraceLogging())
          RDCLOG("Detaching %u with SIGSTOP to allow a debugger to attach, waiting %u seconds",
                 childPid, delaySeconds);

        // detach but stop, to allow a debugger to attach
        ptrace(PTRACE_DETACH, childPid, NULL, SIGSTOP);

        rdcstr filename = StringFormat::Fmt("/proc/%u/status", childPid);

        uint64_t start_nano = get_nanotime();
        uint64_t end_nano = 0;

        const uint64_t timeoutNanoseconds = uint64_t(delaySeconds) * 1000 * 1000 * 1000;

        bool connected = false;

        // watch for a tracer to attach
        do
        {
          usleep(10);

          rdcstr status;
          FileIO::ReadAll(filename, status);

          int32_t offs = status.find("TracerPid:");

          if(offs < 0)
            break;

          status.erase(0, offs + sizeof("TracerPid:"));
          status.trim();

          end_nano = get_nanotime();

          if(status[0] != '0')
          {
            RDCLOG("Debugger PID %u attached after %f seconds", atoi(status.c_str()),
                   double(end_nano - start_nano) / 1000000000.0);
            connected = true;
            break;
          }
        } while(end_nano - start_nano < timeoutNanoseconds);

        if(!connected)
        {
          RDCLOG("Timed out waiting for debugger, resuming");
          kill(childPid, SIGCONT);
        }
        return;
      }
      else
      {
        RDCERR("Can't delay for debugger without ptrace, check ptrace_scope value");
      }
    }

    if(Linux_Debug_PtraceLogging())
      RDCLOG("Detaching immediately from %u", childPid);

    // try to detach and resume the process, ignoring any errors if we weren't tracing
    long ret = ptrace(PTRACE_DETACH, childPid, NULL, NULL);

    if(Linux_Debug_PtraceLogging())
      RDCLOG("Detached pid %u (%ld)", childPid, ret);
  }
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
