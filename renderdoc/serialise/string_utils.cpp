/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2017 Baldur Karlsson
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

#include "string_utils.h"
#include <ctype.h>
#include <stdint.h>
#include <wctype.h>
#include <algorithm>
#include "common/globalconfig.h"

uint32_t strhash(const char *str, uint32_t seed)
{
  if(str == NULL)
    return seed;

  uint32_t hash = seed;
  int c = *str;
  str++;

  while(c)
  {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    c = *str;
    str++;
  }

  return hash;
}

// since tolower is int -> int, this warns below. make a char -> char alternative
char toclower(char c)
{
  return (char)tolower(c);
}

char tocupper(char c)
{
  return (char)toupper(c);
}

string strlower(const string &str)
{
  string newstr(str);
  transform(newstr.begin(), newstr.end(), newstr.begin(), toclower);
  return newstr;
}

wstring strlower(const wstring &str)
{
  wstring newstr(str);
  transform(newstr.begin(), newstr.end(), newstr.begin(), towlower);
  return newstr;
}

string strupper(const string &str)
{
  string newstr(str);
  transform(newstr.begin(), newstr.end(), newstr.begin(), tocupper);
  return newstr;
}

wstring strupper(const wstring &str)
{
  wstring newstr(str);
  transform(newstr.begin(), newstr.end(), newstr.begin(), towupper);
  return newstr;
}

std::string trim(const std::string &str)
{
  const char *whitespace = "\t \n\r";
  size_t start = str.find_first_not_of(whitespace);
  size_t end = str.find_last_not_of(whitespace);

  // no non-whitespace characters, return the empty string
  if(start == std::string::npos)
    return "";

  // searching from the start found something, so searching from the end must have too.
  return str.substr(start, end - start + 1);
}

bool endswith(const std::string &value, const std::string &ending)
{
  if(ending.length() > value.length())
    return false;

  return (0 == value.compare(value.length() - ending.length(), ending.length(), ending));
}

std::string removeFromEnd(const std::string &value, const std::string &ending)
{
  string::size_type pos;
  pos = value.rfind(ending);

  // Create new string from beginning to pattern
  if(string::npos != pos)
    return value.substr(0, pos);

  // If pattern not found, just return original string
  return value;
}

#if ENABLED(ENABLE_UNIT_TESTS)
#include "3rdparty/catch/catch.hpp"

TEST_CASE("String hashing", "[string]")
{
  SECTION("Same value returns the same hash")
  {
    CHECK(strhash("foobar") == strhash("foobar"));
    CHECK(strhash("blah") == strhash("blah"));
    CHECK(strhash("test of a long string for strhash") ==
          strhash("test of a long string for strhash"));
  };

  SECTION("Different inputs have different hashes")
  {
    CHECK(strhash("foobar") != strhash("blah"));
    CHECK(strhash("test thing") != strhash("test test test"));
    CHECK(strhash("test1") != strhash("test2"));
    CHECK(strhash("test1") != strhash("test3"));
  };

  SECTION("Same input with different seeds have different hashes")
  {
    CHECK(strhash("foobar", 1) != strhash("foobar", 2));
    CHECK(strhash("foobar", 100) != strhash("foobar", 256));
    CHECK(strhash("foobar", 1024) != strhash("foobar", 2048));
  };

  SECTION("Incremental hashing")
  {
    int complete = strhash("test of a long string for strhash");

    int partial = strhash("test of");
    partial = strhash(" a long", partial);
    partial = strhash(" string", partial);
    partial = strhash(" for ", partial);
    partial = strhash("strhash", partial);

    CHECK(partial == complete);
  };
};

TEST_CASE("String manipulation", "[string]")
{
  SECTION("strlower")
  {
    CHECK(strlower("foobar") == "foobar");
    CHECK(strlower("Foobar") == "foobar");
    CHECK(strlower("FOOBAR") == "foobar");
  };

  SECTION("strupper")
  {
    CHECK(strupper("foobar") == "FOOBAR");
    CHECK(strupper("Foobar") == "FOOBAR");
    CHECK(strupper("FOOBAR") == "FOOBAR");
  };

  SECTION("trim")
  {
    CHECK(trim("  foo bar  ") == "foo bar");
    CHECK(trim("  Foo bar") == "Foo bar");
    CHECK(trim("  Foo\nbar") == "Foo\nbar");
    CHECK(trim("FOO BAR  ") == "FOO BAR");
    CHECK(trim("FOO BAR  \t\n") == "FOO BAR");
  };

  SECTION("endswith / removeFromEnd")
  {
    CHECK(endswith("foobar", "bar"));
    CHECK_FALSE(endswith("foobar", "foo"));
    CHECK(endswith("foobar", ""));

    CHECK(removeFromEnd("test/foobar", "") == "test/foobar");
    CHECK(removeFromEnd("test/foobar", "foo") == "test/");
    CHECK(removeFromEnd("test/foobar", "bar") == "test/foo");
  };

  SECTION("split by comma")
  {
    std::vector<string> vec;

    split(std::string("foo,bar, blah,test"), vec, ',');

    REQUIRE(vec.size() == 4);
    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "bar");
    CHECK(vec[2] == " blah");
    CHECK(vec[3] == "test");
  };

  SECTION("split by space")
  {
    std::vector<string> vec;

    split(std::string("this is a test string for   splitting! "), vec, ' ');

    REQUIRE(vec.size() == 9);
    CHECK(vec[0] == "this");
    CHECK(vec[1] == "is");
    CHECK(vec[2] == "a");
    CHECK(vec[3] == "test");
    CHECK(vec[4] == "string");
    CHECK(vec[5] == "for");
    CHECK(vec[6] == "");
    CHECK(vec[7] == "");
    CHECK(vec[8] == "splitting!");
  };

  SECTION("merge")
  {
    std::vector<string> vec;
    std::string str;

    merge(vec, str, ' ');
    CHECK(str == "");

    vec.push_back("Hello");

    merge(vec, str, ' ');
    CHECK(str == "Hello");

    vec.push_back("World");

    merge(vec, str, ' ');
    CHECK(str == "Hello World");
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
