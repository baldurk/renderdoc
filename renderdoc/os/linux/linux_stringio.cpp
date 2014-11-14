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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include <iconv.h>

#include "common/string_utils.h"
#include "common/threading.h"
using std::string;

namespace Keyboard
{
	void Init()
	{
	}
	
	Display *CurrentXDisplay = NULL;

	void AddInputWindow(void *wnd)
	{
		// TODO check against this drawable & parent window being focused in GetKeyState
	}

	void RemoveInputWindow(void *wnd)
	{
	}

	bool GetKeyState(int key)
	{
		KeySym ks = 0;
		
		if(CurrentXDisplay == NULL) return false;

		if(key >= eKey_A && key <= eKey_Z) ks = key;
		if(key >= eKey_0 && key <= eKey_9) ks = key;
		
		switch(key)
		{
			case eKey_Divide:    ks = XK_KP_Divide; break;
			case eKey_Multiply:  ks = XK_KP_Multiply; break;
			case eKey_Subtract:  ks = XK_KP_Subtract; break;
			case eKey_Plus:      ks = XK_KP_Add; break;
			case eKey_F1:        ks = XK_F1; break;
			case eKey_F2:        ks = XK_F2; break;
			case eKey_F3:        ks = XK_F3; break;
			case eKey_F4:        ks = XK_F4; break;
			case eKey_F5:        ks = XK_F5; break;
			case eKey_F6:        ks = XK_F6; break;
			case eKey_F7:        ks = XK_F7; break;
			case eKey_F8:        ks = XK_F8; break;
			case eKey_F9:        ks = XK_F9; break;
			case eKey_F10:       ks = XK_F10; break;
			case eKey_F11:       ks = XK_F11; break;
			case eKey_F12:       ks = XK_F12; break;
			case eKey_Home:      ks = XK_Home; break;
			case eKey_End:       ks = XK_End; break;
			case eKey_Insert:    ks = XK_Insert; break;
			case eKey_Delete:    ks = XK_Delete; break;
			case eKey_PageUp:    ks = XK_Prior; break;
			case eKey_PageDn:    ks = XK_Next; break;
			case eKey_Backspace: ks = XK_BackSpace; break;
			case eKey_Tab:       ks = XK_Tab; break;
			case eKey_PrtScrn:   ks = XK_Print; break;
			case eKey_Pause:     ks = XK_Pause; break;
			default:
				break;
		}

		if(ks == 0)
			return false;
		
		KeyCode kc = XKeysymToKeycode(CurrentXDisplay, ks);
		
		char keyState[32];
		XQueryKeymap(CurrentXDisplay, keyState);
		
		int byteIdx = (kc/8);
		int bitMask = 1 << (kc%8);
		
		uint8_t keyByte = (uint8_t)keyState[byteIdx];
		
		return (keyByte & bitMask) != 0;
	}
}

namespace FileIO
{
	void GetExecutableFilename(wstring &selfName)
	{
		char path[512] = {0};
		readlink("/proc/self/exe", path, 511);

		selfName = widen(string(path));
	}

	void GetDefaultFiles(const wchar_t *logBaseName, wstring &capture_filename, wstring &logging_filename, wstring &target)
	{
		char path[512] = {0};
		readlink("/proc/self/exe", path, 511);
		const char *mod = strrchr(path, '/');
		if(mod == NULL)
			mod = "unknown";
		else
			mod++;

		target = widen(string(mod));

		time_t t = time(NULL);
		tm now = *localtime(&t);

		char temp_filename[512] = {0};

		snprintf(temp_filename, 511, "/tmp/%s_%04d.%02d.%02d_%02d.%02d.rdc", mod, 1900+now.tm_year, now.tm_mon+1, now.tm_mday, now.tm_hour, now.tm_min);

		capture_filename = widen(string(temp_filename));

		string baseName = narrow(wstring(logBaseName));

		snprintf(temp_filename, 511, "/tmp/%s_%04d.%02d.%02d_%02d.%02d.%02d.log", baseName.c_str(), 1900+now.tm_year, now.tm_mon+1, now.tm_mday, now.tm_hour, now.tm_min, now.tm_sec);

		logging_filename = widen(string(temp_filename));
	}
	
	uint64_t GetModifiedTimestamp(const wchar_t *filename)
	{
		string fn = narrow(wstring(filename));

		struct ::stat st;
		int res = stat(fn.c_str(), &st);

		if(res == 0)
		{
			return (uint64_t)st.st_mtim.tv_sec;
		}
		
		return 0;
	}

	void CopyFileW(const wchar_t *from, const wchar_t *to, bool allowOverwrite)
	{
		if(from[0] == 0 || to[0] == 0)
			return;

		RDCUNIMPLEMENTED();
	}

	void UnlinkFileW(const wchar_t *path)
	{
		string fn = narrow(wstring(path));

		unlink(fn.c_str());
	}

	FILE *fopen(const wchar_t *filename, const wchar_t *mode)
	{
		string fn = narrow(wstring(filename));

		char m[5];

		for(int i=0; i < 5; i++)
		{
			if(!mode[i]) break;
			m[i] = (char)mode[i];
		}

		return ::fopen(fn.c_str(), m);
	}

	size_t fread(void *buf, size_t elementSize, size_t count, FILE *f) { return ::fread(buf, elementSize, count, f); }
	size_t fwrite(const void *buf, size_t elementSize, size_t count, FILE *f) { return ::fwrite(buf, elementSize, count, f); }

	uint64_t ftell64(FILE *f) { return (uint64_t)::ftell(f); }
	void fseek64(FILE *f, uint64_t offset, int origin) { ::fseek(f, (long)offset, origin); }

	int fclose(FILE *f) { return ::fclose(f); }
};

namespace StringFormat
{
	int snprintf(char *str, size_t bufSize, const char *fmt, ...)
	{
		va_list args;
		va_start(args, fmt);

		int ret = vsnprintf(str, bufSize, fmt, args);

		va_end(args);

		return ret;
	}

	int vsnprintf(char *str, size_t bufSize, const char *format, va_list args)
	{
		return ::vsnprintf(str, bufSize, format, args);
	}
	
	int wsnprintf(wchar_t *str, size_t bufSize, const wchar_t *fmt, ...)
	{
		va_list args;
		va_start(args, fmt);

		int ret = vswprintf(str, bufSize, fmt, args);

		va_end(args);

		return ret;
	}

	void sntimef(char *str, size_t bufSize, const char *format)
	{
		time_t tim;
		time(&tim);

		tm *tmv = localtime(&tim);

		strftime(str, bufSize, format, tmv);
	}

	void wcsncpy(wchar_t *dst, const wchar_t *src, size_t count)
	{
		::wcsncpy(dst, src, count);
	}

	string Fmt(const char *format, ...)
	{
		va_list args;
		va_start(args, format);

		int size = ::vsnprintf(NULL, 0, format, args)+1;

		va_end(args);
		va_start(args, format);

		char *buf = new char[size];

		StringFormat::vsnprintf(buf, size, format, args);

		va_end(args);

		string ret = buf;

		delete[] buf;
		
		return ret;
	}

	wstring WFmt(const wchar_t *format, ...)
	{
		va_list args;
		va_start(args, format);

		FILE *f = ::fopen("/dev/null", "wb");
		
		int size = ::vfwprintf(f, format, args)+1;
		
		fclose(f);
		
		va_end(args);
		va_start(args, format);

		wchar_t *buf = new wchar_t[size];

		::vswprintf(buf, size, format, args);

		va_end(args);

		wstring ret = buf;

		delete[] buf;
		
		return ret;
	}

	// save on reallocation, keep a persistent scratch buffer for conversions
	vector<char> charBuffer;
	vector<wchar_t> wcharBuffer;

	// cache iconv_t descriptors to save on iconv_open/iconv_close each time
	iconv_t iconvWide2UTF8 = (iconv_t)-1;
	iconv_t iconvUTF82Wide = (iconv_t)-1;

	// iconv is not thread safe when sharing an iconv_t descriptor
	// I don't expect much contention but if it happens we could TryLock
	// before creating a temporary iconv_t, or hold two iconv_ts, or something.
	Threading::CriticalSection lockWide2UTF8, lockUTF82Wide;

	string Wide2UTF8(const wstring &s)
	{
		// include room for null terminator, assuming unicode input (not ucs)
		// utf-8 characters can be max 4 bytes. We can afford to be generous about
		// this length as we resize relatively rarely.
		size_t len = (s.length()+1)*4;

		if(charBuffer.size() < len)
			charBuffer.resize(len);

		size_t ret;

		{
			SCOPED_LOCK(lockWide2UTF8);

			if(iconvWide2UTF8 == (iconv_t)-1)
				iconvWide2UTF8 = iconv_open("UTF-8", "WCHAR_T");

			if(iconvWide2UTF8 == (iconv_t)-1)
			{
				RDCERR("Couldn't open iconv for WCHAR_T to UTF-8: %d", errno);
				return "";
			}

			char *inbuf = (char *)s.c_str();
			size_t insize = (s.length()+1)*sizeof(wchar_t); // include null terminator
			char *outbuf = &charBuffer[0];
			size_t outsize = len;

			ret = iconv(iconvWide2UTF8, &inbuf, &insize, &outbuf, &outsize);
		}

		if(ret == (size_t)-1)
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

		size_t ret;

		{
			SCOPED_LOCK(lockUTF82Wide);

			if(iconvUTF82Wide == (iconv_t)-1)
				iconvUTF82Wide = iconv_open("WCHAR_T", "UTF-8");

			if(iconvUTF82Wide == (iconv_t)-1)
			{
				RDCERR("Couldn't open iconv for UTF-8 to WCHAR_T: %d", errno);
				return L"";
			}

			char *inbuf = (char *)s.c_str();
			size_t insize = s.length()+1; // include null terminator
			char *outbuf = (char *)&wcharBuffer[0];
			size_t outsize = len*sizeof(wchar_t);

			ret = iconv(iconvUTF82Wide, &inbuf, &insize, &outbuf, &outsize);
		}

		if(ret == (size_t)-1)
		{
#if !defined(_RELEASE)
			RDCWARN("Failed to convert utf-8 string: \"%s\"", s.c_str());
#endif
			return L"";
		}

		return wstring(&wcharBuffer[0]);
	}
};
