/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include <tchar.h>
#include <tlhelp32.h>
#include <string>
#include "core/core.h"
#include "os/os_specific.h"
#include "serialise/string_utils.h"

using std::string;

static wstring lowercase(wstring in)
{
  wstring ret;
  ret.resize(in.size());
  for(size_t i = 0; i < ret.size(); i++)
    ret[i] = towlower(in[i]);
  return ret;
}

static vector<EnvironmentModification> &GetEnvModifications()
{
  static vector<EnvironmentModification> envCallbacks;
  return envCallbacks;
}

static map<wstring, string> EnvStringToEnvMap(const wchar_t *envstring)
{
  map<wstring, string> ret;

  const wchar_t *e = envstring;

  while(*e)
  {
    const wchar_t *equals = wcschr(e, L'=');

    wstring name;
    wstring value;

    name.assign(e, equals);
    value = equals + 1;

    // set all names to lower case so we can do case-insensitive lookups
    ret[lowercase(name)] = StringFormat::Wide2UTF8(value);

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

// on windows we apply environment changes here, after process initialisation
// but before any real work (in RenderDoc::Initialise) so that we support
// injecting the dll into processes we didn't launch (ie didn't control the
// starting environment for), or even the application loading the dll itself
// without any interaction with our replay app.
void Process::ApplyEnvironmentModification()
{
  // turn environment string to a UTF-8 map
  LPWCH envStrings = GetEnvironmentStringsW();
  map<wstring, string> currentEnv = EnvStringToEnvMap(envStrings);
  FreeEnvironmentStringsW(envStrings);
  vector<EnvironmentModification> &modifications = GetEnvModifications();

  for(size_t i = 0; i < modifications.size(); i++)
  {
    EnvironmentModification &m = modifications[i];

    // set all names to lower case so we can do case-insensitive lookups, but
    // preserve the original name so that added variables maintain the same case
    wstring name = StringFormat::UTF82Wide(m.name.c_str());
    wstring lowername = lowercase(name);

    string value;

    auto it = currentEnv.find(lowername);
    if(it != currentEnv.end())
    {
      value = it->second;
      name = lowername;
    }

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
          if(m.sep == EnvSep::Platform || m.sep == EnvSep::SemiColon)
            value += ";";
          else if(m.sep == EnvSep::Colon)
            value += ":";
        }
        else
        {
          value = m.value.c_str();
        }
        break;
      }
    }

    SetEnvironmentVariableW(name.c_str(), StringFormat::UTF82Wide(value).c_str());
  }

  // these have been applied to the current process
  modifications.clear();
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

extern "C" __declspec(dllexport) void __cdecl INTERNAL_SetLogFile(const char *log)
{
  if(log)
    RenderDoc::Inst().SetLogFile(log);
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

void InjectDLL(HANDLE hProcess, wstring libName)
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
    WriteProcessMemory(hProcess, remoteMem, (void *)dllPath, sizeof(dllPath), NULL);

    HANDLE hThread = CreateRemoteThread(
        hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(kernel32, "LoadLibraryW"),
        remoteMem, 0, NULL);
    if(hThread)
    {
      WaitForSingleObject(hThread, INFINITE);
      CloseHandle(hThread);
    }
    VirtualFreeEx(hProcess, remoteMem, 0, MEM_RELEASE);
  }
  else
  {
    RDCERR("Couldn't allocate remote memory for DLL '%ls'", libName.c_str());
  }
}

uintptr_t FindRemoteDLL(DWORD pid, wstring libName)
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

    if(wcsstr(modnameLower, libName.c_str()))
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
          "Possibly the process has crashed during early startup?",
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

  wstring workdir = L"";

  if(workingDir != NULL && workingDir[0] != 0)
    workdir = StringFormat::UTF82Wide(string(workingDir));
  else
    workdir = StringFormat::UTF82Wide(dirname(string(app)));

  wchar_t *paramsAlloc = NULL;

  wstring wapp = StringFormat::UTF82Wide(string(app));

  // CreateProcessW can modify the params, need space.
  size_t len = wapp.length() + 10;

  wstring wcmd = L"";

  if(cmdLine != NULL && cmdLine[0] != 0)
  {
    wcmd = StringFormat::UTF82Wide(string(cmdLine));
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

    si.dwFlags |= STARTF_USESHOWWINDOW    // Hide the command prompt window from showing.
                  | STARTF_USESTDHANDLES;
    si.hStdOutput = hChildStdOutput_Wr;
    si.hStdError = hChildStdError_Wr;
  }

  RDCLOG("Running process %s", app);

  BOOL retValue = CreateProcessW(NULL, paramsAlloc, &pSec, &tSec,
                                 true,    // Need to inherit handles for ReadFile to read stdout
                                 CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT, NULL,
                                 workdir.c_str(), &si, &pi);

  if(phChildStdOutput_Rd)
  {
    CloseHandle(hChildStdOutput_Wr);
    CloseHandle(hChildStdError_Wr);
  }

  SAFE_DELETE_ARRAY(paramsAlloc);

  if(!retValue)
  {
    RDCWARN("Process %s could not be loaded.", app);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    RDCEraseEl(pi);
  }

  return pi;
}

uint32_t Process::InjectIntoProcess(uint32_t pid, const rdctype::array<EnvironmentModification> &env,
                                    const char *logfile, const CaptureOptions &opts, bool waitForExit)
{
  wstring wlogfile = logfile == NULL ? L"" : StringFormat::UTF82Wide(logfile);

  HANDLE hProcess =
      OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                      PROCESS_VM_WRITE | PROCESS_VM_READ | SYNCHRONIZE,
                  FALSE, pid);

  if(opts.DelayForDebugger > 0)
  {
    RDCDEBUG("Waiting for debugger attach to %lu", pid);
    uint32_t timeout = 0;

    BOOL debuggerAttached = FALSE;

    while(!debuggerAttached)
    {
      CheckRemoteDebuggerPresent(hProcess, &debuggerAttached);

      Sleep(10);
      timeout += 10;

      if(timeout > opts.DelayForDebugger * 1000)
        break;
    }

    if(debuggerAttached)
      RDCDEBUG("Debugger attach detected after %.2f s", float(timeout) / 1000.0f);
    else
      RDCDEBUG("Timed out waiting for debugger, gave up after %u s", opts.DelayForDebugger);
  }

  RDCLOG("Injecting renderdoc into process %lu", pid);

  wchar_t renderdocPath[MAX_PATH] = {0};
  GetModuleFileNameW(GetModuleHandleA(STRINGIZE(RDOC_DLL_FILE) ".dll"), &renderdocPath[0],
                     MAX_PATH - 1);

  wchar_t renderdocPathLower[MAX_PATH] = {0};
  memcpy(renderdocPathLower, renderdocPath, MAX_PATH * sizeof(wchar_t));
  for(size_t i = 0; renderdocPathLower[i] && i < MAX_PATH; i++)
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
    return 0;
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
    return 0;
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
      return 0;
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

    pSec.nLength = sizeof(pSec);
    tSec.nLength = sizeof(tSec);

    // serialise to string with two chars per byte
    string optstr;
    {
      optstr.reserve(sizeof(CaptureOptions) * 2 + 1);
      byte *b = (byte *)&opts;
      for(size_t i = 0; i < sizeof(CaptureOptions); i++)
      {
        optstr.push_back(char('a' + ((b[i] >> 4) & 0xf)));
        optstr.push_back(char('a' + ((b[i]) & 0xf)));
      }
    }

    wchar_t *paramsAlloc = new wchar_t[2048];

    std::string debugLogfile = RDCGETLOGFILE();
    wstring wdebugLogfile = StringFormat::UTF82Wide(debugLogfile);

    _snwprintf_s(paramsAlloc, 2047, 2047,
                 L"\"%ls\" capaltbit --pid=%d --log=\"%ls\" --debuglog=\"%ls\" --capopts=\"%hs\"",
                 renderdocPath, pid, wlogfile.c_str(), wdebugLogfile.c_str(), optstr.c_str());

    RDCDEBUG("params %ls", paramsAlloc);

    paramsAlloc[2047] = 0;

    wchar_t *commandLine = paramsAlloc;

    wstring cmdWithEnv;

    if(!env.empty())
    {
      cmdWithEnv = paramsAlloc;

      for(const EnvironmentModification &e : env)
      {
        string name = trim(e.name.c_str());
        string value = e.value.c_str();

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

    BOOL retValue = CreateProcessW(NULL, commandLine, &pSec, &tSec, false, CREATE_SUSPENDED, NULL,
                                   NULL, &si, &pi);

    SAFE_DELETE_ARRAY(paramsAlloc);

    if(!retValue)
    {
      RDCERR(
          "Can't spawn alternate bitness renderdoccmd - have you built 32-bit and 64-bit?\n"
          "You need to build the matching bitness for the programs you want to capture.");
      return 0;
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

    return (uint32_t)exitCode;
  }

  InjectDLL(hProcess, renderdocPath);

  uintptr_t loc = FindRemoteDLL(pid, CONCAT(L, STRINGIZE(RDOC_DLL_FILE)) L".dll");

  uint32_t controlident = 0;

  if(loc == 0)
  {
    RDCERR("Can't locate " STRINGIZE(RDOC_DLL_FILE) ".dll in remote PID %d", pid);
  }
  else
  {
    // safe to cast away the const as we know these functions don't modify the parameters

    if(logfile != NULL)
      InjectFunctionCall(hProcess, loc, "INTERNAL_SetLogFile", (void *)logfile, strlen(logfile) + 1);

    std::string debugLogfile = RDCGETLOGFILE();

    InjectFunctionCall(hProcess, loc, "RENDERDOC_SetDebugLogFile", (void *)debugLogfile.c_str(),
                       debugLogfile.size() + 1);

    InjectFunctionCall(hProcess, loc, "INTERNAL_SetCaptureOptions", (CaptureOptions *)&opts,
                       sizeof(CaptureOptions));

    InjectFunctionCall(hProcess, loc, "INTERNAL_GetTargetControlIdent", &controlident,
                       sizeof(controlident));

    if(!env.empty())
    {
      for(const EnvironmentModification &e : env)
      {
        string name = trim(e.name.c_str());
        string value = e.value.c_str();
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

  return controlident;
}

uint32_t Process::LaunchProcess(const char *app, const char *workingDir, const char *cmdLine,
                                ProcessResult *result)
{
  HANDLE hChildStdOutput_Rd, hChildStdError_Rd;

  PROCESS_INFORMATION pi = RunProcess(app, workingDir, cmdLine, result ? &hChildStdOutput_Rd : NULL,
                                      result ? &hChildStdError_Rd : NULL);

  if(pi.dwProcessId == 0)
  {
    RDCWARN("Couldn't launch process '%s'", app);
    return 0;
  }

  RDCLOG("Launched process '%s' with '%s'", app, cmdLine);

  ResumeThread(pi.hThread);

  if(result)
  {
    result->strStdout = "";
    result->strStderror = "";
    for(;;)
    {
      char chBuf[1000];
      DWORD dwOutputRead, dwErrorRead;

      BOOL success = ReadFile(hChildStdOutput_Rd, chBuf, sizeof(chBuf), &dwOutputRead, NULL);
      string s(chBuf, dwOutputRead);
      result->strStdout += s;

      success = ReadFile(hChildStdError_Rd, chBuf, sizeof(chBuf), &dwErrorRead, NULL);
      s = string(chBuf, dwErrorRead);
      result->strStderror += s;

      if(!success && !dwOutputRead && !dwErrorRead)
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

uint32_t Process::LaunchAndInjectIntoProcess(const char *app, const char *workingDir,
                                             const char *cmdLine,
                                             const rdctype::array<EnvironmentModification> &env,
                                             const char *logfile, const CaptureOptions &opts,
                                             bool waitForExit)
{
  void *func =
      GetProcAddress(GetModuleHandleA(STRINGIZE(RDOC_DLL_FILE) ".dll"), "INTERNAL_SetLogFile");

  if(func == NULL)
  {
    RDCERR("Can't find required export function in " STRINGIZE(
        RDOC_DLL_FILE) ".dll - corrupted/missing file?");
    return 0;
  }

  PROCESS_INFORMATION pi = RunProcess(app, workingDir, cmdLine, NULL, NULL);

  if(pi.dwProcessId == 0)
    return 0;

  uint32_t ret = InjectIntoProcess(pi.dwProcessId, env, logfile, opts, false);

  CloseHandle(pi.hProcess);
  ResumeThread(pi.hThread);
  ResumeThread(pi.hThread);

  if(ret == 0)
  {
    CloseHandle(pi.hThread);
    return 0;
  }

  if(waitForExit)
    WaitForSingleObject(pi.hThread, INFINITE);

  CloseHandle(pi.hThread);

  return ret;
}

void Process::StartGlobalHook(const char *pathmatch, const char *logfile, const CaptureOptions &opts)
{
  if(pathmatch == NULL)
    return;

  wchar_t renderdocPath[MAX_PATH] = {0};
  GetModuleFileNameW(GetModuleHandleA(STRINGIZE(RDOC_DLL_FILE) ".dll"), &renderdocPath[0],
                     MAX_PATH - 1);

  wchar_t *slash = wcsrchr(renderdocPath, L'\\');

  if(slash)
    *slash = 0;
  else
    slash = renderdocPath + wcslen(renderdocPath);

  wcscat_s(renderdocPath, L"\\renderdoccmd.exe");

  PROCESS_INFORMATION pi = {0};
  STARTUPINFO si = {0};
  SECURITY_ATTRIBUTES pSec = {0};
  SECURITY_ATTRIBUTES tSec = {0};
  pSec.nLength = sizeof(pSec);
  tSec.nLength = sizeof(tSec);

  wchar_t *paramsAlloc = new wchar_t[2048];

  // serialise to string with two chars per byte
  string optstr;
  {
    optstr.reserve(sizeof(CaptureOptions) * 2 + 1);
    byte *b = (byte *)&opts;
    for(size_t i = 0; i < sizeof(CaptureOptions); i++)
    {
      optstr.push_back(char('a' + ((b[i] >> 4) & 0xf)));
      optstr.push_back(char('a' + ((b[i]) & 0xf)));
    }
  }

  wstring wlogfile = logfile == NULL ? L"" : StringFormat::UTF82Wide(string(logfile));
  wstring wpathmatch = StringFormat::UTF82Wide(string(pathmatch));

  std::string debugLogfile = RDCGETLOGFILE();
  wstring wdebugLogfile = StringFormat::UTF82Wide(debugLogfile);

  _snwprintf_s(
      paramsAlloc, 2047, 2047,
      L"\"%ls\" globalhook --match \"%ls\" --logfile \"%ls\" --debuglog \"%ls\" --capopts \"%hs\"",
      renderdocPath, wpathmatch.c_str(), wlogfile.c_str(), wdebugLogfile.c_str(), optstr.c_str());

  paramsAlloc[2047] = 0;

  BOOL retValue = CreateProcessW(NULL, paramsAlloc, &pSec, &tSec, false, 0, NULL, NULL, &si, &pi);

  if(retValue == FALSE)
    return;

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);

#if ENABLED(RDOC_X64)
  *slash = 0;

  wcscat_s(renderdocPath, L"\\x86\\renderdoccmd.exe");

  _snwprintf_s(paramsAlloc, 2047, 2047,
               L"\"%ls\" globalhook --match \"%ls\" --log \"%ls\" --capopts \"%hs\"", renderdocPath,
               wpathmatch.c_str(), wlogfile.c_str(), optstr.c_str());

  paramsAlloc[2047] = 0;

  retValue = CreateProcessW(NULL, paramsAlloc, &pSec, &tSec, false, 0, NULL, NULL, &si, &pi);

  if(retValue == FALSE)
    return;

  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
#endif
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
