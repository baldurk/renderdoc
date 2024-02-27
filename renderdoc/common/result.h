/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2024 Baldur Karlsson
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

#include "api/replay/stringise.h"
#include "common/globalconfig.h"

// forward declared so we don't pull in too many headers
enum class ResultCode : uint32_t;
struct ResultDetails;

struct RDResult
{
  // bit of a hack, rather than including the result code definition just assume 0 is succeeded
  RDResult() : code(ResultCode(0)) {}
  RDResult(ResultCode code) : code(code) {}
  RDResult(ResultCode code, const rdcstr &message) : code(code), message(message) {}
  RDResult(ResultCode code, const rdcliteral &message) : code(code), message(message) {}
  ResultCode code;
  // inflexible string is used here because on desktop it's the size of one pointer, meaning this
  // struct is overall two pointers (the smallest we could make it without packing the code into the
  // string pointer, or using string tables with an index). Since these results are not returned on
  // any very high traffic calls, it's better to prioritise simplicity and directness over tight
  // memory optimisations
  rdcinflexiblestr message;

  bool operator==(ResultCode result) const { return code == result; }
  bool operator!=(ResultCode result) const { return code != result; }
  operator ResultDetails() const;
};

DECLARE_REFLECTION_STRUCT(RDResult);

// helper macros since we often want to print the error message that gets returned.
// one helper returns immediately, the other sets a result and prints - to allow cleanup
#define RETURN_ERROR_RESULT_INTERNAL(code, msg, ...)                                           \
  do                                                                                           \
  {                                                                                            \
    RDResult CONCAT(res, __LINE__)(code, StringFormat::Fmt(STRING_LITERAL(msg), __VA_ARGS__)); \
    RDCERR("%s", CONCAT(res, __LINE__).message.c_str());                                       \
    return CONCAT(res, __LINE__);                                                              \
  } while(0)

#define SET_ERROR_RESULT_INTERNAL(res, code, msg, ...)                         \
  do                                                                           \
  {                                                                            \
    res = RDResult(code, StringFormat::Fmt(STRING_LITERAL(msg), __VA_ARGS__)); \
    RDCERR("%s", res.message.c_str());                                         \
  } while(0)

#define RETURN_WARNING_RESULT_INTERNAL(code, msg, ...)                                         \
  do                                                                                           \
  {                                                                                            \
    RDResult CONCAT(res, __LINE__)(code, StringFormat::Fmt(STRING_LITERAL(msg), __VA_ARGS__)); \
    RDCWARN("%s", CONCAT(res, __LINE__).message.c_str());                                      \
    return CONCAT(res, __LINE__);                                                              \
  } while(0)

#define SET_WARNING_RESULT_INTERNAL(res, code, msg, ...)                       \
  do                                                                           \
  {                                                                            \
    res = RDResult(code, StringFormat::Fmt(STRING_LITERAL(msg), __VA_ARGS__)); \
    RDCWARN("%s", res.message.c_str());                                        \
  } while(0)

// VS automatically elides any trailing comma if there are no arguments above. GCC/Clang are
// stricter and may fail, so we add an extra 0 since it won't get processed by the format anyway
#if ENABLED(RDOC_MSVS)

#define RETURN_ERROR_RESULT RETURN_ERROR_RESULT_INTERNAL
#define RETURN_WARNING_RESULT RETURN_WARNING_RESULT_INTERNAL
#define SET_ERROR_RESULT SET_ERROR_RESULT_INTERNAL
#define SET_WARNING_RESULT SET_WARNING_RESULT_INTERNAL

#else

#define RETURN_ERROR_RESULT(...) RETURN_ERROR_RESULT_INTERNAL(__VA_ARGS__, 0)
#define RETURN_WARNING_RESULT(...) RETURN_WARNING_RESULT_INTERNAL(__VA_ARGS__, 0)
#define SET_ERROR_RESULT(...) SET_ERROR_RESULT_INTERNAL(__VA_ARGS__, 0)
#define SET_WARNING_RESULT(...) SET_WARNING_RESULT_INTERNAL(__VA_ARGS__, 0)

#endif
