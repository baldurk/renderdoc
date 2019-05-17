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

// must be separate so that it's included first and not sorted by clang-format
#include <windows.h>

#include <Psapi.h>
#include <tchar.h>
#include <tlhelp32.h>
#include <string>
#include "core/core.h"
#include "os/os_specific.h"
#include "strings/string_utils.h"

// add wstring strlower overload since we don't want to always be converting to/from wchars.
static std::wstring strlower(std::wstring in)
{
  std::wstring ret;
  ret.resize(in.size());
  for(size_t i = 0; i < ret.size(); i++)
    ret[i] = towlower(in[i]);
  return ret;
}

static std::vector<EnvironmentModification> &GetEnvModifications()
{
  static std::vector<EnvironmentModification> envCallbacks;
  return envCallbacks;
}

struct InsensitiveComparison
{
  bool operator()(const std::wstring &a, const std::wstring &b) const
  {
    return strlower(a) < strlower(b);
  }
};

typedef std::map<std::wstring, std::string, InsensitiveComparison> EnvMap;

static EnvMap EnvStringToEnvMap(const wchar_t *envstring)
{
  EnvMap ret;

  const wchar_t *e = envstring;

  while(*e)
  {
    const wchar_t *equals = wcschr(e, L'=');

    std::wstring name;
    std::wstring value;

    name.assign(e, equals);
    value = equals + 1;

    ret[name] = StringFormat::Wide2UTF8(value);

    e += name.size();     // jump to =
    e++;                  // advance past it
    e += value.size();    // jump to \0
    e++;                  // advance past it
  }

  return ret;
}

void Process::RegisterEnvironmentModification(EnvironmentModification modif)
{
  GetEnvModifications().push_back(modif);
}

static void ApplyEnvModifications(EnvMap &envValues,
                                  const std::vector<EnvironmentModification> &modifications,
                                  bool setToSystem)
{
  for(size_t i = 0; i < modifications.size(); i++)
  {
    const EnvironmentModification &m = modifications[i];

    std::wstring name = StringFormat::UTF82Wide(m.name.c_str());

    std::string value;

    auto it = envValues.find(name);
    if(it != envValues.end())
      value = it->second;

    switch(m.mod)
    {
      case EnvMod::Set: value = m.value.c_str(); break;
      case EnvMod::Append:
      {
        if(!value.empty())
        {
          if(m.sep == EnvSep::Platform || m.sep == EnvSep::SemiColon)
            value += ";";
          else if(m.sep == EnvSep::Colon)
            value += ":";
        }
        value += m.value.c_str();
        break;
      }
      case EnvMod::Prepend:
      {
        if(!value.empty())
        {
          std::string prep = m.value;
          if(m.sep == EnvSep::Platform || m.sep == EnvSep::SemiColon)
            prep += ";";
          else if(m.sep == EnvSep::Colon)
            prep += ":";
          value = prep + value;
        }
        else
        {
          value = m.value.c_str();
        }
        break;
      }
    }

    envValues[name] = value;

    if(setToSystem)
      SetEnvironmentVariableW(name.c_str(), StringFormat::UTF82Wide(value).c_str());
  }
}

// on windows we apply environment changes here, after process initialisation
// but before any real work (in RenderDoc::Initialise) so that we support
// injecting the dll into processes we didn't launch (ie didn't control the
// starting environment for), or even the application loading the dll itself
// without any interaction with our replay app.
void Process::ApplyEnvironmentModification()
{
  // turn environment string to a UTF-8 map
  LPWCH envStrings = GetEnvironmentStringsW();
  EnvMap envValues = EnvStringToEnvMap(envStrings);
  FreeEnvironmentStringsW(envStrings);
  std::vector<EnvironmentModification> &modifications = GetEnvModifications();

  ApplyEnvModifications(envValues, modifications, true);

  // these have been applied to the current process
  modifications.clear();
}

const char *Process::GetEnvVariable(const char *name)
{
  DWORD ret = GetEnvironmentVariableA(name, NULL, 0);
  if(ret == 0 && GetLastError() == ERROR_ENVVAR_NOT_FOUND)
    return NULL;

  static char buf[1024] = {};
  if(ret >= 1024)
    RDCERR("Static buffer insufficiently sized");

  RDCEraseEl(buf);
  GetEnvironmentVariableA(name, buf, RDCMIN((DWORD)1023U, ret));
  return buf;
}

uint64_t Process::GetMemoryUsage()
{
  HANDLE proc = GetCurrentProcess();

  if(proc == NULL)
  {
    RDCERR("Couldn't open process: %d", GetLastError());
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
    RDCERR("Couldn't get process memory info: %d", GetLastError());
  }

  return ret;
}

// helpers for various shims and dlls etc, not part of the public API
extern "C" __declspec(dllexport) void __cdecl INTERNAL_GetTargetControlIdent(uint32_t *ident)
{
  if(ident)
    *ident = RenderDoc::Inst().GetTargetControlIdent();
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_SetCaptureOptions(CaptureOptions *opts)
{
  if(opts)
    RenderDoc::Inst().SetCaptureOptions(*opts);
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_SetCaptureFile(const char *capfile)
{
  if(capfile)
    RenderDoc::Inst().SetCaptureFileTemplate(capfile);
}

static EnvironmentModification tempEnvMod;

extern "C" __declspec(dllexport) void __cdecl INTERNAL_EnvModName(const char *name)
{
  if(name)
    tempEnvMod.name = name;
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_EnvModValue(const char *value)
{
  if(value)
    tempEnvMod.value = value;
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_EnvSep(EnvSep *sep)
{
  if(sep)
    tempEnvMod.sep = *sep;
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_EnvMod(EnvMod *mod)
{
  if(mod)
  {
    tempEnvMod.mod = *mod;
    Process::RegisterEnvironmentModification(tempEnvMod);
  }
}

extern "C" __declspec(dllexport) void __cdecl INTERNAL_ApplyEnvMods(void *ignored)
{
  Process::ApplyEnvironmentModification();
}

void InjectDLL(HANDLE hProcess, std::wstring libName)
{
  wchar_t dllPath[MAX_PATH + 1] = {0};
  wcscpy_s(dllPath, libName.c_str());

  static HMODULE kernel32 = GetModuleHandleA("kernel32.dll");

  if(kernel32 == NULL)
  {
    RDCERR("Couldn't get handle for kernel32.dll");
    return;
  }

  void *remoteMem =
      VirtualAllocEx(hProcess, NULL, sizeof(dllPath), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  if(remoteMem)
  {
    BOOL success = WriteProcessMemory(hProcess, remoteMem, (void *)dllPath, sizeof(dllPath), NULL);
    if(success)
    {
      HANDLE hThread = CreateRemoteThread(
          hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(kernel32, "LoadLibraryW"),
          remoteMem, 0, NULL);
      if(hThread)
      {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
      }
      else
      {
        RDCERR("Couldn't create remote thread for LoadLibraryW: %u", GetLastError());
      }
    }
    else
    {
      RDCERR("Couldn't write remote memory %p with dllPath '%ls': %u", remoteMem, dllPath,
             GetLastError());
    }

    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
  }
  else
  {
    RDCERR("Couldn't allocate remote memory for DLL '%ls': %u", libName.c_str(), GetLastError());
  }
}

uintptr_t FindRemoteDLL(DWORD pid, std::wstring libName)
{
  HANDLE hModuleSnap = INVALID_HANDLE_VALUE;

  libName = strlower(libName);

  // up to 10 retries
  for(int i = 0; i < 10; i++)
  {
    hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);

    if(hModuleSnap == INVALID_HANDLE_VALUE)
    {
      DWORD err = GetLastError();

      RDCWARN("CreateToolhelp32Snapshot(%u) -> 0x%08x", pid, err);

      // retry if error is ERROR_BAD_LENGTH
      if(err == ERROR_BAD_LENGTH)
        continue;
    }

    // didn't retry, or succeeded
    break;
  }

  if(hModuleSnap == INVALID_HANDLE_VALUE)
  {
    RDCERR("Couldn't create toolhelp dump of modules in process %u", pid);
    return 0;
  }

  MODULEENTRY32 me32;
  RDCEraseEl(me32);
  me32.dwSize = sizeof(MODULEENTRY32);

  BOOL success = Module32First(hModuleSnap, &me32);

  if(success == FALSE)
  {
    DWORD err = GetLastError();

    RDCERR("Couldn't get first module in process %u: 0x%08x", pid, err);
    CloseHandle(hModuleSnap);
    return 0;
  }

  uintptr_t ret = 0;

  int numModules = 0;

  do
  {
    wchar_t modnameLower[MAX_MODULE_NAME32 + 1];
    RDCEraseEl(modnameLower);
    wcsncpy_s(modnameLower, me32.szModule, MAX_MODULE_NAME32);

    wchar_t *wc = &modnameLower[0];
    while(*wc)
    {
      *wc = towlower(*wc);
      wc++;
    }

    numModules++;

    if(wcsstr(modnameLower, libName.c_str()) == modnameLower)
    {
      ret = (uintptr_t)me32.modBaseAddr;
    }
  } while(ret == 0 && Module32Next(hModuleSnap, &me32));

  if(ret == 0)
  {
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);

    DWORD exitCode = 0;

    if(h)
      GetExitCodeProcess(h, &exitCode);

    if(h == NULL || exitCode != STILL_ACTIVE)
    {
      RDCERR(
          "Error injecting into remote process with PID %u which is no longer available.\n"
          "Possibly the process has crashed during early startup, or is missing DLLs to run?",
          pid);
    }
    else
    {
      RDCERR("Couldn't find module '%ls' among %d modules", libName.c_str(), numModules);
    }

    if(h)
      CloseHandle(h);
  }

  CloseHandle(hModuleSnap);

  return ret;
}

void InjectFunctionCall(HANDLE hProcess, uintptr_t renderdoc_remote, const char *funcName,
                        void *data, const size_t dataLen)
{
  if(dataLen == 0)
  {
    RDCERR("Invalid function call injection attempt");
    return;
  }

  RDCDEBUG("Injecting call to %s", funcName);

  HMODULE renderdoc_local = GetModuleHandleA(STRINGIZE(RDOC_DLL_FILE) ".dll");

  uintptr_t func_local = (uintptr_t)GetProcAddress(renderdoc_local, funcName);

  // we've found SetCaptureOptions in our local instance of the module, now calculate the offset and
  // so get the function
  // in the remote module (which might be loaded at a different base address
  uintptr_t func_remote = func_local + renderdoc_remote - (uintptr_t)renderdoc_local;

  void *remoteMem = VirtualAllocEx(hProcess, NULL, dataLen, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
  SIZE_T numWritten;
  WriteProcessMemory(hProcess, remoteMem, data, dataLen, &numWritten);

  HANDLE hThread =
      CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)func_remote, remoteMem, 0, NULL);
  WaitForSingleObject(hThread, INFINITE);

  ReadProcessMemory(hProcess, remoteMem, data, dataLen, &numWritten);

  CloseHandle(hThread);
  VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
}

static PROCESS_INFORMATION RunProcess(const char *app, const char *workingDir, const char *cmdLine,
                                      const rdcarray<EnvironmentModification> &env, bool internal,
                                      HANDLE *phChildStdOutput_Rd, HANDLE *phChildStdError_Rd)
{
  PROCESS_INFORMATION pi;
  STARTUPINFO si;
  SECURITY_ATTRIBUTES pSec;
  SECURITY_ATTRIBUTES tSec;

  RDCEraseEl(pi);
  RDCEraseEl(si);
  RDCEraseEl(pSec);
  RDCEraseEl(tSec);

  pSec.nLength = sizeof(pSec);
  tSec.nLength = sizeof(tSec);

  std::wstring workdir = L"";

  if(workingDir != NULL && workingDir[0] != 0)
    workdir = StringFormat::UTF82Wide(std::string(workingDir));
  else
    workdir = StringFormat::UTF82Wide(get_dirname(std::string(app)));

  wchar_t *paramsAlloc = NULL;

  std::wstring wapp = StringFormat::UTF82Wide(std::string(app));

  // CreateProcessW can modify the params, need space.
  size_t len = wapp.length() + 10;

  std::wstring wcmd = L"";

  if(cmdLine != NULL && cmdLine[0] != 0)
  {
    wcmd = StringFormat::UTF82Wide(std::string(cmdLine));
    len += wcmd.length();
  }

  paramsAlloc = new wchar_t[len];

  RDCEraseMem(paramsAlloc, len * sizeof(wchar_t));

  wcscpy_s(paramsAlloc, len, L"\"");
  wcscat_s(paramsAlloc, len, wapp.c_str());
  wcscat_s(paramsAlloc, len, L"\"");

  if(cmdLine != NULL && cmdLine[0] != 0)
  {
    wcscat_s(paramsAlloc, len, L" ");
    wcscat_s(paramsAlloc, len, wcmd.c_str());
  }

  HANDLE hChildStdOutput_Wr = 0, hChildStdError_Wr = 0;
  if(phChildStdOutput_Rd)
  {
    RDCASSERT(phChildStdError_Rd);

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    if(!CreatePipe(phChildStdOutput_Rd, &hChildStdOutput_Wr, &sa, 0))
      RDCERR("Could not create pipe to read stdout");
    if(!SetHandleInformation(*phChildStdOutput_Rd, HANDLE_FLAG_INHERIT, 0))
      RDCERR("Could not set pipe handle information");

    if(!CreatePipe(phChildStdError_Rd, &hChildStdError_Wr, &sa, 0))
      RDCERR("Could not create pipe to read stdout");
    if(!SetHandleInformation(*phChildStdError_Rd, HANDLE_FLAG_INHERIT, 0))
      RDCERR("Could not set pipe handle information");

    si.dwFlags |= STARTF_USESTDHANDLES;
    si.hStdOutput = hChildStdOutput_Wr;
    si.hStdError = hChildStdError_Wr;
  }

  // if it's a utility launch, hide the command prompt window from showing
  if(phChildStdOutput_Rd || internal)
    si.dwFlags |= STARTF_USESHOWWINDOW;

  if(!internal)
    RDCLOG("Running process %s", app);

  // turn environment string to a UTF-8 map
  std::wstring envString;

  if(!env.empty())
  {
    LPWCH envStrings = GetEnvironmentStringsW();
    EnvMap envValues = EnvStringToEnvMap(envStrings);
    FreeEnvironmentStringsW(envStrings);

    std::vector<EnvironmentModification> envMods;
    envMods.insert(envMods.begin(), env.begin(), env.end());
    ApplyEnvModifications(envValues, envMods, false);

    for(auto it = envValues.begin(); it != envValues.end(); ++it)
    {
      envString += it->first;
      envString += L"=";
      envString += StringFormat::UTF82Wide(it->second);
      envString.push_back(0);
    }
  }

  BOOL retValue = CreateProcessW(NULL, paramsAlloc, &pSec, &tSec,
                                 true,    // Need to inherit handles for ReadFile to read stdout
                                 CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                                 envString.empty() ? NULL : (void *)envString.data(),
                                 workdir.c_str(), &si, &pi);

  if(phChildStdOutput_Rd)
  {
    CloseHandle(hChildStdOutput_Wr);
    CloseHandle(hChildStdError_Wr);
  }

  SAFE_DELETE_ARRAY(paramsAlloc);

  if(!retValue)
  {
    if(!internal)
      RDCWARN("Process %s could not be loaded.", app);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    RDCEraseEl(pi);
  }

  return pi;
}

ExecuteResult Process::InjectIntoProcess(uint32_t pid, const rdcarray<EnvironmentModification> &env,
                                         const char *capturefile, const CaptureOptions &opts,
                                         bool waitForExit)
{
  std::wstring wcapturefile = capturefile == NULL ? L"" : StringFormat::UTF82Wide(capturefile);

  HANDLE hProcess =
      OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                      PROCESS_VM_WRITE | PROCESS_VM_READ | SYNCHRONIZE,
                  FALSE, pid);

  if(opts.delayForDebugger > 0)
  {
    RDCDEBUG("Waiting for debugger attach to %lu", pid);
    uint32_t timeout = 0;

    BOOL debuggerAttached = FALSE;

    while(!debuggerAttached)
    {
      CheckRemoteDebuggerPresent(hProcess, &debuggerAttached);

      Sleep(10);
      timeout += 10;

      if(timeout > opts.delayForDebugger * 1000)
        break;
    }

    if(debuggerAttached)
      RDCDEBUG("Debugger attach detected after %.2f s", float(timeout) / 1000.0f);
    else
      RDCDEBUG("Timed out waiting for debugger, gave up after %u s", opts.delayForDebugger);
  }

  RDCLOG("Injecting renderdoc into process %lu", pid);

  wchar_t renderdocPath[MAX_PATH] = {0};
  GetModuleFileNameW(GetModuleHandleA(STRINGIZE(RDOC_DLL_FILE) ".dll"), &renderdocPath[0],
                     MAX_PATH - 1);

  wchar_t renderdocPathLower[MAX_PATH] = {0};
  memcpy(renderdocPathLower, renderdocPath, MAX_PATH * sizeof(wchar_t));
  for(size_t i = 0; i < MAX_PATH && renderdocPathLower[i]; i++)
  {
    // lowercase
    if(renderdocPathLower[i] >= 'A' && renderdocPathLower[i] <= 'Z')
      renderdocPathLower[i] = 'a' + char(renderdocPathLower[i] - 'A');

    // normalise paths
    if(renderdocPathLower[i] == '/')
      renderdocPathLower[i] = '\\';
  }

  BOOL isWow64 = FALSE;
  BOOL success = IsWow64Process(hProcess, &isWow64);

  if(!success)
  {
    DWORD err = GetLastError();
    RDCERR("Couldn't determine bitness of process, err: %08x", err);
    CloseHandle(hProcess);
    return {ReplayStatus::IncompatibleProcess, 0};
  }

  bool capalt = false;

#if DISABLED(RDOC_X64)
  BOOL selfWow64 = FALSE;

  HANDLE hSelfProcess = GetCurrentProcess();

  // check to see if we're a WoW64 process
  success = IsWow64Process(hSelfProcess, &selfWow64);

  CloseHandle(hSelfProcess);

  if(!success)
  {
    DWORD err = GetLastError();
    RDCERR("Couldn't determine bitness of self, err: %08x", err);
    return {ReplayStatus::InternalError, 0};
  }

  // we know we're 32-bit, so if the target process is not wow64
  // and we are, it's 64-bit. If we're both not wow64 then we're
  // running on 32-bit windows, and if we're both wow64 then we're
  // both 32-bit on 64-bit windows.
  //
  // We don't support capturing 64-bit programs from a 32-bit install
  // because it's pointless - a 64-bit install will work for all in
  // that case. But we do want to handle the case of:
  // 64-bit renderdoc -> 32-bit program (via 32-bit renderdoccmd)
  //    -> 64-bit program (going back to 64-bit renderdoccmd).
  // so we try to see if we're an x86 invoked renderdoccmd in an
  // otherwise 64-bit install, and 'promote' back to 64-bit.
  if(selfWow64 && !isWow64)
  {
    wchar_t *slash = wcsrchr(renderdocPath, L'\\');

    if(slash && slash > renderdocPath + 4)
    {
      slash -= 4;

      if(slash && !wcsncmp(slash, L"\\x86", 4))
      {
        RDCDEBUG("Promoting back to 64-bit");
        capalt = true;
      }
    }

    // if it looks like we're in the development environment, look for the alternate bitness in the
    // corresponding folder
    if(!capalt)
    {
      const wchar_t *devLocation = wcsstr(renderdocPathLower, L"\\win32\\development\\");
      if(!devLocation)
        devLocation = wcsstr(renderdocPathLower, L"\\win32\\release\\");

      if(devLocation)
      {
        RDCDEBUG("Promoting back to 64-bit");
        capalt = true;
      }
    }

    // if we couldn't promote, then bail out.
    if(!capalt)
    {
      RDCDEBUG("Running from %ls", renderdocPathLower);
      RDCERR("Can't capture x64 process with x86 renderdoc");

      CloseHandle(hProcess);
      return {ReplayStatus::IncompatibleProcess, 0};
    }
  }
#else
  // farm off to alternate bitness renderdoccmd.exe

  // if the target process is 'wow64' that means it's 32-bit.
  capalt = (isWow64 == TRUE);
#endif

  if(capalt)
  {
#if ENABLED(RDOC_X64)
    // if it looks like we're in the development environment, look for the alternate bitness in the
    // corresponding folder
    const wchar_t *devLocation = wcsstr(renderdocPathLower, L"\\x64\\development\\");
    if(devLocation)
    {
      size_t idx = devLocation - renderdocPathLower;

      renderdocPath[idx] = 0;

      wcscat_s(renderdocPath, L"\\Win32\\Development\\renderdoccmd.exe");
    }

    if(!devLocation)
    {
      devLocation = wcsstr(renderdocPathLower, L"\\x64\\release\\");

      if(devLocation)
      {
        size_t idx = devLocation - renderdocPathLower;

        renderdocPath[idx] = 0;

        wcscat_s(renderdocPath, L"\\Win32\\Release\\renderdoccmd.exe");
      }
    }

    if(!devLocation)
    {
      // look in a subfolder for x86.

      // remove the filename from the path
      wchar_t *slash = wcsrchr(renderdocPath, L'\\');

      if(slash)
        *slash = 0;

      // append path
      wcscat_s(renderdocPath, L"\\x86\\renderdoccmd.exe");
    }
#else
    // if it looks like we're in the development environment, look for the alternate bitness in the
    // corresponding folder
    const wchar_t *devLocation = wcsstr(renderdocPathLower, L"\\win32\\development\\");
    if(devLocation)
    {
      size_t idx = devLocation - renderdocPathLower;

      renderdocPath[idx] = 0;

      wcscat_s(renderdocPath, L"\\x64\\Development\\renderdoccmd.exe");
    }

    if(!devLocation)
    {
      devLocation = wcsstr(renderdocPathLower, L"\\win32\\release\\");

      if(devLocation)
      {
        size_t idx = devLocation - renderdocPathLower;

        renderdocPath[idx] = 0;

        wcscat_s(renderdocPath, L"\\x64\\Release\\renderdoccmd.exe");
      }
    }

    if(!devLocation)
    {
      // look upwards on 32-bit to find the parent renderdoccmd.
      wchar_t *slash = wcsrchr(renderdocPath, L'\\');

      // remove the filename
      if(slash)
        *slash = 0;

      // remove the \\x86
      slash = wcsrchr(renderdocPath, L'\\');

      if(slash)
        *slash = 0;

      // append path
      wcscat_s(renderdocPath, L"\\renderdoccmd.exe");
    }
#endif

    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    SECURITY_ATTRIBUTES pSec;
    SECURITY_ATTRIBUTES tSec;

    RDCEraseEl(pi);
    RDCEraseEl(si);
    RDCEraseEl(pSec);
    RDCEraseEl(tSec);

    // hide the console window
    si.cb = sizeof(si);
    si.dwFlags |= STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    pSec.nLength = sizeof(pSec);
    tSec.nLength = sizeof(tSec);

    // serialise to string with two chars per byte
    std::string optstr = opts.EncodeAsString();

    wchar_t *paramsAlloc = new wchar_t[2048];

    std::string debugLogfile = RDCGETLOGFILE();
    std::wstring wdebugLogfile = StringFormat::UTF82Wide(debugLogfile);

    _snwprintf_s(
        paramsAlloc, 2047, 2047,
        L"\"%ls\" capaltbit --pid=%d --capfile=\"%ls\" --debuglog=\"%ls\" --capopts=\"%hs\"",
        renderdocPath, pid, wcapturefile.c_str(), wdebugLogfile.c_str(), optstr.c_str());

    RDCDEBUG("params %ls", paramsAlloc);

    paramsAlloc[2047] = 0;

    wchar_t *commandLine = paramsAlloc;

    std::wstring cmdWithEnv;

    if(!env.empty())
    {
      cmdWithEnv = paramsAlloc;

      for(const EnvironmentModification &e : env)
      {
        std::string name = trim(e.name.c_str());
        std::string value = e.value.c_str();

        if(name == "")
          break;

        cmdWithEnv += L" +env-";
        switch(e.mod)
        {
          case EnvMod::Set: cmdWithEnv += L"replace"; break;
          case EnvMod::Append: cmdWithEnv += L"append"; break;
          case EnvMod::Prepend: cmdWithEnv += L"prepend"; break;
        }

        if(e.mod != EnvMod::Set)
        {
          switch(e.sep)
          {
            case EnvSep::Platform: cmdWithEnv += L"-platform"; break;
            case EnvSep::SemiColon: cmdWithEnv += L"-semicolon"; break;
            case EnvSep::Colon: cmdWithEnv += L"-colon"; break;
          }
        }

        cmdWithEnv += L" ";

        // escape the parameters
        for(auto it = name.begin(); it != name.end(); ++it)
        {
          if(*it == '"')
            it = name.insert(it, '\\') + 1;
        }

        for(auto it = value.begin(); it != value.end(); ++it)
        {
          if(*it == '"')
            it = value.insert(it, '\\') + 1;
        }

        if(name.back() == '\\')
          name += "\\";

        if(value.back() == '\\')
          value += "\\";

        cmdWithEnv += L"\"" + StringFormat::UTF82Wide(name) + L"\" ";
        cmdWithEnv += L"\"" + StringFormat::UTF82Wide(value) + L"\" ";
      }

      commandLine = (wchar_t *)cmdWithEnv.c_str();
    }

    BOOL retValue = CreateProcessW(NULL, commandLine, &pSec, &tSec, false,
                                   CREATE_NEW_CONSOLE | CREATE_SUSPENDED, NULL, NULL, &si, &pi);

    SAFE_DELETE_ARRAY(paramsAlloc);

    if(!retValue)
    {
      RDCERR(
          "Can't spawn alternate bitness renderdoccmd - have you built 32-bit and 64-bit?\n"
          "You need to build the matching bitness for the programs you want to capture.");
      return {ReplayStatus::InternalError, 0};
    }

    ResumeThread(pi.hThread);
    WaitForSingleObject(pi.hThread, INFINITE);
    CloseHandle(pi.hThread);

    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);

    if(waitForExit)
      WaitForSingleObject(hProcess, INFINITE);

    CloseHandle(hProcess);

    if(exitCode == 0)
      return {ReplayStatus::UnknownError, 0};
    if(exitCode < RenderDoc_FirstTargetControlPort)
      return {(ReplayStatus)exitCode, 0};

    return {ReplayStatus::Succeeded, (uint32_t)exitCode};
  }

  InjectDLL(hProcess, renderdocPath);

  uintptr_t loc = FindRemoteDLL(pid, CONCAT(L, STRINGIZE(RDOC_DLL_FILE)) L".dll");

  ExecuteResult result = {ReplayStatus::Succeeded, 0};

  if(loc == 0)
  {
    RDCERR("Can't locate " STRINGIZE(RDOC_DLL_FILE) ".dll in remote PID %d", pid);
    result.status = ReplayStatus::InjectionFailed;
  }
  else
  {
    // safe to cast away the const as we know these functions don't modify the parameters

    if(capturefile != NULL)
      InjectFunctionCall(hProcess, loc, "INTERNAL_SetCaptureFile", (void *)capturefile,
                         strlen(capturefile) + 1);

    std::string debugLogfile = RDCGETLOGFILE();

    InjectFunctionCall(hProcess, loc, "RENDERDOC_SetDebugLogFile", (void *)debugLogfile.c_str(),
                       debugLogfile.size() + 1);

    InjectFunctionCall(hProcess, loc, "INTERNAL_SetCaptureOptions", (CaptureOptions *)&opts,
                       sizeof(CaptureOptions));

    InjectFunctionCall(hProcess, loc, "INTERNAL_GetTargetControlIdent", &result.ident,
                       sizeof(result.ident));

    if(!env.empty())
    {
      for(const EnvironmentModification &e : env)
      {
        std::string name = trim(e.name.c_str());
        std::string value = e.value.c_str();
        EnvMod mod = e.mod;
        EnvSep sep = e.sep;

        if(name == "")
          break;

        InjectFunctionCall(hProcess, loc, "INTERNAL_EnvModName", (void *)name.c_str(),
                           name.size() + 1);
        InjectFunctionCall(hProcess, loc, "INTERNAL_EnvModValue", (void *)value.c_str(),
                           value.size() + 1);
        InjectFunctionCall(hProcess, loc, "INTERNAL_EnvSep", &sep, sizeof(sep));
        InjectFunctionCall(hProcess, loc, "INTERNAL_EnvMod", &mod, sizeof(mod));
      }

      // parameter is unused
      void *dummy = NULL;
      InjectFunctionCall(hProcess, loc, "INTERNAL_ApplyEnvMods", &dummy, sizeof(dummy));
    }
  }

  if(waitForExit)
    WaitForSingleObject(hProcess, INFINITE);

  CloseHandle(hProcess);

  return result;
}

uint32_t Process::LaunchProcess(const char *app, const char *workingDir, const char *cmdLine,
                                bool internal, ProcessResult *result)
{
  HANDLE hChildStdOutput_Rd = NULL, hChildStdError_Rd = NULL;

  PROCESS_INFORMATION pi =
      RunProcess(app, workingDir, cmdLine, {}, internal, result ? &hChildStdOutput_Rd : NULL,
                 result ? &hChildStdError_Rd : NULL);

  if(pi.dwProcessId == 0)
  {
    if(!internal)
      RDCWARN("Couldn't launch process '%s'", app);
    return 0;
  }

  if(!internal)
    RDCLOG("Launched process '%s' with '%s'", app, cmdLine);

  ResumeThread(pi.hThread);

  if(result)
  {
    result->strStdout = "";
    result->strStderror = "";

    char chBuf[4096];
    DWORD dwOutputRead, dwErrorRead;
    BOOL success = FALSE;
    std::string s;
    for(;;)
    {
      success = ReadFile(hChildStdOutput_Rd, chBuf, sizeof(chBuf), &dwOutputRead, NULL);
      s = std::string(chBuf, dwOutputRead);
      result->strStdout += s;

      if(!success && !dwOutputRead)
        break;
    }

    for(;;)
    {
      success = ReadFile(hChildStdError_Rd, chBuf, sizeof(chBuf), &dwErrorRead, NULL);
      s = std::string(chBuf, dwErrorRead);
      result->strStderror += s;

      if(!success && !dwErrorRead)
        break;
    }

    CloseHandle(hChildStdOutput_Rd);
    CloseHandle(hChildStdError_Rd);

    WaitForSingleObject(pi.hProcess, INFINITE);
    GetExitCodeProcess(pi.hProcess, (LPDWORD)&result->retCode);
  }

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

  return pi.dwProcessId;
}

uint32_t Process::LaunchScript(const char *script, const char *workingDir, const char *argList,
                               bool internal, ProcessResult *result)
{
  // Change parameters to invoke command interpreter
  std::string args = "/C " + std::string(script) + " " + std::string(argList);

  return LaunchProcess("cmd.exe", workingDir, args.c_str(), internal, result);
}

ExecuteResult Process::LaunchAndInjectIntoProcess(const char *app, const char *workingDir,
                                                  const char *cmdLine,
                                                  const rdcarray<EnvironmentModification> &env,
                                                  const char *capturefile,
                                                  const CaptureOptions &opts, bool waitForExit)
{
  void *func =
      GetProcAddress(GetModuleHandleA(STRINGIZE(RDOC_DLL_FILE) ".dll"), "INTERNAL_SetCaptureFile");

  if(func == NULL)
  {
    RDCERR("Can't find required export function in " STRINGIZE(
        RDOC_DLL_FILE) ".dll - corrupted/missing file?");
    return {ReplayStatus::InternalError, 0};
  }

  PROCESS_INFORMATION pi = RunProcess(app, workingDir, cmdLine, env, false, NULL, NULL);

  if(pi.dwProcessId == 0)
    return {ReplayStatus::InjectionFailed, 0};

  ExecuteResult ret = InjectIntoProcess(pi.dwProcessId, {}, capturefile, opts, false);

  CloseHandle(pi.hProcess);
  ResumeThread(pi.hThread);
  ResumeThread(pi.hThread);

  if(ret.ident == 0 || ret.status != ReplayStatus::Succeeded)
  {
    CloseHandle(pi.hThread);
    return ret;
  }

  if(waitForExit)
    WaitForSingleObject(pi.hThread, INFINITE);

  CloseHandle(pi.hThread);

  return ret;
}

bool Process::CanGlobalHook()
{
  // all we need is admin rights and it's the caller's responsibility to ensure that.
  return true;
}

// to simplify the below code, rather than splitting by 32-bit/64-bit we split by native and Wow32.
// This means that for 32-bit code (whether it's on 32-bit OS or not) we just have native, and the
// Wow32 stuff is empty/unused. For 64-bit we use both. Thus the native registry key is always the
// same path regardless of the bitness we're running as and we don't have to move things around or
// have conditionals all over

struct GlobalHookData
{
  struct
  {
    HANDLE pipe = NULL;
    DWORD appinitEnabled = 0;
    std::wstring appinitDLLs;
  } dataNative, dataWow32;

  volatile int32_t finished = 0;
  Threading::ThreadHandle pipeThread = 0;
};

// utility function to close the registry keys, print an error, and quit
static bool HandleRegError(HKEY keyNative, HKEY keyWow32, LSTATUS ret, const char *msg)
{
  if(keyNative)
    RegCloseKey(keyNative);

  if(keyWow32)
    RegCloseKey(keyWow32);

  RDCERR("Error with AppInit registry keys - %s (%d)", msg, ret);
  return false;
}

#define REG_CHECK(msg)                                    \
  if(ret != ERROR_SUCCESS)                                \
  {                                                       \
    return HandleRegError(keyNative, keyWow32, ret, msg); \
  }

// function to backup the previous settings for AppInit, then enable it and write our own paths.
bool BackupAndChangeRegistry(GlobalHookData &hookdata, const std::wstring &shimpathWow32,
                             const std::wstring &shimpathNative)
{
  HKEY keyNative = NULL;
  HKEY keyWow32 = NULL;

  // open the native key
  LSTATUS ret = RegCreateKeyExA(HKEY_LOCAL_MACHINE,
                                "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows", 0, NULL,
                                0, KEY_READ | KEY_WRITE, NULL, &keyNative, NULL);

  REG_CHECK("Could not open AppInit key");

  // if we are doing Wow32, open that key as well
  if(!shimpathWow32.empty())
  {
    ret = RegCreateKeyExA(HKEY_LOCAL_MACHINE,
                          "SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Windows",
                          0, NULL, 0, KEY_READ | KEY_WRITE, NULL, &keyWow32, NULL);

    REG_CHECK("Could not open AppInit key");
  }

  const DWORD one = 1;

  // fetch the previous data for LoadAppInit_DLLs and AppInit_DLLs
  DWORD sz = 4;
  ret = RegGetValueA(keyNative, NULL, "LoadAppInit_DLLs", RRF_RT_REG_DWORD, NULL,
                     (void *)&hookdata.dataNative.appinitEnabled, &sz);
  REG_CHECK("Could not fetch LoadAppInit_DLLs");

  sz = 0;
  ret = RegGetValueW(keyNative, NULL, L"AppInit_DLLs", RRF_RT_ANY, NULL, NULL, &sz);
  if(ret == ERROR_MORE_DATA || ret == ERROR_SUCCESS)
  {
    hookdata.dataNative.appinitDLLs.resize(sz / sizeof(wchar_t));
    ret = RegGetValueW(keyNative, NULL, L"AppInit_DLLs", RRF_RT_ANY, NULL,
                       (void *)&hookdata.dataNative.appinitDLLs[0], &sz);
  }
  REG_CHECK("Could not fetch AppInit_DLLs");

  // set DWORD:1 for LoadAppInit_DLLs and convert our path to a short path then set it
  ret = RegSetValueExA(keyNative, "LoadAppInit_DLLs", 0, REG_DWORD, (const BYTE *)&one, sizeof(one));
  REG_CHECK("Could not set LoadAppInit_DLLs");

  std::wstring shortpath;
  shortpath = shimpathNative;
  GetShortPathNameW(shimpathNative.c_str(), (wchar_t *)&shortpath[0], (DWORD)shortpath.size());

  ret = RegSetValueExW(keyNative, L"AppInit_DLLs", 0, REG_SZ, (const BYTE *)shortpath.data(),
                       DWORD(shortpath.size() * sizeof(wchar_t)));
  REG_CHECK("Could not set AppInit_DLLs");

  // if we're doing Wow32, repeat the process for those keys
  if(keyWow32)
  {
    sz = 4;
    ret = RegGetValueA(keyWow32, NULL, "LoadAppInit_DLLs", RRF_RT_REG_DWORD, NULL,
                       (void *)&hookdata.dataWow32.appinitEnabled, &sz);
    REG_CHECK("Could not fetch LoadAppInit_DLLs");

    sz = 0;
    ret = RegGetValueW(keyWow32, NULL, L"AppInit_DLLs", RRF_RT_ANY, NULL, NULL, &sz);
    if(ret == ERROR_MORE_DATA || ret == ERROR_SUCCESS)
    {
      hookdata.dataWow32.appinitDLLs.resize(sz / sizeof(wchar_t));
      ret = RegGetValueW(keyWow32, NULL, L"AppInit_DLLs", RRF_RT_ANY, NULL,
                         (void *)&hookdata.dataWow32.appinitDLLs[0], &sz);
    }
    REG_CHECK("Could not fetch AppInit_DLLs");

    ret = RegSetValueExA(keyWow32, "LoadAppInit_DLLs", 0, REG_DWORD, (const BYTE *)&one, sizeof(one));
    REG_CHECK("Could not set LoadAppInit_DLLs");

    shortpath = shimpathWow32;
    GetShortPathNameW(shimpathWow32.c_str(), &shortpath[0], (DWORD)shortpath.size());

    ret = RegSetValueExW(keyWow32, L"AppInit_DLLs", 0, REG_SZ, (const BYTE *)shortpath.data(),
                         DWORD(shortpath.size() * sizeof(wchar_t)));
    REG_CHECK("Could not set AppInit_DLLs");
  }

  std::wstring backup;

  // write a .reg file that contains the previous settings, so that if all else fails the user can
  // manually insert it back into the registry to restore everything.
  backup += L"Windows Registry Editor Version 5.00\n";
  backup += L"\n";
  backup += L"[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows]\n";
  backup += L"\"LoadAppInit_DLLs\"=dword:0000000";
  backup += (hookdata.dataNative.appinitEnabled ? L"1\n" : L"0\n");
  backup += L"\"AppInit_DLLs\"=\"";
  // we append with the C string so we don't add trailing NULLs into the text.
  backup += hookdata.dataNative.appinitDLLs.c_str();
  backup += L"\"\n";
  if(keyWow32)
  {
    backup += L"\n";
    backup +=
        L"[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Microsoft\\"
        L"Windows NT\\CurrentVersion\\Windows]\n";
    backup += L"\"LoadAppInit_DLLs\"=dword:0000000";
    backup += (hookdata.dataWow32.appinitEnabled ? L"1\n" : L"0\n");
    backup += L"\"AppInit_DLLs\"=\"";
    backup += hookdata.dataWow32.appinitDLLs.c_str();
    backup += L"\"\n";
  }

  if(keyNative)
    RegCloseKey(keyNative);

  if(keyWow32)
    RegCloseKey(keyWow32);

  keyNative = keyWow32 = NULL;

  // write it to disk but don't fail if we can't, just print it to the log and keep going.
  wchar_t reg_backup[MAX_PATH];
  GetTempPathW(MAX_PATH, reg_backup);
  wcscat_s(reg_backup, L"RenderDoc_RestoreGlobalHook.reg");

  FILE *f = NULL;
  _wfopen_s(&f, reg_backup, L"w");
  if(f)
  {
    fputws(backup.c_str(), f);
    fclose(f);
  }
  else
  {
    RDCERR("Error opening registry backup file %ls", reg_backup);
    RDCERR("Backup registry data is:\n\n%ls\n\n", backup.c_str());
  }

  return true;
}

// switch error-handling to print-and-continue, as we can't really do anything about it at this
// point and we want to continue restoring in case only one thing failed.
#undef REG_CHECK
#define REG_CHECK(msg)                                                      \
  if(ret != ERROR_SUCCESS)                                                  \
  {                                                                         \
    HandleRegError(keyNative, keyWow32, ret, "Could not open AppInit key"); \
  }

void RestoreRegistry(const GlobalHookData &hookdata)
{
  HKEY keyNative = NULL;
  HKEY keyWow32 = NULL;
  LSTATUS ret = RegCreateKeyExA(HKEY_LOCAL_MACHINE,
                                "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows", 0, NULL,
                                0, KEY_READ | KEY_WRITE, NULL, &keyNative, NULL);

  REG_CHECK("Could not open AppInit key");

#if ENABLED(RDOC_X64)
  ret = RegCreateKeyExA(HKEY_LOCAL_MACHINE,
                        "SOFTWARE\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\Windows", 0,
                        NULL, 0, KEY_READ | KEY_WRITE, NULL, &keyWow32, NULL);

  REG_CHECK("Could not open AppInit key");
#endif

  // set the native values back to where they were
  ret = RegSetValueExA(keyNative, "LoadAppInit_DLLs", 0, REG_DWORD,
                       (const BYTE *)&hookdata.dataNative.appinitEnabled,
                       sizeof(hookdata.dataNative.appinitEnabled));
  REG_CHECK("Could not set LoadAppInit_DLLs");

  ret = RegSetValueExW(keyNative, L"AppInit_DLLs", 0, REG_SZ,
                       (const BYTE *)hookdata.dataNative.appinitDLLs.data(),
                       DWORD(hookdata.dataNative.appinitDLLs.size() * sizeof(wchar_t)));
  REG_CHECK("Could not set AppInit_DLLs");

  // if we opened it, restore the Wow32 values as well
  if(keyWow32)
  {
    ret = RegSetValueExA(keyWow32, "LoadAppInit_DLLs", 0, REG_DWORD,
                         (const BYTE *)&hookdata.dataWow32.appinitEnabled,
                         sizeof(hookdata.dataWow32.appinitEnabled));
    REG_CHECK("Could not set LoadAppInit_DLLs");

    ret = RegSetValueExW(keyWow32, L"AppInit_DLLs", 0, REG_SZ,
                         (const BYTE *)hookdata.dataWow32.appinitDLLs.data(),
                         DWORD(hookdata.dataWow32.appinitDLLs.size() * sizeof(wchar_t)));
    REG_CHECK("Could not set AppInit_DLLs");
  }
}

static GlobalHookData *globalHook = NULL;

// a thread we run in the background just to keep the pipes open and wait until we're ready to stop
// the global hook.
static void GlobalHookThread()
{
  // keep looping doing an atomic compare-exchange to check that finished is still 0
  while(Atomic::CmpExch32(&globalHook->finished, 0, 0) == 0)
  {
    // wake every quarter of a second to test again
    Threading::Sleep(250);
  }

  char exitData[32] = "exit";

  // write some data into the pipe and close it. The data is (currently) unimportant, just that it
  // causes the blocking read on the other end to succeed and close the program.
  DWORD dummy = 0;
  if(globalHook->dataNative.pipe)
  {
    WriteFile(globalHook->dataNative.pipe, exitData, (DWORD)sizeof(exitData), &dummy, NULL);
    CloseHandle(globalHook->dataNative.pipe);
  }

  if(globalHook->dataWow32.pipe)
  {
    WriteFile(globalHook->dataWow32.pipe, exitData, (DWORD)sizeof(exitData), &dummy, NULL);
    CloseHandle(globalHook->dataWow32.pipe);
  }
}

bool Process::StartGlobalHook(const char *pathmatch, const char *capturefile,
                              const CaptureOptions &opts)
{
  if(pathmatch == NULL)
    return false;

  wchar_t renderdocPath[MAX_PATH] = {0};
  GetModuleFileNameW(GetModuleHandleA(STRINGIZE(RDOC_DLL_FILE) ".dll"), &renderdocPath[0],
                     MAX_PATH - 1);

  wchar_t *slash = wcsrchr(renderdocPath, L'\\');

  if(slash)
    *slash = 0;
  else
    slash = renderdocPath + wcslen(renderdocPath);

  // the native renderdoccmd.exe is always next to the dll. Wow32 will be somewhere else
  std::wstring cmdpathNative = renderdocPath;
  cmdpathNative += L"\\renderdoccmd.exe";
  std::wstring cmdpathWow32;

  std::wstring shimpathNative = renderdocPath;
  std::wstring shimpathWow32;

#if ENABLED(RDOC_X64)

  // native shim is just renderdocshim64.dll
  *slash = 0;
  wcscat_s(renderdocPath, L"\\renderdocshim64.dll");
  shimpathNative = renderdocPath;

  // if it looks like we're in the development environment, look for the alternate bitness in the
  // corresponding folder
  const wchar_t *devLocation = wcsstr(renderdocPath, L"\\x64\\Development\\");
  if(devLocation)
  {
    size_t idx = devLocation - renderdocPath;

    renderdocPath[idx] = 0;

    shimpathWow32 = renderdocPath;
    shimpathWow32 += L"\\Win32\\Development\\renderdocshim32.dll";

    cmdpathWow32 = renderdocPath;
    cmdpathWow32 += L"\\Win32\\Development\\renderdoccmd.exe";
  }

  if(!devLocation)
  {
    devLocation = wcsstr(renderdocPath, L"\\x64\\Release\\");

    if(devLocation)
    {
      size_t idx = devLocation - renderdocPath;

      renderdocPath[idx] = 0;

      shimpathWow32 = renderdocPath;
      shimpathWow32 += L"\\Win32\\Release\\renderdocshim32.dll";

      cmdpathWow32 = renderdocPath;
      cmdpathWow32 += L"\\Win32\\Release\\renderdoccmd.exe";
    }
  }

  // if we're not in the dev environment, assume it's under a x86\ subfolder
  if(!devLocation)
  {
    *slash = 0;
    shimpathWow32 = renderdocPath;
    shimpathWow32 += L"\\x86\\renderdocshim32.dll";

    cmdpathWow32 = renderdocPath;
    cmdpathWow32 += L"\\x86\\renderdoccmd.exe";
  }

#else

  // nothing fancy to do here for 32-bit, just point the shim next to our dll.
  *slash = 0;
  wcscat_s(renderdocPath, L"\\renderdocshim32.dll");
  shimpathNative = renderdocPath;

#endif

  GlobalHookData hookdata;

  // try to backup and change the registry settings to start loading our shim dlls. If that fails,
  // we bail out immediately
  bool success = BackupAndChangeRegistry(hookdata, shimpathWow32, shimpathNative);
  if(!success)
    return false;

  PROCESS_INFORMATION pi = {0};
  STARTUPINFO si = {0};
  SECURITY_ATTRIBUTES pSec = {0};
  SECURITY_ATTRIBUTES tSec = {0};
  pSec.nLength = sizeof(pSec);
  tSec.nLength = sizeof(tSec);

  si.cb = sizeof(si);

  std::wstring paramsAlloc;
  paramsAlloc.resize(2048);

  // serialise to string with two chars per byte
  std::string optstr = opts.EncodeAsString();

  std::wstring wcapturefile =
      capturefile == NULL ? L"" : StringFormat::UTF82Wide(std::string(capturefile));
  std::wstring wpathmatch = StringFormat::UTF82Wide(std::string(pathmatch));

  std::string debugLogfile = RDCGETLOGFILE();
  std::wstring wdebugLogfile = StringFormat::UTF82Wide(debugLogfile);

  _snwprintf_s(&paramsAlloc[0], 2047, 2047,
               L"\"%ls\" globalhook --match \"%ls\" --capfile \"%ls\" --debuglog \"%ls\" "
               L"--capopts \"%hs\"",
               cmdpathNative.c_str(), wpathmatch.c_str(), wcapturefile.c_str(),
               wdebugLogfile.c_str(), optstr.c_str());

  paramsAlloc[2047] = 0;

  // we'll be setting stdin
  si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;

  // hide the console window
  si.wShowWindow = SW_HIDE;

  // this is the end of the pipe that the child will inherit and use as stdin
  HANDLE childEnd = NULL;

  // create a pipe with the writing end for us, and the reading end as the child process's stdin
  {
    SECURITY_ATTRIBUTES pipeSec;
    pipeSec.nLength = sizeof(SECURITY_ATTRIBUTES);
    pipeSec.bInheritHandle = TRUE;
    pipeSec.lpSecurityDescriptor = NULL;

    BOOL res;
    res = CreatePipe(&childEnd, &hookdata.dataNative.pipe, &pipeSec, 0);

    if(!res)
    {
      RDCERR("Could not create 32-bit stdin pipe");
      RestoreRegistry(hookdata);
      return false;
    }

    // we don't want the child process to inherit our end
    res = SetHandleInformation(hookdata.dataNative.pipe, HANDLE_FLAG_INHERIT, 0);

    if(!res)
    {
      RDCERR("Could not make 32-bit stdin pipe inheritable");
      RestoreRegistry(hookdata);
      return false;
    }

    si.hStdInput = childEnd;
  }

  // launch the process
  BOOL retValue = CreateProcessW(NULL, &paramsAlloc[0], &pSec, &tSec, true, CREATE_NEW_CONSOLE,
                                 NULL, NULL, &si, &pi);

  // we don't need this end anymore, the child has it
  CloseHandle(childEnd);

  if(retValue == FALSE)
  {
    RDCERR("Can't launch 64-bit renderdoccmd from '%ls'", cmdpathNative.c_str());
    CloseHandle(hookdata.dataNative.pipe);
    RestoreRegistry(hookdata);
    return false;
  }

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

  RDCEraseEl(pi);

// repeat the process for the Wow32 renderdoccmd
#if ENABLED(RDOC_X64)
  _snwprintf_s(&paramsAlloc[0], 2047, 2047,
               L"\"%ls\" globalhook --match \"%ls\" --capfile \"%ls\" --debuglog \"%ls\" "
               L"--capopts \"%hs\"",
               cmdpathWow32.c_str(), wpathmatch.c_str(), wcapturefile.c_str(),
               wdebugLogfile.c_str(), optstr.c_str());

  paramsAlloc[2047] = 0;

  {
    SECURITY_ATTRIBUTES pipeSec;
    pipeSec.nLength = sizeof(SECURITY_ATTRIBUTES);
    pipeSec.bInheritHandle = TRUE;
    pipeSec.lpSecurityDescriptor = NULL;

    BOOL res;
    res = CreatePipe(&childEnd, &hookdata.dataWow32.pipe, &pipeSec, 0);

    if(!res)
    {
      RDCERR("Could not create 64-bit stdin pipe");
      RestoreRegistry(hookdata);
      return false;
    }

    res = SetHandleInformation(hookdata.dataWow32.pipe, HANDLE_FLAG_INHERIT, 0);

    if(!res)
    {
      RDCERR("Could not make 64-bit stdin pipe inheritable");
      RestoreRegistry(hookdata);
      return false;
    }

    si.hStdInput = childEnd;
  }

  retValue = CreateProcessW(NULL, &paramsAlloc[0], &pSec, &tSec, true, CREATE_NEW_CONSOLE, NULL,
                            NULL, &si, &pi);

  // we don't need this end anymore
  CloseHandle(childEnd);

  if(retValue == FALSE)
  {
    RDCERR("Can't launch 32-bit renderdoccmd from '%ls'", cmdpathWow32.c_str());
    CloseHandle(hookdata.dataNative.pipe);
    CloseHandle(hookdata.dataWow32.pipe);
    RestoreRegistry(hookdata);
    return false;
  }

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
#endif

  // set static global pointer with our data, and launch the thread
  globalHook = new GlobalHookData;
  *globalHook = hookdata;

  globalHook->pipeThread = Threading::CreateThread(&GlobalHookThread);

  return true;
}

bool Process::IsGlobalHookActive()
{
  return globalHook != NULL;
}
void Process::StopGlobalHook()
{
  if(!globalHook)
    return;

  // set the finished flag and join to the thread so it closes the pipes (and so the child
  // processes)
  Atomic::Inc32(&globalHook->finished);

  Threading::JoinThread(globalHook->pipeThread);
  Threading::CloseThread(globalHook->pipeThread);

  // restore the registry settings from before we started
  RestoreRegistry(*globalHook);

  delete globalHook;
  globalHook = NULL;
}

void *Process::LoadModule(const char *module)
{
  HMODULE mod = GetModuleHandleA(module);
  if(mod != NULL)
    return mod;

  return LoadLibraryA(module);
}

void *Process::GetFunctionAddress(void *module, const char *function)
{
  if(module == NULL)
    return NULL;

  return (void *)GetProcAddress((HMODULE)module, function);
}

uint32_t Process::GetCurrentPID()
{
  return (uint32_t)GetCurrentProcessId();
}

void Process::Shutdown()
{
  // nothing to do
}