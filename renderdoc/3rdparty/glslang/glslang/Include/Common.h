//
//Copyright (C) 2002-2005  3Dlabs Inc. Ltd.
//Copyright (C) 2012-2013 LunarG, Inc.
//
//All rights reserved.
//
//Redistribution and use in source and binary forms, with or without
//modification, are permitted provided that the following conditions
//are met:
//
//    Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
//    Redistributions in binary form must reproduce the above
//    copyright notice, this list of conditions and the following
//    disclaimer in the documentation and/or other materials provided
//    with the distribution.
//
//    Neither the name of 3Dlabs Inc. Ltd. nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
//THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
//FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
//COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
//BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
//CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
//LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
//ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//POSSIBILITY OF SUCH DAMAGE.
//

#ifndef _COMMON_INCLUDED_
#define _COMMON_INCLUDED_

#if defined _MSC_VER || defined MINGW_HAS_SECURE_API
    #include <basetsd.h>
    #define snprintf sprintf_s
    #define safe_vsprintf(buf,max,format,args) vsnprintf_s((buf), (max), (max), (format), (args))
#elif defined (solaris)
    #define safe_vsprintf(buf,max,format,args) vsnprintf((buf), (max), (format), (args))
    #include <sys/int_types.h>
    #define UINT_PTR uintptr_t
#else
    #define safe_vsprintf(buf,max,format,args) vsnprintf((buf), (max), (format), (args))
    #include <stdint.h>
    #define UINT_PTR uintptr_t
#endif

/* windows only pragma */
#ifdef _MSC_VER
    #pragma warning(disable : 4786) // Don't warn about too long identifiers
    #pragma warning(disable : 4514) // unused inline method
    #pragma warning(disable : 4201) // nameless union
#endif

#include <set>
#include <vector>
#include <map>
#include <list>
#include <algorithm>
#include <string>
#include <stdio.h>
#include <assert.h>

#include "PoolAlloc.h"

//
// Put POOL_ALLOCATOR_NEW_DELETE in base classes to make them use this scheme.
//
#define POOL_ALLOCATOR_NEW_DELETE(A)                                  \
    void* operator new(size_t s) { return (A).allocate(s); }          \
    void* operator new(size_t, void *_Where) { return (_Where);	}     \
    void operator delete(void*) { }                                   \
    void operator delete(void *, void *) { }                          \
    void* operator new[](size_t s) { return (A).allocate(s); }        \
    void* operator new[](size_t, void *_Where) { return (_Where);	} \
    void operator delete[](void*) { }                                 \
    void operator delete[](void *, void *) { }

#define TBaseMap std::map
#define TBaseList std::list
#define TBaseSet std::set

namespace glslang {

//
// Pool version of string.
//
typedef pool_allocator<char> TStringAllocator;
typedef std::basic_string <char, std::char_traits<char>, TStringAllocator> TString;
inline TString* NewPoolTString(const char* s)
{
	void* memory = GetThreadPoolAllocator().allocate(sizeof(TString));
	return new(memory) TString(s);
}

template<class T> inline T* NewPoolObject(T)
{
    return new(GetThreadPoolAllocator().allocate(sizeof(T))) T;
}

template<class T> inline T* NewPoolObject(T, int instances)
{
    return new(GetThreadPoolAllocator().allocate(instances * sizeof(T))) T[instances];
}

//
// Pool allocator versions of vectors, lists, and maps
//
template <class T> class TVector : public std::vector<T, pool_allocator<T> > {
public:
    POOL_ALLOCATOR_NEW_DELETE(GetThreadPoolAllocator())

    typedef typename std::vector<T, pool_allocator<T> >::size_type size_type;
    TVector() : std::vector<T, pool_allocator<T> >() {}
    TVector(const pool_allocator<T>& a) : std::vector<T, pool_allocator<T> >(a) {}
    TVector(size_type i) : std::vector<T, pool_allocator<T> >(i) {}
    TVector(size_type i, const T& val) : std::vector<T, pool_allocator<T> >(i, val) {}
};

template <class T> class TList   : public TBaseList  <T, pool_allocator<T> > {
public:
    typedef typename TBaseList<T, pool_allocator<T> >::size_type size_type;
    TList() : TBaseList<T, pool_allocator<T> >() {}
    TList(const pool_allocator<T>& a) : TBaseList<T, pool_allocator<T> >(a) {}
    TList(size_type i): TBaseList<T, pool_allocator<T> >(i) {}
};

// This is called TStlSet, because TSet is taken by an existing compiler class.
template <class T, class CMP> class TStlSet : public std::set<T, CMP, pool_allocator<T> > {
    // No pool allocator versions of constructors in std::set.
};


template <class K, class D, class CMP = std::less<K> > 
class TMap : public TBaseMap<K, D, CMP, pool_allocator<std::pair<K, D> > > {
public:
    typedef pool_allocator<std::pair <K, D> > tAllocator;

    TMap() : TBaseMap<K, D, CMP, tAllocator >() {}
    // use correct two-stage name lookup supported in gcc 3.4 and above
    TMap(const tAllocator& a) : TBaseMap<K, D, CMP, tAllocator>(TBaseMap<K, D, CMP, tAllocator >::key_compare(), a) {}
};

//
// Persistent string memory.  Should only be used for strings that survive
// across compiles/links.
//
typedef std::basic_string<char> TPersistString;

//
// templatized min and max functions.
//
template <class T> T Min(const T a, const T b) { return a < b ? a : b; }
template <class T> T Max(const T a, const T b) { return a > b ? a : b; }

//
// Create a TString object from an integer.
//
inline const TString String(const int i, const int base = 10)
{
    char text[16];     // 32 bit ints are at most 10 digits in base 10
    
    #if defined _MSC_VER || defined MINGW_HAS_SECURE_API
        _itoa_s(i, text, sizeof(text), base);
    #else
        // we assume base 10 for all cases
        snprintf(text, sizeof(text), "%d", i);
    #endif

    return text;
}

struct TSourceLoc {
    void init() { string = 0; line = 0; column = 0; }
    int string;
    int line;
    int column;
};

typedef TMap<TString, TString> TPragmaTable;
typedef TMap<TString, TString>::tAllocator TPragmaTableAllocator;

const int GlslangMaxTokenLength = 1024;

template <class T> bool IsPow2(T powerOf2)
{
    if (powerOf2 <= 0)
        return false;

    return (powerOf2 & (powerOf2 - 1)) == 0;
}

// Round number up to a multiple of the given powerOf2, which is not
// a power, just a number that must be a power of 2.
template <class T> void RoundToPow2(T& number, int powerOf2)
{
    assert(IsPow2(powerOf2));
    number = (number + powerOf2 - 1) & ~(powerOf2 - 1);
}

template <class T> bool IsMultipleOfPow2(T number, int powerOf2)
{
    assert(IsPow2(powerOf2));
    return ! (number & (powerOf2 - 1));
}

} // end namespace glslang

#endif // _COMMON_INCLUDED_
