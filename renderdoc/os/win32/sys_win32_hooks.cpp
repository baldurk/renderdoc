/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2016 Baldur Karlsson
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


#include "core/core.h"
#include "api/replay/renderdoc_replay.h"
#include "serialise/string_utils.h"

#include "hooks/hooks.h"

#define DLL_NAME "kernel32.dll"

typedef BOOL (WINAPI *PFN_CREATE_PROCESS_A)(
    __in_opt    LPCSTR lpApplicationName,
    __inout_opt LPSTR lpCommandLine,
    __in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in        BOOL bInheritHandles,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOA lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation
    );

typedef BOOL (WINAPI *PFN_CREATE_PROCESS_W)(
    __in_opt    LPCWSTR lpApplicationName,
    __inout_opt LPWSTR lpCommandLine,
    __in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
    __in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    __in        BOOL bInheritHandles,
    __in        DWORD dwCreationFlags,
    __in_opt    LPVOID lpEnvironment,
    __in_opt    LPCWSTR lpCurrentDirectory,
    __in        LPSTARTUPINFOW lpStartupInfo,
    __out       LPPROCESS_INFORMATION lpProcessInformation
    );

class SysHook : LibraryHook
{
	public:
		SysHook() { LibraryHooks::GetInstance().RegisterHook(DLL_NAME, this); m_EnabledHooks = true; }

		bool CreateHooks(const char *libName)
		{
			bool success = true;

			// we want to hook CreateProcess purely so that we can recursively insert our hooks (if we so wish)
			success &= CreateProcessA.Initialize("CreateProcessA", DLL_NAME, CreateProcessA_hook);
			success &= CreateProcessW.Initialize("CreateProcessW", DLL_NAME, CreateProcessW_hook);

			if(!success) return false;

			m_HasHooks = true;
			m_EnabledHooks = true;

			return true;
		}

		void EnableHooks(const char *libName, bool enable)
		{
			m_EnabledHooks = enable;
		}

		void OptionsUpdated(const char *libName) {}

	private:
		static SysHook syshooks;

		bool m_HasHooks;
		bool m_EnabledHooks;

		// D3DPERF api
		Hook<PFN_CREATE_PROCESS_A> CreateProcessA;
		Hook<PFN_CREATE_PROCESS_W> CreateProcessW;

		static BOOL WINAPI CreateProcessA_hook(
			__in_opt    LPCSTR lpApplicationName,
			__inout_opt LPSTR lpCommandLine,
			__in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
			__in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
			__in        BOOL bInheritHandles,
			__in        DWORD dwCreationFlags,
			__in_opt    LPVOID lpEnvironment,
			__in_opt    LPCSTR lpCurrentDirectory,
			__in        LPSTARTUPINFOA lpStartupInfo,
			__out       LPPROCESS_INFORMATION lpProcessInformation)
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

			dwCreationFlags |= CREATE_SUSPENDED;

			BOOL ret = syshooks.CreateProcessA()(lpApplicationName, lpCommandLine,
												 lpProcessAttributes, lpThreadAttributes,
												 bInheritHandles, dwCreationFlags,
												 lpEnvironment, lpCurrentDirectory, lpStartupInfo,
												 lpProcessInformation);
			
			if(RenderDoc::Inst().GetCaptureOptions().HookIntoChildren)
			{
				RDCDEBUG("Intercepting CreateProcessA");

				bool inject = true;

				// sanity check to make sure we're not going to go into an infinity loop injecting into ourselves.
				if(lpApplicationName)
				{
					string app = lpApplicationName;
					app = strlower(app);

					if(app.find("renderdoccmd.exe") != string::npos ||
					   app.find("renderdocui.vshost.exe") != string::npos ||
					   app.find("qrenderdoc.exe") != string::npos ||
					   app.find("renderdocui.exe") != string::npos)
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
					   cmd.find("qrenderdoc.exe") != string::npos ||
					   cmd.find("renderdocui.exe") != string::npos)
					{
						inject = false;
					}
				}

				if(inject)
				{
					// inherit logfile and capture options
					uint32_t ident = RENDERDOC_InjectIntoProcess(lpProcessInformation->dwProcessId,
						RenderDoc::Inst().GetLogFile(), &RenderDoc::Inst().GetCaptureOptions(), false);

					RenderDoc::Inst().AddChildProcess((uint32_t)lpProcessInformation->dwProcessId, ident);
				}
			}

			ResumeThread(lpProcessInformation->hThread);

			// ensure we clean up after ourselves
			if(dummy.dwProcessId != 0)
			{
				CloseHandle(dummy.hProcess);
				CloseHandle(dummy.hThread);
			}

			return ret;
		}
		
		static BOOL WINAPI CreateProcessW_hook(
			__in_opt    LPCWSTR lpApplicationName,
			__inout_opt LPWSTR lpCommandLine,
			__in_opt    LPSECURITY_ATTRIBUTES lpProcessAttributes,
			__in_opt    LPSECURITY_ATTRIBUTES lpThreadAttributes,
			__in        BOOL bInheritHandles,
			__in        DWORD dwCreationFlags,
			__in_opt    LPVOID lpEnvironment,
			__in_opt    LPCWSTR lpCurrentDirectory,
			__in        LPSTARTUPINFOW lpStartupInfo,
			__out       LPPROCESS_INFORMATION lpProcessInformation)
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

			dwCreationFlags |= CREATE_SUSPENDED;

			BOOL ret = syshooks.CreateProcessW()(lpApplicationName, lpCommandLine,
												 lpProcessAttributes, lpThreadAttributes,
												 bInheritHandles, dwCreationFlags,
												 lpEnvironment, lpCurrentDirectory, lpStartupInfo,
												 lpProcessInformation);
			
			if(RenderDoc::Inst().GetCaptureOptions().HookIntoChildren)
			{
				RDCDEBUG("Intercepting CreateProcessW");

				bool inject = true;

				// sanity check to make sure we're not going to go into an infinity loop injecting into ourselves.
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
					// inherit logfile and capture options
					uint32_t ident = RENDERDOC_InjectIntoProcess(lpProcessInformation->dwProcessId,
											RenderDoc::Inst().GetLogFile(), &RenderDoc::Inst().GetCaptureOptions(), false);

					RenderDoc::Inst().AddChildProcess((uint32_t)lpProcessInformation->dwProcessId, ident);
				}
			}
			
			ResumeThread(lpProcessInformation->hThread);

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