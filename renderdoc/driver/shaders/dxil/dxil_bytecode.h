/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

#include "driver/dx/official/d3dcommon.h"
#include "driver/shaders/dxbc/dxbc_common.h"

namespace DXIL
{
class Program
{
public:
  Program(const byte *bytes, size_t length);

  void FetchComputeProperties(DXBC::Reflection *reflection);
  DXBC::Reflection *GetReflection();

  DXBC::ShaderType GetShaderType() { return m_Type; }
  uint32_t GetMajorVersion() { return m_Major; }
  uint32_t GetMinorVersion() { return m_Minor; }
  D3D_PRIMITIVE_TOPOLOGY GetOutputTopology();
  const std::string &GetDisassembly()
  {
    if(m_Disassembly.empty())
      MakeDisassemblyString();
    return m_Disassembly;
  }

private:
  void MakeDisassemblyString();

  DXBC::ShaderType m_Type;
  uint32_t m_Major, m_Minor;

  rdcstr m_Triple, m_Datalayout;

  std::string m_Disassembly;
};

};    // namespace DXIL
