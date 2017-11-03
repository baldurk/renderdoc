/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
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

#include "api/replay/renderdoc_replay.h"
#include "common/globalconfig.h"
#include "os/os_specific.h"

#if ENABLED(ENABLE_UNIT_TESTS)

#include "3rdparty/catch/catch.hpp"

static volatile int32_t constructor = 0;
static volatile int32_t valueConstructor = 0;
static volatile int32_t copyConstructor = 0;
static volatile int32_t destructor = 0;

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
  ~ConstructorCounter() { Atomic::Inc32(&destructor); }
};

TEST_CASE("Test array type", "[basictypes]")
{
  SECTION("Basic test")
  {
    rdcarray<int> test;

    CHECK(test.size() == 0);
    CHECK(test.capacity() == 0);
    CHECK(test.empty());
    CHECK(test.isEmpty());
    CHECK(test.begin() == test.end());

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

    CHECK(test.size() == 2);
    CHECK(test.capacity() >= 2);
    CHECK_FALSE(test.empty());
    CHECK_FALSE(test.isEmpty());
    CHECK(test.begin() + 2 == test.end());

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

    sum = 0;
    for(int x : test)
      sum += x;

    CHECK(sum == 0);

    test = {4, 1, 77, 0, 0, 8, 20, 934};

    CHECK(test.size() == 8);
    CHECK(test.capacity() >= 8);
    CHECK_FALSE(test.empty());
    CHECK_FALSE(test.isEmpty());
    CHECK(test.begin() + 8 == test.end());

    sum = 0;
    for(int x : test)
      sum += x;

    CHECK(sum == 1044);

    CHECK(test[2] == 77);

    test[2] = 10;

    CHECK(test[2] == 10);

    test.reserve(100);

    CHECK(test.size() == 8);
    CHECK(test.capacity() >= 100);
    CHECK_FALSE(test.empty());
    CHECK_FALSE(test.isEmpty());
    CHECK(test.begin() + 8 == test.end());
  };

  SECTION("Test constructing/assigning from other types")
  {
    rdcarray<int> test;

    SECTION("std::vector")
    {
      std::vector<int> vec = {2, 3, 4, 5};

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

  SECTION("Verify insert()")
  {
    rdcarray<int> vec = {6, 3, 13, 5};

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

  SECTION("Verify erase()")
  {
    rdcarray<int> vec = {6, 3, 13, 5};

    vec.erase(2);

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
  };

  SECTION("Check construction")
  {
    rdcarray<ConstructorCounter> test;

    CHECK(constructor == 0);
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

    test.push_back(ConstructorCounter(10));

    CHECK(test[0].value == 10);

    // for the temporary going into push_back
    CHECK(valueConstructor == 1);
    // for the temporary going out of scope
    CHECK(destructor == 2);
    // for the temporary being copied into the new element
    CHECK(copyConstructor == 2);

    // previous value
    CHECK(constructor == 1);

    test.reserve(1000);

    // single element in test was moved to new backing storage
    CHECK(destructor == 3);
    CHECK(copyConstructor == 3);

    // previous values
    CHECK(valueConstructor == 1);
    CHECK(constructor == 1);

    test.resize(50);

    // 49 default initialisations
    CHECK(constructor == 50);

    // previous values
    CHECK(valueConstructor == 1);
    CHECK(destructor == 3);
    CHECK(copyConstructor == 3);

    test.clear();

    // 50 destructions
    CHECK(destructor == 53);

    // previous values
    CHECK(constructor == 50);
    CHECK(valueConstructor == 1);
    CHECK(copyConstructor == 3);
  };
};

#define CHECK_NULL_TERM(str) CHECK(str.c_str()[str.size()] == '\0');

TEST_CASE("Test string type", "[basictypes][string]")
{
  rdcstr test;

  // should not have any data in it
  CHECK(test.size() == 0);
  CHECK(test.capacity() == 0);
  CHECK(test.empty());
  CHECK(test.isEmpty());
  CHECK(test.begin() == test.end());

  CHECK(test.c_str() != NULL);
  CHECK_NULL_TERM(test);

  test = "Test string type";

  CHECK(test.size() == 16);
  CHECK(test.capacity() >= 16);
  CHECK_FALSE(test.empty());
  CHECK_FALSE(test.isEmpty());
  CHECK(test.begin() + 16 == test.end());

  CHECK(strlen(test.c_str()) == 16);
  CHECK(test.c_str() != NULL);
  CHECK(test == "Test string type");
  CHECK(test == std::string("Test string type"));
  CHECK(test == rdcstr("Test string type"));
  CHECK_NULL_TERM(test);

  test[4] = '!';

  CHECK(test.size() == 16);
  CHECK(test.capacity() >= 16);
  CHECK_FALSE(test.empty());
  CHECK_FALSE(test.isEmpty());
  CHECK(test.begin() + 16 == test.end());

  CHECK(strlen(test.c_str()) == 16);
  CHECK(test.c_str() != NULL);
  CHECK(test == "Test!string type");
  CHECK(test == std::string("Test!string type"));
  CHECK(test == rdcstr("Test!string type"));
  CHECK_NULL_TERM(test);

  test.clear();

  CHECK(test.size() == 0);
  CHECK(test.capacity() >= 16);
  CHECK(test.empty());
  CHECK(test.isEmpty());
  CHECK(test.begin() == test.end());
  CHECK_NULL_TERM(test);
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)