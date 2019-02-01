/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016-2019 Baldur Karlsson
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

#include <pthread.h>
#include <signal.h>
#include "data/embedded_files.h"

#define __PRETTY_FUNCTION_SIGNATURE__ __PRETTY_FUNCTION__

#define OS_DEBUG_BREAK() raise(SIGTRAP)

#if ENABLED(RDOC_APPLE)

#include <libkern/OSByteOrder.h>
#define EndianSwap16(x) OSSwapInt16(x)
#define EndianSwap32(x) OSSwapInt32(x)
#define EndianSwap64(x) OSSwapInt64(x)

#else

#define EndianSwap16(x) __builtin_bswap16(x)
#define EndianSwap32(x) __builtin_bswap32(x)
#define EndianSwap64(x) __builtin_bswap64(x)

#endif

struct EmbeddedResourceType
{
  EmbeddedResourceType(const unsigned char *b, int l) : base(b), len(l) {}
  const unsigned char *base;
  int len;
  std::string Get() const { return std::string(base, base + len); }
};

#define EmbeddedResource(filename) \
  EmbeddedResourceType(&CONCAT(data_, filename)[0], CONCAT(CONCAT(data_, filename), _len))

#define GetEmbeddedResource(filename) EmbeddedResource(filename).Get()
#define GetDynamicEmbeddedResource(resource) resource.Get()

namespace OSUtility
{
inline void ForceCrash()
{
  __builtin_trap();
}
inline void DebugBreak()
{
  raise(SIGTRAP);
}
bool DebuggerPresent();
void WriteOutput(int channel, const char *str);
};

namespace Threading
{
struct pthreadLockData
{
  pthread_mutex_t lock;
  pthread_mutexattr_t attr;
};
typedef CriticalSectionTemplate<pthreadLockData> CriticalSection;

struct pthreadRWLockData
{
  pthread_rwlock_t rwlock;
  pthread_rwlockattr_t attr;
};
typedef RWLockTemplate<pthreadRWLockData> RWLock;
};

namespace Bits
{
inline uint32_t CountLeadingZeroes(uint32_t value)
{
  return value == 0 ? 32 : __builtin_clz(value);
}

#if ENABLED(RDOC_X64)
inline uint64_t CountLeadingZeroes(uint64_t value)
{
  return value == 0 ? 64 : __builtin_clzl(value);
}
#endif
};
