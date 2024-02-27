/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "api/replay/rdcarray.h"
#include "api/replay/rdcstr.h"
#include "api/replay/version.h"
#include "common/common.h"

struct SDObject;

template <typename T>
struct ConfigVarRegistration;

#define CONFIG_SUPPORT_TYPE(T)                                                    \
  template <>                                                                     \
  struct ConfigVarRegistration<T>                                                 \
  {                                                                               \
    ConfigVarRegistration(rdcliteral name, const T &defaultValue, bool debugOnly, \
                          rdcliteral description);                                \
    const T &value();                                                             \
                                                                                  \
  private:                                                                        \
    SDObject *obj;                                                                \
    T tmp;                                                                        \
  };

CONFIG_SUPPORT_TYPE(rdcstr);
CONFIG_SUPPORT_TYPE(bool);
CONFIG_SUPPORT_TYPE(uint64_t);
CONFIG_SUPPORT_TYPE(uint32_t);
CONFIG_SUPPORT_TYPE(rdcarray<rdcstr>);

#undef CONFIG_SUPPORT_TYPE

#define RDOC_CONFIG(type, name, defaultValue, description)                                \
  static ConfigVarRegistration<type> CONCAT(config, __LINE__)(                            \
      STRING_LITERAL(STRINGIZE(name)), defaultValue, false, STRING_LITERAL(description)); \
  const type &name()                                                                      \
  {                                                                                       \
    return CONCAT(config, __LINE__).value();                                              \
  }
#define RDOC_EXTERN_CONFIG(type, name) extern const type &name();

// debug configs get set to constants in official stable builds, they will remain configurable
// in nightly builds and of course in development builds
#if RENDERDOC_STABLE_BUILD

#define RDOC_DEBUG_CONFIG(type, name, defaultValue, description)                         \
  static ConfigVarRegistration<type> CONCAT(config, __LINE__)(                           \
      STRING_LITERAL(STRINGIZE(name)), defaultValue, true, STRING_LITERAL(description)); \
  const type &name()                                                                     \
  {                                                                                      \
    static const type ret = defaultValue;                                                \
    return ret;                                                                          \
  }
#else

#define RDOC_DEBUG_CONFIG(type, name, defaultValue, description)                         \
  static ConfigVarRegistration<type> CONCAT(config, __LINE__)(                           \
      STRING_LITERAL(STRINGIZE(name)), defaultValue, true, STRING_LITERAL(description)); \
  const type &name()                                                                     \
  {                                                                                      \
    return CONCAT(config, __LINE__).value();                                             \
  }
#endif
