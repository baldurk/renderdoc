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

#include <dlfcn.h>    // for dlsym
#include <stdlib.h>
// clang-format off
// sys/types.h needs to be included before sys/sysctl.h
#include <sys/types.h>
#include <sys/sysctl.h>
// clang-format on
#include <sys/user.h>
#include <unistd.h>
#include "common/common.h"
#include "common/formatting.h"
#include "core/core.h"
#include "os/os_specific.h"

// extern char **environ;

char **GetCurrentEnvironment()
{
  // environ is broken: https://reviews.freebsd.org/D30842
  return *(char ***)dlsym(RTLD_DEFAULT, "environ");
  // return environ;
}

rdcstr execcmd(const char *cmd)
{
  FILE *pipe = popen(cmd, "r");

  if(!pipe)
    return "ERROR";

  char buffer[128];

  rdcstr result = "";

  while(!feof(pipe))
  {
    if(fgets(buffer, 128, pipe) != NULL)
      result += buffer;
  }

  pclose(pipe);

  return result;
}

bool isNewline(char c)
{
  return c == '\n' || c == '\r';
}

// FIXME: lsof is in ports but not in base system
int GetIdentPort(pid_t childPid)
{
  rdcstr lsof = StringFormat::Fmt("lsof -p %d -a -i 4 -F n", (int)childPid);
  rdcstr result;
  uint32_t wait = 1;
  // Wait for a maximum of ~16 seconds
  for(int i = 0; i < 14; ++i)
  {
    result = execcmd(lsof.c_str());
    if(!result.empty())
      break;
    usleep(wait * 1000);
    wait *= 2;
  }
  if(result.empty())
  {
    RDCERR("No output from lsof command: '%s'", lsof.c_str());
    return 0;
  }

  // Parse the result expecting:
  // p<PID>
  // <TEXT>
  // n*:<PORT>

  rdcstr parseResult(result);
  const int len = parseResult.count();
  if(parseResult[0] == 'p')
  {
    int tokenStart = 1;
    int i = tokenStart;
    for(; i < len; i++)
    {
      if(parseResult[i] < '0' || parseResult[i] > '9')
        break;
    }
    parseResult[i++] = 0;

    if(isNewline(parseResult[i]))
      i++;

    const int pid = atoi(&result[tokenStart]);
    if(pid == (int)childPid)
    {
      const char *netString("n*:");
      while(i < len)
      {
        const int netStart = parseResult.find(netString, i);
        if(netStart >= 0)
        {
          tokenStart = netStart + (int)strlen(netString);
          i = tokenStart;
          for(; i < len; i++)
          {
            if(parseResult[i] < '0' || parseResult[i] > '9')
              break;
          }
          parseResult[i++] = 0;

          if(isNewline(parseResult[i]))
            i++;

          const int port = atoi(&result[tokenStart]);
          if(port >= RenderDoc_FirstTargetControlPort && port <= RenderDoc_LastTargetControlPort)
          {
            return port;
          }
          // continue on to next port
        }
        else
        {
          RDCERR("Malformed line - expected 'n*':\n%s", &result[i]);
          return 0;
        }
      }
    }
    else
    {
      RDCERR("pid from lsof output doesn't match childPid");
      return 0;
    }
  }
  RDCERR("Failed to parse output from lsof:\n%s", result.c_str());
  return 0;
}

void StopAtMainInChild()
{
}

bool StopChildAtMain(pid_t childPid, bool *exitWithNoExec)
{
  return false;
}

void ResumeProcess(pid_t childPid, uint32_t delay)
{
}

// OSUtility::DebuggerPresent is called a lot
// cache the value at startup as an optimisation
static bool s_debuggerPresent = false;
static bool s_debuggerCached = false;

void CacheDebuggerPresent()
{
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  struct kinfo_proc info = {};
  size_t size = sizeof(info);
  if(!sysctl(mib, ARRAY_COUNT(mib), &info, &size, NULL, 0))
  {
    s_debuggerPresent = (info.ki_flag & P_TRACED);
    s_debuggerCached = true;
  }
}

bool OSUtility::DebuggerPresent()
{
  if(!s_debuggerCached)
    CacheDebuggerPresent();
  return s_debuggerPresent;
}

rdcstr Process::GetEnvVariable(const rdcstr &name)
{
  const char *val = getenv(name.c_str());
  return val ? val : rdcstr();
}

uint64_t Process::GetMemoryUsage()
{
  int mib[4] = {CTL_VM, KERN_PROC, KERN_PROC_PID, getpid()};
  struct kinfo_proc info = {};
  size_t size = sizeof(info);
  if(sysctl(mib, ARRAY_COUNT(mib), &info, &size, NULL, 0) != 0)
    return 0;
  // from usr.bin/top/machine.c macro PROCSIZE
  return info.ki_size / 1024;
}
