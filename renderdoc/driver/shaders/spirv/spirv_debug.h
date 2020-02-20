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
class DebugAPIWrapper
{
public:
  virtual ~DebugAPIWrapper() {}
  virtual void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, rdcstr d) = 0;

  virtual void FillInputValue(ShaderVariable &var, ShaderBuiltin builtin, uint32_t location,
                              uint32_t offset) = 0;
};

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

  // for any allocated variables, a list of 'extra' pointers pointing to it. By default the actual
  // storage of allocated variables is not directly accessible (it's stored in e.g. inputs, outputs,
  // global constants, stack frame variables, etc). The ID for the allocating OpVariable is replaced
  // with a pointer pointing to that storage. However more pointers can be generated with
  // OpAccessChain etc, and these pointers must be listed as changed whenever the underlying Id
  // changes (and vice-versa - a change via any of those pointers must update all other pointers).
  SparseIdMap<rdcarray<Id>> pointersForId;

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
  ShaderDebugTrace *BeginDebug(DebugAPIWrapper *apiWrapper, const ShaderStage stage,
                               const rdcstr &entryPoint, const rdcarray<SpecConstant> &specInfo,
                               const std::map<size_t, uint32_t> &instructionLines,
                               uint32_t activeIndex);

  rdcarray<ShaderDebugState> ContinueDebug();

  GlobalState GetGlobal() { return global; }
  ThreadState &GetActiveLane() { return workgroup[activeLaneIndex]; }
private:
  virtual void PreParse(uint32_t maxId);
  virtual void PostParse();
  virtual void RegisterOp(Iter it);

  ShaderVariable EvaluatePointerVariable(const ShaderVariable &v) const;
  ShaderVariable MakePointerVariable(Id id, const ShaderVariable *v, uint32_t scalar0 = ~0U,
                                     uint32_t scalar1 = ~0U) const;
  Id GetPointerBaseId(const ShaderVariable &v) const;
  void WriteThroughPointer(const ShaderVariable &ptr, const ShaderVariable &val);
  ShaderVariable MakeCompositePointer(const ShaderVariable &base, Id id, rdcarray<uint32_t> &indices);

  void AllocateVariable(const Decorations &varDecorations, const Decorations &curDecorations,
                        DebugVariableType sourceVarType, const rdcstr &sourceName, uint32_t offset,
                        const DataType &inType, ShaderVariable &outVar);

  DebugAPIWrapper *apiWrapper = NULL;

  GlobalState global;
  rdcarray<ThreadState> workgroup;

  rdcarray<SourceVariableMapping> sourceVars;
  rdcarray<size_t> instructionOffsets;

  uint32_t activeLaneIndex = 0;
  ShaderStage stage;

  int steps = 0;

  DenseIdMap<rdcstr> strings;

  struct MemberName
  {
    Id id;
    uint32_t member;
    rdcstr name;
  };

  rdcarray<MemberName> memberNames;

  rdcstr GetRawName(Id id) const;
  rdcstr GetHumanName(Id id);

  std::set<rdcstr> usedNames;
  std::map<Id, rdcstr> dynamicNames;
};

// this does a 'safe' value assignment, by doing parallel depth-first iteration of both variables
// and only copying the value itself. This ensures we don't change any locations that might be
// pointed to. Assignments should only ever be between compatible types so this should be safe.
void AssignValue(ShaderVariable &dst, const ShaderVariable &src);

};    // namespace rdcspv
