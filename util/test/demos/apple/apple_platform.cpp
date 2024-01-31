/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2024 Baldur Karlsson
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

#include <mach-o/dyld.h>
#include <mach/mach.h>
#include "test_common.h"

uint64_t GetMemoryUsage()
{
  mach_task_basic_info taskInfo;
  mach_msg_type_number_t infoCount = MACH_TASK_BASIC_INFO_COUNT;

  int ret = task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&taskInfo, &infoCount);

  if(ret != KERN_SUCCESS)
    return 0;

  return taskInfo.resident_size;
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
  std::string selfName;

  uint32_t pathSize = (uint32_t)sizeof(path);
  if(_NSGetExecutablePath(path, &pathSize) == 0)
  {
    selfName = path;
  }
  else
  {
    pathSize++;
    if(_NSGetExecutablePath(path, &pathSize) == 0)
    {
      selfName = path;
    }
    else
    {
      return "/unknown/unknown";
    }
  }

  memset(path, 0, sizeof(path));
  readlink(selfName.c_str(), path, 511);
  if(path[0] != 0)
    selfName = path;

  return selfName;
}
