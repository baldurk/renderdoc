/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017-2019 Baldur Karlsson
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
#include "common/timing.h"
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

    // insert a large amount of data to ensure this doesn't read off start/end of vector
    std::vector<int> largedata;
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

    // 5 copies and 5 destructs to shift the array contents up, then another copy for inserting tmp
    CHECK(constructor == 6);
    CHECK(valueConstructor == 0);
    CHECK(copyConstructor == 5 + 1);
    CHECK(destructor == 5);

    CHECK(test[0].value == 999);
    CHECK(test[1].value == 10);

    // this should copy the value, then do an insert
    test.insert(0, test[0]);

    CHECK(test.capacity() == 100);
    CHECK(test.size() == 7);

    // on top of the above, another 6 copies & destructs to shift the array contents, 1 copy for
    // inserting test[0], and a copy&destruct of the temporary copy
    CHECK(constructor == 6);
    CHECK(valueConstructor == 0);
    CHECK(copyConstructor == (5 + 1) + 6 + 1 + 1);
    CHECK(destructor == (5) + 6 + 1);

    CHECK(test[0].value == 999);
    CHECK(test[1].value == 999);
    CHECK(test[2].value == 10);

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

    // on top of the above:
    // - 7 copies and destructs for the duplication (copies into the new storage, destructs from the
    // old storage)
    // - 7 copies and destructs for shifting the array contents
    // - 3 copies for the inserted items
    CHECK(constructor == 6);
    CHECK(valueConstructor == 0);
    CHECK(copyConstructor == (5 + 1 + 6 + 1 + 1) + 7 + 7 + 3);
    CHECK(destructor == (5 + 6 + 1) + 7 + 7);
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
    CHECK(test == std::string());
  };

  SECTION("Empty string after containing data")
  {
    rdcstr test;

    auto lambda = [](rdcstr test, const char *str) {
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
      CHECK(test != ((const char *)NULL));
      CHECK(test == str);
      CHECK(test == std::string(str));
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
      CHECK(test != str);
      CHECK(test != std::string(str));
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

    test.erase(0);

    CHECK(test == "ello, World! This is a test string");

    test.erase(0, 4);

    CHECK(test == ", World! This is a test string");

    test.erase(9, 5);

    CHECK(test == ", World! is a test string");

    test.erase(14, 1000);

    CHECK(test == ", World! is a ");

    test.erase(100);

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

    CHECK(test.indexOf('H') == 0);
    CHECK(test.indexOf('l') == 2);
    CHECK(test.indexOf('?') == -1);

    CHECK(test.contains('!'));
    CHECK_FALSE(test.contains('?'));

    CHECK(test.contains('H'));
    CHECK(test.contains("Hello"));

    char H = test.takeAt(0);

    CHECK(H == 'H');
    CHECK_FALSE(test.contains('H'));
    CHECK_FALSE(test.contains("Hello"));

    test.removeOne('!');

    CHECK_FALSE(test.contains('!'));

    CHECK(test == "ello, World");
  };

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
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)