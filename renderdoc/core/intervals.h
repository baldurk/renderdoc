/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2018 Baldur Karlsson
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

template <typename T>
struct Intervals;

template <typename T>
struct IntervalsIter
{
  friend struct Intervals<T>;

private:
  typename std::map<uint64_t, T>::iterator iter;
  std::map<uint64_t, T> *owner;
  IntervalsIter(std::map<uint64_t, T> *owner, typename std::map<uint64_t, T>::iterator iter)
      : iter(iter), owner(owner)
  {
  }
  typename std::map<uint64_t, T>::iterator unwrap() { return iter; }
public:
  IntervalsIter(const IntervalsIter &src) : owner(src.owner), iter(src.iter) {}
  IntervalsIter &operator++()
  {
    ++iter;
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
    --iter;
    return *this;
  }
  IntervalsIter operator--(int)
  {
    IntervalsIter tmp(*this);
    operator--();
    return tmp;
  }
  bool operator==(const IntervalsIter &rhs) const { return iter == rhs.iter && owner == rhs.owner; }
  bool operator!=(const IntervalsIter &rhs) const { return iter != rhs.iter && owner == rhs.owner; }
  IntervalsIter &operator=(const IntervalsIter &rhs)
  {
    iter = rhs.iter;
    owner = rhs.owner;
    return *this;
  }
  inline uint64_t start() const { return iter->first; }
  inline uint64_t end() const
  {
    typename std::map<uint64_t, T>::iterator next = iter;
    next++;
    if(next == owner->end())
    {
      return UINT64_MAX;
    }
    return next->first;
  }
  inline const T &value() const { return iter->second; }
  inline void setValue(uint64_t aStart, uint64_t aEnd, const T &aValue)
  {
    T old_value = this->value();
    if((aValue == old_value || aEnd <= start()) && end() <= aStart)
    {
      // The value is unchanged, or the specified interval is disjoint from this interval.
      return;
    }

    // Add a new endpoint for aStart, if necessary, and update iter's value to aValue.
    if(start() < aStart)
    {
      // The updated portion of this interval begins at aStart.
      iter = owner->insert(std::pair<uint64_t, T>(aStart, aValue)).first;
    }
    else
    {
      // The updated portion of this interval begins at start()
      iter->second = aValue;
    }

    // Add a new endpoint for aEnd, if necessary.
    if(aEnd < end())
    {
      owner->insert(std::pair<uint64_t, T>(aEnd, old_value));
    }

    // Merge with preceding interval, if necessary
    if(iter != owner->begin())
    {
      typename std::map<uint64_t, T>::iterator prev_it = iter;
      prev_it--;
      if(prev_it->second == iter->second)
      {
        owner->erase(iter);
        iter = prev_it;
      }
    }

    // Merge with succeding interval, if necessary
    typename std::map<uint64_t, T>::iterator next_it = iter;
    next_it++;
    if(next_it != owner->end())
    {
      if(next_it->second == iter->second)
      {
        owner->erase(next_it);
      }
    }
  }
};

template <typename T>
struct Intervals
{
private:
  std::map<uint64_t, T> StartPoints;

public:
private:
  IntervalsIter<T> Wrap(typename std::map<uint64_t, T>::iterator iter)
  {
    return IntervalsIter<T>(&StartPoints, iter);
  }

public:
  Intervals() : StartPoints{{0, T()}} {}
  IntervalsIter<T> begin() { return Wrap(StartPoints.begin()); }
  IntervalsIter<T> end() { return Wrap(StartPoints.end()); }
  // finds the interval containing `x`.
  IntervalsIter<T> find(uint64_t x)
  {
    // Find the first interval which starts AFTER `x`
    typename std::map<uint64_t, T>::iterator it = StartPoints.upper_bound(x);
    // Because the first interval always starts at 0, the found interval cannot be the first
    // interval
    RDCASSERT(it != StartPoints.begin());
    // Move back 1 interval, to find the last interval that starts no later than `range.start`.
    // This is the interval that contains the point
    it--;
    return Wrap(it);
  }
};