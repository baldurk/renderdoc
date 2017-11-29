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

#include <winsock2.h>
#include "api/replay/renderdoc_replay.h"
#include "core/core.h"
#include "hooks/hooks.h"
#include "strings/string_utils.h"

#define DLL_NAME "kernel32.dll"

typedef int(WSAAPI *PFN_WSASTARTUP)(__in WORD wVersionRequested, __out LPWSADATA lpWSAData);
typedef int(WSAAPI *PFN_WSACLEANUP)();

typedef BOOL(WINAPI *PFN_CREATE_PROCESS_A)(LPCSTR lpApplicationName, LPSTR lpCommandLine,
                                           LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                           LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                           BOOL bInheritHandles, DWORD dwCreationFlags,
                                           LPVOID lpEnvironment, LPCSTR lpCurrentDirectory,
                                           LPSTARTUPINFOA lpStartupInfo,
                                           LPPROCESS_INFORMATION lpProcessInformation);

typedef BOOL(WINAPI *PFN_CREATE_PROCESS_W)(LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                                           LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                           LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                           BOOL bInheritHandles, DWORD dwCreationFlags,
                                           LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
                                           LPSTARTUPINFOW lpStartupInfo,
                                           LPPROCESS_INFORMATION lpProcessInformation);

typedef BOOL(WINAPI *PFN_CREATE_PROCESS_AS_USER_A)(
    HANDLE hToken, LPCSTR lpApplicationName, LPSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCSTR lpCurrentDirectory,
    LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

typedef BOOL(WINAPI *PFN_CREATE_PROCESS_AS_USER_W)(
    HANDLE hToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
    LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
    BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
    LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

typedef BOOL(WINAPI *PFN_CREATE_PROCESS_WITH_LOGON_W)(LPCWSTR lpUsername, LPCWSTR lpDomain,
                                                      LPCWSTR lpPassword, DWORD dwLogonFlags,
                                                      LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                                                      DWORD dwCreationFlags, LPVOID lpEnvironment,
                                                      LPCWSTR lpCurrentDirectory,
                                                      LPSTARTUPINFOW lpStartupInfo,
                                                      LPPROCESS_INFORMATION lpProcessInformation);

class SysHook : LibraryHook
{
public:
  SysHook()
  {
    LibraryHooks::GetInstance().RegisterHook(DLL_NAME, this);
    m_EnabledHooks = true;
    m_HasHooks = false;
    m_WSARefCount = 1;
  }

  bool CreateHooks(const char *libName)
  {
    bool success = true;

    // we want to hook CreateProcess purely so that we can recursively insert our hooks (if we so
    // wish)
    success &= CreateProcessA.Initialize("CreateProcessA", "kernel32.dll", CreateProcessA_hook);
    success &= CreateProcessW.Initialize("CreateProcessW", "kernel32.dll", CreateProcessW_hook);

    success &= CreateProcessAsUserA.Initialize("CreateProcessAsUserA", "advapi32.dll",
                                               CreateProcessAsUserA_hook);
    success &= CreateProcessAsUserW.Initialize("CreateProcessAsUserW", "advapi32.dll",
                                               CreateProcessAsUserW_hook);

    success &= CreateProcessWithLogonW.Initialize("CreateProcessWithLogonW", "advapi32.dll",
                                                  CreateProcessWithLogonW_hook);

    // handle API set exports if they exist. These don't really exist so we don't have to worry
    // about
    // double hooking, and also they call into the 'real' implementation in kernelbase.dll
    API110CreateProcessA.Initialize("CreateProcessA", "api-ms-win-core-processthreads-l1-1-0.dll",
                                    API110CreateProcessA_hook);
    API110CreateProcessW.Initialize("CreateProcessW", "api-ms-win-core-processthreads-l1-1-0.dll",
                                    API110CreateProcessW_hook);
    API110CreateProcessAsUserW.Initialize("CreateProcessAsUserW",
                                          "api-ms-win-core-processthreads-l1-1-0.dll",
                                          API110CreateProcessAsUserW_hook);

    API111CreateProcessA.Initialize("CreateProcessA", "api-ms-win-core-processthreads-l1-1-1.dll",
                                    API111CreateProcessA_hook);
    API111CreateProcessW.Initialize("CreateProcessW", "api-ms-win-core-processthreads-l1-1-1.dll",
                                    API111CreateProcessW_hook);
    API111CreateProcessAsUserW.Initialize("CreateProcessAsUserW",
                                          "api-ms-win-core-processthreads-l1-1-0.dll",
                                          API111CreateProcessAsUserW_hook);

    API112CreateProcessA.Initialize("CreateProcessA", "api-ms-win-core-processthreads-l1-1-2.dll",
                                    API112CreateProcessA_hook);
    API112CreateProcessW.Initialize("CreateProcessW", "api-ms-win-core-processthreads-l1-1-2.dll",
                                    API112CreateProcessW_hook);
    API112CreateProcessAsUserW.Initialize("CreateProcessAsUserW",
                                          "api-ms-win-core-processthreads-l1-1-0.dll",
                                          API112CreateProcessAsUserW_hook);

    success &= WSAStartup.Initialize("WSAStartup", "ws2_32.dll", WSAStartup_hook);
    success &= WSACleanup.Initialize("WSACleanup", "ws2_32.dll", WSACleanup_hook);

    // we start with a refcount of 1 because we initialise WSA ourselves for our own sockets.
    m_WSARefCount = 1;

    m_RecurseSlot = Threading::AllocateTLSSlot();
    Threading::SetTLSValue(m_RecurseSlot, NULL);

    if(!success)
      return false;

    m_HasHooks = true;
    m_EnabledHooks = true;

    return true;
  }

  void EnableHooks(const char *libName, bool enable) { m_EnabledHooks = enable; }
  void OptionsUpdated(const char *libName) {}
private:
  static SysHook syshooks;

  bool m_HasHooks;
  bool m_EnabledHooks;

  int m_WSARefCount;
  uint64_t m_RecurseSlot = 0;

  bool CheckRecurse()
  {
    if(Threading::GetTLSValue(m_RecurseSlot) == NULL)
    {
      Threading::SetTLSValue(m_RecurseSlot, (void *)1);
      return false;
    }

    return true;
  }
  void EndRecurse() { Threading::SetTLSValue(m_RecurseSlot, NULL); }
  Hook<PFN_CREATE_PROCESS_A> CreateProcessA;
  Hook<PFN_CREATE_PROCESS_W> CreateProcessW;

  Hook<PFN_CREATE_PROCESS_A> API110CreateProcessA;
  Hook<PFN_CREATE_PROCESS_W> API110CreateProcessW;
  Hook<PFN_CREATE_PROCESS_A> API111CreateProcessA;
  Hook<PFN_CREATE_PROCESS_W> API111CreateProcessW;
  Hook<PFN_CREATE_PROCESS_A> API112CreateProcessA;
  Hook<PFN_CREATE_PROCESS_W> API112CreateProcessW;

  Hook<PFN_CREATE_PROCESS_AS_USER_A> CreateProcessAsUserA;
  Hook<PFN_CREATE_PROCESS_AS_USER_W> CreateProcessAsUserW;

  Hook<PFN_CREATE_PROCESS_AS_USER_W> API110CreateProcessAsUserW;
  Hook<PFN_CREATE_PROCESS_AS_USER_W> API111CreateProcessAsUserW;
  Hook<PFN_CREATE_PROCESS_AS_USER_W> API112CreateProcessAsUserW;

  Hook<PFN_CREATE_PROCESS_WITH_LOGON_W> CreateProcessWithLogonW;

  Hook<PFN_WSASTARTUP> WSAStartup;
  Hook<PFN_WSACLEANUP> WSACleanup;

  static int WSAAPI WSAStartup_hook(WORD wVersionRequested, LPWSADATA lpWSAData)
  {
    int ret = syshooks.WSAStartup()(wVersionRequested, lpWSAData);

    // only increment the refcount if the function succeeded
    if(ret == 0)
      syshooks.m_WSARefCount++;

    return ret;
  }

  static int WSAAPI WSACleanup_hook()
  {
    // don't let the application murder our sockets with a mismatched WSACleanup() call
    if(syshooks.m_WSARefCount == 1)
    {
      RDCLOG("WSACleanup called with (to the application) no WSAStartup! Ignoring.");
      SetLastError(WSANOTINITIALISED);
      return SOCKET_ERROR;
    }

    // decrement refcount and call the real thing
    syshooks.m_WSARefCount--;
    return syshooks.WSACleanup()();
  }

  static BOOL WINAPI Hooked_CreateProcess(
      const char *entryPoint,
      std::function<BOOL(DWORD dwCreationFlags, LPPROCESS_INFORMATION lpProcessInformation)> realFunc,
      DWORD dwCreationFlags, bool inject, LPPROCESS_INFORMATION lpProcessInformation)
  {
    bool recursive = syshooks.CheckRecurse();

    if(recursive)
      return realFunc(dwCreationFlags, lpProcessInformation);

    PROCESS_INFORMATION dummy;
    RDCEraseEl(dummy);

    // not sure if this is valid, but I need the PID so I'll fill in my own struct to ensure that.
    if(lpProcessInformation == NULL)
    {
      lpProcessInformation = &dummy;
    }
    else
    {
      *lpProcessInformation = dummy;
    }

    bool resume = (dwCreationFlags & CREATE_SUSPENDED) == 0;
    dwCreationFlags |= CREATE_SUSPENDED;

    RDCDEBUG("Calling real %s", entryPoint);
    BOOL ret = realFunc(dwCreationFlags, lpProcessInformation);
    RDCDEBUG("Called real %s", entryPoint);

    if(ret && inject)
    {
      RDCDEBUG("Intercepting %s", entryPoint);

      rdcarray<EnvironmentModification> env;

      // inherit logfile and capture options
      uint32_t ident = RENDERDOC_InjectIntoProcess(lpProcessInformation->dwProcessId, env,
                                                   RenderDoc::Inst().GetLogFile(),
                                                   RenderDoc::Inst().GetCaptureOptions(), false);

      RenderDoc::Inst().AddChildProcess((uint32_t)lpProcessInformation->dwProcessId, ident);
    }

    if(resume)
    {
      ResumeThread(lpProcessInformation->hThread);
    }

    // ensure we clean up after ourselves
    if(dummy.dwProcessId != 0)
    {
      CloseHandle(dummy.hProcess);
      CloseHandle(dummy.hThread);
    }

    syshooks.EndRecurse();

    return ret;
  }

  static bool ShouldInject(LPCWSTR lpApplicationName, LPCWSTR lpCommandLine)
  {
    if(!RenderDoc::Inst().GetCaptureOptions().HookIntoChildren)
      return false;

    bool inject = true;

    // sanity check to make sure we're not going to go into an infinity loop injecting into
    // ourselves.
    if(lpApplicationName)
    {
      wstring app = lpApplicationName;
      app = strlower(app);

      if(app.find(L"renderdoccmd.exe") != wstring::npos || app.find(L"qrenderdoc.exe") != string::npos)
      {
        inject = false;
      }
    }
    if(lpCommandLine)
    {
      wstring cmd = lpCommandLine;
      cmd = strlower(cmd);

      if(cmd.find(L"renderdoccmd.exe") != wstring::npos ||
         cmd.find(L"qrenderdoc.exe") != wstring::npos)
      {
        inject = false;
      }
    }

    return inject;
  }

  static bool ShouldInject(LPCSTR lpApplicationName, LPCSTR lpCommandLine)
  {
    if(!RenderDoc::Inst().GetCaptureOptions().HookIntoChildren)
      return false;

    return ShouldInject(lpApplicationName ? StringFormat::UTF82Wide(lpApplicationName).c_str() : NULL,
                        lpCommandLine ? StringFormat::UTF82Wide(lpCommandLine).c_str() : NULL);
  }

  static BOOL WINAPI CreateProcessA_hook(
      __in_opt LPCSTR lpApplicationName, __inout_opt LPSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCSTR lpCurrentDirectory,
      __in LPSTARTUPINFOA lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessA",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.CreateProcessA()(lpApplicationName, lpCommandLine, lpProcessAttributes,
                                           lpThreadAttributes, bInheritHandles, flags,
                                           lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }

  static BOOL WINAPI CreateProcessW_hook(__in_opt LPCWSTR lpApplicationName,
                                         __inout_opt LPWSTR lpCommandLine,
                                         __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
                                         __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes,
                                         __in BOOL bInheritHandles, __in DWORD dwCreationFlags,
                                         __in_opt LPVOID lpEnvironment,
                                         __in_opt LPCWSTR lpCurrentDirectory,
                                         __in LPSTARTUPINFOW lpStartupInfo,
                                         __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessW",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.CreateProcessW()(lpApplicationName, lpCommandLine, lpProcessAttributes,
                                           lpThreadAttributes, bInheritHandles, flags,
                                           lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }

  static BOOL WINAPI API110CreateProcessA_hook(
      __in_opt LPCSTR lpApplicationName, __inout_opt LPSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCSTR lpCurrentDirectory,
      __in LPSTARTUPINFOA lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessA",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.API110CreateProcessA()(
              lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
              bInheritHandles, flags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }

  static BOOL WINAPI API110CreateProcessW_hook(
      __in_opt LPCWSTR lpApplicationName, __inout_opt LPWSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCWSTR lpCurrentDirectory,
      __in LPSTARTUPINFOW lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessW",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.API110CreateProcessW()(
              lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
              bInheritHandles, flags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }

  static BOOL WINAPI API111CreateProcessA_hook(
      __in_opt LPCSTR lpApplicationName, __inout_opt LPSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCSTR lpCurrentDirectory,
      __in LPSTARTUPINFOA lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessA",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.API111CreateProcessA()(
              lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
              bInheritHandles, flags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }

  static BOOL WINAPI API111CreateProcessW_hook(
      __in_opt LPCWSTR lpApplicationName, __inout_opt LPWSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCWSTR lpCurrentDirectory,
      __in LPSTARTUPINFOW lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessW",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.API111CreateProcessW()(
              lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
              bInheritHandles, flags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }

  static BOOL WINAPI API112CreateProcessA_hook(
      __in_opt LPCSTR lpApplicationName, __inout_opt LPSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCSTR lpCurrentDirectory,
      __in LPSTARTUPINFOA lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessA",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.API112CreateProcessA()(
              lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
              bInheritHandles, flags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }

  static BOOL WINAPI API112CreateProcessW_hook(
      __in_opt LPCWSTR lpApplicationName, __inout_opt LPWSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCWSTR lpCurrentDirectory,
      __in LPSTARTUPINFOW lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessW",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.API112CreateProcessW()(
              lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
              bInheritHandles, flags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }

  static BOOL WINAPI CreateProcessAsUserA_hook(
      HANDLE hToken, LPCSTR lpApplicationName, LPSTR lpCommandLine,
      LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
      BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCSTR lpCurrentDirectory,
      LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessAsUserA",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.CreateProcessAsUserA()(
              hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
              bInheritHandles, flags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }

  static BOOL WINAPI CreateProcessAsUserW_hook(
      HANDLE hToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
      LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
      BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
      LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessAsUserW",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.CreateProcessAsUserW()(
              hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
              bInheritHandles, flags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }

  static BOOL WINAPI CreateProcessWithLogonW_hook(LPCWSTR lpUsername, LPCWSTR lpDomain,
                                                  LPCWSTR lpPassword, DWORD dwLogonFlags,
                                                  LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
                                                  DWORD dwCreationFlags, LPVOID lpEnvironment,
                                                  LPCWSTR lpCurrentDirectory,
                                                  LPSTARTUPINFOW lpStartupInfo,
                                                  LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessAsUserW",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.CreateProcessWithLogonW()(
              lpUsername, lpDomain, lpPassword, dwLogonFlags, lpApplicationName, lpCommandLine,
              flags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }

  static BOOL WINAPI API110CreateProcessAsUserW_hook(
      HANDLE hToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
      LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
      BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
      LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessAsUserW",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.API110CreateProcessAsUserW()(
              hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
              bInheritHandles, flags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }

  static BOOL WINAPI API111CreateProcessAsUserW_hook(
      HANDLE hToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
      LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
      BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
      LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessAsUserW",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.API111CreateProcessAsUserW()(
              hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
              bInheritHandles, flags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }

  static BOOL WINAPI API112CreateProcessAsUserW_hook(
      HANDLE hToken, LPCWSTR lpApplicationName, LPWSTR lpCommandLine,
      LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
      BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCWSTR lpCurrentDirectory,
      LPSTARTUPINFOW lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hooked_CreateProcess(
        "CreateProcessAsUserW",
        [=](DWORD flags, LPPROCESS_INFORMATION pi) {
          return syshooks.API112CreateProcessAsUserW()(
              hToken, lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
              bInheritHandles, flags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, pi);
        },
        dwCreationFlags, ShouldInject(lpApplicationName, lpCommandLine), lpProcessInformation);
  }
};

SysHook SysHook::syshooks;
