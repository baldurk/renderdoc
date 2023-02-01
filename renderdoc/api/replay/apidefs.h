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

#include <stdint.h>

// Guidelines for documentation:
//
// * If you only need a short string, use DOCUMENT("Here is my string");
// * If your string is only just over the limit by clang-format, allow it to be reformatted and
//   moved to a new line as necessary.
// * If your string is a couple of lines long or a paragraph or more, use raw C++11 string literals
//   like so:
//   R"(Here is my string. It is fairly long so I am going to break the first line over a paragraph
//   boundary like so, so that I have enough room to continue it.
//
//   A second paragraph can be used like so. Note that the first line is right after the opening
//   quotation mark, but the terminating bracket and quote should be on a new line.
//   )"
// * Use :class:`ClassName` to refer to classes, :data:`ClassName.constant` to refer to constants or
//   member variables, and :meth:`ClassName.method` to refer to member functions. You can also link
//   to the external documentation with :ref:`external-ref-name`. Function parameters can be
//   referenced with :paramref:`parameter`.
// * For constants like ``None`` or ``True`` use the python term (i.e. ``None`` not ``NULL``) and
//   surround with double backticks ``.
// * Likewise use python types to refer to basic types - ``str``, ``int``, ``float``, etc.
// * All values for enums should be documented in the docstring for the enum itself, you can't
//   document the values. See the examples in replay_enums.h for the syntax
// * Take care not to go too far over 100 columns, if you're using raw C++11 string literals then
//   clang-format won't reformat them into the column limit.
// * Type annotations should follow python typing rules - e.g. List[int] for rdcarray<uint32_t>.
//   All parameters and return types should be fully documented, and any 'complex' struct members
//   i.e. lists, tuples, and other structs, should be given an explicit type in their docstring with
//   :type:.
//
#ifndef DOCUMENT
#define DOCUMENT(text)
#endif

// There's a bug in visual assist that stops highlighting if a raw string is too long. It looks like
// it happens when it reaches 128 lines long or ~5000 bytes which is quite suspicious.
// Anyway since this doesn't come up that often, we split the following docstring in two part-way
// through. Don't be alarmed, just move along
#ifndef DOCUMENT2
#define DOCUMENT2(text1, text2)
#endif

#ifndef DOCUMENT3
#define DOCUMENT3(text1, text2, text3)
#endif

#ifndef DOCUMENT4
#define DOCUMENT4(text1, text2, text3, text4)
#endif

#if defined(RENDERDOC_PLATFORM_WIN32)

#define RENDERDOC_EXPORT_API __declspec(dllexport)
#define RENDERDOC_IMPORT_API __declspec(dllimport)
#define RENDERDOC_CC __cdecl

#elif defined(RENDERDOC_PLATFORM_LINUX) || defined(RENDERDOC_PLATFORM_APPLE) || \
    defined(RENDERDOC_PLATFORM_ANDROID) || defined(RENDERDOC_PLATFORM_GGP) ||   \
    defined(RENDERDOC_PLATFORM_SWITCH)

#define RENDERDOC_EXPORT_API __attribute__((visibility("default")))
#define RENDERDOC_IMPORT_API

#define RENDERDOC_CC

#else

#error "Unknown platform"

#endif

// define the API visibility depending on whether we're exporting
#ifdef RENDERDOC_EXPORTS
#define RENDERDOC_API RENDERDOC_EXPORT_API
#else
#define RENDERDOC_API RENDERDOC_IMPORT_API
#endif

#ifdef NO_ENUM_CLASS_OPERATORS

#define BITMASK_OPERATORS(a)
#define ITERABLE_OPERATORS(a)

#else

#include <type_traits>

// helper template that allows the result of & to be cast back to the enum or explicitly cast to
// bool for use in if() or ?: or so on or compared against 0.
//
// If you get an error about missing operator then you're probably doing something like
// (bitfield & value) == 0 or (bitfield & value) != 0 or similar. Instead prefer:
// !(bitfield & value)     or (bitfield & value) to make use of the bool cast directly
template <typename enum_name>
struct EnumCastHelper
{
public:
  constexpr EnumCastHelper(enum_name v) : val(v) {}
  constexpr operator enum_name() const { return val; }
  constexpr explicit operator bool() const
  {
    typedef typename std::underlying_type<enum_name>::type etype;
    return etype(val) != 0;
  }

private:
  const enum_name val;
};

// helper templates for iterating over all values in an enum that has sequential values and is
// to be used for array indices or something like that.
template <typename enum_name>
struct ValueIterContainer
{
  struct ValueIter
  {
    ValueIter(enum_name v) : val(v) {}
    enum_name val;
    enum_name operator*() const { return val; }
    bool operator!=(const ValueIter &it) const { return !(val == *it); }
    const inline enum_name operator++()
    {
      ++val;
      return val;
    }
  };

  ValueIter begin() { return ValueIter(enum_name::First); }
  ValueIter end() { return ValueIter(enum_name::Count); }
};

template <typename enum_name>
struct IndexIterContainer
{
  typedef typename std::underlying_type<enum_name>::type etype;

  struct IndexIter
  {
    IndexIter(enum_name v) : val(v) {}
    enum_name val;
    etype operator*() const { return etype(val); }
    bool operator!=(const IndexIter &it) const { return !(val == it.val); }
    const inline enum_name operator++()
    {
      ++val;
      return val;
    }
  };

  IndexIter begin() { return IndexIter(enum_name::First); }
  IndexIter end() { return IndexIter(enum_name::Count); }
};

template <typename enum_name>
constexpr inline ValueIterContainer<enum_name> values()
{
  return ValueIterContainer<enum_name>();
};

template <typename enum_name>
constexpr inline IndexIterContainer<enum_name> indices()
{
  return IndexIterContainer<enum_name>();
};

template <typename enum_name>
constexpr inline unsigned int arraydim()
{
  typedef typename std::underlying_type<enum_name>::type etype;
  return (unsigned int)etype(enum_name::Count);
};

#define ENUM_ARRAY_SIZE(enum_name) int(enum_name::Count)

// clang-format makes a even more of a mess of this multi-line macro than it usually does, for some
// reason. So we just disable it since it's still readable and this isn't really the intended case
// we are using clang-format for.

// clang-format off
#define BITMASK_OPERATORS(enum_name)                                           \
                                                                               \
constexpr inline enum_name operator|(enum_name a, enum_name b)                 \
{                                                                              \
  typedef typename std::underlying_type<enum_name>::type etype;                \
  return enum_name(etype(a) | etype(b));                                       \
}                                                                              \
                                                                               \
constexpr inline EnumCastHelper<enum_name> operator&(enum_name a, enum_name b) \
{                                                                              \
  typedef typename std::underlying_type<enum_name>::type etype;                \
  return EnumCastHelper<enum_name>(enum_name(etype(a) & etype(b)));            \
}                                                                              \
                                                                               \
constexpr inline enum_name operator~(enum_name a)                              \
{                                                                              \
  typedef typename std::underlying_type<enum_name>::type etype;                \
  return enum_name(~etype(a));                                                 \
}                                                                              \
                                                                               \
inline enum_name &operator|=(enum_name &a, enum_name b)                        \
{ return a = a | b; }                                                          \
                                                                               \
inline enum_name &operator&=(enum_name &a, enum_name b)                        \
{ return a = a & b; }

#define ITERABLE_OPERATORS(enum_name)                                          \
                                                                               \
inline enum_name operator++(enum_name &a)                                      \
{                                                                              \
  typedef typename std::underlying_type<enum_name>::type etype;                \
  return a = enum_name(etype(a)+1);                                            \
}
// clang-format on

#endif
