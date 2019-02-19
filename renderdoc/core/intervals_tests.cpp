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

#include "intervals.h"
#include "common/globalconfig.h"

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"

#include <stdint.h>
#include <vector>

struct Interval
{
  uint64_t start;
  uint64_t value;
  uint64_t end;
};

void check_intervals(Intervals<uint64_t> &value, const std::vector<Interval> &expected)
{
  auto i = value.begin();
  auto j = expected.begin();
  for(; i != value.end() && j != expected.end(); i++, j++)
  {
    CHECK(i->start() == j->start);
    CHECK(i->value() == j->value);
    CHECK(i->finish() == j->end);
  }
  CHECK((i == value.end()));
  CHECK((j == expected.end()));
}

Intervals<uint64_t> make_intervals(const std::vector<Interval> &intervals)
{
  Intervals<uint64_t> res;
  for(auto i = intervals.begin(); i != intervals.end(); i++)
  {
    auto j = res.end();
    j--;
    if(i->start > j->start())
      j->split(i->start);
    if(i->end < j->finish())
    {
      j->split(i->end);
      j--;
    }
    j->setValue(i->value);
  }
  check_intervals(res, intervals);
  return res;
}

TEST_CASE("Test Intervals type", "[intervals]")
{
  SECTION("update tests")
  {
    SECTION("empty Intervals")
    {
      Intervals<uint64_t> test;
      check_intervals(test, {{0, 0, UINT64_MAX}});
    };

    SECTION("update a sub-interval")
    {
      Intervals<uint64_t> test;
      test.update(5, 10, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
    };

    SECTION("update a sub-interval matching on the left")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(5, 7, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 2, 7}, {7, 1, 10}, {10, 0, UINT64_MAX}});
    };

    SECTION("update a sub-interval matching on the right")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(7, 10, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 7}, {7, 2, 10}, {10, 0, UINT64_MAX}});
    };

    SECTION("update an interval that exactly matches an existing interval")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(5, 10, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 2, 10}, {10, 0, UINT64_MAX}});
    };

    SECTION("update a properly overlapping interval")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(7, 15, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 7}, {7, 2, 10}, {10, 1, 15}, {15, 0, UINT64_MAX}});
    };

    SECTION("update a super-interval")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(2, 15, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 2}, {2, 1, 5}, {5, 2, 10}, {10, 1, 15}, {15, 0, UINT64_MAX}});
    };

    SECTION("update a super-interval matching on the left")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(5, 15, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 2, 10}, {10, 1, 15}, {15, 0, UINT64_MAX}});
    };

    SECTION("update a super-interval matching on the right")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(2, 10, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 2}, {2, 1, 5}, {5, 2, 10}, {10, 0, UINT64_MAX}});
    };

    SECTION("update overlapping 2 intervals")
    {
      Intervals<uint64_t> test =
          make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, 20}, {20, 10, 30}, {30, 0, UINT64_MAX}});
      test.update(7, 25, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5},
                             {5, 1, 7},
                             {7, 2, 10},
                             {10, 1, 20},
                             {20, 11, 25},
                             {25, 10, 30},
                             {30, 0, UINT64_MAX}});
    };

    SECTION("update overlapping 2 intervals matching on start of leftmost interval")
    {
      Intervals<uint64_t> test =
          make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, 20}, {20, 10, 30}, {30, 0, UINT64_MAX}});
      test.update(5, 25, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(
          test,
          {{0, 0, 5}, {5, 2, 10}, {10, 1, 20}, {20, 11, 25}, {25, 10, 30}, {30, 0, UINT64_MAX}});
    };

    SECTION("update overlapping 2 intervals matching on end of leftmost interval")
    {
      Intervals<uint64_t> test =
          make_intervals({{0, 0, 5}, {5, 5, 10}, {10, 0, 20}, {20, 10, 30}, {30, 0, UINT64_MAX}});
      test.update(10, 25, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(
          test,
          {{0, 0, 5}, {5, 5, 10}, {10, 1, 20}, {20, 11, 25}, {25, 10, 30}, {30, 0, UINT64_MAX}});
    };

    SECTION("update overlapping 2 intervals matching on start of rightmost interval")
    {
      Intervals<uint64_t> test =
          make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, 20}, {20, 10, 30}, {30, 0, UINT64_MAX}});
      test.update(7, 20, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(
          test, {{0, 0, 5}, {5, 1, 7}, {7, 2, 10}, {10, 1, 20}, {20, 10, 30}, {30, 0, UINT64_MAX}});
    };

    SECTION("update overlapping 2 intervals matching on end of rightmost interval")
    {
      Intervals<uint64_t> test =
          make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, 20}, {20, 10, 30}, {30, 0, UINT64_MAX}});
      test.update(7, 30, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(
          test, {{0, 0, 5}, {5, 1, 7}, {7, 2, 10}, {10, 1, 20}, {20, 11, 30}, {30, 0, UINT64_MAX}});
    };

    SECTION("update triggering merge on left")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(10, 20, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 20}, {20, 0, UINT64_MAX}});
    };

    SECTION("update triggering merge on right")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(2, 5, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 2}, {2, 1, 10}, {10, 0, UINT64_MAX}});
    };

    SECTION("overlapping update triggering merge on left")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(7, 20, 1, [](uint64_t, uint64_t) -> uint64_t { return 1; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 20}, {20, 0, UINT64_MAX}});
    };

    SECTION("overlapping update triggering merge on right")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(2, 7, 1, [](uint64_t, uint64_t) -> uint64_t { return 1; });
      check_intervals(test, {{0, 0, 2}, {2, 1, 10}, {10, 0, UINT64_MAX}});
    };

    SECTION("update triggering multiple merges")
    {
      Intervals<uint64_t> test = make_intervals(
          {{0, 0, 5}, {5, 1, 10}, {10, 0, 12}, {12, 5, 18}, {18, 0, 20}, {20, 1, 30}, {30, 0, UINT64_MAX}});
      test.update(7, 25, 1, [](uint64_t, uint64_t) -> uint64_t { return 1; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 30}, {30, 0, UINT64_MAX}});
    };

    SECTION(
        "update triggering multiple merges, including merge with non-overlapping interval on left")
    {
      Intervals<uint64_t> test = make_intervals(
          {{0, 0, 5}, {5, 1, 10}, {10, 0, 12}, {12, 5, 18}, {18, 0, 20}, {20, 1, 30}, {30, 0, UINT64_MAX}});
      test.update(10, 25, 1, [](uint64_t, uint64_t) -> uint64_t { return 1; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 30}, {30, 0, UINT64_MAX}});
    };

    SECTION(
        "update triggering multiple merges, including merge with non-overlapping interval on right")
    {
      Intervals<uint64_t> test = make_intervals(
          {{0, 0, 5}, {5, 1, 10}, {10, 0, 12}, {12, 5, 18}, {18, 0, 20}, {20, 1, 30}, {30, 0, UINT64_MAX}});
      test.update(7, 20, 1, [](uint64_t, uint64_t) -> uint64_t { return 1; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 30}, {30, 0, UINT64_MAX}});
    };

    SECTION("update a interval starting at 0")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(0, 10, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 1, 5}, {5, 2, 10}, {10, 0, UINT64_MAX}});
    };

    SECTION("update a interval finishing at UINT64_MAX")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(5, UINT64_MAX, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 2, 10}, {10, 1, UINT64_MAX}});
    };

    SECTION("update entire range")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(0, UINT64_MAX, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 1, 5}, {5, 2, 10}, {10, 1, UINT64_MAX}});
    };

    SECTION("update an empty interval in the interior of an interval")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(2, 2, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
    };

    SECTION("update an empty interval on a boundary")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(5, 5, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
    };

    SECTION("update an empty interval at 0")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(0, 0, 1, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
    };

    SECTION("update an empty interval at UINT64_MAX")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
      test.update(UINT64_MAX, UINT64_MAX, 1,
                  [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 10}, {10, 0, UINT64_MAX}});
    };
  };

  SECTION("mergeIntervals tests")
  {
    SECTION("merge matching intervals")
    {
      Intervals<uint64_t> test =
          make_intervals({{0, 0, 10}, {10, 1, 20}, {20, 0, 30}, {30, 1, 40}, {40, 0, UINT64_MAX}});
      Intervals<uint64_t> other =
          make_intervals({{0, 0, 10}, {10, 1, 20}, {20, 0, 30}, {30, 1, 40}, {40, 0, UINT64_MAX}});
      test.merge(other, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 10}, {10, 2, 20}, {20, 0, 30}, {30, 2, 40}, {40, 0, UINT64_MAX}});
    };

    SECTION("merge shifted intervals")
    {
      Intervals<uint64_t> test =
          make_intervals({{0, 0, 10}, {10, 1, 20}, {20, 0, 30}, {30, 1, 40}, {40, 0, UINT64_MAX}});
      Intervals<uint64_t> other =
          make_intervals({{0, 0, 5}, {5, 1, 15}, {15, 0, 25}, {25, 1, 35}, {35, 0, UINT64_MAX}});
      test.merge(other, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5},
                             {5, 1, 10},
                             {10, 2, 15},
                             {15, 1, 20},
                             {20, 0, 25},
                             {25, 1, 30},
                             {30, 2, 35},
                             {35, 1, 40},
                             {40, 0, UINT64_MAX}});
    };

    SECTION("merge into empty intervals")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, UINT64_MAX}});
      Intervals<uint64_t> other =
          make_intervals({{0, 0, 5}, {5, 1, 15}, {15, 0, 25}, {25, 1, 35}, {35, 0, UINT64_MAX}});
      test.merge(other, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 15}, {15, 0, 25}, {25, 1, 35}, {35, 0, UINT64_MAX}});
    };

    SECTION("merge with empty intervals")
    {
      Intervals<uint64_t> test =
          make_intervals({{0, 0, 5}, {5, 1, 15}, {15, 0, 25}, {25, 1, 35}, {35, 0, UINT64_MAX}});
      Intervals<uint64_t> other = make_intervals({{0, 0, UINT64_MAX}});
      test.merge(other, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5}, {5, 1, 15}, {15, 0, 25}, {25, 1, 35}, {35, 0, UINT64_MAX}});
    };

    SECTION("merge into single interval")
    {
      Intervals<uint64_t> test = make_intervals({{0, 0, 10}, {10, 1, 30}, {30, 0, UINT64_MAX}});
      Intervals<uint64_t> other =
          make_intervals({{0, 0, 5}, {5, 1, 15}, {15, 0, 25}, {25, 1, 35}, {35, 0, UINT64_MAX}});
      test.merge(other, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5},
                             {5, 1, 10},
                             {10, 2, 15},
                             {15, 1, 25},
                             {25, 2, 30},
                             {30, 1, 35},
                             {35, 0, UINT64_MAX}});
    };

    SECTION("merge with single interval")
    {
      Intervals<uint64_t> test =
          make_intervals({{0, 0, 5}, {5, 1, 15}, {15, 0, 25}, {25, 1, 35}, {35, 0, UINT64_MAX}});
      Intervals<uint64_t> other = make_intervals({{0, 0, 10}, {10, 1, 30}, {30, 0, UINT64_MAX}});
      test.merge(other, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 5},
                             {5, 1, 10},
                             {10, 2, 15},
                             {15, 1, 25},
                             {25, 2, 30},
                             {30, 1, 35},
                             {35, 0, UINT64_MAX}});
    };

    SECTION("merge disjoint before")
    {
      Intervals<uint64_t> test =
          make_intervals({{0, 0, 50}, {50, 1, 60}, {60, 0, 70}, {70, 1, 80}, {80, 0, UINT64_MAX}});
      Intervals<uint64_t> other =
          make_intervals({{0, 0, 10}, {10, 1, 20}, {20, 0, 30}, {30, 1, 40}, {40, 0, UINT64_MAX}});
      test.merge(other, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 10},
                             {10, 1, 20},
                             {20, 0, 30},
                             {30, 1, 40},
                             {40, 0, 50},
                             {50, 1, 60},
                             {60, 0, 70},
                             {70, 1, 80},
                             {80, 0, UINT64_MAX}});
    };

    SECTION("merge disjoint after")
    {
      Intervals<uint64_t> test =
          make_intervals({{0, 0, 10}, {10, 1, 20}, {20, 0, 30}, {30, 1, 40}, {40, 0, UINT64_MAX}});
      Intervals<uint64_t> other =
          make_intervals({{0, 0, 50}, {50, 1, 60}, {60, 0, 70}, {70, 1, 80}, {80, 0, UINT64_MAX}});
      test.merge(other, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 10},
                             {10, 1, 20},
                             {20, 0, 30},
                             {30, 1, 40},
                             {40, 0, 50},
                             {50, 1, 60},
                             {60, 0, 70},
                             {70, 1, 80},
                             {80, 0, UINT64_MAX}});
    };

    SECTION("merge disjoint interleaved")
    {
      Intervals<uint64_t> test =
          make_intervals({{0, 0, 10}, {10, 1, 20}, {20, 0, 50}, {50, 1, 60}, {60, 0, UINT64_MAX}});
      Intervals<uint64_t> other =
          make_intervals({{0, 0, 30}, {30, 1, 40}, {40, 0, 70}, {70, 1, 80}, {80, 0, UINT64_MAX}});
      test.merge(other, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 10},
                             {10, 1, 20},
                             {20, 0, 30},
                             {30, 1, 40},
                             {40, 0, 50},
                             {50, 1, 60},
                             {60, 0, 70},
                             {70, 1, 80},
                             {80, 0, UINT64_MAX}});
    };

    SECTION("merge disjoint interleaved touching")
    {
      Intervals<uint64_t> test =
          make_intervals({{0, 0, 10}, {10, 1, 20}, {20, 0, 30}, {30, 1, 40}, {40, 0, UINT64_MAX}});
      Intervals<uint64_t> other =
          make_intervals({{0, 0, 20}, {20, 1, 30}, {30, 0, 40}, {40, 1, 50}, {50, 0, UINT64_MAX}});
      test.merge(other, [](uint64_t x, uint64_t y) -> uint64_t { return x + y; });
      check_intervals(test, {{0, 0, 10}, {10, 1, 50}, {50, 0, UINT64_MAX}});
    };
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)