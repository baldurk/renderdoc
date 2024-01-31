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

// To get kVK_ entries
#include <Carbon/Carbon.h>
#include <dlfcn.h>
#include <errno.h>
#include <iconv.h>
#include <mach-o/dyld.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include "api/app/renderdoc_app.h"
#include "common/common.h"
#include "common/threading.h"
#include "os/os_specific.h"

// helpers defined in apple_helpers.mm
extern void apple_InitKeyboard();
extern bool apple_IsKeyPressed(int appleKeyCode);

namespace Keyboard
{
void Init()
{
  apple_InitKeyboard();
}

bool PlatformHasKeyInput()
{
  return true;
}

void AddInputWindow(WindowingSystem windowSystem, void *wnd)
{
}

void RemoveInputWindow(WindowingSystem windowSystem, void *wnd)
{
}

bool GetKeyState(int key)
{
  unsigned short appleKeyCode;

  switch(key)
  {
    case eRENDERDOC_Key_A: appleKeyCode = kVK_ANSI_A; break;
    case eRENDERDOC_Key_B: appleKeyCode = kVK_ANSI_B; break;
    case eRENDERDOC_Key_C: appleKeyCode = kVK_ANSI_C; break;
    case eRENDERDOC_Key_D: appleKeyCode = kVK_ANSI_D; break;
    case eRENDERDOC_Key_E: appleKeyCode = kVK_ANSI_E; break;
    case eRENDERDOC_Key_F: appleKeyCode = kVK_ANSI_F; break;
    case eRENDERDOC_Key_G: appleKeyCode = kVK_ANSI_G; break;
    case eRENDERDOC_Key_H: appleKeyCode = kVK_ANSI_H; break;
    case eRENDERDOC_Key_I: appleKeyCode = kVK_ANSI_I; break;
    case eRENDERDOC_Key_J: appleKeyCode = kVK_ANSI_J; break;
    case eRENDERDOC_Key_K: appleKeyCode = kVK_ANSI_K; break;
    case eRENDERDOC_Key_L: appleKeyCode = kVK_ANSI_L; break;
    case eRENDERDOC_Key_M: appleKeyCode = kVK_ANSI_M; break;
    case eRENDERDOC_Key_N: appleKeyCode = kVK_ANSI_N; break;
    case eRENDERDOC_Key_O: appleKeyCode = kVK_ANSI_O; break;
    case eRENDERDOC_Key_P: appleKeyCode = kVK_ANSI_P; break;
    case eRENDERDOC_Key_Q: appleKeyCode = kVK_ANSI_Q; break;
    case eRENDERDOC_Key_R: appleKeyCode = kVK_ANSI_R; break;
    case eRENDERDOC_Key_S: appleKeyCode = kVK_ANSI_S; break;
    case eRENDERDOC_Key_T: appleKeyCode = kVK_ANSI_T; break;
    case eRENDERDOC_Key_U: appleKeyCode = kVK_ANSI_U; break;
    case eRENDERDOC_Key_V: appleKeyCode = kVK_ANSI_V; break;
    case eRENDERDOC_Key_W: appleKeyCode = kVK_ANSI_W; break;
    case eRENDERDOC_Key_X: appleKeyCode = kVK_ANSI_X; break;
    case eRENDERDOC_Key_Y: appleKeyCode = kVK_ANSI_Y; break;
    case eRENDERDOC_Key_Z: appleKeyCode = kVK_ANSI_Z; break;

    case eRENDERDOC_Key_0: appleKeyCode = kVK_ANSI_0; break;
    case eRENDERDOC_Key_1: appleKeyCode = kVK_ANSI_1; break;
    case eRENDERDOC_Key_2: appleKeyCode = kVK_ANSI_2; break;
    case eRENDERDOC_Key_3: appleKeyCode = kVK_ANSI_3; break;
    case eRENDERDOC_Key_4: appleKeyCode = kVK_ANSI_4; break;
    case eRENDERDOC_Key_5: appleKeyCode = kVK_ANSI_5; break;
    case eRENDERDOC_Key_6: appleKeyCode = kVK_ANSI_6; break;
    case eRENDERDOC_Key_7: appleKeyCode = kVK_ANSI_7; break;
    case eRENDERDOC_Key_8: appleKeyCode = kVK_ANSI_8; break;
    case eRENDERDOC_Key_9: appleKeyCode = kVK_ANSI_9; break;

    case eRENDERDOC_Key_Divide: appleKeyCode = kVK_ANSI_KeypadDivide; break;
    case eRENDERDOC_Key_Multiply: appleKeyCode = kVK_ANSI_KeypadMultiply; break;
    case eRENDERDOC_Key_Subtract: appleKeyCode = kVK_ANSI_KeypadMinus; break;
    case eRENDERDOC_Key_Plus: appleKeyCode = kVK_ANSI_KeypadPlus; break;

    case eRENDERDOC_Key_F1: appleKeyCode = kVK_F1; break;
    case eRENDERDOC_Key_F2: appleKeyCode = kVK_F2; break;
    case eRENDERDOC_Key_F3: appleKeyCode = kVK_F3; break;
    case eRENDERDOC_Key_F4: appleKeyCode = kVK_F4; break;
    case eRENDERDOC_Key_F5: appleKeyCode = kVK_F5; break;
    case eRENDERDOC_Key_F6: appleKeyCode = kVK_F6; break;
    case eRENDERDOC_Key_F7: appleKeyCode = kVK_F7; break;
    case eRENDERDOC_Key_F8: appleKeyCode = kVK_F8; break;
    case eRENDERDOC_Key_F9: appleKeyCode = kVK_F9; break;
    case eRENDERDOC_Key_F10: appleKeyCode = kVK_F10; break;
    case eRENDERDOC_Key_F11: appleKeyCode = kVK_F11; break;
    case eRENDERDOC_Key_F12: appleKeyCode = kVK_F12; break;

    case eRENDERDOC_Key_Home: appleKeyCode = kVK_Home; break;
    case eRENDERDOC_Key_End: appleKeyCode = kVK_End; break;
    case eRENDERDOC_Key_Insert: appleKeyCode = kVK_Help; break;
    case eRENDERDOC_Key_Delete: appleKeyCode = kVK_ForwardDelete; break;
    case eRENDERDOC_Key_PageUp: appleKeyCode = kVK_PageUp; break;
    case eRENDERDOC_Key_PageDn: appleKeyCode = kVK_PageDown; break;
    case eRENDERDOC_Key_Backspace: appleKeyCode = kVK_Delete; break;
    case eRENDERDOC_Key_Tab: appleKeyCode = kVK_Tab; break;
    case eRENDERDOC_Key_PrtScrn: appleKeyCode = kVK_F13; break;
    case eRENDERDOC_Key_Pause: appleKeyCode = kVK_F16; break;
    default: return false;
  }
  return apple_IsKeyPressed(appleKeyCode);
}
}

namespace FileIO
{
rdcstr GetTempRootPath()
{
  return "/tmp";
}

rdcstr GetAppFolderFilename(const rdcstr &filename)
{
  passwd *pw = getpwuid(getuid());
  const char *homedir = pw->pw_dir;

  rdcstr ret = rdcstr(homedir) + "/.renderdoc/";

  mkdir(ret.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

  return ret + filename;
}

rdcstr DefaultFindFileInPath(const rdcstr &fileName);
rdcstr FindFileInPath(const rdcstr &fileName)
{
  return DefaultFindFileInPath(fileName);
}

void GetExecutableFilename(rdcstr &selfName)
{
  char path[512] = {0};

  uint32_t pathSize = (uint32_t)sizeof(path);
  if(_NSGetExecutablePath(path, &pathSize) == 0)
  {
    selfName = rdcstr(path);
  }
  else
  {
    pathSize++;
    if(_NSGetExecutablePath(path, &pathSize) == 0)
    {
      selfName = rdcstr(path);
    }
    else
    {
      selfName = "/unknown/unknown";
      RDCERR("Can't get executable name");
      return;    // don't try and readlink this
    }
  }

  memset(path, 0, sizeof(path));
  readlink(selfName.c_str(), path, 511);

  if(path[0] != 0)
    selfName = rdcstr(path);
}

int LibraryLocator = 42;

void GetLibraryFilename(rdcstr &selfName)
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

rdcstr Wide2UTF8(const rdcwstr &s)
{
  // include room for null terminator, assuming unicode input (not ucs)
  // utf-8 characters can be max 4 bytes.
  size_t len = (s.length() + 1) * 4;

  rdcarray<char> charBuffer;
  charBuffer.resize(len);

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
  return rdcstr(&charBuffer[0]);
}

rdcwstr UTF82Wide(const rdcstr &s)
{
  // include room for null terminator, for ascii input we need at least as many output chars as
  // input.
  size_t len = s.length() + 1;

  rdcarray<wchar_t> wcharBuffer;
  wcharBuffer.resize(len);

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
  return rdcwstr(&wcharBuffer[0]);
}
};

// Helper method to avoid #include file conflicts between
// <Carbon/Carbon.h> and "core/core.h"
bool ShouldOutputDebugMon();

namespace OSUtility
{
void WriteOutput(int channel, const char *str)
{
  if(channel == OSUtility::Output_StdOut)
    fprintf(stdout, "%s", str);
  else if(channel == OSUtility::Output_StdErr)
    fprintf(stderr, "%s", str);
  else if(channel == OSUtility::Output_DebugMon && ShouldOutputDebugMon())
    fprintf(stdout, "%s", str);
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
