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

#pragma once

#include "api/replay/rdcstr.h"
#include "official/metal-cpp.h"
#include "serialise/serialiser.h"

#define METALCPP_WRAPPED_PROTOCOLS(FUNC) \
  FUNC(CommandBuffer);                   \
  FUNC(CommandQueue);                    \
  FUNC(Device);                          \
  FUNC(Function);                        \
  FUNC(Library);

// These serialise overloads will fetch the ID during capture, serialise the ID
// directly as-if it were the original type, then on replay load up the resource if available.
#define DECLARE_WRAPPED_TYPE_SERIALISE(CPPTYPE)       \
  class WrappedMTL##CPPTYPE;                          \
  template <>                                         \
  inline rdcliteral TypeName<WrappedMTL##CPPTYPE *>() \
  {                                                   \
    return STRING_LITERAL(STRINGIZE(MTL##CPPTYPE));   \
  }                                                   \
  template <class SerialiserType>                     \
  void DoSerialise(SerialiserType &ser, WrappedMTL##CPPTYPE *&el);

METALCPP_WRAPPED_PROTOCOLS(DECLARE_WRAPPED_TYPE_SERIALISE);
#undef DECLARE_WRAPPED_TYPE_SERIALISE

#define DECLARE_OBJC_HELPERS(CPPTYPE)                           \
  class WrappedMTL##CPPTYPE;                                    \
  inline WrappedMTL##CPPTYPE *GetWrapped(MTL::CPPTYPE *cppType) \
  {                                                             \
    return (WrappedMTL##CPPTYPE *)cppType;                      \
  }                                                             \
  extern void AllocateObjCBridge(WrappedMTL##CPPTYPE *wrapped);

METALCPP_WRAPPED_PROTOCOLS(DECLARE_OBJC_HELPERS)
#undef DECLARE_OBJC_HELPERS

template <>
inline rdcliteral TypeName<NS::String *>()
{
  return "NSString"_lit;
}
template <class SerialiserType>
void DoSerialise(SerialiserType &ser, NS::String *&el);
