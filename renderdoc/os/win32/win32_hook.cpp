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

#include "common/string_utils.h"

#include <windows.h>
#include <tlhelp32.h> 

#include <vector>
#include <map>
using std::vector;
using std::map;

struct FunctionHook
{
	FunctionHook(const char *f, void **o, void *d)
		: function(f), origptr(o), hookptr(d), excludeModule(NULL)
	{}
	
	bool operator <(const FunctionHook &h)
	{
		return function < h.function;
	}

	void ApplyHook(void **IATentry)
	{
		DWORD oldProtection = PAGE_EXECUTE;

		BOOL success = TRUE;

		success = VirtualProtect(IATentry, sizeof(void*), PAGE_READWRITE, &oldProtection);
		if(!success)
		{
			RDCERR("Failed to make IAT entry writeable 0x%p", IATentry);
			return;
		}

		if(origptr && *origptr == NULL && *IATentry != hookptr) *origptr = *IATentry;
		
		*IATentry = hookptr;

		success = VirtualProtect(IATentry, sizeof(void*), oldProtection, &oldProtection);
		if(!success)
		{
			RDCERR("Failed to restore IAT entry protection 0x%p", IATentry);
			return;
		}
	}

	string function;
	void **origptr;
	void *hookptr;
	HMODULE excludeModule;
};

struct DllHookset
{
	DllHookset() : module(NULL) {}

	HMODULE module;
	vector<FunctionHook> FunctionHooks;
};

struct CachedHookData
{
	map<string, DllHookset> DllHooks;
	HMODULE module;

	void ApplyHooks(const char *modName, HMODULE module)
	{
		string name = strlower(string(modName));

		// set module pointer if we are hooking exports from this module
		for(auto it=DllHooks.begin(); it != DllHooks.end(); ++it)
			if(it->first == name)
				it->second.module = module;

		byte *baseAddress = (byte *)module;

		PIMAGE_DOS_HEADER dosheader = (PIMAGE_DOS_HEADER)baseAddress;

		char *PE00 = (char *)(baseAddress + dosheader->e_lfanew);
		PIMAGE_FILE_HEADER fileHeader = (PIMAGE_FILE_HEADER)(PE00+4);
		PIMAGE_OPTIONAL_HEADER optHeader = (PIMAGE_OPTIONAL_HEADER)((BYTE *)fileHeader+sizeof(IMAGE_FILE_HEADER));

		DWORD iatSize = optHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size;
		DWORD iatOffset = optHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

		IMAGE_IMPORT_DESCRIPTOR *importDesc = (IMAGE_IMPORT_DESCRIPTOR *)(baseAddress + iatOffset);

		while(iatOffset && importDesc->FirstThunk)
		{
			string dllName = strlower(string((const char *)(baseAddress + importDesc->Name)));

			DllHookset *hookset = NULL;

			for(auto it=DllHooks.begin(); it != DllHooks.end(); ++it)
				if(it->first == dllName)
					hookset = &it->second;

			if(hookset)
			{
				IMAGE_THUNK_DATA *origFirst = (IMAGE_THUNK_DATA *)(baseAddress + importDesc->OriginalFirstThunk);
				IMAGE_THUNK_DATA *first = (IMAGE_THUNK_DATA *)(baseAddress + importDesc->FirstThunk);

				while(origFirst->u1.AddressOfData)
				{
#ifdef WIN64
					if(origFirst->u1.AddressOfData & 0x8000000000000000)
#else
					if(origFirst->u1.AddressOfData & 0x80000000)
#endif
					{
						// low bits of origFirst->u1.AddressOfData contain an ordinal
						origFirst++;
						first++;
						continue;
					}

					IMAGE_IMPORT_BY_NAME *import = (IMAGE_IMPORT_BY_NAME *)(baseAddress + origFirst->u1.AddressOfData);
					void **IATentry = (void **)&first->u1.AddressOfData;

					FunctionHook search(import->Name, NULL, NULL);
					auto found = std::lower_bound(hookset->FunctionHooks.begin(), hookset->FunctionHooks.end(), search);

					if(found != hookset->FunctionHooks.end() && !(search < *found) && found->excludeModule != module)
						found->ApplyHook(IATentry);

					origFirst++;
					first++;
				}
			}

			importDesc++;
		}
	}
};

static CachedHookData *s_HookData = NULL;

HMODULE WINAPI Hooked_LoadLibraryExA(LPCSTR lpLibFileName, HANDLE fileHandle, DWORD flags)
{
	// we can use the function naked, as when setting up the hook for LoadLibraryExA, our own module
	// was excluded from IAT patching
	HMODULE mod = LoadLibraryExA(lpLibFileName, fileHandle, flags);

	if(mod)
		s_HookData->ApplyHooks(lpLibFileName, mod);

	return mod;
}

HMODULE WINAPI Hooked_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE fileHandle, DWORD flags)
{
	// we can use the function naked, as when setting up the hook for LoadLibraryExA, our own module
	// was excluded from IAT patching
	HMODULE mod = LoadLibraryExW(lpLibFileName, fileHandle, flags);

	if(mod)
		s_HookData->ApplyHooks(narrow(wstring(lpLibFileName)).c_str(), mod);

	return mod;
}

HMODULE WINAPI Hooked_LoadLibraryA(LPCSTR lpLibFileName)
{
	return Hooked_LoadLibraryExA(lpLibFileName, NULL, 0);
}

HMODULE WINAPI Hooked_LoadLibraryW(LPCWSTR lpLibFileName)
{
	return Hooked_LoadLibraryExW(lpLibFileName, NULL, 0);
}

static bool OrdinalAsString(void *func)
{
	return uint64_t(func) <= 0xffff;
}

FARPROC WINAPI Hooked_GetProcAddress(HMODULE mod, LPCSTR func)
{
	if(mod == s_HookData->module || mod == NULL || func == NULL || OrdinalAsString((void *)func))
		return GetProcAddress(mod, func);
	
	for(auto it=s_HookData->DllHooks.begin(); it != s_HookData->DllHooks.end(); ++it)
	{
		if(mod == it->second.module)
		{
			FunctionHook search(func, NULL, NULL);

			for(size_t i=0; i < it->second.FunctionHooks.size(); i++)
			{
				auto found = std::lower_bound(it->second.FunctionHooks.begin(), it->second.FunctionHooks.end(), search);
				if(found != it->second.FunctionHooks.end() && !(search < *found))
				{
					if(found->origptr && *found->origptr == NULL)
						*found->origptr = (void *)GetProcAddress(mod, func);

					return (FARPROC)found->hookptr;
				}
			}
		}
	}

	return GetProcAddress(mod, func);
}

void Win32_IAT_BeginHooks()
{
	s_HookData = new CachedHookData;
	RDCASSERT(s_HookData->DllHooks.empty());
	s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(FunctionHook("LoadLibraryA", NULL, &Hooked_LoadLibraryA));
	s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(FunctionHook("LoadLibraryW", NULL, &Hooked_LoadLibraryW));
	s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(FunctionHook("LoadLibraryExA", NULL, &Hooked_LoadLibraryExA));
	s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(FunctionHook("LoadLibraryExW", NULL, &Hooked_LoadLibraryExW));
	s_HookData->DllHooks["kernel32.dll"].FunctionHooks.push_back(FunctionHook("GetProcAddress", NULL, &Hooked_GetProcAddress));
	
	GetModuleHandleEx(
		GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		(LPCTSTR)&s_HookData,
		&s_HookData->module);

	for(auto it=s_HookData->DllHooks.begin(); it != s_HookData->DllHooks.end(); ++it)
		for(size_t i=0; i < it->second.FunctionHooks.size(); i++)
			it->second.FunctionHooks[i].excludeModule = s_HookData->module;
}

// hook all functions for currently loaded modules.
// some of these hooks (as above) will hook LoadLibrary/GetProcAddress, to protect
void Win32_IAT_EndHooks()
{
	for(auto it=s_HookData->DllHooks.begin(); it != s_HookData->DllHooks.end(); ++it)
		std::sort(it->second.FunctionHooks.begin(), it->second.FunctionHooks.end());

	HANDLE hModuleSnap = INVALID_HANDLE_VALUE;

	// up to 10 retries
	for(int i=0; i < 10; i++)
	{
		hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId()); 

		if(hModuleSnap == INVALID_HANDLE_VALUE) 
		{
			DWORD err = GetLastError();

			RDCWARN("CreateToolhelp32Snapshot() -> 0x%08x", err);

			// retry if error is ERROR_BAD_LENGTH
			if(err == ERROR_BAD_LENGTH)
				continue;
		} 

		// didn't retry, or succeeded
		break;
	}
	
	if(hModuleSnap == INVALID_HANDLE_VALUE) 
	{
		RDCERR("Couldn't create toolhelp dump of modules in process");
		return;
	} 

#ifdef UNICODE
#undef MODULEENTRY32
#undef Module32First
#undef Module32Next
#endif
 
  MODULEENTRY32 me32; 
	RDCEraseEl(me32);
  me32.dwSize = sizeof(MODULEENTRY32); 
 
	BOOL success = Module32First(hModuleSnap, &me32);

  if(success == FALSE) 
  { 
		DWORD err = GetLastError();

		RDCERR("Couldn't get first module in process: 0x%08x", err);
    CloseHandle(hModuleSnap);
		return;
  }

	uintptr_t ret = 0;

	int numModules = 0;
 
  do
  {
		s_HookData->ApplyHooks(me32.szModule, me32.hModule);
  } while(ret == 0 && Module32Next(hModuleSnap, &me32)); 

  CloseHandle(hModuleSnap); 
}

bool Win32_IAT_Hook(void **orig_function_ptr, const char *module_name, const char *function, void *destination_function_ptr)
{
	if(!_stricmp(module_name, "kernel32.dll"))
	{
		if(!strcmp(function, "LoadLibraryA") ||
			!strcmp(function, "LoadLibraryW") ||
			!strcmp(function, "LoadLibraryExA") ||
			!strcmp(function, "LoadLibraryExW") ||
			!strcmp(function, "GetProcAddress"))
		{
			RDCERR("Cannot hook LoadLibrary* or GetProcAddress, as these are hooked internally");
			return false;
		}
	}
	s_HookData->DllHooks[strlower(string(module_name))].FunctionHooks.push_back(FunctionHook(function, orig_function_ptr, destination_function_ptr));
	return true;
}
