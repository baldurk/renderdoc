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
#include "api/app/renderdoc_app.h"
#include "serialise/string_utils.h"

#include <time.h>
#include <stdio.h>
#include <string.h>

#include <shlobj.h>
#include <tchar.h>

#include <set>
using std::set;
using std::wstring;

// gives us an address to identify this dll with
static int dllLocator=0;

string GetEmbeddedResourceWin32(int resource)
{
	HMODULE mod = NULL;
	GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,	(const char *)&dllLocator, &mod);

	HRSRC res = FindResource(mod, MAKEINTRESOURCE(resource), MAKEINTRESOURCE(TYPE_EMBED));
	
	if(res == NULL)
	{
		RDCFATAL("Couldn't find embedded win32 resource");
	}

	HGLOBAL data = LoadResource(mod, res);

	if(data != NULL)
	{
		DWORD resSize = SizeofResource(mod, res);
		const char* resData = (const char*)LockResource(data);

		if(resData)
			return string(resData, resData+resSize);
	}

	return "";
}

namespace Keyboard
{
	void Init()
	{
	}

	set<HWND> inputWindows;

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
		
		if(key >= eRENDERDOC_Key_A && key <= eRENDERDOC_Key_Z) vk = key;
		if(key >= eRENDERDOC_Key_0 && key <= eRENDERDOC_Key_9) vk = key;

		switch(key)
		{
			case eRENDERDOC_Key_Divide:    vk = VK_DIVIDE; break;
			case eRENDERDOC_Key_Multiply:  vk = VK_MULTIPLY; break;
			case eRENDERDOC_Key_Subtract:  vk = VK_SUBTRACT; break;
			case eRENDERDOC_Key_Plus:      vk = VK_ADD; break;
			case eRENDERDOC_Key_F1:        vk = VK_F1; break;
			case eRENDERDOC_Key_F2:        vk = VK_F2; break;
			case eRENDERDOC_Key_F3:        vk = VK_F3; break;
			case eRENDERDOC_Key_F4:        vk = VK_F4; break;
			case eRENDERDOC_Key_F5:        vk = VK_F5; break;
			case eRENDERDOC_Key_F6:        vk = VK_F6; break;
			case eRENDERDOC_Key_F7:        vk = VK_F7; break;
			case eRENDERDOC_Key_F8:        vk = VK_F8; break;
			case eRENDERDOC_Key_F9:        vk = VK_F9; break;
			case eRENDERDOC_Key_F10:       vk = VK_F10; break;
			case eRENDERDOC_Key_F11:       vk = VK_F11; break;
			case eRENDERDOC_Key_F12:       vk = VK_F12; break;
			case eRENDERDOC_Key_Home:      vk = VK_HOME; break;
			case eRENDERDOC_Key_End:       vk = VK_END; break;
			case eRENDERDOC_Key_Insert:    vk = VK_INSERT; break;
			case eRENDERDOC_Key_Delete:    vk = VK_DELETE; break;
			case eRENDERDOC_Key_PageUp:    vk = VK_PRIOR; break;
			case eRENDERDOC_Key_PageDn:    vk = VK_NEXT; break;
			case eRENDERDOC_Key_Backspace: vk = VK_BACK; break;
			case eRENDERDOC_Key_Tab:       vk = VK_TAB; break;
			case eRENDERDOC_Key_PrtScrn:   vk = VK_SNAPSHOT; break;
			case eRENDERDOC_Key_Pause:     vk = VK_PAUSE; break;
			default:
				break;
		}

		if(vk == 0)
			return false;
		
		bool keydown = GetAsyncKeyState(vk) != 0;

		if(inputWindows.empty() || !keydown)
			return keydown;

		for(auto it=inputWindows.begin(); it != inputWindows.end(); ++it)
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
	void GetExecutableFilename(string &selfName)
	{
		wchar_t curFile[512] = {0};
		GetModuleFileNameW(NULL, curFile, 511);

		selfName = StringFormat::Wide2UTF8(wstring(curFile));
	}

	string GetFullPathname(const string &filename)
	{
		wstring wfn = StringFormat::UTF82Wide(filename);

		wchar_t path[512] = {0};
		GetFullPathNameW(wfn.c_str(), ARRAY_COUNT(path)-1, path, NULL);

		return StringFormat::Wide2UTF8(wstring(path));
	}

	void CreateParentDirectory(const string &filename)
	{
		wstring wfn = StringFormat::UTF82Wide(filename);

		wfn = dirname(wfn);

		// This function needs \\s not /s. So stupid!
		for(size_t i=0; i < wfn.size(); i++)
			if(wfn[i] == L'/')
				wfn[i] = L'\\';

		SHCreateDirectoryExW(NULL, wfn.c_str(), NULL);
	}

	string GetReplayAppFilename()
	{
		HMODULE hModule = NULL;
		GetModuleHandleEx(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCTSTR)&dllLocator,
			&hModule);
		wchar_t curFile[512] = {0};
		GetModuleFileNameW(hModule, curFile, 511);

		string path = StringFormat::Wide2UTF8(wstring(curFile));
		path = dirname(path);
		string exe = path + "/renderdocui.exe";

		FILE *f = FileIO::fopen(exe.c_str(), "rb");
		if(f)
		{
			FileIO::fclose(f);
			return exe;
		}

		// if renderdocui.exe doesn't live in the same dir, we must be in x86/
		// so look one up the tree.
		exe = path + "/../renderdocui.exe";

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
		RegGetValueW(HKEY_CLASSES_ROOT, L"RenderDoc.RDCCapture.1\\DefaultIcon", NULL, RRF_RT_ANY,
			&type, (void *)curFile, &dataSize);

		if(type == REG_EXPAND_SZ || type == REG_SZ)
		{
			return StringFormat::Wide2UTF8(wstring(curFile));
		}

		return "";
	}

	void GetDefaultFiles(const char *logBaseName, string &capture_filename, string &logging_filename, string &target)
	{
		wchar_t temp_filename[MAX_PATH];

		GetTempPathW(MAX_PATH, temp_filename);

		wchar_t curFile[512];
		GetModuleFileNameW(NULL, curFile, 512);

		wchar_t fn[MAX_PATH];
		wcscpy_s(fn, MAX_PATH, curFile);

		wchar_t *mod = wcsrchr(fn, L'.');
		if(mod) *mod = 0;
		mod = wcsrchr(fn, L'/');
		if(!mod) mod = fn;
		mod = wcsrchr(mod, L'\\');

		mod++; // now points to base filename without extension

		target = StringFormat::Wide2UTF8(wstring(mod));

		time_t t = time(NULL);
		tm now;
		localtime_s(&now, &t);

		wchar_t *filename_start = temp_filename+wcslen(temp_filename);

		wsprintf(filename_start, L"%ls_%04d.%02d.%02d_%02d.%02d.rdc", mod, 1900+now.tm_year, now.tm_mon+1, now.tm_mday, now.tm_hour, now.tm_min);

		capture_filename = StringFormat::Wide2UTF8(wstring(temp_filename));

		*filename_start = 0;

		wstring wbase = StringFormat::UTF82Wide(string(logBaseName));

		wsprintf(filename_start, L"%ls_%04d.%02d.%02d_%02d.%02d.%02d.log", wbase.c_str(), 1900+now.tm_year, now.tm_mon+1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);

		logging_filename = StringFormat::Wide2UTF8(wstring(temp_filename));
	}

	string GetAppFolderFilename(const string &filename)
	{
		PWSTR appDataPath;
		SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_SIMPLE_IDLIST|KF_FLAG_DONT_UNEXPAND, NULL, &appDataPath);
		wstring appdata = appDataPath;
		CoTaskMemFree(appDataPath);

		if(appdata[appdata.size()-1] == '/' || appdata[appdata.size()-1] == '\\') appdata.pop_back();

		appdata += L"\\renderdoc\\";

		CreateDirectoryW(appdata.c_str(), NULL);

		string ret = StringFormat::Wide2UTF8(appdata) + filename;
		return ret;
	}

	uint64_t GetModifiedTimestamp(const string &filename)
	{
		wstring wfn = StringFormat::UTF82Wide(filename);

		struct _stat st;
		int res = _wstat(wfn.c_str(), &st);

		if(res == 0)
		{
			return (uint64_t)st.st_mtime;
		}
		
		return 0;
	}

	void Copy(const char *from, const char *to, bool allowOverwrite)
	{
		wstring wfrom = StringFormat::UTF82Wide(string(from));
		wstring wto = StringFormat::UTF82Wide(string(to));

		::CopyFileW(wfrom.c_str(), wto.c_str(), allowOverwrite == false);
	}

	void Delete(const char *path)
	{
		wstring wpath = StringFormat::UTF82Wide(string(path));
		::DeleteFileW(wpath.c_str());
	}

	FILE *fopen(const char *filename, const char *mode)
	{
		wstring wfn = StringFormat::UTF82Wide(string(filename));
		wstring wmode = StringFormat::UTF82Wide(string(mode));

		FILE *ret = NULL;
		::_wfopen_s(&ret, wfn.c_str(), wmode.c_str());
		return ret;
	}

	size_t fread(void *buf, size_t elementSize, size_t count, FILE *f) { return ::fread(buf, elementSize, count, f); }
	size_t fwrite(const void *buf, size_t elementSize, size_t count, FILE *f) { return ::fwrite(buf, elementSize, count, f); }

	uint64_t ftell64(FILE *f) { return ::_ftelli64(f); }
	void fseek64(FILE *f, uint64_t offset, int origin) { ::_fseeki64(f, offset, origin); }

	bool feof(FILE *f) { return ::feof(f) != 0; }

	int fclose(FILE *f) { return ::fclose(f); }
};

namespace StringFormat
{
	void sntimef(char *str, size_t bufSize, const char *format)
	{
		time_t tim;
		time(&tim);

		tm tmv;
		localtime_s(&tmv, &tim);

		wchar_t *buf = new wchar_t[bufSize+1]; buf[bufSize] = 0;
		wstring wfmt = StringFormat::UTF82Wide(string(format));

		wcsftime(buf, bufSize, wfmt.c_str(), &tmv);

		string result = StringFormat::Wide2UTF8(wstring(buf));

		delete[] buf;

		if(result.length()+1 < bufSize)
		{
			memcpy(str, result.c_str(), result.length());
			str[result.length()] = 0;
		}
	}
		
	// this function is only platform specific because va_copy isn't implemented
	// on MSVC
	string Fmt(const char *format, ...)
	{
		va_list args;
		va_start(args, format);

		va_list args2;
		//va_copy(args2, args); // not implemented on VS2010
		args2 = args;

		int size = StringFormat::vsnprintf(NULL, 0, format, args2);

		char *buf = new char[size+1];
		StringFormat::vsnprintf(buf, size+1, format, args);
		buf[size] = 0;

		va_end(args);
		va_end(args2);

		string ret = buf;

		delete[] buf;
		
		return ret;
	}

	string Wide2UTF8(const wstring &s)
	{
		int bytes_required = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, NULL, 0, NULL, NULL);

		if(bytes_required == 0)
			return "";

		string ret;
		ret.resize(bytes_required);

		int res = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &ret[0], bytes_required, NULL, NULL);

		if(ret.back() == 0) ret.pop_back();

		if(res == 0)
		{
#if !defined(_RELEASE)
			RDCWARN("Failed to convert wstring"); // can't pass string through as this would infinitely recurse
#endif
			return "";
		}

		return ret;
	}

	wstring UTF82Wide(const string &s)
	{
		int chars_required = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);

		if(chars_required == 0)
			return L"";
		
		wstring ret;
		ret.resize(chars_required);

		int res = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ret[0], chars_required);

		if(ret.back() == 0) ret.pop_back();

		if(res == 0)
		{
#if !defined(_RELEASE)
			RDCWARN("Failed to convert utf-8 string"); // can't pass string through as this would infinitely recurse
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
		wstring wstr = StringFormat::UTF82Wide(string(str));

		if(channel == OSUtility::Output_DebugMon)
			OutputDebugStringW(wstr.c_str());
		else if(channel == OSUtility::Output_StdOut)
			fwprintf(stdout, L"%ls", wstr.c_str());
		else if(channel == OSUtility::Output_StdErr)
			fwprintf(stderr, L"%ls", wstr.c_str());
	}
};
