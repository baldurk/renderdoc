/******************************************************************************
 * The MIT License (MIT)
 * 
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


#include "os/os_specific.h"

#include "core/core.h"

#include "common/string_utils.h"

#include <windows.h>
#include <tlhelp32.h> 
#include <tchar.h>

#include <string>
using std::string;

void InjectDLL(HANDLE hProcess, wstring libName)
{
	wchar_t dllPath[MAX_PATH + 1] = {0};
	wcscpy_s(dllPath, libName.c_str());

	static HMODULE kernel32 = GetModuleHandle(_T("Kernel32"));

	void *remoteMem = VirtualAllocEx(hProcess, NULL, sizeof(dllPath), MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	WriteProcessMemory(hProcess, remoteMem, (void *)dllPath, sizeof(dllPath), NULL);

	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(kernel32, "LoadLibraryW"), remoteMem, 0, NULL);
	WaitForSingleObject(hThread, INFINITE);

	CloseHandle(hThread);
	VirtualFreeEx(hProcess, remoteMem, sizeof(dllPath), MEM_RELEASE); 
}

uintptr_t FindRemoteDLL(DWORD pid, wstring libName)
{
	HANDLE hModuleSnap = INVALID_HANDLE_VALUE;

	libName = strlower(libName);

	// up to 10 retries
	for(int i=0; i < 10; i++)
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
		wchar_t modnameLower[MAX_MODULE_NAME32+1];
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
		RDCERR("Couldn't find module '%ls' among %d modules", libName.c_str(), numModules);
	}

  CloseHandle(hModuleSnap); 

	return ret;
}

void InjectFunctionCall(HANDLE hProcess, uintptr_t renderdoc_remote, const char *funcName, void *data, const size_t dataLen)
{
	if(dataLen == 0)
	{
		RDCERR("Invalid function call injection attempt");
		return;
	}

	RDCDEBUG("Injecting call to %hs", funcName);
	
	HMODULE renderdoc_local = GetModuleHandleA("renderdoc.dll");
	
	uintptr_t func_local = (uintptr_t)GetProcAddress(renderdoc_local, funcName);

	// we've found SetCaptureOptions in our local instance of the module, now calculate the offset and so get the function
	// in the remote module (which might be loaded at a different base address
	uintptr_t func_remote = func_local + renderdoc_remote - (uintptr_t)renderdoc_local;
	
	void *remoteMem = VirtualAllocEx(hProcess, NULL, dataLen, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	SIZE_T numWritten;
	WriteProcessMemory(hProcess, remoteMem, data, dataLen, &numWritten);
	
	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)func_remote, remoteMem, 0, NULL);
	WaitForSingleObject(hThread, INFINITE);

	ReadProcessMemory(hProcess, remoteMem, data, dataLen, &numWritten);

	CloseHandle(hThread);
	VirtualFreeEx(hProcess, remoteMem, dataLen, MEM_RELEASE);
}

uint32_t Process::InjectIntoProcess(uint32_t pid, const wchar_t *logfile, const CaptureOptions *opts, bool waitForExit)
{
	CaptureOptions options;
	if(opts) options = *opts;

	HANDLE hProcess = OpenProcess( PROCESS_CREATE_THREAD | 
		PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
		PROCESS_VM_WRITE | PROCESS_VM_READ | SYNCHRONIZE,
		FALSE, pid );

	if(options.DelayForDebugger > 0)
	{
		RDCDEBUG("Waiting for debugger attach to %lu", pid);
		uint32_t timeout = 0;

		BOOL debuggerAttached = FALSE;

		while(!debuggerAttached)
		{
			CheckRemoteDebuggerPresent(hProcess, &debuggerAttached);

			Sleep(10);
			timeout += 10;

			if(timeout > options.DelayForDebugger*1000)
				break;
		}

		if(debuggerAttached)
			RDCDEBUG("Debugger attach detected after %.2f s", float(timeout)/1000.0f);
		else
			RDCDEBUG("Timed out waiting for debugger, gave up after %u s", options.DelayForDebugger);
	}

	RDCLOG("Injecting renderdoc into process %lu", pid);
	
	wchar_t renderdocPath[MAX_PATH] = {0};
	GetModuleFileNameW(GetModuleHandleA("renderdoc.dll"), &renderdocPath[0], MAX_PATH-1);

	BOOL isWow64 = FALSE;
	BOOL success = IsWow64Process(hProcess, &isWow64);

	if(!success)
	{
		RDCERR("Couldn't determine bitness of process");
		CloseHandle(hProcess);
		return 0;
	}

#if !defined(WIN64)
	BOOL selfWow64 = FALSE;

	HANDLE hSelfProcess = GetCurrentProcess();

	success = IsWow64Process(hSelfProcess, &selfWow64);

	CloseHandle(hSelfProcess);

	if(!success)
	{
		RDCERR("Couldn't determine bitness of self");
		return 0;
	}

	if(selfWow64 && !isWow64)
	{
		RDCERR("Can't capture x64 process with x86 renderdoc");
		CloseHandle(hProcess);
		return 0;
	}
#else
	// farm off to x86 version
	if(isWow64)
	{
		wchar_t *slash = wcsrchr(renderdocPath, L'\\');

		if(slash) *slash = 0;

		wcscat_s(renderdocPath, L"\\x86\\renderdoccmd.exe");

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
	
		wchar_t *paramsAlloc = new wchar_t[256];

		string optstr = opts->ToString();

		_snwprintf_s(paramsAlloc, 255, 255, L"\"%ls\" --cap32for64 %d \"%ls\" \"%hs\"", renderdocPath, pid, logfile, optstr.c_str());
	
		BOOL retValue = CreateProcessW(NULL, paramsAlloc, &pSec, &tSec, false, CREATE_SUSPENDED, NULL, NULL, &si, &pi);

		SAFE_DELETE_ARRAY(paramsAlloc);

		if (!retValue)
		{
			RDCERR("Can't spawn x86 renderdoccmd - missing files?");
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
#endif

#if USE_MHOOK
	// misc
	InjectDLL(hProcess, L"kernel32.dll");

	// D3D11
	InjectDLL(hProcess, L"d3d9.dll");
	InjectDLL(hProcess, L"d3d11.dll");
	InjectDLL(hProcess, L"dxgi.dll");

	// OpenGL
	InjectDLL(hProcess, L"opengl32.dll");
	InjectDLL(hProcess, L"gdi32.dll");
#endif

	InjectDLL(hProcess, renderdocPath);

	uintptr_t loc = FindRemoteDLL(pid, L"renderdoc.dll");
	
	uint32_t remoteident = 0;

	if(loc == 0)
	{
		RDCERR("Can't locate renderdoc.dll in remote PID %d", pid);
	}
	else
	{
		// safe to cast away the const as we know these functions don't modify the parameters

		if(logfile != NULL)
			InjectFunctionCall(hProcess, loc, "RENDERDOC_SetLogFile", (wchar_t *)logfile, (wcslen(logfile)+1)*sizeof(wchar_t));

		if(opts != NULL)
			InjectFunctionCall(hProcess, loc, "RENDERDOC_SetCaptureOptions", (CaptureOptions *)opts, sizeof(CaptureOptions));

		InjectFunctionCall(hProcess, loc, "RENDERDOC_InitRemoteAccess", &remoteident, sizeof(remoteident));
	}

	if(waitForExit)
		WaitForSingleObject(hProcess, INFINITE);

	CloseHandle(hProcess);

	return remoteident;
}

uint32_t Process::CreateAndInjectIntoProcess(const wchar_t *app, const wchar_t *workingDir, const wchar_t *cmdLine,
										const wchar_t *logfile, const CaptureOptions *opts, bool waitForExit)
{
	void *func = GetProcAddress(GetModuleHandleA("renderdoc.dll"), "RENDERDOC_SetLogFile");

	if (func == NULL)
	{
		RDCERR("Can't find required export function in renderdoc.dll - corrupted/missing file?");
		return 0;
	}

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

	wstring workdir = dirname(wstring(app));

	if(workingDir != NULL && workingDir[0] != 0)
		workdir = workingDir;

	wchar_t *paramsAlloc = NULL;

	// CreateProcessW can modify the params, need space.
	size_t len = wcslen(app)+10;
	
	if(cmdLine != NULL && cmdLine[0] != 0)
		len += wcslen(cmdLine);

	paramsAlloc = new wchar_t[len];

	RDCEraseMem(paramsAlloc, len);

	wcscpy_s(paramsAlloc, len, L"\"");
	wcscat_s(paramsAlloc, len, app);
	wcscat_s(paramsAlloc, len, L"\"");

	if(cmdLine != NULL && cmdLine[0] != 0)
	{
		wcscat_s(paramsAlloc, len, L" ");
		wcscat_s(paramsAlloc, len, cmdLine);
	}

	BOOL retValue = CreateProcessW(NULL, paramsAlloc,
		&pSec, &tSec, false, CREATE_SUSPENDED,
		NULL, workdir.c_str(), &si, &pi);

	SAFE_DELETE_ARRAY(paramsAlloc);

	// don't need it
	CloseHandle(pi.hProcess);

	if (!retValue)
	{
		RDCERR("Process %ls could not be loaded.", app);
		return 0;
	}

	uint32_t ret = InjectIntoProcess(pi.dwProcessId, logfile, opts, false);

	if(ret == 0)
	{
		ResumeThread(pi.hThread);
		CloseHandle(pi.hThread);
		return 0;
	}

	ResumeThread(pi.hThread);

	if(waitForExit)
		WaitForSingleObject(pi.hThread, INFINITE);

	CloseHandle(pi.hThread);

	return ret;
}

void *Process::GetFunctionAddress(const char *module, const char *function)
{
	HMODULE mod = GetModuleHandleA(module);
	if(mod == 0)
		return NULL;

	return (void *)GetProcAddress(mod, function);
}

uint32_t Process::GetCurrentPID()
{
	return (uint32_t)GetCurrentProcessId();
}

