/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <map>
#include <string>
#include <vector>
#include "api/replay/renderdoc_replay.h"
#include "common/common.h"

using std::string;
using std::vector;
using std::map;

struct CaptureOptions;

namespace Process
{
void RegisterEnvironmentModification(EnvironmentModification modif);

void ApplyEnvironmentModification();

bool CanGlobalHook();
bool StartGlobalHook(const char *pathmatch, const char *logfile, const CaptureOptions &opts);
bool IsGlobalHookActive();
void StopGlobalHook();

uint32_t InjectIntoProcess(uint32_t pid, const rdctype::array<EnvironmentModification> &env,
                           const char *logfile, const CaptureOptions &opts, bool waitForExit);
struct ProcessResult
{
  string strStdout, strStderror;
  int retCode;
};
uint32_t LaunchProcess(const char *app, const char *workingDir, const char *cmdLine,
                       ProcessResult *result = NULL);
uint32_t LaunchAndInjectIntoProcess(const char *app, const char *workingDir, const char *cmdLine,
                                    const rdctype::array<EnvironmentModification> &env,
                                    const char *logfile, const CaptureOptions &opts,
                                    bool waitForExit);
void *LoadModule(const char *module);
void *GetFunctionAddress(void *module, const char *function);
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
template <class data>
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
  CriticalSectionTemplate &operator=(const CriticalSectionTemplate &other);
  CriticalSectionTemplate(const CriticalSectionTemplate &other);

  data m_Data;
};

void Init();
void Shutdown();
uint64_t AllocateTLSSlot();

void *GetTLSValue(uint64_t slot);
void SetTLSValue(uint64_t slot, void *value);

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

  uint32_t GetRemoteIP() const;

  bool IsRecvDataWaiting();

  bool SendDataBlocking(const void *buf, uint32_t length);
  bool RecvDataBlocking(void *data, uint32_t length);

private:
  ptrdiff_t socket;
};

Socket *CreateServerSocket(const char *addr, uint16_t port, int queuesize);
Socket *CreateClientSocket(const char *host, uint16_t port, int timeoutMS);

// ip is packed in HOST byte order
inline uint32_t GetIPOctet(uint32_t ip, uint32_t octet)
{
  uint32_t shift = (3 - octet) * 8;
  uint32_t mask = 0xff << shift;

  return (ip & mask) >> shift;
}

// returns ip packed in HOST byte order (ie. little endian)
inline uint32_t MakeIP(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
  return ((a & 0xff) << 24) | ((b & 0xff) << 16) | ((c & 0xff) << 8) | ((d & 0xff) << 0);
}

// checks if `ip` matches the given `range` and subnet `mask`
inline bool MatchIPMask(uint32_t ip, uint32_t range, uint32_t mask)
{
  return (ip & mask) == (range & mask);
}

// parses the null-terminated string at 'str' for CIDR notation IP range
// aaa.bbb.ccc.ddd/nn
bool ParseIPRangeCIDR(const char *str, uint32_t &ip, uint32_t &mask);

void Init();
void Shutdown();
};

namespace Atomic
{
int32_t Inc32(volatile int32_t *i);
int32_t Dec32(volatile int32_t *i);
int64_t Inc64(volatile int64_t *i);
int64_t Dec64(volatile int64_t *i);
int64_t ExchAdd64(volatile int64_t *i, int64_t a);
int32_t CmpExch32(volatile int32_t *dest, int32_t oldVal, int32_t newVal);
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

StackResolver *MakeResolver(char *moduleDB, size_t DBSize, string pdbSearchPaths,
                            volatile bool *killSignal);

bool GetLoadedModules(char *&buf, size_t &size);
};    // namespace Callstack

namespace FileIO
{
void GetDefaultFiles(const char *logBaseName, string &capture_filename, string &logging_filename,
                     string &target);
string GetHomeFolderFilename();
string GetAppFolderFilename(const string &filename);
string GetTempFolderFilename();
string GetReplayAppFilename();

void CreateParentDirectory(const string &filename);

bool IsRelativePath(const string &path);
string GetFullPathname(const string &filename);

void GetExecutableFilename(string &selfName);

uint64_t GetModifiedTimestamp(const string &filename);

void Copy(const char *from, const char *to, bool allowOverwrite);
void Delete(const char *path);
std::vector<PathEntry> GetFilesInDirectory(const char *path);

FILE *fopen(const char *filename, const char *mode);

size_t fread(void *buf, size_t elementSize, size_t count, FILE *f);
size_t fwrite(const void *buf, size_t elementSize, size_t count, FILE *f);

bool exists(const char *filename);

std::string ErrorString();

std::string getline(FILE *f);

uint64_t ftell64(FILE *f);
void fseek64(FILE *f, uint64_t offset, int origin);

bool feof(FILE *f);

int fclose(FILE *f);

// functions for atomically appending to a log that may be in use in multiple
// processes
bool logfile_open(const char *filename);
void logfile_append(const char *msg, size_t length);
void logfile_close(const char *filename);

// utility functions
inline bool dump(const char *filename, const void *buffer, size_t size)
{
  FILE *f = FileIO::fopen(filename, "wb");
  if(f == NULL)
    return false;

  size_t numWritten = FileIO::fwrite(buffer, 1, size, f);

  FileIO::fclose(f);

  return numWritten == size;
}

inline bool slurp(const char *filename, vector<unsigned char> &buffer)
{
  FILE *f = FileIO::fopen(filename, "rb");
  if(f == NULL)
    return false;

  FileIO::fseek64(f, 0, SEEK_END);
  uint64_t size = ftell64(f);
  FileIO::fseek64(f, 0, SEEK_SET);

  buffer.resize((size_t)size);

  size_t numRead = FileIO::fread(&buffer[0], 1, buffer.size(), f);

  FileIO::fclose(f);

  return numRead == buffer.size();
}
};

namespace Keyboard
{
void Init();
void AddInputWindow(void *wnd);
void RemoveInputWindow(void *wnd);
bool GetKeyState(int key);
bool PlatformHasKeyInput();
};

// implemented per-platform
namespace StringFormat
{
void sntimef(char *str, size_t bufSize, const char *format);

string Wide2UTF8(const std::wstring &s);
};

// utility functions, implemented in os_specific.cpp, not per-platform (assuming standard stdarg.h)
// forwarded to custom printf implementation in utf8printf.cpp
namespace StringFormat
{
int vsnprintf(char *str, size_t bufSize, const char *format, va_list v);
int snprintf(char *str, size_t bufSize, const char *format, ...);

string Fmt(const char *format, ...);

int Wide2UTF8(wchar_t chr, char mbchr[4]);
};

namespace OSUtility
{
inline void ForceCrash();
inline void DebugBreak();
bool DebuggerPresent();
enum
{
  Output_DebugMon,
  Output_StdOut,
  Output_StdErr
};
void WriteOutput(int channel, const char *str);

enum MachineIdentBits
{
  MachineIdent_Windows = 0x00000001,
  MachineIdent_Linux = 0x00000002,
  MachineIdent_macOS = 0x00000004,
  MachineIdent_Android = 0x00000008,
  MachineIdent_iOS = 0x00000010,
  // unused bits 0x20, 0x40, 0x80
  MachineIdent_OS_Mask = 0x000000ff,

  MachineIdent_Arch_x86 = 0x00000100,
  MachineIdent_Arch_ARM = 0x00000200,
  // unused bits 0x400, 0x800
  MachineIdent_Arch_Mask = 0x00000f00,

  MachineIdent_32bit = 0x00001000,
  MachineIdent_64bit = 0x00002000,
  MachineIdent_Width_Mask = (MachineIdent_64bit | MachineIdent_32bit),

  // unused bits 0x4000, 0x8000

  // not filled out as yet but reserved for future use
  MachineIdent_GPU_ARM = 0x00010000,
  MachineIdent_GPU_AMD = 0x00020000,
  MachineIdent_GPU_IMG = 0x00040000,
  MachineIdent_GPU_Intel = 0x00080000,
  MachineIdent_GPU_NV = 0x00100000,
  MachineIdent_GPU_QUALCOMM = 0x00200000,
  MachineIdent_GPU_Samsung = 0x00400000,
  MachineIdent_GPU_Verisilicon = 0x00800000,
  MachineIdent_GPU_Mask = 0x0fff0000,
};

uint64_t GetMachineIdent();
string MakeMachineIdentString(uint64_t ident);
};

namespace Bits
{
inline uint32_t CountLeadingZeroes(uint32_t value);
#if ENABLED(RDOC_X64)
inline uint64_t CountLeadingZeroes(uint64_t value);
#endif
};

// must #define:
// __PRETTY_FUNCTION_SIGNATURE__ - undecorated function signature
// GetEmbeddedResource(name_with_underscores_ext) - function/inline that returns the given file in a
// std::string
// OS_DEBUG_BREAK() - instruction that debugbreaks the debugger - define instead of function to
// preserve callstacks

#if ENABLED(RDOC_WIN32)
#include "win32/win32_specific.h"
#elif ENABLED(RDOC_POSIX)
#include "posix/posix_specific.h"
#else
#error Undefined Platform!
#endif

namespace Android
{
bool IsHostADB(const char *hostname);
uint32_t StartAndroidPackageForCapture(const char *host, const char *package);
string adbExecCommand(const string &deviceID, const string &args);
void extractDeviceIDAndIndex(const string &hostname, int &index, string &deviceID);
}
