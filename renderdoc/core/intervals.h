/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2018-2019 Baldur Karlsson
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
#include <map>
#include "common/common.h"

template <typename T>
struct Intervals;

template <typename T, typename Map, typename Iter, typename Interval>
class IntervalsIter;

// An interval in an `Intervals<T>` instance.
template <typename T, typename Map, typename Iter>
class ConstIntervalRef
{
  friend class IntervalsIter<T, Map, Iter, ConstIntervalRef>;

protected:
  Iter iter;
  Map *owner;

  ConstIntervalRef(Map *owner, Iter iter) : iter(iter), owner(owner) {}
public:
  // Inclusive lower bound
  inline uint64_t start() const { return iter->first; }
  // Exclusive upper bound
  inline uint64_t finish() const
  {
    Iter next = iter;
    next++;
    if(next == owner->end())
    {
      return UINT64_MAX;
    }
    return next->first;
  }

  // Value associated with this interval
  inline const T &value() const { return iter->second; }
};

// A mutable interval in an `Intervals<T>` instance
template <typename T, typename Map, typename Iter>
class IntervalRef : public ConstIntervalRef<T, Map, Iter>
{
  friend class IntervalsIter<T, Map, Iter, IntervalRef>;

protected:
  IntervalRef(Map *owner, Iter iter) : ConstIntervalRef<T, Map, Iter>(owner, iter) {}
public:
  inline void setValue(const T &val) { this->iter->second = val; }
  // Split this interval into two intervals:
  //   [start, x), [x, finish)
  // This iterator will point to [x, finish) after the split.
  // `x` must be in the interval [start, finish).
  // If `x == start`, then `split(x)` is a no-op.
  inline void split(uint64_t x)
  {
    if(this->start() < x)
      this->iter = this->owner->insert(std::pair<uint64_t, T>(x, this->value())).first;
  }

  // Merge this interval with the interval to the left, if both intervals have
  // the same value.
  // This iterator will point to the merged interval, if the merge is actually
  // performed; otherwise this iterator is unmodified.
  inline void mergeLeft()
  {
    if(this->iter != this->owner->begin())
    {
      auto prev_it = this->iter;
      prev_it--;
      if(this->iter->second == prev_it->second)
      {
        this->owner->erase(this->iter);
        this->iter = prev_it;
      }
    }
  }
};

// An iterator in an `Intervals<T>` instance.
template <typename T, typename Map, typename Iter, typename Interval>
class IntervalsIter
{
  friend struct Intervals<T>;

protected:
  Interval ref;
  IntervalsIter(Map *owner, Iter iter) : ref(owner, iter) {}
  Iter unwrap() { return ref.iter; }
public:
  IntervalsIter(const IntervalsIter &src) : ref(src.ref) {}
  IntervalsIter &operator++()
  {
    ++ref.iter;
    return *this;
  }
  IntervalsIter operator++(int)
  {
    IntervalsIter tmp(*this);
    operator++();
    return tmp;
  }
  IntervalsIter &operator--()
  {
    --ref.iter;
    return *this;
  }
  IntervalsIter operator--(int)
  {
    IntervalsIter tmp(*this);
    operator--();
    return tmp;
  }
  bool operator==(const IntervalsIter &rhs) const
  {
    return ref.iter == rhs.ref.iter && ref.owner == rhs.ref.owner;
  }
  bool operator!=(const IntervalsIter &rhs) const
  {
    return ref.iter != rhs.ref.iter && ref.owner == rhs.ref.owner;
  }
  IntervalsIter &operator=(const IntervalsIter &rhs)
  {
    ref.iter = rhs.ref.iter;
    ref.owner = rhs.ref.owner;
    return *this;
  }

  inline Interval *operator->() { return &ref; }
};

// Data structure to efficiently store values for disjoint intervals.
template <typename T>
struct Intervals
{
public:
  typedef IntervalRef<T, std::map<uint64_t, T>, typename std::map<uint64_t, T>::iterator> interval;
  typedef IntervalsIter<T, std::map<uint64_t, T>, typename std::map<uint64_t, T>::iterator, interval> iterator;

  typedef ConstIntervalRef<T, const std::map<uint64_t, T>, typename std::map<uint64_t, T>::const_iterator>
      const_interval;
  typedef IntervalsIter<T, const std::map<uint64_t, T>,
                        typename std::map<uint64_t, T>::const_iterator, const_interval>
      const_iterator;

private:
  std::map<uint64_t, T> StartPoints;

  iterator Wrap(typename std::map<uint64_t, T>::iterator iter)
  {
    return iterator(&StartPoints, iter);
  }

  const_iterator Wrap(typename std::map<uint64_t, T>::const_iterator iter) const
  {
    return const_iterator(&StartPoints, iter);
  }

public:
  Intervals() : StartPoints{{0, T()}} {}
  inline iterator end() { return Wrap(StartPoints.end()); }
  inline iterator begin() { return Wrap(StartPoints.begin()); }
  inline const_iterator begin() const { return Wrap(StartPoints.begin()); }
  inline const_iterator end() const { return Wrap(StartPoints.end()); }
  typedef typename std::map<uint64_t, T>::size_type size_type;
  inline size_type size() const { return StartPoints.size(); }
  // Find the interval containing `x`.
  iterator find(uint64_t x)
  {
    // Find the first interval starting after `x`; return the preceding interval.
    auto it = StartPoints.upper_bound(x);
    it--;
    return Wrap(it);
  }

  // Find the interval containing `x`.
  const_iterator find(uint64_t x) const
  {
    // Find the first interval starting after `x`; return the preceding interval.
    auto it = StartPoints.upper_bound(x);
    it--;
    return Wrap(it);
  }

  // Update the values of overlapping intervals to `comp(oldValue, val)`
  // (where `oldValue` is the value of the interval prior to calling `update`).
  // If start/finish do not lie on the boundaries between intervals, the intervals
  // will be split as necessary.
  template <typename Compose>
  void update(uint64_t start, uint64_t finish, T val, Compose comp)
  {
    if(finish <= start)
      return;

    auto i = find(start);

    // Split the interval so that `i.start == start`
    i->split(start);

    // Loop over all the intervals in `a` that intersect the interval [start, finish)
    for(; i != end() && i->start() < finish; i++)
    {
      if(i->finish() > finish)
      {
        // In this case, interval `i` extends beyond `finish`;
        // split `i` so that we only update the portion of `i` in [start, finish).
        i->split(finish);

        // `split` leaves `i` pointing at the interval starting at `finish`;
        // move back to the interval finishing at `finish`.
        i--;
      }
      i->setValue(comp(i->value(), val));
      i->mergeLeft();
    }

    // `i` now points to the interval following the last interval whose value was
    // modified; merge `i` with that last modified interval, if the values match.
    if(i != end())
      i->mergeLeft();
  }

  // Update `this` by composing the value of each interval with the value of the
  // corresponding interval in `other`.
  // If the intervals in `this` and `other` do not line up, then the intervals in
  // `this` will be split as necessary.
  template <typename Compose>
  void merge(const Intervals &other, Compose comp)
  {
    auto j = other.begin();
    auto i = begin();

    // Loop over the intervals in `this` (iterator `i`), while maintaining the
    // interval `j` in `other` that contains `i`.
    // The intervals in `this` are split as necessary, so that each `i` is
    // contained in a single interval of `other`.
    // Loop invariants:
    //  * i.start() >= j.start()
    //  * i.start() < j.end()
    while(true)
    {
      if(i->finish() > j->finish())
      {
        i->split(j->finish());
        i--;
      }

      // Now i is contained in j, so we can update the value of all of i
      i->setValue(comp(i->value(), j->value()));

      // The value of i and the interval left of i are now final;
      // if these two intervals now have the same value, they can safely be
      // merged into a single interval.
      i->mergeLeft();

      // Move to the next interval in `this`; also advance to the next interval
      // in `other`, if necessary to maintain the invariant `i.start < j.end`.
      i++;
      if(i == end())
        return;
      if(i->start() >= j->finish())
        j++;
    }
  }
};
