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

#ifdef RDCASSERTMSG
#error RDCASSERTMSG already defined when including custom_assert.h
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////
// excellent set of macros to wrap individual parameters in a varargs macro expansion.
// See: http://stackoverflow.com/a/1872506/4070143
//      http://groups.google.com/group/comp.std.c/browse_thread/thread/77ee8c8f92e4a3fb/346fc464319b1ee5
//
// Some modification needed on VC++.
// See: http://compgroups.net/comp.lang.c++/visual-c++-too-few-many-args-warnings-for-apply/2075805
//
// A few more twiddles by hand to get everything playing nicely

#define RDCASSERT_FAILMSG_1(value) failmsg += (STRINGIZE(value) "=") + ToStr(value) + ", ";
#define RDCASSERT_FAILMSG_2(value, _1)                     \
  failmsg += (STRINGIZE(value) "=") + ToStr(value) + ", "; \
  RDCASSERT_FAILMSG_1(_1)
#define RDCASSERT_FAILMSG_3(value, _1, _2)                 \
  failmsg += (STRINGIZE(value) "=") + ToStr(value) + ", "; \
  RDCASSERT_FAILMSG_2(_1, _2)
#define RDCASSERT_FAILMSG_4(value, _1, _2, _3)             \
  failmsg += (STRINGIZE(value) "=") + ToStr(value) + ", "; \
  RDCASSERT_FAILMSG_3(_1, _2, _3)
#define RDCASSERT_FAILMSG_5(value, _1, _2, _3, _4)         \
  failmsg += (STRINGIZE(value) "=") + ToStr(value) + ", "; \
  RDCASSERT_FAILMSG_4(_1, _2, _3, _4)
#define RDCASSERT_FAILMSG_6(value, _1, _2, _3, _4, _5)     \
  failmsg += (STRINGIZE(value) "=") + ToStr(value) + ", "; \
  RDCASSERT_FAILMSG_5(_1, _2, _3, _4, _5)
#define RDCASSERT_FAILMSG_7(value, _1, _2, _3, _4, _5, _6) \
  failmsg += (STRINGIZE(value) "=") + ToStr(value) + ", "; \
  RDCASSERT_FAILMSG_6(_1, _2, _3, _4, _5, _6)
#define RDCASSERT_FAILMSG_8(value, _1, _2, _3, _4, _5, _6, _7) \
  failmsg += (STRINGIZE(value) "=") + ToStr(value) + ", ";     \
  RDCASSERT_FAILMSG_7(_1, _2, _3, _4, _5, _6, _7)

// this is the terminating clause
#define RDCASSERT_FAILMSG_DISCARD_1(cond)
#define RDCASSERT_FAILMSG_DISCARD_2(cond, _1) RDCASSERT_FAILMSG_1(_1)
#define RDCASSERT_FAILMSG_DISCARD_3(cond, _1, _2) RDCASSERT_FAILMSG_2(_1, _2)
#define RDCASSERT_FAILMSG_DISCARD_4(cond, _1, _2, _3) RDCASSERT_FAILMSG_3(_1, _2, _3)
#define RDCASSERT_FAILMSG_DISCARD_5(cond, _1, _2, _3, _4) RDCASSERT_FAILMSG_4(_1, _2, _3, _4)
#define RDCASSERT_FAILMSG_DISCARD_6(cond, _1, _2, _3, _4, _5) \
  RDCASSERT_FAILMSG_5(_1, _2, _3, _4, _5)
#define RDCASSERT_FAILMSG_DISCARD_7(cond, _1, _2, _3, _4, _5, _6) \
  RDCASSERT_FAILMSG_6(_1, _2, _3, _4, _5, _6)
#define RDCASSERT_FAILMSG_DISCARD_8(cond, _1, _2, _3, _4, _5, _6, _7) \
  RDCASSERT_FAILMSG_7(_1, _2, _3, _4, _5, _6, _7)

#define RDCASSERT_FAILMSG_NARG(...) RDCASSERT_FAILMSG_NARG_(__VA_ARGS__, RDCASSERT_FAILMSG_RSEQ_N())
#define RDCASSERT_FAILMSG_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N
#define RDCASSERT_FAILMSG_RSEQ_N() 8, 7, 6, 5, 4, 3, 2, 1, 0

#define RDCASSERT_FAILMSG(...) RDCASSERT_FAILMSG_(RDCASSERT_FAILMSG_NARG(__VA_ARGS__), __VA_ARGS__)

#define RDCASSERT_GETCOND(cond, ...) cond

#if ENABLED(RDOC_MSVS)

// only needed on VC++, but unfortunately breaks on g++/clang++
#define RDCASSERT_FAILMSG_INVOKE(macro, args) macro args

#define RDCASSERT_FAILMSG_NARG_(...) \
  RDCASSERT_FAILMSG_INVOKE(RDCASSERT_FAILMSG_ARG_N, (__VA_ARGS__))
#define RDCASSERT_FAILMSG_(N, ...) \
  RDCASSERT_FAILMSG_INVOKE(CONCAT(RDCASSERT_FAILMSG_DISCARD_, N), (__VA_ARGS__))

#define RDCASSERT_IFCOND(cond, ...) RDCASSERT_FAILMSG_INVOKE(RDCASSERT_GETCOND, (cond))

#else

#define RDCASSERT_FAILMSG_NARG_(...) RDCASSERT_FAILMSG_ARG_N(__VA_ARGS__)
#define RDCASSERT_FAILMSG_(N, ...) CONCAT(RDCASSERT_FAILMSG_DISCARD_, N)(__VA_ARGS__)

#define RDCASSERT_IFCOND(cond, ...) (cond)

#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define RDCASSERTMSG(msg, ...)                                                            \
  do                                                                                      \
  {                                                                                       \
    if(!(RDCASSERT_IFCOND(__VA_ARGS__)))                                                  \
    {                                                                                     \
      const char custommsg[] = msg;                                                       \
      (void)custommsg;                                                                    \
      std::string assertmsg = "'" STRINGIZE(RDCASSERT_GETCOND(__VA_ARGS__)) "' ";         \
      assertmsg += (sizeof(custommsg) > 1) ? msg " " : "";                                \
      std::string failmsg;                                                                \
      RDCASSERT_FAILMSG(__VA_ARGS__);                                                     \
      if(!failmsg.empty())                                                                \
      {                                                                                   \
        failmsg.pop_back();                                                               \
        failmsg.pop_back();                                                               \
      }                                                                                   \
      std::string combinedmsg = assertmsg + (failmsg.empty() ? "" : "(" + failmsg + ")"); \
      rdcassert(combinedmsg.c_str(), __FILE__, __LINE__, __PRETTY_FUNCTION_SIGNATURE__);  \
      rdclog_flush();                                                                     \
      RDCBREAK();                                                                         \
    }                                                                                     \
  } while((void)0, 0)
