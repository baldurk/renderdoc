/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include <type_traits>
#include "rdcstr.h"

template <typename T>
rdcstr DoStringise(const T &el);

template <typename T, bool is_pointer = std::is_pointer<T>::value>
struct StringConverter
{
  static rdcstr Do(const T &el) { return DoStringise<T>(el); }
};

template <typename T>
struct StringConverter<T, true>
{
  static rdcstr Do(const T &el) { return DoStringise<void *>(el); }
};

template <typename T>
rdcstr ToStr(const T &el)
{
  return StringConverter<T>::Do(el);
}

// helper macros for common enum switch
#define BEGIN_ENUM_STRINGISE(type)                               \
  using enumType = type;                                         \
  static const char unknown_prefix[] = #type "(";                \
  constexpr rdcliteral empty_ret = STRING_LITERAL(#type "(0)");  \
  static_assert(std::is_same<const type &, decltype(el)>::value, \
                "Type in macro doesn't match el");               \
  (void)(enumType) el;                                           \
  switch(el)                                                     \
  {                                                              \
    default: break;

// stringise the parameter
#define STRINGISE_ENUM_CLASS(a) \
  case enumType::a: return STRING_LITERAL(#a);

// stringise the parameter with a custom string
#define STRINGISE_ENUM_CLASS_NAMED(value, str) \
  case enumType::value: return STRING_LITERAL(str);

// stringise the parameter
#define STRINGISE_ENUM(a) \
  case a: return STRING_LITERAL(#a);

// stringise the parameter with a custom string
#define STRINGISE_ENUM_NAMED(value, str) \
  case value: return STRING_LITERAL(str);

// end enum switches
#define END_ENUM_STRINGISE() \
  }                          \
  return (uint32_t)el == 0 ? rdcstr(empty_ret) : unknown_prefix + ToStr((uint32_t)el) + ")";

// helper macros for common bitfield check-and-append
#define BEGIN_BITFIELD_STRINGISE(type)                           \
  using enumType = type;                                         \
  static const char unknown_prefix[] = " | " #type "(";          \
  constexpr rdcliteral empty_ret = STRING_LITERAL(#type "(0)");  \
  static_assert(std::is_same<const type &, decltype(el)>::value, \
                "Type in macro doesn't match el");               \
  uint64_t local = (uint64_t)el;                                 \
  (void)(enumType) el;                                           \
  rdcstr ret;

#define STRINGISE_BITFIELD_VALUE(b) \
  if(el == b)                       \
    return STRING_LITERAL(#b);

#define STRINGISE_BITFIELD_CLASS_VALUE(b) \
  if(el == enumType::b)                   \
    return STRING_LITERAL(#b);

#define STRINGISE_BITFIELD_VALUE_NAMED(b, str) \
  if(el == b)                                  \
    return STRING_LITERAL(str);

#define STRINGISE_BITFIELD_CLASS_VALUE_NAMED(b, str) \
  if(el == enumType::b)                              \
    return STRING_LITERAL(str);

#define STRINGISE_BITFIELD_BIT(b) \
  STRINGISE_BITFIELD_VALUE(b);    \
  if(el & b)                      \
  {                               \
    local &= ~uint64_t(b);        \
    ret += " | " #b;              \
  }

#define STRINGISE_BITFIELD_CLASS_BIT(b) \
  STRINGISE_BITFIELD_CLASS_VALUE(b);    \
  if(el & enumType::b)                  \
  {                                     \
    local &= ~uint64_t(enumType::b);    \
    ret += " | " #b;                    \
  }

#define STRINGISE_BITFIELD_BIT_NAMED(b, str) \
  STRINGISE_BITFIELD_VALUE_NAMED(b, str);    \
  if(el & b)                                 \
  {                                          \
    local &= ~uint64_t(b);                   \
    ret += " | " str;                        \
  }

#define STRINGISE_BITFIELD_CLASS_BIT_NAMED(b, str) \
  STRINGISE_BITFIELD_CLASS_VALUE_NAMED(b, str);    \
  if(el & enumType::b)                             \
  {                                                \
    local &= ~uint64_t(enumType::b);               \
    ret += " | " str;                              \
  }

#define END_BITFIELD_STRINGISE()                          \
  if(local)                                               \
    ret += unknown_prefix + ToStr((uint32_t)local) + ")"; \
                                                          \
  if(ret.empty())                                         \
  {                                                       \
    ret = empty_ret;                                      \
  }                                                       \
  else                                                    \
  {                                                       \
    ret = ret.substr(3);                                  \
  }                                                       \
  return ret;

template <typename T>
inline rdcliteral TypeName();

#define DECLARE_STRINGISE_TYPE(type) \
  template <>                        \
  inline rdcliteral TypeName<type>() \
  {                                  \
    return STRING_LITERAL(#type);    \
  }

// This is a little bit ugly, but not *too* much. We declare the macro for serialised types to
// forward-declare the template used by the serialiser. This will be visible externally, but it will
// do nothing as the template won't be invoked externally. It means we can use the correct macro in
// the external interface headers to pre-declare them as serialisable, without having to have the
// public interface include serialiser.h which leads to horrible circular dependencies.

// main serialisation templated function that is specialised for any type
// that needs to be serialised. Implementations will either be a struct type,
// in which case they'll call Serialise() on each of their members, or
// they'll be some custom leaf type (most basic types are specialised in this
// header) which will call SerialiseValue().

template <class SerialiserType, class T>
void DoSerialise(SerialiserType &ser, T &el);

#ifndef DECLARE_REFLECTION_STRUCT
#define DECLARE_REFLECTION_STRUCT(type) \
  DECLARE_STRINGISE_TYPE(type)          \
  template <class SerialiserType>       \
  void DoSerialise(SerialiserType &ser, type &el);
#endif

// enums don't have anything special to do to serialise, they're all handled automagically once they
// have a DoStringise function
#ifndef DECLARE_REFLECTION_ENUM
#define DECLARE_REFLECTION_ENUM(type) DECLARE_STRINGISE_TYPE(type)
#endif
