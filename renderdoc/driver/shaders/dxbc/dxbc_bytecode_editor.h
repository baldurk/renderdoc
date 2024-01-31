/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2021-2024 Baldur Karlsson
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

#include "dxbc_bytecode.h"

namespace DXBC
{
class DXBCContainer;
};

namespace DXBCBytecode
{
namespace Edit
{
inline Operand temp(uint32_t reg)
{
  Operand o;
  o.type = TYPE_TEMP;
  o.indices.resize(1);
  o.indices[0].absolute = true;
  o.indices[0].index = reg;
  return o.swizzle(0, 1, 2, 3);
}

inline Operand input(uint32_t reg)
{
  Operand o;
  o.type = TYPE_INPUT;
  o.indices.resize(1);
  o.indices[0].absolute = true;
  o.indices[0].index = reg;
  return o.swizzle(0, 1, 2, 3);
}

inline Operand imm(uint32_t val)
{
  Operand o;
  o.type = TYPE_IMMEDIATE32;
  o.values[0] = val;
  return o.swizzle(0);
}

inline Operand imm(uint32_t x, uint32_t y, uint32_t z, uint32_t w)
{
  Operand o;
  o.type = TYPE_IMMEDIATE32;
  o.values[0] = x;
  o.values[1] = y;
  o.values[2] = z;
  o.values[3] = w;
  return o.swizzle(0, 1, 2, 3);
}

typedef rdcpair<uint32_t, uint32_t> ResourceIdentifier;

inline Operand res(ResourceIdentifier identifier, uint32_t idx = 0)
{
  Operand o;
  o.type = TYPE_RESOURCE;
  if(identifier.second == ~0U)
  {
    // SM5.0, just identifier
    o.indices.resize(1);
    o.indices[0].absolute = true;
    o.indices[0].index = identifier.first;
  }
  else
  {
    // SM5.1 identifier and register
    o.indices.resize(2);
    o.indices[0].absolute = true;
    o.indices[0].index = identifier.first;
    o.indices[1].absolute = true;
    o.indices[1].index = identifier.second + idx;
  }
  return o.swizzle(0, 1, 2, 3);
}

inline Operand uav(ResourceIdentifier identifier, uint32_t idx = 0)
{
  Operand o;
  o.type = TYPE_UNORDERED_ACCESS_VIEW;
  if(identifier.second == ~0U)
  {
    // SM5.0, just identifier
    o.indices.resize(1);
    o.indices[0].absolute = true;
    o.indices[0].index = identifier.first;
  }
  else
  {
    // SM5.1 identifier and register
    o.indices.resize(2);
    o.indices[0].absolute = true;
    o.indices[0].index = identifier.first;
    o.indices[1].absolute = true;
    o.indices[1].index = identifier.second + idx;
  }
  return o.swizzle(0, 1, 2, 3);
}

Operation oper(OpcodeType o, const rdcarray<Operand> &operands);

};    // namespace Edit

struct ResourceDecl
{
  TextureType type = TextureType::Buffer;
  CompType compType = CompType::Float;
  uint32_t sampleCount = 0;

  bool structured = false;
  uint32_t stride = 0;
  bool hasCounter = false, globallyCoherant = false, rov = false;

  bool raw = false;
};

class ProgramEditor : public Program
{
public:
  ProgramEditor(const DXBC::DXBCContainer *container, bytebuf &outBlob);
  ~ProgramEditor();

  uint32_t AddTemp();

  Edit::ResourceIdentifier DeclareResource(const ResourceDecl &desc, uint32_t space,
                                           uint32_t regLow, uint32_t regHigh);
  Edit::ResourceIdentifier DeclareUAV(const ResourceDecl &desc, uint32_t space, uint32_t regLow,
                                      uint32_t regHigh);

  void InsertOperation(size_t beforeIndex, const Operation &op)
  {
    m_Instructions.insert(beforeIndex, op);
  }

  void RemoveOperation(size_t idx, size_t count = 1) { m_Instructions.erase(idx, count); }
  Operation &GetInstruction(size_t idx) { return m_Instructions[idx]; };
  Declaration &GetDeclaration(size_t idx) { return m_Declarations[idx]; };
private:
  bytebuf &m_OutBlob;
  bool m_SM51 = false;

  size_t GetDeclarationPosition(OpcodeType op);
};

};    // namespace DXBCBytecode
