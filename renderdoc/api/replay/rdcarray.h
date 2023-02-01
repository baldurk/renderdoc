/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#include <stdint.h>    // for standard types
#include <string.h>    // for memcpy, etc
#include <functional>
#include <initializer_list>
#include <type_traits>

#ifdef RENDERDOC_EXPORTS
#include <stdlib.h>    // for malloc/free
void RENDERDOC_OutOfMemory(uint64_t sz);
#endif

template <typename T, bool isStd = std::is_trivial<T>::value>
struct ItemHelper
{
  static void initRange(T *first, size_t count)
  {
    for(size_t i = 0; i < count; i++)
      new(first + i) T();
  }

  static int compRange(const T *a, const T *b, size_t count)
  {
    for(size_t i = 0; i < count; i++)
      if(!(a[i] == b[i]))
        return a[i] < b[i] ? -1 : 1;

    return 0;
  }
};

template <typename T>
struct ItemHelper<T, true>
{
  static void initRange(T *first, size_t itemCount) { memset(first, 0, itemCount * sizeof(T)); }
  static int compRange(const T *a, const T *b, size_t count)
  {
    return memcmp(a, b, count * sizeof(T));
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
  static void moveRange(T *dest, T *src, size_t count)
  {
    for(size_t i = 0; i < count; i++)
      new(dest + i) T(std::move(src[i]));
  }
};

template <typename T>
struct ItemCopyHelper<T, true>
{
  static void copyRange(T *dest, const T *src, size_t count)
  {
    memcpy(dest, src, count * sizeof(T));
  }
  static void moveRange(T *dest, const T *src, size_t count)
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
    T *ret = NULL;
#ifdef RENDERDOC_EXPORTS
    ret = (T *)malloc(count * sizeof(T));
    if(ret == NULL)
      RENDERDOC_OutOfMemory(count * sizeof(T));
#else
    ret = (T *)RENDERDOC_AllocArrayMem(count * sizeof(T));
#endif
    return ret;
  }
  static void deallocate(T *p)
  {
#ifdef RENDERDOC_EXPORTS
    free((void *)p);
#else
    RENDERDOC_FreeArrayMem((void *)p);
#endif
  }

  inline void setUsedCount(size_t newCount) { usedCount = newCount; }
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
    return usedCount == o.usedCount && ItemHelper<T>::compRange(elems, o.elems, usedCount) == 0;
  }
  bool operator!=(const rdcarray<T> &o) const { return !(*this == o); }
  bool operator<(const rdcarray<T> &o) const
  {
    // compare the subset of elements in both arrays
    size_t c = usedCount;
    if(o.usedCount < c)
      c = o.usedCount;

    // compare the range
    int comp = ItemHelper<T>::compRange(elems, o.elems, c);
    // if it's not equal, we can return either true or false now
    if(comp != 0)
      return (comp < 0);

    // if they compared equal, the smaller array is less-than (if they're different sizes)
    return usedCount < o.usedCount;
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
  void clear()
  {
    // we specialise clear() so that it doesn't implicitly require a default constructor of T()
    // resize(0);

    size_t sz = size();

    if(sz == 0)
      return;

    setUsedCount(0);

    // destroy the old items
    ItemDestroyHelper<T>::destroyRange(elems, sz);
  }

  /////////////////////////////////////////////////////////////////
  // managing elements and memory

  void reserve(size_t s)
  {
    // nothing to do if we already have this much space. We only size up
    if(s <= capacity())
      return;

    // either double, or allocate what's needed, whichever is bigger. ie. by default we double in
    // size but we don't grow exponentially in 2^n to cover a single really large resize
    if(size_t(allocatedCount) * 2 > s)
      s = size_t(allocatedCount) * 2;

    T *newElems = allocate(s);

    // when elems is NULL, usedCount should also be 0, but add an extra check in here just to
    // satisfy coverity's static analysis which can't figure that out from the copy constructor
    if(elems)
    {
      // copy the elements to new storage
      ItemCopyHelper<T>::moveRange(newElems, elems, usedCount);

      // delete the old elements
      ItemDestroyHelper<T>::destroyRange(elems, usedCount);
    }

    // deallocate the old storage
    deallocate(elems);

    // swap the storage. usedCount doesn't change
    elems = newElems;

    // update allocated size
    allocatedCount = s;
  }

  void resize_for_index(size_t s)
  {
    // do nothing if we're already big enough
    if(size() >= s + 1)
      return;

    // otherwise resize so that [s] is valid
    resize(s + 1);
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

  void push_back(T &&el)
  {
    // if we're pushing from within the array, save the index and move from that index after
    // potentially resizing
    if(begin() <= &el && &el < end())
    {
      size_t idx = &el - begin();
      const size_t lastIdx = size();
      reserve(size() + 1);
      new(elems + lastIdx) T(std::forward<T>(elems[idx]));
      setUsedCount(usedCount + 1);
      return;
    }

    const size_t lastIdx = size();
    reserve(size() + 1);
    new(elems + lastIdx) T(std::forward<T>(el));
    setUsedCount(usedCount + 1);
  }

  template <typename... ConstructArgs>
  void emplace_back(ConstructArgs... args)
  {
    const size_t lastIdx = size();
    reserve(size() + 1);
    new(elems + lastIdx) T(std::forward<ConstructArgs...>(args)...);
    setUsedCount(usedCount + 1);
  }

  // fill the array with 'count' copies of 'el'
  void fill(size_t count, const T &el)
  {
    // destruct any old elements
    clear();
    // ensure we have enough space for the count
    reserve(count);
    // copy-construct all elements in place and update space
    for(size_t i = 0; i < count; i++)
      new(elems + i) T(el);
    setUsedCount(count);
  }

  void insert(size_t offs, const T *el, size_t count)
  {
    if(count == 0)
      return;

    if(elems < el + count && el < elems + allocatedCount)
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
      // newly-allocated elements that don't need to be destructed. Then one-by-one
      // move-constructing the new one into place
      //
      // e.g. an array of 6 elements, inserting 3 more at offset 1
      //
      // <==    old data     ==> <== new ==>
      // [0] [1] [2] [3] [4] [5] [6] [7] [8]
      //  A   B   C   D   E   F   .   .   .
      //
      // first pass:
      //
      // [8].moveConstruct([5])
      // [7].moveConstruct([4])
      // [6].moveConstruct([3])
      //
      // [0] [1] [2] [3] [4] [5] [6] [7] [8]
      //  A   B   C   D*  E*  F*  D   E   F
      //
      // * marked elements have been moved now, so they're free to destruct.
      //
      // second pass:
      // [5].destruct()
      // [5].moveConstruct([2])
      // [4].destruct()
      // [4].moveConstruct([1])
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

      // first pass, move construct elements in place
      const size_t moveCount = count < oldSize ? count : oldSize;
      for(size_t i = 0; i < moveCount; i++)
      {
        new(elems + oldSize + count - 1 - i) T(std::move(elems[oldSize - 1 - i]));
      }

      // second pass, move any overlap. We're moving *into* any elements that got moved *out of*
      // above
      if(count < oldSize - offs)
      {
        size_t overlap = oldSize - offs - count;
        for(size_t i = 0; i < overlap; i++)
        {
          // destruct old element
          ItemDestroyHelper<T>::destroyRange(elems + oldSize - 1 - i, 1);
          // copy from earlier
          new(elems + oldSize - 1 - i) T(std::move(elems[oldSize - 1 - count - i]));
        }
      }

      // elems[offs] to elems[offs + count - 1] are now free to construct into.
      for(size_t i = 0; i < count; i++)
      {
        // if this was one used previously, destruct it
        if(i < oldSize)
          ItemDestroyHelper<T>::destroyRange(elems + offs + i, 1);

        // then copy construct the new value
        new(elems + offs + i) T(el[i]);
      }
    }

    // update new size
    setUsedCount(usedCount + count);
  }

  // a couple of helpers
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

  // insert by moving
  inline void insert(size_t offs, T &&el)
  {
    const size_t oldSize = size();

    // invalid size
    if(offs > oldSize)
      return;

    if(begin() <= &el && &el < end())
    {
      // if we're inserting from within our range, save the index
      size_t idx = &el - begin();

      // do any potentially reallocating resize
      reserve(oldSize + 1);

      // fast path where offs == size(), for push_back
      if(offs == oldSize)
      {
        new(elems + offs) T(std::move(elems[idx]));
      }
      else
      {
        // we need to shuffle everything up by one

        // first pass, move construct elements and destruct as we go
        const size_t moveCount = oldSize - offs;
        for(size_t i = 0; i < moveCount; i++)
        {
          new(elems + oldSize - i) T(std::move(elems[oldSize - i - 1]));
          ItemDestroyHelper<T>::destroyRange(elems + oldSize - i - 1, 1);
        }

        // if idx moved as a result of the insert, it will be coming from a different place
        if(idx >= offs)
          idx++;

        // then move construct the new value.
        new(elems + offs) T(std::move(elems[idx]));
      }

      // update new size
      setUsedCount(usedCount + 1);

      return;
    }

    // reserve more space if needed
    reserve(oldSize + 1);

    // fast path where offs == size(), for push_back
    if(offs == oldSize)
    {
      new(elems + offs) T(std::move(el));
    }
    else
    {
      // we need to shuffle everything up by one

      // first pass, move construct elements and destruct as we go
      const size_t moveCount = oldSize - offs;
      for(size_t i = 0; i < moveCount; i++)
      {
        new(elems + oldSize - i) T(std::move(elems[oldSize - i - 1]));
        ItemDestroyHelper<T>::destroyRange(elems + oldSize - i - 1, 1);
      }

      // then move construct the new value
      new(elems + offs) T(std::move(el));
    }

    // update new size
    setUsedCount(usedCount + 1);
  }

  // helpful shortcut for 'append at end', basically a multi-element push_back
  inline void append(const T *el, size_t count) { insert(size(), el, count); }
  inline void append(const rdcarray<T> &in) { insert(size(), in.data(), in.size()); }
  // overload for 'append from move' to move all the elements individually even though we can't move
  // the allocation obviously.
  inline void append(rdcarray<T> &&in)
  {
    reserve(size() + in.size());
    for(size_t i = 0; i < in.size(); i++)
      push_back(std::move(in[i]));
    // don't have to clear here, since moved object can be left in any indeterminate but valid state
    // (an all the members are in that state, while the array is fully valid), but this gives fewer
    // surprises.
    in.clear();
  }
  void erase(size_t offs, size_t count = 1)
  {
    if(count == 0)
      return;

    const size_t sz = size();

    // invalid offset
    if(offs >= sz)
      return;

    if(count > sz - offs)
      count = sz - offs;

    // this is simpler to implement than insert(). We do two simpler passes:
    //
    // Pass 1: Iterate over the secified range, destruct it.
    // Pass 2: Iterate over the remainder after the range (if it exists), destruct and
    // copy-construct into new place

    // destruct elements to be removed
    ItemDestroyHelper<T>::destroyRange(elems + offs, count);

    // move remaining elements into place
    for(size_t i = offs + count; i < sz; i++)
    {
      new(elems + i - count) T(std::move(elems[i]));
      ItemDestroyHelper<T>::destroyRange(elems + i, 1);
    }

    // update new size
    setUsedCount(usedCount - count);
  }

  void pop_back()
  {
    if(!empty())
      erase(size() - 1);
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

  void removeIf(std::function<bool(const T &)> predicate)
  {
    for(size_t i = 0; i < size();)
    {
      if(predicate(at(i)))
      {
        erase(i);
        // continue with same i
      }
      else
      {
        // move to next i
        i++;
      }
    }
  }

  void removeOneIf(std::function<bool(const T &)> predicate)
  {
    for(size_t i = 0; i < size(); i++)
    {
      if(predicate(at(i)))
      {
        erase(i);
        break;
      }
    }
  }

  /////////////////////////////////////////////////////////////////
  // constructors that just forward to assign
  rdcarray(const T *in, size_t count)
  {
    elems = NULL;
    allocatedCount = usedCount = 0;
    assign(in, count);
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

  // move operator/constructor using swap

  rdcarray &operator=(rdcarray &&in)
  {
    // if we have old elems, clear (to destruct) and deallocate
    if(elems)
    {
      clear();
      deallocate(elems);
    }

    // set ourselves to a pristine state
    elems = NULL;
    allocatedCount = 0;
    usedCount = 0;

    // now swap with the incoming array, so it becomes empty
    swap(in);

    return *this;
  }

  rdcarray(rdcarray &&in)
  {
    // set ourselves to a pristine state
    elems = NULL;
    allocatedCount = 0;
    usedCount = 0;

    // now swap with the incoming array, so it becomes empty
    swap(in);
  }

  // assign forwards to operator =
  inline void assign(const std::initializer_list<T> &in) { *this = in; }
  inline void assign(const rdcarray<T> &in) { *this = in; }
  /////////////////////////////////////////////////////////////////
  // assignment operators
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

// fixed size array, wrapped to be more python-friendly (mapped to an N-tuple)
template <typename T, size_t N>
struct rdcfixedarray
{
public:
  /////////////////////////////////////////////////////////////////
  // simple accessors
  T &operator[](size_t i) { return elems[i]; }
  const T &operator[](size_t i) const { return elems[i]; }
  bool operator==(const rdcfixedarray<T, N> &o) const
  {
    return ItemHelper<T>::compRange(elems, o.elems, N) == 0;
  }
  bool operator!=(const rdcfixedarray<T, N> &o) const { return !(*this == o); }
  bool operator<(const rdcfixedarray<T, N> &o) const
  {
    return ItemHelper<T>::compRange(elems, o.elems, N) < 0;
  }
  T *data() { return elems; }
  const T *data() const { return elems; }
  T *begin() { return elems; }
  T *end() { return elems + N; }
  T &front() { return *elems; }
  T &back() { return *(elems + N - 1); }
  T &at(size_t idx) { return elems[idx]; }
  const T *begin() const { return elems; }
  const T *end() const { return elems + N; }
  const T &front() const { return *elems; }
  const T &back() const { return *(elems + N - 1); }
  const T &at(size_t idx) const { return elems[idx]; }
  size_t size() const { return N; }
  size_t byteSize() const { return N * sizeof(T); }
  int32_t count() const { return (int32_t)N; }
  // find the first occurrence of an element
  int32_t indexOf(const T &el, size_t first = 0, size_t last = ~0U) const
  {
    for(size_t i = first; i < N && i < last; i++)
    {
      if(elems[i] == el)
        return (int32_t)i;
    }

    return -1;
  }

  // return true if an element is found
  bool contains(const T &el) const { return indexOf(el) != -1; }
  rdcfixedarray<T, N> &operator=(const std::initializer_list<T> &in)
  {
    size_t i = 0;
    for(const T &t : in)
    {
      elems[i] = t;
      i++;

      if(i >= N)
        break;
    }

    return *this;
  }
  rdcfixedarray<T, N> &operator=(const T (&in)[N])
  {
    for(size_t i = 0; i < N; i++)
      elems[i] = in[i];

    return *this;
  }
  rdcfixedarray() = default;
  rdcfixedarray(const T (&in)[N])
  {
    for(size_t i = 0; i < N; i++)
      elems[i] = in[i];
  }
  rdcfixedarray(const std::initializer_list<T> &in)
  {
    static_assert(std::is_trivial<T>::value,
                  "rdcfixedarray should only be used with POD types like float or uint32_t.");

    // consume all available in the initializer_list, up to N
    size_t i = 0;
    for(const T &t : in)
    {
      elems[i] = t;
      i++;

      if(i >= N)
        break;
    }

    // default-initialise any others
    for(; i < N; i++)
      elems[i] = T();
  }

private:
  T elems[N];
};

typedef uint8_t byte;

struct bytebuf : public rdcarray<byte>
{
  bytebuf() : rdcarray<byte>() {}
  bytebuf(const std::initializer_list<byte> &in) : rdcarray<byte>(in) {}
  bytebuf(const byte *in, size_t size) : rdcarray<byte>(in, size) {}
#if defined(RENDERDOC_QT_COMPAT)
  bytebuf(const QByteArray &in)
  {
    resize(in.size());
    memcpy(elems, in.data(), (size_t)in.size());
  }
  operator QByteArray() const { return QByteArray((const char *)elems, (int32_t)usedCount); }
#endif
};
