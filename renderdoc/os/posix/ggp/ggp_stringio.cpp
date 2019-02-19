/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include "api/app/renderdoc_app.h"
#include "common/threading.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

using std::string;

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
string GetTempRootPath()
{
  return "/tmp";
}

string GetAppFolderFilename(const string &filename)
{
  const char *homedir = NULL;
  if(getenv("HOME") != NULL)
  {
    homedir = getenv("HOME");
    RDCLOG("$HOME value is %s", homedir);
  }
  else
  {
    RDCLOG("$HOME value is NULL");
    homedir = getpwuid(getuid())->pw_dir;
  }

  string ret = string(homedir) + "/.renderdoc/";

  mkdir(ret.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  return ret + filename;
}

void GetExecutableFilename(string &selfName)
{
  char path[512] = {0};
  readlink("/proc/self/exe", path, 511);

  selfName = string(path);
}

int LibraryLocator = 42;

void GetLibraryFilename(string &selfName)
{
  // this is a hack, but the only reliable way to find the absolute path to the library.
  // dladdr would be fine but it returns the wrong result for symbols in the library

  string librenderdoc_path;

  FILE *f = fopen("/proc/self/maps", "r");

  if(f)
  {
    // read the whole thing in one go. There's no need to try and be tight with
    // this allocation, so just make sure we can read everything.
    char *map_string = new char[1024 * 1024];
    memset(map_string, 0, 1024 * 1024);

    ::fread(map_string, 1, 1024 * 1024, f);

    ::fclose(f);

    char *c = strstr(map_string, "/librenderdoc.so");

    if(c)
    {
      // walk backwards until we hit the start of the line
      while(c > map_string)
      {
        c--;

        if(c[0] == '\n')
        {
          c++;
          break;
        }
      }

      // walk forwards across the address range (00400000-0040c000)
      while(isalnum(c[0]) || c[0] == '-')
        c++;

      // whitespace
      while(c[0] == ' ')
        c++;

      // permissions (r-xp)
      while(isalpha(c[0]) || c[0] == '-')
        c++;

      // whitespace
      while(c[0] == ' ')
        c++;

      // offset (0000b000)
      while(isalnum(c[0]) || c[0] == '-')
        c++;

      // whitespace
      while(c[0] == ' ')
        c++;

      // dev
      while(isalnum(c[0]) || c[0] == ':')
        c++;

      // whitespace
      while(c[0] == ' ')
        c++;

      // inode
      while(isdigit(c[0]))
        c++;

      // whitespace
      while(c[0] == ' ')
        c++;

      // FINALLY we are at the start of the actual path
      char *end = strchr(c, '\n');

      if(end)
        librenderdoc_path = string(c, end - c);
    }

    delete[] map_string;
  }

  if(librenderdoc_path.empty())
  {
    RDCWARN("Couldn't get librenderdoc.so path from /proc/self/maps, falling back to dladdr");

    Dl_info info;
    if(dladdr(&LibraryLocator, &info))
      librenderdoc_path = info.dli_fname;
  }

  selfName = librenderdoc_path;
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
  {
    fprintf(stdout, "%s", str);
    fflush(stdout);
  }
  else if(channel == OSUtility::Output_StdErr)
  {
    fprintf(stderr, "%s", str);
    fflush(stderr);
  }
}

uint64_t GetMachineIdent()
{
  uint64_t ret = MachineIdent_Linux;

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
