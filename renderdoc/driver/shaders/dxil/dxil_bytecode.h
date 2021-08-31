/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2021 Baldur Karlsson
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

// undef some annoying defines that might come from OS headers
#undef VOID
#undef FLOAT
#undef LABEL
#undef OPAQUE

namespace LLVMBC
{
struct BlockOrRecord;
};

namespace DXIL
{
struct ProgramHeader
{
  uint16_t ProgramVersion;
  uint16_t ProgramType;
  uint32_t SizeInUint32;     // Size in uint32_t units including this header.
  uint32_t DxilMagic;        // 0x4C495844, ASCII "DXIL".
  uint32_t DxilVersion;      // DXIL version.
  uint32_t BitcodeOffset;    // Offset to LLVM bitcode (from DxilMagic).
  uint32_t BitcodeSize;      // Size of LLVM bitcode.
};

enum class KnownBlocks : uint32_t
{
  BLOCKINFO = 0,

  // 1-7 reserved,

  MODULE_BLOCK = 8,
  PARAMATTR_BLOCK = 9,
  PARAMATTR_GROUP_BLOCK = 10,
  CONSTANTS_BLOCK = 11,
  FUNCTION_BLOCK = 12,
  TYPE_SYMTAB_BLOCK = 13,
  VALUE_SYMTAB_BLOCK = 14,
  METADATA_BLOCK = 15,
  METADATA_ATTACHMENT = 16,
  TYPE_BLOCK = 17,
  USELIST_BLOCK = 18,
};

enum class ModuleRecord : uint32_t
{
  VERSION = 1,
  TRIPLE = 2,
  DATALAYOUT = 3,
  SECTIONNAME = 5,
  GLOBALVAR = 7,
  FUNCTION = 8,
  ALIAS = 14,
};

enum class ConstantsRecord : uint32_t
{
  SETTYPE = 1,
  CONST_NULL = 2,
  UNDEF = 3,
  INTEGER = 4,
  FLOAT = 6,
  AGGREGATE = 7,
  STRING = 8,
  CSTRING = 9,
  EVAL_CAST = 11,
  EVAL_GEP = 20,
  DATA = 22,
};

enum class FunctionRecord : uint32_t
{
  DECLAREBLOCKS = 1,
  INST_BINOP = 2,
  INST_CAST = 3,
  INST_GEP_OLD = 4,
  INST_SELECT = 5,
  INST_EXTRACTELT = 6,
  INST_INSERTELT = 7,
  INST_SHUFFLEVEC = 8,
  INST_CMP = 9,
  INST_RET = 10,
  INST_BR = 11,
  INST_SWITCH = 12,
  INST_INVOKE = 13,
  INST_UNREACHABLE = 15,
  INST_PHI = 16,
  INST_ALLOCA = 19,
  INST_LOAD = 20,
  INST_VAARG = 23,
  INST_STORE_OLD = 24,
  INST_EXTRACTVAL = 26,
  INST_INSERTVAL = 27,
  INST_CMP2 = 28,
  INST_VSELECT = 29,
  INST_INBOUNDS_GEP_OLD = 30,
  INST_INDIRECTBR = 31,
  DEBUG_LOC_AGAIN = 33,
  INST_CALL = 34,
  DEBUG_LOC = 35,
  INST_FENCE = 36,
  INST_CMPXCHG_OLD = 37,
  INST_ATOMICRMW = 38,
  INST_RESUME = 39,
  INST_LANDINGPAD_OLD = 40,
  INST_LOADATOMIC = 41,
  INST_STOREATOMIC_OLD = 42,
  INST_GEP = 43,
  INST_STORE = 44,
  INST_STOREATOMIC = 45,
  INST_CMPXCHG = 46,
  INST_LANDINGPAD = 47,
};

enum class ParamAttrRecord : uint32_t
{
  ENTRY = 2,
};

enum class ParamAttrGroupRecord : uint32_t
{
  ENTRY = 3,
};

enum class ValueSymtabRecord : uint32_t
{
  ENTRY = 1,
  BBENTRY = 2,
  FNENTRY = 3,
  COMBINED_ENTRY = 5,
};

enum class MetaDataRecord : uint32_t
{
  STRING_OLD = 1,
  VALUE = 2,
  NODE = 3,
  NAME = 4,
  DISTINCT_NODE = 5,
  KIND = 6,
  LOCATION = 7,
  OLD_NODE = 8,
  OLD_FN_NODE = 9,
  NAMED_NODE = 10,
  ATTACHMENT = 11,
  GENERIC_DEBUG = 12,
  SUBRANGE = 13,
  ENUMERATOR = 14,
  BASIC_TYPE = 15,
  FILE = 16,
  DERIVED_TYPE = 17,
  COMPOSITE_TYPE = 18,
  SUBROUTINE_TYPE = 19,
  COMPILE_UNIT = 20,
  SUBPROGRAM = 21,
  LEXICAL_BLOCK = 22,
  LEXICAL_BLOCK_FILE = 23,
  NAMESPACE = 24,
  TEMPLATE_TYPE = 25,
  TEMPLATE_VALUE = 26,
  GLOBAL_VAR = 27,
  LOCAL_VAR = 28,
  EXPRESSION = 29,
  OBJC_PROPERTY = 30,
  IMPORTED_ENTITY = 31,
  MODULE = 32,
  MACRO = 33,
  MACRO_FILE = 34,
  STRINGS = 35,
  GLOBAL_DECL_ATTACHMENT = 36,
  GLOBAL_VAR_EXPR = 37,
  INDEX_OFFSET = 38,
  INDEX = 39,
  LABEL = 40,
  COMMON_BLOCK = 44,
};

enum class TypeRecord : uint32_t
{
  NUMENTRY = 1,
  VOID = 2,
  FLOAT = 3,
  DOUBLE = 4,
  LABEL = 5,
  OPAQUE = 6,
  INTEGER = 7,
  POINTER = 8,
  FUNCTION_OLD = 9,
  HALF = 10,
  ARRAY = 11,
  VECTOR = 12,
  METADATA = 16,
  STRUCT_ANON = 18,
  STRUCT_NAME = 19,
  STRUCT_NAMED = 20,
  FUNCTION = 21,
};

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

  enum class PointerAddrSpace
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
  ExternalLinkage = 1,
  AvailableExternallyLinkage = 2,
  LinkOnceAnyLinkage = 3,
  LinkOnceODRLinkage = 4,
  WeakAnyLinkage = 5,
  WeakODRLinkage = 6,
  AppendingLinkage = 7,
  InternalLinkage = 8,
  PrivateLinkage = 9,
  ExternalWeakLinkage = 10,
  CommonLinkage = 11,
  LinkageMask = 0xf,
  IsConst = 0x10,
  IsExternal = 0x20,
  LocalUnnamedAddr = 0x40,
  GlobalUnnamedAddr = 0x80,
  IsAppending = 0x100,
  ExternallyInitialised = 0x200,
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

inline Operation DecodeCast(uint64_t opcode)
{
  switch(opcode)
  {
    case 0: return Operation::Trunc; break;
    case 1: return Operation::ZExt; break;
    case 2: return Operation::SExt; break;
    case 3: return Operation::FToU; break;
    case 4: return Operation::FToS; break;
    case 5: return Operation::UToF; break;
    case 6: return Operation::SToF; break;
    case 7: return Operation::FPTrunc; break;
    case 8: return Operation::FPExt; break;
    case 9: return Operation::PtrToI; break;
    case 10: return Operation::IToPtr; break;
    case 11: return Operation::Bitcast; break;
    case 12: return Operation::AddrSpaceCast; break;
    default: RDCERR("Unhandled cast type %llu", opcode); return Operation::Bitcast;
  }
}

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

  uint64_t align = 0;

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
  Program(const Program &o) = delete;
  Program(Program &&o) = delete;
  Program &operator=(const Program &o) = delete;
  virtual ~Program() {}
  static bool Valid(const byte *bytes, size_t length);

  const bytebuf &GetBytes() const { return m_Bytes; }
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

protected:
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
  const Type *GetPointerType(const Type *type, Type::PointerAddrSpace addrSpace) const;

  bytebuf m_Bytes;

  DXBC::ShaderType m_Type;
  uint32_t m_Major, m_Minor;
  uint32_t m_DXILVersion;

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
