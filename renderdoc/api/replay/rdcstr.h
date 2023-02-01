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

#include <stdint.h>     // for standard types
#include <string.h>     // for memcpy, etc
#include <algorithm>    // for std::swap

#ifdef RENDERDOC_EXPORTS
#include <stdlib.h>    // for malloc/free
void RENDERDOC_OutOfMemory(uint64_t sz);
#endif

class rdcinflexiblestr;

// special type for storing literals. This allows functions to force callers to pass them literals
class rdcliteral
{
  const char *str;
  size_t len;

  // make the literal operator a friend so it can construct fixed strings. No-one else can.
  friend constexpr rdcliteral operator"" _lit(const char *str, size_t len);

  // similarly friend inflexible strings to allow them to decompose to a literal
  friend class rdcinflexiblestr;

  constexpr rdcliteral(const char *s, size_t l) : str(s), len(l) {}
  rdcliteral() = delete;

public:
  const char *c_str() const { return str; }
  size_t length() const { return len; }
  const char *begin() const { return str; }
  const char *end() const { return str + len; }
};

inline constexpr rdcliteral operator"" _lit(const char *str, size_t len)
{
  return rdcliteral(str, len);
}

class rdcstr
{
private:
  // ARRAY_STATE is deliberately 0 so that 0-initialisation is a valid empty array string
  static constexpr size_t ARRAY_STATE = size_t(0);
  static constexpr size_t ALLOC_STATE = size_t(1) << ((sizeof(size_t) * 8) - 2);
  static constexpr size_t FIXED_STATE = size_t(1) << ((sizeof(size_t) * 8) - 1);

  struct alloc_ptr_rep
  {
    // we reserve two bits but we only have three states
    static constexpr size_t CAPACITY_MASK = (~size_t(0)) >> 2;
    static constexpr size_t STATE_MASK = ~CAPACITY_MASK;

    // the storage
    char *str;
    // the current size of the string (less than or equal to capacity). Doesn't include NULL
    // terminator
    size_t size;

    // accessors for capacity, preserving the state bits
    size_t get_capacity() const { return _capacity & CAPACITY_MASK; };
    void set_capacity(size_t s) { _capacity = ALLOC_STATE | s; }
  private:
    // the capacity currently available in the allocated storage. Doesn't include NULL terminator
    size_t _capacity;
  };

  struct fixed_ptr_rep
  {
    // the immutable string storage
    const char *str;
    // the size of the immutable string. Doesn't include NULL terminator
    size_t size;
    // access to the flags
    size_t flags;
  };

  struct arr_rep
  {
    // all bytes except the last one are used for storing short strings
    char str[sizeof(size_t) * 3 - 1];
    // capacity is fixed - 1 less than the number of characters above (so we always have room for
    // the NULL terminator)
    static const size_t capacity = sizeof(arr_rep::str) - 1;

    // don't have to mask any state bits here because we assume the size is in bounds and state bits
    // of 0 means array representation, so setting and retrieving can return the size as-is.
    // We keep these accessors though just in case that changes in future
    size_t get_size() const { return _size; }
    void set_size(size_t s) { _size = (unsigned char)s; }
  private:
    // we only have 6-bits of this available is enough for up to 63 size, more than what we can
    // store anyway.
    unsigned char _size;
  };

  // zero-initialised this becomes an empty string in array format
  union string_data
  {
    // stored as size, capacity, and pointer to d
    alloc_ptr_rep alloc;
    // stored as size and pointer
    fixed_ptr_rep fixed;
    // stored as in-line array
    arr_rep arr;
  } d;

  bool is_alloc() const { return !!(d.fixed.flags & ALLOC_STATE); }
  bool is_fixed() const { return !!(d.fixed.flags & FIXED_STATE); }
  bool is_array() const { return !is_alloc() && !is_fixed(); }
  // allow inflexible string to introspect to see if we're a literal
  friend class rdcinflexiblestr;

  /////////////////////////////////////////////////////////////////
  // memory management, in a dll safe way

  static char *allocate(size_t count)
  {
    char *ret = NULL;
#ifdef RENDERDOC_EXPORTS
    ret = (char *)malloc(count);
    if(ret == NULL)
      RENDERDOC_OutOfMemory(count);
#else
    ret = (char *)RENDERDOC_AllocArrayMem(count);
#endif
    return ret;
  }
  static void deallocate(char *p)
  {
#ifdef RENDERDOC_EXPORTS
    free((void *)p);
#else
    RENDERDOC_FreeArrayMem((void *)p);
#endif
  }

  // if we're not already mutable (i.e. fixed string) then change to a mutable string
  void ensure_mutable(size_t s = 0)
  {
    if(!is_fixed())
      return;

    // if we're not yet mutable, convert to allocated string at the same time as reserving as
    // necessary

    const char *fixed_str = d.fixed.str;
    size_t fixed_size = d.fixed.size;

    // allocate at least enough for the string - reserve is non-destructive.
    if(s < fixed_size)
      s = fixed_size;

    // if we can satisfy the request with the array representation, it's easier
    if(s <= d.arr.capacity)
    {
      // copy d, we can safely include the NULL terminator we know is present
      memcpy(d.arr.str, fixed_str, fixed_size + 1);

      // store metadata
      d.arr.set_size(fixed_size);
    }
    else
    {
      // otherwise we need to allocate

      // allocate the requested size now, +1 for NULL terminator
      d.alloc.str = allocate(s + 1);
      // copy d, we can safely include the NULL terminator we know is present
      memcpy(d.alloc.str, fixed_str, fixed_size + 1);

      // store metadata
      d.alloc.set_capacity(fixed_size);
      d.alloc.size = fixed_size;
    }
  }

public:
  // default constructor just 0-initialises
  rdcstr() { memset(&d, 0, sizeof(d)); }
  ~rdcstr()
  {
    // only free d if it was allocated
    if(is_alloc())
      deallocate(d.alloc.str);
  }
  // move constructor is simple - just move the d element. We take ownership of the allocation if
  // it's allocated, otherwise this is a copy anyway
  rdcstr(rdcstr &&in)
  {
    // we can just move the d element
    d = in.d;

    // the input no longer owns d. Set to 0 to be extra-clear
    memset(&in.d, 0, sizeof(d));
  }
  rdcstr &operator=(rdcstr &&in)
  {
    // deallocate current storage if it's allocated
    if(is_alloc())
      deallocate(d.alloc.str);

    // move the d element
    d = in.d;

    // the input no longer owns d. Set to 0 to be extra-clear
    memset(&in.d, 0, sizeof(d));

    return *this;
  }

  // special constructor from literals
  rdcstr(const rdcliteral &lit)
  {
    d.fixed.str = lit.c_str();
    d.fixed.size = lit.length();
    d.fixed.flags = FIXED_STATE;
  }

  // copy constructors forward to assign
  rdcstr(const rdcstr &in)
  {
    memset(&d, 0, sizeof(d));
    assign(in);
  }
  rdcstr(const char *const in)
  {
    memset(&d, 0, sizeof(d));
    assign(in, strlen(in));
  }
  rdcstr(const char *const in, size_t length)
  {
    memset(&d, 0, sizeof(d));
    assign(in, length);
  }
  // also operator=
  rdcstr &operator=(const rdcstr &in)
  {
    assign(in);
    return *this;
  }
  rdcstr &operator=(const char *const in)
  {
    assign(in, strlen(in));
    return *this;
  }

  inline void swap(rdcstr &other)
  {
    // just need to swap the d element
    std::swap(d, other.d);
  }

  // assign from an rdcstr, copy the d element and allocate if needed
  void assign(const rdcstr &in)
  {
    // do nothing if we're self-assigning
    if(this == &in)
      return;

    // if the input d is allocated, we need to make our own allocation. Go through the standard
    // string assignment function which will allocate & copy
    if(in.is_alloc())
    {
      assign(in.d.alloc.str, in.d.alloc.size);
    }
    else
    {
      // otherwise just deallocate if necessary and copy
      if(is_alloc())
        deallocate(d.alloc.str);

      d = in.d;
    }
  }

  // assign from something else
  void assign(const char *const in, size_t length)
  {
    // ensure we have enough capacity allocated
    reserve(length);

    // write to the string we're using, depending on if we allocated or not
    char *str = is_alloc() ? d.alloc.str : d.arr.str;
    // copy the string itself
    memcpy(str, in, length);
    // cap off with NULL terminator
    str[length] = 0;

    if(is_alloc())
      d.alloc.size = length;
    else
      d.arr.set_size(length);
  }

  void assign(const char *const str) { assign(str, strlen(str)); }
  // in-place modification functions
  void append(const char *const str) { append(str, strlen(str)); }
  void append(const rdcstr &str) { append(str.c_str(), str.size()); }
  void append(const char *const str, size_t length) { insert(size(), str, length); }
  void erase(size_t offs, size_t count)
  {
    const size_t sz = size();

    // invalid offset
    if(offs >= sz)
      return;

    if(count > sz - offs)
      count = sz - offs;

    char *str = data();
    for(size_t i = offs; i < sz - count; i++)
      str[i] = str[i + count];

    resize(sz - count);
  }

  void insert(size_t offset, const char *const str) { insert(offset, str, strlen(str)); }
  void insert(size_t offset, const rdcstr &str) { insert(offset, str.c_str(), str.size()); }
  void insert(size_t offset, char c) { insert(offset, &c, 1); }
  void insert(size_t offset, const char *const instr, size_t length)
  {
    if(!is_fixed() && instr + length >= begin() && end() >= instr)
    {
      // we're inserting from ourselves and we're not allocated, so if we did this blindly
      // we'd potentially change the contents of the inserted range while doing the insertion.
      // To fix that, we store our original data in a temp and copy into ourselves again. Then we
      // insert from the temp copy and let it be destroyed.
      // This could be more efficient as an append and then a rotate, but this is simpler for now.
      rdcstr copy;
      copy.swap(*this);
      this->reserve(copy.capacity() + length);
      *this = copy;
      insert(offset, copy.c_str(), copy.size());
      return;
    }

    const size_t sz = size();

    // invalid offset
    if(offset > sz)
      return;

    // allocate needed size
    reserve(sz + length);

    // move anything after the offset upwards, including the NULL terminator by starting at sz + 1
    char *str = data();
    for(size_t i = sz + 1; i > offset; i--)
      str[i + length - 1] = str[i - 1];

    // copy the string to the offset
    memcpy(str + offset, instr, length);

    // increase the length
    if(is_alloc())
      d.alloc.size += length;
    else
      d.arr.set_size(sz + length);
  }

  void replace(size_t offset, size_t length, const rdcstr &str)
  {
    erase(offset, length);
    insert(offset, str);
  }

  // fill the string with 'count' copies of 'c'
  void fill(size_t count, char c)
  {
    resize(count);
    memset(data(), c, count);
  }

  // read-only by-value accessor can look up directly in c_str() since it can't be modified
  const char &operator[](size_t i) const { return c_str()[i]; }
  // assignment operator must make the string mutable first
  char &operator[](size_t i)
  {
    ensure_mutable();
    return is_alloc() ? d.alloc.str[i] : d.arr.str[i];
  }

  // stl type interface
  void reserve(size_t s)
  {
    if(is_fixed())
    {
      ensure_mutable(s);
      return;
    }

    const size_t old_capacity = capacity();

    // nothing to do if we already have this much space. We only size up
    if(s <= old_capacity)
      return;

    // if we're currently using the array representation, the current capacity is always maxed out,
    // meaning if we don't have enough space we *must* now allocate.

    const size_t old_size = is_alloc() ? d.alloc.size : d.arr.get_size();
    const char *old_str = is_alloc() ? d.alloc.str : d.arr.str;

    // either double, or allocate what's needed, whichever is bigger. ie. by default we double in
    // size but we don't grow exponentially in 2^n to cover a single really large resize
    if(old_capacity * 2 > s)
      s = old_capacity * 2;

    // allocate +1 for the NULL terminator
    char *new_str = allocate(s + 1);

    // copy the current characters over, including NULL terminator
    memcpy(new_str, old_str, old_size + 1);

    // deallocate the old storage
    if(is_alloc())
      deallocate(d.alloc.str);

    // we are now an allocated string
    d.alloc.str = new_str;

    // updated capacity
    d.alloc.set_capacity(s);
    // size is unchanged
    d.alloc.size = old_size;
  }

  void push_back(char c)
  {
    // store old size
    size_t s = size();

    // reserve enough memory and ensure we're mutable
    reserve(s + 1);

    // append the character
    if(is_alloc())
    {
      d.alloc.size++;
      d.alloc.str[s] = c;
      d.alloc.str[s + 1] = 0;
    }
    else
    {
      d.arr.set_size(s + 1);
      d.arr.str[s] = c;
      d.arr.str[s + 1] = 0;
    }
  }

  void pop_back()
  {
    if(!empty())
      resize(size() - 1);
  }

  void resize(const size_t s)
  {
    // if s is 0, fast path - if we're allocated just change the size, otherwise reset to an empty
    // array representation.
    if(s == 0)
    {
      if(is_alloc())
      {
        d.alloc.size = 0;
        d.alloc.str[0] = 0;
        return;
      }
      else
      {
        // either we're a fixed string, and we need to become an empty array, or we're already an
        // array in which case we empty the array.
        memset(&d, 0, sizeof(d));
        return;
      }
    }

    const size_t oldSize = size();

    // call reserve first. This handles resizing up, and also making the string mutable if necessary
    reserve(s);

    // if the size didn't change, return.
    if(s == oldSize)
      return;

    // now resize the string
    if(is_alloc())
    {
      // if we resized upwards, memset the new elements to 0, if we resized down set the new NULL
      // terminator
      if(s > oldSize)
        memset(d.alloc.str + oldSize, 0, s - oldSize + 1);
      else
        d.alloc.str[s] = 0;

      // update the size.
      d.alloc.size = s;
    }
    else
    {
      // if we resized upwards, memset the new elements to 0, if we resized down set the new NULL
      // terminator
      if(s > oldSize)
        memset(d.arr.str + oldSize, 0, s - oldSize + 1);
      else
        d.arr.str[s] = 0;

      // update the size.
      d.arr.set_size(s);
    }
  }

  size_t capacity() const
  {
    if(is_alloc())
      return d.alloc.get_capacity();
    if(is_fixed())
      return d.fixed.size;
    return d.arr.capacity;
  }
  size_t size() const
  {
    if(is_alloc() || is_fixed())
      return d.fixed.size;
    return d.arr.get_size();
  }
  size_t length() const { return size(); }
  const char *c_str() const
  {
    if(is_alloc() || is_fixed())
      return d.alloc.str;
    return d.arr.str;
  }

  void clear() { resize(0); }
  bool empty() const { return size() == 0; }
  const char *data() const { return c_str(); }
  char *data()
  {
    ensure_mutable();
    return is_alloc() ? d.alloc.str : d.arr.str;
  }
  const char *begin() const { return c_str(); }
  const char *end() const { return c_str() + size(); }
  char *begin() { return data(); }
  char *end() { return data() + size(); }
  char front() const { return *c_str(); }
  char &front()
  {
    ensure_mutable();
    return *data();
  }
  char back() const { return *(end() - 1); }
  char &back()
  {
    ensure_mutable();
    return data()[size() - 1];
  }

  rdcstr substr(size_t offs, size_t length = ~0U) const
  {
    const size_t sz = size();
    if(offs >= sz)
      return rdcstr();

    if(length == ~0U || offs + length > sz)
      length = sz - offs;

    return rdcstr(c_str() + offs, length);
  }

  rdcstr &operator+=(const char *const str)
  {
    append(str, strlen(str));
    return *this;
  }
  rdcstr &operator+=(const rdcstr &str)
  {
    append(str.c_str(), str.size());
    return *this;
  }
  rdcstr &operator+=(char c)
  {
    push_back(c);
    return *this;
  }
  rdcstr operator+(const char *const str) const
  {
    rdcstr ret = *this;
    ret += str;
    return ret;
  }
  rdcstr operator+(const rdcstr &str) const
  {
    rdcstr ret = *this;
    ret += str;
    return ret;
  }
  rdcstr operator+(const char c) const
  {
    rdcstr ret = *this;
    ret += c;
    return ret;
  }

  // Qt-type interface
  bool isEmpty() const { return size() == 0; }
  int32_t count() const { return (int32_t)size(); }
  char takeAt(int32_t offs)
  {
    char ret = c_str()[offs];
    erase(offs, 1);
    return ret;
  }

  // Python interface
  int32_t indexOf(char el, int32_t first = 0, int32_t last = -1) const
  {
    if(first < 0)
      return -1;

    const char *str = c_str();
    size_t sz = size();

    if(last >= 0 && (size_t)last < sz)
      sz = last;

    for(size_t i = first; i < sz; i++)
    {
      if(str[i] == el)
        return (int32_t)i;
    }

    return -1;
  }

  // find a substring. Optionally starting at a given 'first' character and not including an
  // optional 'last' character. If last is -1 (the default), the whole string is searched
  int32_t find(const char *needle_str, size_t needle_len, int32_t first, int32_t last) const
  {
    // no default parameters for first/last here because otherwise it'd be dangerous with casting
    // and
    // misusing this instead of just specifying a 'first' below.
    const char *haystack = c_str();
    size_t haystack_len = size();

    if(first < 0)
      return -1;

    if(needle_len == 0)
      return 0;

    if(last >= 0 && (size_t)last < haystack_len)
      haystack_len = last;

    if((size_t)first >= haystack_len)
      return -1;

    if(needle_len > haystack_len - first)
      return -1;

    for(size_t i = first; i <= haystack_len - needle_len; i++)
    {
      if(strncmp(haystack + i, needle_str, needle_len) == 0)
        return (int32_t)i;
    }

    return -1;
  }

  int32_t find(const char needle, int32_t first = 0, int32_t last = -1) const
  {
    if(first < 0)
      return -1;

    const char *haystack = c_str();
    size_t haystack_len = size();

    if(last >= 0 && (size_t)last < haystack_len)
      haystack_len = last;

    for(size_t i = first; i < haystack_len; i++)
    {
      if(haystack[i] == needle)
        return (int32_t)i;
    }

    return -1;
  }

  int32_t find(const rdcstr &needle, int32_t first = 0, int32_t last = -1) const
  {
    return find(needle.c_str(), needle.size(), first, last);
  }
  int32_t find(const char *needle, int32_t first = 0, int32_t last = -1) const
  {
    return find(needle, strlen(needle), first, last);
  }

  // find the first character that is in a given set of characters, from the start of the string
  // Optionally starting at a given 'first' character and not including an optional 'last'
  // character. If last is -1 (the default), the whole string is searched
  int32_t find_first_of(const rdcstr &needle_set, int32_t first = 0, int32_t last = -1) const
  {
    return find_first_last(needle_set, true, true, first, last);
  }
  // find the first character that is not in a given set of characters, from the start of the string
  int32_t find_first_not_of(const rdcstr &needle_set, int32_t first = 0, int32_t last = -1) const
  {
    return find_first_last(needle_set, true, false, first, last);
  }
  // find the first character that is in a given set of characters, from the end of the string
  int32_t find_last_of(const rdcstr &needle_set, int32_t first = 0, int32_t last = -1) const
  {
    return find_first_last(needle_set, false, true, first, last);
  }
  // find the first character that is not in a given set of characters, from the end of the string
  int32_t find_last_not_of(const rdcstr &needle_set, int32_t first = 0, int32_t last = -1) const
  {
    return find_first_last(needle_set, false, false, first, last);
  }

private:
  int32_t find_first_last(const rdcstr &needle_set, bool forward_search, bool search_in_set,
                          int32_t first, int32_t last) const
  {
    if(first < 0)
      return -1;

    const char *haystack = c_str();
    size_t haystack_len = size();

    if(last >= 0 && (size_t)last < haystack_len)
      haystack_len = last;

    if(forward_search)
    {
      for(size_t i = first; i < haystack_len; i++)
      {
        bool in_set = needle_set.contains(haystack[i]);
        if(in_set == search_in_set)
          return (int32_t)i;
      }
    }
    else
    {
      for(size_t i = 0; i < haystack_len; i++)
      {
        size_t idx = haystack_len - 1 - i;

        if(idx < (size_t)first)
          break;

        bool in_set = needle_set.contains(haystack[idx]);
        if(in_set == search_in_set)
          return (int32_t)idx;
      }
    }

    return -1;
  }

public:
  bool contains(char needle) const { return indexOf(needle) != -1; }
  bool contains(const rdcstr &needle) const { return find(needle) != -1; }
  bool contains(const char *needle) const { return find(needle) != -1; }
  bool beginsWith(const rdcstr &beginning) const
  {
    if(beginning.length() > length())
      return false;

    return !strncmp(c_str(), beginning.c_str(), beginning.length());
  }
  bool endsWith(const rdcstr &ending) const
  {
    if(ending.length() > length())
      return false;

    return !strcmp(c_str() + length() - ending.length(), ending.c_str());
  }

  void removeOne(char el)
  {
    int idx = indexOf(el);
    if(idx >= 0)
      erase((size_t)idx, 1);
  }

  // remove any preceeding or trailing whitespace from the string, in-place
  void trim()
  {
    if(empty())
      return;

#define IS_WHITESPACE(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\n' || (c) == '\0')

    const char *str = c_str();
    size_t sz = size();

    size_t start = 0;
    while(start < sz && IS_WHITESPACE(str[start]))
      start++;

    // no non-whitespace characters, become the empty string
    if(start == sz)
    {
      clear();
      return;
    }

    size_t end = sz - 1;
    while(end > start && IS_WHITESPACE(str[end]))
      end--;

    erase(end + 1, ~0U);
    erase(0, start);
  }

  // return a copy of the string with preceeding and trailing whitespace removed
  rdcstr trimmed() const
  {
    rdcstr ret = *this;
    ret.trim();
    return ret;
  }

  // for equality check with rdcstr, check quickly for empty string comparisons
  bool operator==(const rdcstr &o) const
  {
    if(o.size() == 0)
      return size() == 0;
    return !strcmp(o.c_str(), c_str());
  }

  // equality checks for other types, just check string directly
  bool operator==(const char *const o) const
  {
    if(o == NULL)
      return size() == 0;
    return !strcmp(o, c_str());
  }
  // for inverse check just reverse results of above
  bool operator!=(const char *const o) const { return !(*this == o); }
  bool operator!=(const rdcstr &o) const { return !(*this == o); }
  // define ordering operators
  bool operator<(const rdcstr &o) const { return strcmp(c_str(), o.c_str()) < 0; }
  bool operator>(const rdcstr &o) const { return strcmp(c_str(), o.c_str()) > 0; }
// Qt compatibility
#if defined(RENDERDOC_QT_COMPAT)
  rdcstr(const QString &in)
  {
    QByteArray arr = in.toUtf8();
    memset(&d, 0, sizeof(d));
    assign(arr.data(), (size_t)arr.size());
  }
  rdcstr(const QChar &in)
  {
    QByteArray arr = QString(in).toUtf8();
    memset(&d, 0, sizeof(d));
    assign(arr.data(), (size_t)arr.size());
  }
  operator QString() const { return QString::fromUtf8(c_str(), (int32_t)size()); }
  operator QVariant() const { return QVariant(QString::fromUtf8(c_str(), (int32_t)size())); }
  rdcstr &operator+=(const QString &str)
  {
    QByteArray arr = str.toUtf8();
    append(arr.data(), (size_t)arr.size());
    return *this;
  }
  rdcstr operator+(const QString &str) const
  {
    rdcstr ret = *this;
    ret += str;
    return ret;
  }
  rdcstr &operator+=(const QChar &chr)
  {
    QByteArray arr = QString(chr).toUtf8();
    append(arr.data(), (size_t)arr.size());
    return *this;
  }
  rdcstr operator+(const QChar &chr) const
  {
    rdcstr ret = *this;
    ret += QString(chr);
    return ret;
  }
#endif
};

// macro that can append _lit to a macro parameter
#define STRING_LITERAL2(string) string##_lit
#define STRING_LITERAL(string) STRING_LITERAL2(string)

inline rdcstr operator+(const char *const left, const rdcstr &right)
{
  return rdcstr(left) += right;
}

inline bool operator==(const char *const left, const rdcstr &right)
{
  return right == left;
}

inline bool operator!=(const char *const left, const rdcstr &right)
{
  return right != left;
}

#if defined(RENDERDOC_QT_COMPAT)
inline rdcstr operator+(const QString &left, const rdcstr &right)
{
  return rdcstr(left) += right;
}

inline rdcstr operator+(const QChar &left, const rdcstr &right)
{
  return rdcstr(left) += right;
}
#endif

// this class generally should not be used directly. You almost always want rdcstr (or rarely
// rdcliteral) instead. This class is used for structured data where the vast majority of the time
// it stores a literal and is only accessed for the string contents, but it has to allow for
// modification just in case the structured data is dynamically generated.
// It is optimised for storage space and uses a single tagged pointer on x64 - if the tag is set,
// the pointer is to a null-terminated literal, otherwise it is to an allocated null-terminated
// string. There are no built-in modification functions but it can be assigned from an rdcstr and
// converted to an rdcstr - whenever it's assigned, the old storage is deallocated and new storage
// is allocated so it is highly inefficient if the string is being modified.
class rdcinflexiblestr
{
  static char *allocate(size_t count)
  {
    char *ret = NULL;
#ifdef RENDERDOC_EXPORTS
    ret = (char *)malloc(count);
    if(ret == NULL)
      RENDERDOC_OutOfMemory(count);
#else
    ret = (char *)RENDERDOC_AllocArrayMem(count);
#endif
    return ret;
  }
  static void deallocate(char *p)
  {
#ifdef RENDERDOC_EXPORTS
    free((void *)p);
#else
    RENDERDOC_FreeArrayMem((void *)p);
#endif
  }

// we use tagged pointers on x86-64 to minimise storage. On other architecture this isn't safe
// so we have to keep it separate. This is still a storage win over rdcstr

#if defined(__x86_64__) || defined(_M_X64)
  // use a signed pointer to sign-extend for canonical form
  intptr_t pointer : 63;
  intptr_t is_literal : 1;
#else
  intptr_t pointer;
  intptr_t is_literal;
#endif

public:
  rdcinflexiblestr()
  {
    pointer = (intptr_t)(void *)"";
    is_literal = 0;
    is_literal |= 0x1;
  }
  ~rdcinflexiblestr()
  {
    if(is_literal == 0)
      deallocate((char *)(uintptr_t)pointer);
    pointer = 0;
    is_literal = 0;
  }
  rdcinflexiblestr(rdcinflexiblestr &&in)
  {
    pointer = in.pointer;
    is_literal = in.is_literal;
    in.pointer = 0;
    in.is_literal = 0;
  }
  rdcinflexiblestr &operator=(rdcinflexiblestr &&in)
  {
    if(is_literal == 0)
      deallocate((char *)(uintptr_t)pointer);

    pointer = in.pointer;
    is_literal = in.is_literal;
    in.pointer = 0;
    in.is_literal = 0;

    return *this;
  }
  rdcinflexiblestr(const rdcinflexiblestr &in)
  {
    pointer = 0;
    is_literal = 0;
    *this = in;
  }

  rdcinflexiblestr(const rdcliteral &lit)
  {
    pointer = (intptr_t)lit.c_str();
    is_literal = 0;
    is_literal |= 0x1;
  }
  rdcinflexiblestr &operator=(const rdcliteral &in)
  {
    if(is_literal == 0)
      deallocate((char *)(uintptr_t)pointer);
    pointer = (intptr_t)in.c_str();
    is_literal |= 0x1;
    return *this;
  }
  rdcinflexiblestr(const rdcstr &in)
  {
    pointer = 0;
    is_literal = 0;
    *this = in;
  }
  rdcinflexiblestr &operator=(const rdcstr &in)
  {
    if(is_literal == 0)
      deallocate((char *)(uintptr_t)pointer);

    // unbox a literal from the rdcstr if it has one
    if(in.is_fixed())
    {
      pointer = (intptr_t)in.c_str();
      is_literal |= 0x1;
    }
    else
    {
      // always allocate for rdcstr, don't try to unbox a literal if one exists
      size_t size = in.size() + 1;
      void *dst = allocate(size);
      memcpy(dst, in.c_str(), size);
      pointer = (intptr_t)dst;
      is_literal = 0;
    }
    return *this;
  }
  rdcinflexiblestr &operator=(const rdcinflexiblestr &in)
  {
    if(is_literal == 0)
      deallocate((char *)(uintptr_t)pointer);

    if(in.is_literal != 0)
    {
      pointer = in.pointer;
      is_literal = in.is_literal;
    }
    else
    {
      size_t size = in.size() + 1;
      void *dst = allocate(size);
      memcpy(dst, in.c_str(), size);
      pointer = (intptr_t)dst;
      is_literal = 0;
    }

    return *this;
  }

  bool operator==(const rdcstr &o) const
  {
    if(o.c_str()[0] == 0)
      return c_str()[0] == 0;
    return !strcmp(o.c_str(), c_str());
  }
  bool operator==(const rdcinflexiblestr &o) const
  {
    if(o.c_str()[0] == 0)
      return c_str()[0] == 0;
    return !strcmp(o.c_str(), c_str());
  }
  bool operator==(const rdcliteral &o) const
  {
    if(o.c_str()[0] == 0)
      return c_str()[0] == 0;
    return !strcmp(o.c_str(), c_str());
  }

  bool operator!=(const rdcinflexiblestr &o) const { return !(*this == o); }
  bool operator!=(const rdcstr &o) const { return !(*this == o); }
  bool operator<(const rdcinflexiblestr &o) const { return strcmp(c_str(), o.c_str()) < 0; }
  bool operator>(const rdcinflexiblestr &o) const { return strcmp(c_str(), o.c_str()) > 0; }
  bool empty() const { return c_str()[0] == 0; }
  const char *c_str() const { return (const char *)(uintptr_t)pointer; }
  size_t size() const { return strlen(c_str()); }
  operator rdcstr() const
  {
    if(is_literal == 0)
      return rdcstr(c_str());
    else
      return rdcstr(rdcliteral(c_str(), size()));
  }

#if defined(RENDERDOC_QT_COMPAT)
  operator QString() const { return QString::fromUtf8(c_str(), (int32_t)size()); }
  operator QVariant() const { return QVariant(QString::fromUtf8(c_str(), (int32_t)size())); }
#endif
};
