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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

// we provide a basic templated type that is a fixed array that just contains a pointer to the
// element
// array and a size. This could easily map to C as just void* and size but in C++ at least we can be
// type safe.
//
// While we can use STL elsewhere in the main library, we want to expose data to the UI layer as
// fixed
// arrays so that it's a plain C compatible interface.
namespace rdctype
{
template <typename A, typename B>
struct pair
{
  A first;
  B second;
};

template <typename T>
struct array
{
  T *elems;
  int32_t count;

  array() : elems(0), count(0) {}
  ~array() { Delete(); }
  void Delete()
  {
    for(int32_t i = 0; i < count; i++)
      elems[i].~T();
    deallocate(elems);
    elems = 0;
    count = 0;
  }

#ifdef RENDERDOC_EXPORTS
  static void *allocate(size_t s) { return malloc(s); }
  static void deallocate(const void *p) { free((void *)p); }
#else
  static void *allocate(size_t s) { return RENDERDOC_AllocArrayMem(s); }
  static void deallocate(const void *p) { RENDERDOC_FreeArrayMem(p); }
#endif

  T &operator[](size_t i) { return elems[i]; }
  const T &operator[](size_t i) const { return elems[i]; }
  array(const T *const in)
  {
    elems = 0;
    count = 0;
    *this = in;
  }
  array &operator=(const T *const in);

  array(const std::vector<T> &in)
  {
    elems = 0;
    count = 0;
    *this = in;
  }
  array &operator=(const std::vector<T> &in)
  {
    Delete();
    count = (int32_t)in.size();
    if(count == 0)
    {
      elems = 0;
    }
    else
    {
      elems = (T *)allocate(sizeof(T) * count);
      for(int32_t i = 0; i < count; i++)
        new(elems + i) T(in[i]);
    }
    return *this;
  }

  array(const array &o)
  {
    elems = 0;
    count = 0;
    *this = o;
  }

  array &operator=(const array &o)
  {
    // do nothing if we're self-assigning
    if(this == &o)
      return *this;

    Delete();
    count = o.count;
    if(count == 0)
    {
      elems = 0;
    }
    else
    {
      elems = (T *)allocate(sizeof(T) * o.count);
      for(int32_t i = 0; i < count; i++)
        new(elems + i) T(o.elems[i]);
    }
    return *this;
  }

  // provide some of the familiar stl interface
  size_t size() const { return (size_t)count; }
  void clear() { Delete(); }
  bool empty() const { return count == 0; }
  T *begin() { return elems ? elems : end(); }
  T *end() { return elems ? elems + count : NULL; }
  const T *begin() const { return elems ? elems : end(); }
  const T *end() const { return elems ? elems + count : NULL; }
};

struct str : public rdctype::array<char>
{
  str &operator=(const std::string &in);
  str &operator=(const char *const in);

  str() : rdctype::array<char>() {}
  str(const str &o) : rdctype::array<char>() { *this = o; }
  str(const std::string &o) : rdctype::array<char>() { *this = o; }
  str(const char *const o) : rdctype::array<char>() { *this = o; }
  str &operator=(const str &o)
  {
    // do nothing if we're self-assigning
    if(this == &o)
      return *this;

    Delete();
    count = o.count;
    if(count == 0)
    {
      elems = (char *)allocate(sizeof(char));
      elems[0] = 0;
    }
    else
    {
      elems = (char *)allocate(sizeof(char) * (o.count + 1));
      memcpy(elems, o.elems, sizeof(char) * o.count);
      elems[count] = 0;
    }

    return *this;
  }

  operator const char *() const { return elems ? elems : ""; }
  const char *c_str() const { return elems ? elems : ""; }
};

};    // namespace rdctype
