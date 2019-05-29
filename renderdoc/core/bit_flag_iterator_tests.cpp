/******************************************************************************
* The MIT License (MIT)
*
* Copyright (c) 2019 Baldur Karlsson
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

#include "bit_flag_iterator.h"
#include "common/globalconfig.h"

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"

#include <stdint.h>
#include <vector>

typedef BitFlagIterator<uint32_t, uint32_t, int32_t> TestFlagIter;

std::vector<uint32_t> get_bits(const TestFlagIter &begin, const TestFlagIter &end)
{
  std::vector<uint32_t> bits;
  for(TestFlagIter it = begin; it != end; ++it)
  {
    bits.push_back(*it);
  }
  return bits;
}

TEST_CASE("Test BitFlagIterator type", "[bit_flag_iterator]")
{
  SECTION("empty")
  {
    std::vector<uint32_t> expected = {};
    CHECK(get_bits(TestFlagIter::begin(0x0), TestFlagIter::end()) == expected);
  };
  SECTION("full")
  {
    std::vector<uint32_t> expected = {
        0x1,       0x2,       0x4,       0x8,       0x10,       0x20,       0x40,       0x80,
        0x100,     0x200,     0x400,     0x800,     0x1000,     0x2000,     0x4000,     0x8000,
        0x10000,   0x20000,   0x40000,   0x80000,   0x100000,   0x200000,   0x400000,   0x800000,
        0x1000000, 0x2000000, 0x4000000, 0x8000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000};
    CHECK(get_bits(TestFlagIter::begin(UINT32_MAX), TestFlagIter::end()) == expected);
  };
  SECTION("even")
  {
    std::vector<uint32_t> expected = {
        0x1,     0x4,     0x10,     0x40,     0x100,     0x400,     0x1000,     0x4000,
        0x10000, 0x40000, 0x100000, 0x400000, 0x1000000, 0x4000000, 0x10000000, 0x40000000};
    CHECK(get_bits(TestFlagIter::begin(0x55555555), TestFlagIter::end()) == expected);
  };
  SECTION("odd")
  {
    std::vector<uint32_t> expected = {
        0x2,     0x8,     0x20,     0x80,     0x200,     0x800,     0x2000,     0x8000,
        0x20000, 0x80000, 0x200000, 0x800000, 0x2000000, 0x8000000, 0x20000000, 0x80000000};
    CHECK(get_bits(TestFlagIter::begin(0xAAAAAAAA), TestFlagIter::end()) == expected);
  };
  SECTION("single")
  {
    for(int i = 0; i < 32; i++)
    {
      uint32_t b = 1 << i;
      std::vector<uint32_t> expected = {b};
      CHECK(get_bits(TestFlagIter::begin(b), TestFlagIter::end()) == expected);
    }
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
