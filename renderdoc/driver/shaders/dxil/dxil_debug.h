/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
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

#include <set>
#include "common/common.h"
#include "driver/shaders/dxbc/dxbc_bytecode.h"
#include "driver/shaders/dxbc/dxbc_container.h"
#include "dxil_bytecode.h"

namespace DXILDebug
{
using namespace DXDebug;

typedef DXDebug::SampleGatherResourceData SampleGatherResourceData;
typedef DXDebug::SampleGatherSamplerData SampleGatherSamplerData;
typedef DXDebug::BindingSlot BindingSlot;
typedef DXDebug::GatherChannel GatherChannel;
typedef DXBCBytecode::SamplerMode SamplerMode;
typedef DXBC::InterpolationMode InterpolationMode;

typedef uint32_t Id;
class Debugger;
struct GlobalState;

struct InstructionRange
{
  uint32_t min;
  uint32_t max;
};

typedef std::map<ShaderBuiltin, ShaderVariable> BuiltinInputs;
typedef std::set<Id> ReferencedIds;
typedef std::map<Id, InstructionRange> InstructionRangePerId;

void GetInterpolationModeForInputParams(const rdcarray<SigParameter> &stageInputSig,
                                        const DXIL::Program *program,
                                        rdcarray<DXBC::InterpolationMode> &interpModes);

struct PSInputData
{
  PSInputData(int inputIndex, int numWords, ShaderBuiltin sysAttribute, bool inc, void *pData)
  {
    input = inputIndex;
    numwords = numWords;
    sysattribute = sysAttribute;
    included = inc;
    data = pData;
  }

  void *data;
  ShaderBuiltin sysattribute;
  int input;
  int numwords;
  bool included;
};

void ApplyAllDerivatives(GlobalState &global, rdcarray<ThreadState> &quad, int destIdx,
                         const rdcarray<PSInputData> &psInputs, float *data);

struct FunctionInfo
{
  const DXIL::Function *function = NULL;
  ReferencedIds referencedIds;
  InstructionRangePerId rangePerId;
  uint32_t globalInstructionOffset = ~0U;
};

struct StackFrame
{
  StackFrame(const DXIL::Function *func) : function(func) {}
  const DXIL::Function *function;

  // the thread's live and dormant lists before the function was entered
  rdcarray<uint32_t> live;
  rdcarray<uint32_t> dormant;
};

class DebugAPIWrapper
{
public:
  // During shader debugging, when a new resource is encountered
  // These will be called to fetch the data on demand.
  virtual void FetchSRV(const BindingSlot &slot) = 0;
  virtual void FetchUAV(const BindingSlot &slot) = 0;

  virtual bool CalculateMathIntrinsic(DXIL::DXOp dxOp, const ShaderVariable &input,
                                      ShaderVariable &output) = 0;
  virtual bool CalculateSampleGather(DXIL::DXOp dxOp, SampleGatherResourceData resourceData,
                                     SampleGatherSamplerData samplerData, const ShaderVariable &uv,
                                     const ShaderVariable &ddxCalc, const ShaderVariable &ddyCalc,
                                     const int8_t texelOffsets[3], int multisampleIndex,
                                     float lodOrCompareValue, const uint8_t swizzle[4],
                                     GatherChannel gatherChannel, DXBC::ShaderType shaderType,
                                     uint32_t instructionIdx, const char *opString,
                                     ShaderVariable &output) = 0;
  virtual ShaderVariable GetResourceInfo(DXIL::ResourceClass resClass,
                                         const DXDebug::BindingSlot &slot, uint32_t mipLevel,
                                         const DXBC::ShaderType shaderType, int &dim) = 0;
  virtual ShaderVariable GetSampleInfo(DXIL::ResourceClass resClass, const DXDebug::BindingSlot &slot,
                                       const DXBC::ShaderType shaderType, const char *opString) = 0;
  virtual ShaderVariable GetRenderTargetSampleInfo(const DXBC::ShaderType shaderType,
                                                   const char *opString) = 0;
  virtual bool IsResourceBound(DXIL::ResourceClass resClass, const DXDebug::BindingSlot &slot) = 0;
};

struct ThreadState
{
  ThreadState(uint32_t workgroupIndex, Debugger &debugger, const GlobalState &globalState);
  ~ThreadState();

  void EnterFunction(const DXIL::Function *function, const rdcarray<DXIL::Value *> &args);
  void EnterEntryPoint(const DXIL::Function *function, ShaderDebugState *state);
  void StepNext(ShaderDebugState *state, DebugAPIWrapper *apiWrapper,
                const rdcarray<ThreadState> &workgroups);

  bool Finished() const;
  bool ExecuteInstruction(DebugAPIWrapper *apiWrapper, const rdcarray<ThreadState> &workgroups);

  void MarkResourceAccess(const rdcstr &name, const DXIL::ResourceReference *resRef);
  void SetResult(const Id &id, ShaderVariable &result, DXIL::Operation op, DXIL::DXOp dxOpCode,
                 ShaderEvents flags);
  rdcstr GetArgumentName(uint32_t i) const;
  Id GetArgumentId(uint32_t i) const;
  const DXIL::ResourceReference *GetResource(rdcstr handle);
  bool GetShaderVariable(const DXIL::Value *dxilValue, DXIL::Operation op, DXIL::DXOp dxOpCode,
                         ShaderVariable &var, bool flushDenormInput = true) const;
  bool GetVariable(const Id &id, DXIL::Operation opCode, DXIL::DXOp dxOpCode,
                   ShaderVariable &var) const;

  void PerformGPUResourceOp(const rdcarray<ThreadState> &workgroups, DXIL::Operation opCode,
                            DXIL::DXOp dxOpCode, const DXIL::ResourceReference *resRef,
                            DebugAPIWrapper *apiWrapper, const DXIL::Instruction &inst,
                            ShaderVariable &result);
  void Sub(const ShaderVariable &a, const ShaderVariable &b, ShaderValue &ret) const;

  ShaderValue DDX(bool fine, DXIL::Operation opCode, DXIL::DXOp dxOpCode,
                  const rdcarray<ThreadState> &quad, const Id &id) const;
  ShaderValue DDY(bool fine, DXIL::Operation opCode, DXIL::DXOp dxOpCode,
                  const rdcarray<ThreadState> &quad, const Id &id) const;

  void ProcessScopeChange(const rdcarray<Id> &oldLive, const rdcarray<Id> &newLive);

  void InitialiseHelper(const ThreadState &activeState);

  struct StackAlloc
  {
    void *backingMemory;
    size_t size;
  };

  struct StackAllocPointer
  {
    Id baseMemoryId;
    void *backingMemory;
    size_t size;
  };

  struct
  {
    uint32_t coverage;
    uint32_t primID;
    uint32_t isFrontFace;
  } m_Semantics;

  Debugger &m_Debugger;
  const DXIL::Program &m_Program;
  const GlobalState &m_GlobalState;

  rdcarray<StackFrame *> m_Callstack;
  ShaderDebugState *m_State = NULL;

  ShaderVariable m_Input;
  ShaderVariable m_Output;
  uint32_t m_OutputSSAId = ~0U;

  // Known active SSA ShaderVariables
  std::map<Id, ShaderVariable> m_LiveVariables;
  // Known dormant SSA ShaderVariables
  std::map<Id, ShaderVariable> m_DormantVariables;
  // Live variables at the current scope
  rdcarray<Id> m_Live;
  // Dormant variables at the current scope
  rdcarray<Id> m_Dormant;

  const FunctionInfo *m_FunctionInfo = NULL;
  DXBC::ShaderType m_ShaderType;

  // Track stack allocations
  // A single global stack, do not bother popping when leaving functions
  size_t m_StackAllocTop = 0;
  std::map<Id, StackAlloc> m_StackAllocs;
  std::map<Id, StackAllocPointer> m_StackAllocPointers;

  // The instruction index within the current function
  uint32_t m_FunctionInstructionIdx = ~0U;
  const DXIL::Instruction *m_CurrentInstruction = NULL;
  // The current and previous function basic block index
  uint32_t m_Block = ~0U;
  uint32_t m_PreviousBlock = ~0U;
  // A global logical instruction index (bit like a PC) not the instruction index within a function
  uint32_t m_GlobalInstructionIdx = ~0U;

  rdcarray<ShaderBindIndex> m_accessedSRVs;
  rdcarray<ShaderBindIndex> m_accessedUAVs;

  // index in the pixel quad
  uint32_t m_WorkgroupIndex = ~0U;
  bool m_Killed = true;
  bool m_Ended = true;
};

struct GlobalState
{
  GlobalState() = default;
  BuiltinInputs builtinInputs;

  struct ViewFmt
  {
    int byteWidth = 0;
    int numComps = 0;
    CompType fmt = CompType::Typeless;
    int stride = 0;

    int Stride() const
    {
      if(stride != 0)
        return stride;

      if(byteWidth == 10 || byteWidth == 11)
        return 4;    // 10 10 10 2 or 11 11 10

      return byteWidth * numComps;
    }
  };

  struct UAVData
  {
    UAVData()
        : firstElement(0), numElements(0), tex(false), rowPitch(0), depthPitch(0), hiddenCounter(0)
    {
    }

    bytebuf data;
    uint32_t firstElement;
    uint32_t numElements;

    bool tex;
    uint32_t rowPitch, depthPitch;

    ViewFmt format;

    uint32_t hiddenCounter;
  };
  std::map<BindingSlot, UAVData> uavs;
  typedef std::map<BindingSlot, UAVData>::const_iterator UAVIterator;

  struct SRVData
  {
    SRVData() : firstElement(0), numElements(0) {}
    bytebuf data;
    uint32_t firstElement;
    uint32_t numElements;

    ViewFmt format;
  };

  std::map<BindingSlot, SRVData> srvs;
  typedef std::map<BindingSlot, SRVData>::const_iterator SRVIterator;

  // allocated storage for opaque uniform blocks, does not change over the course of debugging
  rdcarray<ShaderVariable> constantBlocks;

  // workgroup private variables
  rdcarray<ShaderVariable> workgroups;

  // resources may be read-write but the variable itself doesn't change
  rdcarray<ShaderVariable> readOnlyResources;
  rdcarray<ShaderVariable> readWriteResources;
  rdcarray<ShaderVariable> samplers;
  // Globals across workgroups including inputs (immutable) and outputs (mutable)
  rdcarray<ShaderVariable> globals;
};

class Debugger : public DXBCContainerDebugger
{
public:
  Debugger() : DXBCContainerDebugger(true){};
  ShaderDebugTrace *BeginDebug(uint32_t eventId, const DXBC::DXBCContainer *dxbcContainer,
                               const ShaderReflection &reflection, uint32_t activeLaneIndex);
  rdcarray<ShaderDebugState> ContinueDebug(DebugAPIWrapper *apiWrapper);
  GlobalState &GetGlobalState() { return m_GlobalState; }
  ThreadState &GetActiveLane() { return m_Workgroups[m_ActiveLaneIndex]; }
  ThreadState &GetWorkgroup(const uint32_t i) { return m_Workgroups[i]; }
  rdcarray<ThreadState> &GetWorkgroups() { return m_Workgroups; }
  const rdcarray<Id> &GetLiveGlobals() { return m_LiveGlobals; }
  static rdcstr GetResourceReferenceName(const DXIL::Program *program, DXIL::ResourceClass resClass,
                                         const BindingSlot &slot);
  const DXIL::Program &GetProgram() const { return *m_Program; }
  const DXBC::DXBCContainer *const GetDXBCContainer() { return m_DXBC; }
  uint32_t GetEventId() { return m_EventId; }
  const FunctionInfo *GetFunctionInfo(const DXIL::Function *function) const;

private:
  rdcarray<ThreadState> m_Workgroups;
  std::map<const DXIL::Function *, FunctionInfo> m_FunctionInfos;

  // the live mutable global variables, to initialise a stack frame's live list
  rdcarray<Id> m_LiveGlobals;

  GlobalState m_GlobalState;

  const DXBC::DXBCContainer *m_DXBC = NULL;
  const DXIL::Program *m_Program = NULL;

  uint32_t m_EventId = 0;
  uint32_t m_ActiveLaneIndex = 0;
};

};    // namespace DXILDebug
