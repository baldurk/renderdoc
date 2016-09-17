/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2016 Baldur Karlsson
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

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "globalconfig.h"

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

#include "os/os_specific.h"

#define RDCEraseMem(a, b) memset(a, 0, b)
#define RDCEraseEl(a) memset(&a, 0, sizeof(a))

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

uint32_t Log2Floor(uint32_t value);
#if RDC64BIT
uint64_t Log2Floor(uint64_t value);
#endif

/////////////////////////////////////////////////
// Debugging features

#define RDCDUMP()            \
  do                         \
  {                          \
    OSUtility::ForceCrash(); \
  } while((void)0, 0)

#if !defined(RELEASE) || defined(FORCE_DEBUGBREAK)
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

#define RDCUNIMPLEMENTED(...)                              \
  do                                                       \
  {                                                        \
    rdclog(RDCLog_Warning, "Unimplemented: " __VA_ARGS__); \
    RDCBREAK();                                            \
  } while((void)0, 0)

//
// Logging
//

enum LogType
{
  RDCLog_First = -1,
  RDCLog_Debug,
  RDCLog_Comment,
  RDCLog_Warning,
  RDCLog_Error,
  RDCLog_Fatal,
  RDCLog_NumTypes,
};

#if defined(STRIP_LOG)
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

// actual low-level print to log output streams defined (useful for if we need to print
// fatal error messages from within the more complex log function).
void rdclogprint_int(LogType type, const char *fullMsg, const char *msg);

// printf() style main logger function
void rdclog_int(LogType type, const char *file, unsigned int line, const char *fmt, ...);

#define rdclog(type, ...) rdclog_int(type, __FILE__, __LINE__, __VA_ARGS__)

const char *rdclog_getfilename();
void rdclog_filename(const char *filename);
void rdclog_enableoutput();

#define RDCLOGFILE(fn) rdclog_filename(fn)
#define RDCGETLOGFILE() rdclog_getfilename()

#define RDCLOGOUTPUT() rdclog_enableoutput()

#if(!defined(RELEASE) || defined(FORCE_DEBUG_LOGS)) && !defined(STRIP_DEBUG_LOGS)
#define RDCDEBUG(...) rdclog(RDCLog_Debug, __VA_ARGS__)
#else
#define RDCDEBUG(...) \
  do                  \
  {                   \
  } while((void)0, 0)
#endif

#define RDCLOG(...) rdclog(RDCLog_Comment, __VA_ARGS__)
#define RDCWARN(...) rdclog(RDCLog_Warning, __VA_ARGS__)

#if defined(DEBUGBREAK_ON_ERROR_LOG)
#define RDCERR(...)                    \
  do                                   \
  {                                    \
    rdclog(RDCLog_Error, __VA_ARGS__); \
    rdclog_flush();                    \
    RDCBREAK();                        \
  } while((void)0, 0)
#else
#define RDCERR(...) rdclog(RDCLog_Error, __VA_ARGS__)
#endif

#define RDCFATAL(...)                  \
  do                                   \
  {                                    \
    rdclog(RDCLog_Fatal, __VA_ARGS__); \
    rdclog_flush();                    \
    RDCDUMP();                         \
    exit(0);                           \
  } while((void)0, 0)
#define RDCDUMPMSG(message)                          \
  do                                                 \
  {                                                  \
    rdclogprint_int(RDCLog_Fatal, message, message); \
    rdclog_flush();                                  \
    RDCDUMP();                                       \
    exit(0);                                         \
  } while((void)0, 0)
#endif

//
// Assert
//

#if !defined(RELEASE) || defined(FORCE_ASSERTS)
void rdcassert(const char *msg, const char *file, unsigned int line, const char *func);

// this defines the root macro, RDCASSERTMSG(msg, cond, ...)
// where it will check cond, then print msg (if it's not "") and the values of all values passed via
// varargs.
// the other asserts are defined in terms of that
#include "custom_assert.h"

#else
#define RDCASSERTMSG(cond) \
  do                       \
  {                        \
  } while((void)0, 0)
#endif

#define RDCASSERT(...) RDCASSERTMSG("", __VA_ARGS__)
#define RDCASSERTEQUAL(a, b) RDCASSERTMSG("", a == b, a, b)
#define RDCASSERTNOTEQUAL(a, b) RDCASSERTMSG("", a != b, a, b)

//
// Compile asserts
//

#if defined(STRIP_COMPILE_ASSERTS)
#define RDCCOMPILE_ASSERT(condition, message) \
  do                                          \
  {                                           \
  } while((void)0, 0)
#else
#define RDCCOMPILE_ASSERT(condition, message) static_assert(condition, message)
#endif

typedef uint8_t byte;
