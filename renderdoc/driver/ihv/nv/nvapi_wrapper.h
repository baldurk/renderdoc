/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Baldur Karlsson
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
#include <windows.h>

enum class NvShaderOpcode : uint32_t
{
  UINT64Atomic = 20,
};

enum class NvShaderAtomic : uint32_t
{
  And = 0,
  Or = 1,
  Xor = 2,
  Add = 3,
  Max = 6,
  Min = 7,
  Swap = 8,
  CompareAndSwap = 9,
};

MIDL_INTERFACE("DA122FC2-0F60-4904-AEA4-5ED1D2E1D19F")
INVAPID3DDevice : public IUnknown
{
  virtual BOOL STDMETHODCALLTYPE SetReal(IUnknown * device) = 0;
  virtual IUnknown *STDMETHODCALLTYPE GetReal() = 0;
  virtual BOOL STDMETHODCALLTYPE SetShaderExtUAV(DWORD space, DWORD reg, BOOL global) = 0;
};

INVAPID3DDevice *InitialiseNVAPIReplay();
bool SupportedOpcode(NvShaderOpcode opcode);
