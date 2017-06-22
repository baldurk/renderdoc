/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Baldur Karlsson
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

#include <stdint.h>
#include <string>
#include <vector>
using std::string;
using std::wstring;
using std::vector;

std::string strlower(const std::string &str);
std::wstring strlower(const std::wstring &str);
std::string strupper(const std::string &str);
std::wstring strupper(const std::wstring &str);

std::string trim(const std::string &str);

uint32_t strhash(const char *str, uint32_t existingHash = 5381);

bool endswith(const std::string &value, const std::string &ending);

template <class strType>
strType basename(const strType &path)
{
  strType base = path;

  if(base.length() == 0)
    return base;

  if(base[base.length() - 1] == '/' || base[base.length() - 1] == '\\')
    base.erase(base.size() - 1);

  typename strType::value_type pathSep[3] = {'\\', '/', 0};

  size_t offset = base.find_last_of(pathSep);

  if(offset == strType::npos)
    return base;

  return base.substr(offset + 1);
}

template <class strType>
strType dirname(const strType &path)
{
  strType base = path;

  if(base.length() == 0)
    return base;

  if(base[base.length() - 1] == '/' || base[base.length() - 1] == '\\')
    base.erase(base.size() - 1);

  typename strType::value_type pathSep[3] = {'\\', '/', 0};

  size_t offset = base.find_last_of(pathSep);

  if(offset == strType::npos)
  {
    base.resize(1);
    base[0] = typename strType::value_type('.');
    return base;
  }

  return base.substr(0, offset);
}

template <class CharType>
void split(const std::basic_string<CharType> &in, vector<std::basic_string<CharType> > &out,
           const CharType sep)
{
  std::basic_string<CharType> work = in;
  typename std::basic_string<CharType>::size_type offset = work.find(sep);

  while(offset != std::basic_string<CharType>::npos)
  {
    out.push_back(work.substr(0, offset));
    work = work.substr(offset + 1);

    offset = work.find(sep);
  }

  if(work.size() && work[0] != 0)
    out.push_back(work);
}

template <class CharType>
void merge(const vector<std::basic_string<CharType> > &in, std::basic_string<CharType> &out,
           const CharType sep)
{
  out = std::basic_string<CharType>();
  for(size_t i = 0; i < in.size(); i++)
  {
    out += in[i];
    out += sep;
  }
}
