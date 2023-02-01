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
  ProgramEditor(const DXBC::DXBCContainer *container, size_t reservationSize, bytebuf &outBlob);
  ~ProgramEditor();

  // publish these interfaces
  using Program::GetVoidType;
  using Program::GetBoolType;
  using Program::GetInt32Type;
  using Program::GetInt8Type;
  using Program::GetPointerType;

  const rdcarray<AttributeSet> &GetAttributeSets() { return m_AttributeSets; }
  // find existing type or metadata by name, returns NULL if not found
  const rdcarray<Type> &GetTypes() const { return m_Types; }
  const Type *GetTypeByName(const rdcstr &name);
  Function *GetFunctionByName(const rdcstr &name);
  Metadata *GetMetadataByName(const rdcstr &name);

  // add a type, function, or metadata, and return the pointer to the stored one (which can be
  // referenced from elsewhere)
  const Type *AddType(const Type &t);
  const Function *DeclareFunction(const Function &f);
  Metadata *AddMetadata(const Metadata &m);
  NamedMetadata *AddNamedMetadata(const NamedMetadata &m);

  // I think constants have to be unique, so this will return an existing constant (for simple cases
  // like integers or NULL) if it exists
  const Constant *GetOrAddConstant(const Constant &c);
  const Constant *GetOrAddConstant(Function *f, const Constant &c);

  Instruction *AddInstruction(Function *f, size_t idx, const Instruction &inst);

  void RegisterUAV(DXILResourceType type, uint32_t space, uint32_t regBase, uint32_t regEnd,
                   ResourceKind kind);

private:
  bytebuf &m_OutBlob;

  void Fixup(Type *&t);
  void Fixup(Function *&f);
  void Fixup(Block *&b, Function *oldf = NULL, Function *newf = NULL);
  void Fixup(Instruction *&i, Function *oldf = NULL, Function *newf = NULL);
  void Fixup(Constant *&c, Function *oldf = NULL, Function *newf = NULL);
  void Fixup(Metadata *&m, Function *oldf = NULL, Function *newf = NULL);
  void Fixup(Value &v, Function *oldf = NULL, Function *newf = NULL);

  void Fixup(const Type *&t) { Fixup((Type *&)t); }
  void Fixup(const Function *&f) { Fixup((Function *&)f); }
  void Fixup(const Block *&b, Function *oldf = NULL, Function *newf = NULL)
  {
    Fixup((Block *&)b, oldf, newf);
  }
  void Fixup(const Instruction *&i, Function *oldf = NULL, Function *newf = NULL)
  {
    Fixup((Instruction *&)i, oldf, newf);
  }
  void Fixup(const Constant *&c, Function *oldf = NULL, Function *newf = NULL)
  {
    Fixup((Constant *&)c, oldf, newf);
  }

  bytebuf EncodeProgram() const;

  void EncodeConstants(LLVMBC::BitcodeWriter &writer, const rdcarray<Value> &values,
                       const rdcarray<Constant> &constants) const;
  void EncodeMetadata(LLVMBC::BitcodeWriter &writer, const rdcarray<Value> &values,
                      const rdcarray<Metadata> &meta) const;

  // these are arrays which hold the original program's storage, so all pointers remain valid. We
  // then duplicate into the editable arrays

  // in the destructor before encoding we will look up any pointers that still point here and find
  // the element in the current arrays

  // every array which might be mutated by editing must be here
  rdcarray<Function> m_OldFunctions;
  rdcarray<Type> m_OldTypes;
  rdcarray<Constant> m_OldConstants;
  rdcarray<Metadata> m_OldMetadata;

  uint32_t m_InsertedInstructionID = 0xfffffff0;

#if ENABLED(RDOC_DEVEL)
  Function *m_DebugFunctionsData;
  Type *m_DebugTypesData;
  Constant *m_DebugConstantsData;
  Metadata *m_DebugMetadataData;

  struct DebugFunctionData
  {
    Instruction *instructions;
    Constant *constants;
  };
  rdcarray<DebugFunctionData> m_DebugFunctions;
#endif
};

};    // namespace DXIL
