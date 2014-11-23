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


// win32_libentry.cpp : Defines the entry point for the DLL
#include <windows.h>
#include <tchar.h>

#include "common/common.h"
#include "serialise/string_utils.h"
#include "hooks/hooks.h"

#include "core/core.h"

void shutdown()
{
}

BOOL add_hooks()
{
	wchar_t curFile[512];
	GetModuleFileNameW(NULL, curFile, 512);

	wstring f = strlower(wstring(curFile));

	// bail immediately if we're in a system process. We don't want to hook, log, anything -
	// this instance is being used for a shell extension.
	if(f.find(L"dllhost.exe") != wstring::npos ||
		f.find(L"explorer.exe") != wstring::npos)
	{
#ifndef _RELEASE
		OutputDebugStringA("Hosting renderdoc.dll in shell process\n");
#endif
		return TRUE;
	}

	if(f.find(L"renderdoccmd.exe") != wstring::npos ||
		 f.find(L"renderdocui.vshost.exe") != wstring::npos ||
		 f.find(L"renderdocui.exe") != wstring::npos)
	{
		RDCDEBUG("Not creating hooks - in replay app");

		RenderDoc::Inst().SetReplayApp(true);
	
		RenderDoc::Inst().Initialise();

		return true;
	}
	
	RenderDoc::Inst().Initialise();

	RDCLOG("Loading into %ls", curFile);

	LibraryHooks::GetInstance().CreateHooks();

	return TRUE;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
						)
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		{
			return add_hooks();
			break;
		}
		case DLL_THREAD_ATTACH:
		{
			int thread_attach=0;
			break;
		}
		case DLL_THREAD_DETACH:
		{
			int thread_detach=0;
			break;
		}
		case DLL_PROCESS_DETACH:
		{
			shutdown();
			break;
		}
	}
	return TRUE;
}

