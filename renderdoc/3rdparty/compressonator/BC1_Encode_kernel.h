//=====================================================================
// Copyright (c) 2018    Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//=====================================================================
#ifndef BC1_ENCODE_KERNEL_H
#define BC1_ENCODE_KERNEL_H

#include "BCn_Common_Kernel.h"
#include "Common_Def.h"

#define CS_RED(r, g, b) (r)
#define CS_GREEN(r, g, b) (g)
#define CS_BLUE(r, g, b) ((b + g) * 0.5f)
#define DCS_RED(r, g, b) (r)
#define DCS_GREEN(r, g, b) (g)
#define DCS_BLUE(r, g, b) ((2.0f * b) - g)
#define BYTEPP 4
#define BC1CompBlockSize 8

#define ROUND_AND_CLAMP(v, shift)          \
  {                                        \
    if(v < 0)                              \
      v = 0;                               \
    else if(v > 255)                       \
      v = 255;                             \
    else                                   \
      v += (0x80 >> shift) - (v >> shift); \
  }

#define POS(x, y) (pos_on_axis[(x) + (y)*4])

#endif