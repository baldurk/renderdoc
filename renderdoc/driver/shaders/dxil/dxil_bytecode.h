/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

#define DXC_COMPATIBLE_DISASM OPTION_OFF

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

struct Instruction;
struct AttributeSet;

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
  rdcstr declFunction(rdcstr funcName, const rdcarray<Instruction> &args,
                      const AttributeSet *attrs) const;

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
  bool packedStruct = false, vararg = false, opaque = false;
  uint16_t id = 0xFFFF;
  rdcstr name;
  rdcarray<const Type *> members;    // the members for a struct, the parameters for functions
};

enum class ValueType
{
  Unknown,
  Function,
  GlobalVar,
  Alias,
  Constant,
  Instruction,
  Metadata,
  Literal,
  BasicBlock,
};

struct Function;
struct GlobalVar;
struct Alias;
struct Constant;
struct Instruction;
struct Metadata;
struct Block;
struct Type;

struct Value
{
  enum ForwardRefTag
  {
    ForwardRef
  };
  Value() : type(ValueType::Unknown), literal(0) {}
  explicit Value(ForwardRefTag, const Value *value) : type(ValueType::Unknown), value(value) {}
  explicit Value(const Function *function) : type(ValueType::Function), function(function) {}
  explicit Value(const GlobalVar *global) : type(ValueType::GlobalVar), global(global) {}
  explicit Value(const Alias *alias) : type(ValueType::Alias), alias(alias) {}
  explicit Value(const Constant *constant) : type(ValueType::Constant), constant(constant) {}
  explicit Value(const Instruction *instruction)
      : type(ValueType::Instruction), instruction(instruction)
  {
  }
  explicit Value(const Metadata *meta) : type(ValueType::Metadata), meta(meta) {}
  explicit Value(const Block *block) : type(ValueType::BasicBlock), block(block) {}
  explicit Value(uint64_t literal) : type(ValueType::Literal), literal(literal) {}
  bool empty() const { return type == ValueType::Unknown && literal == 0; }
  const Type *GetType() const;
  rdcstr toString(bool withType = false) const;

  bool operator==(const Value &o) { return type == o.type && literal == o.literal; }
  ValueType type;
  union
  {
    const Value *value;
    const Function *function;
    const GlobalVar *global;
    const Alias *alias;
    const Constant *constant;
    const Instruction *instruction;
    const Metadata *meta;
    const Block *block;
    uint64_t literal;
  };
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
  const Constant *initialiser = NULL;
};

struct Alias
{
  rdcstr name;
  const Type *type = NULL;
  Value val;
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

struct AttributeGroup
{
  enum Slot : uint64_t
  {
    InvalidSlot = ~0U - 1,
    FunctionSlot = ~0U,
    ReturnSlot = 0U,
    Param1Slot = 1U,
  };

  uint32_t slotIndex = InvalidSlot;

  Attribute params = Attribute::None;
  uint64_t align = 0, stackAlign = 0, derefBytes = 0, derefOrNullBytes = 0;
  rdcarray<rdcpair<rdcstr, rdcstr>> strs;

  rdcstr toString(bool stringAttrs) const;
};

struct AttributeSet
{
  const AttributeGroup *functionSlot = NULL;
  rdcarray<const AttributeGroup *> groupSlots;

  rdcarray<uint64_t> orderedGroups;
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

inline uint64_t EncodeCast(Operation op)
{
  switch(op)
  {
    case Operation::Trunc: return 0; break;
    case Operation::ZExt: return 1; break;
    case Operation::SExt: return 2; break;
    case Operation::FToU: return 3; break;
    case Operation::FToS: return 4; break;
    case Operation::UToF: return 5; break;
    case Operation::SToF: return 6; break;
    case Operation::FPTrunc: return 7; break;
    case Operation::FPExt: return 8; break;
    case Operation::PtrToI: return 9; break;
    case Operation::IToPtr: return 10; break;
    case Operation::Bitcast: return 11; break;
    case Operation::AddrSpaceCast: return 12; break;
    default: return ~0U;
  }
}

struct Constant
{
  Constant() = default;
  Constant(const Type *t, uint32_t v) : type(t) { val.u32v[0] = v; }
  const Type *type = NULL;
  ShaderValue val = {};
  rdcarray<Value> members;
  Value inner;
  rdcstr str;
  bool undef = false, nullconst = false, data = false;
  Operation op = Operation::NoOp;

  // steal the last unused part of ShaderValue, used for identifying constants under patching
  uint32_t getID() { return val.u32v[15]; }
  void setID(uint32_t id) { val.u32v[15] = id; }
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
  bool isDistinct = false, isConstant = false, isString = false;

  Value value;

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

  ArgumentAlloca = 1 << 18,
};

BITMASK_OPERATORS(InstructionFlags);

typedef rdcarray<rdcpair<uint64_t, Metadata *>> AttachedMetadata;

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
  rdcarray<Value> args;
  AttachedMetadata attachedMeta;

  // function calls
  const AttributeSet *paramAttrs = NULL;
  const Function *funcCall = NULL;
};

struct Block
{
  uint32_t resultID = ~0U;
  rdcstr name;
  rdcarray<const Block *> preds;
};

struct UselistEntry
{
  bool block = false;
  Value value;
  rdcarray<uint64_t> shuffle;
};

struct Function
{
  rdcstr name;

  const Type *funcType = NULL;
  bool external = false;
  const AttributeSet *attrs = NULL;

  uint64_t align = 0;

  rdcarray<Instruction> args;
  rdcarray<Instruction> instructions;
  rdcarray<Value> values;

  rdcarray<Value> valueSymtabOrder;
  bool sortedSymtab = true;

  rdcarray<Block> blocks;
  rdcarray<Constant> constants;
  rdcarray<Metadata> metadata;

  rdcarray<UselistEntry> uselist;

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

  DXBC::ShaderType GetShaderType() const { return m_Type; }
  uint32_t GetMajorVersion() const { return m_Major; }
  uint32_t GetMinorVersion() const { return m_Minor; }
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

  const Metadata *GetMetadataByName(const rdcstr &name) const;
  size_t GetMetadataCount() const { return m_Metadata.size() + m_NamedMeta.size(); }
  uint32_t GetDirectHeapAcessCount() const { return m_directHeapAccessCount; }
protected:
  void MakeDisassemblyString();

  bool ParseDebugMetaRecord(const LLVMBC::BlockOrRecord &metaRecord, Metadata &meta);
  rdcstr GetDebugVarName(const DIBase *d);
  rdcstr GetFunctionScopeName(const DIBase *d);

  rdcstr &GetValueSymtabString(const Value &v);

  uint32_t GetOrAssignMetaID(Metadata *m);
  uint32_t GetOrAssignMetaID(DebugLocation &l);
  const Type *GetVoidType(bool precache = false);
  const Type *GetBoolType(bool precache = false);
  const Type *GetInt32Type(bool precache = false);
  const Type *GetInt8Type();
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
  rdcarray<Value> m_Values;
  rdcarray<rdcstr> m_Sections;
  uint32_t m_directHeapAccessCount = 0;

  rdcarray<rdcstr> m_Kinds;

  rdcarray<Value> m_ValueSymtabOrder;
  bool m_SortedSymtab = true;

  rdcarray<Type> m_Types;
  const Type *m_VoidType = NULL;
  const Type *m_BoolType = NULL;
  const Type *m_Int32Type = NULL;
  const Type *m_Int8Type = NULL;

  rdcarray<AttributeGroup> m_AttributeGroups;
  rdcarray<AttributeSet> m_AttributeSets;

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
