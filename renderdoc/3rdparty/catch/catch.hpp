/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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

// this file exists just to wrap the *real* catch.hpp and define any configuration defines we always
// want on.

#define CATCH_CONFIG_FALLBACK_STRINGIFIER ToStrAsStdString
#define CATCH_CONFIG_FORCE_FALLBACK_STRINGIFIER
#define CATCH_CONFIG_INLINE_DEBUG_BREAK

// define the debugbreak to not be in a lambda, so that we get the right stack frame!
#define CATCH_BREAK_INTO_DEBUGGER() \
  if(Catch::isDebuggerActive())     \
  {                                 \
    CATCH_TRAP();                   \
  }

#include "api/replay/rdcstr.h"
#include "api/replay/stringise.h"

#include <ostream>
#include <string>

template <typename T>
std::string ToStrAsStdString(const T &el)
{
  rdcstr s = ToStr(el);
  return std::string(s.begin(), s.end());
}

inline std::ostream &operator<<(std::ostream &os, rdcstr const &str)
{
  return os << std::string(str.begin(), str.end());
}

#include "official/catch.hpp"

namespace Catch
{
template <>
struct StringMaker<rdcstr>
{
  static std::string convert(rdcstr const &value)
  {
    return std::string(value.begin(), value.end());
  }
};
}
