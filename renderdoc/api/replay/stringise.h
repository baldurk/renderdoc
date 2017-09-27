/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Baldur Karlsson
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

#include <string>
#include <type_traits>

template <typename T>
std::string DoStringise(const T &el);

template <typename T, bool is_pointer = std::is_pointer<T>::value>
struct StringConverter
{
  static std::string Do(const T &el) { return DoStringise<T>(el); }
};

template <typename T>
struct StringConverter<T, true>
{
  static std::string Do(const T &el) { return DoStringise<void *>(el); }
};

template <typename T>
std::string ToStr(const T &el)
{
  return StringConverter<T>::Do(el);
}

// helper macros for common enum switch
#define BEGIN_ENUM_STRINGISE(type)                               \
  using enumType = type;                                         \
  static const char unknown_prefix[] = #type "<";                \
  static_assert(std::is_same<const type &, decltype(el)>::value, \
                "Type in macro doesn't match el");               \
  (void)(enumType) el;                                           \
  switch(el)                                                     \
  {                                                              \
    default: break;

// stringise the parameter
#define STRINGISE_ENUM_CLASS(a) \
  case enumType::a: return #a;

// stringise the parameter with a custom string
#define STRINGISE_ENUM_CLASS_NAMED(value, str) \
  case enumType::value: return str;

// stringise the parameter
#define STRINGISE_ENUM(a) \
  case a: return #a;

// stringise the parameter with a custom string
#define STRINGISE_ENUM_NAMED(value, str) \
  case value: return str;

// end enum switches
#define END_ENUM_STRINGISE() \
  }                          \
  return unknown_prefix + ToStr((uint32_t)el) + ">";

// helper macros for common bitfield check-and-append
#define BEGIN_BITFIELD_STRINGISE(type)                           \
  using enumType = type;                                         \
  static const char unknown_prefix[] = " | " #type "(";          \
  static_assert(std::is_same<const type &, decltype(el)>::value, \
                "Type in macro doesn't match el");               \
  uint32_t local = (uint32_t)el;                                 \
  (void)(enumType) el;                                           \
  std::string ret;

#define STRINGISE_BITFIELD_VALUE(b) \
  if(el == b)                       \
    return #b;

#define STRINGISE_BITFIELD_CLASS_VALUE(b) \
  if(el == enumType::b)                   \
    return #b;

#define STRINGISE_BITFIELD_VALUE_NAMED(b, str) \
  if(el == b)                                  \
    return str;

#define STRINGISE_BITFIELD_CLASS_VALUE_NAMED(b, str) \
  if(el == enumType::b)                              \
    return str;

#define STRINGISE_BITFIELD_BIT(b) \
  if(el & b)                      \
  {                               \
    local -= (uint32_t)b;         \
    ret += " | " #b;              \
  }

#define STRINGISE_BITFIELD_CLASS_BIT(b) \
  if(el & enumType::b)                  \
  {                                     \
    local -= (uint32_t)enumType::b;     \
    ret += " | " #b;                    \
  }

#define STRINGISE_BITFIELD_BIT_NAMED(b, str) \
  if(el & b)                                 \
  {                                          \
    local -= (uint32_t)b;                    \
    ret += " | " str;                        \
  }

#define STRINGISE_BITFIELD_CLASS_BIT_NAMED(b, str) \
  if(el & enumType::b)                             \
  {                                                \
    local -= (uint32_t)b;                          \
    ret += " | " str;                              \
  }

#define END_BITFIELD_STRINGISE()                \
  if(local)                                     \
    ret += unknown_prefix + ToStr(local) + ")"; \
                                                \
  if(!ret.empty())                              \
  {                                             \
    ret = ret.substr(3);                        \
  }                                             \
  return ret;

template <typename T>
inline const char *TypeName();

#define DECLARE_STRINGISE_TYPE(type)  \
  template <>                         \
  inline const char *TypeName<type>() \
  {                                   \
    return #type;                     \
  }
