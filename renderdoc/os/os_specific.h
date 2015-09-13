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
using std::vector;

struct CaptureOptions;

namespace Process
{
	void StartGlobalHook(const char *pathmatch, const char *logfile, const CaptureOptions *opts);
	uint32_t InjectIntoProcess(uint32_t pid, const char *logfile, const CaptureOptions *opts, bool waitForExit);
	uint32_t LaunchProcess(const char *app, const char *workingDir, const char *cmdLine);
	uint32_t LaunchAndInjectIntoProcess(const char *app, const char *workingDir, const char *cmdLine,
										const char *logfile, const CaptureOptions *opts, bool waitForExit);
	bool LoadModule(const char *module);
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
	Socket *CreateClientSocket(const char *host, uint16_t port, int timeoutMS);
	
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

		virtual void Set(uint64_t *calls, size_t numLevels) = 0;

		virtual size_t NumLevels() const = 0;
		virtual const uint64_t *GetAddrs() const = 0;
	};

	struct AddressDetails
	{
		AddressDetails() : line(0) {}

		string function;
		string filename;
		uint32_t line;

		string formattedString(const char *commonPath = NULL);
	};

	class StackResolver
	{
	public:
		virtual ~StackResolver() {}
		virtual AddressDetails GetAddr(uint64_t addr) = 0;
	};

	void Init();

	Stackwalk *Collect();
	Stackwalk *Create();

	StackResolver *MakeResolver(char *moduleDB, size_t DBSize, string pdbSearchPaths, volatile bool *killSignal);

	bool GetLoadedModules(char *&buf, size_t &size);
}; // namespace Callstack

namespace FileIO
{
	void GetDefaultFiles(const char *logBaseName, string &capture_filename, string &logging_filename, string &target);
	string GetAppFolderFilename(const string &filename);
	string GetReplayAppFilename();

	void CreateParentDirectory(const string &filename);

	string GetFullPathname(const string &filename);

	void GetExecutableFilename(string &selfName);
	
	uint64_t GetModifiedTimestamp(const string &filename);
	
	void Copy(const char *from, const char *to, bool allowOverwrite);
	void Delete(const char *path);

	FILE *fopen(const char *filename, const char *mode);

	size_t fread(void *buf, size_t elementSize, size_t count, FILE *f);
	size_t fwrite(const void *buf, size_t elementSize, size_t count, FILE *f);

	uint64_t ftell64(FILE *f);
	void fseek64(FILE *f, uint64_t offset, int origin);

	bool feof(FILE *f);

	int fclose(FILE *f);
};

namespace Keyboard
{
	void Init();
	void AddInputWindow(void *wnd);
	void RemoveInputWindow(void *wnd);
	bool GetKeyState(int key);
};

// implemented per-platform
namespace StringFormat
{
	void sntimef(char *str, size_t bufSize, const char *format);

	// forwards to vsnprintf below, needed to be here due to va_copy differences
	string Fmt(const char *format, ...);

	string Wide2UTF8(const std::wstring &s);
};

// utility functions, implemented in os_specific.cpp, not per-platform (assuming standard stdarg.h)
// forwarded to custom printf implementation in utf8printf.cpp
namespace StringFormat
{
	int vsnprintf(char *str, size_t bufSize, const char *format, va_list v);
	int snprintf(char *str, size_t bufSize, const char *format, ...);

	int Wide2UTF8(wchar_t chr, char mbchr[4]);
};

namespace OSUtility
{
	inline void ForceCrash();
	inline void DebugBreak();
	inline bool DebuggerPresent();
	enum { Output_DebugMon, Output_StdOut, Output_StdErr };
	void WriteOutput(int channel, const char *str);
};

// must #define:
// __PRETTY_FUNCTION_SIGNATURE__ - undecorated function signature
// GetEmbeddedResource(name_with_underscores_ext) - function/inline that returns the given file in a std::string
// OS_DEBUG_BREAK() - instruction that debugbreaks the debugger - define instead of function to preserve callstacks

#ifdef RENDERDOC_PLATFORM
// "win32_specific.h" (in directory os/)
#include STRINGIZE(CONCAT(RENDERDOC_PLATFORM,_specific.h))
#else
#error Undefined Platform!
#endif
