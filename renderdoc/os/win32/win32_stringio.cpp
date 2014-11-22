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

#include <time.h>
#include <stdio.h>
#include <string.h>

#include <shlobj.h>
#include <tchar.h>

#include <set>
using std::set;

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
		
		if(key >= eKey_A && key <= eKey_Z) vk = key;
		if(key >= eKey_0 && key <= eKey_9) vk = key;

		switch(key)
		{
			case eKey_Divide:    vk = VK_DIVIDE; break;
			case eKey_Multiply:  vk = VK_MULTIPLY; break;
			case eKey_Subtract:  vk = VK_SUBTRACT; break;
			case eKey_Plus:      vk = VK_ADD; break;
			case eKey_F1:        vk = VK_F1; break;
			case eKey_F2:        vk = VK_F2; break;
			case eKey_F3:        vk = VK_F3; break;
			case eKey_F4:        vk = VK_F4; break;
			case eKey_F5:        vk = VK_F5; break;
			case eKey_F6:        vk = VK_F6; break;
			case eKey_F7:        vk = VK_F7; break;
			case eKey_F8:        vk = VK_F8; break;
			case eKey_F9:        vk = VK_F9; break;
			case eKey_F10:       vk = VK_F10; break;
			case eKey_F11:       vk = VK_F11; break;
			case eKey_F12:       vk = VK_F12; break;
			case eKey_Home:      vk = VK_HOME; break;
			case eKey_End:       vk = VK_END; break;
			case eKey_Insert:    vk = VK_INSERT; break;
			case eKey_Delete:    vk = VK_DELETE; break;
			case eKey_PageUp:    vk = VK_PRIOR; break;
			case eKey_PageDn:    vk = VK_NEXT; break;
			case eKey_Backspace: vk = VK_BACK; break;
			case eKey_Tab:       vk = VK_TAB; break;
			case eKey_PrtScrn:   vk = VK_SNAPSHOT; break;
			case eKey_Pause:     vk = VK_PAUSE; break;
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
	void GetExecutableFilename(wstring &selfName)
	{
		wchar_t curFile[512] = {0};
		GetModuleFileNameW(NULL, curFile, 511);

		selfName = curFile;
	}

	void GetDefaultFiles(const wchar_t *logBaseName, wstring &capture_filename, wstring &logging_filename, wstring &target)
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

		target = mod;

		time_t t = time(NULL);
		tm now;
		localtime_s(&now, &t);

		wchar_t *filename_start = temp_filename+wcslen(temp_filename);

		wsprintf(filename_start, L"%ls_%04d.%02d.%02d_%02d.%02d.rdc", mod, 1900+now.tm_year, now.tm_mon+1, now.tm_mday, now.tm_hour, now.tm_min);

		capture_filename = temp_filename;

		*filename_start = 0;

		wsprintf(filename_start, L"%ls_%04d.%02d.%02d_%02d.%02d.%02d.log", logBaseName, 1900+now.tm_year, now.tm_mon+1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);

		logging_filename = temp_filename;
	}

	wstring GetAppFolderFilename(wstring filename)
	{
		PWSTR appDataPath;
		SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_SIMPLE_IDLIST|KF_FLAG_DONT_UNEXPAND, NULL, &appDataPath);
		wstring appdata = appDataPath;
		CoTaskMemFree(appDataPath);

		if(appdata[appdata.size()-1] == '/' || appdata[appdata.size()-1] == '\\') appdata.pop_back();

		appdata += L"\\renderdoc\\";

		CreateDirectoryW(appdata.c_str(), NULL);

		return appdata + filename;
	}

	uint64_t GetModifiedTimestamp(const wchar_t *filename)
	{
		struct _stat st;
		int res = _wstat(filename, &st);

		if(res == 0)
		{
			return (uint64_t)st.st_mtime;
		}
		
		return 0;
	}

	void CopyFileW(const wchar_t *from, const wchar_t *to, bool allowOverwrite)
	{
		::CopyFileW(from, to, allowOverwrite == false);
	}

	void UnlinkFileW(const wchar_t *path)
	{
		::DeleteFileW(path);
	}

	FILE *fopen(const wchar_t *filename, const wchar_t *mode)
	{
		FILE *ret = NULL;
		::_wfopen_s(&ret, filename, mode);
		return ret;
	}

	size_t fread(void *buf, size_t elementSize, size_t count, FILE *f) { return ::fread(buf, elementSize, count, f); }
	size_t fwrite(const void *buf, size_t elementSize, size_t count, FILE *f) { return ::fwrite(buf, elementSize, count, f); }
	int fprintf(FILE *f, const char *fmt, ...)
	{
		va_list args;
		va_start(args, fmt);

		int ret = ::vfprintf(f, fmt, args);

		va_end(args);

		return ret;
	}

	uint64_t ftell64(FILE *f) { return ::_ftelli64(f); }
	void fseek64(FILE *f, uint64_t offset, int origin) { ::_fseeki64(f, offset, origin); }

	int fclose(FILE *f) { return ::fclose(f); }
};

namespace StringFormat
{
	///////////////////////////////////////////////////////////////////////////
	int wsnprintf(wchar_t *str, size_t bufSize, const wchar_t *format, ...)
	{
		va_list args;
		va_start(args, format);

		int ret =  ::_vsnwprintf_s(str, bufSize, bufSize-1, format, args);

		va_end(args);

		return ret;
	}

	wstring WFmt(const wchar_t *format, ...)
	{
		va_list args;
		va_start(args, format);

		int size = _vscwprintf(format, args)+1;

		wchar_t *buf = new wchar_t[size];

		::vswprintf_s(buf, size, format, args);

		va_end(args);

		wstring ret = buf;

		delete[] buf;
		
		return ret;
	}
	///////////////////////////////////////////////////////////////////////////
	
	void sntimef(char *str, size_t bufSize, const char *format)
	{
		time_t tim;
		time(&tim);

		tm tmv;
		localtime_s(&tmv, &tim);

		strftime(str, bufSize, format, &tmv);
	}
		
	string Fmt(const char *format, ...)
	{
		va_list args;
		va_start(args, format);

		va_list args2;
		//va_copy(args2, args); // not implemented on VS2010
		args2 = args;

		int size = StringFormat::vsnprintf(NULL, 0, format, args2);

		char *buf = new char[size+1];
		buf[size] = 0;

		StringFormat::vsnprintf(buf, size, format, args);

		va_end(args);
		va_end(args2);

		string ret = buf;

		delete[] buf;
		
		return ret;
	}

	// save on reallocation, keep a persistent scratch buffer for conversions
	vector<char> charBuffer;
	vector<wchar_t> wcharBuffer;

	string Wide2UTF8(const wstring &s)
	{
		// include room for null terminator, assuming unicode input (not ucs)
		// utf-8 characters can be max 4 bytes. We can afford to be generous about
		// this length as we resize relatively rarely.
		size_t len = (s.length()+1)*4;

		if(charBuffer.size() < len)
			charBuffer.resize(len);

		int ret = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), -1, &charBuffer[0], (int)len, NULL, NULL);

		if(ret == 0)
		{
#if !defined(_RELEASE)
			RDCWARN("Failed to convert wstring: \"%ls\"", s.c_str());
#endif
			return "";
		}

		// convert to string from null-terminated string - utf-8 never contains
		// 0 bytes before the null terminator, and this way we don't care if
		// charBuffer is larger than the string
		return string(&charBuffer[0]);
	}

	wstring UTF82Wide(const string &s)
	{
		// Include room for null terminator. Since we're converting from utf-8,
		// worst case it's ascii and every byte is a character so length is the same
		size_t len = s.length()+1;
		
		if(wcharBuffer.size() < len)
			wcharBuffer.resize(len);

		int ret = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &wcharBuffer[0], (int)len);

		if(ret == 0)
		{
#if !defined(_RELEASE)
			RDCWARN("Failed to convert utf-8 string: \"%s\"", s.c_str());
#endif
			return L"";
		}

		return wstring(&wcharBuffer[0]);
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
