/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2024 Baldur Karlsson
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

#include "metal_types.h"

#import <Metal/Metal.h>

// clang-format off
#define DECLARE_OBJC_WRAPPED_INTERFACES(CPPTYPE)                              \
  @interface ObjCBridgeMTL##CPPTYPE : NSObject<MTL##CPPTYPE>                  \
  @end                                                                        \
  inline WrappedMTL##CPPTYPE *GetWrapped(ObjCBridgeMTL##CPPTYPE *objCWrapped) \
  {                                                                           \
    return (WrappedMTL##CPPTYPE *)objCWrapped;                                \
  }
// clang-format on

METALCPP_WRAPPED_PROTOCOLS(DECLARE_OBJC_WRAPPED_INTERFACES)
#undef DECLARE_OBJC_WRAPPED_INTERFACES

#define DECLARE_UNIMPLEMENTED_WRAPPED_OBJC_HELPERS(CPPTYPE)     \
  inline WrappedMTL##CPPTYPE *GetWrapped(id<MTL##CPPTYPE> objC) \
  {                                                             \
    return (WrappedMTL##CPPTYPE *)objC;                         \
  }

METALCPP_UNIMPLEMENTED_WRAPPED_PROTOCOLS(DECLARE_UNIMPLEMENTED_WRAPPED_OBJC_HELPERS)
#undef DECLARE_UNIMPLEMENTED_WRAPPED_OBJC_HELPERS

inline WrappedMTLResource *GetWrapped(id<MTLResource> objC)
{
  return (WrappedMTLResource *)objC;
}

// Define Mac SDK versions when compiling with earlier SDKs
#ifndef __MAC_12_5
#define __MAC_12_5 120500
#endif

#ifndef __MAC_13_0
#define __MAC_13_0 130000
#endif

#ifndef __MAC_13_3
#define __MAC_13_3 130300
#endif

#ifndef __MAC_14_0
#define __MAC_14_0 140000
#endif

#ifndef __MAC_14_4
#define __MAC_14_4 140400
#endif
