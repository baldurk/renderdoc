/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2020 Baldur Karlsson
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

#include "api/replay/rdcstr.h"
#include "driver/dx/official/d3dcommon.h"
#include "driver/shaders/dxbc/dxbc_common.h"

namespace DXIL
{
struct Function
{
  rdcstr name;

  uint32_t funcType;
  uint32_t linkage;
};

struct GlobalVar
{
  rdcstr name;
};

struct Alias
{
  rdcstr name;
};

enum class SymbolType
{
  Function,
  GlobalVar,
  Alias,
};

struct Symbol
{
  SymbolType type;
  size_t idx;
};

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
  const rdcstr &GetDisassembly()
  {
    if(m_Disassembly.empty())
      MakeDisassemblyString();
    return m_Disassembly;
  }
  uint32_t GetDisassemblyLine(uint32_t instruction) const;

private:
  void MakeDisassemblyString();

  DXBC::ShaderType m_Type;
  uint32_t m_Major, m_Minor;

  rdcarray<GlobalVar> m_GlobalVars;
  rdcarray<Function> m_Functions;
  rdcarray<Alias> m_Aliases;
  rdcarray<Symbol> m_Symbols;

  rdcarray<rdcstr> m_Kinds;

  rdcstr m_Triple, m_Datalayout;

  rdcstr m_Disassembly;
};

};    // namespace DXIL
