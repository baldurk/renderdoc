/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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
#include "api/replay/renderdoc_replay.h"
#include "common/common.h"
#include "spirv_common.h"
#include "spirv_op_helpers.h"

namespace rdcspv
{
class Editor;

struct Binding
{
  Binding() = default;
  Binding(uint32_t s, uint32_t b) : set(s), binding(b) {}
  uint32_t set = 0;
  uint32_t binding = ~0U;

  bool operator<(const Binding &o) const
  {
    if(set != o.set)
      return set < o.set;
    return binding < o.binding;
  }

  bool operator!=(const Binding &o) const { return !operator==(o); }
  bool operator==(const Binding &o) const { return set == o.set && binding == o.binding; }
};

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

  Operation decl(Editor &editor) const
  {
    if(type == Op::TypeVoid)
      return OpTypeVoid(Id());
    else if(type == Op::TypeBool)
      return OpTypeBool(Id());
    else if(type == Op::TypeFloat)
      return OpTypeFloat(Id(), width);
    else if(type == Op::TypeInt)
      return OpTypeInt(Id(), width, signedness ? 1U : 0U);
    else
      return OpNop();
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
  Operation decl(Editor &editor) const;
};

struct Matrix
{
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
  Operation decl(Editor &editor) const;
};

struct Pointer
{
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
  Operation decl(Editor &editor) const;
};

struct Image
{
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
  Operation decl(Editor &editor) const;
};

struct Sampler
{
  // no properties, all sampler types are equal
  bool operator<(const Sampler &o) const { return false; }
  bool operator!=(const Sampler &o) const { return false; }
  bool operator==(const Sampler &o) const { return true; }
  Operation decl(Editor &editor) const;
};

struct SampledImage
{
  SampledImage(Id b) : baseId(b) {}
  Id baseId;

  bool operator<(const SampledImage &o) const { return baseId < o.baseId; }
  bool operator!=(const SampledImage &o) const { return !operator==(o); }
  bool operator==(const SampledImage &o) const { return baseId == o.baseId; }
  Operation decl(Editor &editor) const;
};

struct Function
{
  Function(Id ret, const rdcarray<Id> &args) : returnId(ret), argumentIds(args) {}
  Id returnId;
  rdcarray<Id> argumentIds;

  bool operator<(const Function &o) const
  {
    if(returnId != o.returnId)
      return returnId < o.returnId;
    return argumentIds < o.argumentIds;
  }

  bool operator!=(const Function &o) const { return !operator==(o); }
  bool operator==(const Function &o) const
  {
    return returnId == o.returnId && argumentIds == o.argumentIds;
  }
  Operation decl(Editor &editor) const;
};

template <typename Type>
using TypeId = std::pair<Type, Id>;

template <typename Type>
using TypeIds = std::vector<TypeId<Type>>;

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

class Editor
{
public:
  Editor(std::vector<uint32_t> &spirvWords);
  ~Editor() { StripNops(); }
  void StripNops();

  Id MakeId();

  void AddOperation(Iter iter, const Operation &op);

  // callbacks to allow us to update our internal structures over changes

  // called before any modifications are made. Removes the operation from internal structures.
  void PreModify(Iter iter) { UnregisterOp(iter); }
  // called after any modifications, re-adds the operation to internal structures with its new
  // properties
  void PostModify(Iter iter) { RegisterOp(iter); }
  // removed an operation and replaces it with nops
  void Remove(Iter iter)
  {
    UnregisterOp(iter);
    iter.nopRemove();
  }

  void SetName(Id id, const char *name);
  void AddDecoration(const Operation &op);
  void AddCapability(Capability cap);
  void AddExtension(const rdcstr &extension);
  void AddExecutionMode(const Operation &mode);
  Id ImportExtInst(const char *setname);
  Id AddType(const Operation &op);
  Id AddVariable(const Operation &op);
  Id AddConstant(const Operation &op);
  void AddFunction(const Operation *ops, size_t count);

  Iter GetID(Id id);
  // the entry point has 'two' opcodes, the entrypoint declaration and the function.
  // This returns the first, GetID returns the second.
  Iter GetEntry(Id id);
  Iter Begin(Section::Type section) { return Iter(spirv, sections[section].startOffset); }
  Iter End(Section::Type section) { return Iter(spirv, sections[section].endOffset); }
  // fetches the id of this type. If it exists already the old ID will be returned, otherwise it
  // will be declared and the new ID returned
  template <typename Type>
  Id DeclareType(const Type &t)
  {
    std::map<Type, Id> &table = GetTable<Type>();

    auto it = table.lower_bound(t);
    if(it != table.end() && it->first == t)
      return it->second;

    Operation decl = t.decl(*this);
    Id id = MakeId();
    decl[1] = id.value();
    AddType(decl);

    table.insert(it, std::pair<Type, Id>(t, id));

    return id;
  }

  template <typename Type>
  Id GetType(const Type &t)
  {
    std::map<Type, Id> &table = GetTable<Type>();

    auto it = table.find(t);
    if(it != table.end())
      return it->second;

    return Id();
  }

  template <typename Type>
  TypeIds<Type> GetTypes()
  {
    std::map<Type, Id> &table = GetTable<Type>();

    TypeIds<Type> ret;

    for(auto it = table.begin(); it != table.end(); ++it)
      ret.push_back(*it);

    return ret;
  }

  template <typename Type>
  const std::map<Type, Id> &GetTypeInfo() const
  {
    return GetTable<Type>();
  }

  Binding GetBinding(Id id) const
  {
    auto it = bindings.find(id);
    if(it == bindings.end())
      return Binding();
    return it->second;
  }
  const std::set<Id> &GetStructTypes() const { return structTypes; }
  Id DeclareStructType(const std::vector<Id> &members);

  // helper for AddConstant
  template <typename T>
  Id AddConstantImmediate(T t)
  {
    Id typeId = DeclareType(scalar<T>());
    std::vector<uint32_t> words = {typeId.value(), MakeId().value()};

    words.insert(words.end(), sizeof(T) / 4, 0U);

    memcpy(&words[2], &t, sizeof(T));

    return AddConstant(Operation(Op::Constant, words));
  }

  // accessors to structs/vectors of data
  const std::vector<OpEntryPoint> &GetEntries() { return entries; }
  const std::vector<OpVariable> &GetVariables() { return variables; }
  const std::vector<Id> &GetFunctions() { return functions; }
  Id GetIDType(Id id) { return idTypes[id.value()]; }
private:
  inline void addWords(size_t offs, size_t num) { addWords(offs, (int32_t)num); }
  void addWords(size_t offs, int32_t num);

  void RegisterOp(Iter iter);
  void UnregisterOp(Iter iter);

  struct LogicalSection
  {
    size_t startOffset = 0;
    size_t endOffset = 0;
  };

  LogicalSection sections[Section::Count];

  AddressingModel addressmodel;
  MemoryModel memorymodel;

  std::vector<OpDecorate> decorations;

  std::map<Id, Binding> bindings;

  std::vector<size_t> idOffsets;
  std::vector<Id> idTypes;

  std::vector<OpEntryPoint> entries;
  std::vector<OpVariable> variables;
  std::vector<Id> functions;
  std::set<rdcstr> extensions;
  std::set<Capability> capabilities;

  std::map<rdcstr, Id> extSets;

  std::map<Scalar, Id> scalarTypes;
  std::map<Vector, Id> vectorTypes;
  std::map<Matrix, Id> matrixTypes;
  std::map<Pointer, Id> pointerTypes;
  std::map<Image, Id> imageTypes;
  std::map<Sampler, Id> samplerTypes;
  std::map<SampledImage, Id> sampledImageTypes;
  std::map<Function, Id> functionTypes;

  std::set<Id> structTypes;

  template <typename Type>
  std::map<Type, Id> &GetTable();

  template <typename Type>
  const std::map<Type, Id> &GetTable() const;

  std::vector<uint32_t> &spirv;
};

inline bool operator<(const OpDecorate &a, const OpDecorate &b)
{
  if(a.target != b.target)
    return a.target < b.target;
  if(a.decoration.value != b.decoration.value)
    return a.decoration.value < b.decoration.value;

  return memcmp(&a.decoration, &b.decoration, sizeof(a.decoration)) < 0;
}

inline bool operator==(const OpDecorate &a, const OpDecorate &b)
{
  return a.target == b.target && !memcmp(&a.decoration, &b.decoration, sizeof(a.decoration));
}

};    // namespace rdcspv