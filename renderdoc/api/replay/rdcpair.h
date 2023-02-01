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

#include <type_traits>
#include <utility>

template <typename A, typename B>
struct rdcpair
{
  A first;
  B second;

  rdcpair(const A &a, const B &b) : first(a), second(b) {}
  rdcpair() = default;
  rdcpair(const rdcpair<A, B> &o) = default;
  rdcpair(rdcpair<typename std::decay<A>::type, typename std::decay<B>::type> &&o)
      : first(std::move(o.first)), second(std::move(o.second))
  {
  }
  rdcpair(typename std::decay<A>::type &&a, typename std::decay<B>::type &&b)
      : first(std::move(a)), second(std::move(b))
  {
  }
  ~rdcpair() = default;
  inline void swap(rdcpair<A, B> &o)
  {
    rdcpair<A, B> tmp = *this;
    *this = o;
    o = tmp;
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

  template <typename A_, typename B_>
  rdcpair<A, B> &operator=(rdcpair<A_, B_> &&o)
  {
    first = std::move(o.first);
    second = std::move(o.second);
    return *this;
  }

  rdcpair<A, B> &operator=(rdcpair<A, B> &&o)
  {
    first = std::move(o.first);
    second = std::move(o.second);
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
