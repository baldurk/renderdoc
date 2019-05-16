/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

#include <crt_externs.h>
#include <mach/mach.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#include "os/os_specific.h"

char **GetCurrentEnvironment()
{
  return *_NSGetEnviron();
}

std::string execcmd(const char *cmd)
{
  FILE *pipe = popen(cmd, "r");

  if(!pipe)
    return "ERROR";

  char buffer[128];

  std::string result = "";

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

int GetIdentPort(pid_t childPid)
{
  std::string lsof = StringFormat::Fmt("lsof -p %d -a -i 4 -F n", (int)childPid);
  std::string result;
  uint32_t wait = 1;
  for(int i = 0; i < 10; ++i)
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

  std::string parseResult(result);
  const size_t len = parseResult.length();
  if(parseResult[0] == 'p')
  {
    size_t tokenStart = 1;
    size_t i = tokenStart;
    for(; i < len; i++)
    {
      if(parseResult[i] < '0' || parseResult[i] > '9')
        break;
    }
    parseResult[i++] = 0;

    if(isNewline(parseResult[i]))
      i++;

    const int pid = std::stoi(&result[tokenStart]);
    if(pid == (int)childPid)
    {
      const char *netString("n*:");
      while(i < len)
      {
        const size_t netStart = parseResult.find(netString, i);
        if(netStart != std::string::npos)
        {
          tokenStart = netStart + strlen(netString);
          i = tokenStart;
          for(; i < len; i++)
          {
            if(parseResult[i] < '0' || parseResult[i] > '9')
              break;
          }
          parseResult[i++] = 0;

          if(isNewline(parseResult[i]))
            i++;

          const int port = std::stoi(&result[tokenStart]);
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

void CacheDebuggerPresent()
{
}

// from https://developer.apple.com/library/mac/qa/qa1361/_index.html on how to detect the debugger
bool OSUtility::DebuggerPresent()
{
// apple requires that this only be called in debug builds
#if ENABLED(RDOC_RELEASE)
  return false;
#else
  int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};
  kinfo_proc info = {};
  size_t size = sizeof(info);
  sysctl(mib, ARRAY_COUNT(mib), &info, &size, NULL, 0);

  return info.kp_proc.p_flag & P_TRACED;
#endif
}

const char *Process::GetEnvVariable(const char *name)
{
  return getenv(name);
}

uint64_t Process::GetMemoryUsage()
{
  mach_task_basic_info taskInfo;
  mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;

  int ret = task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&taskInfo, &infoCount);

  if(ret != KERN_SUCCESS)
    return 0;

  return taskInfo.resident_size;
}