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
#include "serialise/string_utils.h"

#if ENABLED(RDOC_XLIB)
#include <X11/Xlib.h>
#include <X11/keysym.h>
#endif

#if ENABLED(RDOC_XCB)
#include <xcb/xcb_keysyms.h>
#endif

using std::string;

namespace Keyboard
{
void Init()
{
}

bool PlatformHasKeyInput()
{
#if ENABLED(RDOC_XCB) || ENABLED(RDOC_XLIB)
  return true;
#else
  return false;
#endif
}

#if ENABLED(RDOC_XLIB)

Display *CurrentXDisplay = NULL;

void CloneDisplay(Display *dpy)
{
  if(CurrentXDisplay || dpy == NULL)
    return;

  CurrentXDisplay = XOpenDisplay(XDisplayString(dpy));
}

bool GetXlibKeyState(int key)
{
  if(CurrentXDisplay == NULL)
    return false;

  KeySym ks = 0;

  if(key >= eRENDERDOC_Key_A && key <= eRENDERDOC_Key_Z)
    ks = key;
  if(key >= eRENDERDOC_Key_0 && key <= eRENDERDOC_Key_9)
    ks = key;

  switch(key)
  {
    case eRENDERDOC_Key_Divide: ks = XK_KP_Divide; break;
    case eRENDERDOC_Key_Multiply: ks = XK_KP_Multiply; break;
    case eRENDERDOC_Key_Subtract: ks = XK_KP_Subtract; break;
    case eRENDERDOC_Key_Plus: ks = XK_KP_Add; break;
    case eRENDERDOC_Key_F1: ks = XK_F1; break;
    case eRENDERDOC_Key_F2: ks = XK_F2; break;
    case eRENDERDOC_Key_F3: ks = XK_F3; break;
    case eRENDERDOC_Key_F4: ks = XK_F4; break;
    case eRENDERDOC_Key_F5: ks = XK_F5; break;
    case eRENDERDOC_Key_F6: ks = XK_F6; break;
    case eRENDERDOC_Key_F7: ks = XK_F7; break;
    case eRENDERDOC_Key_F8: ks = XK_F8; break;
    case eRENDERDOC_Key_F9: ks = XK_F9; break;
    case eRENDERDOC_Key_F10: ks = XK_F10; break;
    case eRENDERDOC_Key_F11: ks = XK_F11; break;
    case eRENDERDOC_Key_F12: ks = XK_F12; break;
    case eRENDERDOC_Key_Home: ks = XK_Home; break;
    case eRENDERDOC_Key_End: ks = XK_End; break;
    case eRENDERDOC_Key_Insert: ks = XK_Insert; break;
    case eRENDERDOC_Key_Delete: ks = XK_Delete; break;
    case eRENDERDOC_Key_PageUp: ks = XK_Prior; break;
    case eRENDERDOC_Key_PageDn: ks = XK_Next; break;
    case eRENDERDOC_Key_Backspace: ks = XK_BackSpace; break;
    case eRENDERDOC_Key_Tab: ks = XK_Tab; break;
    case eRENDERDOC_Key_PrtScrn: ks = XK_Print; break;
    case eRENDERDOC_Key_Pause: ks = XK_Pause; break;
    default: break;
  }

  if(ks == 0)
    return false;

  KeyCode kc = XKeysymToKeycode(CurrentXDisplay, ks);

  char keyState[32];
  XQueryKeymap(CurrentXDisplay, keyState);

  int byteIdx = (kc / 8);
  int bitMask = 1 << (kc % 8);

  uint8_t keyByte = (uint8_t)keyState[byteIdx];

  return (keyByte & bitMask) != 0;
}

#else

// if RENDERDOC_WINDOWING_XLIB is not enabled

bool GetXlibKeyState(int key)
{
  return false;
}

#endif

#if ENABLED(RDOC_XCB)

xcb_connection_t *connection;
xcb_key_symbols_t *symbols;

void UseConnection(xcb_connection_t *conn)
{
  connection = conn;
  symbols = xcb_key_symbols_alloc(conn);
}

bool GetXCBKeyState(int key)
{
  if(symbols == NULL)
    return false;

  KeySym ks = 0;

  if(key >= eRENDERDOC_Key_A && key <= eRENDERDOC_Key_Z)
    ks = key;
  if(key >= eRENDERDOC_Key_0 && key <= eRENDERDOC_Key_9)
    ks = key;

  switch(key)
  {
    case eRENDERDOC_Key_Divide: ks = XK_KP_Divide; break;
    case eRENDERDOC_Key_Multiply: ks = XK_KP_Multiply; break;
    case eRENDERDOC_Key_Subtract: ks = XK_KP_Subtract; break;
    case eRENDERDOC_Key_Plus: ks = XK_KP_Add; break;
    case eRENDERDOC_Key_F1: ks = XK_F1; break;
    case eRENDERDOC_Key_F2: ks = XK_F2; break;
    case eRENDERDOC_Key_F3: ks = XK_F3; break;
    case eRENDERDOC_Key_F4: ks = XK_F4; break;
    case eRENDERDOC_Key_F5: ks = XK_F5; break;
    case eRENDERDOC_Key_F6: ks = XK_F6; break;
    case eRENDERDOC_Key_F7: ks = XK_F7; break;
    case eRENDERDOC_Key_F8: ks = XK_F8; break;
    case eRENDERDOC_Key_F9: ks = XK_F9; break;
    case eRENDERDOC_Key_F10: ks = XK_F10; break;
    case eRENDERDOC_Key_F11: ks = XK_F11; break;
    case eRENDERDOC_Key_F12: ks = XK_F12; break;
    case eRENDERDOC_Key_Home: ks = XK_Home; break;
    case eRENDERDOC_Key_End: ks = XK_End; break;
    case eRENDERDOC_Key_Insert: ks = XK_Insert; break;
    case eRENDERDOC_Key_Delete: ks = XK_Delete; break;
    case eRENDERDOC_Key_PageUp: ks = XK_Prior; break;
    case eRENDERDOC_Key_PageDn: ks = XK_Next; break;
    case eRENDERDOC_Key_Backspace: ks = XK_BackSpace; break;
    case eRENDERDOC_Key_Tab: ks = XK_Tab; break;
    case eRENDERDOC_Key_PrtScrn: ks = XK_Print; break;
    case eRENDERDOC_Key_Pause: ks = XK_Pause; break;
    default: break;
  }

  if(ks == 0)
    return false;

  xcb_keycode_t *keyCodes = xcb_key_symbols_get_keycode(symbols, ks);

  if(!keyCodes)
    return false;

  xcb_query_keymap_cookie_t keymapcookie = xcb_query_keymap(connection);
  xcb_query_keymap_reply_t *keys = xcb_query_keymap_reply(connection, keymapcookie, NULL);

  bool ret = false;

  if(keys && keyCodes[0] != XCB_NO_SYMBOL)
  {
    int byteIdx = (keyCodes[0] / 8);
    int bitMask = 1 << (keyCodes[0] % 8);

    ret = (keys->keys[byteIdx] & bitMask) != 0;
  }

  free(keyCodes);
  free(keys);

  return ret;
}

#else

// if RENDERDOC_WINDOWING_XCB is not enabled

bool GetXCBKeyState(int key)
{
  return false;
}

#endif

void AddInputWindow(void *wnd)
{
  // TODO check against this drawable & parent window being focused in GetKeyState
}

void RemoveInputWindow(void *wnd)
{
}

bool GetKeyState(int key)
{
  return GetXCBKeyState(key) || GetXlibKeyState(key);
}
}

namespace FileIO
{
const char *GetTempRootPath()
{
  return "/tmp";
}

string GetAppFolderFilename(const string &filename)
{
  passwd *pw = getpwuid(getuid());
  const char *homedir = pw->pw_dir;

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
};

namespace StringFormat
{
// cache iconv_t descriptor to save on iconv_open/iconv_close each time
iconv_t iconvWide2UTF8 = (iconv_t)-1;

// iconv is not thread safe when sharing an iconv_t descriptor
// I don't expect much contention but if it happens we could TryLock
// before creating a temporary iconv_t, or hold two iconv_ts, or something.
Threading::CriticalSection lockWide2UTF8;

string Wide2UTF8(const std::wstring &s)
{
  // include room for null terminator, assuming unicode input (not ucs)
  // utf-8 characters can be max 4 bytes.
  size_t len = (s.length() + 1) * 4;

  vector<char> charBuffer;

  if(charBuffer.size() < len)
    charBuffer.resize(len);

  size_t ret;

  {
    SCOPED_LOCK(lockWide2UTF8);

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
  return string(&charBuffer[0]);
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
