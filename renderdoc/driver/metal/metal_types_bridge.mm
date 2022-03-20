/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022 Baldur Karlsson
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

#include "metal_types_bridge.h"
#include "metal_command_buffer.h"
#include "metal_command_queue.h"
#include "metal_device.h"
#include "metal_function.h"
#include "metal_library.h"

#define DEFINE_OBJC_HELPERS(CPPTYPE)                                               \
  static ObjCBridgeMTL##CPPTYPE *GetObjCBridge(MTL::CPPTYPE *cppType)              \
  {                                                                                \
    if(cppType == NULL)                                                            \
    {                                                                              \
      return NULL;                                                                 \
    }                                                                              \
    ObjCBridgeMTL##CPPTYPE *objC = (ObjCBridgeMTL##CPPTYPE *)cppType;              \
    RDCASSERT([objC isKindOfClass:[ObjCBridgeMTL##CPPTYPE class]]);                \
    return objC;                                                                   \
  }                                                                                \
                                                                                   \
  WrappedMTL##CPPTYPE *GetWrapped(MTL::CPPTYPE *cppType)                           \
  {                                                                                \
    ObjCBridgeMTL##CPPTYPE *objC = GetObjCBridge(cppType);                         \
    return objC.wrappedCPP;                                                        \
  }                                                                                \
                                                                                   \
  MTL::CPPTYPE *GetReal(MTL::CPPTYPE *cppType)                                     \
  {                                                                                \
    ObjCBridgeMTL##CPPTYPE *objC = GetObjCBridge(cppType);                         \
    MTL::CPPTYPE *real = (MTL::CPPTYPE *)objC.real;                                \
    return real;                                                                   \
  }                                                                                \
                                                                                   \
  bool IsObjCBridge(MTL::CPPTYPE *cppType)                                         \
  {                                                                                \
    ObjCBridgeMTL##CPPTYPE *objC = (ObjCBridgeMTL##CPPTYPE *)cppType;              \
    return [objC isKindOfClass:[ObjCBridgeMTL##CPPTYPE class]];                    \
  }                                                                                \
                                                                                   \
  ResourceId GetResID(MTL::CPPTYPE *cppType)                                       \
  {                                                                                \
    WrappedMTL##CPPTYPE *wrappedCPP = GetWrapped(cppType);                         \
    if(wrappedCPP == NULL)                                                         \
    {                                                                              \
      return ResourceId();                                                         \
    }                                                                              \
    return wrappedCPP->id;                                                         \
  }                                                                                \
                                                                                   \
  MTL::CPPTYPE *AllocateObjCBridge(WrappedMTL##CPPTYPE *wrappedCPP)                \
  {                                                                                \
    ObjCBridgeMTL##CPPTYPE *objC = [ObjCBridgeMTL##CPPTYPE alloc];                 \
    objC.wrappedCPP = wrappedCPP;                                                  \
    MTL::CPPTYPE *real = (MTL::CPPTYPE *)objC.real;                                \
    if(real)                                                                       \
    {                                                                              \
      objc_setAssociatedObject((id)real, objC, (id)objC, OBJC_ASSOCIATION_RETAIN); \
      [objC release];                                                              \
    }                                                                              \
    return (MTL::CPPTYPE *)objC;                                                   \
  }

METALCPP_WRAPPED_PROTOCOLS(DEFINE_OBJC_HELPERS)
#undef DEFINE_OBJC_HELPERS
