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

// this file defines several 'interfaces' that are then implemented by conditionally compiling in
// the platform's specific implementation
//
// Anything that won't compile on all platforms MUST be wrapped and specified in this file, so
// that we isolate any OS-specific code to one place that can just be swapped out easily.

#pragma once

#include "common/common.h"

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

#include <string>
#include <vector>
using std::string;
using std::wstring;
using std::vector;

struct CaptureOptions;

namespace Process
{
	void StartGlobalHook(const wchar_t *pathmatch, const wchar_t *logfile, const CaptureOptions *opts);
	uint32_t InjectIntoProcess(uint32_t pid, const wchar_t *logfile, const CaptureOptions *opts, bool waitForExit);
	uint32_t CreateAndInjectIntoProcess(const wchar_t *app, const wchar_t *workingDir, const wchar_t *cmdLine,
										const wchar_t *logfile, const CaptureOptions *opts, bool waitForExit);
	void *GetFunctionAddress(const char *module, const char *function);
	uint32_t GetCurrentPID();
};

namespace Timing
{
	double GetTickFrequency();
	uint64_t GetTick();
	uint64_t GetUnixTimestamp();
};

namespace Threading
{
	template<class data>
	class CriticalSectionTemplate
	{
		public:
			CriticalSectionTemplate();
			~CriticalSectionTemplate();
			void Lock();
			bool Trylock();
			void Unlock();

		private:
			// no copying
			CriticalSectionTemplate &operator =(const CriticalSectionTemplate &other);
			CriticalSectionTemplate(const CriticalSectionTemplate &other);

			data m_Data;
	};

	// must typedef CriticalSectionTemplate<X> CriticalSection

	typedef void (*ThreadEntry)(void *);
	typedef uint64_t ThreadHandle;
	ThreadHandle CreateThread(ThreadEntry entryFunc, void *userData);
	uint64_t GetCurrentID();
	void JoinThread(ThreadHandle handle);
	void CloseThread(ThreadHandle handle);
	void Sleep(uint32_t milliseconds);

	// kind of windows specific, to handle this case:
	// http://blogs.msdn.com/b/oldnewthing/archive/2013/11/05/10463645.aspx
	void KeepModuleAlive();
	void ReleaseModuleExitThread();
};

namespace Network
{
	class Socket
	{
		public:
			Socket(ptrdiff_t s) : socket(s) {}
			~Socket();
			void Shutdown();

			bool Connected() const;

			Socket *AcceptClient(bool wait);

			bool IsRecvDataWaiting();

			bool SendDataBlocking(const void *buf, uint32_t length);
			bool RecvDataBlocking(void *data, uint32_t length);
		private:
			ptrdiff_t socket;
	};

	Socket *CreateServerSocket(const char *addr, uint16_t port, int queuesize);
	Socket *CreateClientSocket(const wchar_t *host, uint16_t port, int timeoutMS);
	
	void Init();
	void Shutdown();
};

namespace Atomic
{
	int32_t Inc32(volatile int32_t *i);
	int64_t Inc64(volatile int64_t *i);
	int64_t Dec64(volatile int64_t *i);
	int64_t ExchAdd64(volatile int64_t *i, int64_t a);
};

namespace Callstack
{
	class Stackwalk
	{
	public:
		virtual ~Stackwalk() {}
		virtual size_t NumLevels() const = 0;
		virtual uint64_t *GetAddrs() const = 0;
	};

	struct AddressDetails
	{
		AddressDetails() : line(0) {}

		wstring function;
		wstring filename;
		uint32_t line;

		wstring formattedString(const char *commonPath = NULL);
	};

	class StackResolver
	{
	public:
		virtual ~StackResolver() {}
		virtual AddressDetails GetAddr(uint64_t addr) = 0;
	};

	void Init();

	Stackwalk *Collect();
	Stackwalk *Load(uint64_t *calls, size_t numLevels);

	StackResolver *MakeResolver(char *moduleDB, size_t DBSize, wstring pdbSearchPaths, volatile bool *killSignal);

	bool GetLoadedModules(char *&buf, size_t &size);
}; // namespace Callstack

namespace FileIO
{
	void GetDefaultFiles(const wchar_t *logBaseName, wstring &capture_filename, wstring &logging_filename, wstring &target);
	wstring GetAppFolderFilename(wstring filename);

	void GetExecutableFilename(wstring &selfName);
	
	uint64_t GetModifiedTimestamp(const wchar_t *filename);
	
	void CopyFileW(const wchar_t *from, const wchar_t *to, bool allowOverwrite);
	void UnlinkFileW(const wchar_t *path);

	FILE *fopen(const wchar_t *filename, const wchar_t *mode);

	size_t fread(void *buf, size_t elementSize, size_t count, FILE *f);
	size_t fwrite(const void *buf, size_t elementSize, size_t count, FILE *f);

	uint64_t ftell64(FILE *f);
	void fseek64(FILE *f, uint64_t offset, int origin);

	int fclose(FILE *f);
};

namespace Keyboard
{
	void Init();
	void AddInputWindow(void *wnd);
	void RemoveInputWindow(void *wnd);
	bool GetKeyState(int key);
};

namespace StringFormat
{
	int snprintf(char *str, size_t bufSize, const char *format, ...);
	int wsnprintf(wchar_t *str, size_t bufSize, const wchar_t *format, ...);
	int vsnprintf(char *str, size_t bufSize, const char *format, va_list v);
	void sntimef(char *str, size_t bufSize, const char *format);
	void wcsncpy(wchar_t *dst, const wchar_t *src, size_t count);

	string Fmt(const char *format, ...);
	wstring WFmt(const wchar_t *format, ...);

	string Wide2UTF8(const wstring &s);
	wstring UTF82Wide(const string &s);
};

namespace OSUtility
{
	inline void ForceCrash();
	inline void DebugBreak();
	inline bool DebuggerPresent();
	inline void DebugOutputA(const char *str);
};

// must #define:
// __PRETTY_FUNCTION_SIGNATURE__ - undecorated function signature
// GetEmbeddedResource(name_with_underscores_ext) - function/inline that returns the given file in a std::string

#ifdef RENDERDOC_PLATFORM
// "win32_specific.h" (in directory os/)
#include STRINGIZE(CONCAT(RENDERDOC_PLATFORM,_specific.h))
#else
#error Undefined Platform!
#endif
