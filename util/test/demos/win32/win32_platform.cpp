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

#include "../test_common.h"

#include <Psapi.h>

uint64_t GetMemoryUsage()
{
  HANDLE proc = GetCurrentProcess();

  if(proc == NULL)
  {
    TEST_ERROR("Couldn't open process: %d", GetLastError());
    return 0;
  }

  PROCESS_MEMORY_COUNTERS memInfo = {};

  uint64_t ret = 0;

  if(GetProcessMemoryInfo(proc, &memInfo, sizeof(memInfo)))
  {
    ret = memInfo.WorkingSetSize;
  }
  else
  {
    TEST_ERROR("Couldn't get process memory info: %d", GetLastError());
  }

  return ret;
}

std::string GetCWD()
{
  char cwd[MAX_PATH + 1] = {0};
  GetCurrentDirectoryA(MAX_PATH, cwd);

  std::string cwdstr = cwd;

  for(size_t i = 0; i < cwdstr.size(); i++)
    if(cwdstr[i] == '\\')
      cwdstr[i] = '/';

  while(cwdstr.back() == '/' || cwdstr.back() == '\\')
    cwdstr.pop_back();

  return cwdstr;
}

std::string Wide2UTF8(const std::wstring &s)
{
  int bytes_required = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, NULL, 0, NULL, NULL);

  if(bytes_required == 0)
    return "";

  std::string ret;
  ret.resize(bytes_required);

  int res = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &ret[0], bytes_required, NULL, NULL);

  if(ret.back() == 0)
    ret.pop_back();

  if(res == 0)
  {
    TEST_WARN("Failed to convert wstring '%ls'", s.c_str());
    return "";
  }

  return ret;
}

std::wstring UTF82Wide(const std::string &s)
{
  int chars_required = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);

  if(chars_required == 0)
    return L"";

  std::wstring ret;
  ret.resize(chars_required);

  int res = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ret[0], chars_required);

  if(ret.back() == 0)
    ret.pop_back();

  if(res == 0)
  {
    TEST_WARN("Failed to convert utf-8 string '%s'", s.c_str());
    return L"";
  }

  return ret;
}

std::string GetEnvVar(const char *var)
{
  std::wstring wvar = UTF82Wide(var);
  wchar_t wval[1024] = {};
  DWORD len = GetEnvironmentVariableW(wvar.c_str(), wval, 1023);

  if(len > 0 && len <= 1023)
    return Wide2UTF8(wval);

  return "";
}

std::string GetExecutableName()
{
  wchar_t curFile[512] = {0};
  GetModuleFileNameW(NULL, curFile, 511);

  return Wide2UTF8(curFile);
}
