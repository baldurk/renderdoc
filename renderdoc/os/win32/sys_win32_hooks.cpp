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
#include "serialise/string_utils.h"

#define DLL_NAME "kernel32.dll"

typedef int(WSAAPI *PFN_WSASTARTUP)(__in WORD wVersionRequested, __out LPWSADATA lpWSAData);
typedef int(WSAAPI *PFN_WSACLEANUP)();

typedef BOOL(WINAPI *PFN_CREATE_PROCESS_A)(
    __in_opt LPCSTR lpApplicationName, __inout_opt LPSTR lpCommandLine,
    __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
    __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCSTR lpCurrentDirectory,
    __in LPSTARTUPINFOA lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation);

typedef BOOL(WINAPI *PFN_CREATE_PROCESS_W)(
    __in_opt LPCWSTR lpApplicationName, __inout_opt LPWSTR lpCommandLine,
    __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
    __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCWSTR lpCurrentDirectory,
    __in LPSTARTUPINFOW lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation);

class SysHook : LibraryHook
{
public:
  SysHook()
  {
    LibraryHooks::GetInstance().RegisterHook(DLL_NAME, this);
    m_EnabledHooks = true;
    m_HasHooks = false;
  }

  bool CreateHooks(const char *libName)
  {
    bool success = true;

    // we want to hook CreateProcess purely so that we can recursively insert our hooks (if we so
    // wish)
    success &= CreateProcessA.Initialize("CreateProcessA", "kernel32.dll", CreateProcessA_hook);
    success &= CreateProcessW.Initialize("CreateProcessW", "kernel32.dll", CreateProcessW_hook);

    // handle API set exports if they exist. These don't really exist so we don't have to worry
    // about
    // double hooking, and also they call into the 'real' implementation in kernelbase.dll
    API110CreateProcessA.Initialize("CreateProcessA", "api-ms-win-core-processthreads-l1-1-0.dll",
                                    API110CreateProcessA_hook);
    API110CreateProcessW.Initialize("CreateProcessW", "api-ms-win-core-processthreads-l1-1-0.dll",
                                    API110CreateProcessW_hook);

    API111CreateProcessA.Initialize("CreateProcessA", "api-ms-win-core-processthreads-l1-1-1.dll",
                                    API111CreateProcessA_hook);
    API111CreateProcessW.Initialize("CreateProcessW", "api-ms-win-core-processthreads-l1-1-1.dll",
                                    API111CreateProcessW_hook);

    API112CreateProcessA.Initialize("CreateProcessA", "api-ms-win-core-processthreads-l1-1-2.dll",
                                    API112CreateProcessA_hook);
    API112CreateProcessW.Initialize("CreateProcessW", "api-ms-win-core-processthreads-l1-1-2.dll",
                                    API112CreateProcessW_hook);

    success &= WSAStartup.Initialize("WSAStartup", "ws2_32.dll", WSAStartup_hook);
    success &= WSACleanup.Initialize("WSACleanup", "ws2_32.dll", WSACleanup_hook);

    // we start with a refcount of 1 because we initialise WSA ourselves for our own sockets.
    m_WSARefCount = 1;

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

  Hook<PFN_CREATE_PROCESS_A> CreateProcessA;
  Hook<PFN_CREATE_PROCESS_W> CreateProcessW;

  Hook<PFN_CREATE_PROCESS_A> API110CreateProcessA;
  Hook<PFN_CREATE_PROCESS_W> API110CreateProcessW;
  Hook<PFN_CREATE_PROCESS_A> API111CreateProcessA;
  Hook<PFN_CREATE_PROCESS_W> API111CreateProcessW;
  Hook<PFN_CREATE_PROCESS_A> API112CreateProcessA;
  Hook<PFN_CREATE_PROCESS_W> API112CreateProcessW;

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

  static BOOL WINAPI CreateProcessA_hook(
      __in_opt LPCSTR lpApplicationName, __inout_opt LPSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCSTR lpCurrentDirectory,
      __in LPSTARTUPINFOA lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hook_CreateProcessA(syshooks.CreateProcessA(), lpApplicationName, lpCommandLine,
                               lpProcessAttributes, lpThreadAttributes, bInheritHandles,
                               dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo,
                               lpProcessInformation);
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
    return Hook_CreateProcessW(syshooks.CreateProcessW(), lpApplicationName, lpCommandLine,
                               lpProcessAttributes, lpThreadAttributes, bInheritHandles,
                               dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo,
                               lpProcessInformation);
  }

  static BOOL WINAPI API110CreateProcessA_hook(
      __in_opt LPCSTR lpApplicationName, __inout_opt LPSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCSTR lpCurrentDirectory,
      __in LPSTARTUPINFOA lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hook_CreateProcessA(syshooks.API110CreateProcessA(), lpApplicationName, lpCommandLine,
                               lpProcessAttributes, lpThreadAttributes, bInheritHandles,
                               dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo,
                               lpProcessInformation);
  }

  static BOOL WINAPI API110CreateProcessW_hook(
      __in_opt LPCWSTR lpApplicationName, __inout_opt LPWSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCWSTR lpCurrentDirectory,
      __in LPSTARTUPINFOW lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hook_CreateProcessW(syshooks.API110CreateProcessW(), lpApplicationName, lpCommandLine,
                               lpProcessAttributes, lpThreadAttributes, bInheritHandles,
                               dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo,
                               lpProcessInformation);
  }

  static BOOL WINAPI API111CreateProcessA_hook(
      __in_opt LPCSTR lpApplicationName, __inout_opt LPSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCSTR lpCurrentDirectory,
      __in LPSTARTUPINFOA lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hook_CreateProcessA(syshooks.API111CreateProcessA(), lpApplicationName, lpCommandLine,
                               lpProcessAttributes, lpThreadAttributes, bInheritHandles,
                               dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo,
                               lpProcessInformation);
  }

  static BOOL WINAPI API111CreateProcessW_hook(
      __in_opt LPCWSTR lpApplicationName, __inout_opt LPWSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCWSTR lpCurrentDirectory,
      __in LPSTARTUPINFOW lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hook_CreateProcessW(syshooks.API111CreateProcessW(), lpApplicationName, lpCommandLine,
                               lpProcessAttributes, lpThreadAttributes, bInheritHandles,
                               dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo,
                               lpProcessInformation);
  }

  static BOOL WINAPI API112CreateProcessA_hook(
      __in_opt LPCSTR lpApplicationName, __inout_opt LPSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCSTR lpCurrentDirectory,
      __in LPSTARTUPINFOA lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hook_CreateProcessA(syshooks.API112CreateProcessA(), lpApplicationName, lpCommandLine,
                               lpProcessAttributes, lpThreadAttributes, bInheritHandles,
                               dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo,
                               lpProcessInformation);
  }

  static BOOL WINAPI API112CreateProcessW_hook(
      __in_opt LPCWSTR lpApplicationName, __inout_opt LPWSTR lpCommandLine,
      __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCWSTR lpCurrentDirectory,
      __in LPSTARTUPINFOW lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
    return Hook_CreateProcessW(syshooks.API112CreateProcessW(), lpApplicationName, lpCommandLine,
                               lpProcessAttributes, lpThreadAttributes, bInheritHandles,
                               dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo,
                               lpProcessInformation);
  }

  static BOOL WINAPI Hook_CreateProcessA(
      PFN_CREATE_PROCESS_A realFunc, __in_opt LPCSTR lpApplicationName,
      __inout_opt LPSTR lpCommandLine, __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCSTR lpCurrentDirectory,
      __in LPSTARTUPINFOA lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
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

    BOOL ret = realFunc(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
                        bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory,
                        lpStartupInfo, lpProcessInformation);

    if(ret && RenderDoc::Inst().GetCaptureOptions().HookIntoChildren)
    {
      RDCDEBUG("Intercepting CreateProcessA");

      bool inject = true;

      // sanity check to make sure we're not going to go into an infinity loop injecting into
      // ourselves.
      if(lpApplicationName)
      {
        string app = lpApplicationName;
        app = strlower(app);

        if(app.find("renderdoccmd.exe") != string::npos ||
           app.find("renderdocui.vshost.exe") != string::npos ||
           app.find("qrenderdoc.exe") != string::npos || app.find("renderdocui.exe") != string::npos)
        {
          inject = false;
        }
      }
      if(lpCommandLine)
      {
        string cmd = lpCommandLine;
        cmd = strlower(cmd);

        if(cmd.find("renderdoccmd.exe") != string::npos ||
           cmd.find("renderdocui.vshost.exe") != string::npos ||
           cmd.find("qrenderdoc.exe") != string::npos || cmd.find("renderdocui.exe") != string::npos)
        {
          inject = false;
        }
      }

      if(inject)
      {
        rdctype::array<EnvironmentModification> env;

        // inherit logfile and capture options
        uint32_t ident = RENDERDOC_InjectIntoProcess(lpProcessInformation->dwProcessId, env,
                                                     RenderDoc::Inst().GetLogFile(),
                                                     RenderDoc::Inst().GetCaptureOptions(), false);

        RenderDoc::Inst().AddChildProcess((uint32_t)lpProcessInformation->dwProcessId, ident);
      }
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

    return ret;
  }

  static BOOL WINAPI Hook_CreateProcessW(
      PFN_CREATE_PROCESS_W realFunc, __in_opt LPCWSTR lpApplicationName,
      __inout_opt LPWSTR lpCommandLine, __in_opt LPSECURITY_ATTRIBUTES lpProcessAttributes,
      __in_opt LPSECURITY_ATTRIBUTES lpThreadAttributes, __in BOOL bInheritHandles,
      __in DWORD dwCreationFlags, __in_opt LPVOID lpEnvironment, __in_opt LPCWSTR lpCurrentDirectory,
      __in LPSTARTUPINFOW lpStartupInfo, __out LPPROCESS_INFORMATION lpProcessInformation)
  {
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

    BOOL ret = realFunc(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
                        bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory,
                        lpStartupInfo, lpProcessInformation);

    if(ret && RenderDoc::Inst().GetCaptureOptions().HookIntoChildren)
    {
      RDCDEBUG("Intercepting CreateProcessW");

      bool inject = true;

      // sanity check to make sure we're not going to go into an infinity loop injecting into
      // ourselves.
      if(lpApplicationName)
      {
        wstring app = lpApplicationName;
        app = strlower(app);

        if(app.find(L"renderdoccmd.exe") != wstring::npos ||
           app.find(L"renderdocui.vshost.exe") != wstring::npos ||
           app.find(L"qrenderdoc.exe") != string::npos ||
           app.find(L"renderdocui.exe") != wstring::npos)
        {
          inject = false;
        }
      }
      if(lpCommandLine)
      {
        wstring cmd = lpCommandLine;
        cmd = strlower(cmd);

        if(cmd.find(L"renderdoccmd.exe") != wstring::npos ||
           cmd.find(L"renderdocui.vshost.exe") != wstring::npos ||
           cmd.find(L"qrenderdoc.exe") != wstring::npos ||
           cmd.find(L"renderdocui.exe") != wstring::npos)
        {
          inject = false;
        }
      }

      if(inject)
      {
        rdctype::array<EnvironmentModification> env;

        // inherit logfile and capture options
        uint32_t ident = RENDERDOC_InjectIntoProcess(lpProcessInformation->dwProcessId, env,
                                                     RenderDoc::Inst().GetLogFile(),
                                                     RenderDoc::Inst().GetCaptureOptions(), false);

        RenderDoc::Inst().AddChildProcess((uint32_t)lpProcessInformation->dwProcessId, ident);
      }
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

    return ret;
  }
};

SysHook SysHook::syshooks;
