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

#include <algorithm>
#include "apidefs.h"
#include "rdcarray.h"
#include "rdcpair.h"

// this is a container with a key-value interface but no strong ordering guarantee.
// The storage is an array of K,V pairs, which are unsorted below a given threshold. As a result
// this should be favoured in cases where the absolute number of K,V pairs is relatively low - not
// many thousands.
// The map can be forced to be sorted if SortThreshold is set to 0.
// For ease of transition it presents a std::map like interface, though it has weaker guarantees
// than the STL structures.
DOCUMENT("");
template <typename Key, typename Value, size_t SortThreshold = 16>
struct rdcflatmap
{
  using iterator = rdcpair<Key, Value> *;
  using const_iterator = const rdcpair<Key, Value> *;
  using size_type = size_t;

  DOCUMENT("");
  iterator find(const Key &id)
  {
    if(sorted)
      return sorted_find(id);
    return unsorted_find(id);
  }
  const_iterator find(const Key &id) const
  {
    if(sorted)
      return sorted_find(id);
    return unsorted_find(id);
  }

  void erase(const Key &id)
  {
    if(sorted)
      return sorted_erase(id);
    return unsorted_erase(id);
  }
  void erase(rdcpair<Key, Value> *it) { storage.erase(it - begin()); }
  Value &operator[](const Key &id)
  {
    if(sorted)
      return sorted_at(id);

    // pessimistically assume an insertion
    if(size() >= SortThreshold)
    {
      sort();
      return sorted_at(id);
    }

    return unsorted_at(id);
  }

  iterator insert(rdcpair<Key, Value> *it, const rdcpair<Key, Value> &val)
  {
    size_t idx = it - begin();
    if(sorted)
    {
      // if the map is sorted already, check that the `it` hint is actually valid.
      // we require [idx] < val.first < [idx+1]. If val.first is already in the array then we're
      // going to fail the insert but we'll treat that as if it's out of bounds.
      // This we want to check if either half is broken
      if((idx < storage.size() && !(val.first < storage.at(idx).first)) ||
         (idx + 1 < storage.size() && !(val.first < storage.at(idx + 1).first)))
      {
        return insert(val).first;
      }
    }
    storage.insert(idx, val);
    return begin() + idx;
  }

  iterator insert(rdcpair<Key, Value> *it, rdcpair<Key, Value> &&val)
  {
    size_t idx = it - begin();
    if(sorted)
    {
      // if the map is sorted already, check that the `it` hint is actually valid.
      // we require [idx] < val.first < [idx+1]. If val.first is already in the array then we're
      // going to fail the insert but we'll treat that as if it's out of bounds.
      // This we want to check if either half is broken
      if((idx < storage.size() && !(val.first < storage.at(idx).first)) ||
         (idx + 1 < storage.size() && !(val.first < storage.at(idx + 1).first)))
      {
        return insert(val).first;
      }
    }
    storage.insert(idx, std::move(val));
    return begin() + idx;
  }

  rdcpair<iterator, bool> insert(const rdcpair<Key, Value> &val)
  {
    if(!sorted)
      sort();

    size_t idx = lower_bound_idx(val.first);
    bool inserted = false;
    if(idx >= size() || !(storage.at(idx).first == val.first))
    {
      storage.insert(idx, val);
      inserted = true;
    }

    return {(begin() + idx), inserted};
  }

  rdcpair<iterator, bool> insert(rdcpair<Key, Value> &&val)
  {
    if(!sorted)
      sort();

    size_t idx = lower_bound_idx(val.first);
    bool inserted = false;
    if(idx >= size() || !(storage.at(idx).first == val.first))
    {
      storage.insert(idx, std::move(val));
      inserted = true;
    }

    return {(begin() + idx), inserted};
  }

  iterator begin() { return storage.begin(); }
  iterator end() { return storage.end(); }
  const_iterator begin() const { return storage.begin(); }
  const_iterator end() const { return storage.end(); }
  bool empty() const { return storage.empty(); }
  size_t size() const { return storage.size(); }
  void swap(rdcflatmap &other)
  {
    std::swap(sorted, other.sorted);
    storage.swap(other.storage);
  }
  void clear() { storage.clear(); }
protected:
  rdcarray<rdcpair<Key, Value>> storage;

  size_t lower_bound_idx(const Key &id) const
  {
    // start looking at the whole range
    size_t start = 0, sz = size();
    // continue iterating until the range is empty
    while(sz > 0)
    {
      const size_t halfsz = sz / 2;
      const size_t mid = start + halfsz;
      const Key comp = storage.at(mid).first;
      if(comp < id)
      {
        start = mid + 1;
        sz -= halfsz + 1;
      }
      else
      {
        sz = halfsz;
      }
    }
    return start;
  }

private:
  bool sorted = (SortThreshold == 0);

  void sort()
  {
    std::sort(storage.begin(), storage.end(),
              [](const rdcpair<Key, Value> &a, const rdcpair<Key, Value> &b) {
                return a.first < b.first;
              });
    sorted = true;
  }

  iterator sorted_find(const Key &id)
  {
    size_t idx = lower_bound_idx(id);
    if(idx >= size() || !(storage.at(idx).first == id))
      return end();

    return begin() + idx;
  }

  const_iterator sorted_find(const Key &id) const
  {
    size_t idx = lower_bound_idx(id);
    if(idx >= size() || !(storage.at(idx).first == id))
      return end();

    return begin() + idx;
  }

  void sorted_erase(const Key &id)
  {
    size_t idx = lower_bound_idx(id);
    if(idx < size() && storage.at(idx).first == id)
      storage.erase(idx);
  }

  Value &sorted_at(const Key &id)
  {
    size_t idx = lower_bound_idx(id);
    if(idx >= size() || !(storage.at(idx).first == id))
    {
      storage.insert(idx, {id, Value()});
    }

    return (begin() + idx)->second;
  }

  iterator unsorted_find(const Key &id)
  {
    for(auto it = begin(); it != end(); ++it)
      if(it->first == id)
        return it;

    return end();
  }

  const_iterator unsorted_find(const Key &id) const
  {
    for(auto it = begin(); it != end(); ++it)
      if(it->first == id)
        return it;

    return end();
  }

  void unsorted_erase(const Key &id)
  {
    auto it = find(id);
    if(it != end())
      storage.erase(it - begin());
  }

  Value &unsorted_at(const Key &id)
  {
    auto it = find(id);
    if(it != end())
      return it->second;

    // only allocate once for the unsorted size
    storage.reserve(SortThreshold);

    storage.push_back({id, Value()});
    return storage.back().second;
  }
};

// a version of rdcflatmap which is guaranteed to be sorted
// adds upper_bound and lower_bound APIs
template <typename Key, typename Value>
struct rdcsortedflatmap : rdcflatmap<Key, Value, 0>
{
  using iterator = rdcpair<Key, Value> *;
  using const_iterator = const rdcpair<Key, Value> *;

  using rdcflatmap<Key, Value, 0>::begin;
  using rdcflatmap<Key, Value, 0>::size;
  using rdcflatmap<Key, Value, 0>::storage;
  using rdcflatmap<Key, Value, 0>::lower_bound_idx;

  iterator upper_bound(const Key &key)
  {
    size_t idx = lower_bound_idx(key);

    // almost the same behaviour as lower_bound, except if we actually have the key, return the next
    // element.
    if(idx < size() && storage.at(idx).first == key)
      return begin() + idx + 1;

    return begin() + idx;
  }

  const_iterator upper_bound(const Key &key) const
  {
    size_t idx = lower_bound_idx(key);

    // almost the same behaviour as lower_bound, except if we actually have the key, return the next
    // element.
    if(idx < size() && storage.at(idx).first == key)
      return begin() + idx + 1;

    return begin() + idx;
  }

  iterator lower_bound(const Key &key) { return begin() + lower_bound_idx(key); }
  const_iterator lower_bound(const Key &key) const { return begin() + lower_bound_idx(key); }
};
