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

#include <crt_externs.h>
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

  string result = "";

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
  string lsof = StringFormat::Fmt("lsof -p %d -a -i 4 -F n", (int)childPid);
  string result = execcmd(lsof.c_str());

  // Parse the result expecting:
  // p<PID>
  // n*:<PORT>

  const size_t len = result.length();
  bool found = false;
  if(result[0] == 'p')
  {
    size_t i = 1;
    for(; i < len; i++)
    {
      if(result[i] < '0' || result[i] > '9')
        break;
    }
    result[i++] = 0;

    if(isNewline(result[i]))
      i++;

    const int pid = std::stoi(&result[1]);
    if(pid == (int)childPid)
    {
      while(i < len)
      {
        if(result[i] == 'n')
        {
          char *portStr = NULL;
          for(; i < len; i++)
          {
            if(result[i] == ':')
            {
              portStr = &result[i + 1];
            }
            if(isNewline(result[i]))
            {
              result[i++] = 0;
              if(i < len && isNewline(result[i]))
                result[i++] = 0;
              break;
            }
          }

          const int port = std::stoi(portStr);
          if(port >= RenderDoc_FirstTargetControlPort && port <= RenderDoc_LastTargetControlPort)
          {
            return port;
          }

          // continue on to next port
        }
        else
        {
          RDCERR("Malformed line - expected 'n':\n%s", &result[i]);
          break;
        }
      }
    }
    else
    {
      RDCERR("pid from lsof output doesn't match childPid");
    }
  }
  else
  {
    RDCERR("lsof output doesn't begin with p<PID>:\n%s", &result[0]);
  }

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
