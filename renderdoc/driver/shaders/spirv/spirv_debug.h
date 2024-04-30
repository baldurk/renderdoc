/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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
struct ImageOperandsAndParamDatas;

enum class GatherChannel : uint8_t
{
  Red = 0,
  Green = 1,
  Blue = 2,
  Alpha = 3,
};

struct ThreadState;

class DebugAPIWrapper
{
public:
  virtual ~DebugAPIWrapper() {}
  virtual void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, rdcstr d) = 0;

  virtual ResourceId GetShaderID() = 0;

  virtual uint64_t GetBufferLength(ShaderBindIndex bind) = 0;

  virtual void ReadBufferValue(ShaderBindIndex bind, uint64_t offset, uint64_t byteSize,
                               void *dst) = 0;
  virtual void WriteBufferValue(ShaderBindIndex bind, uint64_t offset, uint64_t byteSize,
                                const void *src) = 0;

  virtual void ReadAddress(uint64_t address, uint64_t byteSize, void *dst) = 0;
  virtual void WriteAddress(uint64_t address, uint64_t byteSize, const void *src) = 0;

  virtual bool ReadTexel(ShaderBindIndex imageBind, const ShaderVariable &coord, uint32_t sample,
                         ShaderVariable &output) = 0;
  virtual bool WriteTexel(ShaderBindIndex imageBind, const ShaderVariable &coord, uint32_t sample,
                          const ShaderVariable &value) = 0;

  virtual void FillInputValue(ShaderVariable &var, ShaderBuiltin builtin, uint32_t location,
                              uint32_t component) = 0;

  enum TextureType
  {
    Float_Texture = 0x00,

    UInt_Texture = 0x01,
    SInt_Texture = 0x02,

    Buffer_Texture = 0x10,
    Subpass_Texture = 0x20,
  };

  virtual bool CalculateSampleGather(ThreadState &lane, Op opcode, TextureType texType,
                                     ShaderBindIndex imageBind, ShaderBindIndex samplerBind,
                                     const ShaderVariable &uv, const ShaderVariable &ddxCalc,
                                     const ShaderVariable &ddyCalc, const ShaderVariable &compare,
                                     GatherChannel gatherChannel,
                                     const ImageOperandsAndParamDatas &operands,
                                     ShaderVariable &output) = 0;

  virtual bool CalculateMathOp(ThreadState &lane, GLSLstd450 op,
                               const rdcarray<ShaderVariable> &params, ShaderVariable &output) = 0;

  struct DerivativeDeltas
  {
    ShaderVariable ddxcoarse;
    ShaderVariable ddycoarse;
    ShaderVariable ddxfine;
    ShaderVariable ddyfine;
  };

  virtual DerivativeDeltas GetDerivative(ShaderBuiltin builtin, uint32_t location,
                                         uint32_t component, VarType type) = 0;
};

typedef ShaderVariable (*ExtInstImpl)(ThreadState &, uint32_t, const rdcarray<Id> &);

struct ExtInstDispatcher
{
  rdcstr name;
  bool nonsemantic = false;
  rdcarray<rdcstr> names;
  rdcarray<ExtInstImpl> functions;
};

void ConfigureGLSLStd450(ExtInstDispatcher &extinst);

struct GlobalState
{
public:
  GlobalState() {}
  // allocated storage for opaque uniform blocks, does not change over the course of debugging
  rdcarray<ShaderVariable> constantBlocks;

  // workgroup private variables
  rdcarray<ShaderVariable> workgroups;

  // resources may be read-write but the variable itself doesn't change
  rdcarray<ShaderVariable> readOnlyResources;
  rdcarray<ShaderVariable> readWriteResources;
  rdcarray<ShaderVariable> samplers;

  SparseIdMap<ExtInstDispatcher> extInsts;

  uint64_t clock;
};

struct StackFrame
{
  StackFrame() = default;
  Id function;
  uint32_t funcCallInstruction = ~0U;

  // allocated storage for locals
  rdcarray<ShaderVariable> locals;

  // list of Ids we created, either variables/function parameters in this function, or IDs created
  // in this function. When we return from this frame they will be emptied.
  // This prevents a use-after-free with ShaderVariableChanges if we re-enter the same function
  // and want to show the previous value of an id
  rdcarray<Id> idsCreated;

  // as a hack for scoping without proper debug info, we track locals from their first use
  rdcarray<Id> localsUsed;

  // the thread's live list before the function was entered
  rdcarray<Id> live;

  // the last block we were in and the current block, for OpPhis
  Id lastBlock, curBlock;

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

  void EnterEntryPoint(ShaderDebugState *state);
  void StepNext(ShaderDebugState *state, const rdcarray<ThreadState> &workgroup);

  enum DerivDir
  {
    DDX,
    DDY
  };
  enum DerivType
  {
    Coarse,
    Fine
  };

  ShaderVariable CalcDeriv(DerivDir dir, DerivType type, const rdcarray<ThreadState> &workgroup,
                           Id val);

  void FillCallstack(rdcarray<Id> &funcs);

  bool Finished() const;

  uint32_t nextInstruction;

  const GlobalState &global;
  Debugger &debugger;

  // thread-local inputs/outputs. This array does not change over the course of debugging
  rdcarray<ShaderVariable> inputs, outputs;

  // thread-local private variables
  rdcarray<ShaderVariable> privates;

  // every ID's variable, if a pointer it may be pointing at a ShaderVariable stored elsewhere
  DenseIdMap<ShaderVariable> ids;

  // for any allocated variables, a list of 'extra' pointers pointing to it. By default the actual
  // storage of allocated variables is not directly accessible (it's stored in e.g. inputs, outputs,
  // global constants, stack frame variables, etc). The ID for the allocating OpVariable is replaced
  // with a pointer pointing to that storage. However more pointers can be generated with
  // OpAccessChain etc, and these pointers must be listed as changed whenever the underlying Id
  // changes (and vice-versa - a change via any of those pointers must update all other pointers).
  SparseIdMap<rdcarray<Id>> pointersForId;

  // the id of the merge block that the last branch targetted
  Id mergeBlock;
  ShaderVariable returnValue;
  rdcarray<StackFrame *> callstack;

  // the list of IDs that are currently valid and live
  rdcarray<Id> live;

  std::map<Id, uint32_t> lastWrite;

  // index in the pixel quad
  uint32_t workgroupIndex;
  bool helperInvocation;
  bool killed;

  const ShaderVariable &GetSrc(Id id) const;
  void WritePointerValue(Id pointer, const ShaderVariable &val);
  ShaderVariable ReadPointerValue(Id pointer);

private:
  void EnterFunction(const rdcarray<Id> &arguments);
  void SetDst(Id id, const ShaderVariable &val);
  void ProcessScopeChange(const rdcarray<Id> &oldLive, const rdcarray<Id> &newLive);
  void JumpToLabel(Id target);
  bool ReferencePointer(Id id);

  void SkipIgnoredInstructions();

  ShaderDebugState *m_State = NULL;
};

enum class DebugScope
{
  CompilationUnit,
  Composite,
  Function,
  Block,
};

struct TypeData
{
  VarType type = VarType::Unknown;
  uint32_t vecSize = 0, matSize = 0;
  bool colMajorMat = false;

  Id baseType;
  rdcarray<uint32_t> arrayDimensions;
  rdcarray<rdcpair<rdcstr, Id>> structMembers;
  rdcarray<uint32_t> memberOffsets;
};

struct LocalMapping
{
  bool operator<(const LocalMapping &o) const
  {
    if(sourceVar != o.sourceVar)
      return sourceVar < o.sourceVar;
    if(indexes != o.indexes)
      return indexes < o.indexes;
    return debugVar < o.debugVar;
  }

  bool isSourceSupersetOf(const LocalMapping &o) const
  {
    // this mapping is a superset of the other if:

    // it's the same source var
    if(sourceVar != o.sourceVar)
      return false;

    // it contains the same or fewer indices
    if(o.indexes.size() < indexes.size())
      return false;

    // the common prefix of indexes is identical
    for(size_t i = 0; i < indexes.size(); i++)
      if(indexes[i] != o.indexes[i])
        return false;

    // if all those conditions are true, we either map to the same index (indexes size is the same -
    // likely case) or else we cover a whole sub-tree where the other only covers a leaf (other has
    // more indexes - unlikely but possible)
    return true;
  }

  uint32_t instIndex;
  Id sourceVar;
  Id debugVar;
  bool isDeclare;
  rdcarray<uint32_t> indexes;
};

struct ScopeData
{
  DebugScope type;
  ScopeData *parent;
  uint32_t line;
  uint32_t column;
  int32_t fileIndex;
  size_t end;

  rdcstr name;

  rdcarray<Id> locals;

  rdcarray<LocalMapping> localMappings;

  bool HasAncestor(const ScopeData *check) const
  {
    const ScopeData *cur = this;

    while(cur)
    {
      if(cur == check)
        return true;
      cur = cur->parent;
    }

    return false;
  }
};

struct InlineData
{
  ScopeData *scope;
  InlineData *parent;
};

struct LocalData
{
  rdcstr name;
  ScopeData *scope;
  TypeData *type;
};

Id ParseRawName(const rdcstr &name);
rdcstr GetRawName(Id id);

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
  const DataType &GetTypeForId(Id ssaId);
  const Decorations &GetDecorations(Id typeId);
  bool IsDebugExtInstSet(Id id) const;
  bool HasDebugInfo() const { return m_DebugInfo.valid; }
  bool InDebugScope(uint32_t inst) const;
  rdcstr GetHumanName(Id id);
  void AllocateVariable(Id id, Id typeId, ShaderVariable &outVar);

  ShaderVariable ReadFromPointer(const ShaderVariable &v) const;
  ShaderVariable GetPointerValue(const ShaderVariable &v) const;
  uint64_t GetPointerByteOffset(const ShaderVariable &ptr) const;
  DebugAPIWrapper::TextureType GetTextureType(const ShaderVariable &img) const;
  ShaderVariable MakePointerVariable(Id id, const ShaderVariable *v, uint8_t scalar0 = 0xff,
                                     uint8_t scalar1 = 0xff) const;
  ShaderVariable MakeTypedPointer(uint64_t value, const DataType &type) const;
  Id GetPointerBaseId(const ShaderVariable &v) const;
  uint32_t GetPointerArrayStride(const ShaderVariable &ptr) const;
  bool IsOpaquePointer(const ShaderVariable &v) const;
  bool IsPhysicalPointer(const ShaderVariable &v) const;

  bool ArePointersAndEqual(const ShaderVariable &a, const ShaderVariable &b) const;
  void WriteThroughPointer(ShaderVariable &ptr, const ShaderVariable &val);
  ShaderVariable MakeCompositePointer(const ShaderVariable &base, Id id, rdcarray<uint32_t> &indices);

  DebugAPIWrapper *GetAPIWrapper() { return apiWrapper; }
  uint32_t GetNumInstructions() { return (uint32_t)instructionOffsets.size(); }
  GlobalState GetGlobal() { return global; }
  const rdcarray<Id> &GetLiveGlobals() { return liveGlobals; }
  ThreadState &GetActiveLane() { return workgroup[activeLaneIndex]; }
  const ThreadState &GetActiveLane() const { return workgroup[activeLaneIndex]; }
private:
  virtual void PreParse(uint32_t maxId);
  virtual void PostParse();
  virtual void RegisterOp(Iter it);

  uint32_t ApplyDerivatives(uint32_t quadIndex, const Decorations &curDecorations,
                            uint32_t location, const DataType &inType, ShaderVariable &outVar);

  template <typename ShaderVarType, bool allocate>
  uint32_t WalkVariable(const Decorations &curDecorations, const DataType &type,
                        uint64_t offsetOrLocation, ShaderVarType &var, const rdcstr &accessSuffix,
                        std::function<void(ShaderVarType &, const Decorations &, const DataType &,
                                           uint64_t, const rdcstr &)>
                            callback) const;
  void SetStructArrayNames(ShaderVariable &c, const DataType *typeWalk,
                           const rdcarray<SpecConstant> &specInfo);

  void MakeSignatureNames(const rdcarray<SPIRVInterfaceAccess> &sigList, rdcarray<rdcstr> &sigNames);

  void FillCallstack(ThreadState &thread, ShaderDebugState &state);
  void FillDebugSourceVars(rdcarray<InstructionSourceInfo> &instInfo);
  void FillDefaultSourceVars(rdcarray<InstructionSourceInfo> &instInfo);

  /////////////////////////////////////////////////////////
  // debug data

  DebugAPIWrapper *apiWrapper = NULL;

  GlobalState global;
  rdcarray<ThreadState> workgroup;

  Id convergeBlock;

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
  std::map<ShaderEntryPoint, Id> entryLookup;

  DenseIdMap<rdcpair<size_t, size_t>> idLiveRange;

  SparseIdMap<size_t> m_Files;
  LineColumnInfo m_CurLineCol;
  rdcarray<InstructionSourceInfo> m_InstInfo;

  SparseIdMap<uint32_t> labelInstruction;

  SparseIdMap<uint16_t> idToPointerType;
  rdcarray<rdcspv::Id> pointerTypeToId;

  // the live mutable global variables, to initialise a stack frame's live list
  rdcarray<Id> liveGlobals;

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

  struct
  {
    bool valid = false;

    SparseIdMap<TypeData> types;
    SparseIdMap<ScopeData> scopes;
    SparseIdMap<InlineData> inlined;
    ScopeData *curScope = NULL;
    InlineData *curInline = NULL;

    rdcarray<rdcpair<const ScopeData *, LocalMapping>> pendingMappings;

    rdcarray<Id> globals;
    rdcarray<Id> constants;

    SparseIdMap<LocalData> locals;

    SparseIdMap<int32_t> sources;
    SparseIdMap<rdcstr> filenames;

    SparseIdMap<Id> funcToDebugFunc;

    std::map<size_t, ScopeData *> lineScope;
    std::map<size_t, InlineData *> lineInline;

    rdcarray<LocalMapping> activeLocalMappings;
  } m_DebugInfo;

  const ScopeData *GetScope(size_t offset) const;
};

// this does a 'safe' value assignment, by doing parallel depth-first iteration of both variables
// and only copying the value itself. This ensures we don't change any locations that might be
// pointed to. Assignments should only ever be between compatible types so this should be safe.
void AssignValue(ShaderVariable &dst, const ShaderVariable &src);

};    // namespace rdcspv
