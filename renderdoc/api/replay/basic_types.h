/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include <type_traits>
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

// primarily here just to remove a dependency on QDateTime in the Qt UI, so we don't have to bind
// against Qt at all in the interface.
DOCUMENT("");
struct rdcdatetime
{
  DOCUMENT("");
  int32_t year = 0;
  int32_t month = 0;
  int32_t day = 0;
  int32_t hour = 0;
  int32_t minute = 0;
  int32_t second = 0;
  int32_t microsecond = 0;

  rdcdatetime() = default;

  rdcdatetime(int y, int mn, int d, int h = 0, int m = 0, int s = 0, int us = 0)
      : year(y), month(mn), day(d), hour(h), minute(m), second(s), microsecond(us)
  {
  }

  bool operator==(const rdcdatetime &o) const
  {
    return year == o.year && month == o.month && day == o.day && hour == o.hour &&
           minute == o.minute && second == o.second && microsecond == o.microsecond;
  }
  bool operator!=(const rdcdatetime &o) const { return !(*this == o); }
  bool operator<(const rdcdatetime &o) const
  {
    if(year != o.year)
      return year < o.year;
    if(month != o.month)
      return month < o.month;
    if(day != o.day)
      return day < o.day;
    if(hour != o.hour)
      return hour < o.hour;
    if(minute != o.minute)
      return minute < o.minute;
    if(second != o.second)
      return second < o.second;
    if(microsecond != o.microsecond)
      return microsecond < o.microsecond;
    return false;
  }

#if defined(RENDERDOC_QT_COMPAT)
  rdcdatetime(const QDateTime &in)
  {
    year = in.date().year();
    month = in.date().month();
    day = in.date().day();
    hour = in.time().hour();
    minute = in.time().minute();
    second = in.time().second();
    microsecond = in.time().msec() * 1000;
  }
  operator QDateTime() const
  {
    return QDateTime(QDate(year, month, day), QTime(hour, minute, second, microsecond / 1000));
  }
  operator QVariant() const { return QVariant(QDateTime(*this)); }
#endif
};

// here we define our own data structures that are ABI compatible between modules, as STL is not
// safe to pass a module boundary.
template <typename A, typename B>
struct rdcpair
{
  A first;
  B second;

  rdcpair(const A &a, const B &b) : first(a), second(b) {}
  rdcpair() = default;
  rdcpair(const rdcpair<A, B> &o) = default;
  rdcpair(rdcpair<A, B> &&o) = default;
  ~rdcpair() = default;
  inline void swap(rdcpair<A, B> &o)
  {
    std::swap(first, o.first);
    std::swap(second, o.second);
  }

  template <typename A_, typename B_>
  rdcpair<A, B> &operator=(const rdcpair<A_, B_> &o)
  {
    first = o.first;
    second = o.second;
    return *this;
  }

  rdcpair<A, B> &operator=(const rdcpair<A, B> &o)
  {
    first = o.first;
    second = o.second;
    return *this;
  }

  bool operator==(const rdcpair<A, B> &o) const { return first == o.first && second == o.second; }
  bool operator<(const rdcpair<A, B> &o) const
  {
    if(first != o.first)
      return first < o.first;
    return second < o.second;
  }
};

template <typename A, typename B>
rdcpair<A, B> make_rdcpair(const A &a, const B &b)
{
  return rdcpair<A, B>(a, b);
}

template <typename A, typename B>
rdcpair<A &, B &> rdctie(A &a, B &b)
{
  return rdcpair<A &, B &>(a, b);
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

template <typename T, bool isStd = std::is_trivial<T>::value>
struct ItemHelper
{
  static void initRange(T *first, size_t count)
  {
    for(size_t i = 0; i < count; i++)
      new(first + i) T();
  }

  static bool equalRange(T *a, T *b, size_t count)
  {
    for(size_t i = 0; i < count; i++)
      if(!(a[i] == b[i]))
        return false;

    return true;
  }

  static bool lessthanRange(T *a, T *b, size_t count)
  {
    for(size_t i = 0; i < count; i++)
      if(a[i] < b[i])
        return true;

    return false;
  }
};

template <typename T>
struct ItemHelper<T, true>
{
  static void initRange(T *first, size_t itemCount) { memset(first, 0, itemCount * sizeof(T)); }
  static bool equalRange(T *a, T *b, size_t count) { return !memcmp(a, b, count * sizeof(T)); }
  static bool lessthanRange(T *a, T *b, size_t count)
  {
    return memcmp(a, b, count * sizeof(T)) < 0;
  }
};

// ItemCopyHelper checks if memcpy can be used over placement new

template <typename T, bool isStd = std::is_trivially_copyable<T>::value>
struct ItemCopyHelper
{
  static void copyRange(T *dest, const T *src, size_t count)
  {
    for(size_t i = 0; i < count; i++)
      new(dest + i) T(src[i]);
  }
};

template <typename T>
struct ItemCopyHelper<T, true>
{
  static void copyRange(T *dest, const T *src, size_t count)
  {
    memcpy(dest, src, count * sizeof(T));
  }
};

// ItemDestroyHelper checks if the destructor is trivial/do-nothing and can be skipped

template <typename T, bool isStd = std::is_trivially_destructible<T>::value>
struct ItemDestroyHelper
{
  static void destroyRange(T *first, size_t count)
  {
    for(size_t i = 0; i < count; i++)
      (first + i)->~T();
  }
};

template <typename T>
struct ItemDestroyHelper<T, true>
{
  static void destroyRange(T *first, size_t itemCount) {}
};

template <typename T>
struct rdcarray
{
protected:
  T *elems;
  size_t allocatedCount;
  size_t usedCount;

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

  inline void setUsedCount(size_t newCount)
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
  bool operator==(const rdcarray<T> &o) const
  {
    return usedCount == o.usedCount && ItemHelper<T>::equalRange(elems, o.elems, usedCount);
  }
  bool operator<(const rdcarray<T> &o) const
  {
    if(usedCount != o.usedCount)
      return usedCount < o.usedCount;
    return ItemHelper<T>::lessthanRange(elems, o.elems, usedCount);
  }
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
  const T &at(size_t idx) const { return elems[idx]; }
  size_t size() const { return usedCount; }
  size_t byteSize() const { return usedCount * sizeof(T); }
  int32_t count() const { return (int32_t)usedCount; }
  size_t capacity() const { return allocatedCount; }
  bool empty() const { return usedCount == 0; }
  bool isEmpty() const { return usedCount == 0; }
  void clear() { resize(0); }
  /////////////////////////////////////////////////////////////////
  // managing elements and memory

  void reserve(size_t s)
  {
    // if we're empty then normally reserving s==0 would do nothing, but if we need to append a null
    // terminator then we do actually need to allocate
    if(s == 0 && capacity() == 0 && elems == NULL && null_terminator<T>::allocCount(0) > 0)
    {
      elems = allocate(null_terminator<T>::allocCount(0));
      return;
    }

    // nothing to do if we already have this much space. We only size up
    if(s <= capacity())
      return;

    // either double, or allocate what's needed, whichever is bigger. ie. by default we double in
    // size but we don't grow exponentially in 2^n to cover a single really large resize
    if(size_t(allocatedCount) * 2 > s)
      s = size_t(allocatedCount) * 2;

    T *newElems = allocate(null_terminator<T>::allocCount(s));

    // when elems is NULL, usedCount should also be 0, but add an extra check in here just to
    // satisfy coverity's static analysis which can't figure that out from the copy constructor
    if(elems)
    {
      // copy the elements to new storage
      ItemCopyHelper<T>::copyRange(newElems, elems, usedCount);

      // delete the old elements
      ItemDestroyHelper<T>::destroyRange(elems, usedCount);
    }

    // deallocate tee old storage
    deallocate(elems);

    // swap the storage. usedCount doesn't change
    elems = newElems;

    // update allocated size
    allocatedCount = s;
  }

  void resize(size_t s)
  {
    // do nothing if we're already this size
    if(s == size())
      return;

    size_t oldCount = usedCount;

    if(s > size())
    {
      // make sure we have backing store allocated
      reserve(s);

      // update the currently allocated count
      setUsedCount(s);

      // default initialise the new elements
      ItemHelper<T>::initRange(elems + oldCount, usedCount - oldCount);
    }
    else
    {
      // resizing down, we just need to update the count and destruct removed elements
      setUsedCount(s);

      ItemDestroyHelper<T>::destroyRange(elems + usedCount, oldCount - usedCount);
    }
  }

  void push_back(const T &el)
  {
    // in-line implementation here instead of insert()
    const size_t lastIdx = size();
    reserve(size() + 1);
    new(elems + lastIdx) T(el);
    setUsedCount(usedCount + 1);
  }

  void insert(size_t offs, const T *el, size_t count)
  {
    if(el + count >= begin() && end() >= el)
    {
      // we're inserting from ourselves, so if we did this blindly we'd potentially change the
      // contents of the inserted range while doing the insertion.
      // To fix that, we store our original data in a temp and copy into ourselves again. Then we
      // insert from the range (which now points to the copy) and let it be destroyed.
      // This could be more efficient as an append and then a rotate, but this is simpler for now.
      rdcarray<T> copy;
      copy.swap(*this);
      this->reserve(copy.capacity());
      *this = copy;
      return insert(offs, el, count);
    }

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
      size_t copyCount = count < oldSize ? count : oldSize;
      for(size_t i = 0; i < copyCount; i++)
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
    setUsedCount(usedCount + count);
  }

  // a couple of helpers
  inline void insert(size_t offs, const std::vector<T> &in) { insert(offs, in.data(), in.size()); }
  inline void insert(size_t offs, const std::initializer_list<T> &in)
  {
    insert(offs, in.begin(), in.size());
  }
  inline void insert(size_t offs, const rdcarray<T> &in) { insert(offs, in.data(), in.size()); }
  inline void insert(size_t offs, const T &in)
  {
    if(&in < begin() || &in > end())
    {
      insert(offs, &in, 1);
    }
    else
    {
      T copy(in);
      insert(offs, &copy, 1);
    }
  }
  // helpful shortcut for 'append at end', basically a multi-element push_back
  inline void append(const T *el, size_t count) { insert(size(), el, count); }
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
    setUsedCount(usedCount - count);
  }

  /////////////////////////////////////////////////////////////////
  // Qt style helper functions

  // erase & return an index
  T takeAt(size_t offs)
  {
    T ret = elems[offs];
    erase(offs);
    return ret;
  }

  // find the first occurrence of an element
  int32_t indexOf(const T &el, size_t first = 0, size_t last = ~0U) const
  {
    for(size_t i = first; i < usedCount && i < last; i++)
    {
      if(elems[i] == el)
        return (int32_t)i;
    }

    return -1;
  }

  // return true if an element is found
  bool contains(const T &el) const { return indexOf(el) != -1; }
  // remove the first occurrence of an element
  void removeOne(const T &el)
  {
    int idx = indexOf(el);
    if(idx >= 0)
      erase((size_t)idx);
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
    setUsedCount(in.size());

    // copy construct the new elems
    ItemCopyHelper<T>::copyRange(elems, in.data(), usedCount);

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
    setUsedCount(in.size());

    // copy construct the new elems
    size_t i = 0;
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
    setUsedCount(in.size());

    // copy construct the new elems
    ItemCopyHelper<T>::copyRange(elems, in.data(), usedCount);

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
    setUsedCount(count);

    // copy construct the new elems
    ItemCopyHelper<T>::copyRange(elems, in, usedCount);
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
    for(size_t i = 0; i < usedCount; i++)
      new(elems + i) T(in[(int32_t)i]);

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
    for(size_t i = 0; i < usedCount; i++)
      new(elems + i) T(in[(int32_t)i]);

    return *this;
  }
#endif
};

#include "rdcstr.h"

DOCUMENT("");
struct bytebuf : public rdcarray<byte>
{
  bytebuf() : rdcarray<byte>() {}
  bytebuf(const std::vector<byte> &in) : rdcarray<byte>(in) {}
#if defined(RENDERDOC_QT_COMPAT)
  bytebuf(const QByteArray &in)
  {
    resize(in.size());
    memcpy(elems, in.data(), (size_t)in.size());
  }
  operator QByteArray() const
  {
    return QByteArray::fromRawData((const char *)elems, (int32_t)usedCount);
  }
#endif
};

typedef rdcpair<rdcstr, rdcstr> rdcstrpair;
typedef rdcarray<rdcstrpair> rdcstrpairs;
