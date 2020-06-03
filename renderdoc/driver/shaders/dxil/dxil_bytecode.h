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

#include "api/replay/apidefs.h"
#include "api/replay/rdcstr.h"
#include "driver/dx/official/d3dcommon.h"
#include "driver/shaders/dxbc/dxbc_common.h"

namespace LLVMBC
{
struct BlockOrRecord;
};

namespace DXIL
{
struct Type
{
  enum TypeKind
  {
    None = 0,

    Scalar,
    Vector,
    Pointer,
    Array,
    Function,
    Struct,

    Metadata,
    Label,
  } type = None;

  enum ScalarKind
  {
    Void = 0,
    Float,
    Int,
  } scalarType = Void;

  rdcstr toString() const;
  rdcstr declFunction(rdcstr funcName) const;

  // for scalars, arrays, vectors
  uint32_t bitWidth = 0, elemCount = 0;

  // the single inner type for pointers, vectors, or arrays, the return type for functions
  const Type *inner = NULL;

  // struct or function
  bool packedStruct = false, vararg = false;
  rdcstr name;
  rdcarray<const Type *> members;    // the members for a struct, the parameters for functions
};

struct GlobalVar
{
  rdcstr name;
  const Type *type = NULL;
  bool isconst = false;
  bool external = false;
  uint64_t align = 0;
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

// this enum is ordered to match the serialised order of these attributes
enum class Attribute : uint64_t
{
  None = 0,
  // 0 is unused, so no 1ULL << 0
  Alignment = 1ULL << 1,
  AlwaysInline = 1ULL << 2,
  ByVal = 1ULL << 3,
  InlineHint = 1ULL << 4,
  InReg = 1ULL << 5,
  MinSize = 1ULL << 6,
  Naked = 1ULL << 7,
  Nest = 1ULL << 8,
  NoAlias = 1ULL << 9,
  NoBuiltin = 1ULL << 10,
  NoCapture = 1ULL << 11,
  NoDuplicate = 1ULL << 12,
  NoImplicitFloat = 1ULL << 13,
  NoInline = 1ULL << 14,
  NonLazyBind = 1ULL << 15,
  NoRedZone = 1ULL << 16,
  NoReturn = 1ULL << 17,
  NoUnwind = 1ULL << 18,
  OptimizeForSize = 1ULL << 19,
  ReadNone = 1ULL << 20,
  ReadOnly = 1ULL << 21,
  Returned = 1ULL << 22,
  ReturnsTwice = 1ULL << 23,
  SExt = 1ULL << 24,
  StackAlignment = 1ULL << 25,
  StackProtect = 1ULL << 26,
  StackProtectReq = 1ULL << 27,
  StackProtectStrong = 1ULL << 28,
  StructRet = 1ULL << 29,
  SanitizeAddress = 1ULL << 30,
  SanitizeThread = 1ULL << 31,
  SanitizeMemory = 1ULL << 32,
  UWTable = 1ULL << 33,
  ZExt = 1ULL << 34,
  Builtin = 1ULL << 35,
  Cold = 1ULL << 36,
  OptimizeNone = 1ULL << 37,
  InAlloca = 1ULL << 38,
  NonNull = 1ULL << 39,
  JumpTable = 1ULL << 40,
  Dereferenceable = 1ULL << 41,
  DereferenceableOrNull = 1ULL << 42,
  Convergent = 1ULL << 43,
  SafeStack = 1ULL << 44,
  ArgMemOnly = 1ULL << 45,
};

BITMASK_OPERATORS(Attribute);

struct Attributes
{
  uint64_t index = 0;

  Attribute params = Attribute::None;
  uint64_t align = 0, stackAlign = 0, derefBytes = 0, derefOrNullBytes = 0;
  rdcarray<rdcpair<rdcstr, rdcstr>> strs;

  rdcstr toString() const;
};

struct Function
{
  rdcstr name;

  const Type *funcType = NULL;
  bool external = false;
  const Attributes *attrs = NULL;
};

struct Value
{
  const Type *type = NULL;
  ShaderValue val = {};
  rdcarray<Value> members;
  rdcstr str;
  bool undef = false, symbol = false;

  rdcstr toString() const;
};

struct DIBase
{
  enum Type
  {
    File,
    CompileUnit,
    BasicType,
    DerivedType,
    CompositeType,
    TemplateTypeParameter,
    TemplateValueParameter,
    Subprogram,
    SubroutineType,
    GlobalVariable,
    LocalVariable,
    Location,
    Expression,
  } type;

  DIBase(Type t) : type(t) {}
  virtual ~DIBase() = default;
  virtual rdcstr toString() const = 0;

  template <typename Derived>
  const Derived As()
  {
    RDCASSERT(type == Derived::DIType);
    return (Derived *)this;
  }
};

struct Metadata
{
  ~Metadata();

  uint32_t id = ~0U;
  bool distinct = false, value = false;
  const Value *val = NULL;
  const Type *type = NULL;
  rdcstr str;
  rdcarray<Metadata *> children;
  DIBase *dwarf = NULL;

  rdcstr refString() const;
  rdcstr valString() const;
};

struct NamedMetadata : public Metadata
{
  rdcstr name;
};

class Program
{
public:
  Program(const byte *bytes, size_t length);
  Program(const Program &o) = default;
  Program &operator=(const Program &o) = default;

  static bool Valid(const byte *bytes, size_t length);

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

  bool ParseDebugMetaRecord(const LLVMBC::BlockOrRecord &metaRecord, Metadata &meta);

  uint32_t GetOrAssignMetaID(Metadata *m);

  DXBC::ShaderType m_Type;
  uint32_t m_Major, m_Minor;

  rdcarray<GlobalVar> m_GlobalVars;
  rdcarray<Function> m_Functions;
  rdcarray<Alias> m_Aliases;
  rdcarray<Symbol> m_Symbols;

  rdcarray<rdcstr> m_Kinds;

  rdcarray<Type> m_Types;

  rdcarray<Attributes> m_AttributeGroups;
  rdcarray<Attributes> m_Attributes;

  rdcarray<Value> m_Values;

  rdcarray<Metadata> m_Metadata;
  rdcarray<NamedMetadata> m_NamedMeta;
  rdcarray<Metadata *> m_NumberedMeta;

  rdcstr m_Triple, m_Datalayout;

  rdcstr m_Disassembly;
};

};    // namespace DXIL
