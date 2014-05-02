/******************************************************************************
 * The MIT License (MIT)
 * 
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

#include <algorithm>
#include <string>
#include <vector>
#include <stdint.h>
#include <ctype.h>
using std::basic_string;
using std::vector;

// in common.cpp
template<class charType> const charType* pathSeparator();
template<class charType> const charType* curdir();

template<class strType> strType basename(const strType &path)
{
	strType base = path;

	if(base.length() == 0)
		return base;

	if(base[base.length()-1] == '/' || base[base.length()-1] == '\\')
		base.erase(base.size()-1);

	size_t offset = base.find_last_of(pathSeparator<typename strType::value_type>());

	if(offset == strType::npos)
		return base;

	return base.substr(offset+1);
}

template<class strType> strType dirname(const strType &path)
{
	strType base = path;

	if(base.length() == 0)
		return base;

	if(base[base.length()-1] == '/' || base[base.length()-1] == '\\')
		base.erase(base.size()-1);
	
	size_t offset = base.find_last_of(pathSeparator<typename strType::value_type>());

	if(offset == strType::npos)
		return curdir<typename strType::value_type>();

	return base.substr(0, offset);
}

template <class CharType>
void strreplace(basic_string<CharType>& str, const basic_string<CharType>& toFind, const basic_string<CharType>& replacement, typename basic_string<CharType>::size_type index = 0)
{
	typename basic_string<CharType>::size_type length = toFind.length();

	while(index < str.length())
	{
		index = str.find(toFind, index);

		if(index < str.length())
		{
			str.replace(index, length, replacement);
		}
	}
}

template <typename CharType>
basic_string<CharType> strlower(const basic_string<CharType>& str)
{
	basic_string<CharType> newstr(str);
	transform(newstr.begin(), newstr.end(), newstr.begin(), tolower);
	return newstr;
}

template <typename CharType>
basic_string<CharType> strupper(const basic_string<CharType>& str)
{
	basic_string<CharType> newstr(str);
	transform(newstr.begin(), newstr.end(), newstr.begin(), (int(*)(int))toupper);
	return newstr;
}

template <class CharType>
void split(const basic_string<CharType>& in, vector<basic_string<CharType> >& out, const CharType sep)
{
	basic_string<CharType> work = in;
	typename basic_string<CharType>::size_type offset = work.find(sep);

	while(offset != basic_string<CharType>::npos)
	{
		out.push_back(work.substr(0, offset));
		work = work.substr(offset+1);

		offset = work.find(sep);
	}

	if(work.size() && work[0] != 0)
		out.push_back(work);
}

template <class CharType>
void merge(const vector<basic_string<CharType> >& in, basic_string<CharType>& out, const CharType sep)
{
	out = basic_string<CharType>();
	for(size_t i=0; i < in.size(); i++)
	{
		out += in[i];
		out += sep;
	}
}

std::wstring widen(std::string str);
std::string narrow(std::wstring str);
