/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2023 Baldur Karlsson
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

#include "dxil_bytecode.h"
#include "dxil_common.h"

namespace DXBC
{
class DXBCContainer;
};

namespace LLVMBC
{
class BitcodeWriter;
};

namespace DXIL
{
enum class DXILResourceType
{
  Unknown,
  Sampler,
  CBuffer,
  TypedSRV,
  ByteAddressSRV,
  StructuredSRV,
  TypedUAV,
  ByteAddressUAV,
  StructuredUAV,
  StructuredUAVWithCounter,
};

enum class HandleKind
{
  SRV = 0,
  UAV = 1,
  CBuffer = 2,
  Sampler = 3,
};

class ProgramEditor : public Program
{
public:
  ProgramEditor(const DXBC::DXBCContainer *container, bytebuf &outBlob);
  ~ProgramEditor();

  // publish these interfaces
  using Program::GetVoidType;
  using Program::GetBoolType;
  using Program::GetInt32Type;
  using Program::GetInt8Type;
  using Program::GetPointerType;

  const rdcarray<AttributeSet> &GetAttributeSets() { return m_AttributeSets; }
  const rdcarray<Type *> &GetTypes() const { return m_Types; }
  Type *CreateNamedStructType(const rdcstr &name, rdcarray<const Type *> members);
  Type *CreateFunctionType(const Type *ret, rdcarray<const Type *> params);
  Type *CreatePointerType(const Type *inner, Type::PointerAddrSpace addrSpace);
  Function *GetFunctionByName(const rdcstr &name);
  Metadata *GetMetadataByName(const rdcstr &name);

  Function *DeclareFunction(const Function &f);
  Metadata *CreateMetadata();
  Metadata *CreateConstantMetadata(Constant *val);
  Metadata *CreateConstantMetadata(uint32_t val);
  Metadata *CreateConstantMetadata(uint8_t val);
  Metadata *CreateConstantMetadata(bool val);
  Metadata *CreateConstantMetadata(const char *str) { return CreateConstantMetadata(rdcstr(str)); }
  Metadata *CreateConstantMetadata(const rdcstr &str);
  NamedMetadata *CreateNamedMetadata(const rdcstr &name);

  // I think constants have to be unique, so this will return an existing constant (for simple cases
  // like integers or NULL) if it exists
  Constant *CreateConstant(const Constant &c);
  Constant *CreateConstant(const Type *t, const rdcarray<Value *> &members);

  Constant *CreateConstant(uint32_t u) { return CreateConstant(Constant(m_Int32Type, u)); }
  Constant *CreateConstant(uint8_t u) { return CreateConstant(Constant(m_Int8Type, u)); }
  Constant *CreateConstant(bool b) { return CreateConstant(Constant(m_BoolType, b)); }
  Instruction *CreateInstruction(Operation op);
  Instruction *CreateInstruction(const Function *f);

  void RegisterUAV(DXILResourceType type, uint32_t space, uint32_t regBase, uint32_t regEnd,
                   ResourceKind kind);

private:
  bytebuf &m_OutBlob;

  rdcarray<Constant *> m_Constants;

  Type *CreateNewType();

  bytebuf EncodeProgram();

  void EncodeConstants(LLVMBC::BitcodeWriter &writer, const rdcarray<const Value *> &values,
                       size_t firstIdx, size_t count) const;
  void EncodeMetadata(LLVMBC::BitcodeWriter &writer, const rdcarray<const Metadata *> &meta) const;
};

};    // namespace DXIL
