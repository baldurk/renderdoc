/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Baldur Karlsson
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
#include <string>
#include <vector>
#include "3rdparty/glslang/SPIRV/spirv.hpp"

class SPIRVOperation;
class SPIRVEditor;

struct SPIRVId
{
  constexpr inline SPIRVId() : id(0) {}
  constexpr inline SPIRVId(uint32_t i) : id(i) {}
  inline operator uint32_t() const { return id; }
  constexpr inline bool operator==(SPIRVId o) const { return id == o.id; }
  constexpr inline bool operator!=(SPIRVId o) const { return id != o.id; }
  constexpr inline bool operator<(SPIRVId o) const { return id < o.id; }
  uint32_t id;
};

// length of 1 word in the top 16-bits, OpNop = 0 in the lower 16-bits
#define SPV_NOP (0x00010000)

class SPIRVIterator
{
public:
  // constructors
  SPIRVIterator() = default;
  SPIRVIterator(std::vector<uint32_t> &w, size_t o) : words(&w), offset(o) {}
  // increment to the next op
  SPIRVIterator operator++(int)
  {
    SPIRVIterator ret = *this;
    operator++();
    return ret;
  }
  SPIRVIterator operator++()
  {
    do
    {
      offset += cur() >> spv::WordCountShift;
      // silently skip nops
    } while(*this && opcode() == spv::OpNop);

    return *this;
  }
  // utility functions
  explicit operator bool() const { return words != NULL && offset < words->size(); }
  uint32_t &operator*() { return cur(); }
  const uint32_t &operator*() const { return cur(); }
  spv::Op opcode() { return spv::Op(cur() & spv::OpCodeMask); }
  uint32_t &word(size_t idx) { return words->at(offset + idx); }
  const uint32_t &word(size_t idx) const { return words->at(offset + idx); }
  size_t size() const { return cur() >> spv::WordCountShift; }
private:
  inline uint32_t &cur() { return words->at(offset); }
  inline const uint32_t &cur() const { return words->at(offset); }
  // we add some friend classes to poke directly into words when it wants to edit
  friend class SPIRVOperation;
  friend class SPIRVEditor;
  std::vector<uint32_t>::iterator it() { return words->begin() + offset; }
  std::vector<uint32_t>::const_iterator it() const { return words->cbegin() + offset; }
  size_t offset = 0;
  std::vector<uint32_t> *words = NULL;
};

class SPIRVOperation
{
public:
  // constructor of a synthetic operation, from an operation & subsequent words, calculates the
  // length then constructs the first word with opcode + length.
  SPIRVOperation(spv::Op op, const std::vector<uint32_t> &data)
  {
    words.push_back(MakeHeader(op, data.size() + 1));
    words.insert(words.begin() + 1, data.begin(), data.end());

    iter = SPIRVIterator(words, 0);
  }

  SPIRVOperation(const SPIRVOperation &op)
  {
    words = op.words;

    iter = SPIRVIterator(words, 0);
  }

  // constructor that takes existing words from elsewhere and just references it.
  // Since this is iterator based, normal iteration invalidation rules apply, if you modify earlier
  // in the SPIR-V this operation will become invalid.
  SPIRVOperation(SPIRVIterator it) : iter(it) {}
  uint32_t &operator[](size_t idx) { return iter.word(idx); }
  const uint32_t &operator[](size_t idx) const { return iter.word(idx); }
  size_t size() const { return iter.size(); }
  void nopRemove()
  {
    for(size_t i = 0, sz = size(); i < sz; i++)
      iter.word(i) = SPV_NOP;
  }
  void nopRemove(size_t idx, size_t count = 0)
  {
    size_t oldSize = size();

    if(count == 0)
      count = oldSize - idx;

    // reduce the size of this op
    *iter = MakeHeader(iter.opcode(), oldSize - count);

    // move any words on the end into the middle, then nop them
    for(size_t i = 0; i < count; i++)
    {
      iter.word(idx + i) = iter.word(idx + count + i);
      iter.word(oldSize - i - 1) = SPV_NOP;
    }
  }

private:
  friend class SPIRVEditor;

  std::vector<uint32_t>::const_iterator begin() const { return iter.it(); }
  std::vector<uint32_t>::const_iterator end() const { return iter.it() + size(); }
  inline uint32_t MakeHeader(spv::Op op, size_t WordCount)
  {
    return (uint32_t(op) & spv::OpCodeMask) | (uint16_t(WordCount) << spv::WordCountShift);
  }

  // everything is based around this iterator, which may point into our local storage or to external
  // storage.
  SPIRVIterator iter;

  // may not be used, if we refer to an external iterator
  std::vector<uint32_t> words;
};

struct SPIRVEntry
{
  SPIRVId id;
  std::string name;
};

struct SPIRVVoid
{
};

struct SPIRVScalar
{
  constexpr SPIRVScalar(spv::Op t, uint32_t w, bool s) : type(t), width(w), signedness(s) {}
  SPIRVScalar(SPIRVIterator op);

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

  inline SPIRVOperation decl() const
  {
    if(type == spv::Op::OpTypeBool)
      return SPIRVOperation(type, {0});
    else if(type == spv::Op::OpTypeFloat)
      return SPIRVOperation(type, {0, width});
    else if(type == spv::Op::OpTypeInt)
      return SPIRVOperation(type, {0, width, signedness ? 1U : 0U});
    else
      return SPIRVOperation(spv::Op::OpNop, {0});
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

SCALAR_TYPE(bool, spv::Op::OpTypeBool, 0, false);
SCALAR_TYPE(uint16_t, spv::Op::OpTypeInt, 16, false);
SCALAR_TYPE(uint32_t, spv::Op::OpTypeInt, 32, false);
SCALAR_TYPE(int16_t, spv::Op::OpTypeInt, 16, true);
SCALAR_TYPE(int32_t, spv::Op::OpTypeInt, 32, true);
SCALAR_TYPE(float, spv::Op::OpTypeFloat, 32, false);
SCALAR_TYPE(double, spv::Op::OpTypeFloat, 64, false);

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
};

struct SPIRVPointer
{
  SPIRVPointer(SPIRVId b, spv::StorageClass s) : baseId(b), storage(s) {}
  SPIRVId baseId;
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
};

struct SPIRVFunction
{
  SPIRVFunction(SPIRVId ret, const std::vector<SPIRVId> &args) : returnId(ret), argumentIds(args) {}
  SPIRVId returnId;
  std::vector<SPIRVId> argumentIds;

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
};

class SPIRVEditor
{
public:
  SPIRVEditor(std::vector<uint32_t> &spirvWords);

  void StripNops();

  SPIRVId MakeId();

  void SetName(uint32_t id, const char *name);
  void AddDecoration(const SPIRVOperation &op);
  void AddType(const SPIRVOperation &op);
  void AddVariable(const SPIRVOperation &op);
  void AddFunction(const SPIRVOperation *ops, size_t count);

  SPIRVIterator GetID(SPIRVId id);
  // the entry point has 'two' opcodes, the entrypoint declaration and the function.
  // This returns the first, GetID returns the second.
  SPIRVIterator GetEntry(SPIRVId id);
  SPIRVIterator GetDebugInstructions();
  SPIRVIterator GetDecorationInstructions();
  SPIRVIterator GetTypeInstructions();

  // fetches the id of this type. If it exists already the old ID will be returned, otherwise it
  // will be declared and the new ID returned
  SPIRVId DeclareType(const SPIRVVoid &);
  SPIRVId DeclareType(const SPIRVScalar &scalar);
  SPIRVId DeclareType(const SPIRVVector &vector);
  SPIRVId DeclareType(const SPIRVMatrix &matrix);
  SPIRVId DeclareType(const SPIRVPointer &pointer);
  SPIRVId DeclareType(const SPIRVFunction &func);

  SPIRVId DeclareStructType(std::vector<uint32_t> members);

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
  const std::vector<SPIRVId> &GetFunctions() { return functions; }
private:
  inline void addWords(size_t offs, size_t num) { addWords(offs, (int32_t)num); }
  void addWords(size_t offs, int32_t num);

  struct LogicalSection
  {
    size_t startOffset = 0;
    size_t endOffset = 0;
  };

  LogicalSection entryPointSection;
  LogicalSection debugSection;
  LogicalSection decorationSection;
  LogicalSection typeVarSection;

  std::vector<size_t> idOffsets;

  std::vector<SPIRVEntry> entries;
  std::vector<SPIRVId> functions;

  std::map<SPIRVScalar, SPIRVId> scalarTypes;
  std::map<SPIRVVector, SPIRVId> vectorTypes;
  std::map<SPIRVMatrix, SPIRVId> matrixTypes;
  std::map<SPIRVPointer, SPIRVId> pointerTypes;
  std::map<SPIRVFunction, SPIRVId> functionTypes;
  SPIRVId voidType;

  std::vector<uint32_t> &spirv;
};
