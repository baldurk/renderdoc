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

#include <android/log.h>
#include <fcntl.h>
#include <unistd.h>
#include "common/common.h"
#include "common/formatting.h"
#include "os/os_specific.h"

#define LOGCAT_TAG "renderdoc"

namespace Keyboard
{
void Init()
{
}

bool PlatformHasKeyInput()
{
  return false;
}

void AddInputWindow(WindowingSystem windowSystem, void *wnd)
{
}

void RemoveInputWindow(WindowingSystem windowSystem, void *wnd)
{
}

bool GetKeyState(int key)
{
  return false;
}
}

namespace FileIO
{
rdcstr GetTempRootPath()
{
  // Save captures in the app's private /sdcard directory, which doesnt require
  // WRITE_EXTERNAL_STORAGE permissions. There is no security enforced here,
  // so the replay server can load it as it has READ_EXTERNAL_STORAGE.
  // This is the same as returned by getExternalFilesDir(). It might possibly change in the future.
  rdcstr package;
  GetExecutableFilename(package);
  return "/sdcard/Android/data/" + package + "/files";
}

rdcstr GetAppFolderFilename(const rdcstr &filename)
{
  return GetTempRootPath() + rdcstr("/") + filename;
}

// For RenderDoc's apk, this returns our package name
// For other APKs, we use it to get the writable temp directory.
void GetExecutableFilename(rdcstr &selfName)
{
  int fd = open(StringFormat::Fmt("/proc/%u/cmdline", getpid()).c_str(), O_RDONLY);
  if(fd < 0)
    return;

  char buf[4096];
  ssize_t len = read(fd, buf, sizeof(buf));
  close(fd);
  if(len < 0 || len == sizeof(buf))
  {
    return;
  }

  // Strip any process name from cmdline (android:process)
  rdcstr cmdline = buf;
  rdcstr filename = cmdline.substr(0, cmdline.find(":"));
  selfName = filename;
}

void GetLibraryFilename(rdcstr &selfName)
{
  RDCERR("GetLibraryFilename is not defined on Android");
  GetExecutableFilename(selfName);
}
};

namespace StringFormat
{
rdcstr Wide2UTF8(const rdcwstr &s)
{
  RDCFATAL("Converting wide strings to UTF-8 is not supported on Android!");
  return "";
}

rdcwstr UTF82Wide(const rdcstr &s)
{
  RDCFATAL("Converting UTF-8 to wide strings is not supported on Android!");
  return L"";
}

void Shutdown()
{
}
};

namespace OSUtility
{
void WriteOutput(int channel, const char *str)
{
  static uint32_t seq = 0;
  seq++;
  if(channel == OSUtility::Output_StdOut)
    fprintf(stdout, "%s", str);
  else if(channel == OSUtility::Output_StdErr)
    fprintf(stderr, "%s", str);
  else if(channel == OSUtility::Output_DebugMon)
    __android_log_print(ANDROID_LOG_INFO, LOGCAT_TAG, "@%08x%08x@ %s",
                        uint32_t(Timing::GetUTCTime()), seq, str);
}

uint64_t GetMachineIdent()
{
  uint64_t ret = MachineIdent_Android;

#if defined(_M_ARM) || defined(__arm__) || defined(__aarch64__)
  ret |= MachineIdent_Arch_ARM;
#else
  ret |= MachineIdent_Arch_x86;
#endif

#if ENABLED(RDOC_X64)
  ret |= MachineIdent_64bit;
#else
  ret |= MachineIdent_32bit;
#endif

  return ret;
}
};
