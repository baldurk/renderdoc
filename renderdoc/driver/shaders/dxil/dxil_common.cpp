/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "dxil_common.h"

template <>
rdcstr DoStringise(const DXIL::ComponentType &el)
{
  BEGIN_ENUM_STRINGISE(DXIL::ComponentType);
  {
    STRINGISE_ENUM_CLASS_NAMED(Invalid, "<invalid CompType>");
    STRINGISE_ENUM_CLASS(I1);
    STRINGISE_ENUM_CLASS(I16);
    STRINGISE_ENUM_CLASS(U16);
    STRINGISE_ENUM_CLASS(I32);
    STRINGISE_ENUM_CLASS(U32);
    STRINGISE_ENUM_CLASS(I64);
    STRINGISE_ENUM_CLASS(U64);
    STRINGISE_ENUM_CLASS(F16);
    STRINGISE_ENUM_CLASS(F32);
    STRINGISE_ENUM_CLASS(F64);
    STRINGISE_ENUM_CLASS(SNormF16);
    STRINGISE_ENUM_CLASS(UNormF16);
    STRINGISE_ENUM_CLASS(SNormF32);
    STRINGISE_ENUM_CLASS(UNormF32);
    STRINGISE_ENUM_CLASS(SNormF64);
    STRINGISE_ENUM_CLASS(UNormF64);
  }
  END_ENUM_STRINGISE();
}

template <>
rdcstr DoStringise(const DXIL::ResourceClass &el)
{
  BEGIN_ENUM_STRINGISE(DXIL::ResourceClass);
  {
    STRINGISE_ENUM_CLASS(SRV);
    STRINGISE_ENUM_CLASS(UAV);
    STRINGISE_ENUM_CLASS(CBuffer);
    STRINGISE_ENUM_CLASS(Sampler);
    STRINGISE_ENUM_CLASS_NAMED(Invalid, "<invalid ResourceClass>");
  }
  END_ENUM_STRINGISE();
}
