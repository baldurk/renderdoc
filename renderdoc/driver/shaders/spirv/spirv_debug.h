/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020 Baldur Karlsson
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

#include "api/replay/rdcarray.h"
#include "spirv_common.h"
#include "spirv_processor.h"

namespace rdcspv
{
struct GlobalState
{
public:
  GlobalState() {}
  // allocated storage for opaque uniform blocks, does not change over the course of debugging
  rdcarray<ShaderVariable> constantBlocks;
};

class ThreadState
{
public:
  ThreadState(int workgroupIdx, GlobalState &globalState);

  bool Finished() const { return done; }
  uint32_t nextInstruction;

  GlobalState &global;

  // thread-local inputs/outputs. This array does not change over the course of debugging
  rdcarray<ShaderVariable> inputs, outputs;

  // every ID's variable, if a pointer it may be pointing at a ShaderVariable stored elsewhere
  DenseIdMap<ShaderVariable> ids;

  // the list of IDs that are currently valid and live
  rdcarray<Id> live;

private:
  // index in the pixel quad
  int workgroupIndex;
  bool done;
};

class Debugger : public Processor, public ShaderDebugger
{
public:
  Debugger();
  ~Debugger();
  virtual void Parse(const rdcarray<uint32_t> &spirvWords);
  ShaderDebugTrace *BeginDebug(const ShaderStage stage, const rdcstr &entryPoint,
                               const rdcarray<SpecConstant> &specInfo,
                               const std::map<size_t, uint32_t> &instructionLines,
                               uint32_t activeIndex);

  rdcarray<ShaderDebugState> ContinueDebug();

  GlobalState GetGlobal() { return global; }
  ThreadState &GetActiveLane() { return workgroup[activeLaneIndex]; }
private:
  virtual void PreParse(uint32_t maxId);
  virtual void PostParse();
  virtual void RegisterOp(Iter it);

  GlobalState global;
  rdcarray<ThreadState> workgroup;

  rdcarray<SourceVariableMapping> sourceVars;
  rdcarray<size_t> instructionOffsets;

  uint32_t activeLaneIndex = 0;

  int steps = 0;

  DenseIdMap<rdcstr> strings;

  struct MemberName
  {
    Id id;
    uint32_t member;
    rdcstr name;
  };

  rdcarray<MemberName> memberNames;

  rdcstr GetRawName(Id id);
  rdcstr GetHumanName(Id id);

  std::set<rdcstr> usedNames;
  std::map<Id, rdcstr> dynamicNames;
};

};    // namespace rdcspv
