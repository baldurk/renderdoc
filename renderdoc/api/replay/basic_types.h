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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <tuple>
#include <vector>

typedef uint8_t byte;

#ifndef DOCUMENT
#define DOCUMENT(text)
#endif

#ifndef DOCUMENT2
#define DOCUMENT2(text1, text2)
#endif

#ifndef DOCUMENT3
#define DOCUMENT3(text1, text2, text3)
#endif

#ifndef DOCUMENT4
#define DOCUMENT4(text1, text2, text3, text4)
#endif

// here we define our own data structures that are ABI compatible between modules, as STL is not
// safe to pass a module boundary.
template <typename A, typename B>
struct rdcpair
{
  A first;
  B second;

  operator std::tuple<A &, B &>() { return std::tie(first, second); }
};

template <typename A, typename B>
rdcpair<A, B> make_rdcpair(const A &a, const B &b)
{
  rdcpair<A, B> ret;
  ret.first = a;
  ret.second = b;
  return ret;
}

// utility class that adds a NULL terminator to array operations only if T == char
template <typename T>
struct null_terminator
{
  // adds 1 to every allocation to ensure we have space. Happens invisibly so even capacity()
  // doesn't know about it
  inline static size_t allocCount(size_t c) { return c; }
  // adds the NULL terminator after a resize operation
  inline static void fixup(T *elems, size_t count) {}
};

template <>
struct null_terminator<char>
{
  inline static size_t allocCount(size_t c) { return c + 1; }
  // indexing 'off the end' of elems is safe because we over-allocated above
  inline static void fixup(char *elems, size_t count) { elems[count] = 0; }
};

template <typename T>
struct rdcarray
{
protected:
  T *elems;
  int32_t allocatedCount;
  int32_t usedCount;

  /////////////////////////////////////////////////////////////////
  // memory management, in a dll safe way
  static T *allocate(size_t count)
  {
#ifdef RENDERDOC_EXPORTS
    return (T *)malloc(count * sizeof(T));
#else
    return (T *)RENDERDOC_AllocArrayMem(count * sizeof(T));
#endif
  }
  static void deallocate(const T *p)
  {
#ifdef RENDERDOC_EXPORTS
    free((void *)p);
#else
    RENDERDOC_FreeArrayMem((const void *)p);
#endif
  }

  inline void setUsedCount(int32_t newCount)
  {
    usedCount = newCount;
    null_terminator<T>::fixup(elems, usedCount);
  }

public:
  typedef T value_type;

  rdcarray() : elems(NULL), allocatedCount(0), usedCount(0) {}
  ~rdcarray()
  {
    // clear will destruct the actual elements still existing
    clear();
    // then we deallocate the backing store
    deallocate(elems);
    elems = NULL;
    allocatedCount = 0;
  }

  /////////////////////////////////////////////////////////////////
  // simple accessors
  T &operator[](size_t i) { return elems[i]; }
  const T &operator[](size_t i) const { return elems[i]; }
  T *data() { return elems; }
  const T *data() const { return elems; }
  T *begin() { return elems ? elems : end(); }
  T *end() { return elems ? elems + usedCount : NULL; }
  T &front() { return *elems; }
  T &back() { return *(elems + usedCount - 1); }
  T &at(size_t idx) { return elems[idx]; }
  const T *begin() const { return elems ? elems : end(); }
  const T *end() const { return elems ? elems + usedCount : NULL; }
  const T &front() const { return *elems; }
  const T &back() const { return *(elems + usedCount - 1); }
  size_t size() const { return (size_t)usedCount; }
  size_t byteSize() const { return (size_t)usedCount * sizeof(T); }
  int32_t count() const { return (int32_t)usedCount; }
  size_t capacity() const { return (size_t)allocatedCount; }
  bool empty() const { return usedCount == 0; }
  bool isEmpty() const { return usedCount == 0; }
  void clear() { resize(0); }
  void push_back(const T &el) { insert(size(), &el, 1); }
  /////////////////////////////////////////////////////////////////
  // managing elements and memory

  void reserve(size_t s)
  {
    // if we're empty then normally reserving s==0 would do nothing, but if we need to append a null
    // terminator then we do actually need to allocate
    if(s == 0 && capacity() == 0 && null_terminator<T>::allocCount(0) > 0)
    {
      elems = allocate(null_terminator<T>::allocCount(0));
      return;
    }

    // nothing to do if we already have this much space. We only size up
    if(s <= capacity())
      return;

    // for now, just resize exactly to what's needed.
    T *newElems = allocate(null_terminator<T>::allocCount(s));

    // copy the elements to new storage
    for(int32_t i = 0; i < usedCount; i++)
      new(newElems + i) T(elems[i]);

    // delete the old elements
    for(int32_t i = 0; i < usedCount; i++)
      elems[i].~T();

    // deallocate tee old storage
    deallocate(elems);

    // swap the storage. usedCount doesn't change
    elems = newElems;

    // update allocated size
    allocatedCount = (int32_t)s;
  }

  void resize(size_t s)
  {
    // do nothing if we're already this size
    if(s == size())
      return;

    int32_t oldCount = usedCount;

    if(s > size())
    {
      // make sure we have backing store allocated
      reserve(s);

      // update the currently allocated count
      setUsedCount((int32_t)s);

      // default initialise the new elements
      for(int32_t i = oldCount; i < usedCount; i++)
        new(elems + i) T();
    }
    else
    {
      // resizing down, we just need to update the count and destruct removed elements
      setUsedCount((int32_t)s);

      for(int32_t i = usedCount; i < oldCount; i++)
        elems[i].~T();
    }
  }

  void insert(size_t offs, const T *el, size_t count)
  {
    const size_t oldSize = size();

    // invalid size
    if(offs > oldSize)
      return;

    size_t newSize = oldSize + count;

    // reserve more space if needed
    reserve(newSize);

    // fast path where offs == size(), for push_back
    if(offs == oldSize)
    {
      // copy construct the new element into place. There was nothing here to destruct as it was
      // unused memory
      for(size_t i = 0; i < count; i++)
        new(elems + offs + i) T(el[i]);
    }
    else
    {
      // we need to shuffle everything up. Iterate from the back in two stages: first into the
      // newly-allocated elements that don't need to be destructed. Then one-by-one destructing an
      // element (which has been moved later), and copy-constructing the new one into place
      //
      // e.g. an array of 6 elements, inserting 3 more at offset 1
      //
      // <==    old data     ==> <== new ==>
      // [0] [1] [2] [3] [4] [5] [6] [7] [8]
      //  A   B   C   D   E   F   .   .   .
      //
      // first pass:
      //
      // [8].copyConstruct([5])
      // [7].copyConstruct([4])
      // [6].copyConstruct([3])
      //
      // [0] [1] [2] [3] [4] [5] [6] [7] [8]
      //  A   B   C   D*  E*  F*  D   E   F
      //
      // * marked elements have been moved now, so they're free to destruct.
      //
      // second pass:
      // [5].destruct()
      // [5].copyConstruct([2])
      // [4].destruct()
      // [4].copyConstruct([1])
      //
      // [0] [1] [2] [3] [4] [5] [6] [7] [8]
      //  A   B*  C*  D*  B   C   D   E   F
      //
      // Note that at each point, we're moving 'count' elements along - 5->8, 4->7, 3->6, 2->5, 1->4
      //
      // [1] through [3] will be destructed next when we actually do the insert
      //
      // if we're inserting more elements than existed before, there may be gaps. E.g. if we
      // inserted 10 in the example above we'd end up with something like:
      //
      // [0] [1] [2] [3] [4] [5] [6] [7] [8] [9] [a] [b] [c] [d] [e] [f]
      //  A   B*  C*  D*  E*  F*  .   .   .   .   .   B   C   D   E   F
      //
      // and the second pass wouldn't have had anything to do.
      // In the next part we just need to check if the slot was < oldCount to know if we should
      // destruct it before inserting.

      // first pass, copy
      for(size_t i = 0; i < count; i++)
        new(elems + oldSize + count - 1 - i) T(elems[oldSize - 1 - i]);

      // second pass, destruct & copy if there was any overlap
      if(count < oldSize - offs)
      {
        size_t overlap = oldSize - offs - count;
        for(size_t i = 0; i < overlap; i++)
        {
          // destruct old element
          elems[oldSize - 1 - i].~T();
          // copy from earlier
          new(elems + oldSize - 1 - i) T(elems[oldSize - 1 - count - i]);
        }
      }

      // elems[offs] to elems[offs + count - 1] are now free to construct into.
      for(size_t i = 0; i < count; i++)
      {
        // if this was one used previously, destruct it
        if(i < oldSize)
          elems[offs + i].~T();

        // then copy construct the new value
        new(elems + offs + i) T(el[i]);
      }
    }

    // update new size
    setUsedCount(usedCount + (int32_t)count);
  }

  // a couple of helpers
  inline void insert(size_t offs, const std::vector<T> &in) { insert(offs, in.data(), in.size()); }
  inline void insert(size_t offs, const std::initializer_list<T> &in)
  {
    insert(offs, in.begin(), in.size());
  }
  inline void insert(size_t offs, const rdcarray<T> &in) { insert(offs, in.data(), in.size()); }
  inline void insert(size_t offs, const T &in) { insert(offs, &in, 1); }
  void erase(size_t offs, size_t count = 1)
  {
    // invalid count
    if(offs + count > size())
      return;

    // this is simpler to implement than insert(). We do two simpler passes:
    //
    // Pass 1: Iterate over the secified range, destruct it.
    // Pass 2: Iterate over the remainder after the range (if it exists), destruct and
    // copy-construct into new place

    // destruct elements to be removed
    for(size_t i = 0; i < count; i++)
      elems[offs + i].~T();

    // move remaining elements into place
    for(size_t i = offs + count; i < size(); i++)
    {
      new(elems + i - count) T(elems[i]);
      elems[i].~T();
    }

    // update new size
    setUsedCount(usedCount - (int32_t)count);
  }
  /////////////////////////////////////////////////////////////////
  // constructors that just forward to assign
  rdcarray(const T *in, size_t count)
  {
    elems = NULL;
    allocatedCount = usedCount = 0;
    assign(in, count);
  }
  rdcarray(const std::vector<T> &in)
  {
    elems = NULL;
    allocatedCount = usedCount = 0;
    assign(in);
  }
  rdcarray(const std::initializer_list<T> &in)
  {
    elems = NULL;
    allocatedCount = usedCount = 0;
    assign(in);
  }
  rdcarray(const rdcarray<T> &in)
  {
    elems = NULL;
    allocatedCount = usedCount = 0;
    assign(in);
  }

  inline void swap(rdcarray<T> &other)
  {
    std::swap(elems, other.elems);
    std::swap(allocatedCount, other.allocatedCount);
    std::swap(usedCount, other.usedCount);
  }

  // assign forwards to operator =
  inline void assign(const std::vector<T> &in) { *this = in; }
  inline void assign(const std::initializer_list<T> &in) { *this = in; }
  inline void assign(const rdcarray<T> &in) { *this = in; }
  /////////////////////////////////////////////////////////////////
  // assignment operators
  rdcarray &operator=(const std::vector<T> &in)
  {
    // make sure we have enough space, allocating more if needed
    reserve(in.size());
    // destruct the old objects
    clear();

    // update new size
    setUsedCount((int32_t)in.size());

    // copy construct the new elems
    for(int32_t i = 0; i < usedCount; i++)
      new(elems + i) T(in[i]);

    null_terminator<T>::fixup(elems, usedCount);

    return *this;
  }

  rdcarray &operator=(const std::initializer_list<T> &in)
  {
    // make sure we have enough space, allocating more if needed
    reserve(in.size());
    // destruct the old objects
    clear();

    // update new size
    setUsedCount((int32_t)in.size());

    // copy construct the new elems
    int32_t i = 0;
    for(const T &t : in)
    {
      new(elems + i) T(t);
      i++;
    }

    null_terminator<T>::fixup(elems, usedCount);

    return *this;
  }

  rdcarray &operator=(const rdcarray &in)
  {
    // do nothing if we're self-assigning
    if(this == &in)
      return *this;

    // make sure we have enough space, allocating more if needed
    reserve(in.size());
    // destruct the old objects
    clear();

    // update new size
    setUsedCount((int32_t)in.size());

    // copy construct the new elems
    for(int32_t i = 0; i < usedCount; i++)
      new(elems + i) T(in[i]);

    null_terminator<T>::fixup(elems, usedCount);

    return *this;
  }

  // assignment with no operator = taking a pointer and length
  inline void assign(const T *in, size_t count)
  {
    // make sure we have enough space, allocating more if needed
    reserve(count);
    // destruct the old objects
    clear();

    // update new size
    setUsedCount((int32_t)count);

    // copy construct the new elems
    for(int32_t i = 0; i < usedCount; i++)
      new(elems + i) T(in[i]);
  }

#if defined(RENDERDOC_QT_COMPAT)
  rdcarray(const QList<T> &in)
  {
    elems = NULL;
    allocatedCount = usedCount = 0;
    assign(in);
  }
  inline void assign(const QList<T> &in) { *this = in; }
  rdcarray &operator=(const QList<T> &in)
  {
    // make sure we have enough space, allocating more if needed
    reserve(in.size());
    // destruct the old objects
    clear();

    // update new size
    setUsedCount(in.count());

    // copy construct the new elems
    for(int32_t i = 0; i < usedCount; i++)
      new(elems + i) T(in[i]);

    return *this;
  }

  rdcarray(const QVector<T> &in)
  {
    elems = NULL;
    allocatedCount = usedCount = 0;
    assign(in);
  }
  inline void assign(const QVector<T> &in) { *this = in; }
  rdcarray &operator=(const QVector<T> &in)
  {
    // make sure we have enough space, allocating more if needed
    reserve(in.size());
    // destruct the old objects
    clear();

    // update new size
    setUsedCount(in.count());

    // copy construct the new elems
    for(int32_t i = 0; i < usedCount; i++)
      new(elems + i) T(in[i]);

    return *this;
  }
#endif
};

DOCUMENT("");
struct rdcstr : public rdcarray<char>
{
  // extra string constructors
  rdcstr() : rdcarray<char>() {}
  rdcstr(const rdcstr &in) : rdcarray<char>() { assign(in); }
  rdcstr(const std::string &in) : rdcarray<char>() { assign(in.c_str(), in.size()); }
  rdcstr(const char *const in) : rdcarray<char>() { assign(in, strlen(in)); }
  // extra string assignment
  rdcstr &operator=(const std::string &in)
  {
    assign(in.c_str(), in.size());
    return *this;
  }
  rdcstr &operator=(const char *const in)
  {
    assign(in, strlen(in));
    return *this;
  }

  // cast operators
  operator std::string() const { return std::string(elems, elems + usedCount); }
#if defined(RENDERDOC_QT_COMPAT)
  operator QString() const { return QString::fromUtf8(elems, usedCount); }
  operator QVariant() const { return QVariant(QString::fromUtf8(elems, usedCount)); }
#endif

  // conventional data accessor
  DOCUMENT("");
  const char *c_str() const { return elems ? elems : ""; }
  // equality checks
  bool operator==(const char *const o) const
  {
    if(!elems)
      return o == NULL;
    return !strcmp(elems, o);
  }
  bool operator==(const std::string &o) const { return o == elems; }
  bool operator==(const rdcstr &o) const { return *this == (const char *const)o.elems; }
  bool operator!=(const char *const o) const { return !(*this == o); }
  bool operator!=(const std::string &o) const { return !(*this == o); }
  bool operator!=(const rdcstr &o) const { return !(*this == o); }
  // define ordering operators
  bool operator<(const rdcstr &o) const { return strcmp(elems, o.elems) < 0; }
  bool operator>(const rdcstr &o) const { return strcmp(elems, o.elems) > 0; }
};

DOCUMENT("");
struct bytebuf : public rdcarray<byte>
{
  bytebuf() : rdcarray<byte>() {}
  bytebuf(const std::vector<byte> &in) : rdcarray<byte>(in) {}
};
