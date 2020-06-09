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

  bool isVoid() const { return type == Scalar && scalarType == Void; }
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
  Unknown,
  Function,
  GlobalVar,
  Alias,
  Constant,
  Argument,
  Instruction,
  Metadata,
  Literal,
};

struct Symbol
{
  Symbol(SymbolType type = SymbolType::Unknown, uint64_t idx = 0) : type(type), idx(idx) {}
  SymbolType type;
  uint64_t idx;
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

struct Value
{
  const Type *type = NULL;
  ShaderValue val = {};
  rdcarray<Value> members;
  rdcstr str;
  bool undef = false, nullconst = true, symbol = false;

  rdcstr toString(bool withType = false) const;
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
    Expression,
    LexicalBlock,
    Subrange,
  } type;

  DIBase(Type t) : type(t) {}
  virtual ~DIBase() = default;
  virtual rdcstr toString() const = 0;

  template <typename Derived>
  const Derived *As() const
  {
    RDCASSERT(type == Derived::DIType);
    return (const Derived *)this;
  }
};

struct Function;

struct Metadata
{
  ~Metadata();

  uint32_t id = ~0U;
  bool distinct = false, value = false;

  const Value *val = NULL;

  const Function *func = NULL;
  size_t instruction = ~0U;

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

struct DebugLocation
{
  uint32_t id = ~0U;

  uint64_t line = 0;
  uint64_t col = 0;
  const Metadata *scope = NULL;
  const Metadata *inlinedAt = NULL;

  bool operator==(const DebugLocation &o) const
  {
    return line == o.line && col == o.col && scope == o.scope && inlinedAt == o.inlinedAt;
  }
};

struct Function;

enum class InstructionFlags : uint32_t
{
  NoFlags = 0,

  // float fastmath flags. These should match LLVMs bits
  FastMath = (1 << 0),
  NoNaNs = (1 << 1),
  NoInfs = (1 << 2),
  NoSignedZeros = (1 << 3),
  AllowReciprocal = (1 << 4),

  // integer add/mul/sub/left shift
  NoUnsignedWrap = (1 << 5),
  NoSignedWrap = (1 << 6),

  // shifts/divs
  Exact = (1 << 7),

  // load/store
  InBounds = (1 << 8),
  Volatile = (1 << 9),
};

BITMASK_OPERATORS(InstructionFlags);

typedef rdcarray<rdcpair<uint64_t, Metadata *>> AttachedMetadata;

struct Instruction
{
  enum
  {
    Unknown,
    Call,
    Trunc,
    ZExt,
    SExt,
    FToU,
    FToS,
    UToF,
    SToF,
    FPTrunc,
    FPExt,
    PtrToI,
    IToPtr,
    Bitcast,
    AddrSpaceCast,
    ExtractVal,
    Ret,
    FAdd,
    FSub,
    FMul,
    FDiv,
    FRem,
    Add,
    Sub,
    Mul,
    UDiv,
    SDiv,
    URem,
    SRem,
    ShiftLeft,
    LogicalShiftRight,
    ArithShiftRight,
    And,
    Or,
    Xor,
    Unreachable,
    Alloca,
    GetElementPtr,
    Load,
    Store,
    FOrdFalse,
    FOrdEqual,
    FOrdGreater,
    FOrdGreaterEqual,
    FOrdLess,
    FOrdLessEqual,
    FOrdNotEqual,
    FOrd,
    FUnord,
    FUnordEqual,
    FUnordGreater,
    FUnordGreaterEqual,
    FUnordLess,
    FUnordLessEqual,
    FUnordNotEqual,
    FOrdTrue,
    IEqual,
    INotEqual,
    UGreater,
    UGreaterEqual,
    ULess,
    ULessEqual,
    SGreater,
    SGreaterEqual,
    SLess,
    SLessEqual,
    Select,
    ExtractElement,
    InsertElement,
    ShuffleVector,
    InsertValue,
  } op = Unknown;

  InstructionFlags opFlags = InstructionFlags::NoFlags;

  // common to all instructions
  rdcstr name;
  uint32_t disassemblyLine = 0;
  uint32_t resultID = ~0U;
  uint32_t debugLoc = ~0U;
  uint32_t align = 0;
  const Type *type = NULL;
  rdcarray<Symbol> args;
  AttachedMetadata attachedMeta;

  // function calls
  const Attributes *paramAttrs = NULL;
  const Function *funcCall = NULL;
};

struct Block
{
};

struct Function
{
  rdcstr name;

  const Type *funcType = NULL;
  bool external = false;
  const Attributes *attrs = NULL;

  rdcarray<Instruction> args;
  rdcarray<Instruction> instructions;

  rdcarray<Block> blocks;
  rdcarray<Value> values;
  rdcarray<Metadata> metadata;

  AttachedMetadata attachedMeta;
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

private:
  void MakeDisassemblyString();

  bool ParseDebugMetaRecord(const LLVMBC::BlockOrRecord &metaRecord, Metadata &meta);
  rdcstr GetDebugVarName(const DIBase *d);

  uint32_t GetOrAssignMetaID(Metadata *m);
  uint32_t GetOrAssignMetaID(DebugLocation &l);
  const Type *GetSymbolType(const Function &f, Symbol s);
  const Value *GetFunctionValue(const Function &f, uint64_t v);
  const Metadata *GetFunctionMetadata(const Function &f, uint64_t v);
  const Type *GetVoidType();
  const Type *GetBoolType();

  DXBC::ShaderType m_Type;
  uint32_t m_Major, m_Minor;

  rdcarray<GlobalVar> m_GlobalVars;
  rdcarray<Function> m_Functions;
  rdcarray<Alias> m_Aliases;
  rdcarray<Symbol> m_Symbols;

  rdcarray<rdcstr> m_Kinds;

  rdcarray<Type> m_Types;
  const Type *m_VoidType = NULL;
  const Type *m_BoolType = NULL;

  rdcarray<Attributes> m_AttributeGroups;
  rdcarray<Attributes> m_Attributes;

  rdcarray<Value> m_Values;

  rdcarray<Metadata> m_Metadata;
  rdcarray<NamedMetadata> m_NamedMeta;
  rdcarray<Metadata *> m_NumberedMeta;
  uint32_t m_NextMetaID = 0;

  rdcarray<DebugLocation> m_DebugLocations;

  rdcstr m_Triple, m_Datalayout;

  rdcstr m_Disassembly;
};

bool needsEscaping(const rdcstr &name);
rdcstr escapeString(rdcstr str);
rdcstr escapeStringIfNeeded(const rdcstr &name);

};    // namespace DXIL

DECLARE_REFLECTION_ENUM(DXIL::Attribute);
DECLARE_STRINGISE_TYPE(DXIL::InstructionFlags);
