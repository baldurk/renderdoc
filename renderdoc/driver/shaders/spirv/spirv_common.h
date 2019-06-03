/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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
#include <vector>
#include "3rdparty/glslang/SPIRV/spirv.hpp"
#include "api/replay/renderdoc_replay.h"

namespace rdcspv
{
// length of 1 word in the top 16-bits, OpNop = 0 in the lower 16-bits
static constexpr uint32_t OpNopWord = 0x00010000U;

struct Id
{
  constexpr inline Id() : id(0) {}
  // only allow explicit functions to cast to/from uint32_t
  constexpr static inline Id fromWord(uint32_t i) { return Id(i); }
  inline uint32_t value() const { return id; }
  constexpr inline explicit operator bool() const { return id != 0; }
  constexpr inline bool operator==(const Id o) const { return id == o.id; }
  constexpr inline bool operator!=(const Id o) const { return id != o.id; }
  constexpr inline bool operator<(const Id o) const { return id < o.id; }
  constexpr inline bool operator==(const uint32_t o) const { return id == o; }
  constexpr inline bool operator!=(const uint32_t o) const { return id != o; }
  constexpr inline bool operator<(const uint32_t o) const { return id < o; }
private:
  constexpr inline Id(uint32_t i) : id(i) {}
  uint32_t id;
};

class Operation;

class Iter
{
public:
  // constructors
  Iter() = default;
  Iter(std::vector<uint32_t> &w, size_t o) : words(&w), offset(o) {}
  // increment to the next op
  Iter operator++(int)
  {
    Iter ret = *this;
    operator++();
    return ret;
  }
  Iter operator++()
  {
    do
    {
      offset += cur() >> spv::WordCountShift;
      // silently skip nops
    } while(*this && opcode() == spv::OpNop);

    return *this;
  }
  bool operator==(const Iter &it) const = delete;
  bool operator!=(const Iter &it) const = delete;
  bool operator<(const Iter &it) const { return words == it.words && offset < it.offset; }
  // utility functions
  explicit operator bool() const { return words != NULL && offset < words->size(); }
  uint32_t &operator*() { return cur(); }
  const uint32_t &operator*() const { return cur(); }
  spv::Op opcode() { return spv::Op(cur() & spv::OpCodeMask); }
  uint32_t &word(size_t idx) { return words->at(offset + idx); }
  const uint32_t &word(size_t idx) const { return words->at(offset + idx); }
  size_t offs() const { return offset; }
  size_t size() const { return cur() >> spv::WordCountShift; }
  // replace part of this operation with NOPs and update the length. Cannot completely erase the
  // operation, or expand it
  void nopRemove(size_t idx, size_t count = 0);
  // completely remove the operation and replace with NOPs
  void nopRemove();

private:
  friend class Operation;
  inline uint32_t &cur() { return words->at(offset); }
  inline const uint32_t &cur() const { return words->at(offset); }
  std::vector<uint32_t>::iterator it() { return words->begin() + offset; }
  std::vector<uint32_t>::const_iterator it() const { return words->cbegin() + offset; }
  size_t offset = 0;
  std::vector<uint32_t> *words = NULL;
};

class Operation
{
public:
  // constructor of a synthetic operation, from an operation & subsequent words, calculates the
  // length then constructs the first word with opcode + length.
  Operation(spv::Op op, const std::vector<uint32_t> &data)
  {
    words.push_back(MakeHeader(op, data.size() + 1));
    words.insert(words.begin() + 1, data.begin(), data.end());

    iter = Iter(words, 0);
  }

  Operation(const Operation &op)
  {
    words = op.words;

    iter = Iter(words, 0);
  }

  static Operation copy(Iter it)
  {
    Operation ret(it);

    ret.words.insert(ret.words.begin(), it.it(), it.it() + it.size());
    ret.iter = Iter(ret.words, 0);

    return ret;
  }

  // constructor that takes existing words from elsewhere and just references it.
  // Since this is iterator based, normal iteration invalidation rules apply, if you modify earlier
  // in the SPIR-V this operation will become invalid.
  Operation(Iter it) : iter(it) {}
  uint32_t &operator[](size_t idx) { return iter.word(idx); }
  const uint32_t &operator[](size_t idx) const { return iter.word(idx); }
  size_t size() const { return iter.size(); }
  // insert the words for this op into the destination vector
  void appendTo(std::vector<uint32_t> &dest) const { dest.insert(dest.end(), begin(), end()); }
  void insertInto(std::vector<uint32_t> &dest, size_t offset) const
  {
    dest.insert(dest.begin() + offset, begin(), end());
  }
  inline static uint32_t MakeHeader(spv::Op op, size_t WordCount)
  {
    return (uint32_t(op) & spv::OpCodeMask) | (uint16_t(WordCount) << spv::WordCountShift);
  }

private:
  std::vector<uint32_t>::const_iterator begin() const { return iter.it(); }
  std::vector<uint32_t>::const_iterator end() const { return iter.it() + size(); }
  // everything is based around this iterator, which may point into our local storage or to external
  // storage.
  Iter iter;

  // may not be used, if we refer to an external iterator
  std::vector<uint32_t> words;
};
};    // namespace rdcspv

DECLARE_STRINGISE_TYPE(rdcspv::Id);
