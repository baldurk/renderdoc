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

//
// Implement types for tracking GLSL arrays, arrays of arrays, etc.
//

#ifndef _ARRAYS_INCLUDED
#define _ARRAYS_INCLUDED

namespace glslang {

//
// TSmallArrayVector is used as the container for the set of sizes in TArraySizes.
// It has generic-container semantics, while TArraySizes has array-of-array semantics.
//
struct TSmallArrayVector {
    //
    // TODO: memory: TSmallArrayVector is intended to be smaller.
    // Almost all arrays could be handled by two sizes each fitting
    // in 16 bits, needing a real vector only in the cases where there
    // are more than 3 sizes or a size needing more than 16 bits.
    //
    POOL_ALLOCATOR_NEW_DELETE(GetThreadPoolAllocator())

    TSmallArrayVector() : sizes(0) { }
    virtual ~TSmallArrayVector() { dealloc(); }

    // For breaking into two non-shared copies, independently modifiable.
    TSmallArrayVector& operator=(const TSmallArrayVector& from)
    {
        if (from.sizes == nullptr)
            sizes = nullptr;
        else {
            alloc();
            *sizes = *from.sizes;
        }

        return *this;
    }

    int size()
    {
        if (sizes == nullptr)
            return 0;
        return (int)sizes->size();
    }

    unsigned int front()
    {
        assert(sizes != nullptr && sizes->size() > 0);
        return sizes->front();
    }

    void changeFront(unsigned int s)
    {
        assert(sizes != nullptr);
        sizes->front() = s;
    }

    void push_back(unsigned int e)
    {
        alloc();
        sizes->push_back(e);
    }

    unsigned int operator[](int i)
    {
        assert(sizes && (int)sizes->size() > i);
        return (*sizes)[i];
    }

    bool operator==(const TSmallArrayVector& rhs)
    {
        if (sizes == nullptr && rhs.sizes == nullptr)
            return true;
        if (sizes == nullptr || rhs.sizes == nullptr)
            return false;
        return *sizes == *rhs.sizes;
    }

protected:
    TSmallArrayVector(const TSmallArrayVector&);

    void alloc()
    {
        if (sizes == nullptr)
            sizes = new TVector<unsigned int>;
    }
    void dealloc()
    {
        delete sizes;
    }

    TVector<unsigned int>* sizes; // will either hold such a pointer, or in the future, hold the two array sizes
};

//
// Represent an array, or array of arrays, to arbitrary depth.  This is not
// done through a hierarchy of types, but localized into this single cumulative object.
//
// The arrayness in TTtype is a pointer, so that it can be non-allocated and zero
// for the vast majority of types that are non-array types.
//
struct TArraySizes {
    POOL_ALLOCATOR_NEW_DELETE(GetThreadPoolAllocator())

    TArraySizes() : implicitArraySize(1) { }

    // For breaking into two non-shared copies, independently modifiable.
    TArraySizes& operator=(const TArraySizes& from)
    {
        implicitArraySize = from.implicitArraySize;
        sizes = from.sizes;

        return *this;
    }

    // translate from array-of-array semantics to container semantics
    int getNumDims() { return sizes.size(); }
    int getOuterSize() { return sizes.front(); }
    void setOuterSize(int s) { sizes.push_back((unsigned)s); }
    void changeOuterSize(int s) { sizes.changeFront((unsigned)s); }
    int getImplicitSize() { return (int)implicitArraySize; }
    void setImplicitSize(int s) { implicitArraySize = s; }
    int operator[](int i) { return sizes[i]; }
    bool operator==(const TArraySizes& rhs) { return sizes == rhs.sizes; }

protected:
    TSmallArrayVector sizes;

    TArraySizes(const TArraySizes&);

    // for tracking maximum referenced index, before an explicit size is given
    // applies only to the outer-most dimension
    int implicitArraySize;
};

} // end namespace glslang

#endif // _ARRAYS_INCLUDED_
