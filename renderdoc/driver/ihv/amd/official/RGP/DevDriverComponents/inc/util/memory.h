/*
 *******************************************************************************
 *
 * Copyright (c) 2016-2018 Advanced Micro Devices, Inc. All rights reserved.
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
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/
/**
***********************************************************************************************************************
* @file  memory.h
* @brief GPUOpen common memory management definitions
***********************************************************************************************************************
*/

#pragma once

#include "template.h"
#include <new>

#define DD_CACHE_LINE_BYTES 64
#define DD_DEFAULT_ALIGNMENT alignof(void*)

#define DD_MALLOC(size, alignment, allocCb) allocCb.pfnAlloc(allocCb.pUserdata, size, Platform::Max(DD_DEFAULT_ALIGNMENT, alignment), false)
#define DD_CALLOC(size, alignment, allocCb) allocCb.pfnAlloc(allocCb.pUserdata, size, Platform::Max(DD_DEFAULT_ALIGNMENT, alignment), true)
#define DD_FREE(memory, allocCb) allocCb.pfnFree(allocCb.pUserdata, memory)

#define DD_NEW(className, allocCb) new(DD_MALLOC(sizeof(className), alignof(className), allocCb)) className
#define DD_DELETE(memory, allocCb) DevDriver::Platform::Destructor(memory); DD_FREE(memory, allocCb)

#define DD_NEW_ARRAY(className, numElements, allocCb) DevDriver::Platform::NewArray<className>(numElements, allocCb)
#define DD_DELETE_ARRAY(memory, allocCb) DevDriver::Platform::DeleteArray(memory, allocCb)

namespace DevDriver
{
    namespace Platform
    {

        template<typename T>
        inline void static Destructor(T* p)
        {
            if (p != nullptr)
            {
                p->~T();
            }
        }

        template<typename T>
        static T* NewArray(size_t numElements, const AllocCb& allocCb)
        {
            size_t allocSize = (sizeof(T) * numElements) + DD_CACHE_LINE_BYTES;
            size_t allocAlign = DD_CACHE_LINE_BYTES;

            T* pMem = reinterpret_cast<T*>(DD_MALLOC(allocSize, allocAlign, allocCb));
            if (pMem != nullptr)
            {
                pMem = reinterpret_cast<T*>(reinterpret_cast<char*>(pMem) + DD_CACHE_LINE_BYTES);
                size_t* pNumElements = reinterpret_cast<size_t*>(reinterpret_cast<char*>(pMem) - sizeof(size_t));
                *pNumElements = numElements;
                T* pCurrentElement = pMem;
                for (size_t elementIndex = 0; elementIndex < numElements; ++elementIndex)
                {
                    new(pCurrentElement) T;
                    ++pCurrentElement;
                }
            }

            return pMem;
        }

        template<typename T>
        static void DeleteArray(T* pElements, const AllocCb& allocCb)
        {
            if (pElements != nullptr)
            {
                size_t numElements = *reinterpret_cast<size_t*>(reinterpret_cast<char*>(pElements) - sizeof(size_t));
                T* pCurrentElement = pElements;
                for (size_t elementIndex = 0; elementIndex < numElements; ++elementIndex)
                {
                    pCurrentElement->~T();
                    ++pCurrentElement;
                }

                pElements = reinterpret_cast<T*>(reinterpret_cast<char*>(pElements) - DD_CACHE_LINE_BYTES);
            }

            DD_FREE(pElements, allocCb);
        }
    }
} // DevDriver
