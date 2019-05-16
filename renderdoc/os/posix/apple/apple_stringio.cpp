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

#include <dlfcn.h>
#include <errno.h>
#include <iconv.h>
#include <mach-o/dyld.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include "common/threading.h"
#include "os/os_specific.h"

namespace Keyboard
{
void Init()
{
}

bool PlatformHasKeyInput()
{
  return false;
}

void AddInputWindow(void *wnd)
{
}

void RemoveInputWindow(void *wnd)
{
}

bool GetKeyState(int key)
{
  return false;
}
}

namespace FileIO
{
std::string GetTempRootPath()
{
  return "/tmp";
}

std::string GetAppFolderFilename(const std::string &filename)
{
  passwd *pw = getpwuid(getuid());
  const char *homedir = pw->pw_dir;

  std::string ret = std::string(homedir) + "/.renderdoc/";

  mkdir(ret.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  return ret + filename;
}

void GetExecutableFilename(std::string &selfName)
{
  char path[512] = {0};

  uint32_t pathSize = (uint32_t)sizeof(path);
  if(_NSGetExecutablePath(path, &pathSize) == 0)
  {
    selfName = std::string(path);
  }
  else
  {
    pathSize++;
    char *allocPath = new char[pathSize];
    memset(allocPath, 0, pathSize);
    if(_NSGetExecutablePath(path, &pathSize) == 0)
    {
      selfName = std::string(path);
    }
    else
    {
      selfName = "/unknown/unknown";
      RDCERR("Can't get executable name");
      delete[] allocPath;
      return;    // don't try and readlink this
    }
    delete[] allocPath;
  }

  memset(path, 0, sizeof(path));
  readlink(selfName.c_str(), path, 511);

  if(path[0] != 0)
    selfName = std::string(path);
}

int LibraryLocator = 42;

void GetLibraryFilename(std::string &selfName)
{
  Dl_info info;
  if(dladdr(&LibraryLocator, &info))
  {
    selfName = info.dli_fname;
  }
  else
  {
    RDCERR("dladdr failed to get library path");
    selfName = "";
  }
}
};

namespace StringFormat
{
// cache iconv_t descriptor to save on iconv_open/iconv_close each time
iconv_t iconvWide2UTF8 = (iconv_t)-1;
iconv_t iconvUTF82Wide = (iconv_t)-1;

// iconv is not thread safe when sharing an iconv_t descriptor
// I don't expect much contention but if it happens we could TryLock
// before creating a temporary iconv_t, or hold two iconv_ts, or something.
Threading::CriticalSection iconvLock;

void Shutdown()
{
  SCOPED_LOCK(iconvLock);

  if(iconvWide2UTF8 != (iconv_t)-1)
    iconv_close(iconvWide2UTF8);
  iconvWide2UTF8 = (iconv_t)-1;

  if(iconvUTF82Wide != (iconv_t)-1)
    iconv_close(iconvUTF82Wide);
  iconvUTF82Wide = (iconv_t)-1;
}

std::string Wide2UTF8(const std::wstring &s)
{
  // include room for null terminator, assuming unicode input (not ucs)
  // utf-8 characters can be max 4 bytes.
  size_t len = (s.length() + 1) * 4;

  std::vector<char> charBuffer(len);

  size_t ret;

  {
    SCOPED_LOCK(iconvLock);

    if(iconvWide2UTF8 == (iconv_t)-1)
      iconvWide2UTF8 = iconv_open("UTF-8", "WCHAR_T");

    if(iconvWide2UTF8 == (iconv_t)-1)
    {
      RDCERR("Couldn't open iconv for WCHAR_T to UTF-8: %d", errno);
      return "";
    }

    char *inbuf = (char *)s.c_str();
    size_t insize = (s.length() + 1) * sizeof(wchar_t);    // include null terminator
    char *outbuf = &charBuffer[0];
    size_t outsize = len;

    ret = iconv(iconvWide2UTF8, &inbuf, &insize, &outbuf, &outsize);
  }

  if(ret == (size_t)-1)
  {
#if ENABLED(RDOC_DEVEL)
    RDCWARN("Failed to convert wstring");
#endif
    return "";
  }

  // convert to string from null-terminated string - utf-8 never contains
  // 0 bytes before the null terminator, and this way we don't care if
  // charBuffer is larger than the string
  return std::string(&charBuffer[0]);
}

std::wstring UTF82Wide(const std::string &s)
{
  // include room for null terminator, for ascii input we need at least as many output chars as
  // input.
  size_t len = s.length() + 1;

  std::vector<wchar_t> wcharBuffer(len);

  size_t ret;

  {
    SCOPED_LOCK(iconvLock);

    if(iconvUTF82Wide == (iconv_t)-1)
      iconvUTF82Wide = iconv_open("WCHAR_T", "UTF-8");

    if(iconvUTF82Wide == (iconv_t)-1)
    {
      RDCERR("Couldn't open iconv for UTF-8 to WCHAR_T: %d", errno);
      return L"";
    }

    char *inbuf = (char *)s.c_str();
    size_t insize = s.length() + 1;    // include null terminator
    char *outbuf = (char *)&wcharBuffer[0];
    size_t outsize = len * sizeof(wchar_t);

    ret = iconv(iconvUTF82Wide, &inbuf, &insize, &outbuf, &outsize);
  }

  if(ret == (size_t)-1)
  {
#if ENABLED(RDOC_DEVEL)
    RDCWARN("Failed to convert wstring");
#endif
    return L"";
  }

  // convert to string from null-terminated string
  return std::wstring(&wcharBuffer[0]);
}
};

namespace OSUtility
{
void WriteOutput(int channel, const char *str)
{
  if(channel == OSUtility::Output_StdOut)
    fprintf(stdout, "%s", str);
  else if(channel == OSUtility::Output_StdErr)
    fprintf(stderr, "%s", str);
}

uint64_t GetMachineIdent()
{
  uint64_t ret = MachineIdent_macOS;

#if defined(_M_ARM) || defined(__arm__)
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
