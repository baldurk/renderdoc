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

#include <stdlib.h>
#include <unistd.h>
#include "test_common.h"

uint64_t GetMemoryUsage()
{
  FILE *f = fopen("/proc/self/statm", "r");

  if(f == NULL)
  {
    TEST_WARN("Couldn't open /proc/self/statm");
    return 0;
  }

  char line[512] = {};
  fgets(line, 511, f);

  fclose(f);

  uint32_t rssPages = 0;
  int num = sscanf(line, "%*u %u", &rssPages);

  if(num == 1 && rssPages > 0)
    return rssPages * (uint64_t)sysconf(_SC_PAGESIZE);

  return 0;
}

std::string GetCWD()
{
  char cwd[MAX_PATH + 1] = {0};
  getcwd(cwd, MAX_PATH);

  std::string cwdstr = cwd;

  for(size_t i = 0; i < cwdstr.size(); i++)
    if(cwdstr[i] == '\\')
      cwdstr[i] = '/';

  while(cwdstr.back() == '/' || cwdstr.back() == '\\')
    cwdstr.pop_back();

  return cwdstr;
}

std::string GetEnvVar(const char *var)
{
  const char *data = getenv(var);
  if(data)
    return data;

  return "";
}

std::string GetExecutableName()
{
  char path[512] = {0};
  readlink("/proc/self/exe", path, 511);

  return path;
}
