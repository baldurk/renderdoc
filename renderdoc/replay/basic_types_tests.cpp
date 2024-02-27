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

#include "common/globalconfig.h"

#if ENABLED(ENABLE_UNIT_TESTS)

#include "api/replay/rdcarray.h"
#include "api/replay/rdcflatmap.h"
#include "api/replay/rdcpair.h"
#include "api/replay/rdcstr.h"
#include "api/replay/resourceid.h"
#include "common/formatting.h"
#include "common/globalconfig.h"
#include "common/timing.h"
#include "os/os_specific.h"

#include "catch/catch.hpp"

static int32_t constructor = 0;
static int32_t moveConstructor = 0;
static int32_t valueConstructor = 0;
static int32_t copyConstructor = 0;
static int32_t destructor = 0;
static int32_t movedDestructor = 0;
static int32_t copyAssignment = 0;
static int32_t moveAssignment = 0;

struct NonTrivial
{
  NonTrivial(int v) : val(v) {}
  int val = 6;

  bool operator==(const NonTrivial &o) const { return val == o.val; }
  bool operator<(const NonTrivial &o) const { return val > o.val; }
};

template <>
rdcstr DoStringise(const NonTrivial &el)
{
  return "NonTrivial{" + DoStringise(el.val) + "}";
}

struct ConstructorCounter
{
  int value;

  ConstructorCounter()
  {
    value = 0;
    Atomic::Inc32(&constructor);
  }
  ConstructorCounter(int v)
  {
    value = v;
    Atomic::Inc32(&valueConstructor);
  }
  ConstructorCounter(const ConstructorCounter &other)
  {
    value = other.value;
    Atomic::Inc32(&copyConstructor);
  }
  ConstructorCounter(ConstructorCounter &&other)
  {
    value = other.value;
    other.value = -9999;
    Atomic::Inc32(&moveConstructor);
  }
  ConstructorCounter &operator=(const ConstructorCounter &other)
  {
    value = other.value;
    Atomic::Inc32(&copyAssignment);
    return *this;
  }
  ConstructorCounter &operator=(ConstructorCounter &&other)
  {
    value = other.value;
    other.value = -9999;
    Atomic::Inc32(&moveAssignment);
    return *this;
  }
  ~ConstructorCounter()
  {
    Atomic::Inc32(&destructor);
    if(value == -9999)
      Atomic::Inc32(&movedDestructor);
    value = -1234;
  }
  bool operator==(const ConstructorCounter &other) { return value == other.value; }
};

TEST_CASE("Test array type", "[basictypes]")
{
  // reset globals
  constructor = 0;
  moveConstructor = 0;
  valueConstructor = 0;
  copyConstructor = 0;
  destructor = 0;
  movedDestructor = 0;

  SECTION("Basic test")
  {
    rdcarray<int> test;
    const rdcarray<int> &constTest = test;

    CHECK(test.size() == 0);
    CHECK(test.capacity() == 0);
    CHECK(test.empty());
    CHECK(test.isEmpty());
    CHECK(test.begin() == test.end());

    CHECK(constTest.size() == 0);
    CHECK(constTest.capacity() == 0);
    CHECK(constTest.empty());
    CHECK(constTest.isEmpty());
    CHECK(constTest.begin() == constTest.end());

    test.clear();

    CHECK(test.size() == 0);
    CHECK(test.capacity() == 0);
    CHECK(test.empty());
    CHECK(test.isEmpty());
    CHECK(test.begin() == test.end());

    test.push_back(5);

    CHECK(test.size() == 1);
    CHECK(test.capacity() >= 1);
    CHECK_FALSE(test.empty());
    CHECK_FALSE(test.isEmpty());
    CHECK(test.begin() != test.end());
    CHECK(test.begin() + 1 == test.end());

    test.push_back(10);

    CHECK(test.front() == 5);
    CHECK(test.back() == 10);
    CHECK(test.size() == 2);
    CHECK(test.capacity() >= 2);
    CHECK_FALSE(test.empty());
    CHECK_FALSE(test.isEmpty());
    CHECK(test.begin() + 2 == test.end());

    CHECK(constTest.front() == 5);
    CHECK(constTest.back() == 10);
    CHECK(constTest.size() == 2);
    CHECK(constTest.capacity() >= 2);
    CHECK_FALSE(constTest.empty());
    CHECK_FALSE(constTest.isEmpty());
    CHECK(constTest.begin() + 2 == constTest.end());

    int sum = 0;
    for(int x : test)
      sum += x;

    CHECK(sum == 15);

    test.clear();

    CHECK(test.size() == 0);
    CHECK(test.capacity() >= 2);
    CHECK(test.empty());
    CHECK(test.isEmpty());
    CHECK(test.begin() == test.end());

    test = {4, 1, 77, 6, 0, 8, 20, 934};

    CHECK(test.size() == 8);
    CHECK(test.capacity() >= 8);
    CHECK_FALSE(test.empty());
    CHECK_FALSE(test.isEmpty());
    CHECK(test.begin() + 8 == test.end());

    sum = 0;
    for(int x : test)
      sum += x;

    CHECK(sum == 1050);

    CHECK(test[2] == 77);

    test[2] = 10;

    CHECK(test[2] == 10);

    test.reserve(100);

    CHECK(test.size() == 8);
    CHECK(test.capacity() >= 100);
    CHECK_FALSE(test.empty());
    CHECK_FALSE(test.isEmpty());
    CHECK(test.begin() + 8 == test.end());

    int x = test.takeAt(2);

    CHECK(test.size() == 7);
    CHECK(x == 10);
    CHECK(test[2] == 6);
  }

  SECTION("Comparison test with trivial type")
  {
    rdcarray<int> test;
    rdcarray<int> test2;

    test = {1, 2, 3, 4, 5, 6};
    test2 = {1, 2, 3, 5, 6, 6};

    CHECK(test < test2);
    CHECK(test != test2);
    CHECK_FALSE(test2 < test);

    test2[3]--;
    test2[4]--;

    CHECK(test == test2);

    test.pop_back();

    // smaller arrays are considered less-than if all elements are equal
    CHECK(test < test2);
    CHECK_FALSE(test2 < test);
    CHECK_FALSE(test == test2);

    // if however one of the common prefix is greater, that's not true
    test[0] += 10;
    CHECK(test2 < test);
    CHECK_FALSE(test < test2);
    CHECK_FALSE(test == test2);

    // ensure in the case of comparing different sized arrays we don't read off the end of one
    rdcarray<int> emptyTest;

    CHECK(emptyTest < test);
    CHECK_FALSE(test < emptyTest);
    CHECK_FALSE(test == test2);
  };

  SECTION("Comparison test with non-trivial type")
  {
    rdcarray<NonTrivial> test;
    rdcarray<NonTrivial> test2;

    test.push_back(NonTrivial(1));
    test.push_back(NonTrivial(2));
    test.push_back(NonTrivial(3));
    test.push_back(NonTrivial(4));
    test.push_back(NonTrivial(5));
    test.push_back(NonTrivial(6));

    test2.push_back(NonTrivial(1));
    test2.push_back(NonTrivial(2));
    test2.push_back(NonTrivial(3));
    test2.push_back(NonTrivial(3));
    test2.push_back(NonTrivial(4));
    test2.push_back(NonTrivial(6));

    // order is reversed because custom NonTrivial < is reversed
    CHECK(test < test2);
    CHECK(test != test2);
    CHECK_FALSE(test2 < test);

    test2[3].val++;
    test2[4].val++;

    CHECK(test == test2);

    test.pop_back();

    // smaller arrays are considered less-than if all elements are equal
    CHECK(test < test2);
    CHECK_FALSE(test2 < test);
    CHECK_FALSE(test == test2);

    // if however one of the common prefix is greater, that's not true
    test[0].val -= 10;
    CHECK(test2 < test);
    CHECK_FALSE(test < test2);
    CHECK_FALSE(test == test2);

    // ensure in the case of comparing different sized arrays we don't read off the end of one
    rdcarray<NonTrivial> emptyTest;

    CHECK(emptyTest < test);
    CHECK_FALSE(test < emptyTest);
    CHECK_FALSE(test == test2);
  }

  SECTION("Test constructing/assigning from other types")
  {
    rdcarray<int> test;

    SECTION("std::initializer_list")
    {
      test = {2, 3, 4, 5};

      REQUIRE(test.size() == 4);
      CHECK(test[0] == 2);
      CHECK(test[1] == 3);
      CHECK(test[2] == 4);
      CHECK(test[3] == 5);

      rdcarray<int> cc({2, 3, 4, 5});

      REQUIRE(cc.size() == 4);
      CHECK(cc[0] == 2);
      CHECK(cc[1] == 3);
      CHECK(cc[2] == 4);
      CHECK(cc[3] == 5);

      rdcarray<int> ass;

      ass.assign({2, 3, 4, 5});

      REQUIRE(ass.size() == 4);
      CHECK(ass[0] == 2);
      CHECK(ass[1] == 3);
      CHECK(ass[2] == 4);
      CHECK(ass[3] == 5);
    };

    SECTION("other array")
    {
      rdcarray<int> vec = {2, 3, 4, 5};

      test = vec;

      REQUIRE(test.size() == 4);
      CHECK(test[0] == 2);
      CHECK(test[1] == 3);
      CHECK(test[2] == 4);
      CHECK(test[3] == 5);

      rdcarray<int> cc(vec);

      REQUIRE(cc.size() == 4);
      CHECK(cc[0] == 2);
      CHECK(cc[1] == 3);
      CHECK(cc[2] == 4);
      CHECK(cc[3] == 5);

      rdcarray<int> ass;

      ass.assign(vec);

      REQUIRE(ass.size() == 4);
      CHECK(ass[0] == 2);
      CHECK(ass[1] == 3);
      CHECK(ass[2] == 4);
      CHECK(ass[3] == 5);
    };
  };

  SECTION("insert")
  {
    rdcarray<int> vec;
    vec.insert(0, {});
    REQUIRE(vec.size() == 0);
    vec.insert(0, vec);
    REQUIRE(vec.size() == 0);
    vec.insert(0, NULL, 0);
    REQUIRE(vec.size() == 0);
    vec.insert(0, 5);
    REQUIRE(vec.size() == 1);
    CHECK(vec[0] == 5);

    vec.insert(0, {6, 3, 13});
    REQUIRE(vec.size() == 4);
    CHECK(vec[0] == 6);
    CHECK(vec[1] == 3);
    CHECK(vec[2] == 13);
    CHECK(vec[3] == 5);

    vec.insert(0, 9);

    REQUIRE(vec.size() == 5);
    CHECK(vec[0] == 9);
    CHECK(vec[1] == 6);
    CHECK(vec[2] == 3);
    CHECK(vec[3] == 13);
    CHECK(vec[4] == 5);

    vec.insert(3, 8);

    REQUIRE(vec.size() == 6);
    CHECK(vec[0] == 9);
    CHECK(vec[1] == 6);
    CHECK(vec[2] == 3);
    CHECK(vec[3] == 8);
    CHECK(vec[4] == 13);
    CHECK(vec[5] == 5);

    vec.insert(6, 4);

    REQUIRE(vec.size() == 7);
    CHECK(vec[0] == 9);
    CHECK(vec[1] == 6);
    CHECK(vec[2] == 3);
    CHECK(vec[3] == 8);
    CHECK(vec[4] == 13);
    CHECK(vec[5] == 5);
    CHECK(vec[6] == 4);

    vec.insert(3, {20, 21});

    REQUIRE(vec.size() == 9);
    CHECK(vec[0] == 9);
    CHECK(vec[1] == 6);
    CHECK(vec[2] == 3);
    CHECK(vec[3] == 20);
    CHECK(vec[4] == 21);
    CHECK(vec[5] == 8);
    CHECK(vec[6] == 13);
    CHECK(vec[7] == 5);
    CHECK(vec[8] == 4);

    // insert a large amount of data to ensure this doesn't read off start/end of vector
    rdcarray<int> largedata;
    largedata.resize(100000);

    vec.insert(4, largedata);

    REQUIRE(vec.size() == 9 + largedata.size());
    CHECK(vec[0] == 9);
    CHECK(vec[1] == 6);
    CHECK(vec[2] == 3);
    CHECK(vec[3] == 20);
    CHECK(vec[4 + largedata.size()] == 21);
    CHECK(vec[5 + largedata.size()] == 8);
    CHECK(vec[6 + largedata.size()] == 13);
    CHECK(vec[7 + largedata.size()] == 5);
    CHECK(vec[8 + largedata.size()] == 4);

    vec.clear();

    REQUIRE(vec.size() == 0);

    vec.insert(0, {6, 8, 10, 14, 16});

    REQUIRE(vec.size() == 5);
    CHECK(vec[0] == 6);
    CHECK(vec[1] == 8);
    CHECK(vec[2] == 10);
    CHECK(vec[3] == 14);
    CHECK(vec[4] == 16);

    vec.insert(4, {20, 9, 9, 14, 7, 13, 10, 1, 1, 45});

    REQUIRE(vec.size() == 15);
    CHECK(vec[0] == 6);
    CHECK(vec[1] == 8);
    CHECK(vec[2] == 10);
    CHECK(vec[3] == 14);
    CHECK(vec[4] == 20);
    CHECK(vec[5] == 9);
    CHECK(vec[6] == 9);
    CHECK(vec[7] == 14);
    CHECK(vec[8] == 7);
    CHECK(vec[9] == 13);
    CHECK(vec[10] == 10);
    CHECK(vec[11] == 1);
    CHECK(vec[12] == 1);
    CHECK(vec[13] == 45);
    CHECK(vec[14] == 16);
  };

  SECTION("erase")
  {
    rdcarray<int> vec = {6, 3, 13, 5};

    vec.erase(2);

    REQUIRE(vec.size() == 3);
    REQUIRE(vec.capacity() >= 4);
    CHECK(vec[0] == 6);
    CHECK(vec[1] == 3);
    CHECK(vec[2] == 5);

    // do some empty/invalid erases
    vec.erase(2, 0);

    REQUIRE(vec.size() == 3);
    REQUIRE(vec.capacity() >= 4);
    CHECK(vec[0] == 6);
    CHECK(vec[1] == 3);
    CHECK(vec[2] == 5);

    vec.erase(200, 5);

    REQUIRE(vec.size() == 3);
    REQUIRE(vec.capacity() >= 4);
    CHECK(vec[0] == 6);
    CHECK(vec[1] == 3);
    CHECK(vec[2] == 5);

    vec.insert(2, {0, 1});

    REQUIRE(vec.size() == 5);
    CHECK(vec[0] == 6);
    CHECK(vec[1] == 3);
    CHECK(vec[2] == 0);
    CHECK(vec[3] == 1);
    CHECK(vec[4] == 5);

    vec.erase(0);

    REQUIRE(vec.size() == 4);
    CHECK(vec[0] == 3);
    CHECK(vec[1] == 0);
    CHECK(vec[2] == 1);
    CHECK(vec[3] == 5);

    vec.erase(vec.size() - 1);

    REQUIRE(vec.size() == 3);
    CHECK(vec[0] == 3);
    CHECK(vec[1] == 0);
    CHECK(vec[2] == 1);

    vec.erase(0, 3);

    REQUIRE(vec.size() == 0);

    vec = {5, 6, 3, 9, 1, 0};

    vec.erase(2, 3);
    REQUIRE(vec.size() == 3);
    CHECK(vec[0] == 5);
    CHECK(vec[1] == 6);
    CHECK(vec[2] == 0);

    vec = {5, 6, 3, 9, 1, 0};

    vec.erase(3, 3);
    REQUIRE(vec.size() == 3);
    CHECK(vec[0] == 5);
    CHECK(vec[1] == 6);
    CHECK(vec[2] == 3);

    vec = {5, 6, 3, 9, 1, 0};

    vec.erase(3, 100);
    REQUIRE(vec.size() == 3);
    CHECK(vec[0] == 5);
    CHECK(vec[1] == 6);
    CHECK(vec[2] == 3);
  };

  SECTION("removeOne / removeOneIf / removeIf")
  {
    rdcarray<int> vec = {6, 3, 9, 6, 6, 3, 5, 15, 5};

    vec.removeOne(3);

    REQUIRE(vec.size() == 8);
    CHECK(vec[0] == 6);
    CHECK(vec[1] == 9);
    CHECK(vec[2] == 6);
    CHECK(vec[3] == 6);
    CHECK(vec[4] == 3);
    CHECK(vec[5] == 5);
    CHECK(vec[6] == 15);
    CHECK(vec[7] == 5);

    vec.removeOne(3);

    REQUIRE(vec.size() == 7);
    CHECK(vec[0] == 6);
    CHECK(vec[1] == 9);
    CHECK(vec[2] == 6);
    CHECK(vec[3] == 6);
    CHECK(vec[4] == 5);
    CHECK(vec[5] == 15);
    CHECK(vec[6] == 5);

    vec.removeOne(3);

    REQUIRE(vec.size() == 7);
    CHECK(vec[0] == 6);
    CHECK(vec[1] == 9);
    CHECK(vec[2] == 6);
    CHECK(vec[3] == 6);
    CHECK(vec[4] == 5);
    CHECK(vec[5] == 15);
    CHECK(vec[6] == 5);

    vec.removeOneIf([](const int &el) { return (el % 3) == 0; });

    REQUIRE(vec.size() == 6);
    CHECK(vec[0] == 9);
    CHECK(vec[1] == 6);
    CHECK(vec[2] == 6);
    CHECK(vec[3] == 5);
    CHECK(vec[4] == 15);
    CHECK(vec[5] == 5);

    vec.removeIf([](const int &el) { return (el % 3) == 0; });

    REQUIRE(vec.size() == 2);
    CHECK(vec[0] == 5);
    CHECK(vec[1] == 5);
  };

  SECTION("resize_for_index")
  {
    rdcarray<int> test;

    CHECK(test.empty());

    test.resize_for_index(0);

    CHECK(test.size() == 1);
    CHECK(test.capacity() >= 1);

    test.resize_for_index(5);

    CHECK(test.size() == 6);
    CHECK(test.capacity() >= 6);

    test.resize_for_index(5);

    CHECK(test.size() == 6);
    CHECK(test.capacity() >= 6);

    test.resize_for_index(3);

    CHECK(test.size() == 6);
    CHECK(test.capacity() >= 6);

    test.resize_for_index(0);

    CHECK(test.size() == 6);
    CHECK(test.capacity() >= 6);

    test.resize_for_index(9);

    CHECK(test.size() == 10);
    CHECK(test.capacity() >= 10);
  };

  SECTION("Check construction")
  {
    rdcarray<ConstructorCounter> test;

    CHECK(constructor == 0);
    CHECK(moveConstructor == 0);
    CHECK(valueConstructor == 0);
    CHECK(copyConstructor == 0);
    CHECK(destructor == 0);

    ConstructorCounter tmp;
    tmp.value = 9;

    test.push_back(tmp);

    // 1 for the temporary
    CHECK(constructor == 1);
    // 1 for the element inside array
    CHECK(copyConstructor == 1);

    // previous values
    CHECK(valueConstructor == 0);
    CHECK(destructor == 0);

    CHECK(test[0].value == 9);
    CHECK(tmp.value == 9);

    test.clear();

    // destroyed item inside array
    CHECK(destructor == 1);

    // previous values
    CHECK(constructor == 1);
    CHECK(copyConstructor == 1);
    CHECK(valueConstructor == 0);
    CHECK(moveConstructor == 0);

    test.push_back(ConstructorCounter(10));

    CHECK(test[0].value == 10);

    // for the temporary going into push_back
    CHECK(valueConstructor == 1);
    // for the temporary going out of scope
    CHECK(destructor == 2);
    // for the temporary being moved into the new element
    CHECK(copyConstructor == 1);
    CHECK(moveConstructor == 1);

    // previous value
    CHECK(constructor == 1);

    test.reserve(1000);

    // single element in test was moved to new backing storage
    CHECK(destructor == 3);
    CHECK(copyConstructor == 1);
    CHECK(moveConstructor == 2);

    // previous values
    CHECK(valueConstructor == 1);
    CHECK(constructor == 1);

    test.resize(50);

    // 49 default initialisations
    CHECK(constructor == 50);

    // previous values
    CHECK(valueConstructor == 1);
    CHECK(destructor == 3);
    CHECK(copyConstructor == 1);
    CHECK(moveConstructor == 2);

    test.clear();

    // 50 destructions
    CHECK(destructor == 53);

    // previous values
    CHECK(constructor == 50);
    CHECK(valueConstructor == 1);
    CHECK(copyConstructor == 1);
    CHECK(moveConstructor == 2);

    // reset counters
    constructor = 0;
    valueConstructor = 0;
    copyConstructor = 0;
    destructor = 0;
    moveConstructor = 0;

    CHECK(constructor == 0);
    CHECK(moveConstructor == 0);
    CHECK(valueConstructor == 0);
    CHECK(copyConstructor == 0);
    CHECK(destructor == 0);

    auto lambda = []() -> rdcarray<ConstructorCounter> {
      rdcarray<ConstructorCounter> ret;
      ConstructorCounter tmp(9);
      ret.push_back(tmp);
      return ret;
    };

    test = lambda();

    // ensure that the value constructor was called only once within the lambda
    CHECK(valueConstructor == 1);

    // the copy constructor was called in push_back
    CHECK(copyConstructor == 1);

    // the destructor was called once for tmp
    CHECK(destructor == 1);

    // and no default construction or moves
    CHECK(constructor == 0);
    CHECK(moveConstructor == 0);

    // check that the new value arrived
    CHECK(test.back().value == 9);

    // no assignments
    CHECK(copyAssignment == 0);
    CHECK(moveAssignment == 0);
  };

  SECTION("operations with empty array")
  {
    rdcarray<ConstructorCounter> test;

    ConstructorCounter val;

    test.append(test);

    CHECK(test.empty());

    test.insert(0, test);

    CHECK(test.empty());

    test.removeOne(val);

    CHECK(test.empty());

    test.assign(test.data(), test.size());

    CHECK(test.empty());

    CHECK(test.indexOf(val) == -1);

    rdcarray<ConstructorCounter> test2(test);

    CHECK(test.empty());
    CHECK(test2.empty());

    rdcarray<ConstructorCounter> test3;

    test3 = test;

    CHECK(test.empty());
    CHECK(test3.empty());

    CHECK(constructor == 1);
    CHECK(moveConstructor == 0);
    CHECK(valueConstructor == 0);
    CHECK(copyConstructor == 0);
    CHECK(destructor == 0);
  };

  SECTION("Inserting from array into itself")
  {
    constructor = 0;
    valueConstructor = 0;
    copyConstructor = 0;
    destructor = 0;

    rdcarray<ConstructorCounter> test;

    // ensure no re-allocations due to size
    test.reserve(100);

    test.resize(5);
    test[0].value = 10;
    test[1].value = 20;
    test[2].value = 30;
    test[3].value = 40;
    test[4].value = 50;

    CHECK(constructor == 5);
    CHECK(valueConstructor == 0);
    CHECK(copyConstructor == 0);
    CHECK(destructor == 0);

    CHECK(test.capacity() == 100);
    CHECK(test.size() == 5);

    ConstructorCounter tmp;
    tmp.value = 999;

    // 5 constructed objects in the array, and tmp
    CHECK(constructor == 6);
    CHECK(valueConstructor == 0);
    CHECK(copyConstructor == 0);
    CHECK(destructor == 0);

    // this should shift everything up, and copy-construct the element into place
    test.insert(0, tmp);

    CHECK(test.capacity() == 100);
    CHECK(test.size() == 6);

    // 5 moves and 5 destructs to shift the array contents up, then a copy for inserting tmp
    CHECK(constructor == 6);
    CHECK(valueConstructor == 0);
    CHECK(moveConstructor == 5);
    CHECK(destructor == 5);
    CHECK(copyConstructor == 1);

    CHECK(test[0].value == 999);
    CHECK(test[1].value == 10);

    constructor = valueConstructor = copyConstructor = moveConstructor = destructor = 0;

    // this should copy the value, then do an insert
    test.insert(0, test[0]);

    CHECK(test.capacity() == 100);
    CHECK(test.size() == 7);

    // another 6 moves & destructs to shift the array contents, 2 copies for
    // inserting test[0] (once to a temporary with a destructor of that once into the new storage)
    CHECK(constructor == 0);
    CHECK(valueConstructor == 0);
    CHECK(copyConstructor == 2);
    CHECK(moveConstructor == 6);
    CHECK(destructor == 6 + 1);

    CHECK(test[0].value == 999);
    CHECK(test[1].value == 999);
    CHECK(test[2].value == 10);

    constructor = valueConstructor = copyConstructor = moveConstructor = destructor = 0;

    // this should detect the overlapped range, and duplicate the whole object
    test.insert(0, test.data(), 3);

    // ensure the correct size and allocated space
    CHECK(test.capacity() == 100);
    CHECK(test.size() == 10);

    CHECK(test[0].value == 999);
    CHECK(test[1].value == 999);
    CHECK(test[2].value == 10);
    CHECK(test[3].value == 999);
    CHECK(test[4].value == 999);
    CHECK(test[5].value == 10);

    // - 7 copies and destructs for the duplication (copies into the new storage, destructs from the
    // old storage)
    // - 7 moves and destructs for shifting the array contents
    // - 3 copies for the inserted items
    CHECK(constructor == 0);
    CHECK(valueConstructor == 0);
    CHECK(copyConstructor == 7 + 3);
    CHECK(destructor == 7 + 7);
    CHECK(moveConstructor == 7);
  };

  SECTION("Inserting from array's unused memory into itself")
  {
    constructor = 0;
    valueConstructor = 0;
    copyConstructor = 0;
    destructor = 0;

    rdcarray<ConstructorCounter> test;

    // ensure no re-allocations due to size
    test.reserve(100);

    test.resize(5);
    test[0].value = 10;
    test[1].value = 20;
    test[2].value = 30;
    test[3].value = 40;
    test[4].value = 50;

    test.resize(1);

    CHECK(constructor == 5);
    CHECK(valueConstructor == 0);
    CHECK(copyConstructor == 0);
    CHECK(destructor == 4);

    constructor = destructor = 0;

    // this should detect the overlapped range, and duplicate the whole object
    test.insert(0, test.data() + 2, 3);

    // ensure the correct size and allocated space
    CHECK(test.capacity() == 100);
    CHECK(test.size() == 4);

    CHECK(test[0].value == -1234);
    CHECK(test[1].value == -1234);
    CHECK(test[2].value == -1234);
    CHECK(test[3].value == 10);

    // on top of the above:
    // - 1 copy and destruct for the duplication (copy into the new storage, destruct from
    // the old storage)
    // - 1 move and destruct for shifting the array contents
    // - 3 copies for the inserted items
    CHECK(constructor == 0);
    CHECK(valueConstructor == 0);
    CHECK(copyConstructor == 1 + 3);
    CHECK(destructor == 1 + 1);
    CHECK(moveConstructor == 1);
  };

  SECTION("Check move constructor is used when possible")
  {
    rdcarray<ConstructorCounter> test;

    // don't test moves due to resizes in this test
    test.reserve(100);

    CHECK(constructor == 0);
    CHECK(moveConstructor == 0);
    CHECK(valueConstructor == 0);
    CHECK(copyConstructor == 0);
    CHECK(destructor == 0);

    ConstructorCounter tmp;
    tmp.value = 9;

    // 1 for the temporary
    CHECK(constructor == 1);

    test.push_back(tmp);

    // element should have been copied, not moved
    CHECK(copyConstructor == 1);
    CHECK(moveConstructor == 0);
    CHECK(destructor == 0);

    test.push_back(ConstructorCounter(5));

    // one more temporary as a value
    CHECK(valueConstructor == 1);
    // temporary should have been moved, not copied
    CHECK(copyConstructor == 1);
    CHECK(moveConstructor == 1);
    // the temporary should have been destroyed, but after it was moved
    CHECK(destructor == 1);
    CHECK(movedDestructor == 1);

    test.emplace_back(15);

    // should be constructed in place
    CHECK(valueConstructor == 2);
    // no other move/copy/destruct
    CHECK(copyConstructor == 1);
    CHECK(moveConstructor == 1);
    CHECK(destructor == 1);
    CHECK(movedDestructor == 1);

    test.emplace_back(25);
    test.emplace_back(35);

    CHECK(valueConstructor == 4);
    CHECK(copyConstructor == 1);
    CHECK(moveConstructor == 1);
    CHECK(destructor == 1);
    CHECK(movedDestructor == 1);

    CHECK(test.size() == 5);

    // insert a new element at 0, without allowing move of the new element
    test.insert(0, &tmp, 1);

    CHECK(test.size() == 6);
    CHECK(valueConstructor == 4);
    // the element will be copied into place
    CHECK(copyConstructor == 2);
    // all the internal shifts for the resize should have happened via move constructor
    CHECK(moveConstructor == 6);
    CHECK(destructor == 6);
    CHECK(movedDestructor == 6);
    CHECK(copyAssignment == 0);
    CHECK(moveAssignment == 0);

    // insert an element at an invalid offset, this should do nothing
    test.insert(100, &tmp, 1);

    CHECK(test.size() == 6);
    CHECK(valueConstructor == 4);
    CHECK(copyConstructor == 2);
    CHECK(moveConstructor == 6);
    CHECK(destructor == 6);
    CHECK(movedDestructor == 6);
    CHECK(copyAssignment == 0);
    CHECK(moveAssignment == 0);

    // self-assignment should also do nothing
    test = test;

    CHECK(test.size() == 6);
    CHECK(valueConstructor == 4);
    CHECK(copyConstructor == 2);
    CHECK(moveConstructor == 6);
    CHECK(destructor == 6);
    CHECK(movedDestructor == 6);
    CHECK(copyAssignment == 0);
    CHECK(moveAssignment == 0);

    // insert an element at 0, allowing it to move
    test.insert(4, ConstructorCounter(55));

    CHECK(test.size() == 7);
    CHECK(valueConstructor == 5);
    CHECK(copyConstructor == 2);
    CHECK(moveConstructor == 9);
    CHECK(destructor == 9);
    CHECK(movedDestructor == 9);
    CHECK(copyAssignment == 0);
    CHECK(moveAssignment == 0);

    // erase the element at 3
    test.erase(3);

    CHECK(test.size() == 6);
    CHECK(valueConstructor == 5);
    CHECK(copyConstructor == 2);
    CHECK(moveConstructor == 12);
    CHECK(destructor == 13);
    CHECK(movedDestructor == 12);
    CHECK(copyAssignment == 0);
    CHECK(moveAssignment == 0);

    test[1].value = 10;

    CHECK(test[0].value == 9);
    CHECK(test[1].value == 10);
    CHECK(test[2].value == 5);
    CHECK(test[3].value == 55);
    CHECK(test[4].value == 25);
    CHECK(test[5].value == 35);

    // this should move
    test.push_back(std::move(test[0]));

    CHECK(test.back().value == 9);
    CHECK(test.size() == 7);
    CHECK(valueConstructor == 5);
    CHECK(copyConstructor == 2);
    CHECK(moveConstructor == 13);
    CHECK(destructor == 13);
    CHECK(movedDestructor == 12);
    CHECK(copyAssignment == 0);
    CHECK(moveAssignment == 0);

    // insert by moving at the end, this will only do the one move construct from [1] to [7]
    test.insert(7, std::move(test[1]));

    CHECK(test[7].value == 10);
    CHECK(test.size() == 8);
    CHECK(valueConstructor == 5);
    CHECK(copyConstructor == 2);
    CHECK(moveConstructor == 14);
    CHECK(destructor == 13);
    CHECK(movedDestructor == 12);
    CHECK(copyAssignment == 0);
    CHECK(moveAssignment == 0);

    // insert with some moves required to shuffle up values, but the move itself still happens from
    // the same element (lower to higher index)
    test.insert(5, std::move(test[2]));

    CHECK(test[5].value == 5);
    CHECK(test.size() == 9);
    CHECK(valueConstructor == 5);
    CHECK(copyConstructor == 2);
    CHECK(moveConstructor == 18);
    CHECK(destructor == 16);
    CHECK(movedDestructor == 15);
    CHECK(copyAssignment == 0);
    CHECK(moveAssignment == 0);

    // insert with some moves required to shuffle up values, plus the move itself happens from one
    // of the moved elements
    test.insert(7, std::move(test[8]));

    CHECK(test[7].value == 10);
    CHECK(test.size() == 10);
    CHECK(valueConstructor == 5);
    CHECK(copyConstructor == 2);
    CHECK(moveConstructor == 21);
    CHECK(destructor == 18);
    CHECK(movedDestructor == 17);
    CHECK(copyAssignment == 0);
    CHECK(moveAssignment == 0);

    // invalid move insert
    test.insert(100, std::move(test[8]));

    CHECK(test[7].value == 10);
    CHECK(test.size() == 10);
    CHECK(valueConstructor == 5);
    CHECK(copyConstructor == 2);
    CHECK(moveConstructor == 21);
    CHECK(destructor == 18);
    CHECK(movedDestructor == 17);
    CHECK(copyAssignment == 0);
    CHECK(moveAssignment == 0);
  };
};

#define CHECK_NULL_TERM(str) CHECK(str.c_str()[str.size()] == '\0');
#define SMALL_STRING "Small str!"
#define LARGE_STRING \
  "String literal that cannot be stored directly in a small-string optimisation array!"
#define VERY_LARGE_STRING \
  R"(So: Lorem ipsum dolor sit amet, consectetur adipiscing elit. Fusce viverra dui dolor. Donec fermentum metus eu lorem rutrum, nec sodales urna vehicula. Praesent finibus tincidunt volutpat. Aliquam ullamcorper metus semper suscipit dignissim. Phasellus at odio nec arcu venenatis euismod id eget mi. Vestibulum consequat nisi sed massa venenatis, vel pellentesque nunc semper. Maecenas porttitor nulla non purus pellentesque pharetra. Ut ornare rhoncus massa at eleifend. Sed ultricies tincidunt bibendum. Pellentesque neque dolor, elementum eget scelerisque et, euismod at tortor. Duis vel porta sapien. Integer facilisis nisl condimentum tempor faucibus. Sed convallis tempus dolor quis fringilla. Nam dictum accumsan quam, eget pretium turpis mattis id. Praesent vitae enim ut est porttitor consectetur et at ante. Proin porttitor quam eu enim gravida, eget congue diam dapibus.!)"

TEST_CASE("Test string type", "[basictypes][string]")
{
  RDCCOMPILE_ASSERT(sizeof(rdcstr) == sizeof(void *) * 3, "rdcstr is mis-sized");

  SECTION("Empty strings")
  {
    const rdcstr test;

    // should not have any data in it
    CHECK(test.size() == 0);
    CHECK(test.empty());
    CHECK(test.isEmpty());
    CHECK(test.begin() == test.end());

    CHECK(test.c_str() != NULL);
    CHECK_NULL_TERM(test);
    CHECK(test == "");
    CHECK(test == ((const char *)NULL));
    CHECK(test == rdcstr());
  };

  SECTION("Empty string after containing data")
  {
    auto lambda = [](rdcstr test, const char *c_str) {
      test.clear();

      CHECK(test.size() == 0);
      CHECK(test.empty());
      CHECK(test.isEmpty());
      CHECK(test.begin() == test.end());

      CHECK(test.c_str() != NULL);
      CHECK_NULL_TERM(test);
    };

    lambda(SMALL_STRING, SMALL_STRING);
    lambda(LARGE_STRING, LARGE_STRING);
    lambda(VERY_LARGE_STRING, VERY_LARGE_STRING);
    lambda(STRING_LITERAL(LARGE_STRING), LARGE_STRING);
  };

  SECTION("Small string read-only accessors")
  {
    auto lambda = [](const rdcstr &test, const char *str) {
      const size_t len = strlen(str);

      CHECK(test.size() == len);
      CHECK(test.capacity() >= len);
      CHECK_FALSE(test.empty());
      CHECK_FALSE(test.isEmpty());
      CHECK(test.begin() + len == test.end());

      CHECK(strlen(test.c_str()) == len);
      CHECK(test.c_str() != NULL);
      CHECK(strcmp(test.c_str(), str) == 0);
      CHECK(strcmp(test.data(), str) == 0);
      CHECK(test != ((const char *)NULL));
      CHECK(test == str);
      CHECK(test == rdcstr(str));
      CHECK_NULL_TERM(test);

      CHECK(test.front() == 'S');
      CHECK(test.back() == '!');
    };

    lambda(SMALL_STRING, SMALL_STRING);
    lambda(LARGE_STRING, LARGE_STRING);
    lambda(VERY_LARGE_STRING, VERY_LARGE_STRING);
    lambda(STRING_LITERAL(LARGE_STRING), LARGE_STRING);
  };

  SECTION("String read-only accessors after modification")
  {
    auto lambda = [](rdcstr test, const char *str) {
      const size_t len = strlen(str);

      test[4] = '!';

      CHECK(test.size() == len);
      CHECK(test.capacity() >= len);
      CHECK_FALSE(test.empty());
      CHECK_FALSE(test.isEmpty());
      CHECK(test.begin() + len == test.end());

      CHECK(strlen(test.c_str()) == len);
      CHECK(test.c_str() != NULL);
      CHECK(strcmp(test.c_str(), str) < 0);
      CHECK(strcmp(test.data(), str) < 0);
      CHECK(test != str);
      CHECK(test != rdcstr(str));
      CHECK_NULL_TERM(test);

      CHECK(test.front() == 'S');
      CHECK(test.back() == '!');
    };

    lambda(SMALL_STRING, SMALL_STRING);
    lambda(LARGE_STRING, LARGE_STRING);
    lambda(VERY_LARGE_STRING, VERY_LARGE_STRING);
    lambda(STRING_LITERAL(LARGE_STRING), LARGE_STRING);
  };

  SECTION("String copies")
  {
    auto lambda = [](const rdcstr &test, const char *str) {
      rdcstr test2;

      test2 = test;

      CHECK(test.size() == test2.size());
      CHECK(test.empty() == test2.empty());

      for(size_t i = 0; i < test.size(); i++)
      {
        CHECK(test[i] == test2[i]);
      }

      CHECK(test.c_str() != test2.c_str());

      rdcstr empty;

      test2 = empty;

      CHECK(test2.size() == empty.size());
      CHECK(test2.empty() == empty.empty());
    };

    lambda(SMALL_STRING, SMALL_STRING);
    lambda(LARGE_STRING, LARGE_STRING);
    lambda(VERY_LARGE_STRING, VERY_LARGE_STRING);
    lambda(STRING_LITERAL(LARGE_STRING), LARGE_STRING);
  };

  SECTION("Assigning a string to itself")
  {
    auto lambda = [](rdcstr test) {
      // create a version without doing self-insertion
      rdcstr test2 = test;

      // self-assign
      test2 = test2;

      CHECK(test.size() == test2.size());
      CHECK(test.empty() == test2.empty());

      for(size_t i = 0; i < test.size(); i++)
      {
        CHECK(test[i] == test2[i]);
      }

      CHECK(test.c_str() != test2.c_str());
    };

    lambda(SMALL_STRING);
    lambda(LARGE_STRING);
    lambda(VERY_LARGE_STRING);
    lambda(STRING_LITERAL(LARGE_STRING));
  };

  SECTION("Inserting from string into itself")
  {
    auto lambda = [](rdcstr test) {
      // create a version without doing self-insertion
      rdcstr test2 = test;

      test2.insert(4, test);

      test.insert(4, test);

      CHECK(test.size() == test2.size());
      CHECK(test.empty() == test2.empty());

      for(size_t i = 0; i < test.size(); i++)
      {
        CHECK(test[i] == test2[i]);
      }

      CHECK(test.c_str() != test2.c_str());
    };

    // need a small string small enough that even doubling it is still small
    lambda("foo");
    lambda(SMALL_STRING);
    lambda(LARGE_STRING);
    lambda(VERY_LARGE_STRING);
    lambda(STRING_LITERAL(LARGE_STRING));
  };

  SECTION("Shrinking and expanding strings")
  {
    rdcstr test = "A longer string that would have been heap allocated";
    test.resize(5);

    CHECK(test.size() == 5);
    CHECK_NULL_TERM(test);
    CHECK(test == "A lon");

    // should do nothing
    test.resize(5);

    CHECK(test.size() == 5);
    CHECK_NULL_TERM(test);
    CHECK(test == "A lon");

    // this copy will copy to the internal array since it's small enough now
    rdcstr test2 = test;

    CHECK(test2.size() == 5);
    CHECK_NULL_TERM(test2);
    CHECK(test2 == "A lon");

    test2 = "abcdefghij";

    CHECK(test2.size() == 10);
    CHECK_NULL_TERM(test2);

    test2.resize(3);

    CHECK(test2.size() == 3);
    CHECK_NULL_TERM(test2);

    test2.resize(6);

    CHECK(test2.size() == 6);
    CHECK_NULL_TERM(test2);

    test.resize(12345);

    CHECK(test.capacity() == 12345);
    CHECK(test.size() == 12345);

    const char *prev_ptr = test.c_str();

    // this could fit in the internal array but to avoid allocation thrashing we should keep the
    // same allocation
    test = "Short str";

    CHECK(test.capacity() == 12345);
    CHECK(test.size() == 9);
    CHECK(test.c_str() == prev_ptr);
    CHECK_NULL_TERM(test);
    CHECK(test == "Short str");

    test.resize(4);

    CHECK(test.size() == 4);
    CHECK_NULL_TERM(test);
    CHECK(test == "Shor");

    test.resize(8);

    CHECK(test.size() == 8);
    CHECK_NULL_TERM(test);
    CHECK(test == "Shor");
    CHECK(test[4] == 0);
    CHECK(test[5] == 0);
    CHECK(test[6] == 0);
    CHECK(test[7] == 0);
  };

  SECTION("erase")
  {
    rdcstr test = "Hello, World! This is a test string";

    test.erase(0, 0);

    CHECK(test == "Hello, World! This is a test string");

    test.erase(0, 1);

    CHECK(test == "ello, World! This is a test string");

    test.erase(0, 4);

    CHECK(test == ", World! This is a test string");

    test.erase(9, 5);

    CHECK(test == ", World! is a test string");

    test.erase(14, 1000);

    CHECK(test == ", World! is a ");

    test.erase(100, 1);

    CHECK(test == ", World! is a ");

    test.erase(100, 100);

    CHECK(test == ", World! is a ");
  };

  SECTION("append")
  {
    rdcstr test = "Hello";

    test += " World";

    CHECK(test.size() == 11);
    CHECK_NULL_TERM(test);
    CHECK(test == "Hello World");

    rdcstr test2 = test + "!";

    CHECK(test2.size() == 12);
    CHECK_NULL_TERM(test2);
    CHECK(test2 == "Hello World!");

    test2 += " And enough characters to force an allocation";

    CHECK(test2 == "Hello World! And enough characters to force an allocation");

    test2 += ", " + test + "?";

    CHECK(test2 == "Hello World! And enough characters to force an allocation, Hello World?");

    test += '?';
    CHECK(test == "Hello World?");

    test2 = test + '!';

    CHECK(test2 == "Hello World?!");

    test2.append(" cstring");

    CHECK(test2 == "Hello World?! cstring");

    test2.append(rdcstr(" rdcstr"));

    CHECK(test2 == "Hello World?! cstring rdcstr");

    test2.append(" cstring that is truncated", 5);

    CHECK(test2 == "Hello World?! cstring rdcstr cstr");
  };

  SECTION("insert")
  {
    rdcstr test = "Hello World!";

    test.insert(5, ",");

    CHECK(test == "Hello, World!");

    rdcstr test2 = test;

    test2.insert(0, test);

    CHECK(test2 == "Hello, World!Hello, World!");

    test2.insert(100, "foo");

    CHECK(test2 == "Hello, World!Hello, World!");

    test2.insert(4, '_');

    CHECK(test2 == "Hell_o, World!Hello, World!");
  };

  SECTION("replace")
  {
    rdcstr test = "Hello, World!";

    test.replace(5, 1, ".");

    CHECK(test == "Hello. World!");

    test.replace(7, 3, "Fau");

    CHECK(test == "Hello. Fauld!");

    test.replace(0, 0, "Hi! ");

    CHECK(test == "Hi! Hello. Fauld!");

    test.replace(0, 99, "Test");

    CHECK(test == "Test");

    test.replace(2, 99, "sting!");

    CHECK(test == "Testing!");

    test.replace(20, 99, "Invalid?");

    CHECK(test == "Testing!");
  };

  SECTION("beginsWith / endsWith")
  {
    rdcstr test = "foobar";

    CHECK_FALSE(test.beginsWith("bar"));
    CHECK(test.beginsWith("foo"));
    CHECK(test.beginsWith(""));

    CHECK(test.endsWith("bar"));
    CHECK_FALSE(test.endsWith("foo"));
    CHECK(test.endsWith(""));

    test = "";

    CHECK(test.endsWith(""));
    CHECK_FALSE(test.endsWith("foo"));

    CHECK(test.beginsWith(""));
    CHECK_FALSE(test.beginsWith("foo"));

    test = "bar";

    CHECK_FALSE(test.beginsWith("foobar"));
    CHECK_FALSE(test.endsWith("foobar"));
  };

  SECTION("trim / trimmed")
  {
    CHECK(rdcstr("  foo bar  ").trimmed() == "foo bar");
    CHECK(rdcstr("  Foo bar").trimmed() == "Foo bar");
    CHECK(rdcstr("  Foo\nbar").trimmed() == "Foo\nbar");
    CHECK(rdcstr("FOO BAR  ").trimmed() == "FOO BAR");
    CHECK(rdcstr("FOO BAR  \t\n").trimmed() == "FOO BAR");
    CHECK(rdcstr("1").trimmed() == "1");
    CHECK(rdcstr("  1  ").trimmed() == "1");
    CHECK(rdcstr("  1").trimmed() == "1");
    CHECK(rdcstr("1  ").trimmed() == "1");
    CHECK(rdcstr("1\n ").trimmed() == "1");
    CHECK(rdcstr("\n1\n ").trimmed() == "1");
    CHECK(rdcstr(" \n\t1\n ").trimmed() == "1");
    CHECK(rdcstr("").trimmed() == "");
    CHECK(rdcstr("  ").trimmed() == "");
    CHECK(rdcstr("  \t  \n ").trimmed() == "");
  };

  SECTION("fill")
  {
    rdcstr s;
    s = "Foo bar";

    CHECK(s == "Foo bar");

    s.fill(4, 'o');

    CHECK(s == "oooo");
    CHECK(s.size() == 4);

    s.fill(0, 'o');

    CHECK(s == "");
    CHECK(s.size() == 0);

    s.fill(10, '_');

    CHECK(s == "__________");
    CHECK(s.size() == 10);
  };

  SECTION("push_back and pop_back")
  {
    rdcstr test = "Hello, World!";

    test.push_back('!');

    CHECK(test == "Hello, World!!");

    test.push_back('!');

    CHECK(test == "Hello, World!!!");

    test.pop_back();

    CHECK(test == "Hello, World!!");

    test.pop_back();

    CHECK(test == "Hello, World!");

    test.pop_back();

    CHECK(test == "Hello, World");

    test.clear();

    CHECK(test == "");

    test.pop_back();

    CHECK(test == "");

    test = "Longer string to force a heap allocation: Hello, World!";

    test.push_back('!');

    CHECK(test == "Longer string to force a heap allocation: Hello, World!!");

    test.pop_back();

    CHECK(test == "Longer string to force a heap allocation: Hello, World!");

    test.pop_back();

    CHECK(test == "Longer string to force a heap allocation: Hello, World");

    test.clear();

    CHECK(test == "");

    test.pop_back();

    CHECK(test == "");
  };

  SECTION("substr")
  {
    rdcstr test = "Hello, World!";

    CHECK(test.substr(0) == "Hello, World!");
    CHECK(test.substr(1) == "ello, World!");
    CHECK(test.substr(5) == ", World!");
    CHECK(test.substr(13) == "");
    CHECK(test.substr(100) == "");
    CHECK(test.substr(5, 2) == ", ");
    CHECK(test.substr(5, 100) == ", World!");

    test = "Hello, World! Hello, World! Hello, World! Hello, World! Hello, World!";

    CHECK(test.substr(0) ==
          "Hello, World! Hello, World! Hello, World! Hello, World! Hello, World!");
    CHECK(test.substr(1) == "ello, World! Hello, World! Hello, World! Hello, World! Hello, World!");
    CHECK(test.substr(5) == ", World! Hello, World! Hello, World! Hello, World! Hello, World!");
    CHECK(test.substr(13) == " Hello, World! Hello, World! Hello, World! Hello, World!");
    CHECK(test.substr(69) == "");
    CHECK(test.substr(100) == "");
    CHECK(test.substr(5, 2) == ", ");
    CHECK(test.substr(5, 100) ==
          ", World! Hello, World! Hello, World! Hello, World! Hello, World!");

    test = "Hello, World! Hello, World! Hello, World! Hello, World! Hello, World!"_lit;

    CHECK(test.substr(0) ==
          "Hello, World! Hello, World! Hello, World! Hello, World! Hello, World!");
    CHECK(test.substr(1) == "ello, World! Hello, World! Hello, World! Hello, World! Hello, World!");
    CHECK(test.substr(5) == ", World! Hello, World! Hello, World! Hello, World! Hello, World!");
    CHECK(test.substr(13) == " Hello, World! Hello, World! Hello, World! Hello, World!");
    CHECK(test.substr(69) == "");
    CHECK(test.substr(100) == "");
    CHECK(test.substr(5, 2) == ", ");
    CHECK(test.substr(5, 100) ==
          ", World! Hello, World! Hello, World! Hello, World! Hello, World!");
  };

  SECTION("searching")
  {
    rdcstr test = "Hello, World!";

    CHECK(test.find("Hello") == 0);
    CHECK(test.find("World") == 7);
    CHECK(test.find("ld!") == 10);
    CHECK(test.find("Foobar") == -1);
    CHECK(test.find("Hello, World!!") == -1);
    CHECK(test.find("Hello, World?") == -1);
    CHECK(test.find("") == 0);
    CHECK(test.find(',') == 5);

    CHECK(test.indexOf('H') == 0);
    CHECK(test.indexOf('l') == 2);
    CHECK(test.indexOf('?') == -1);

    CHECK(test.indexOf('o') == 4);
    CHECK(test.indexOf('o', 4) == 4);
    CHECK(test.indexOf('o', 5) == 8);
    CHECK(test.indexOf('o', 10) == -1);
    CHECK(test.indexOf('o', -1) == -1);

    CHECK(test.indexOf('o', 0, -1) == 4);
    CHECK(test.indexOf('o', 0, 100) == 4);
    CHECK(test.indexOf('o', 5, -1) == 8);
    CHECK(test.indexOf('o', 5, 100) == 8);
    CHECK(test.indexOf('o', 5, 9) == 8);
    CHECK(test.indexOf('o', 5, 8) == -1);

    CHECK(test.contains('!'));
    CHECK_FALSE(test.contains('?'));

    CHECK(test.contains('H'));
    CHECK(test.contains("Hello"));
    CHECK(test.contains(rdcstr("Hello")));

    char H = test.takeAt(0);

    CHECK(H == 'H');
    CHECK_FALSE(test.contains('H'));
    CHECK_FALSE(test.contains("Hello"));

    test.removeOne('!');

    CHECK_FALSE(test.contains('!'));

    CHECK(test == "ello, World");

    CHECK(test.find_first_of("lo") == 1);
    CHECK(test.find_first_of("ol") == 1);
    CHECK(test.find_first_of("foobarl") == 1);
    CHECK(test.find_first_of("foobar") == 3);
    CHECK(test.find_first_of("oforab") == 3);
    CHECK(test.find_first_of("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!o") == 3);
    CHECK(test.find_first_of("!?$") == -1);
    CHECK(test.find_first_of("") == -1);

    CHECK(test.find_last_of("or") == 8);
    CHECK(test.find_last_of("ro") == 8);
    CHECK(test.find_last_of("foobard") == 10);
    CHECK(test.find_last_of("foobar") == 8);
    CHECK(test.find_last_of("oforab") == 8);
    CHECK(test.find_last_of("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!r") == 8);
    CHECK(test.find_last_of("!?$") == -1);
    CHECK(test.find_last_of("") == -1);

    CHECK(test.find_first_not_of("oel") == 4);
    CHECK(test.find_first_not_of("le") == 3);
    CHECK(test.find_first_not_of("oelWr") == 4);
    CHECK(test.find_first_not_of("ooollele") == 4);
    CHECK(test.find_first_not_of("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!e") == 1);
    CHECK(test.find_first_not_of("!?$") == 0);
    CHECK(test.find_first_not_of("") == 0);
    CHECK(test.find_first_not_of("W, rdleo") == -1);

    CHECK(test.find_last_not_of("dl") == 8);
    CHECK(test.find_last_not_of("ld") == 8);
    CHECK(test.find_last_not_of("ldWr") == 7);
    CHECK(test.find_last_not_of("WWrldlRw") == 7);
    CHECK(test.find_last_not_of("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!d") == 9);
    CHECK(test.find_last_not_of("!?$") == 10);
    CHECK(test.find_last_not_of("") == 10);
    CHECK(test.find_last_not_of("W, rdleo") == -1);

    //      0         1         2         3
    //      012345678901234567890123456789012345678
    test = "Test of substring matches and sub-find!";

    // test with first before of the first 'sub'
    CHECK(test.find("sub") == 8);
    CHECK(test.find("sub", 4) == 8);
    CHECK(test.find("sub", 8) == 8);

    // first after first and before second
    CHECK(test.find("sub", 9) == 30);
    CHECK(test.find("sub", 10) == 30);
    CHECK(test.find("sub", 11) == 30);
    CHECK(test.find("sub", 29) == 30);
    CHECK(test.find("sub", 30) == 30);

    // first past the second
    CHECK(test.find("sub", 31) == -1);

    // first past the end of the string
    CHECK(test.find("sub", 40) == -1);

    // first and last around first sub
    CHECK(test.find("sub", 4, 12) == 8);
    CHECK(test.find("sub", 4, 11) == 8);
    CHECK(test.find("sub", 7, 11) == 8);
    CHECK(test.find("sub", 8, 11) == 8);

    // first before but last not including first sub
    CHECK(test.find("sub", 4, 9) == -1);
    CHECK(test.find("sub", 8, 10) == -1);
    CHECK(test.find("sub", 8, 9) == -1);

    // first after and last after
    CHECK(test.find("sub", 9, 11) == -1);

    // empty range
    CHECK(test.find("sub", 9, 9) == -1);
    CHECK(test.find("sub", 8, 8) == -1);

    // invalid range
    CHECK(test.find("sub", 10, 9) == -1);

    CHECK(test.find("find!") == 34);
    CHECK(test.find("find!", 30, 39) == 34);
    CHECK(test.find("find!", 30, 38) == -1);
    CHECK(test.find("find!", -1) == -1);
    CHECK(test.find("find!", -1, 100) == -1);

    CHECK(test.find('s') == 2);
    CHECK(test.find('s', 2) == 2);
    CHECK(test.find('s', 5) == 8);
    CHECK(test.find('s', 2, 3) == 2);
    CHECK(test.find('s', 2, 2) == -1);
    CHECK(test.find('s', 3, 2) == -1);

    CHECK(test.find('!') == 38);
    CHECK(test.find('!', 38) == 38);
    CHECK(test.find('!', 38) == 38);
    CHECK(test.find('!', 38, 39) == 38);
    CHECK(test.find('!', 38, 38) == -1);
    CHECK(test.find('!', 39, 38) == -1);
    CHECK(test.find('!', -1) == -1);
    CHECK(test.find('!', -1, 100) == -1);

    CHECK(test.find_first_of("sx!") == 2);
    CHECK(test.find_first_of("sx!", 2) == 2);
    CHECK(test.find_first_of("sx!", 5) == 8);
    CHECK(test.find_first_of("sx!", 2, 3) == 2);
    CHECK(test.find_first_of("sx!", 2, 2) == -1);
    CHECK(test.find_first_of("sx!", 3, 2) == -1);
    CHECK(test.find_first_of("sx!", -1) == -1);
    CHECK(test.find_first_of("sx!", -1, 100) == -1);

    CHECK(test.find_first_not_of("Teot") == 2);
    CHECK(test.find_first_not_of("Teot", 2) == 2);
    CHECK(test.find_first_not_of("Teot", 5) == 6);
    CHECK(test.find_first_not_of("Teot", 2, 3) == 2);
    CHECK(test.find_first_not_of("Teot", 2, 2) == -1);
    CHECK(test.find_first_not_of("Teot", 3, 2) == -1);
    CHECK(test.find_first_not_of("Teot", -1) == -1);
    CHECK(test.find_first_not_of("Teot", -1, 100) == -1);

    CHECK(test.find_last_of("pur") == 31);
    CHECK(test.find_last_of("pur", 30) == 31);
    CHECK(test.find_last_of("pur", 0, 30) == 13);
    CHECK(test.find_last_of("pur", 0, 31) == 13);
    CHECK(test.find_last_of("pur", 0, 32) == 31);
    CHECK(test.find_last_of("pur", 5) == 31);
    CHECK(test.find_last_of("pur", 0, 5) == -1);
    CHECK(test.find_last_of("pur", 10, 15) == 13);
    CHECK(test.find_last_of("pur", 13, 15) == 13);
    CHECK(test.find_last_of("pur", 14, 15) == -1);
    CHECK(test.find_last_of("pur", 10, 13) == -1);
    CHECK(test.find_last_of("pur", 13, 13) == -1);
    CHECK(test.find_last_of("pur", 15, 13) == -1);
    CHECK(test.find_last_of("pur", -1) == -1);
    CHECK(test.find_last_of("pur", -1, 100) == -1);

    CHECK(test.find_last_not_of("ibe!fonudsc") == 33);
    CHECK(test.find_last_not_of("ibe!fonudsc", 20) == 33);
    CHECK(test.find_last_not_of("ibe!fonudsc", 20, 35) == 33);
    CHECK(test.find_last_not_of("ibe!fonudsc", 20, 33) == 29);
    CHECK(test.find_last_not_of("ibe!fonudsc", 29, 33) == 29);
    CHECK(test.find_last_not_of("ibe!fonudsc", 29, 30) == 29);
    CHECK(test.find_last_not_of("ibe!fonudsc", 29, 29) == -1);
    CHECK(test.find_last_not_of("ibe!fonudsc", 30, 29) == -1);
    CHECK(test.find_last_not_of("ibe!fonudsc", -1) == -1);
    CHECK(test.find_last_not_of("ibe!fonudsc", -1, 100) == -1);
  };

  SECTION("Comparisons")
  {
    rdcstr a = "Hello, World!";
    rdcstr b = "Hello, World!";

    CHECK_FALSE(a < b);
    CHECK(a == b);
    CHECK_FALSE(a > b);

    b.back() = '?';

    CHECK(a < b);
    CHECK_FALSE(a == b);
    CHECK_FALSE(a > b);

    CHECK_FALSE(b < a);
    CHECK_FALSE(b == a);
    CHECK(b > a);

    b[1] = 'a';

    a.pop_back();

    CHECK_FALSE(a < b);
    CHECK_FALSE(a == b);
    CHECK(a > b);

    b[1] = 'e';

    CHECK(a < b);
    CHECK_FALSE(a == b);
    CHECK_FALSE(a > b);
  }

  SECTION("String literal tests")
  {
    rdcstr test = STRING_LITERAL(LARGE_STRING);
    const size_t len = strlen(LARGE_STRING);

    CHECK(test.size() == len);
    CHECK(test.capacity() == test.size());
    CHECK(strlen(test.c_str()) == test.size());

    rdcstr test2;

    test2.resize(12345);
    test2 = test;

    CHECK(test2.size() == len);
    CHECK(test2.capacity() == test2.size());

    CHECK(test == test2);

    // should both be pointing directly to the string storage, so identical pointers
    CHECK(test.c_str() == test2.c_str());

    test2.reserve(1);

    // they will be equal still but not with the same storage now
    CHECK(test == test2);
    CHECK(test.c_str() != test2.c_str());

    test2[0] = '!';

    CHECK(test != test2);

    test = test2;

    // equal now but still not with the same storage
    CHECK(test == test2);
    CHECK(test.c_str() != test2.c_str());

    test = "short literal"_lit;
    test2 = test;

    // this does the copy-on-write but into the internal array
    test[0] = 'S';

    CHECK(test == "Short literal");
    CHECK(test.size() == test2.size());
  };

  SECTION("Inflexible string tests")
  {
    rdcinflexiblestr test = rdcstr("Hello, World");

    CHECK(test == "Hello, World");
    CHECK(test == "Hello, World"_lit);
    CHECK(test == rdcstr("Hello, World"));
    CHECK(test == rdcinflexiblestr("Hello, World"));
    CHECK_FALSE(test == "Hello, World!");
    CHECK_FALSE(test == "Hello, World!"_lit);
    CHECK_FALSE(test == rdcstr("Hello, World!"));
    CHECK_FALSE(test == rdcinflexiblestr("Hello, World!"));
    CHECK_FALSE(test.empty());

    rdcstr str = test;

    CHECK(str == "Hello, World");
    CHECK(str == "Hello, World"_lit);
    CHECK(str == rdcstr("Hello, World"));
    CHECK(str == rdcinflexiblestr("Hello, World"));
    CHECK_FALSE(str == "Hello, World!");
    CHECK_FALSE(str == "Hello, World!"_lit);
    CHECK_FALSE(str == rdcstr("Hello, World!"));
    CHECK_FALSE(str == rdcinflexiblestr("Hello, World!"));
    CHECK_FALSE(str.empty());

    test = "Hello, World"_lit;

    CHECK(test == "Hello, World");
    CHECK(test == "Hello, World"_lit);
    CHECK(test == rdcstr("Hello, World"));
    CHECK(test == rdcinflexiblestr("Hello, World"));
    CHECK_FALSE(test == "Hello, World!");
    CHECK_FALSE(test == "Hello, World!"_lit);
    CHECK_FALSE(test == rdcstr("Hello, World!"));
    CHECK_FALSE(test == rdcinflexiblestr("Hello, World!"));
    CHECK_FALSE(test.empty());

    str = test;

    CHECK(str == "Hello, World");
    CHECK(str == "Hello, World"_lit);
    CHECK(str == rdcstr("Hello, World"));
    CHECK(str == rdcinflexiblestr("Hello, World"));
    CHECK_FALSE(str == "Hello, World!");
    CHECK_FALSE(str == "Hello, World!"_lit);
    CHECK_FALSE(str == rdcstr("Hello, World!"));
    CHECK_FALSE(str == rdcinflexiblestr("Hello, World!"));
    CHECK_FALSE(str.empty());

    rdcinflexiblestr empty;

    CHECK(empty.empty());
    CHECK(empty == "");
    CHECK(empty == ""_lit);
    CHECK(empty == rdcstr(""));
    CHECK(empty == rdcinflexiblestr(""));
    CHECK_FALSE(empty == "Hello, World!");
    CHECK_FALSE(empty == "Hello, World!"_lit);
    CHECK_FALSE(empty == rdcstr("Hello, World!"));
    CHECK_FALSE(empty == rdcinflexiblestr("Hello, World!"));

    str = empty;

    CHECK(str.empty());
    CHECK(str == "");
    CHECK(str == ""_lit);
    CHECK(str == rdcstr(""));
    CHECK(str == rdcinflexiblestr(""));
    CHECK_FALSE(str == "Hello, World!");
    CHECK_FALSE(str == "Hello, World!"_lit);
    CHECK_FALSE(str == rdcstr("Hello, World!"));
    CHECK_FALSE(str == rdcinflexiblestr("Hello, World!"));
  };
};

TEST_CASE("Test flatmap type", "[basictypes][flatmap]")
{
  SECTION("basic lookup of values before and after sorting")
  {
    rdcflatmap<uint32_t, rdcstr, 16> test;

    test[5] = "foo";
    test[7] = "bar";
    test[3] = "asdf";

    CHECK(test[5] == "foo");
    CHECK(test[7] == "bar");
    CHECK(test[3] == "asdf");
    CHECK(!test.empty());
    CHECK(test.size() == 3);

    // order is not guaranteed, but multiplying the keys in any order will give us a unique value
    // because they're prime
    uint32_t product = 1;
    uint32_t count = 0;
    for(auto it = test.begin(); it != test.end(); ++it)
    {
      product *= it->first;
      count++;
    }

    CHECK(product == 3 * 5 * 7);
    CHECK(count == 3);

    // force the map to sort itself
    for(uint32_t i = 0; i < 24; i++)
      test[999 + i] = StringFormat::Fmt("test%u", 999 + i);

    // we should still be able to look up the same values
    CHECK(test[5] == "foo");
    CHECK(test[7] == "bar");
    CHECK(test[3] == "asdf");
    CHECK(!test.empty());
    CHECK(test.size() == 27);

    CHECK(test.find(5)->second == "foo");
    CHECK(test.find(6) == test.end());
    CHECK(test.find(7)->second == "bar");
    CHECK(test.find(8) == test.end());

    // check clearing
    test.clear();

    CHECK(test.empty());
    CHECK(test.size() == 0);

    // this inserts the values as default-initialised, as std::map does
    CHECK(test[5] == "");
    CHECK(test[7] == "");
    CHECK(test[3] == "");
    CHECK(test.size() == 3);
  };

  SECTION("swap")
  {
    rdcflatmap<uint32_t, rdcstr> test;

    test[5] = "foo";
    test[7] = "bar";
    test[3] = "asdf";

    rdcflatmap<uint32_t, rdcstr> swapped;

    test.swap(swapped);

    CHECK(swapped[5] == "foo");
    CHECK(swapped[7] == "bar");
    CHECK(swapped[3] == "asdf");
    CHECK(swapped.size() == 3);
    CHECK(test.empty());
  };

  SECTION("insert with no hint")
  {
    rdcflatmap<uint32_t, rdcstr> test;

    test[5] = "foo";
    test[7] = "bar";
    test[3] = "asdf";

    CHECK(test[5] == "foo");
    CHECK(test[7] == "bar");
    CHECK(test[3] == "asdf");
    CHECK(test.find(15) == test.end());

    test.insert({15, "inserted"});
    CHECK(test.find(15)->second == "inserted");
  };

  SECTION("insert with hint")
  {
    rdcflatmap<uint32_t, rdcstr> test;

    test[5] = "foo";
    test[7] = "bar";
    test[3] = "asdf";

    // insert value with proper hint
    test.insert(test.begin() + 1, {6, "middle"});

    CHECK(test.find(3)->second == "asdf");
    CHECK(test.find(5)->second == "foo");
    CHECK(test.find(6)->second == "middle");
    CHECK(test.find(7)->second == "bar");

    // insert value with wrong hint
    test.insert(test.begin(), {100, "highvalue"});
    CHECK(test.find(100)->second == "highvalue");

    // force the map to sort itself
    for(uint32_t i = 0; i < 24; i++)
      test[999 + i] = StringFormat::Fmt("test%u", 999 + i);

    test.insert(test.begin(), {101, "highvalue2"});

    CHECK(test.find(101)->second == "highvalue2");
  };

  SECTION("erase")
  {
    rdcflatmap<uint32_t, rdcstr> test;

    test[5] = "foo";
    test[7] = "bar";
    test[3] = "asdf";

    CHECK(test.find(5)->second == "foo");

    test.erase(5);

    CHECK(test.find(5) == test.end());

    test[5] = "foo";

    CHECK(test.find(5)->second == "foo");

    test.erase(3);
    test.erase(4);
    test.erase(6);
    test.erase(7);

    CHECK(test.find(5)->second == "foo");
  };

  SECTION("empty_map")
  {
    rdcflatmap<uint32_t, uint32_t> unsorted;
    CHECK(unsorted.begin() == unsorted.end());
    CHECK(unsorted.find(0) == unsorted.end());

    rdcflatmap<uint32_t, uint32_t, 0> sorted;
    CHECK(sorted.begin() == sorted.end());
    CHECK(sorted.find(0) == sorted.end());
  }
}

TEST_CASE("Test sorted flatmap type", "[basictypes][sortedflatmap]")
{
  SECTION("upper_bound")
  {
    rdcsortedflatmap<uint32_t, rdcstr> test;

    test[5] = "foo";
    test[7] = "bar";
    test[3] = "asdf";

    // check that they got sorted
    auto it = test.begin();
    CHECK(it->first == 3);
    CHECK(it->second == "asdf");
    ++it;
    CHECK(it->first == 5);
    CHECK(it->second == "foo");
    ++it;
    CHECK(it->first == 7);
    CHECK(it->second == "bar");

    it = test.upper_bound(2);
    CHECK(it->first == 3);
    CHECK(it->second == "asdf");

    it = test.upper_bound(3);
    CHECK(it->first == 5);
    CHECK(it->second == "foo");

    it = test.upper_bound(4);
    CHECK(it->first == 5);
    CHECK(it->second == "foo");

    it = test.upper_bound(5);
    CHECK(it->first == 7);
    CHECK(it->second == "bar");

    it = test.upper_bound(6);
    CHECK(it->first == 7);
    CHECK(it->second == "bar");

    it = test.upper_bound(7);
    CHECK(it == test.end());

    it = test.upper_bound(8);
    CHECK(it == test.end());
  };

  SECTION("empty_map")
  {
    rdcsortedflatmap<uint32_t, uint32_t> sortedflatmap;
    CHECK(sortedflatmap.lower_bound(1) == 0);
    CHECK(sortedflatmap.upper_bound(2) == 0);
  }
};

union foo
{
  rdcfixedarray<float, 16> f32v;
  rdcfixedarray<uint32_t, 16> u32v;
  rdcfixedarray<int32_t, 16> s32v;
};

TEST_CASE("Test rdcfixedarray type", "[basictypes][rdcfixedarray]")
{
  SECTION("Basic test")
  {
    rdcfixedarray<int32_t, 8> test = {};
    const rdcfixedarray<int32_t, 8> &constTest = test;

    CHECK(test.size() == 8);
    CHECK(test.byteSize() == 32);
    CHECK(test.begin() + 8 == test.end());
    CHECK(test[0] == 0);
    CHECK(test[2] == 0);

    test = {4, 1, 77, 0, 0, 8, 20, 934};

    CHECK(test.contains(1));
    CHECK(!test.contains(2));
    CHECK(test.indexOf(8) == 5);
    CHECK(test.indexOf(9) == -1);
    CHECK(test[0] == 4);
    CHECK(test[2] == 77);
    CHECK(test[4] == 0);
    CHECK(test.front() == 4);
    CHECK(test.back() == 934);
    CHECK(test.data()[0] == 4);
    CHECK(test.data()[2] == 77);
    CHECK(test.begin() != test.end());
    CHECK(test.begin() + 8 == test.end());

    CHECK(constTest.contains(1));
    CHECK(!constTest.contains(2));
    CHECK(constTest.indexOf(8) == 5);
    CHECK(constTest.indexOf(9) == -1);
    CHECK(constTest[0] == 4);
    CHECK(constTest[2] == 77);
    CHECK(constTest[4] == 0);
    CHECK(constTest.front() == 4);
    CHECK(constTest.back() == 934);
    CHECK(constTest.data()[0] == 4);
    CHECK(constTest.data()[2] == 77);
    CHECK(constTest.begin() != constTest.end());
    CHECK(constTest.begin() + 8 == constTest.end());

    int sum = 0;
    for(int x : test)
      sum += x;

    CHECK(sum == 1044);

    test[4] = 1;

    CHECK(test[0] == 4);
    CHECK(test[2] == 77);
    CHECK(test[4] == 1);

    sum = 0;
    for(int x : test)
      sum += x;

    CHECK(sum == 1045);

    rdcfixedarray<int32_t, 8> test2 = {1, 2, 3, 4};

    CHECK(test2[0] == 1);
    CHECK(test2[1] == 2);
    CHECK(test2[2] == 3);
    CHECK(test2[3] == 4);
    CHECK(test2[4] == 0);
    CHECK(test2[5] == 0);
    CHECK(test2[6] == 0);
    CHECK(test2[7] == 0);

    int32_t arr[8] = {5, 6, 7, 8};

    test2 = arr;

    CHECK(test2[0] == 5);
    CHECK(test2[1] == 6);
    CHECK(test2[2] == 7);
    CHECK(test2[3] == 8);
    CHECK(test2[4] == 0);
    CHECK(test2[5] == 0);
    CHECK(test2[6] == 0);
    CHECK(test2[7] == 0);

    CHECK(test < test2);
    CHECK_FALSE(test2 < test);
    CHECK_FALSE(test == test2);
    CHECK(test != test2);

    test2[0] = 3;

    CHECK(test2 < test);
    CHECK_FALSE(test < test2);
  };

  SECTION("Test of rdcfixedarray of ResourceId")
  {
    rdcfixedarray<ResourceId, 8> resources = {};

    ResourceId r = ResourceIDGen::GetNewUniqueID();
    resources[2] = r;
    resources[4] = ResourceIDGen::GetNewUniqueID();

    CHECK(resources[2] != resources[4]);
    CHECK(resources[2] == r);
  };

  SECTION("Test of rdcfixedarray in unions")
  {
    foo u = {};

    u.f32v[0] = 1.0f;

    CHECK(u.u32v[0] == 0x3f800000);

    u = foo();

    CHECK(u.u32v[0] == 0);
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
