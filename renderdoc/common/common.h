/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

#pragma once

#include <stdint.h>
#include <time.h>
#include "globalconfig.h"

// we allow a small amount of leakage from OS-specific to avoid including os_specific.h which is
// heavier than we need. These are macros so can't be declared and then later defined, and are
// fortunately not too messy as it's basically windows vs. everyone else.
#if ENABLED(RDOC_WIN32)

#define __PRETTY_FUNCTION_SIGNATURE__ __FUNCSIG__
#define OS_DEBUG_BREAK() __debugbreak()

#else

#include <signal.h>

#define __PRETTY_FUNCTION_SIGNATURE__ __PRETTY_FUNCTION__
#if ENABLED(RDOC_SWITCH)
#include <stdlib.h>
#define OS_DEBUG_BREAK() abort()
#else
#define OS_DEBUG_BREAK() raise(SIGTRAP)
#endif

#if defined(__clang__)

#define DELIBERATE_FALLTHROUGH() [[clang::fallthrough]]

#elif defined(__GNUC__) && (__GNUC__ >= 7)

// works on GCC 7.0 and up. Before then there was no warning, so we're fine
#define DELIBERATE_FALLTHROUGH() __attribute__((fallthrough))

#endif

#endif

// pre-declare some OS-specific functions we need to reference in the header here.

namespace OSUtility
{
void ForceCrash();
bool DebuggerPresent();
};

/////////////////////////////////////////////////
// Utility macros

#ifndef SAFE_DELETE
#define SAFE_DELETE(p) \
  do                   \
  {                    \
    if(p)              \
    {                  \
      delete(p);       \
      (p) = NULL;      \
    }                  \
  } while((void)0, 0)
#endif

#ifndef SAFE_DELETE_ARRAY
#define SAFE_DELETE_ARRAY(p) \
  do                         \
  {                          \
    if(p)                    \
    {                        \
      delete[](p);           \
      (p) = NULL;            \
    }                        \
  } while((void)0, 0)
#endif

#ifndef SAFE_ADDREF
#define SAFE_ADDREF(p) \
  do                   \
  {                    \
    if(p)              \
    {                  \
      (p)->AddRef();   \
    }                  \
  } while((void)0, 0)
#endif

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) \
  do                    \
  {                     \
    if(p)               \
    {                   \
      (p)->Release();   \
      (p) = NULL;       \
    }                   \
  } while((void)0, 0)
#define SAFE_RELEASE_NOCLEAR(p) \
  do                            \
  {                             \
    if(p)                       \
    {                           \
      (p)->Release();           \
    }                           \
  } while((void)0, 0)
#endif

#ifndef ARRAY_COUNT
#define ARRAY_COUNT(arr) (sizeof(arr) / sizeof(arr[0]))
#endif

#define STRINGIZE2(a) #a
#define STRINGIZE(a) STRINGIZE2(a)

#define CONCAT2(a, b) a##b
#define CONCAT(a, b) CONCAT2(a, b)

#define RDCEraseMem(a, b) memset(a, 0, b)
#define RDCEraseEl(a) memset((void *)&a, 0, sizeof(a))

template <typename T>
T RDCCLAMP(const T &val, const T &mn, const T &mx)
{
  return val < mn ? mn : (val > mx ? mx : val);
}

template <typename T>
T RDCMIN(const T &a, const T &b)
{
  return a < b ? a : b;
}

template <typename T>
T RDCMAX(const T &a, const T &b)
{
  return a > b ? a : b;
}

template <typename T>
T RDCLERP(const T &a, const T &b, const T &step)
{
  return (1.0f - step) * a + step * b;
}

inline bool RDCISNAN(float input)
{
  union
  {
    uint32_t u;
    float f;
  } x;

  x.f = input;

  // ignore sign bit (0x80000000)
  //     check that exponent (0x7f800000) is fully set
  // AND that mantissa (0x007fffff) is greater than 0 (if it's 0 then this is an inf)
  return (x.u & 0x7fffffffU) > 0x7f800000U;
}

inline bool RDCISINF(float input)
{
  union
  {
    uint32_t u;
    float f;
  } x;

  x.f = input;

  // ignore sign bit (0x80000000)
  //     check that exponent (0x7f800000) is fully set
  // AND that mantissa (0x007fffff) is exactly than 0 (if it's non-0 then this is an nan)
  return (x.u & 0x7fffffffU) == 0x7f800000U;
}

inline bool RDCISFINITE(float input)
{
  union
  {
    uint32_t u;
    float f;
  } x;

  x.f = input;

  // ignore sign bit (0x80000000)
  //     check that exponent (0x7f800000) is not fully set (if it's fully set then this is a
  //     nan/inf)
  return (x.u & 0x7f800000U) != 0x7f800000U;
}

// double variants

inline bool RDCISNAN(double input)
{
  union
  {
    uint64_t u;
    double f;
  } x;

  x.f = input;

  // ignore sign bit (0x80000000)
  //     check that exponent (0x7f800000) is fully set
  // AND that mantissa (0x007fffff) is greater than 0 (if it's 0 then this is an inf)
  return (x.u & 0x7fffffffffffffffULL) > 0x7ff0000000000000ULL;
}

inline bool RDCISINF(double input)
{
  union
  {
    uint64_t u;
    double f;
  } x;

  x.f = input;

  return (x.u & 0x7fffffffffffffffULL) == 0x7ff0000000000000ULL;
}

inline bool RDCISFINITE(double input)
{
  union
  {
    uint64_t u;
    double f;
  } x;

  x.f = input;

  return (x.u & 0x7ff0000000000000ULL) != 0x7ff0000000000000ULL;
}

template <typename T>
inline T AlignUp4(T x)
{
  return (x + 0x3) & (~0x3);
}

template <typename T>
inline T AlignUp16(T x)
{
  return (x + 0xf) & (~0xf);
}

template <typename T>
inline T AlignUp(T x, T a)
{
  return (x + (a - 1)) & (~(a - 1));
}

template <typename T, typename A>
inline T AlignUpPtr(T x, A a)
{
  return (T)AlignUp<uintptr_t>((uintptr_t)x, (uintptr_t)a);
}

#define MAKE_FOURCC(a, b, c, d) \
  (((uint32_t)(d) << 24) | ((uint32_t)(c) << 16) | ((uint32_t)(b) << 8) | (uint32_t)(a))

bool FindDiffRange(void *a, void *b, size_t bufSize, size_t &diffStart, size_t &diffEnd);
uint32_t CalcNumMips(int Width, int Height, int Depth);

typedef uint8_t byte;

byte *AllocAlignedBuffer(uint64_t size, uint64_t alignment = 64);
void FreeAlignedBuffer(byte *buf);

uint32_t Log2Floor(uint32_t value);
#if ENABLED(RDOC_X64)
uint64_t Log2Floor(uint64_t value);
#endif

// super ugly - on apple size_t is a separate type, so we need a new overload
#if ENABLED(RDOC_APPLE)
inline size_t Log2Floor(size_t value)
{
#if ENABLED(RDOC_X64)
  return (size_t)Log2Floor((uint64_t)value);
#else
  return (size_t)Log2Floor((uint32_t)value);
#endif
}
#endif

/////////////////////////////////////////////////
// Debugging features

#if !defined(DELIBERATE_FALLTHROUGH)
#define DELIBERATE_FALLTHROUGH() \
  do                             \
  {                              \
  } while(0)
#endif

#define RDCDUMP()            \
  do                         \
  {                          \
    OSUtility::ForceCrash(); \
  } while((void)0, 0)

#if ENABLED(RDOC_DEVEL) || ENABLED(FORCE_DEBUGBREAK)
#define RDCBREAK()                   \
  do                                 \
  {                                  \
    if(OSUtility::DebuggerPresent()) \
      OS_DEBUG_BREAK();              \
  } while((void)0, 0)
#else
#define RDCBREAK() \
  do               \
  {                \
  } while((void)0, 0)
#endif

#define RDCUNIMPLEMENTED(...)                                \
  do                                                         \
  {                                                          \
    rdclog(LogType::Warning, "Unimplemented: " __VA_ARGS__); \
    RDCBREAK();                                              \
  } while((void)0, 0)

//
// Logging
//

#if ENABLED(STRIP_LOG)
#define RDCLOGFILE(fn) \
  do                   \
  {                    \
  } while((void)0, 0)
#define RDCLOGDELETE() \
  do                   \
  {                    \
  } while((void)0, 0)

#define RDCDEBUG(...) \
  do                  \
  {                   \
  } while((void)0, 0)
#define RDCLOG(...) \
  do                \
  {                 \
  } while((void)0, 0)
#define RDCWARN(...) \
  do                 \
  {                  \
  } while((void)0, 0)
#define RDCERR(...) \
  do                \
  {                 \
  } while((void)0, 0)
#define RDCFATAL(...) \
  do                  \
  {                   \
    RDCDUMP();        \
    exit(0);          \
  } while((void)0, 0)
#define RDCDUMPMSG(message) \
  do                        \
  {                         \
    RDCDUMP();              \
    exit(0);                \
  } while((void)0, 0)
#else
// perform any operations necessary to flush the log
void rdclog_flush();

// we want to use the same log enum publicly and privately but we can't redefine the enum. So we let
// each header define it depending on which comes first (since we don't necessarily know if the
// other will be included after). Each defines LOGTYPE_DEFINED when it defines the enum, and uses
// that to gate definition.
//
// The public header just skips the definition entirely, the internal header also declares it as
// LogType__Internal so that we can assert that they're the same.
#if !defined(LOGTYPE_DEFINED)

#define LOGTYPE_DEFINED
#define LOGTYPE_ENUM_NAME LogType

#else

#define LOGTYPE_ENUM_NAME LogType__Internal

#endif

// must match the definition in replay_enums.h
enum class LOGTYPE_ENUM_NAME : uint32_t
{
  Debug,
  First = Debug,
  Comment,
  Warning,
  Error,
  Fatal,
  Count,
};

// actual low-level print to log output streams defined (useful for if we need to print
// fatal error messages from within the more complex log function).
void rdclogprint_int(LogType type, const char *fullMsg, const char *msg);

#if !defined(RDCLOG_PROJECT)
#define RDCLOG_PROJECT "RDOC"
#endif

// printf() style main logger function
void rdclog_direct(time_t utcTime, uint32_t pid, LogType type, const char *project,
                   const char *file, unsigned int line, const char *fmt, ...);

#define FILL_AUTO_VALUE 0x10203040

#define rdclog(type, ...)                                                                 \
  rdclog_direct(time_t(FILL_AUTO_VALUE), FILL_AUTO_VALUE, type, RDCLOG_PROJECT, __FILE__, \
                __LINE__, __VA_ARGS__)

const char *rdclog_getfilename();
void rdclog_filename(const char *filename);
void rdclog_enableoutput();
void rdclog_closelog();

#define RDCLOGFILE(fn) rdclog_filename(fn)
#define RDCGETLOGFILE() rdclog_getfilename()

#define RDCLOGOUTPUT() rdclog_enableoutput()
#define RDCSTOPLOGGING() rdclog_closelog()

#if(ENABLED(RDOC_DEVEL) || ENABLED(FORCE_DEBUG_LOGS)) && DISABLED(STRIP_DEBUG_LOGS)
#define RDCDEBUG(...) rdclog(LogType::Debug, __VA_ARGS__)
#else
#define RDCDEBUG(...) \
  do                  \
  {                   \
  } while((void)0, 0)
#endif

#define RDCLOG(...) rdclog(LogType::Comment, __VA_ARGS__)
#define RDCWARN(...) rdclog(LogType::Warning, __VA_ARGS__)

#if ENABLED(DEBUGBREAK_ON_ERROR_LOG)
#define RDCERR(...)                      \
  do                                     \
  {                                      \
    rdclog(LogType::Error, __VA_ARGS__); \
    rdclog_flush();                      \
    RDCBREAK();                          \
  } while((void)0, 0)
#else
#define RDCERR(...) rdclog(LogType::Error, __VA_ARGS__)
#endif

#define RDCFATAL(...)                    \
  do                                     \
  {                                      \
    rdclog(LogType::Fatal, __VA_ARGS__); \
    rdclog_flush();                      \
    RDCDUMP();                           \
    exit(0);                             \
  } while((void)0, 0)
#define RDCDUMPMSG(message)                            \
  do                                                   \
  {                                                    \
    rdclogprint_int(LogType::Fatal, message, message); \
    rdclog_flush();                                    \
    RDCDUMP();                                         \
    exit(0);                                           \
  } while((void)0, 0)
#endif

//
// Assert
//

#if ENABLED(RDOC_DEVEL) || ENABLED(FORCE_ASSERTS)
void rdcassert(const char *msg, const char *file, unsigned int line, const char *func);

// this defines the root macro, RDCASSERTMSG(msg, cond, ...)
// where it will check cond, then print msg (if it's not "") and the values of all values passed via
// varargs.
// the other asserts are defined in terms of that
#include "custom_assert.h"

#else
#define RDCASSERTMSG(...) \
  do                      \
  {                       \
    (void)(__VA_ARGS__);  \
  } while((void)0, 0)
#endif

#define RDCASSERT(...) RDCASSERTMSG("", __VA_ARGS__)
#define RDCASSERTEQUAL(a, b) RDCASSERTMSG("", (a) == (b), a, b)
#define RDCASSERTNOTEQUAL(a, b) RDCASSERTMSG("", (a) != (b), a, b)

//
// Compile asserts
//

#if ENABLED(STRIP_COMPILE_ASSERTS)
#define RDCCOMPILE_ASSERT(condition, message) \
  do                                          \
  {                                           \
  } while((void)0, 0)
#else
#define RDCCOMPILE_ASSERT(condition, message) static_assert(condition, message)
#endif
