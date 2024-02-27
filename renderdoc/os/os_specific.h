/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include <stdio.h>
#include <functional>
#include "api/replay/rdcarray.h"
#include "api/replay/rdcpair.h"
#include "api/replay/rdcstr.h"
#include "common/globalconfig.h"
#include "common/result.h"

struct CaptureOptions;
struct EnvironmentModification;
struct PathEntry;
enum class WindowingSystem : uint32_t;
typedef std::function<void(float)> RENDERDOC_ProgressCallback;

namespace Process
{
void RegisterEnvironmentModification(const EnvironmentModification &modif);

void ApplyEnvironmentModification();

rdcstr GetEnvVariable(const rdcstr &name);

uint64_t GetMemoryUsage();

bool CanGlobalHook();
RDResult StartGlobalHook(const rdcstr &pathmatch, const rdcstr &capturefile,
                         const CaptureOptions &opts);
bool IsGlobalHookActive();
void StopGlobalHook();

rdcpair<RDResult, uint32_t> InjectIntoProcess(uint32_t pid,
                                              const rdcarray<EnvironmentModification> &env,
                                              const rdcstr &capturefile, const CaptureOptions &opts,
                                              bool waitForExit);
struct ProcessResult
{
  rdcstr strStdout, strStderror;
  int retCode;
};
uint32_t LaunchProcess(const rdcstr &app, const rdcstr &workingDir, const rdcstr &cmdLine,
                       bool internal, ProcessResult *result = NULL);
uint32_t LaunchScript(const rdcstr &script, const rdcstr &workingDir, const rdcstr &args,
                      bool internal, ProcessResult *result = NULL);
rdcpair<RDResult, uint32_t> LaunchAndInjectIntoProcess(const rdcstr &app, const rdcstr &workingDir,
                                                       const rdcstr &cmdLine,
                                                       const rdcarray<EnvironmentModification> &env,
                                                       const rdcstr &capturefile,
                                                       const CaptureOptions &opts, bool waitForExit);
bool IsModuleLoaded(const rdcstr &module);
void *LoadModule(const rdcstr &module);
void *GetFunctionAddress(void *module, const rdcstr &function);
uint32_t GetCurrentPID();

void Shutdown();
};

namespace Timing
{
double GetTickFrequency();
uint64_t GetTick();
uint64_t GetUnixTimestamp();
time_t GetUTCTime();
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

  // no copying
  CriticalSectionTemplate &operator=(const CriticalSectionTemplate &other) = delete;
  CriticalSectionTemplate(const CriticalSectionTemplate &other) = delete;

  data m_Data;
};

template <class data>
class RWLockTemplate
{
public:
  RWLockTemplate();
  ~RWLockTemplate();

  void ReadLock();
  bool TryReadlock();
  void ReadUnlock();

  void WriteLock();
  bool TryWritelock();
  void WriteUnlock();

  // no copying
  RWLockTemplate &operator=(const RWLockTemplate &other) = delete;
  RWLockTemplate(const RWLockTemplate &other) = delete;

  data m_Data;
};

void Init();
void Shutdown();
uint64_t AllocateTLSSlot();

void *GetTLSValue(uint64_t slot);
void SetTLSValue(uint64_t slot, void *value);

// must typedef CriticalSectionTemplate<X> CriticalSection

void SetCurrentThreadName(const rdcstr &name);

typedef uint64_t ThreadHandle;
ThreadHandle CreateThread(std::function<void()> entryFunc);
uint64_t GetCurrentID();
void JoinThread(ThreadHandle handle);
void DetachThread(ThreadHandle handle);
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
  Socket(ptrdiff_t s) : socket(s), timeoutMS(5000) {}
  ~Socket();
  void Shutdown();

  bool Connected() const;

  RDResult GetError() const { return m_Error; }
  uint32_t GetTimeout() const { return timeoutMS; }
  void SetTimeout(uint32_t milliseconds) { timeoutMS = milliseconds; }
  Socket *AcceptClient(uint32_t timeoutMilliseconds);

  uint32_t GetRemoteIP() const;

  bool IsRecvDataWaiting();

  bool SendDataBlocking(const void *buf, uint32_t length);
  bool RecvDataBlocking(void *data, uint32_t length);
  bool RecvDataNonBlocking(void *data, uint32_t &length);

private:
  ptrdiff_t socket;
  uint32_t timeoutMS;
  RDResult m_Error;
};

Socket *CreateServerSocket(const rdcstr &addr, uint16_t port, int queuesize);
Socket *CreateClientSocket(const rdcstr &host, uint16_t port, int timeoutMS);

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
bool ParseIPRangeCIDR(const rdcstr &str, uint32_t &ip, uint32_t &mask);

void Init();
void Shutdown();
};

namespace Atomic
{
int32_t Inc32(int32_t *i);
int32_t Dec32(int32_t *i);
int64_t Inc64(int64_t *i);
int64_t Dec64(int64_t *i);
int64_t ExchAdd64(int64_t *i, int64_t a);
int32_t CmpExch32(int32_t *dest, int32_t oldVal, int32_t newVal);
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
  rdcstr function;
  rdcstr filename;
  uint32_t line;

  rdcstr formattedString(const rdcstr &commonPath = rdcstr());
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

StackResolver *MakeResolver(bool interactive, byte *moduleDB, size_t DBSize,
                            RENDERDOC_ProgressCallback);

bool GetLoadedModules(byte *buf, size_t &size);
};    // namespace Callstack

namespace FileIO
{
void GetDefaultFiles(const rdcstr &logBaseName, rdcstr &capture_filename, rdcstr &logging_filename,
                     rdcstr &target);
rdcstr GetHomeFolderFilename();
rdcstr GetAppFolderFilename(const rdcstr &filename);
rdcstr GetTempFolderFilename();
rdcstr GetReplayAppFilename();

void CreateParentDirectory(const rdcstr &filename);

bool IsRelativePath(const rdcstr &path);
rdcstr GetFullPathname(const rdcstr &filename);
rdcstr FindFileInPath(const rdcstr &fileName);

void GetExecutableFilename(rdcstr &selfName);
void GetLibraryFilename(rdcstr &selfName);

uint64_t GetModifiedTimestamp(const rdcstr &filename);
uint64_t GetFileSize(const rdcstr &filename);

bool Copy(const rdcstr &from, const rdcstr &to, bool allowOverwrite);
bool Move(const rdcstr &from, const rdcstr &to, bool allowOverwrite);
void Delete(const rdcstr &path);
void GetFilesInDirectory(const rdcstr &path, rdcarray<PathEntry> &entries);

enum FileMode
{
  ReadText,
  ReadBinary,
  WriteText,
  WriteBinary,
  UpdateBinary,
  OverwriteBinary,
};
FILE *fopen(const rdcstr &filename, FileMode mode);

size_t fread(void *buf, size_t elementSize, size_t count, FILE *f);
size_t fwrite(const void *buf, size_t elementSize, size_t count, FILE *f);

bool IsUntrustedFile(const rdcstr &filename);

bool exists(const rdcstr &filename);

rdcstr ErrorString();

uint64_t ftell64(FILE *f);
void fseek64(FILE *f, uint64_t offset, int origin);

void ftruncateat(FILE *f, uint64_t length);

bool fflush(FILE *f);

bool feof(FILE *f);

int fclose(FILE *f);

// functions for atomically appending to a log that may be in use in multiple
// processes
struct LogFileHandle;
LogFileHandle *logfile_open(const rdcstr &filename);
void logfile_append(LogFileHandle *logHandle, const char *msg, size_t length);
void logfile_close(LogFileHandle *logHandle, const rdcstr &deleteFilename);

// read the whole logfile into memory starting at a given offset. This may race with processes
// writing, but it will read the whole of the file at some point. Useful since normal file reading
// may fail on the shared logfile
rdcstr logfile_readall(uint64_t offset, const rdcstr &filename);

// utility functions
inline bool WriteAll(const rdcstr &filename, const void *buffer, size_t size)
{
  FILE *f = FileIO::fopen(filename, FileIO::WriteBinary);
  if(f == NULL)
    return false;

  size_t numWritten = FileIO::fwrite(buffer, 1, size, f);

  FileIO::fclose(f);

  return numWritten == size;
}

template <typename T>
bool WriteAll(const rdcstr &filename, const rdcarray<T> &buffer)
{
  return WriteAll(filename, buffer.data(), buffer.size() * sizeof(T));
}

inline bool WriteAll(const rdcstr &filename, const rdcstr &buffer)
{
  return WriteAll(filename, buffer.c_str(), buffer.length());
}

template <typename T>
bool ReadAll(const rdcstr &filename, rdcarray<T> &buffer)
{
  FILE *f = FileIO::fopen(filename, FileIO::ReadBinary);
  if(f == NULL)
    return false;

  FileIO::fseek64(f, 0, SEEK_END);
  uint64_t size = ftell64(f);
  FileIO::fseek64(f, 0, SEEK_SET);

  buffer.resize((size_t)size / sizeof(T));

  size_t numRead = FileIO::fread(&buffer[0], sizeof(T), buffer.size(), f);

  FileIO::fclose(f);

  return numRead == buffer.size();
}

inline bool ReadAll(const rdcstr &filename, rdcstr &str)
{
  FILE *f = FileIO::fopen(filename, FileIO::ReadBinary);
  if(f == NULL)
    return false;

  char chunk[513];

  while(!FileIO::feof(f))
  {
    memset(chunk, 0, 513);
    size_t numRead = FileIO::fread(chunk, 1, 512, f);
    str.append(chunk, numRead);
  }

  FileIO::fclose(f);

  return true;
}
};

namespace Keyboard
{
void Init();
void AddInputWindow(WindowingSystem windowSystem, void *wnd);
void RemoveInputWindow(WindowingSystem windowSystem, void *wnd);
bool GetKeyState(int key);
bool PlatformHasKeyInput();
};

// simple container for passing around temporary wide strings. We leave it immutable and manually
// add the trailing NULL. These are used as rarely as possible but still needed for interacting with
// windows/D3D APIs.
struct rdcwstr : private rdcarray<wchar_t>
{
  rdcwstr() : rdcarray<wchar_t>() {}
  rdcwstr(const wchar_t *str, size_t N) : rdcarray<wchar_t>(str, N)
  {
    // push null terminator
    rdcarray<wchar_t>::push_back(0);
  }
  rdcwstr(const wchar_t *str)
  {
    while(*str)
    {
      rdcarray<wchar_t>::push_back(*str);
      str++;
    }
    // push null terminator
    rdcarray<wchar_t>::push_back(0);
  }
  template <size_t N>
  rdcwstr(const wchar_t (&el)[N]) : rdcwstr(&el[0])
  {
  }
  rdcwstr(size_t N) { resize(N + 1); }
  wchar_t *data() { return rdcarray<wchar_t>::data(); }
  const wchar_t *c_str() const { return rdcarray<wchar_t>::data(); }
  using rdcarray<wchar_t>::operator[];
  size_t length() const { return rdcarray<wchar_t>::size() - 1; }
};

// implemented per-platform
namespace StringFormat
{
rdcstr sntimef(time_t utcTime, const char *format);

rdcstr Wide2UTF8(const rdcwstr &str);
rdcwstr UTF82Wide(const rdcstr &s);

void Shutdown();
};

namespace OSUtility
{
inline void ForceCrash();
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
rdcstr MakeMachineIdentString(uint64_t ident);
};

namespace Bits
{
inline uint32_t CountLeadingZeroes(uint32_t value);
#if ENABLED(RDOC_X64)
inline uint64_t CountLeadingZeroes(uint64_t value);
#endif
};

// must #define:
// GetEmbeddedResource(name_with_underscores_ext) - function/inline that returns the given file in a
// rdcstr
// EndianSwapXX() for XX = 16, 32, 64

#if ENABLED(RDOC_WIN32)
#include "win32/win32_specific.h"
#elif ENABLED(RDOC_POSIX)
#include "posix/posix_specific.h"
#else
#error Undefined Platform!
#endif

inline uint64_t EndianSwap(uint64_t t)
{
  return EndianSwap64(t);
}

inline uint32_t EndianSwap(uint32_t t)
{
  return EndianSwap32(t);
}

inline uint16_t EndianSwap(uint16_t t)
{
  return EndianSwap16(t);
}

inline int64_t EndianSwap(int64_t t)
{
  return (int64_t)EndianSwap(uint64_t(t));
}

inline int32_t EndianSwap(int32_t t)
{
  return (int32_t)EndianSwap(uint32_t(t));
}

inline int16_t EndianSwap(int16_t t)
{
  return (int16_t)EndianSwap(uint16_t(t));
}

inline double EndianSwap(double t)
{
  uint64_t u;
  memcpy(&u, &t, sizeof(t));
  u = EndianSwap(u);
  memcpy(&t, &u, sizeof(t));
  return t;
}

inline float EndianSwap(float t)
{
  uint32_t u;
  memcpy(&u, &t, sizeof(t));
  u = EndianSwap(u);
  memcpy(&t, &u, sizeof(t));
  return t;
}

inline char EndianSwap(char t)
{
  return t;
}

inline byte EndianSwap(byte t)
{
  return t;
}

inline bool EndianSwap(bool t)
{
  return t;
}
