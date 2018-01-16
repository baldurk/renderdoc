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
#include <string>
#include <vector>
#include "3rdparty/glslang/SPIRV/spirv.hpp"

class SPIRVOperation;

class SPIRVIterator
{
public:
  // constructors
  SPIRVIterator() = default;
  SPIRVIterator(std::vector<uint32_t> &w, size_t o) : words(&w), offset(o) {}
  // increment to the next op
  SPIRVIterator &operator++(int) { return operator++(); }
  SPIRVIterator &operator++()
  {
    offset += cur() >> spv::WordCountShift;
    return *this;
  }
  // utility functions
  std::vector<uint32_t>::iterator it() { return words->begin() + offset; }
  operator bool() const { return words != NULL && offset < words->size(); }
  uint32_t &operator*() { return cur(); }
  const uint32_t &operator*() const { return cur(); }
  spv::Op opcode() { return spv::Op(cur() & spv::OpCodeMask); }
  uint32_t &word(size_t idx) { return words->at(offset + idx); }
  const uint32_t &word(size_t idx) const { return words->at(offset + idx); }
  size_t size() const { return cur() >> spv::WordCountShift; }
private:
  inline uint32_t &cur() { return words->at(offset); }
  inline const uint32_t &cur() const { return words->at(offset); }
  // we make this a friend so it can poke directly into words when it wants to edit
  friend class SPIRVOperation;

  std::vector<uint32_t> *words = NULL;
  size_t offset = 0;
};

class SPIRVOperation
{
public:
  // constructor of a synthetic operation, from an operation & subsequent words, calculates the
  // length then constructs the first word with opcode + length.
  template <typename WordContainer>
  SPIRVOperation(spv::Op op, const WordContainer &data)
  {
    auto a = std::begin(data);
    auto b = std::end(data);
    size_t count = 1 + (b - a);
    words.resize(data.size());
    words[0] = MakeHeader(op, data.size());
    words.insert(words.begin() + 1, a, b);

    iter = SPIRVIterator(words, 0);
  }

  // constructor that takes existing words from elsewhere and just references it.
  // Since this is iterator based, normal iteration invalidation rules apply, if you modify earlier
  // in the SPIR-V this operation will become invalid.
  SPIRVOperation(SPIRVIterator it) : iter(it) {}
  uint32_t &operator[](size_t idx) { return iter.word(idx); }
  const uint32_t &operator[](size_t idx) const { return iter.word(idx); }
  size_t size() const { return iter.size(); }
  void push_back(uint32_t word)
  {
    spv::Op op = iter.opcode();
    size_t count = iter.size();
    iter.words->insert(iter.it() + count, word);

    // since the length is in the high-order bits, we have to extract/increment/insert instead of
    // just incrementing. Boo!
    *iter = MakeHeader(op, count + 1);
  }

private:
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
  uint32_t id;
  std::string name;
  SPIRVIterator iter;
};

class SPIRVEditor
{
public:
  SPIRVEditor(std::vector<uint32_t> &spirvWords);

  std::vector<uint32_t> &GetWords() { return spirv; }
  uint32_t MakeId();
  void AddDecoration(const SPIRVOperation &op);
  void AddType(const SPIRVOperation &op);
  void AddVariable(const SPIRVOperation &op);
  void AddFunction(const SPIRVOperation *ops, size_t count);

  // simple properties that are public.
  struct
  {
    uint8_t major = 1, minor = 0;
  } moduleVersion;
  uint32_t generator = 0;

  spv::SourceLanguage sourceLang = spv::SourceLanguageUnknown;
  uint32_t sourceVer = 0;

  const std::vector<SPIRVEntry> &GetEntries() { return entries; }
private:
  SPIRVIterator idBound;

  struct
  {
    SPIRVIterator decoration;
    SPIRVIterator typeVar;
    SPIRVIterator function;
  } globaliters;

  std::vector<SPIRVEntry> entries;

  std::vector<uint32_t> &spirv;
};
