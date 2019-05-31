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

class SPIRVEditor;

struct SPIRVEntry
{
  rdcspv::Id id;
  std::string name;
};

struct SPIRVVariable
{
  rdcspv::Id id;
  rdcspv::Id type;
  spv::StorageClass storageClass;
  rdcspv::Id init;

  bool operator<(const SPIRVVariable &o) const
  {
    if(id != o.id)
      return id < o.id;
    if(type != o.type)
      return type < o.type;
    if(storageClass != o.storageClass)
      return storageClass < o.storageClass;
    return init < o.init;
  }

  bool operator!=(const SPIRVVariable &o) const { return !operator==(o); }
  bool operator==(const SPIRVVariable &o) const
  {
    return id == o.id && type == o.type && storageClass == o.storageClass && init == o.init;
  }
};

struct SPIRVDecoration
{
  rdcspv::Id id;
  spv::Decoration dec = spv::DecorationMax;
  uint32_t parameters[4] = {};

  bool operator<(const SPIRVDecoration &o) const
  {
    if(id != o.id)
      return id < o.id;
    if(dec != o.dec)
      return dec < o.dec;

    for(size_t i = 0; i < ARRAY_COUNT(parameters); i++)
      if(parameters[i] != o.parameters[i])
        return parameters[i] < o.parameters[i];

    return false;
  }

  bool operator!=(const SPIRVDecoration &o) const { return !operator==(o); }
  bool operator==(const SPIRVDecoration &o) const
  {
    return id == o.id && dec == o.dec && !memcmp(parameters, o.parameters, sizeof(parameters));
  }
};

struct SPIRVBinding
{
  SPIRVBinding() = default;
  SPIRVBinding(uint32_t s, uint32_t b) : set(s), binding(b) {}
  uint32_t set = 0;
  uint32_t binding = ~0U;

  bool operator<(const SPIRVBinding &o) const
  {
    if(set != o.set)
      return set < o.set;
    return binding < o.binding;
  }

  bool operator!=(const SPIRVBinding &o) const { return !operator==(o); }
  bool operator==(const SPIRVBinding &o) const { return set == o.set && binding == o.binding; }
};

struct SPIRVScalar
{
  SPIRVScalar() : type(spv::OpMax), width(0), signedness(false) {}
  constexpr SPIRVScalar(spv::Op t, uint32_t w, bool s) : type(t), width(w), signedness(s) {}
  SPIRVScalar(rdcspv::Iter op);

  spv::Op type;
  uint32_t width;
  bool signedness;

  bool operator<(const SPIRVScalar &o) const
  {
    if(type != o.type)
      return type < o.type;
    if(signedness != o.signedness)
      return signedness < o.signedness;
    return width < o.width;
  }

  bool operator!=(const SPIRVScalar &o) const { return !operator==(o); }
  bool operator==(const SPIRVScalar &o) const
  {
    return type == o.type && width == o.width && signedness == o.signedness;
  }

  rdcspv::Operation decl(SPIRVEditor &editor) const
  {
    if(type == spv::OpTypeVoid)
      return rdcspv::Operation(type, {0});
    else if(type == spv::OpTypeBool)
      return rdcspv::Operation(type, {0});
    else if(type == spv::OpTypeFloat)
      return rdcspv::Operation(type, {0, width});
    else if(type == spv::OpTypeInt)
      return rdcspv::Operation(type, {0, width, signedness ? 1U : 0U});
    else
      return rdcspv::Operation(spv::OpNop, {0});
  }
};

// helper to create SPIRVScalar objects for known types
template <typename T>
inline constexpr SPIRVScalar scalar();

#define SCALAR_TYPE(ctype, op, width, sign)    \
  template <>                                  \
  inline constexpr SPIRVScalar scalar<ctype>() \
  {                                            \
    return SPIRVScalar(op, width, sign);       \
  }

SCALAR_TYPE(void, spv::OpTypeVoid, 0, false);
SCALAR_TYPE(bool, spv::OpTypeBool, 0, false);
SCALAR_TYPE(uint8_t, spv::OpTypeInt, 8, false);
SCALAR_TYPE(uint16_t, spv::OpTypeInt, 16, false);
SCALAR_TYPE(uint32_t, spv::OpTypeInt, 32, false);
SCALAR_TYPE(uint64_t, spv::OpTypeInt, 64, false);
SCALAR_TYPE(int8_t, spv::OpTypeInt, 8, true);
SCALAR_TYPE(int16_t, spv::OpTypeInt, 16, true);
SCALAR_TYPE(int32_t, spv::OpTypeInt, 32, true);
SCALAR_TYPE(int64_t, spv::OpTypeInt, 64, true);
SCALAR_TYPE(float, spv::OpTypeFloat, 32, false);
SCALAR_TYPE(double, spv::OpTypeFloat, 64, false);

struct SPIRVVector
{
  SPIRVVector(const SPIRVScalar &s, uint32_t c) : scalar(s), count(c) {}
  SPIRVScalar scalar;
  uint32_t count;

  bool operator<(const SPIRVVector &o) const
  {
    if(scalar != o.scalar)
      return scalar < o.scalar;
    return count < o.count;
  }

  bool operator!=(const SPIRVVector &o) const { return !operator==(o); }
  bool operator==(const SPIRVVector &o) const { return scalar == o.scalar && count == o.count; }
  rdcspv::Operation decl(SPIRVEditor &editor) const;
};

struct SPIRVMatrix
{
  SPIRVMatrix(const SPIRVVector &v, uint32_t c) : vector(v), count(c) {}
  SPIRVVector vector;
  uint32_t count;

  bool operator<(const SPIRVMatrix &o) const
  {
    if(vector != o.vector)
      return vector < o.vector;
    return count < o.count;
  }

  bool operator!=(const SPIRVMatrix &o) const { return !operator==(o); }
  bool operator==(const SPIRVMatrix &o) const { return vector == o.vector && count == o.count; }
  rdcspv::Operation decl(SPIRVEditor &editor) const;
};

struct SPIRVPointer
{
  SPIRVPointer(rdcspv::Id b, spv::StorageClass s) : baseId(b), storage(s) {}
  rdcspv::Id baseId;
  spv::StorageClass storage;

  bool operator<(const SPIRVPointer &o) const
  {
    if(baseId != o.baseId)
      return baseId < o.baseId;
    return storage < o.storage;
  }

  bool operator!=(const SPIRVPointer &o) const { return !operator==(o); }
  bool operator==(const SPIRVPointer &o) const
  {
    return baseId == o.baseId && storage == o.storage;
  }
  rdcspv::Operation decl(SPIRVEditor &editor) const;
};

struct SPIRVImage
{
  SPIRVImage(SPIRVScalar ret, spv::Dim d, uint32_t dp, uint32_t ar, uint32_t m, uint32_t samp,
             spv::ImageFormat f)
      : retType(ret), dim(d), depth(dp), arrayed(ar), ms(m), sampled(samp), format(f)
  {
  }

  SPIRVScalar retType;
  spv::Dim dim;
  uint32_t depth;
  uint32_t arrayed;
  uint32_t ms;
  uint32_t sampled;
  spv::ImageFormat format;

  bool operator<(const SPIRVImage &o) const
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
  bool operator!=(const SPIRVImage &o) const { return !operator==(o); }
  bool operator==(const SPIRVImage &o) const
  {
    return retType == o.retType && dim == o.dim && depth == o.depth && arrayed == o.arrayed &&
           ms == o.ms && sampled == o.sampled && format == o.format;
  }
  rdcspv::Operation decl(SPIRVEditor &editor) const;
};

struct SPIRVSampler
{
  // no properties, all sampler types are equal
  bool operator<(const SPIRVSampler &o) const { return false; }
  bool operator!=(const SPIRVSampler &o) const { return false; }
  bool operator==(const SPIRVSampler &o) const { return true; }
  rdcspv::Operation decl(SPIRVEditor &editor) const;
};

struct SPIRVSampledImage
{
  SPIRVSampledImage(rdcspv::Id b) : baseId(b) {}
  rdcspv::Id baseId;

  bool operator<(const SPIRVSampledImage &o) const { return baseId < o.baseId; }
  bool operator!=(const SPIRVSampledImage &o) const { return !operator==(o); }
  bool operator==(const SPIRVSampledImage &o) const { return baseId == o.baseId; }
  rdcspv::Operation decl(SPIRVEditor &editor) const;
};

struct SPIRVFunction
{
  SPIRVFunction(rdcspv::Id ret, const std::vector<rdcspv::Id> &args)
      : returnId(ret), argumentIds(args)
  {
  }
  rdcspv::Id returnId;
  std::vector<rdcspv::Id> argumentIds;

  bool operator<(const SPIRVFunction &o) const
  {
    if(returnId != o.returnId)
      return returnId < o.returnId;
    return argumentIds < o.argumentIds;
  }

  bool operator!=(const SPIRVFunction &o) const { return !operator==(o); }
  bool operator==(const SPIRVFunction &o) const
  {
    return returnId == o.returnId && argumentIds == o.argumentIds;
  }
  rdcspv::Operation decl(SPIRVEditor &editor) const;
};

template <typename SPIRVType>
using SPIRVTypeId = std::pair<SPIRVType, rdcspv::Id>;

template <typename SPIRVType>
using SPIRVTypeIds = std::vector<SPIRVTypeId<SPIRVType>>;

// hack around enum class being useless for array indices :(
struct SPIRVSection
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

class SPIRVEditor
{
public:
  SPIRVEditor(std::vector<uint32_t> &spirvWords);
  ~SPIRVEditor() { StripNops(); }
  void StripNops();

  rdcspv::Id MakeId();

  void AddOperation(rdcspv::Iter iter, const rdcspv::Operation &op);

  // callbacks to allow us to update our internal structures over changes

  // called before any modifications are made. Removes the operation from internal structures.
  void PreModify(rdcspv::Iter iter) { UnregisterOp(iter); }
  // called after any modifications, re-adds the operation to internal structures with its new
  // properties
  void PostModify(rdcspv::Iter iter) { RegisterOp(iter); }
  // removed an operation and replaces it with nops
  void Remove(rdcspv::Iter iter)
  {
    UnregisterOp(iter);
    iter.nopRemove();
  }

  void SetName(rdcspv::Id id, const char *name);
  void AddDecoration(const rdcspv::Operation &op);
  void AddCapability(spv::Capability cap);
  void AddExtension(const std::string &extension);
  void AddExecutionMode(rdcspv::Id entry, spv::ExecutionMode mode, std::vector<uint32_t> params = {});
  rdcspv::Id ImportExtInst(const char *setname);
  rdcspv::Id AddType(const rdcspv::Operation &op);
  rdcspv::Id AddVariable(const rdcspv::Operation &op);
  rdcspv::Id AddConstant(const rdcspv::Operation &op);
  void AddFunction(const rdcspv::Operation *ops, size_t count);

  rdcspv::Iter GetID(rdcspv::Id id);
  // the entry point has 'two' opcodes, the entrypoint declaration and the function.
  // This returns the first, GetID returns the second.
  rdcspv::Iter GetEntry(rdcspv::Id id);
  rdcspv::Iter Begin(SPIRVSection::Type section)
  {
    return rdcspv::Iter(spirv, sections[section].startOffset);
  }
  rdcspv::Iter End(SPIRVSection::Type section)
  {
    return rdcspv::Iter(spirv, sections[section].endOffset);
  }

  // fetches the id of this type. If it exists already the old ID will be returned, otherwise it
  // will be declared and the new ID returned
  template <typename SPIRVType>
  rdcspv::Id DeclareType(const SPIRVType &t)
  {
    std::map<SPIRVType, rdcspv::Id> &table = GetTable<SPIRVType>();

    auto it = table.lower_bound(t);
    if(it != table.end() && it->first == t)
      return it->second;

    rdcspv::Operation decl = t.decl(*this);
    rdcspv::Id id = MakeId();
    decl[1] = id.value();
    AddType(decl);

    table.insert(it, std::pair<SPIRVType, rdcspv::Id>(t, id));

    return id;
  }

  template <typename SPIRVType>
  rdcspv::Id GetType(const SPIRVType &t)
  {
    std::map<SPIRVType, rdcspv::Id> &table = GetTable<SPIRVType>();

    auto it = table.find(t);
    if(it != table.end())
      return it->second;

    return rdcspv::Id();
  }

  template <typename SPIRVType>
  SPIRVTypeIds<SPIRVType> GetTypes()
  {
    std::map<SPIRVType, rdcspv::Id> &table = GetTable<SPIRVType>();

    SPIRVTypeIds<SPIRVType> ret;

    for(auto it = table.begin(); it != table.end(); ++it)
      ret.push_back(*it);

    return ret;
  }

  template <typename SPIRVType>
  const std::map<SPIRVType, rdcspv::Id> &GetTypeInfo() const
  {
    return GetTable<SPIRVType>();
  }

  SPIRVBinding GetBinding(rdcspv::Id id) const
  {
    auto it = bindings.find(id);
    if(it == bindings.end())
      return SPIRVBinding();
    return it->second;
  }
  const std::set<rdcspv::Id> &GetStructTypes() const { return structTypes; }
  rdcspv::Id DeclareStructType(const std::vector<rdcspv::Id> &members);

  // helper for AddConstant
  template <typename T>
  rdcspv::Id AddConstantImmediate(T t)
  {
    rdcspv::Id typeId = DeclareType(scalar<T>());
    std::vector<uint32_t> words = {typeId.value(), MakeId().value()};

    words.insert(words.end(), sizeof(T) / 4, 0U);

    memcpy(&words[2], &t, sizeof(T));

    return AddConstant(rdcspv::Operation(spv::OpConstant, words));
  }

  // simple properties that are public.
  struct
  {
    uint8_t major = 1, minor = 0;
  } moduleVersion;
  uint32_t generator = 0;

  spv::SourceLanguage sourceLang = spv::SourceLanguageUnknown;
  uint32_t sourceVer = 0;

  // accessors to structs/vectors of data
  const std::vector<SPIRVEntry> &GetEntries() { return entries; }
  const std::vector<SPIRVVariable> &GetVariables() { return variables; }
  const std::vector<rdcspv::Id> &GetFunctions() { return functions; }
  rdcspv::Id GetIDType(rdcspv::Id id) { return idTypes[id.value()]; }
private:
  inline void addWords(size_t offs, size_t num) { addWords(offs, (int32_t)num); }
  void addWords(size_t offs, int32_t num);

  void RegisterOp(rdcspv::Iter iter);
  void UnregisterOp(rdcspv::Iter iter);

  struct LogicalSection
  {
    size_t startOffset = 0;
    size_t endOffset = 0;
  };

  LogicalSection sections[SPIRVSection::Count];

  spv::AddressingModel addressmodel;
  spv::MemoryModel memorymodel;

  std::vector<SPIRVDecoration> decorations;

  std::map<rdcspv::Id, SPIRVBinding> bindings;

  std::vector<size_t> idOffsets;
  std::vector<rdcspv::Id> idTypes;

  std::vector<SPIRVEntry> entries;
  std::vector<SPIRVVariable> variables;
  std::vector<rdcspv::Id> functions;
  std::set<std::string> extensions;
  std::set<spv::Capability> capabilities;

  std::map<std::string, rdcspv::Id> extSets;

  std::map<SPIRVScalar, rdcspv::Id> scalarTypes;
  std::map<SPIRVVector, rdcspv::Id> vectorTypes;
  std::map<SPIRVMatrix, rdcspv::Id> matrixTypes;
  std::map<SPIRVPointer, rdcspv::Id> pointerTypes;
  std::map<SPIRVImage, rdcspv::Id> imageTypes;
  std::map<SPIRVSampler, rdcspv::Id> samplerTypes;
  std::map<SPIRVSampledImage, rdcspv::Id> sampledImageTypes;
  std::map<SPIRVFunction, rdcspv::Id> functionTypes;

  std::set<rdcspv::Id> structTypes;

  template <typename SPIRVType>
  std::map<SPIRVType, rdcspv::Id> &GetTable();

  template <typename SPIRVType>
  const std::map<SPIRVType, rdcspv::Id> &GetTable() const;

  std::vector<uint32_t> &spirv;
};