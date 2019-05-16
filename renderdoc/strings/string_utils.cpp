/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2014-2019 Baldur Karlsson
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
#include "os/os_specific.h"

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

std::string strlower(const std::string &str)
{
  std::string newstr(str);
  std::transform(newstr.begin(), newstr.end(), newstr.begin(), toclower);
  return newstr;
}

std::string strupper(const std::string &str)
{
  std::string newstr(str);
  std::transform(newstr.begin(), newstr.end(), newstr.begin(), tocupper);
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

std::string get_basename(const std::string &path)
{
  std::string base = path;

  if(base.length() == 0)
    return base;

  if(base[base.length() - 1] == '/' || base[base.length() - 1] == '\\')
    base.erase(base.size() - 1);

  char pathSep[] = {'\\', '/', 0};

  size_t offset = base.find_last_of(pathSep);

  if(offset == std::string::npos)
    return base;

  return base.substr(offset + 1);
}

std::wstring get_basename(const std::wstring &path)
{
  return StringFormat::UTF82Wide(get_basename(StringFormat::Wide2UTF8(path)));
}

std::string get_dirname(const std::string &path)
{
  std::string base = path;

  if(base.length() == 0)
    return base;

  if(base[base.length() - 1] == '/' || base[base.length() - 1] == '\\')
    base.erase(base.size() - 1);

  char pathSep[3] = {'\\', '/', 0};

  size_t offset = base.find_last_of(pathSep);

  if(offset == std::string::npos)
  {
    base.resize(1);
    base[0] = '.';
    return base;
  }

  return base.substr(0, offset);
}

std::wstring get_dirname(const std::wstring &path)
{
  return StringFormat::UTF82Wide(get_dirname(StringFormat::Wide2UTF8(path)));
}

void split(const std::string &in, std::vector<std::string> &out, const char sep)
{
  if(in.empty())
    return;

  {
    size_t numSeps = 0;

    size_t offset = in.find(sep);
    while(offset != std::string::npos)
    {
      numSeps++;
      offset = in.find(sep, offset + 1);
    }

    out.reserve(numSeps + 1);
  }

  size_t begin = 0;
  size_t end = in.find(sep);

  while(end != std::string::npos)
  {
    out.push_back(in.substr(begin, end - begin));

    begin = end + 1;
    end = in.find(sep, begin);
  }

  if(begin < in.size() || (begin == in.size() && in.back() == sep))
    out.push_back(in.substr(begin));
}

void merge(const std::vector<std::string> &in, std::string &out, const char sep)
{
  out = std::string();
  for(size_t i = 0; i < in.size(); i++)
  {
    out += in[i];
    if(i + 1 < in.size())
      out += sep;
  }
}

std::string removeFromEnd(const std::string &value, const std::string &ending)
{
  size_t pos = value.rfind(ending);

  // Create new string from beginning to pattern
  if(std::string::npos != pos)
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

  SECTION("basename")
  {
    CHECK(get_basename("foo") == "foo");
    CHECK(get_basename("/foo") == "foo");
    CHECK(get_basename("/dir/foo") == "foo");
    CHECK(get_basename("/long/path/dir/foo") == "foo");
    CHECK(get_basename("relative/long/path/dir/foo") == "foo");
    CHECK(get_basename("../foo") == "foo");
    CHECK(get_basename("relative/../foo") == "foo");
    CHECK(get_basename("C:/windows/foo") == "foo");
    CHECK(get_basename("C:\\windows\\foo") == "foo");
    CHECK(get_basename("C:\\windows\\path/mixed/slashes\\foo") == "foo");
  };

  SECTION("dirname")
  {
    CHECK(get_dirname("foo") == ".");
    CHECK(get_dirname("/foo") == "");
    CHECK(get_dirname("/dir/foo") == "/dir");
    CHECK(get_dirname("/long/path/dir/foo") == "/long/path/dir");
    CHECK(get_dirname("relative/long/path/dir/foo") == "relative/long/path/dir");
    CHECK(get_dirname("../foo") == "..");
    CHECK(get_dirname("relative/../foo") == "relative/..");
    CHECK(get_dirname("C:/windows/foo") == "C:/windows");
    CHECK(get_dirname("C:\\windows\\foo") == "C:\\windows");
    CHECK(get_dirname("C:\\windows\\path/mixed/slashes\\foo") == "C:\\windows\\path/mixed/slashes");
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
    std::vector<std::string> vec;

    split(std::string("foo,bar, blah,test"), vec, ',');

    REQUIRE(vec.size() == 4);
    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "bar");
    CHECK(vec[2] == " blah");
    CHECK(vec[3] == "test");
  };

  SECTION("split by space")
  {
    std::vector<std::string> vec;

    split(std::string("this is a test string for   splitting!"), vec, ' ');

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

  SECTION("split with trailing separator")
  {
    std::vector<std::string> vec;

    split(std::string("foo,,bar, blah,,,test,"), vec, ',');

    REQUIRE(vec.size() == 8);
    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "");
    CHECK(vec[2] == "bar");
    CHECK(vec[3] == " blah");
    CHECK(vec[4] == "");
    CHECK(vec[5] == "");
    CHECK(vec[6] == "test");
    CHECK(vec[7] == "");
  };

  SECTION("split with starting separator")
  {
    std::vector<std::string> vec;

    split(std::string(",foo,bar"), vec, ',');

    REQUIRE(vec.size() == 3);
    CHECK(vec[0] == "");
    CHECK(vec[1] == "foo");
    CHECK(vec[2] == "bar");
  };

  SECTION("merge")
  {
    std::vector<std::string> vec;
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

  SECTION("degenerate cases")
  {
    std::vector<std::string> vec;
    std::string str;

    split(std::string(), vec, ',');

    REQUIRE(vec.empty());

    merge(vec, str, ',');

    REQUIRE(str == "");
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
