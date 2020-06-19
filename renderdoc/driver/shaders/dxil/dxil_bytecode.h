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

  enum PointerAddrSpace
  {
    Default = 0,
    DeviceMemory = 1,
    CBuffer = 2,
    GroupShared = 3,
    GenericPointer = 4,
    ImmediateCBuffer = 5,
  };

  bool isVoid() const { return type == Scalar && scalarType == Void; }
  rdcstr toString() const;
  rdcstr declFunction(rdcstr funcName) const;

  // for scalars, arrays, vectors, pointers
  union
  {
    uint32_t bitWidth = 0;
    PointerAddrSpace addrSpace;
  };
  uint32_t elemCount = 0;

  // the single inner type for pointers, vectors, or arrays, the return type for functions
  const Type *inner = NULL;

  // struct or function
  bool packedStruct = false, vararg = false;
  rdcstr name;
  rdcarray<const Type *> members;    // the members for a struct, the parameters for functions
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
  BasicBlock,
};

struct Symbol
{
  Symbol(SymbolType type = SymbolType::Unknown, uint64_t idx = 0) : type(type), idx(idx) {}
  SymbolType type;
  uint64_t idx;
};

enum class GlobalFlags : uint32_t
{
  NoFlags = 0,
  IsConst = 0x1,
  IsExternal = 0x2,
  LocalUnnamedAddr = 0x4,
  GlobalUnnamedAddr = 0x8,
  IsAppending = 0x10,
};

BITMASK_OPERATORS(GlobalFlags);

struct GlobalVar
{
  rdcstr name;
  const Type *type = NULL;
  uint64_t align = 0;
  int32_t section = -1;
  GlobalFlags flags = GlobalFlags::NoFlags;
  Symbol initialiser;
};

struct Alias
{
  rdcstr name;
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

enum class Operation : uint8_t
{
  NoOp,
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
  Branch,
  Phi,
  Switch,
  Fence,
  CompareExchange,
  LoadAtomic,
  StoreAtomic,
  AtomicExchange,
  AtomicAdd,
  AtomicSub,
  AtomicAnd,
  AtomicNand,
  AtomicOr,
  AtomicXor,
  AtomicMax,
  AtomicMin,
  AtomicUMax,
  AtomicUMin,
};

struct Constant
{
  const Type *type = NULL;
  ShaderValue val = {};
  rdcarray<Constant> members;
  const Constant *inner = NULL;
  rdcstr str;
  bool undef = false, nullconst = false, symbol = false;
  Operation op = Operation::NoOp;

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
  virtual void setID(uint32_t ID) {}
  template <typename Derived>
  const Derived *As() const
  {
    RDCASSERT(type == Derived::DIType);
    return (const Derived *)this;
  }
};

struct Function;

struct Metadata;

struct DebugLocation
{
  uint32_t id = ~0U;

  uint64_t line = 0;
  uint64_t col = 0;
  Metadata *scope = NULL;
  Metadata *inlinedAt = NULL;

  bool operator==(const DebugLocation &o) const
  {
    return line == o.line && col == o.col && scope == o.scope && inlinedAt == o.inlinedAt;
  }

  rdcstr toString() const;
};

struct Metadata
{
  ~Metadata();

  uint32_t id = ~0U;
  bool isDistinct = false, isConstant = false;

  const Constant *constant = NULL;

  const Function *func = NULL;
  size_t instruction = ~0U;

  const Type *type = NULL;
  rdcstr str;
  rdcarray<Metadata *> children;
  DIBase *dwarf = NULL;
  DebugLocation *debugLoc = NULL;

  rdcstr refString() const;
  rdcstr valString() const;
};

struct NamedMetadata : public Metadata
{
  rdcstr name;
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

  // load/store/atomic
  InBounds = (1 << 8),
  Volatile = (1 << 9),

  // atomics
  Weak = (1 << 10),
  SingleThread = (1 << 11),
  // CrossThread = (0 << 11),

  SuccessOrderMask = (0x7 << 12),

  // SuccessNotAtomic = (0x0 << 12),
  SuccessUnordered = (0x1 << 12),
  SuccessMonotonic = (0x2 << 12),
  SuccessAcquire = (0x3 << 12),
  SuccessRelease = (0x4 << 12),
  SuccessAcquireRelease = (0x5 << 12),
  SuccessSequentiallyConsistent = (0x6 << 12),

  FailureOrderMask = (0x7 << 15),

  // FailureNotAtomic = (0x0 << 15),
  FailureUnordered = (0x1 << 15),
  FailureMonotonic = (0x2 << 15),
  FailureAcquire = (0x3 << 15),
  FailureRelease = (0x4 << 15),
  FailureAcquireRelease = (0x5 << 15),
  FailureSequentiallyConsistent = (0x6 << 15),
};

BITMASK_OPERATORS(InstructionFlags);

typedef rdcarray<rdcpair<uint64_t, Metadata *>> AttachedMetadata;

struct Block;

struct Instruction
{
  Operation op = Operation::NoOp;
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
  uint32_t resultID = ~0U;
  rdcstr name;
  rdcarray<const Block *> preds;
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
  rdcarray<Constant> constants;
  rdcarray<Metadata> metadata;

  AttachedMetadata attachedMeta;
};

class Program : public DXBC::IDebugInfo
{
public:
  Program(const byte *bytes, size_t length);
  Program(const Program &o) = default;
  Program &operator=(const Program &o) = default;
  virtual ~Program() {}
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

  // IDebugInfo interface

  rdcstr GetCompilerSig() const override { return m_CompilerSig; }
  rdcstr GetEntryFunction() const override { return m_EntryPoint; }
  rdcstr GetShaderProfile() const override { return m_Profile; }
  ShaderCompileFlags GetShaderCompileFlags() const override { return m_CompileFlags; }
  void GetLineInfo(size_t instruction, uintptr_t offset, LineColumnInfo &lineInfo) const override;
  void GetCallstack(size_t instruction, uintptr_t offset, rdcarray<rdcstr> &callstack) const override;

  bool HasSourceMapping() const override;
  void GetLocals(const DXBC::DXBCContainer *dxbc, size_t instruction, uintptr_t offset,
                 rdcarray<SourceVariableMapping> &locals) const override;

private:
  void MakeDisassemblyString();

  bool ParseDebugMetaRecord(const LLVMBC::BlockOrRecord &metaRecord, Metadata &meta);
  rdcstr GetDebugVarName(const DIBase *d);

  uint32_t GetOrAssignMetaID(Metadata *m);
  uint32_t GetOrAssignMetaID(DebugLocation &l);
  const Type *GetSymbolType(const Function &f, Symbol s);
  const Constant *GetFunctionConstant(const Function &f, uint64_t v);
  const Metadata *GetFunctionMetadata(const Function &f, uint64_t v);
  const Type *GetVoidType();
  const Type *GetBoolType();
  const Type *GetPointerType(const Type *type);

  DXBC::ShaderType m_Type;
  uint32_t m_Major, m_Minor;

  rdcstr m_CompilerSig, m_EntryPoint, m_Profile;
  ShaderCompileFlags m_CompileFlags;

  rdcarray<GlobalVar> m_GlobalVars;
  rdcarray<Function> m_Functions;
  rdcarray<Alias> m_Aliases;
  rdcarray<Symbol> m_Symbols;
  rdcarray<rdcstr> m_Sections;

  rdcarray<rdcstr> m_Kinds;

  rdcarray<Type> m_Types;
  const Type *m_VoidType = NULL;
  const Type *m_BoolType = NULL;

  rdcarray<Attributes> m_AttributeGroups;
  rdcarray<Attributes> m_Attributes;

  rdcarray<Constant> m_Constants;

  rdcarray<Metadata> m_Metadata;
  rdcarray<NamedMetadata> m_NamedMeta;
  rdcarray<Metadata *> m_NumberedMeta;
  uint32_t m_NextMetaID = 0;

  rdcarray<DebugLocation> m_DebugLocations;

  rdcstr m_Triple, m_Datalayout;

  rdcstr m_Disassembly;

  friend struct OpReader;
};

bool needsEscaping(const rdcstr &name);
rdcstr escapeString(const rdcstr &str);
rdcstr escapeStringIfNeeded(const rdcstr &name);

};    // namespace DXIL

DECLARE_REFLECTION_ENUM(DXIL::Attribute);
DECLARE_STRINGISE_TYPE(DXIL::InstructionFlags);
