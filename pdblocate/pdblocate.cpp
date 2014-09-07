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

#define USE_DIA 1

#include <windows.h>
#include <string>

#include <stdio.h>
#include <stdint.h>
#include <wchar.h>
#include <tchar.h>

#include <assert.h>

#include <dbghelp.h>
#include <shlobj.h>

#include <vector>
using std::vector;
using std::wstring;

#if USE_DIA
// if you don't have dia2.h (only included with VS pro and above)
// set USE_DIA to 1. Everything will compile but symbol resolution
// for callstacks will not work.
// If you need this, you could always drop in a pre-compiled pdblocate
// from a normal build as this file/project changes very rarely.
#include <dia2.h>
#include <atlconv.h>

// must match definition in callstack.h
struct AddrInfo
{
	wchar_t funcName[127];
	wchar_t fileName[127];
	unsigned long lineNum;
};

struct Module
{
	Module(IDiaDataSource* src, IDiaSession* sess) :
		pSource(src), pSession(sess) {}

	IDiaDataSource* pSource;
	IDiaSession* pSession;
};

vector<Module> modules;

typedef BOOL (CALLBACK *PFINDFILEINPATHCALLBACKW)(PCWSTR, PVOID);

typedef BOOL (WINAPI *PSYMINITIALIZEW)(HANDLE, PCWSTR, BOOL);
typedef BOOL (WINAPI *PSYMFINDFILEINPATHW)(HANDLE, PCWSTR, PCWSTR, PVOID, DWORD, DWORD, DWORD, PWSTR, PFINDFILEINPATHCALLBACKW, PVOID);

PSYMINITIALIZEW dynSymInitializeW = NULL;
PSYMFINDFILEINPATHW dynSymFindFileInPathW = NULL;

wstring GetSymSearchPath()
{
	PWSTR appDataPath;
	SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_SIMPLE_IDLIST|KF_FLAG_DONT_UNEXPAND, NULL, &appDataPath);
	wstring appdata = appDataPath;
	CoTaskMemFree(appDataPath);

	wstring sympath = L".;";
	sympath += appdata;
	sympath += L"\\renderdoc\\symbols;SRV*";
	sympath += appdata;
	sympath += L"\\renderdoc\\symbols\\symsrv*http://msdl.microsoft.com/download/symbols";

	return sympath;
}

wstring LookupModule(wstring moduleDetails)
{
	uint32_t params[12];
	int charsRead = 0;
	swscanf_s(moduleDetails.c_str(), L"%d  %d %d %d  %d %d %d %d %d %d %d %d%n",
		&params[0], &params[1], &params[2], &params[3], &params[4], &params[5],
		&params[6], &params[7], &params[8], &params[9], &params[10], &params[11], &charsRead);

	wchar_t *modName = (wchar_t *)moduleDetails.c_str() + charsRead + 1;

	while(*modName != L'\0' && iswspace(*modName)) modName++;

	DWORD age = params[0];
	GUID guid;
	guid.Data1 = params[1];
	guid.Data2 = params[2];
	guid.Data3 = params[3];
	guid.Data4[0] = params[4];
	guid.Data4[1] = params[5];
	guid.Data4[2] = params[6];
	guid.Data4[3] = params[7];
	guid.Data4[4] = params[8];
	guid.Data4[5] = params[9];
	guid.Data4[6] = params[10];
	guid.Data4[7] = params[11];

	wchar_t *pdbName = modName;

	if(wcsrchr(pdbName, L'\\'))
		pdbName = wcsrchr(pdbName, L'\\')+1;

	if(wcsrchr(pdbName, L'/'))
		pdbName = wcsrchr(pdbName, L'/')+1;
	
	if(wcsstr(pdbName, L".pdb") == NULL &&
		wcsstr(pdbName, L".PDB") == NULL)
	{
		wchar_t *ext = wcsrchr(pdbName, L'.');

		if(ext)
		{
			ext[1] = L'p';
			ext[2] = L'd';
			ext[3] = L'b';
		}
	}
	
	wstring ret = modName;

	if(dynSymFindFileInPathW != NULL)
	{
		wstring sympath = GetSymSearchPath();

		wchar_t path[MAX_PATH] = {0};
		BOOL found = dynSymFindFileInPathW(GetCurrentProcess(), sympath.c_str(), pdbName, &guid, age, 0, SSRVOPT_GUIDPTR, path, NULL, NULL);
		DWORD err = GetLastError();

		if(found == TRUE && path[0] != 0)
			ret = path;
	}

	return ret;
}

uint32_t GetModule(wstring moduleDetails)
{
	uint32_t params[12];
	int charsRead = 0;
	swscanf_s(moduleDetails.c_str(), L"%d  %d %d %d  %d %d %d %d %d %d %d %d%n",
		&params[0], &params[1], &params[2], &params[3], &params[4], &params[5],
		&params[6], &params[7], &params[8], &params[9], &params[10], &params[11], &charsRead);

	wchar_t *pdbName = (wchar_t *)moduleDetails.c_str() + charsRead + 1;

	while(*pdbName != L'\0' && iswspace(*pdbName)) pdbName++;

	DWORD age = params[0];
	GUID guid;
	guid.Data1 = params[1];
	guid.Data2 = params[2];
	guid.Data3 = params[3];
	guid.Data4[0] = params[4];
	guid.Data4[1] = params[5];
	guid.Data4[2] = params[6];
	guid.Data4[3] = params[7];
	guid.Data4[4] = params[8];
	guid.Data4[5] = params[9];
	guid.Data4[6] = params[10];
	guid.Data4[7] = params[11];
	
	USES_CONVERSION;
	LPCOLESTR fName = W2COLE(pdbName);

	Module m(NULL, NULL);
	
	CoCreateInstance(__uuidof(DiaSource), NULL, CLSCTX_INPROC_SERVER, __uuidof(IDiaDataSource), (void **)&m.pSource);

	HRESULT hr = S_OK;

	// check this pdb is the one we expected from our chunk
	if(guid.Data1 == 0 && guid.Data2 == 0)
	{
		hr = m.pSource->loadDataFromPdb( fName );
	}
	else
	{
		hr = m.pSource->loadAndValidateDataFromPdb( fName, &guid, 0, age);
	}

	if(SUCCEEDED(hr))
	{
		// open the session
		hr = m.pSource->openSession( &m.pSession );
		if (FAILED(hr))
		{
			m.pSource->Release();
			return 0;
		}

		modules.push_back(m);

		return modules.size()-1;
	}
	
	m.pSource->Release();

	return 0;
}

void SetBaseAddress(wstring req)
{
	uint32_t module;
	uint64_t addr;
	int charsRead = swscanf_s(req.c_str(), L"%d %llu", &module, &addr);

	if(module > 0 && module < modules.size())
		modules[module].pSession->put_loadAddress(addr);
}

AddrInfo GetAddr(wstring req)
{
	uint32_t module;
	uint64_t addr;
	int charsRead = swscanf_s(req.c_str(), L"%d %llu", &module, &addr);

	AddrInfo ret;
	ZeroMemory(&ret, sizeof(ret));

	if(module > 0 && module < modules.size())
	{
		IDiaSymbol* pFunc = NULL;
		HRESULT hr = modules[module].pSession->findSymbolByVA( addr, SymTagFunction, &pFunc );

		if(hr != S_OK)
		{
			if(pFunc) pFunc->Release();
			return ret;
		}

		DWORD opts = 0;
		opts |= UNDNAME_NO_LEADING_UNDERSCORES;
		opts |= UNDNAME_NO_MS_KEYWORDS;
		opts |= UNDNAME_NO_FUNCTION_RETURNS;
		opts |= UNDNAME_NO_ALLOCATION_MODEL;
		opts |= UNDNAME_NO_ALLOCATION_LANGUAGE;
		opts |= UNDNAME_NO_THISTYPE;
		opts |= UNDNAME_NO_ACCESS_SPECIFIERS;
		opts |= UNDNAME_NO_THROW_SIGNATURES;
		opts |= UNDNAME_NO_MEMBER_TYPE;
		opts |= UNDNAME_NO_RETURN_UDT_MODEL;
		opts |= UNDNAME_32_BIT_DECODE;
		opts |= UNDNAME_NO_LEADING_UNDERSCORES;

		// first try undecorated name
		BSTR file;
		hr = pFunc->get_undecoratedNameEx(opts, &file);

		// if not, just try name
		if(hr != S_OK)
		{
			hr = pFunc->get_name(&file);

			if(hr != S_OK)
			{
				pFunc->Release();
				return ret;
			}

			wcsncpy_s(ret.funcName, file, 126);
		}
		else
		{
			wcsncpy_s(ret.funcName, file, 126);

			// remove stupid (void) for empty parameters
			if(wcsstr(ret.funcName, L"(void)"))
			{
				wchar_t *a = wcsstr(ret.funcName, L"(void)");
				*(a+1) = L')';
				*(a+2) = 0;
			}
		}

		pFunc->Release();
		pFunc = NULL;

		SysFreeString(file);

		// find the line numbers touched by this address.
		IDiaEnumLineNumbers* lines = NULL;
		hr = modules[module].pSession->findLinesByVA(addr, DWORD(4), &lines);
		if(FAILED(hr))
		{
			if(lines) lines->Release();
			return ret;
		}

		IDiaLineNumber* line = NULL;
		ULONG count = 0;

		// just take the first one
		if(SUCCEEDED(lines->Next(1, &line, &count)) && count == 1)
		{
			IDiaSourceFile *dia_source = NULL;
			hr = line->get_sourceFile(&dia_source);
			if(FAILED(hr))
			{
				line->Release();
				lines->Release();
				if(dia_source) dia_source->Release();
				return ret;
			}

			hr = dia_source->get_fileName(&file);
			if(FAILED(hr))
			{
				line->Release();
				lines->Release();
				dia_source->Release();
				return ret;
			}

			wcsncpy_s(ret.fileName, file, 126);

			SysFreeString(file);

			dia_source->Release();
			dia_source = NULL;

			DWORD line_num = 0;
			hr = line->get_lineNumber(&line_num);
			if(FAILED(hr))
			{
				line->Release();
				lines->Release();
				return ret;
			}

			ret.lineNum = line_num;

			line->Release();
		}

		lines->Release();
	}

	return ret;
}

wstring HandleRequest(wstring req)
{
	size_t idx = req.find(L' ');

	if(idx == wstring::npos)
		return L".";

	wstring type = req.substr(0, idx);
	wstring payload = req.substr(idx+1);

	if(type == L"lookup")
		return LookupModule(payload);

	if(type == L"baseaddr")
	{
		SetBaseAddress(payload);
		return L".";
	}

	if(type == L"getmodule")
	{
		wstring ret;
		ret.resize(4);

		uint32_t *output = (uint32_t *)&ret[0];

		*output = GetModule(payload);

		return ret;
	}

	if(type == L"getaddr")
	{
		wstring ret;
		ret.resize(sizeof(AddrInfo)/sizeof(wchar_t));

		AddrInfo info = GetAddr(payload);

		memcpy(&ret[0], &info, sizeof(AddrInfo));

		return ret;
	}
	
	return L".";
}

int WINAPI wWinMain( __in HINSTANCE hInstance, __in_opt HINSTANCE hPrevInstance, __in LPWSTR lpCmdLine, __in int nShowCmd )
{
	modules.push_back(Module(NULL, NULL));

	// CreatePipe
	HANDLE pipe = CreateNamedPipeW( L"\\\\.\\pipe\\RenderDoc.pdblocate", PIPE_ACCESS_DUPLEX,
									PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
									1, 1024, 1024, 0, NULL);

	if(pipe == INVALID_HANDLE_VALUE)
		return 1;

	BOOL connected = ConnectNamedPipe(pipe, NULL);
	if(!connected && GetLastError() == ERROR_PIPE_CONNECTED)
		connected = true;

	if(!connected)
	{
		CloseHandle(pipe);
		return 1;
	}
		
	CoInitialize(NULL);
	
	HMODULE mod = LoadLibraryW(L"x86/dbghelp.dll");

	if(mod != NULL)
	{
		dynSymInitializeW = (PSYMINITIALIZEW)GetProcAddress(mod, "SymInitializeW");
		dynSymFindFileInPathW = (PSYMFINDFILEINPATHW)GetProcAddress(mod, "SymFindFileInPathW");
		
		wstring sympath = GetSymSearchPath();

		if(dynSymInitializeW != NULL)
		{
			dynSymInitializeW(GetCurrentProcess(), sympath.c_str(), TRUE);
		}
	}

	wchar_t buf[1024];

	while(true)
	{
		DWORD read = 0;
		BOOL success = ReadFile(pipe, buf, 1024, &read, NULL);

		if(!success || read == 0)
		{
			DWORD err = GetLastError();
			break;
		}

		wstring request(buf, buf+read/sizeof(wchar_t));
		if(request.back() != L'\0')
			request.push_back(L'\0');

		wstring reply = HandleRequest(request);

		reply.push_back(L'\0');

		DWORD msglen = reply.length()*sizeof(wchar_t);

		DWORD written = 0;
		success = WriteFile(pipe, reply.c_str(), msglen, &written, NULL);

		if(!success || written != msglen)
		{
			DWORD err = GetLastError();
			break;
		}
	}

	CloseHandle(pipe);
	return 0;
}
#else
int WINAPI wWinMain(__in HINSTANCE hInstance, __in_opt HINSTANCE hPrevInstance, __in LPWSTR lpCmdLine, __in int nShowCmd)
{
	return 2;
}
#endif
