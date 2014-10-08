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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <vector>
#include <string>

// we provide a basic templated type that is a fixed array that just contains a pointer to the element
// array and a size. This could easily map to C as just void* and size but in C++ at least we can be
// type safe.
//
// While we can use STL elsewhere in the main library, we want to expose data to the UI layer as fixed
// arrays so that it's a plain C compatible interface.
namespace rdctype
{

template<typename A, typename B>
struct pair
{
	A first;
	B second;
};

template<typename T>
struct array
{
	T *elems;
	int32_t count;

	array() : elems(0), count(0) {}
	~array() { Delete(); }
	void Delete() { deallocate(elems); elems = 0; count = 0; }

	static void deallocate(const void *p) { free((void *)p); }
	static void *allocate(size_t s) { return malloc(s); }

	T &operator [](size_t i) { return elems[i]; }
	const T &operator [](size_t i) const { return elems[i]; }
	
	array(const T *const in) { elems = 0; count = 0; *this = in; }
	array &operator =(const T *const in);
	
	array(const std::vector<T> &in) { elems = 0; count = 0; *this = in; }
	array &operator =(const std::vector<T> &in)
	{
		Delete();
		count = (int32_t)in.size();
		if(count == 0)
		{
			elems = 0;
		}
		else
		{
			elems = (T*)allocate(sizeof(T)*count);
			for(int32_t i=0; i < count; i++)
				new (elems+i) T(in[i]);
		}
		return *this;
	}
	
	array(const array &o)
	{ elems = 0; count = 0; *this = o; }

	array &operator =(const array &o)
	{
		// do nothing if we're self-assigning
		if(this == &o) return *this;

		Delete();
		count = o.count;
		if(count == 0)
		{
			elems = 0;
		}
		else
		{
			elems = (T*)allocate(sizeof(T)*o.count);
			for(int32_t i=0; i < count; i++)
				new (elems+i) T(o.elems[i]);
		}
		return *this;
	}
};

struct str : public rdctype::array<char>
{
	str &operator =(const std::string &in);
	str &operator =(const char *const in);

	str() : rdctype::array<char>() {}
	str(const str &o) { elems = 0; count = 0; *this = o; }
	str &operator =(const str &o)
	{
		// do nothing if we're self-assigning
		if(this == &o) return *this;

		Delete();
		count = o.count;
		if(count == 0)
		{
			elems = (char*)allocate(sizeof(char));
			elems[0] = 0;
		}
		else
		{
			elems = (char*)allocate(sizeof(char)*(o.count+1));
			memcpy(elems, o.elems, sizeof(char)*o.count);
			elems[count] = 0;
		}

		return *this;
	}
};

struct wstr : public rdctype::array<wchar_t>
{
	wstr &operator =(const std::wstring &in);
	wstr &operator =(const wchar_t *const in);
	
	wstr() : rdctype::array<wchar_t>() {}
	wstr(const wstr &o) { elems = 0; count = 0; *this = o; }
	wstr &operator =(const wstr &o)
	{
		// do nothing if we're self-assigning
		if(this == &o) return *this;

		Delete();
		count = o.count;
		if(count == 0)
		{
			elems = (wchar_t*)allocate(sizeof(wchar_t));
			elems[0] = 0;
		}
		else
		{
			elems = (wchar_t*)allocate(sizeof(wchar_t)*(o.count+1));
			memcpy(elems, o.elems, sizeof(wchar_t)*o.count);
			elems[count] = 0;
		}

		return *this;
	}
};

}; // namespace rdctype
