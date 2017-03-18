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

#pragma once

#include <intrin.h>
#include <windows.h>
#include "data/resource.h"

#define __PRETTY_FUNCTION_SIGNATURE__ __FUNCSIG__

#define OS_DEBUG_BREAK() __debugbreak()

#define GetEmbeddedResource(filename) GetEmbeddedResourceWin32(CONCAT(RESOURCE_, filename))
string GetEmbeddedResourceWin32(int resource);

namespace StringFormat
{
// useful for converting to wide before passing to OS functions
std::wstring UTF82Wide(const string &s);
};

namespace OSUtility
{
inline void ForceCrash()
{
  *((int *)NULL) = 0;
}
inline void DebugBreak()
{
  __debugbreak();
}
inline bool DebuggerPresent()
{
  return ::IsDebuggerPresent() == TRUE;
}
void WriteOutput(int channel, const char *str);
};

namespace Threading
{
typedef CriticalSectionTemplate<CRITICAL_SECTION> CriticalSection;
};

namespace Bits
{
inline uint32_t CountLeadingZeroes(uint32_t value)
{
  DWORD index;
  BOOLEAN result = _BitScanReverse(&index, value);
  return (result == TRUE) ? (index ^ 31) : 32;
}

#if ENABLED(RDOC_X64)
inline uint64_t CountLeadingZeroes(uint64_t value)
{
  DWORD index;
  BOOLEAN result = _BitScanReverse64(&index, value);
  return (result == TRUE) ? (index ^ 63) : 64;
}
#endif
};
