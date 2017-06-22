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
