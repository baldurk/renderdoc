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

#include <unistd.h>
#include "os/os_specific.h"
#include "strings/string_utils.h"

extern char **environ;

char **GetCurrentEnvironment()
{
  return environ;
}

int GetIdentPort(pid_t childPid)
{
  int ret = 0;

  std::string procfile = StringFormat::Fmt("/proc/%d/net/unix", (int)childPid);

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

      int port = 0;
      char *startpos = strstr(line, "@renderdoc_");

      if(startpos == NULL)
      {
        // This is not the line we are looking for, continue the processing.
        continue;
      }

      int num = sscanf(startpos, "@renderdoc_%d", &port);

      // find open listen abstract socket on 'renderdoc_<port>'
      if(num == 1 && port >= RenderDoc_FirstTargetControlPort &&
         port <= RenderDoc_LastTargetControlPort)
      {
        ret = port;
        break;
      }
    }

    FileIO::fclose(f);
  }

  if(ret == 0)
  {
    RDCWARN(
        "Couldn't locate renderdoc target control listening port between @renderdoc_%u and "
        "@renderdoc_%u in %s",
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
  int ret = 0;

  if(f == NULL)
  {
    RDCWARN("Couldn't open /proc/self/status");
    return;
  }

  // read through the proc file to check for TracerPid
  while(ret == 0 && !feof(f))
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
  // we fake environment variables with properties
  Process::ProcessResult result;
  Process::LaunchProcess("getprop", ".",
                         StringFormat::Fmt("debug.rdoc.%s variable_is_not_set", name).c_str(), true,
                         &result);

  static std::string settingsOutput;

  settingsOutput = trim(result.strStdout);

  if(settingsOutput == "variable_is_not_set")
    return NULL;

  return settingsOutput.c_str();
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