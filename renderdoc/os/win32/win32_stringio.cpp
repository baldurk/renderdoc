/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include <io.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <stdio.h>
#include <string.h>
#include <tchar.h>
#include <time.h>
#include <set>
#include "api/app/renderdoc_app.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

// gives us an address to identify this dll with
static int dllLocator = 0;

std::string GetDynamicEmbeddedResource(int resource)
{
  HMODULE mod = NULL;
  GetModuleHandleExA(
      GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      (const char *)&dllLocator, &mod);

  HRSRC res = FindResource(mod, MAKEINTRESOURCE(resource), MAKEINTRESOURCE(TYPE_EMBED));

  if(res == NULL)
  {
    RDCFATAL("Couldn't find embedded win32 resource");
  }

  HGLOBAL data = LoadResource(mod, res);

  if(data != NULL)
  {
    DWORD resSize = SizeofResource(mod, res);
    const char *resData = (const char *)LockResource(data);

    if(resData)
      return std::string(resData, resData + resSize);
  }

  return "";
}

namespace Keyboard
{
void Init()
{
}

bool PlatformHasKeyInput()
{
  return true;
}

std::set<HWND> inputWindows;

void AddInputWindow(void *wnd)
{
  inputWindows.insert((HWND)wnd);
}

void RemoveInputWindow(void *wnd)
{
  inputWindows.erase((HWND)wnd);
}

bool GetKeyState(int key)
{
  int vk = 0;

  if(key >= eRENDERDOC_Key_A && key <= eRENDERDOC_Key_Z)
    vk = key;
  if(key >= eRENDERDOC_Key_0 && key <= eRENDERDOC_Key_9)
    vk = key;

  switch(key)
  {
    case eRENDERDOC_Key_Divide: vk = VK_DIVIDE; break;
    case eRENDERDOC_Key_Multiply: vk = VK_MULTIPLY; break;
    case eRENDERDOC_Key_Subtract: vk = VK_SUBTRACT; break;
    case eRENDERDOC_Key_Plus: vk = VK_ADD; break;
    case eRENDERDOC_Key_F1: vk = VK_F1; break;
    case eRENDERDOC_Key_F2: vk = VK_F2; break;
    case eRENDERDOC_Key_F3: vk = VK_F3; break;
    case eRENDERDOC_Key_F4: vk = VK_F4; break;
    case eRENDERDOC_Key_F5: vk = VK_F5; break;
    case eRENDERDOC_Key_F6: vk = VK_F6; break;
    case eRENDERDOC_Key_F7: vk = VK_F7; break;
    case eRENDERDOC_Key_F8: vk = VK_F8; break;
    case eRENDERDOC_Key_F9: vk = VK_F9; break;
    case eRENDERDOC_Key_F10: vk = VK_F10; break;
    case eRENDERDOC_Key_F11: vk = VK_F11; break;
    case eRENDERDOC_Key_F12: vk = VK_F12; break;
    case eRENDERDOC_Key_Home: vk = VK_HOME; break;
    case eRENDERDOC_Key_End: vk = VK_END; break;
    case eRENDERDOC_Key_Insert: vk = VK_INSERT; break;
    case eRENDERDOC_Key_Delete: vk = VK_DELETE; break;
    case eRENDERDOC_Key_PageUp: vk = VK_PRIOR; break;
    case eRENDERDOC_Key_PageDn: vk = VK_NEXT; break;
    case eRENDERDOC_Key_Backspace: vk = VK_BACK; break;
    case eRENDERDOC_Key_Tab: vk = VK_TAB; break;
    case eRENDERDOC_Key_PrtScrn: vk = VK_SNAPSHOT; break;
    case eRENDERDOC_Key_Pause: vk = VK_PAUSE; break;
    default: break;
  }

  if(vk == 0)
    return false;

  bool keydown = GetAsyncKeyState(vk) != 0;

  if(inputWindows.empty() || !keydown)
    return keydown;

  for(auto it = inputWindows.begin(); it != inputWindows.end(); ++it)
  {
    HWND w = *it;
    HWND fore = GetForegroundWindow();

    while(w)
    {
      if(w == fore)
        return keydown;

      w = GetParent(w);
    }
  }

  return false;
}
}

namespace FileIO
{
void GetExecutableFilename(std::string &selfName)
{
  wchar_t curFile[512] = {0};
  GetModuleFileNameW(NULL, curFile, 511);

  selfName = StringFormat::Wide2UTF8(std::wstring(curFile));
}

void GetLibraryFilename(std::string &selfName)
{
  wchar_t curFile[512] = {0};
  GetModuleFileNameW(GetModuleHandleA(STRINGIZE(RDOC_DLL_FILE) ".dll"), curFile, 511);

  selfName = StringFormat::Wide2UTF8(std::wstring(curFile));
}

bool IsRelativePath(const std::string &path)
{
  if(path.empty())
    return false;

  std::wstring wpath = StringFormat::UTF82Wide(path.c_str());
  return PathIsRelativeW(wpath.c_str()) != 0;
}

std::string GetFullPathname(const std::string &filename)
{
  std::wstring wfn = StringFormat::UTF82Wide(filename);

  wchar_t path[512] = {0};
  GetFullPathNameW(wfn.c_str(), ARRAY_COUNT(path) - 1, path, NULL);

  return StringFormat::Wide2UTF8(std::wstring(path));
}

std::string FindFileInPath(const std::string &file)
{
  std::string filePath;

  // Search the PATH directory list for the application (like shell where) to get the absolute path
  // Return "" if no exectuable found in the PATH list
  const DWORD size = 65535;
  char envPath[size];
  if(GetEnvironmentVariableA("PATH", envPath, size) == 0)
    return filePath;

  char *next = NULL;
  const char *pathSeparator = ";";
  const char *path = strtok_s(envPath, pathSeparator, &next);
  std::wstring fileName = StringFormat::UTF82Wide(file);

  while(path)
  {
    std::wstring testPath = StringFormat::UTF82Wide(path);

    // Check for the following extensions. If fileName already has one, they will be ignored.
    std::vector<std::wstring> extensions;
    extensions.push_back(L".exe");
    extensions.push_back(L".bat");

    for(uint32_t i = 0; i < extensions.size(); i++)
    {
      wchar_t foundPath[512] = {0};
      if(SearchPathW(testPath.c_str(), fileName.c_str(), extensions[i].c_str(),
                     ARRAY_COUNT(foundPath) - 1, foundPath, NULL) != 0)
      {
        filePath = StringFormat::Wide2UTF8(std::wstring(foundPath));
        break;
      }
    }

    path = strtok_s(NULL, pathSeparator, &next);
  }

  return filePath;
}

void CreateParentDirectory(const std::string &filename)
{
  std::wstring wfn = StringFormat::UTF82Wide(get_dirname(filename));

  // This function needs \\s not /s. So stupid!
  for(size_t i = 0; i < wfn.size(); i++)
    if(wfn[i] == L'/')
      wfn[i] = L'\\';

  // Remove trailing \\s
  size_t trailingSeperatorPos = wfn.find_last_of(L'\\');
  while(trailingSeperatorPos == wfn.length() - 1)
  {
    wfn.erase(trailingSeperatorPos);
    trailingSeperatorPos = wfn.find_last_of(L'\\');
  }

  // Find all directories we need to create
  std::vector<std::wstring> directoriesToCreate;
  while(!wfn.empty())
  {
    DWORD fileAttributes = GetFileAttributesW(wfn.c_str());
    if(fileAttributes != INVALID_FILE_ATTRIBUTES)
      break;

    directoriesToCreate.push_back(wfn);

    size_t seperatorPos = wfn.find_last_of(L'\\');
    if(seperatorPos == std::string::npos)
      break;
    else
      wfn = wfn.substr(0, seperatorPos);
  }

  // Create the directories in reverse order
  for(ptrdiff_t i = directoriesToCreate.size() - 1; i >= 0; i--)
  {
    CreateDirectoryW(directoriesToCreate[i].c_str(), NULL);
  }
}

std::string GetReplayAppFilename()
{
  HMODULE hModule = NULL;
  GetModuleHandleEx(
      GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
      (LPCTSTR)&dllLocator, &hModule);
  wchar_t curFile[512] = {0};
  GetModuleFileNameW(hModule, curFile, 511);

  std::string path = StringFormat::Wide2UTF8(std::wstring(curFile));
  path = get_dirname(path);
  std::string exe = path + "/qrenderdoc.exe";

  FILE *f = FileIO::fopen(exe.c_str(), "rb");
  if(f)
  {
    FileIO::fclose(f);
    return exe;
  }

  // if qrenderdoc.exe doesn't live in the same dir, we must be in x86/
  // so look one up the tree.
  exe = path + "/../qrenderdoc.exe";

  f = FileIO::fopen(exe.c_str(), "rb");
  if(f)
  {
    FileIO::fclose(f);
    return exe;
  }

  // if we didn't find the exe at all, we must not be in a standard
  // distributed renderdoc package. On windows we can check in the registry
  // to try and find the installed path.

  DWORD type = 0;
  DWORD dataSize = sizeof(curFile);
  RDCEraseEl(curFile);
  RegGetValueW(HKEY_CLASSES_ROOT, L"RenderDoc.RDCCapture.1\\DefaultIcon", NULL, RRF_RT_ANY, &type,
               (void *)curFile, &dataSize);

  if(type == REG_EXPAND_SZ || type == REG_SZ)
  {
    return StringFormat::Wide2UTF8(std::wstring(curFile));
  }

  return "";
}

void GetDefaultFiles(const char *logBaseName, std::string &capture_filename,
                     std::string &logging_filename, std::string &target)
{
  wchar_t temp_filename[MAX_PATH];

  GetTempPathW(MAX_PATH, temp_filename);

  wchar_t curFile[512];
  GetModuleFileNameW(NULL, curFile, 512);

  wchar_t fn[MAX_PATH];
  wcscpy_s(fn, MAX_PATH, curFile);

  wchar_t *mod = wcsrchr(fn, L'.');
  if(mod)
    *mod = 0;
  mod = wcsrchr(fn, L'/');
  if(!mod)
    mod = fn;
  mod = wcsrchr(mod, L'\\');

  mod++;    // now points to base filename without extension

  target = StringFormat::Wide2UTF8(std::wstring(mod));

  time_t t = time(NULL);
  tm now;
  localtime_s(&now, &t);

  wchar_t *filename_start = temp_filename + wcslen(temp_filename);

  wsprintf(filename_start, L"RenderDoc\\%ls_%04d.%02d.%02d_%02d.%02d.rdc", mod, 1900 + now.tm_year,
           now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min);

  capture_filename = StringFormat::Wide2UTF8(std::wstring(temp_filename));

  *filename_start = 0;

  std::wstring wbase = StringFormat::UTF82Wide(std::string(logBaseName));

  wsprintf(filename_start, L"RenderDoc\\%ls_%04d.%02d.%02d_%02d.%02d.%02d.log", wbase.c_str(),
           1900 + now.tm_year, now.tm_mon + 1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);

  logging_filename = StringFormat::Wide2UTF8(std::wstring(temp_filename));
}

std::string GetHomeFolderFilename()
{
  PWSTR docsPath;
  SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_SIMPLE_IDLIST | KF_FLAG_DONT_UNEXPAND, NULL,
                       &docsPath);
  std::wstring documents = docsPath;
  CoTaskMemFree(docsPath);

  if(documents[documents.size() - 1] == '/' || documents[documents.size() - 1] == '\\')
    documents.pop_back();

  return StringFormat::Wide2UTF8(documents);
}

std::string GetAppFolderFilename(const std::string &filename)
{
  PWSTR appDataPath = NULL;
  HRESULT hr = SHGetKnownFolderPath(
      FOLDERID_RoamingAppData, KF_FLAG_SIMPLE_IDLIST | KF_FLAG_DONT_UNEXPAND, NULL, &appDataPath);
  if(appDataPath == NULL || FAILED(hr))
    return "";

  std::wstring appdata = appDataPath;
  CoTaskMemFree(appDataPath);

  if(appdata[appdata.size() - 1] == '/' || appdata[appdata.size() - 1] == '\\')
    appdata.pop_back();

  appdata += L"\\renderdoc\\";

  CreateDirectoryW(appdata.c_str(), NULL);

  std::string ret = StringFormat::Wide2UTF8(appdata) + filename;
  return ret;
}

std::string GetTempFolderFilename()
{
  wchar_t temp_filename[MAX_PATH];

  GetTempPathW(MAX_PATH, temp_filename);

  return StringFormat::Wide2UTF8(std::wstring(temp_filename));
}

uint64_t GetModifiedTimestamp(const std::string &filename)
{
  std::wstring wfn = StringFormat::UTF82Wide(filename);

  struct __stat64 st;
  int res = _wstat64(wfn.c_str(), &st);

  if(res == 0)
  {
    return (uint64_t)st.st_mtime;
  }

  return 0;
}

bool Copy(const char *from, const char *to, bool allowOverwrite)
{
  std::wstring wfrom = StringFormat::UTF82Wide(std::string(from));
  std::wstring wto = StringFormat::UTF82Wide(std::string(to));

  return ::CopyFileW(wfrom.c_str(), wto.c_str(), allowOverwrite == false) != 0;
}

bool Move(const char *from, const char *to, bool allowOverwrite)
{
  std::wstring wfrom = StringFormat::UTF82Wide(std::string(from));
  std::wstring wto = StringFormat::UTF82Wide(std::string(to));

  if(exists(to))
  {
    if(allowOverwrite)
      Delete(to);
    else
      return false;
  }

  return ::MoveFileW(wfrom.c_str(), wto.c_str()) != 0;
}

void Delete(const char *path)
{
  std::wstring wpath = StringFormat::UTF82Wide(std::string(path));
  ::DeleteFileW(wpath.c_str());
}

std::vector<PathEntry> GetFilesInDirectory(const char *path)
{
  std::vector<PathEntry> ret;

  if(path[0] == '/' && path[1] == 0)
  {
    DWORD driveMask = GetLogicalDrives();

    for(int i = 0; i < 26; i++)
    {
      DWORD mask = (1 << i);

      if(driveMask & mask)
      {
        std::string fn = "A:/";
        fn[0] = char('A' + i);

        ret.push_back(PathEntry(fn.c_str(), PathProperty::Directory));
      }
    }

    return ret;
  }

  std::string pathstr = path;

  // normalise path to windows style
  for(size_t i = 0; i < pathstr.size(); i++)
    if(pathstr[i] == '/')
      pathstr[i] = '\\';

  // remove any trailing slash
  if(pathstr[pathstr.size() - 1] == '\\')
    pathstr.resize(pathstr.size() - 1);

  // append '\*' to do the search we want
  pathstr += "\\*";

  std::wstring wpath = StringFormat::UTF82Wide(pathstr);

  WIN32_FIND_DATAW findData = {};
  HANDLE find = FindFirstFileW(wpath.c_str(), &findData);

  if(find == INVALID_HANDLE_VALUE)
  {
    DWORD err = GetLastError();

    PathProperty flags = PathProperty::ErrorUnknown;

    if(err == ERROR_FILE_NOT_FOUND)
      flags = PathProperty::ErrorInvalidPath;
    else if(err == ERROR_ACCESS_DENIED)
      flags = PathProperty::ErrorAccessDenied;

    ret.push_back(PathEntry(path, flags));
    return ret;
  }

  do
  {
    if(findData.cFileName[0] == L'.' && findData.cFileName[1] == 0)
    {
      // skip "."
    }
    else if(findData.cFileName[0] == L'.' && findData.cFileName[1] == L'.' &&
            findData.cFileName[2] == 0)
    {
      // skip ".."
    }
    else
    {
      PathProperty flags = PathProperty::NoFlags;

      if(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        flags |= PathProperty::Directory;

      if(findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)
        flags |= PathProperty::Hidden;

      if(wcsstr(findData.cFileName, L".EXE") || wcsstr(findData.cFileName, L".exe") ||
         wcsstr(findData.cFileName, L".Exe"))
      {
        flags |= PathProperty::Executable;
      }

      PathEntry f(StringFormat::Wide2UTF8(findData.cFileName).c_str(), flags);

      uint64_t nanosecondsSinceWindowsEpoch = uint64_t(findData.ftLastWriteTime.dwHighDateTime) << 8 |
                                              uint64_t(findData.ftLastWriteTime.dwLowDateTime);

      uint64_t secondsSinceWindowsEpoch = nanosecondsSinceWindowsEpoch / 10000000;

      // this constant is the number of seconds between Jan 1 1601 and Jan 1 1970
      uint64_t secondsSinceUnixEpoch = secondsSinceWindowsEpoch - 11644473600;

      f.lastmod = uint32_t(secondsSinceUnixEpoch);
      f.size = uint64_t(findData.nFileSizeHigh) << 8 | uint64_t(findData.nFileSizeLow);

      ret.push_back(f);
    }
  } while(FindNextFile(find, &findData) != FALSE);

  // don't care if we hit an error or enumerated all files, just finish

  FindClose(find);

  return ret;
}

FILE *fopen(const char *filename, const char *mode)
{
  std::wstring wfn = StringFormat::UTF82Wide(std::string(filename));
  std::wstring wmode = StringFormat::UTF82Wide(std::string(mode));

  FILE *ret = NULL;
  ::_wfopen_s(&ret, wfn.c_str(), wmode.c_str());
  return ret;
}

bool exists(const char *filename)
{
  std::wstring wfn = StringFormat::UTF82Wide(filename);

  struct __stat64 st;
  int res = _wstat64(wfn.c_str(), &st);

  return (res == 0);
}

std::string ErrorString()
{
  int err = errno;

  char buf[256] = {0};

  strerror_s(buf, err);

  return buf;
}

std::string getline(FILE *f)
{
  std::string ret;

  while(!FileIO::feof(f))
  {
    char c = (char)::fgetc(f);

    if(FileIO::feof(f))
      break;

    if(c != 0 && c != '\n')
      ret.push_back(c);
    else
      break;
  }

  return ret;
}

size_t fread(void *buf, size_t elementSize, size_t count, FILE *f)
{
  return ::fread(buf, elementSize, count, f);
}
size_t fwrite(const void *buf, size_t elementSize, size_t count, FILE *f)
{
  return ::fwrite(buf, elementSize, count, f);
}

uint64_t ftell64(FILE *f)
{
  return ::_ftelli64(f);
}
void fseek64(FILE *f, uint64_t offset, int origin)
{
  ::_fseeki64(f, offset, origin);
}

bool feof(FILE *f)
{
  return ::feof(f) != 0;
}

void ftruncateat(FILE *f, uint64_t length)
{
  ::fflush(f);
  int fd = ::_fileno(f);
  ::_chsize_s(fd, (int64_t)length);
}

bool fflush(FILE *f)
{
  return ::fflush(f) == 0;
}

int fclose(FILE *f)
{
  return ::fclose(f);
}

static HANDLE logHandle = NULL;

bool logfile_open(const char *filename)
{
  std::wstring wfn = StringFormat::UTF82Wide(std::string(filename));
  logHandle = CreateFileW(wfn.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                          OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

  return logHandle != NULL;
}

static std::string logfile_readall_fallback(const wchar_t *filename)
{
  // if CreateFile/ReadFile failed, fall back and try regular stdio
  FILE *f = NULL;
  _wfopen_s(&f, filename, L"r");
  if(f)
  {
    ::_fseeki64(f, 0, SEEK_END);
    uint64_t filesize = ::_ftelli64(f);
    ::_fseeki64(f, 0, SEEK_SET);

    if(filesize > 10)
    {
      std::string ret;
      ret.resize((size_t)filesize);

      size_t numRead = ::fread(&ret[0], 1, (size_t)filesize, f);
      ret.resize(numRead);

      ::fclose(f);

      return ret;
    }

    ::fclose(f);
  }

  return "";
}

std::string logfile_readall(const char *filename)
{
  if(!exists(filename))
    return StringFormat::Fmt("Logfile '%s' doesn't exist", filename);

  std::wstring wfn = StringFormat::UTF82Wide(std::string(filename));
  HANDLE h = CreateFileW(wfn.c_str(), FILE_READ_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

  std::string ret;

  if(h == INVALID_HANDLE_VALUE)
  {
    DWORD err = GetLastError();

    ret = logfile_readall_fallback(wfn.c_str());
    ret += StringFormat::Fmt("\n\nCouldn't open logfile, CreateFile() threw %u\n\n", err);
  }
  else
  {
    DWORD len = GetFileSize(h, NULL);

    if(len == INVALID_FILE_SIZE)
    {
      DWORD err = GetLastError();

      ret = logfile_readall_fallback(wfn.c_str());
      ret += StringFormat::Fmt("\n\nFailed to read logfile, GetFileSize() threw %u", err);
    }
    else
    {
      ret.resize(len);

      DWORD dummy = len;

      ReadFile(h, &ret[0], len, &dummy, NULL);
    }

    CloseHandle(h);
  }

  return ret;
}

void logfile_append(const char *msg, size_t length)
{
  if(logHandle)
  {
    DWORD bytesWritten = 0;
    WriteFile(logHandle, msg, (DWORD)length, &bytesWritten, NULL);
  }
}

void logfile_close(const char *filename)
{
  CloseHandle(logHandle);
  logHandle = NULL;

  if(filename)
  {
    // we can just try to delete the file. If it's open elsewhere in another process, the delete
    // will fail.
    std::wstring wpath = StringFormat::UTF82Wide(std::string(filename));
    ::DeleteFileW(wpath.c_str());
  }
}
};

namespace StringFormat
{
void sntimef(time_t utcTime, char *str, size_t bufSize, const char *format)
{
  tm tmv;
  localtime_s(&tmv, &utcTime);

  wchar_t *buf = new wchar_t[bufSize + 1];
  buf[bufSize] = 0;
  std::wstring wfmt = StringFormat::UTF82Wide(std::string(format));

  wcsftime(buf, bufSize, wfmt.c_str(), &tmv);

  std::string result = StringFormat::Wide2UTF8(std::wstring(buf));

  delete[] buf;

  if(result.length() + 1 <= bufSize)
  {
    memcpy(str, result.c_str(), result.length());
    str[result.length()] = 0;
  }
}

void Shutdown()
{
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
#if ENABLED(RDOC_DEVEL)
    RDCWARN("Failed to convert wstring");    // can't pass string through as this would infinitely
                                             // recurse
#endif
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
#if ENABLED(RDOC_DEVEL)
    RDCWARN("Failed to convert utf-8 string");    // can't pass string through as this would
                                                  // infinitely recurse
#endif
    return L"";
  }

  return ret;
}
};

namespace OSUtility
{
void WriteOutput(int channel, const char *str)
{
  std::wstring wstr = StringFormat::UTF82Wide(std::string(str));

  if(channel == OSUtility::Output_DebugMon)
    OutputDebugStringW(wstr.c_str());
  else if(channel == OSUtility::Output_StdOut)
    fwprintf_s(stdout, L"%s", wstr.c_str());
  else if(channel == OSUtility::Output_StdErr)
    fwprintf_s(stderr, L"%s", wstr.c_str());
}

uint64_t GetMachineIdent()
{
  uint64_t ret = MachineIdent_Windows;

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
