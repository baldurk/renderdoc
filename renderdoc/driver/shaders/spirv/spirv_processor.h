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
#include <map>
#include <set>
#include "api/replay/rdcarray.h"
#include "api/replay/rdcpair.h"
#include "api/replay/rdcstr.h"
#include "api/replay/replay_enums.h"
#include "api/replay/shader_types.h"
#include "common/common.h"
#include "spirv_common.h"

namespace half_float
{
class half;
};

namespace rdcspv
{
struct Scalar
{
  Scalar() : type(Op::Max), width(0), signedness(false) {}
  constexpr Scalar(Op t, uint32_t w, bool s) : type(t), width(w), signedness(s) {}
  Scalar(Iter op);

  Op type;
  uint32_t width;
  bool signedness;

  bool operator<(const Scalar &o) const
  {
    if(type != o.type)
      return type < o.type;
    if(signedness != o.signedness)
      return signedness < o.signedness;
    return width < o.width;
  }

  bool operator!=(const Scalar &o) const { return !operator==(o); }
  bool operator==(const Scalar &o) const
  {
    return type == o.type && width == o.width && signedness == o.signedness;
  }

  VarType Type() const
  {
    if(type == Op::TypeFloat)
    {
      if(width == 32)
        return VarType::Float;
      else if(width == 16)
        return VarType::Half;
      else if(width == 64)
        return VarType::Double;
    }
    else if(type == Op::TypeInt)
    {
      if(width == 32)
        return signedness ? VarType::SInt : VarType::UInt;
      else if(width == 16)
        return signedness ? VarType::SShort : VarType::UShort;
      else if(width == 64)
        return signedness ? VarType::SLong : VarType::ULong;
      else if(width == 8)
        return signedness ? VarType::SByte : VarType::UByte;
    }
    else if(type == Op::TypeBool)
    {
      return VarType::Bool;
    }

    return VarType::Unknown;
  }
};

// helper to create Scalar objects for known types
template <typename T>
inline constexpr Scalar scalar();

Scalar scalar(VarType t);

#define SCALAR_TYPE(ctype, op, width, sign) \
  template <>                               \
  inline constexpr Scalar scalar<ctype>()   \
  {                                         \
    return Scalar(op, width, sign);         \
  }

SCALAR_TYPE(void, Op::TypeVoid, 0, false);
SCALAR_TYPE(bool, Op::TypeBool, 32, false);
SCALAR_TYPE(uint8_t, Op::TypeInt, 8, false);
SCALAR_TYPE(uint16_t, Op::TypeInt, 16, false);
SCALAR_TYPE(uint32_t, Op::TypeInt, 32, false);
SCALAR_TYPE(uint64_t, Op::TypeInt, 64, false);
SCALAR_TYPE(int8_t, Op::TypeInt, 8, true);
SCALAR_TYPE(int16_t, Op::TypeInt, 16, true);
SCALAR_TYPE(int32_t, Op::TypeInt, 32, true);
SCALAR_TYPE(int64_t, Op::TypeInt, 64, true);
SCALAR_TYPE(float, Op::TypeFloat, 32, false);
SCALAR_TYPE(double, Op::TypeFloat, 64, false);
SCALAR_TYPE(half_float::half, Op::TypeFloat, 16, false);

struct Vector
{
  Vector() : scalar(), count(0) {}
  Vector(const Scalar &s, uint32_t c) : scalar(s), count(c) {}
  Scalar scalar;
  uint32_t count;

  bool operator<(const Vector &o) const
  {
    if(scalar != o.scalar)
      return scalar < o.scalar;
    return count < o.count;
  }

  bool operator!=(const Vector &o) const { return !operator==(o); }
  bool operator==(const Vector &o) const { return scalar == o.scalar && count == o.count; }
};

struct Matrix
{
  Matrix() : vector(), count(0) {}
  Matrix(const Vector &v, uint32_t c) : vector(v), count(c) {}
  Vector vector;
  uint32_t count;

  bool operator<(const Matrix &o) const
  {
    if(vector != o.vector)
      return vector < o.vector;
    return count < o.count;
  }

  bool operator!=(const Matrix &o) const { return !operator==(o); }
  bool operator==(const Matrix &o) const { return vector == o.vector && count == o.count; }
};

struct Pointer
{
  Pointer() : baseId(), storage(StorageClass::Max) {}
  Pointer(Id b, StorageClass s) : baseId(b), storage(s) {}
  Id baseId;
  StorageClass storage;

  bool operator<(const Pointer &o) const
  {
    if(baseId != o.baseId)
      return baseId < o.baseId;
    return storage < o.storage;
  }

  bool operator!=(const Pointer &o) const { return !operator==(o); }
  bool operator==(const Pointer &o) const { return baseId == o.baseId && storage == o.storage; }
};

struct Image
{
  Image()
      : retType(), dim(Dim::Max), depth(0), arrayed(0), ms(0), sampled(0), format(ImageFormat::Max)
  {
  }
  Image(Scalar ret, Dim d, uint32_t dp, uint32_t ar, uint32_t m, uint32_t samp, ImageFormat f)
      : retType(ret), dim(d), depth(dp), arrayed(ar), ms(m), sampled(samp), format(f)
  {
  }

  Scalar retType;
  Dim dim;
  uint32_t depth;
  uint32_t arrayed;
  uint32_t ms;
  uint32_t sampled;
  ImageFormat format;

  bool operator<(const Image &o) const
  {
    if(retType != o.retType)
      return retType < o.retType;
    if(dim != o.dim)
      return dim < o.dim;
    if(depth != o.depth)
      return depth < o.depth;
    if(arrayed != o.arrayed)
      return arrayed < o.arrayed;
    if(ms != o.ms)
      return ms < o.ms;
    if(sampled != o.sampled)
      return sampled < o.sampled;
    return format < o.format;
  }
  bool operator!=(const Image &o) const { return !operator==(o); }
  bool operator==(const Image &o) const
  {
    return retType == o.retType && dim == o.dim && depth == o.depth && arrayed == o.arrayed &&
           ms == o.ms && sampled == o.sampled && format == o.format;
  }
};

struct Sampler
{
  // no properties, all sampler types are equal
  bool operator<(const Sampler &o) const { return false; }
  bool operator!=(const Sampler &o) const { return false; }
  bool operator==(const Sampler &o) const { return true; }
};

struct SampledImage
{
  SampledImage() = default;
  SampledImage(Id b) : baseId(b) {}
  Id baseId;

  bool operator<(const SampledImage &o) const { return baseId < o.baseId; }
  bool operator!=(const SampledImage &o) const { return !operator==(o); }
  bool operator==(const SampledImage &o) const { return baseId == o.baseId; }
};

struct FunctionType
{
  FunctionType() = default;
  FunctionType(Id ret, const rdcarray<Id> &args) : returnId(ret), argumentIds(args) {}
  Id returnId;
  rdcarray<Id> argumentIds;

  bool operator<(const FunctionType &o) const
  {
    if(returnId != o.returnId)
      return returnId < o.returnId;
    return argumentIds < o.argumentIds;
  }

  bool operator!=(const FunctionType &o) const { return !operator==(o); }
  bool operator==(const FunctionType &o) const
  {
    return returnId == o.returnId && argumentIds == o.argumentIds;
  }
};

struct Decorations
{
  // common ones directly
  enum Flags
  {
    NoFlags = 0,
    Block = 0x1,
    BufferBlock = 0x2,
    RowMajor = 0x4,
    ColMajor = 0x8,
    Restrict = 0x10,
    Aliased = 0x20,

    // which packed decorations have been set
    HasLocation = 0x01000000,
    HasArrayStride = 0x02000000,
    HasDescriptorSet = 0x04000000,
    HasOffset = 0x08000000,
    HasBuiltIn = 0x10000000,
    HasBinding = 0x20000000,
    HasMatrixStride = 0x40000000,
    HasSpecId = 0x80000000,
  };
  Flags flags = NoFlags;

  bool HasDecorations() const { return flags != NoFlags || !others.empty(); }
  // to save some space we union things that can't be used on the same type of object
  union
  {
    uint32_t location = ~0U;    // only valid on variables
    uint32_t arrayStride;       // only valid on types
  };

  union
  {
    uint32_t set = ~0U;    // only valid on global variables
    uint32_t offset;       // only valid on struct members
  };

  union
  {
    BuiltIn builtIn = BuiltIn::Invalid;    // only valid on builtins
    uint32_t binding;    // only valid on binding objects (images, samplers, buffers)
  };

  union
  {
    uint32_t matrixStride = ~0U;    // only valid on matrices
    uint32_t specID;                // only valid on scalars
  };

  // others in an array
  rdcarray<DecorationAndParamData> others;

  void Register(const DecorationAndParamData &decoration);
  void Unregister(const DecorationAndParamData &decoration);
};

struct DataType
{
  enum Type
  {
    UnknownType,
    ScalarType,
    VectorType,
    MatrixType,
    StructType,
    PointerType,
    ArrayType,
    ImageType,
    SamplerType,
    SampledImageType,
    RayQueryType,
    AccelerationStructureType,
  };

  struct Child
  {
    Id type;
    rdcstr name;
    Decorations decorations;
  };

  DataType() = default;
  DataType(Id i, Id inner, const Matrix &m)
      : id(i), type(MatrixType), pointerType(inner, StorageClass::Max), basicType(m)
  {
  }
  DataType(Id i, Id inner, const Vector &v)
      : id(i), type(VectorType), pointerType(inner, StorageClass::Max), basicType(v, 0)
  {
  }
  DataType(Id i, const Scalar &s) : id(i), type(ScalarType), basicType(Vector(s, 0), 0) {}
  DataType(Id i, const Pointer &p) : id(i), type(PointerType), pointerType(p) {}
  DataType(Id i, Id c, Id l) : id(i), type(ArrayType), pointerType(c, StorageClass::Max), length(l)
  {
  }
  DataType(Id i, const rdcarray<Id> &c) : id(i), type(StructType)
  {
    children.reserve(c.size());
    for(Id child : c)
    {
      children.push_back({child});
    }
  }
  DataType(Id i, Type t) : id(i), type(t) {}
  const Scalar &scalar() const { return basicType.vector.scalar; }
  const Vector &vector() const { return basicType.vector; }
  const Matrix &matrix() const { return basicType; }
  Id InnerType() const { return pointerType.baseId; }
  bool IsOpaqueType() const
  {
    switch(type)
    {
      case Type::ScalarType:
      case Type::VectorType:
      case Type::MatrixType:
      case Type::StructType:
      case Type::PointerType:
      case Type::ArrayType: return false;
      case Type::ImageType:
      case Type::SamplerType:
      case Type::SampledImageType:
      case Type::RayQueryType:
      case Type::AccelerationStructureType: return true;
      default:
      {
        RDCWARN("Unknown SPIR-V type!");
        break;
      }
    }
    return false;
  }
  Id id;
  rdcstr name;
  Type type = UnknownType;

  Matrix basicType;
  Pointer pointerType;
  Id length;    // for arrays
  rdcarray<Child> children;
};

struct OpExecutionMode;
struct OpExecutionModeId;

struct ExecutionModes
{
  // common ones directly
  Topology outTopo = Topology::Unknown;

  enum DepthMode
  {
    DepthNormal,
    DepthGreater,
    DepthLess,
  } depthMode = DepthNormal;

  struct
  {
    uint32_t x = 0, y = 0, z = 0;
  } localSize;
  struct
  {
    Id x, y, z;
  } localSizeId;

  // others in an array
  rdcarray<ExecutionModeAndParamData> others;

  void Register(const OpExecutionMode &mode);
  void Register(const OpExecutionModeId &mode);
  void Unregister(const OpExecutionMode &mode);
  void Unregister(const OpExecutionModeId &mode);
};

struct EntryPoint
{
  EntryPoint() = default;
  EntryPoint(ExecutionModel e, Id i, rdcstr n, const rdcarray<Id> &ids)
      : executionModel(e), id(i), name(n), usedIds(ids)
  {
  }
  ExecutionModel executionModel;
  Id id;
  rdcstr name;
  ExecutionModes executionModes;
  rdcarray<Id> usedIds;

  bool operator<(const EntryPoint &o) const
  {
    if(id != o.id)
      return id < o.id;
    return name < o.name;
  }

  bool operator!=(const EntryPoint &o) const { return !operator==(o); }
  bool operator==(const EntryPoint &o) const { return id == o.id && name == o.name; }
};

struct Variable
{
  Variable() = default;
  Variable(Id t, Id i, StorageClass s, Id init) : type(t), id(i), storage(s), initializer(init) {}
  Id type;
  Id id;
  StorageClass storage;
  Id initializer;

  bool operator<(const Variable &o) const
  {
    if(id != o.id)
      return id < o.id;
    if(type != o.type)
      return type < o.type;
    if(storage != o.storage)
      return storage < o.storage;
    return initializer < o.initializer;
  }

  bool operator!=(const Variable &o) const { return !operator==(o); }
  bool operator==(const Variable &o) const
  {
    return id == o.id && type == o.type && storage == o.storage && initializer == o.initializer;
  }
};

struct Constant
{
  Id type;
  Id id;
  ShaderVariable value;
  rdcarray<Id> children;
  Op op;
};

struct SpecOp
{
  Id type;
  Id id;
  Op op;
  rdcarray<Id> params;
};

// hack around enum class being useless for array indices :(
struct Section
{
  enum Type
  {
    Capabilities,
    First = Capabilities,
    Extensions,
    ExtInst,
    MemoryModel,
    EntryPoints,
    ExecutionMode,
    DebugStringSource,
    DebugNames,
    DebugModuleProcessed,
    Annotations,
    TypesVariablesConstants,
    // handy aliases
    Types = TypesVariablesConstants,
    Variables = TypesVariablesConstants,
    Constants = TypesVariablesConstants,
    Functions,
    Count,
  };
};

class Processor
{
public:
  Processor();
  ~Processor();
  Processor(const Processor &o) = default;
  Processor &operator=(const Processor &o) = default;

  // accessors to structs/vectors of data
  const rdcarray<EntryPoint> &GetEntries() { return entries; }
  const rdcarray<Variable> &GetGlobals() { return globals; }
  Id GetIDType(Id id) { return idTypes[id]; }
  DataType &GetDataType(Id id)
  {
    static DataType empty;
    auto it = dataTypes.find(id);
    if(it == dataTypes.end())
      return empty;
    return it->second;
  }
  const Decorations &GetDecorations(Id id) const { return decorations[id]; };
  const rdcarray<uint32_t> &GetSPIRV() const { return m_SPIRV; }
protected:
  virtual void Parse(const rdcarray<uint32_t> &spirvWords);

  rdcarray<uint32_t> m_SPIRV;

  void UpdateMaxID(uint32_t maxId);

  // before parsing - e.g. to prepare any arrays that are max-id sized
  virtual void PreParse(uint32_t maxId);
  // even though we only need UnregisterOp when editing, we declare it here for ease of organisation
  // so we can define pairs of logic for the same things. Rather than having mismatched code with
  // the register of some map in one place and the unregister somewhere else that would be easy to
  // break.
  virtual void RegisterOp(Iter iter);
  virtual void UnregisterOp(Iter iter);
  // after parsing - e.g. to do any deferred post-processing
  virtual void PostParse();

  Iter GetID(Id id);
  ConstIter GetID(Id id) const;

  ShaderVariable MakeNULL(const DataType &type, uint64_t value);

  ShaderVariable EvaluateConstant(Id constID, const rdcarray<SpecConstant> &specInfo) const;

  uint32_t m_MajorVersion = 0, m_MinorVersion = 0;
  Generator m_Generator;
  uint32_t m_GeneratorVersion = 0;

  DenseIdMap<size_t> idOffsets;
  DenseIdMap<Id> idTypes;

  rdcarray<EntryPoint> entries;
  rdcarray<Variable> globals;

  std::set<rdcstr> extensions;
  std::set<Capability> capabilities;

  SparseIdMap<Constant> constants;
  SparseIdMap<SpecOp> specOps;
  std::set<Id> specConstants;

  DenseIdMap<Decorations> decorations;

  SparseIdMap<DataType> dataTypes;
  SparseIdMap<Image> imageTypes;
  SparseIdMap<Sampler> samplerTypes;
  SparseIdMap<SampledImage> sampledImageTypes;
  SparseIdMap<FunctionType> functionTypes;

  enum ExtSet
  {
    ExtSet_GLSL450 = 0,
    ExtSet_Printf = 1,
    ExtSet_ShaderDbg = 2,
    ExtSet_Count,
  };

  std::map<Id, rdcstr> extSets;
  Id knownExtSet[ExtSet_Count];

  struct LogicalSection
  {
    size_t startOffset = 0;
    size_t endOffset = 0;
  };

  LogicalSection m_Sections[Section::Count];

private:
  struct DeferredMemberDecoration
  {
    Id id;
    uint32_t member;
    DecorationAndParamData dec;
  };

  rdcarray<DeferredMemberDecoration> m_MemberDecorations;
  bool m_DeferMemberDecorations = false;
};

};    // namespace rdcspv
