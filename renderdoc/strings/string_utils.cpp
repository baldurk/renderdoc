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

#include "string_utils.h"
#include <ctype.h>
#include <stdint.h>
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

rdcstr strlower(const rdcstr &str)
{
  rdcstr newstr(str);
  for(size_t i = 0; i < newstr.size(); i++)
    newstr[i] = (char)tolower(newstr[i]);
  return newstr;
}

rdcstr strupper(const rdcstr &str)
{
  rdcstr newstr(str);
  for(size_t i = 0; i < newstr.size(); i++)
    newstr[i] = (char)toupper(newstr[i]);
  return newstr;
}

static bool ispathsep(char c)
{
  return c == '\\' || c == '/';
}

static int get_lastpathsep(const rdcstr &path)
{
  if(path.empty())
    return -1;

  size_t offs = path.size() - 1;

  while(offs > 0 && !ispathsep(path[offs]))
    offs--;

  if(offs == 0 && !ispathsep(path[0]))
    return -1;

  return (int)offs;
}

rdcstr get_basename(const rdcstr &path)
{
  rdcstr base = path;

  while(!base.empty() && ispathsep(base.back()))
    base.pop_back();

  if(base.empty())
    return base;

  int offset = get_lastpathsep(base);

  if(offset == -1)
    return base;

  return base.substr(offset + 1);
}

rdcstr get_dirname(const rdcstr &path)
{
  rdcstr base = path;

  while(!base.empty() && ispathsep(base.back()))
    base.pop_back();

  if(base.empty())
    return ".";

  int offset = get_lastpathsep(base);

  if(offset == -1)
  {
    base.resize(1);
    base[0] = '.';
    return base;
  }

  return base.substr(0, offset);
}

rdcstr strip_extension(const rdcstr &path)
{
  if(path.empty())
    return path;

  size_t offs = path.size() - 1;

  while(offs > 0 && path[offs] != '.')
    offs--;

  if(offs == 0 && path[offs] != '.')
    return path;

  return path.substr(0, offs);
}

void strip_nonbasic(rdcstr &str)
{
  for(char &c : str)
  {
    if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
       c == ' ')
      continue;

    c = '_';
  }
}

void split(const rdcstr &in, rdcarray<rdcstr> &out, const char sep)
{
  if(in.empty())
    return;

  {
    size_t numSeps = 0;

    int offset = in.find(sep);
    while(offset >= 0)
    {
      numSeps++;
      offset = in.find(sep, offset + 1);
    }

    out.reserve(numSeps + 1);
    out.clear();
  }

  int32_t begin = 0;
  int32_t end = in.find(sep);

  while(end >= 0)
  {
    out.push_back(in.substr(begin, end - begin));

    begin = end + 1;
    end = in.find(sep, begin);
  }

  if(begin < in.count() || (begin == in.count() && in.back() == sep))
    out.push_back(in.substr(begin));
}

void merge(const rdcarray<rdcstr> &in, rdcstr &out, const char sep)
{
  out = rdcstr();
  for(size_t i = 0; i < in.size(); i++)
  {
    out += in[i];
    if(i + 1 < in.size())
      out += sep;
  }
}

#if ENABLED(ENABLE_UNIT_TESTS)
#include "catch/catch.hpp"

TEST_CASE("String hashing", "[string]")
{
  SECTION("Same value returns the same hash")
  {
    CHECK(strhash("foobar") == strhash("foobar"));
    CHECK(strhash("blah") == strhash("blah"));
    CHECK(strhash("test of a long string for strhash") ==
          strhash("test of a long string for strhash"));
  };

  SECTION("hash of NULL or empty string returns the seed")
  {
    CHECK(strhash(NULL, 5) == 5);
    CHECK(strhash(NULL, 50) == 50);
    CHECK(strhash(NULL, 500) == 500);
    CHECK(strhash(NULL, 5000) == 5000);

    CHECK(strhash("", 5) == 5);
    CHECK(strhash("", 50) == 50);
    CHECK(strhash("", 500) == 500);
    CHECK(strhash("", 5000) == 5000);

    CHECK(strhash("0", 5) != 5);
    CHECK(strhash("0", 50) != 50);
    CHECK(strhash("0", 500) != 500);
    CHECK(strhash("0", 5000) != 5000);
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

  SECTION("get_lastpathsep")
  {
    CHECK(get_lastpathsep("") == -1);
    CHECK(get_lastpathsep("foo") == -1);
    CHECK(get_lastpathsep("foobar.blah") == -1);
    CHECK(get_lastpathsep("/foo") == 0);
    CHECK(get_lastpathsep("/foobar.blah") == 0);
    CHECK(get_lastpathsep("foo/bar/blah/") == 12);
    CHECK(get_lastpathsep("foo\\bar\\blah\\") == 12);
    CHECK(get_lastpathsep("foo/bar/blah") == 7);
    CHECK(get_lastpathsep("foo\\bar\\blah") == 7);
    CHECK(get_lastpathsep("/foo/bar/blah/") == 13);
    CHECK(get_lastpathsep("\\foo\\bar\\blah\\") == 13);
    CHECK(get_lastpathsep("/foo/bar/blah") == 8);
    CHECK(get_lastpathsep("\\foo\\bar\\blah") == 8);
  };

  SECTION("basename")
  {
    CHECK(get_basename("") == "");
    CHECK(get_basename("/") == "");
    CHECK(get_basename("/\\//\\") == "");
    CHECK(get_basename("foo") == "foo");
    CHECK(get_basename("foo/") == "foo");
    CHECK(get_basename("foo//") == "foo");
    CHECK(get_basename("foo/\\//\\") == "foo");
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
    CHECK(get_dirname("") == ".");
    CHECK(get_dirname("/") == ".");
    CHECK(get_dirname("/\\//\\") == ".");
    CHECK(get_dirname("foo") == ".");
    CHECK(get_dirname("foo/") == ".");
    CHECK(get_dirname("foo//") == ".");
    CHECK(get_dirname("foo/\\//\\") == ".");
    CHECK(get_dirname("/foo") == "");
    CHECK(get_dirname("/foo/") == "");
    CHECK(get_dirname("/foo//") == "");
    CHECK(get_dirname("/foo/\\//\\") == "");
    CHECK(get_dirname("/dir/foo") == "/dir");
    CHECK(get_dirname("/long/path/dir/foo") == "/long/path/dir");
    CHECK(get_dirname("relative/long/path/dir/foo") == "relative/long/path/dir");
    CHECK(get_dirname("../foo") == "..");
    CHECK(get_dirname("relative/../foo") == "relative/..");
    CHECK(get_dirname("C:/windows/foo") == "C:/windows");
    CHECK(get_dirname("C:\\windows\\foo") == "C:\\windows");
    CHECK(get_dirname("C:\\windows\\path/mixed/slashes\\foo") == "C:\\windows\\path/mixed/slashes");
  };

  SECTION("strip_extension")
  {
    CHECK(strip_extension("foo.exe") == "foo");
    CHECK(strip_extension("foo.exe.zip") == "foo.exe");
    CHECK(strip_extension("foo..exe") == "foo.");
    CHECK(strip_extension("foo") == "foo");
    CHECK(strip_extension("") == "");
    CHECK(strip_extension(".exe") == "");
    CHECK(strip_extension(".config.txt") == ".config");
    CHECK(strip_extension("bar/foo.exe") == "bar/foo");
  };

  SECTION("strupper")
  {
    CHECK(strupper("foobar") == "FOOBAR");
    CHECK(strupper("Foobar") == "FOOBAR");
    CHECK(strupper("FOOBAR") == "FOOBAR");
  };

  SECTION("split by comma")
  {
    rdcarray<rdcstr> vec;

    split(rdcstr("foo,bar, blah,test"), vec, ',');

    REQUIRE(vec.size() == 4);
    CHECK(vec[0] == "foo");
    CHECK(vec[1] == "bar");
    CHECK(vec[2] == " blah");
    CHECK(vec[3] == "test");
  };

  SECTION("split by space")
  {
    rdcarray<rdcstr> vec;

    split(rdcstr("this is a test string for   splitting!"), vec, ' ');

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

    split(rdcstr("new test"), vec, ' ');

    CHECK(vec.size() == 2);
  };

  SECTION("split with trailing separator")
  {
    rdcarray<rdcstr> vec;

    split(rdcstr("foo,,bar, blah,,,test,"), vec, ',');

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
    rdcarray<rdcstr> vec;

    split(rdcstr(",foo,bar"), vec, ',');

    REQUIRE(vec.size() == 3);
    CHECK(vec[0] == "");
    CHECK(vec[1] == "foo");
    CHECK(vec[2] == "bar");
  };

  SECTION("merge")
  {
    rdcarray<rdcstr> vec;
    rdcstr str;

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
    rdcarray<rdcstr> vec;
    rdcstr str;

    split(rdcstr(), vec, ',');

    REQUIRE(vec.empty());

    merge(vec, str, ',');

    REQUIRE(str == "");
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
