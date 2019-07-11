/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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
#include <string>
#include <vector>
#include "common/common.h"
#include "spirv_common.h"

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
};

// helper to create Scalar objects for known types
template <typename T>
inline constexpr Scalar scalar();

#define SCALAR_TYPE(ctype, op, width, sign) \
  template <>                               \
  inline constexpr Scalar scalar<ctype>()   \
  {                                         \
    return Scalar(op, width, sign);         \
  }

SCALAR_TYPE(void, Op::TypeVoid, 0, false);
SCALAR_TYPE(bool, Op::TypeBool, 0, false);
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

struct EntryPoint
{
  EntryPoint() = default;
  EntryPoint(ExecutionModel e, Id i, rdcstr n) : executionModel(e), id(i), name(n) {}
  ExecutionModel executionModel;
  Id id;
  rdcstr name;

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
  Variable(Id t, Id i, StorageClass s) : type(t), id(i), storage(s) {}
  Id type;
  Id id;
  StorageClass storage;

  bool operator<(const Variable &o) const
  {
    if(id != o.id)
      return id < o.id;
    if(type != o.type)
      return type < o.type;
    return storage < o.storage;
  }

  bool operator!=(const Variable &o) const { return !operator==(o); }
  bool operator==(const Variable &o) const
  {
    return id == o.id && type == o.type && storage == o.storage;
  }
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
    Debug,
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

  // accessors to structs/vectors of data
  const std::vector<Id> &GetFunctions() { return functions; }
  const std::vector<EntryPoint> &GetEntries() { return entries; }
  const std::vector<Variable> &GetGlobals() { return globals; }
  Id GetIDType(Id id) { return idTypes[id.value()]; }
protected:
  virtual void Parse(const std::vector<uint32_t> &spirvWords);

  std::vector<uint32_t> m_SPIRV;

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

  std::vector<size_t> idOffsets;
  std::vector<Id> idTypes;

  std::vector<Id> functions;
  std::vector<EntryPoint> entries;
  std::vector<Variable> globals;

  std::set<rdcstr> extensions;
  std::set<Capability> capabilities;

  std::map<Id, Scalar> scalarTypes;
  std::map<Id, Vector> vectorTypes;
  std::map<Id, Matrix> matrixTypes;
  std::map<Id, Pointer> pointerTypes;
  std::map<Id, Image> imageTypes;
  std::map<Id, Sampler> samplerTypes;
  std::map<Id, SampledImage> sampledImageTypes;
  std::map<Id, FunctionType> functionTypes;

  std::map<rdcstr, Id> extSets;

  struct LogicalSection
  {
    size_t startOffset = 0;
    size_t endOffset = 0;
  };

  LogicalSection m_Sections[Section::Count];
};

};    // namespace rdcspv