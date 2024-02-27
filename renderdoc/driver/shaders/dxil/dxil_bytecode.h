/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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
#include "common/common.h"
#include "driver/dx/official/d3dcommon.h"
#include "driver/shaders/dxbc/dxbc_common.h"

#define DXC_COMPATIBLE_DISASM OPTION_OFF

namespace LLVMBC
{
struct BlockOrRecord;
};

namespace DXIL
{
struct BumpAllocator
{
  BumpAllocator(size_t totalSize);
  ~BumpAllocator();
  void *alloc(size_t sz);

  template <typename T>
  T *alloc()
  {
    void *mem = alloc(sizeof(T));
    return new(mem) T();
  }

private:
  byte *base, *cur;
  rdcarray<byte *> storage;
  size_t m_BlockSize;
};

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

  static void *operator new(size_t count, BumpAllocator &b) { return b.alloc(count); }
  static void operator delete(void *ptr, BumpAllocator &b) {}
  bool isVoid() const { return type == Scalar && scalarType == Void; }
  rdcstr toString() const;
  rdcstr declFunction(rdcstr funcName, const rdcarray<Instruction *> &args,
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

// added as needed, since names in docs/LLVM don't match neatly so there's no pre-made list
enum class DXOp : uint32_t
{
  UMin = 40,
  createHandle = 57,
  atomicBinOp = 78,
  barrier = 80,
  groupId = 94,
  threadIdInGroup = 95,
  flattenedThreadIdInGroup = 96,
  rawBufferLoad = 139,
  rawBufferStore = 140,
  setMeshOutputCounts = 168,
  emitIndices = 169,
  getMeshPayload = 170,
  storeVertexOutput = 171,
  storePrimitiveOutput = 172,
  dispatchMesh = 173,
  annotateHandle = 216,
  createHandleFromBinding = 217,
};

inline Operation DecodeBinOp(const Type *type, uint64_t opcode)
{
  bool isFloatOp = (type->scalarType == Type::Float);

  switch(opcode)
  {
    case 0: return isFloatOp ? Operation::FAdd : Operation::Add; break;
    case 1: return isFloatOp ? Operation::FSub : Operation::Sub; break;
    case 2: return isFloatOp ? Operation::FMul : Operation::Mul; break;
    case 3: return Operation::UDiv; break;
    case 4: return isFloatOp ? Operation::FDiv : Operation::SDiv; break;
    case 5: return Operation::URem; break;
    case 6: return isFloatOp ? Operation::FRem : Operation::SRem; break;
    case 7: return Operation::ShiftLeft; break;
    case 8: return Operation::LogicalShiftRight; break;
    case 9: return Operation::ArithShiftRight; break;
    case 10: return Operation::And; break;
    case 11: return Operation::Or; break;
    case 12: return Operation::Xor; break;
    default: RDCERR("Unhandled binop type %llu", opcode); return Operation::And;
  }
}

inline uint64_t EncodeBinOp(Operation op)
{
  switch(op)
  {
    case Operation::FAdd:
    case Operation::Add: return 0; break;
    case Operation::FSub:
    case Operation::Sub: return 1; break;
    case Operation::FMul:
    case Operation::Mul: return 2; break;
    case Operation::UDiv: return 3; break;
    case Operation::FDiv:
    case Operation::SDiv: return 4; break;
    case Operation::URem: return 5; break;
    case Operation::FRem:
    case Operation::SRem: return 6; break;
    case Operation::ShiftLeft: return 7; break;
    case Operation::LogicalShiftRight: return 8; break;
    case Operation::ArithShiftRight: return 9; break;
    case Operation::And: return 10; break;
    case Operation::Or: return 11; break;
    case Operation::Xor: return 12; break;
    default: return ~0U;
  }
}

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

inline bool IsCast(Operation op)
{
  return EncodeCast(op) != ~0U;
}

enum class ValueKind : uint32_t
{
  ForwardReferencePlaceholder,
  Literal,
  Alias,
  Constant,
  GlobalVar,
  Metadata,
  Instruction,
  Function,
  BasicBlock,
};

struct Type;

struct PlaceholderValue;

struct Value
{
  const Type *type = NULL;

  // the ID for this value (two namespaces: values, and metadata)
  // note, not all derived classes from Value have an id - e.g. void instructions, blocks, these
  // don't have IDs because they don't go in the values array
  //
  // this ID is very close but different to the number displayed in disassembly. This ID is only
  // used internally for encoding
  static constexpr uint32_t NoID = 0x00ffffff;
  // these IDs are used during enumeration to count values which we have or haven't seen before
  static constexpr uint32_t UnvisitedID = 0x00fffffe;
  static constexpr uint32_t VisitedID = 0x00fffffd;
  uint32_t id : 24;

  rdcstr toString(bool withType = false) const;

  static void *operator new(size_t count, BumpAllocator &b) { return b.alloc(count); }
  static void operator delete(void *ptr, BumpAllocator &b) {}
  static void *operator new(size_t count, PlaceholderValue *v) { return v; }
  static void operator delete(void *ptr, PlaceholderValue *v) {}
  static void operator delete(void *ptr) {}
  ValueKind kind() const { return valKind; }
protected:
  Value(ValueKind k) : valKind(k), id(NoID) {}
  ValueKind valKind : 8;
  uint32_t flags = 0;
};

struct PlaceholderValue : public Value
{
private:
  friend struct ValueList;

  PlaceholderValue() : Value(ValueKind::ForwardReferencePlaceholder) {}
  byte storage[64 - sizeof(Value)];
};

// helper class to check at compile time that values will be able to be forward referenced as
// we conservatively allocate room for them.
template <typename T>
struct ForwardReferencableValue : public Value
{
  static constexpr bool IsForwardReferenceable = true;

protected:
  ForwardReferencableValue(ValueKind k) : Value(k)
  {
    RDCCOMPILE_ASSERT(sizeof(T) <= sizeof(PlaceholderValue),
                      "Type is too large to be forward referenceable in-place");
  }
};

// loose wrapper around an array for value pointers. This doesn't use the underlying array size for
// anything but instead tracks the current latest value, so relative indices can be resolved against
// it while we can still future-allocate in the array for forward reference placeholders.
struct ValueList : private rdcarray<Value *>
{
  ValueList(BumpAllocator &alloc) : rdcarray(), alloc(alloc) {}
  Value *&operator[](size_t i)
  {
    resize_for_index(i);
    RDCASSERT(at(i));
    return at(i);
  }
  void hintExpansion(size_t newValues) { reserve(lastValue + newValues); }
  void beginFunction() { functionWatermark = lastValue; }
  void endFunction()
  {
    resize(functionWatermark);
    lastValue = functionWatermark;
  }
  size_t curValueIndex() const { return lastValue; }
  Value *createPlaceholderValue(size_t i)
  {
    resize_for_index(i);
    Value *ret = at(i);
    if(ret)
    {
      RDCASSERT(ret->kind() == ValueKind::ForwardReferencePlaceholder);
      return ret;
    }
    at(i) = ret = new(alloc) PlaceholderValue();
    return ret;
  }
  Value *getOrCreatePlaceholder(size_t i)
  {
    resize_for_index(i);
    Value *ret = at(i);
    if(ret)
      return ret;
    return createPlaceholderValue(i);
  }
  // alloc a value in-place to resolve forward references. Should always be paired with a call to
  // addAllocedValue() once you're done creating the value
  // if you're creating a 'Value' that isn't actually a value (e.g. instruction with no return
  // value) then you can new it directly
  template <typename T>
  T *nextValue()
  {
    RDCASSERT(!pendingValue);
    RDCCOMPILE_ASSERT(typename T::IsForwardReferenceable,
                      "alloc'ing next value for non-forward-referenceable type");

    pendingValue = true;

    // if this value was already allocated as a placeholder, re-use the memory to keep any pointers
    // to it valid
    if(lastValue < size() && at(lastValue))
    {
      Value *v = at(lastValue);
      RDCASSERT(v->kind() == ValueKind::ForwardReferencePlaceholder);
      PlaceholderValue *memory = (PlaceholderValue *)v;
      return new(memory) T();
    }

    // no placeholder, put it in place now and addValue() will be called to bump the count below
    resize_for_index(lastValue);
    T *ret = new(alloc) T();
    at(lastValue) = ret;
    return ret;
  }
  void addValue(Value *v = NULL)
  {
    // either pendingValue should be true, or v should be true, but they shouldn't both be
    if(pendingValue)
    {
      RDCASSERT(v == NULL);
      pendingValue = false;
      lastValue++;
    }
    else
    {
      RDCASSERT(v);
      v->id = lastValue;
      resize_for_index(lastValue);
      RDCASSERTMSG("Forward reference being overwritten with new pointer", at(lastValue) == NULL);
      at(lastValue) = v;
      lastValue++;
    }
  }
  size_t getRelativeBackwards(uint64_t ref) { return lastValue - (size_t)ref; }
  size_t getRelativeForwards(uint64_t ref) { return lastValue + (size_t)ref; }
private:
  BumpAllocator &alloc;
  size_t functionWatermark = 0;
  size_t lastValue = 0;
  bool pendingValue = false;
};

template <typename T>
T *cast(Value *v)
{
  if(v && v->kind() == T::Kind)
    return (T *)v;
  return NULL;
}

template <typename T>
const T *cast(const Value *v)
{
  if(v && v->kind() == T::Kind)
    return (const T *)v;
  return NULL;
}

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

struct Literal : public ForwardReferencableValue<Literal>
{
  static constexpr ValueKind Kind = ValueKind::Literal;
  Literal(uint64_t v) : ForwardReferencableValue(Kind), literal(v) {}
  uint64_t literal;
};

struct Alias : public ForwardReferencableValue<Alias>
{
  static constexpr ValueKind Kind = ValueKind::Alias;
  Alias() : ForwardReferencableValue(Kind) {}
  rdcstr name;
  Value *val = NULL;
};

struct Constant : public ForwardReferencableValue<Constant>
{
  static constexpr ValueKind Kind = ValueKind::Constant;
  Constant() : ForwardReferencableValue(Kind) { u64 = 0; }
  Constant(const Type *t, uint32_t v) : ForwardReferencableValue(ValueKind::Constant)
  {
    type = t;
    u64 = 0;
    setValue(v);
  }
  Operation op = Operation::NoOp;
  rdcstr str;
  // used during encoding to sort constants by number of uses...
  uint32_t refCount = 0;

  bool isUndef() const { return (flags & 0x1) != 0; }
  bool isNULL() const { return (flags & 0x2) != 0; }
  bool isData() const { return (flags & 0x4) != 0; }
  bool isLiteral() const { return (flags & 0x10) != 0; }
  bool isShaderVal() const { return (flags & 0x20) != 0; }
  bool isCast() const { return (flags & 0x40) != 0; }
  bool isCompound() const { return (flags & 0x80) != 0; }
  void setUndef(bool u)
  {
    flags &= ~0x1;
    flags |= u ? 0x1 : 0x0;
  }
  void setNULL(bool n)
  {
    flags &= ~0x2;
    flags |= n ? 0x2 : 0x0;
  }
  void setData(bool d)
  {
    flags &= ~0x4;
    flags |= d ? 0x4 : 0x0;
  }
  void setValue(uint32_t l)
  {
    flags &= ~0xf0;
    flags |= 0x10;
    u32 = l;
  }
  void setValue(uint64_t l)
  {
    flags &= ~0xf0;
    flags |= 0x10;
    u64 = l;
  }
  void setValue(int64_t l)
  {
    flags &= ~0xf0;
    flags |= 0x10;
    s64 = l;
  }
  void setValue(BumpAllocator &alloc, const ShaderValue &v)
  {
    flags &= ~0xf0;
    flags |= 0x20;
    val = alloc.alloc<ShaderValue>();
    *val = v;
  }
  void setInner(Value *i)
  {
    flags &= ~0xf0;
    flags |= 0x40;
    inner = i;
  }
  void setCompound(BumpAllocator &alloc, rdcarray<Value *> &&m)
  {
    flags &= ~0xf0;
    flags |= 0x80;
    members = alloc.alloc<rdcarray<Value *>>();
    new(members) rdcarray<Value *>(m);
  }
  void setCompound(BumpAllocator &alloc, const rdcarray<Value *> &m)
  {
    flags &= ~0xf0;
    flags |= 0x80;
    members = alloc.alloc<rdcarray<Value *>>();
    new(members) rdcarray<Value *>(m);
  }

  uint32_t getU32() const
  {
    if(flags & 0x10)
      return u32;
    // silently return 0 for NULL/Undef constants
    if(flags & 0x03)
      return 0U;
    RDCERR("Wrong type of constant being accessed");
    return 0U;
  }

  uint64_t getU64() const
  {
    if(flags & 0x10)
      return u64;
    // silently return 0 for NULL/Undef constants
    if(flags & 0x03)
      return 0U;
    RDCERR("Wrong type of constant being accessed");
    return 0U;
  }

  int64_t getS64() const
  {
    if(flags & 0x10)
      return s64;
    // silently return 0 for NULL/Undef constants
    if(flags & 0x03)
      return 0;
    RDCERR("Wrong type of constant being accessed");
    return 0U;
  }

  const ShaderValue &getShaderVal() const
  {
    if(flags & 0x20)
      return *val;
    static ShaderValue empty;
    RDCERR("Wrong type of constant being accessed");
    return empty;
  }

  Value *getInner() const
  {
    if(flags & 0x40)
      return inner;
    RDCERR("No inner available");
    return NULL;
  }

  const rdcarray<Value *> &getMembers() const
  {
    if(flags & 0x80)
      return *members;
    static rdcarray<Value *> empty;
    RDCERR("No members available");
    return empty;
  }

  rdcstr toString(bool withType = false) const;

private:
  union
  {
    uint64_t u64;
    int64_t s64;
    uint32_t u32;
    ShaderValue *val;
    Value *inner;
    rdcarray<Value *> *members;
  };
};

struct GlobalVar : public ForwardReferencableValue<GlobalVar>
{
  static constexpr ValueKind Kind = ValueKind::GlobalVar;
  GlobalVar() : ForwardReferencableValue(Kind) {}
  rdcstr name;
  uint64_t align = 0;
  int32_t section = -1;
  GlobalFlags flags = GlobalFlags::NoFlags;
  const Constant *initialiser = NULL;
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
    Namespace,
    ImportedEntity,
    Enum,
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

struct Metadata;

struct DebugLocation
{
  uint32_t slot = ~0U;

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

struct Metadata : public Value
{
  static constexpr ValueKind Kind = ValueKind::Metadata;
  Metadata(size_t idx = 0xffffff) : Value(Kind) { id = idx; }
  ~Metadata();

  bool isDistinct = false, isConstant = false, isString = false;

  // only used for disassembly, the number given to metadata that's directly referenced. NOT the
  // same as it's id (ha ha)
  uint32_t slot = ~0U;

  Value *value = NULL;

  rdcstr str;
  rdcarray<Metadata *> children;
  DIBase *dwarf = NULL;
  DebugLocation *debugLoc = NULL;

  rdcstr refString() const;
  rdcstr valString() const;
};

// loose wrapper around an array for metadata pointer. This creates metadata nodes on demand because
// they can be forward referenced (sigh...)
struct MetadataList : private rdcarray<Metadata *>
{
  MetadataList(BumpAllocator &alloc) : rdcarray(), alloc(&alloc) {}
  MetadataList() : rdcarray(), alloc(NULL) {}
  Metadata *&operator[](size_t i)
  {
    resize_for_index(i);
    if(at(i))
      return at(i);
    RDCASSERT(alloc);
    at(i) = new(*alloc) Metadata(i);
    return at(i);
  }
  void hintExpansion(size_t newValues) { reserve(size() + newValues); }
  void beginFunction() { functionWatermark = size(); }
  void endFunction() { resize(functionWatermark); }
  using rdcarray<Metadata *>::size;
  using rdcarray<Metadata *>::back;
  using rdcarray<Metadata *>::empty;
  using rdcarray<Metadata *>::contains;

  Metadata *getDirect(uint64_t id) { return (*this)[size_t(id)]; }
  Metadata *getOrNULL(uint64_t id) { return id ? (*this)[size_t(id - 1)] : NULL; }
  rdcstr *getStringOrNULL(uint64_t id) { return id ? &(*this)[size_t(id - 1)]->str : NULL; }
private:
  BumpAllocator *alloc = NULL;
  size_t functionWatermark = 0;
  size_t lastValue = 0;
  bool pendingValue = false;
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

// pair of <kind, Metadata>
typedef rdcarray<rdcpair<uint64_t, Metadata *>> AttachedMetadata;

struct Function;

struct Instruction : public ForwardReferencableValue<Instruction>
{
  static constexpr ValueKind Kind = ValueKind::Instruction;
  Instruction() : ForwardReferencableValue(Kind) {}
  uint32_t disassemblyLine = 0;
  uint32_t debugLoc = ~0U;
  Operation op = Operation::NoOp;
  uint8_t align = 0;
  // number assigned to instructions that don't have names and return a value, for disassembly
  uint32_t slot = ~0U;
  InstructionFlags &opFlags() { return (InstructionFlags &)flags; }
  InstructionFlags opFlags() const { return (InstructionFlags)flags; }
  rdcarray<Value *> args;

  struct ExtraInstructionInfo
  {
    rdcstr name;
    AttachedMetadata attachedMeta;

    // function calls
    const AttributeSet *paramAttrs = NULL;
    const Function *funcCall = NULL;
  };

  const rdcstr &getName() const
  {
    static rdcstr empty;
    if(extraData)
      return extraData->name;
    return empty;
  }

  const AttachedMetadata &getAttachedMeta() const
  {
    static AttachedMetadata empty;
    if(extraData)
      return extraData->attachedMeta;
    return empty;
  }

  const AttributeSet *getParamAttrs() const
  {
    if(extraData)
      return extraData->paramAttrs;
    return NULL;
  }

  const Function *getFuncCall() const
  {
    if(extraData)
      return extraData->funcCall;
    return NULL;
  }

  ExtraInstructionInfo &extra(BumpAllocator &alloc)
  {
    if(!extraData)
      extraData = alloc.alloc<ExtraInstructionInfo>();
    return *extraData;
  }

private:
  ExtraInstructionInfo *extraData = NULL;
};

struct Block : public ForwardReferencableValue<Block>
{
  static constexpr ValueKind Kind = ValueKind::BasicBlock;
  Block(const Type *labelType) : ForwardReferencableValue(Kind) { type = labelType; }
  rdcinflexiblestr name;
  rdcarray<const Block *> preds;
  uint32_t slot = ~0U;
};

struct UselistEntry
{
  bool block = false;
  Value *value;
  rdcarray<uint64_t> shuffle;
};

// functions are deliberately not forward referenceable since they're larger, and we shouldn't need
// to
struct Function : public Value
{
  static constexpr ValueKind Kind = ValueKind::Function;
  Function() : Value(Kind) {}
  rdcstr name;

  bool external = false;
  bool internalLinkage = false;
  bool sortedSymtab = true;
  uint32_t comdatIdx = ~0U;
  const AttributeSet *attrs = NULL;

  uint64_t align = 0;

  rdcarray<Instruction *> args;
  rdcarray<Instruction *> instructions;

  rdcarray<Value *> valueSymtabOrder;

  rdcarray<Block *> blocks;

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
  uint32_t GetDirectHeapAcessCount() const { return m_directHeapAccessCount; }
protected:
  void MakeDisassemblyString();

  void ParseConstant(ValueList &values, const LLVMBC::BlockOrRecord &constant);
  bool ParseDebugMetaRecord(MetadataList &metadata, const LLVMBC::BlockOrRecord &metaRecord,
                            Metadata &meta);
  rdcstr GetDebugVarName(const DIBase *d);
  rdcstr GetFunctionScopeName(const DIBase *d);

  rdcstr GetValueSymtabString(Value *v);
  void SetValueSymtabString(Value *v, const rdcstr &s);

  uint32_t GetOrAssignMetaSlot(rdcarray<Metadata *> &metaSlots, uint32_t &nextMetaSlot, Metadata *m);
  uint32_t GetOrAssignMetaSlot(rdcarray<Metadata *> &metaSlots, uint32_t &nextMetaSlot,
                               DebugLocation &l);

  const Type *GetVoidType() { return m_VoidType; }
  const Type *GetBoolType() { return m_BoolType; }
  const Type *GetInt32Type() { return m_Int32Type; }
  const Type *GetInt8Type() { return m_Int8Type; }
  const Type *GetPointerType(const Type *type, Type::PointerAddrSpace addrSpace);

  bytebuf m_Bytes;

  BumpAllocator alloc;

  DXBC::ShaderType m_Type;
  uint32_t m_Major, m_Minor;
  uint32_t m_DXILVersion;

  rdcstr m_CompilerSig, m_EntryPoint, m_Profile;
  ShaderCompileFlags m_CompileFlags;

  const Type *m_CurParseType = NULL;

  rdcarray<GlobalVar *> m_GlobalVars;
  rdcarray<Function *> m_Functions;
  rdcarray<Alias *> m_Aliases;
  rdcarray<rdcstr> m_Sections;
  rdcarray<rdcpair<uint64_t, rdcstr>> m_Comdats;
  uint32_t m_directHeapAccessCount = 0;

  rdcarray<rdcstr> m_Kinds;

  rdcarray<Value *> m_ValueSymtabOrder;
  bool m_SortedSymtab = true;

  rdcarray<Type *> m_Types;
  const Type *m_VoidType = NULL;
  const Type *m_BoolType = NULL;
  const Type *m_Int32Type = NULL;
  const Type *m_Int8Type = NULL;
  const Type *m_MetaType = NULL;
  const Type *m_LabelType = NULL;

  rdcarray<AttributeGroup *> m_AttributeGroups;
  rdcarray<AttributeSet *> m_AttributeSets;

  rdcarray<NamedMetadata *> m_NamedMeta;

  rdcarray<DebugLocation> m_DebugLocations;

  bool m_Uselists = false;

  rdcstr m_Triple, m_Datalayout;

  rdcstr m_Disassembly;

  friend struct OpReader;
  friend class LLVMOrderAccumulator;
};

bool needsEscaping(const rdcstr &name);
rdcstr escapeString(const rdcstr &str);
rdcstr escapeStringIfNeeded(const rdcstr &name);

class LLVMOrderAccumulator
{
public:
  // types in id order
  rdcarray<const Type *> types;
  // types in disassembly print order
  rdcarray<const Type *> printOrderTypes;
  // values in id order
  rdcarray<const Value *> values;
  // metadata in id order
  rdcarray<const Metadata *> metadata;

  size_t firstConst;
  size_t numConsts;

  void processGlobals(Program *p, bool doLiveChecking);

  size_t firstFuncConst;
  size_t numFuncConsts;

  void processFunction(Function *f);
  void exitFunction();

private:
  size_t functionWaterMark;
  bool sortConsts = true;
  bool liveChecking = false;

  void reset(GlobalVar *g);
  void reset(Alias *a);
  void reset(Constant *c);
  void reset(Metadata *m);
  void reset(Function *f);
  void reset(Block *b);
  void reset(Instruction *i);
  void reset(Value *v);

  void accumulate(const Value *v);
  void accumulate(const Metadata *m);
  void accumulateTypePrintOrder(const Type *t);
  void accumulateTypePrintOrder(rdcarray<const Metadata *> &visited, const Metadata *m);
  void assignTypeId(const Type *t);
  void assignTypeId(const Constant *c);
};

};    // namespace DXIL

DECLARE_REFLECTION_ENUM(DXIL::Attribute);
DECLARE_STRINGISE_TYPE(DXIL::InstructionFlags);
