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
#include "maths/vec.h"
#include "spirv_common.h"
#include "spirv_processor.h"

struct SPIRVInterfaceAccess;
struct SPIRVPatchData;

namespace rdcspv
{
class DebugAPIWrapper
{
public:
  virtual ~DebugAPIWrapper() {}
  virtual void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, rdcstr d) = 0;

  // TODO handle arrays of cbuffers
  virtual void ReadConstantBufferValue(uint32_t set, uint32_t bind, uint32_t offset,
                                       uint32_t byteSize, void *dst) = 0;
  virtual void FillInputValue(ShaderVariable &var, ShaderBuiltin builtin, uint32_t location,
                              uint32_t component) = 0;

  struct DerivativeDeltas
  {
    Vec4f ddxcoarse;
    Vec4f ddycoarse;
    Vec4f ddxfine;
    Vec4f ddyfine;
  };

  virtual DerivativeDeltas GetDerivative(ShaderBuiltin builtin, uint32_t location,
                                         uint32_t component) = 0;
};

struct GlobalState
{
public:
  GlobalState() {}
  // allocated storage for opaque uniform blocks, does not change over the course of debugging
  rdcarray<ShaderVariable> constantBlocks;
};

struct StackFrame
{
  StackFrame() = default;
  Id function;
  uint32_t funcCallInstruction = ~0U;

  // allocated storage for locals
  rdcarray<ShaderVariable> locals;

  // the thread's live list before the function was entered
  rdcarray<Id> live;
  rdcarray<SourceVariableMapping> sourceVars;

private:
  // disallow copying to ensure the locals we allocate never move around
  StackFrame(const StackFrame &o) = delete;
  StackFrame &operator=(const StackFrame &o) = delete;
};

class Debugger;

struct ThreadState
{
  ThreadState(uint32_t workgroupIdx, Debugger &debug, const GlobalState &globalState);
  ~ThreadState();

  void EnterFunction(ShaderDebugState *state, const rdcarray<Id> &arguments);
  void StepNext(ShaderDebugState *state, const rdcarray<DenseIdMap<ShaderVariable>> &prevWorkgroup);

  void FillCallstack(ShaderDebugState &state);

  bool Finished() const;

  uint32_t nextInstruction;

  const GlobalState &global;
  Debugger &debugger;

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

  // the last block we were in and the current block, for OpPhis
  Id lastBlock, curBlock;
  ShaderVariable returnValue;
  rdcarray<StackFrame *> callstack;

  // the list of IDs that are currently valid and live
  rdcarray<Id> live;

  rdcarray<SourceVariableMapping> sourceVars;

  // index in the pixel quad
  uint32_t workgroupIndex;
  bool done;

private:
  const ShaderVariable &GetSrc(Id id);
  void SetDst(ShaderDebugState *state, Id id, const ShaderVariable &val);
  void ProcessScopeChange(ShaderDebugState &state, const rdcarray<Id> &oldLive,
                          const rdcarray<Id> &newLive);
  void JumpToLabel(Id target);
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
                               const SPIRVPatchData &patchData, uint32_t activeIndex);

  rdcarray<ShaderDebugState> ContinueDebug();

  Iter GetIterForInstruction(uint32_t inst);
  uint32_t GetInstructionForIter(Iter it);
  uint32_t GetInstructionForFunction(Id id);
  uint32_t GetInstructionForLabel(Id id);
  const DataType &GetType(Id typeId);
  rdcstr GetRawName(Id id) const;
  rdcstr GetHumanName(Id id);
  void AddSourceVars(rdcarray<SourceVariableMapping> &sourceVars, Id id);
  void AllocateVariable(Id id, Id typeId, DebugVariableType sourceVarType, const rdcstr &sourceName,
                        ShaderVariable &outVar);

  ShaderVariable EvaluatePointerVariable(const ShaderVariable &v) const;
  ShaderVariable MakePointerVariable(Id id, const ShaderVariable *v, uint32_t scalar0 = ~0U,
                                     uint32_t scalar1 = ~0U) const;
  Id GetPointerBaseId(const ShaderVariable &v) const;
  void WriteThroughPointer(const ShaderVariable &ptr, const ShaderVariable &val);
  ShaderVariable MakeCompositePointer(const ShaderVariable &base, Id id, rdcarray<uint32_t> &indices);

  uint32_t GetNumInstructions() { return (uint32_t)instructionOffsets.size(); }
  GlobalState GetGlobal() { return global; }
  const rdcarray<Id> &GetLiveGlobals() { return liveGlobals; }
  const rdcarray<SourceVariableMapping> &GetGlobalSourceVars() { return globalSourceVars; }
  ThreadState &GetActiveLane() { return workgroup[activeLaneIndex]; }
private:
  virtual void PreParse(uint32_t maxId);
  virtual void PostParse();
  virtual void RegisterOp(Iter it);

  uint32_t AllocateVariable(const Decorations &varDecorations, const Decorations &curDecorations,
                            DebugVariableType sourceVarType, const rdcstr &sourceName,
                            uint32_t offset, const DataType &inType, ShaderVariable &outVar);
  uint32_t ApplyDerivatives(uint32_t quadIndex, const Decorations &curDecorations,
                            uint32_t location, const DataType &inType, ShaderVariable &outVar);

  void AddSourceVars(rdcarray<SourceVariableMapping> &sourceVars, const DataType &inType,
                     const rdcstr &sourceName, const rdcstr &varName, uint32_t &offset);
  void MakeSignatureNames(const rdcarray<SPIRVInterfaceAccess> &sigList, rdcarray<rdcstr> &sigNames);

  /////////////////////////////////////////////////////////
  // debug data

  DebugAPIWrapper *apiWrapper = NULL;

  GlobalState global;
  rdcarray<ThreadState> workgroup;

  uint32_t activeLaneIndex = 0;
  ShaderStage stage;

  int steps = 0;

  /////////////////////////////////////////////////////////
  // parsed data

  struct MemberName
  {
    Id id;
    uint32_t member;
    rdcstr name;
  };

  DenseIdMap<rdcstr> strings;
  rdcarray<MemberName> memberNames;
  std::map<rdcstr, Id> entryLookup;

  SparseIdMap<size_t> idDeathOffset;

  SparseIdMap<uint32_t> labelInstruction;

  // the live mutable global variables, to initialise a stack frame's live list
  rdcarray<Id> liveGlobals;
  rdcarray<SourceVariableMapping> globalSourceVars;

  struct Function
  {
    size_t begin = 0;
    rdcarray<Id> parameters;
    rdcarray<Id> variables;
  };

  SparseIdMap<Function> functions;
  Function *curFunction = NULL;

  rdcarray<size_t> instructionOffsets;

  std::set<rdcstr> usedNames;
  std::map<Id, rdcstr> dynamicNames;
  void CalcActiveMask(rdcarray<bool> &activeMask);
};

// this does a 'safe' value assignment, by doing parallel depth-first iteration of both variables
// and only copying the value itself. This ensures we don't change any locations that might be
// pointed to. Assignments should only ever be between compatible types so this should be safe.
void AssignValue(ShaderVariable &dst, const ShaderVariable &src);

};    // namespace rdcspv
