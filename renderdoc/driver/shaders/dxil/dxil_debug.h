/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024-2025 Baldur Karlsson
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
#include "driver/shaders/dxbc/dx_debug.h"
#include "driver/shaders/dxbc/dxbc_bytecode.h"
#include "driver/shaders/dxbc/dxbc_container.h"
#include "shaders/controlflow.h"
#include "dxil_bytecode.h"
#include "dxil_controlflow.h"
#include "dxil_debuginfo.h"

#if ENABLED(RDOC_RELEASE)
#define DXIL_DEBUG_RDCASSERT(...) \
  do                              \
  {                               \
    (void)(__VA_ARGS__);          \
  } while((void)0, 0)
#define DXIL_DEBUG_RDCASSERTEQUAL(...) \
  do                                   \
  {                                    \
    (void)(__VA_ARGS__);               \
  } while((void)0, 0)
#else
#define DXIL_DEBUG_RDCASSERT(...) RDCASSERTMSG("", __VA_ARGS__)
#define DXIL_DEBUG_RDCASSERTEQUAL(a, b) RDCASSERTEQUAL(a, b)
#endif

namespace DXILDebug
{
using namespace DXDebug;

typedef DXDebug::SampleGatherResourceData SampleGatherResourceData;
typedef DXDebug::SampleGatherSamplerData SampleGatherSamplerData;
typedef DXDebug::BindingSlot BindingSlot;
typedef DXDebug::GatherChannel GatherChannel;
typedef DXBCBytecode::SamplerMode SamplerMode;
typedef DXBC::InterpolationMode InterpolationMode;

class Debugger;
struct GlobalState;

// D3D12 descriptors are equal sized and treated as effectively one byte in size
const uint32_t D3D12_DESCRIPTOR_BYTESIZE = 1;

enum class DeviceOpResult : uint32_t
{
  Unknown,
  Succeeded,
  Failed,
  NeedsDevice,
};

inline void AtomicStore(int32_t *var, int32_t newVal)
{
  int32_t oldVal = *var;
  while(Atomic::CmpExch32(var, oldVal, newVal) != oldVal)
  {
    oldVal = *var;
  };
}

inline int32_t AtomicLoad(int32_t *var)
{
  return Atomic::CmpExch32(var, 0, 0);
}

inline int32_t AtomicLoad(const int32_t *var)
{
  return Atomic::CmpExch32((int32_t *)var, 0, 0);
}

struct ExecPointReference
{
  ExecPointReference() : block(~0U), instruction(~0U) {}
  ExecPointReference(uint32_t block, uint32_t instruction) : block(block), instruction(instruction)
  {
  }
  bool IsAfter(const ExecPointReference &from, const DXIL::ControlFlow &controlFlow) const;
  bool IsValid() const { return block != ~0U && instruction != ~0U; }

  uint32_t block;
  uint32_t instruction;
};

void GetInterpolationModeForInputParams(const rdcarray<SigParameter> &stageInputSig,
                                        const DXIL::Program *program,
                                        rdcarray<DXBC::InterpolationMode> &interpModes);

struct InputData
{
  InputData(int inputIndex, int arrayIndex, int numWords, ShaderBuiltin sysAttribute, bool inc,
            void *pData)
  {
    input = inputIndex;
    array = arrayIndex;
    numwords = numWords;
    sysattribute = sysAttribute;
    included = inc;
    data = pData;
  }

  void *data;
  ShaderBuiltin sysattribute;
  int input;
  int array;
  int numwords;
  bool included;
};

struct FunctionInfo
{
  typedef std::set<Id> ReferencedIds;
  typedef rdcarray<ExecPointReference> ExecutionPointPerId;
  typedef std::map<uint32_t, ReferencedIds> PhiReferencedIdsPerBlock;
  typedef rdcarray<rdcstr> Callstack;

  const DXIL::Function *function = NULL;
  ReferencedIds referencedIds;
  ExecutionPointPerId maxExecPointPerId;
  PhiReferencedIdsPerBlock phiReferencedIdsPerBlock;
  uint32_t globalInstructionOffset = ~0U;
  rdcarray<uint32_t> uniformBlocks;
  rdcarray<uint32_t> divergentBlocks;
  rdcarray<DXIL::ConvergentBlockData> convergentBlocks;
  rdcarray<DXIL::PartialConvergentBlockData> partialConvergentBlocks;
  DXIL::ControlFlow controlFlow;
  std::map<uint32_t, Callstack> callstacks;
  rdcarray<uint32_t> instructionToBlock;
};

struct StackFrame
{
  StackFrame(const DXIL::Function *func) : function(func) {}
  const DXIL::Function *function;

  // the thread's live list before the function was entered
  rdcarray<bool> live;
};

struct GlobalVariable
{
  Id id;
  ShaderVariable var;
  bool gsm;
};

struct GlobalConstant
{
  Id id;
  ShaderVariable var;
};

struct ResourceReferenceInfo
{
  ResourceReferenceInfo()
      : resClass(DXIL::ResourceClass::Invalid),
        varType(VarType::Unknown),
        descType(DescriptorType::Unknown)
  {
  }
  void Create(const DXIL::ResourceReference *resRef, uint32_t arrayIndex);
  bool Valid() const { return resClass != DXIL::ResourceClass::Invalid; }

  DXIL::ResourceClass resClass;
  BindingSlot binding;
  DescriptorType descType;
  VarType varType;

  struct SRVData
  {
    DXDebug::ResourceDimension dim;
    uint32_t sampleCount;
    DXDebug::ResourceRetType compType;
  };
  struct SamplerData
  {
    SamplerMode samplerMode;
  };
  union
  {
    SRVData srvData;
    SamplerData samplerData;
  };
};

struct ConstantBlockReference
{
  size_t constantBlockIndex;
  size_t arrayIndex;

  bool operator<(const ConstantBlockReference &other) const
  {
    if(constantBlockIndex != other.constantBlockIndex)
      return constantBlockIndex < other.constantBlockIndex;
    return arrayIndex < other.arrayIndex;
  }
};

struct ViewFmt
{
  int byteWidth = 0;
  int numComps = 0;
  CompType compType = CompType::Typeless;
  int stride = 0;
};

struct ResourceInfo
{
  ResourceInfo() : firstElement(0), numElements(0), isByteBuffer(false), isRootDescriptor(false) {}

  size_t dataSize = 0;
  uint32_t firstElement;
  uint32_t numElements;

  bool hasData = false;
  bool isByteBuffer;
  bool isRootDescriptor;
  // Buffer stride is stored in format.stride
  ViewFmt format;
};

struct UAVInfo
{
  UAVInfo() = default;

  ResourceInfo resInfo;

  uint32_t rowPitch = 0;
  uint32_t depthPitch = 0;
  uint32_t hiddenCounter = 0;
  bool tex = false;
};

struct SRVInfo
{
  SRVInfo() = default;

  ResourceInfo resInfo;
};

enum class ThreadProperty : uint32_t
{
  Helper,
  QuadId,
  QuadLane,
  Active,
  SubgroupIdx,
  Count,
};

struct ThreadProperties
{
  rdcfixedarray<uint32_t, arraydim<ThreadProperty>()> props;

  uint32_t &operator[](ThreadProperty p)
  {
    if(p >= ThreadProperty::Count)
      return props[0];
    return props[(uint32_t)p];
  }

  uint32_t operator[](ThreadProperty p) const
  {
    if(p >= ThreadProperty::Count)
      return 0;
    return props[(uint32_t)p];
  }
};

typedef rdcflatmap<ShaderBuiltin, ShaderVariable> BuiltinInputs;

class DebugAPIWrapper
{
public:
  virtual ~DebugAPIWrapper() {}

  virtual ShaderValue TypedUAVLoad(const BindingSlot &slot, const DXILDebug::ViewFmt &fmt,
                                   uint64_t dataOffset) const = 0;
  virtual ShaderValue TypedSRVLoad(const BindingSlot &slot, const DXILDebug::ViewFmt &fmt,
                                   uint64_t dataOffset) const = 0;
  virtual bool TypedUAVStore(const BindingSlot &slot, const DXILDebug::ViewFmt &fmt,
                             uint64_t dataOffset, const ShaderValue &value) = 0;
  virtual bool TypedSRVStore(const BindingSlot &slot, const DXILDebug::ViewFmt &fmt,
                             uint64_t dataOffset, const ShaderValue &value) = 0;

  // These will fetch the data on demand.
  virtual UAVInfo GetUAV(const BindingSlot &slot) = 0;
  virtual SRVInfo GetSRV(const BindingSlot &slot) = 0;

  virtual bool QueueMathIntrinsic(DXIL::DXOp dxOp, const ShaderVariable &input) = 0;
  virtual bool QueueSampleGather(DXIL::DXOp dxOp, SampleGatherResourceData resourceData,
                                 SampleGatherSamplerData samplerData, const ShaderVariable &uv,
                                 const ShaderVariable &ddxCalc, const ShaderVariable &ddyCalc,
                                 const int8_t texelOffsets[3], int multisampleIndex, float lodValue,
                                 float compareValue, GatherChannel gatherChannel,
                                 uint32_t instructionIdx, int &sampleRetType) = 0;
  virtual bool GetQueuedResults(rdcarray<ShaderVariable *> &mathOpResults,
                                rdcarray<ShaderVariable *> &sampleGatherResults,
                                const rdcarray<int> &sampleRetTypes) = 0;
  virtual bool QueuedOpsHasSpace() const = 0;
  virtual ShaderVariable GetResourceInfo(DXIL::ResourceClass resClass,
                                         const DXDebug::BindingSlot &slot, uint32_t mipLevel) = 0;
  virtual ShaderVariable GetSampleInfo(DXIL::ResourceClass resClass,
                                       const DXDebug::BindingSlot &slot, const char *opString) = 0;
  virtual ShaderVariable GetRenderTargetSampleInfo(const char *opString) = 0;
  virtual ResourceReferenceInfo GetResourceReferenceInfo(const DXDebug::BindingSlot &slot) = 0;
  virtual ShaderDirectAccess GetShaderDirectAccess(DescriptorType type,
                                                   const DXDebug::BindingSlot &slot) = 0;

  virtual bool IsSRVCached(const DXDebug::BindingSlot &slot) const = 0;
  virtual bool IsUAVCached(const DXDebug::BindingSlot &slot) const = 0;
  virtual bool IsResourceInfoCached(const DXDebug::BindingSlot &slot, uint32_t mipLevel) = 0;
  virtual bool IsSampleInfoCached(const DXDebug::BindingSlot &slot) = 0;
  virtual bool IsRenderTargetSampleInfoCached() = 0;
  virtual bool IsResourceReferenceInfoCached(const DXDebug::BindingSlot &slot) = 0;
  virtual bool IsShaderDirectAccessCached(const DXDebug::BindingSlot &slot) = 0;

  virtual const ShaderVariable &GetInputPlaceholder() const = 0;
  virtual const rdcarray<DXILDebug::ThreadProperties> &GetWorkgroupProperties() const = 0;
  virtual const rdcarray<ShaderVariable> &GetConstantBlocks() const = 0;
  virtual const std::map<ConstantBlockReference, bytebuf> &GetConstantBlocksDatas() const = 0;
  virtual const BuiltinInputs &GetBuiltins() const = 0;
  virtual uint32_t GetSubgroupSize() const = 0;
  virtual const rdcarray<rdcflatmap<ShaderBuiltin, ShaderVariable>> &GetThreadsBuiltins() const = 0;
  virtual const rdcarray<ShaderVariable> &GetThreadsInputs() const = 0;
  virtual const rdcarray<SourceVariableMapping> &GetSourceVars() const = 0;
};

struct MemoryTracking
{
  void AllocateMemoryForType(const DXIL::Type *type, Id allocId, bool globalVar, bool gsm,
                             ShaderVariable &var);
  void ConvertGlobalAllocToLocal(Id allocId);

  // Represents actual memory allocations (think of it like a memory heap)
  struct Allocation
  {
    // the allocated memory
    void *backingMemory;
    uint64_t size;
    bool globalVarAlloc;
    bool gsm;
    bool localMemory;
  };

  // Represents pointers within a Allocation memory allocation (heap)
  struct Pointer
  {
    // the Allocation that owns the memory pointed to
    Id baseMemoryId;
    // the memory pointer which will be within the Allocation backing memory
    void *memory;
    // size of the data the pointer
    uint64_t size;
  };

  // Memory allocations with backing memory (heaps)
  std::map<Id, Allocation> m_Allocations;
  // Pointers within a Allocation memory allocation, the allocated memory will be in m_Allocations
  std::map<Id, Pointer> m_Pointers;
};

struct GpuMathOperation
{
  void Clear()
  {
    workgroupIndex = 0;
    dxOp = DXIL::DXOp::NumOpCodes;
    input = ShaderVariable();
    result = NULL;
  }
  uint32_t workgroupIndex;
  DXIL::DXOp dxOp;
  ShaderVariable input;
  ShaderVariable *result;
};

struct GpuSampleGatherOperation
{
  void Clear()
  {
    workgroupIndex = 0;
    dxOp = DXIL::DXOp::NumOpCodes;
    resourceData = SampleGatherResourceData();
    samplerData = SampleGatherSamplerData();
    uv = ddxCalc = ddyCalc = ShaderVariable();
    texelOffsets[0] = 0;
    texelOffsets[1] = 0;
    texelOffsets[2] = 0;
    multisampleIndex = ~0U;
    lodValue = 0.0f;
    compareValue = 0.0f;
    gatherChannel = GatherChannel::Red;
    instructionIdx = ~0U;
    result = NULL;
  }
  uint32_t workgroupIndex;
  DXIL::DXOp dxOp;
  SampleGatherResourceData resourceData;
  SampleGatherSamplerData samplerData;
  ShaderVariable uv;
  ShaderVariable ddxCalc;
  ShaderVariable ddyCalc;
  int8_t texelOffsets[3];
  int multisampleIndex;
  float lodValue;
  float compareValue;
  GatherChannel gatherChannel;
  uint32_t instructionIdx;
  ShaderVariable *result = NULL;
};

struct ThreadState
{
  ThreadState(Debugger &debugger, const GlobalState &globalState, uint32_t maxSSAId,
              uint32_t laneIndex, uint32_t numThreads);
  ~ThreadState();

  void EnterEntryPoint(const DXIL::Function *function, bool hasDebugState);
  void StepNext(bool hasDebugState, const rdcarray<ThreadState> &workgroup);
  void StepOverNopInstructions();
  void FillCallstack(ShaderDebugState &state);
  void RetireLiveIDs();

  bool Finished() const;
  bool IsSimulationStepActive() const { return (AtomicLoad(&atomic_isSimulationStepActive) == 1); }
  bool CanRunAnotherStep() const;
  const ShaderVariable &GetInput() const { return m_Input; }
  const GlobalVariable &GetOutput() const { return m_Output; }
  const BuiltinInputs &GetBuiltins() const { return m_Builtins; }
  bool IsDead() const { return m_Dead; }
  uint32_t GetQuadId() const { return m_QuadId; }
  uint32_t GetQuadLaneIndex() const { return m_QuadLaneIndex; }
  uint32_t GetActiveGlobalInstructionIdx() const { return m_ActiveGlobalInstructionIdx; }
  DXIL::BlockArray GetEnteredPoints() const { return m_EnteredPoints; }
  uint32_t GetConvergencePoint() const { return m_ConvergencePoint; }
  bool GetDiverged() const { return m_Diverged; }
  const DXIL::BlockArray *GetPartialConvergencePoints() const
  {
    return &m_PartialConvergencePoints;
  }
  const ShaderDebugState &GetPendingDebugState() const { return m_PendingDebugState; }
  const GpuMathOperation &GetQueuedGpuMathOp() const
  {
    DXIL_DEBUG_RDCASSERT(AtomicLoad(&atomic_stepNeedsGpuMathOp));
    DXIL_DEBUG_RDCASSERT(IsPendingResultPending());
    return m_QueuedGpuMathOp;
  }
  const GpuSampleGatherOperation &GetQueuedGpuSampleGatherOp() const
  {
    DXIL_DEBUG_RDCASSERT(AtomicLoad(&atomic_stepNeedsGpuSampleGatherOp));
    DXIL_DEBUG_RDCASSERT(IsPendingResultPending());
    return m_QueuedGpuSampleGatherOp;
  }
  bool StepNeedsDeviceThread() const { return (AtomicLoad(&atomic_stepNeedsDeviceThread) == 1); }
  bool StepNeedsGpuSampleGatherOp() const
  {
    return (AtomicLoad(&atomic_stepNeedsGpuSampleGatherOp) == 1);
  }
  bool StepNeedsGpuMathOp() const { return (AtomicLoad(&atomic_stepNeedsGpuMathOp) == 1); }

  void SetBuiltins(const BuiltinInputs &builtins) { m_Builtins = builtins; }
  void SetInput(const ShaderVariable &input) { m_Input = input; }
  void SetOutput(const Id id, const ShaderVariable &var)
  {
    m_Output.id = id;
    m_Output.var = var;
  }
  void SetDead(bool dead) { m_Dead = dead; }
  void SetHelper(bool helper) { m_Helper = helper; }
  void SetQuadLaneIndex(uint32_t quadLaneIndex) { m_QuadLaneIndex = quadLaneIndex; }
  void SetQuadId(uint32_t quadId) { m_QuadId = quadId; }
  void SetSubgroupIdx(uint32_t subgroupIdx) { m_SubgroupIdx = subgroupIdx; }
  void SetQuadNeighbours(uint32_t lane, uint32_t index) { m_QuadNeighbours[lane] = index; }
  void SetActiveMask(const rdcarray<bool> &activeMask)
  {
    RDCASSERTEQUAL(m_ActiveMask.size(), activeMask.size());
    memcpy(m_ActiveMask.data(), activeMask.data(), activeMask.size() * sizeof(bool));
  }
  void UpdateCurrentInstruction()
  {
    m_CurrentGlobalInstructionIdx = m_ActiveGlobalInstructionIdx;
    m_CurrentBlock = m_Block;
  }
  void SetSimulationStepCompleted() { AtomicStore(&atomic_isSimulationStepActive, 0); }
  void SetStepQueued()
  {
    AtomicStore(&atomic_isSimulationStepActive, 1);
    AtomicStore(&atomic_stepNeedsGpuSampleGatherOp, 0);
    AtomicStore(&atomic_stepNeedsGpuMathOp, 0);
    AtomicStore(&atomic_stepNeedsDeviceThread, 0);
  }
  void SetPendingResultUnknown() { SetPendingResultStatus(PendingResultStatus::Unknown); }
  void SetPendingResultReady()
  {
    DXIL_DEBUG_RDCASSERTEQUAL(GetPendingResultStatus(), PendingResultStatus::Pending);
    SetPendingResultStatus(PendingResultStatus::Ready);
  }

  void InitialiseFromActive(const ThreadState &active)
  {
    m_Variables = active.m_Variables;
    m_Assigned = active.m_Assigned;
    m_Live = active.m_Live;
    m_IsGlobal = active.m_IsGlobal;
  }

  void UpdateBackingMemoryFromVariable(void *ptr, uint64_t &allocSize, const ShaderVariable &var);

  void ClearPendingDebugState()
  {
    m_PendingDebugState.changes.clear();
    m_PendingDebugState.flags = ShaderEvents::NoEvent;
    m_PendingDebugState.nextInstruction = 0;
  }

  enum class PendingResultStatus : int32_t
  {
    Unknown,
    Pending,
    Ready,
    Stepped,
  };

private:
  PendingResultStatus GetPendingResultStatus() const
  {
    return (PendingResultStatus)AtomicLoad(&atomic_pendingResultStatus);
  }

  void SetPendingResultStatus(PendingResultStatus status)
  {
    AtomicStore(&atomic_pendingResultStatus, (int32_t)status);
  }

  bool IsPendingResultPending() const
  {
    return GetPendingResultStatus() == PendingResultStatus::Pending;
  }
  bool IsPendingResultReady() const
  {
    return GetPendingResultStatus() == PendingResultStatus::Ready;
  }
  const ShaderVariable &GetPendingResult() const
  {
    DXIL_DEBUG_RDCASSERTEQUAL(GetPendingResultStatus(), PendingResultStatus::Ready);
    return m_PendingResultData;
  }
  void SetStepNeedsGpuSampleGatherOp()
  {
    AtomicStore(&atomic_stepNeedsGpuSampleGatherOp, 1);
    SetPendingResultStatus(PendingResultStatus::Pending);
  }
  void SetStepNeedsGpuMathOp()
  {
    AtomicStore(&atomic_stepNeedsGpuMathOp, 1);
    SetPendingResultStatus(PendingResultStatus::Pending);
  }
  void SetStepNeedsDeviceThread()
  {
    AtomicStore(&atomic_stepNeedsDeviceThread, 1);
    SetPendingResultStatus(PendingResultStatus::Pending);
  }

  void EnterFunction(const DXIL::Function *function, const rdcarray<DXIL::Value *> &args);

  bool InUniformBlock() const;

  bool JumpToBlock(const DXIL::Block *target, bool divergencePoint);
  bool ExecuteInstruction(const rdcarray<ThreadState> &workgroup);

  void MarkResourceAccess(const ShaderVariable &var);
  void SetResult(const Id &id, ShaderVariable &result, DXIL::Operation op, DXIL::DXOp dxOpCode,
                 ShaderEvents flags);
  rdcstr GetArgumentName(uint32_t i) const;
  Id GetArgumentId(uint32_t i) const;
  ResourceReferenceInfo GetResource(Id handleId, bool &annotatedHandle,
                                    ShaderVariable &handleVar) const;

  // This must be a thread safe operation using only thread safe containers
  bool GetShaderVariableFromLane(const ThreadState &lane, const DXIL::Value *dxilValue,
                                 DXIL::Operation op, DXIL::DXOp dxOpCode, ShaderVariable &var) const
  {
    return lane.GetShaderVariableHelper(dxilValue, op, dxOpCode, var, true, true, true);
  }
  bool GetShaderVariable(const DXIL::Value *dxilValue, DXIL::Operation op, DXIL::DXOp dxOpCode,
                         ShaderVariable &var, bool flushDenormInput = true) const
  {
    return GetShaderVariableHelper(dxilValue, op, dxOpCode, var, flushDenormInput, true, false);
  }

  bool GetPhiShaderVariable(const DXIL::Value *dxilValue, DXIL::Operation op, DXIL::DXOp dxOpCode,
                            ShaderVariable &var, bool flushDenormInput = true) const
  {
    return GetShaderVariableHelper(dxilValue, op, dxOpCode, var, flushDenormInput, false, false);
  }

  // This must be a thread safe operation using only thread safe containers
  bool GetLiveVariable(const Id &id, DXIL::Operation opCode, DXIL::DXOp dxOpCode,
                       bool ignoreLiveCheck, ShaderVariable &var) const;
  bool GetPhiVariable(const Id &id, DXIL::Operation opCode, DXIL::DXOp dxOpCode,
                      ShaderVariable &var) const;
  bool GetVariableHelper(DXIL::Operation op, DXIL::DXOp dxOpCode, ShaderVariable &var) const;
  void UpdateMemoryVariableFromBackingMemory(Id memoryId, const void *ptr);
  void UpdateGlobalBackingMemory(Id ptrId, const MemoryTracking::Pointer &ptr,
                                 const MemoryTracking::Allocation &allocation,
                                 const ShaderVariable &val);
  bool LoadGSMFromGlobalBackingMemory(const MemoryTracking::Pointer &ptr,
                                      const MemoryTracking::Allocation &allocation,
                                      ShaderVariable &var);

  bool PerformGPUResourceOp(const rdcarray<ThreadState> &workgroup, DXIL::Operation opCode,
                            DXIL::DXOp dxOpCode, const ResourceReferenceInfo &resRef,
                            const DXIL::Instruction &inst, ShaderVariable &result);
  void ConvertSampleGatherReturn(DXIL::DXOp dxOpCode, const DXIL::Instruction &inst,
                                 const ShaderVariable &data, ShaderVariable &result) const;
  void Sub(const ShaderVariable &a, const ShaderVariable &b, ShaderValue &ret) const;

  ShaderValue DDX(bool fine, DXIL::Operation opCode, DXIL::DXOp dxOpCode,
                  const rdcarray<ThreadState> &workgroup, const DXIL::Value *dxilValue) const;
  ShaderValue DDY(bool fine, DXIL::Operation opCode, DXIL::DXOp dxOpCode,
                  const rdcarray<ThreadState> &workgroup, const DXIL::Value *dxilValue) const;

  void ProcessScopeChange(const rdcarray<bool> &oldLive, const rdcarray<bool> &newLive);

  void ExecuteMemoryBarrier();
  static bool WorkgroupIsDiverged(const rdcarray<ThreadState> &workgroup);
  static bool QuadIsDiverged(const rdcarray<ThreadState> &workgroup,
                             const rdcfixedarray<uint32_t, 4> &quadNeighbours);
  static bool SubgroupIsDiverged(const rdcarray<ThreadState> &workgroup,
                                 const rdcarray<uint32_t> &activeLanes);

  // When getting live variables : this must be a thread safe operation using only thread safe containers
  bool GetShaderVariableHelper(const DXIL::Value *dxilValue, DXIL::Operation op,
                               DXIL::DXOp dxOpCode, ShaderVariable &var, bool flushDenormInput,
                               bool isLive, bool ignoreLiveCheck) const;
  bool IsVariableAssigned(const Id id) const;

  ShaderVariable GetBuiltin(ShaderBuiltin builtin) const;
  uint32_t GetSubgroupActiveLanes(const rdcarray<ThreadState> &workgroup,
                                  rdcarray<uint32_t> &activeLanes) const;

  void QueueMathOp(DXIL::DXOp dxOp, const ShaderVariable &input, ShaderVariable &result);
  void QueueSampleGather(DXIL::DXOp dxOp, const SampleGatherResourceData &resourceData,
                         const SampleGatherSamplerData &samplerData, const ShaderVariable &uv,
                         const ShaderVariable &ddxCalc, const ShaderVariable &ddyCalc,
                         const int8_t texelOffsets[3], int multisampleIndex, float lodValue,
                         float compareValue, GatherChannel gatherChannel, uint32_t instructionIdx,
                         ShaderVariable &result);
  void OperationLoad(bool isAtomic, const DXIL::Instruction &inst, DXIL::Operation opCode,
                     DXIL::DXOp dxOpCode, Id &resultId, ShaderVariable &result);
  void OperationStore(const DXIL::Instruction &inst, DXIL::Operation opCode, DXIL::DXOp dxOpCode);
  void OperationAtomic(const DXIL::Instruction &inst, DXIL::Operation opCode, DXIL::DXOp dxOpCode,
                       Id &resultId, ShaderVariable &result);

  struct AnnotationProperties
  {
    DXIL::ResourceKind resKind;
    DXIL::ResourceClass resClass;
    uint32_t structStride;
  };

  BuiltinInputs m_Builtins;

  Debugger &m_Debugger;
  const DXIL::Program &m_Program;
  const GlobalState &m_GlobalState;

  rdcarray<StackFrame *> m_Callstack;
  bool m_HasDebugState = false;

  ShaderVariable m_Input;
  GlobalVariable m_Output;

  // Known SSA ShaderVariables : this must be a thread safe container
  rdcarray<ShaderVariable> m_Variables;
  // SSA Variables captured when a branch happens for use in phi nodes
  std::map<Id, ShaderVariable> m_PhiVariables;
  // Live variables at the current scope
  rdcarray<bool> m_Live;
  // Globals variables at the current scope
  rdcarray<bool> m_IsGlobal;
  // If the variable has been assigned a value : this must be a thread safe container
  rdcarray<bool> m_Assigned;
  // Annotated handle properties
  std::map<Id, AnnotationProperties> m_AnnotatedProperties;
  // ResourceReferenceInfo for any direct heap access bindings created using createHandleFromHeap
  std::map<Id, ResourceReferenceInfo> m_DirectHeapAccessBindings;
  // ConstantBlock information associated with a handle
  std::map<Id, ConstantBlockReference> m_ConstantBlockHandles;

  const FunctionInfo *m_FunctionInfo = NULL;
  DXBC::ShaderType m_ShaderType;

  rdcarray<bool> m_ActiveMask;

  ShaderDebugState m_PendingDebugState;
  ShaderVariable m_PendingResultData;
  GpuMathOperation m_QueuedGpuMathOp;
  GpuSampleGatherOperation m_QueuedGpuSampleGatherOp;

  // Track memory allocations
  // For stack allocations do not bother freeing when leaving functions
  MemoryTracking m_Memory;

  // The instruction index within the current function
  uint32_t m_FunctionInstructionIdx = 0;
  const DXIL::Instruction *m_CurrentInstruction = NULL;
  // The current and previous function basic block index
  uint32_t m_Block = ~0U;
  uint32_t m_PreviousBlock = ~0U;
  // The global PC of the active instruction that will be executed on the next simulation step
  uint32_t m_ActiveGlobalInstructionIdx = 0;
  // The global PC and block of the instruction that was last executed
  uint32_t m_CurrentGlobalInstructionIdx = 0;
  uint32_t m_CurrentBlock = ~0U;

  // true if executed an operation which could trigger divergence
  bool m_Diverged;
  // list of potential convergence points that were entered in a single step (used for tracking thread convergence)
  DXIL::BlockArray m_EnteredPoints;
  uint32_t m_ConvergencePoint;
  DXIL::BlockArray m_PartialConvergencePoints;

  // SSA Ids guaranteed to be greater than 0 and less than this value
  uint32_t m_MaxSSAId;

  // quad ID (arbitrary, just used to find neighbours for derivatives)
  uint32_t m_QuadId = 0;
  // index in the pixel quad (relative to the active lane)
  uint32_t m_QuadLaneIndex = ~0U;
  // the lane indices of our quad neighbours
  rdcfixedarray<uint32_t, 4> m_QuadNeighbours = {~0U, ~0U, ~0U, ~0U};
  // index in the workgroup
  uint32_t m_WorkgroupIndex = ~0U;
  // index in the subgroup
  uint32_t m_SubgroupIdx = ~0U;
  bool m_Dead = false;
  bool m_Ended = false;
  bool m_Helper = false;

  // These need to be accessed using atomics
  int32_t atomic_pendingResultStatus = (int32_t)PendingResultStatus::Unknown;
  int32_t atomic_stepNeedsGpuSampleGatherOp = 0;
  int32_t atomic_stepNeedsGpuMathOp = 0;
  int32_t atomic_stepNeedsDeviceThread = 0;
  int32_t atomic_isSimulationStepActive = 0;
};

struct GlobalState
{
  GlobalState() = default;
  ~GlobalState();
  BuiltinInputs builtins;
  uint32_t subgroupSize = 1;
  bool waveOpsIncludeHelpers = false;

  // allocated storage for opaque uniform blocks, does not change over the course of debugging
  rdcarray<ShaderVariable> constantBlocks;
  std::map<ConstantBlockReference, bytebuf> constantBlocksDatas;

  rdcarray<Id> groupSharedMemoryIds;
  // resources may be read-write but the variable itself doesn't change
  rdcarray<ShaderVariable> readOnlyResources;
  rdcarray<ShaderVariable> readWriteResources;
  rdcarray<ShaderVariable> samplers;
  // Globals across workgroup including inputs (immutable) and outputs (mutable)
  rdcarray<GlobalVariable> globals;
  // Constants across workgroup
  rdcarray<GlobalConstant> constants;
  // Memory created for global variables
  MemoryTracking memory;
};

struct LocalMapping
{
  bool operator<(const LocalMapping &o) const
  {
    if(sourceVarName != o.sourceVarName)
      return sourceVarName < o.sourceVarName;
    if(byteOffset != o.byteOffset)
      return byteOffset < o.byteOffset;
    if(countBytes != o.countBytes)
      return countBytes < o.countBytes;
    if(instIndex != o.instIndex)
      return instIndex < o.instIndex;
    if(isDeclare != o.isDeclare)
      return isDeclare;
    return debugVarSSAName < o.debugVarSSAName;
  }

  bool isSourceSupersetOf(const LocalMapping &o) const
  {
    // this mapping is a superset of the other if:

    // it's the same source var
    if(variable != o.variable)
      return false;

    if(sourceVarName != o.sourceVarName)
      return false;

    // it encompaases the other mapping
    if(byteOffset > o.byteOffset)
      return false;

    // countBytes = 0 means entire variable
    if(countBytes == 0)
      return true;

    if(o.countBytes == 0)
      return false;

    const int64_t thisEnd = byteOffset + countBytes;
    const int64_t otherEnd = o.byteOffset + o.countBytes;
    if(thisEnd < otherEnd)
      return false;

    return true;
  }

  const DXIL::DILocalVariable *variable;
  rdcstr sourceVarName;
  rdcstr debugVarSSAName;
  Id debugVarSSAId;
  int32_t byteOffset;
  uint32_t countBytes;
  uint32_t instIndex;
  bool isDeclare;
};

struct ScopedDebugData
{
  rdcarray<LocalMapping> localMappings;
  const DXIL::Metadata *md;
  ScopedDebugData *parent;
  rdcstr functionName;
  rdcstr fileName;
  uint32_t line;
  uint32_t maxInstruction;

  bool operator<(const ScopedDebugData &o) const { return line < o.line; }
};

struct TypeData
{
  const DXIL::Metadata *baseType = NULL;

  rdcarray<uint32_t> arrayDimensions;
  rdcarray<rdcpair<rdcstr, const DXIL::Metadata *>> structMembers;
  rdcarray<uint32_t> memberOffsets;

  rdcstr name;
  uint32_t sizeInBytes = 0;
  uint32_t alignInBytes = 0;

  VarType type = VarType::Unknown;
  uint32_t vecSize = 0;
  uint32_t matSize = 0;
  bool colMajorMat = false;
};

enum class StepThreadMode
{
  RUN_SINGLE_STEP,
  RUN_MULTIPLE_STEPS,
  QUEUE_SINGLE_STEP,
  QUEUE_MULTIPLE_STEPS
};

class Debugger : public DXBCContainerDebugger
{
public:
  Debugger();
  ~Debugger();

  ShaderDebugTrace *BeginDebug(DebugAPIWrapper *apiWrapper, uint32_t eventId,
                               const DXBC::DXBCContainer *dxbcContainer,
                               const ShaderReflection &reflection, uint32_t activeLaneIndex,
                               uint32_t threadsInWorkgroup);
  rdcarray<ShaderDebugState> ContinueDebug();

  const rdcarray<bool> &GetLiveGlobals() const { return m_LiveGlobals; }
  const DXIL::Program &GetProgram() const { return *m_Program; }
  const FunctionInfo *GetFunctionInfo(const DXIL::Function *function) const;

  DebugAPIWrapper *GetAPIWrapper() const { return m_ApiWrapper; }

  static rdcstr GetResourceBaseName(const DXIL::Program *program,
                                    const DXIL::ResourceReference *resRef);

  static rdcstr GetResourceReferenceName(const DXIL::Program *program, DXIL::ResourceClass resClass,
                                         const BindingSlot &slot);

  ShaderValue TypedResourceLoad(DXIL::ResourceClass resClass, const BindingSlot &slot,
                                const DXILDebug::ViewFmt &fmt, uint64_t dataOffset);
  bool TypedResourceStore(DXIL::ResourceClass resClass, const BindingSlot &slot,
                          const DXILDebug::ViewFmt &fmt, uint64_t dataOffset, ShaderValue &value);

  DeviceOpResult GetUAV(const BindingSlot &slot, UAVInfo &uavInfo) const;
  DeviceOpResult GetSRV(const BindingSlot &slot, SRVInfo &srvInfo) const;

  DeviceOpResult GetResourceInfo(DXIL::ResourceClass resClass, const DXDebug::BindingSlot &slot,
                                 uint32_t mipLevel, ShaderVariable &result) const;
  DeviceOpResult GetSampleInfo(DXIL::ResourceClass resClass, const DXDebug::BindingSlot &slot,
                               const char *opString, ShaderVariable &result) const;
  DeviceOpResult GetRenderTargetSampleInfo(const char *opString, ShaderVariable &result) const;
  DeviceOpResult GetResourceReferenceInfo(const DXDebug::BindingSlot &slot,
                                          ResourceReferenceInfo &result) const;
  DeviceOpResult GetShaderDirectAccess(DescriptorType type, const DXDebug::BindingSlot &slot,
                                       ShaderDirectAccess &result) const;

  bool IsDeviceThread() const { return Threading::GetCurrentID() == m_DeviceThreadID; }
  Threading::CriticalSection &GetAtomicMemoryLock() const { return m_AtomicMemoryLock; }
private:
  void InitialiseWorkgroup();
  ThreadState &GetActiveLane() { return m_Workgroup[m_ActiveLaneIndex]; }
  void ParseDbgOpDeclare(const DXIL::Instruction &inst, uint32_t instructionIndex);
  void ParseDbgOpValue(const DXIL::Instruction &inst, uint32_t instructionIndex);
  const DXIL::Metadata *GetMDScope(const DXIL::Metadata *scopeMD) const;
  ScopedDebugData *AddScopedDebugData(const DXIL::Metadata *scopeMD);
  ScopedDebugData *FindScopedDebugData(const DXIL::Metadata *md) const;
  const TypeData &AddDebugType(const DXIL::Metadata *typeMD);
  void AddStructMembers(const DXIL::DICompositeType *structTypeData, TypeData &structType);
  void AddLocalVariable(const DXIL::SourceMappingInfo &srcMapping, uint32_t instructionIndex);
  void ParseDebugData();

  void QueueJob(uint32_t lane);
  void StepThread(uint32_t lane, StepThreadMode stepMode);
  void InternalStepThread(uint32_t lane);
  void SimulationJobHelper();
  void QueueDeviceThreadStep(uint32_t lane);

  void ProcessQueuedDeviceThreadSteps();
  void ProcessQueuedOps();
  void ProcessQueuedGpuMathOps();
  void ProcessQueuedGpuSampleGatherOps();
  void SyncPendingGpuOps();
  void SyncPendingLanes();

  void QueueGpuMathOp(uint32_t lane);
  void QueueGpuSampleGatherOp(uint32_t lane);

  DebugAPIWrapper *m_ApiWrapper = NULL;

  rdcarray<ThreadState> m_Workgroup;
  std::map<const DXIL::Function *, FunctionInfo> m_FunctionInfos;
  rdcshaders::ControlFlow m_ControlFlow;

  rdcarray<ShaderDebugState> *m_ShaderChangesReturn = NULL;
  ShaderDebugState m_ActiveDebugState;

  mutable Threading::CriticalSection m_AtomicMemoryLock;
  rdcarray<int32_t> m_QueuedJobs;
  rdcarray<bool> m_QueuedDeviceThreadSteps;
  rdcarray<bool> m_QueuedGpuMathOps;
  rdcarray<bool> m_QueuedGpuSampleGatherOps;
  rdcarray<bool> m_PendingLanes;
  rdcarray<ShaderVariable *> m_PendingGpuMathsOpsResults;
  rdcarray<ShaderVariable *> m_PendingGpuSampleGatherOpsResults;
  rdcarray<int> m_PendingGpuSampleGatherOpsSampleRetTypes;

  // the live mutable global variables, to initialise a stack frame's live list
  rdcarray<bool> m_LiveGlobals;

  GlobalState m_GlobalState;

  struct DebugInfo
  {
    ~DebugInfo();
    rdcarray<ScopedDebugData *> scopedDebugDatas;
    std::map<const DXIL::DILocalVariable *, LocalMapping> locals;
    std::map<const DXIL::Metadata *, TypeData> types;
  } m_DebugInfo;

  const DXIL::Program *m_Program = NULL;
  const DXIL::Function *m_EntryPointFunction = NULL;
  const DXIL::EntryPointInterface *m_EntryPointInterface = NULL;
  ShaderStage m_Stage;

  const uint64_t m_DeviceThreadID;
  uint32_t m_ActiveLaneIndex = 0;
  int m_Steps = 0;
  bool m_RetireIDs = true;
  bool m_MTSimulation;

  // These need to be accessed using atomics
  int32_t atomic_simulationFinished;
};

};    // namespace DXILDebug

DECLARE_REFLECTION_ENUM(DXILDebug::StepThreadMode);
DECLARE_REFLECTION_ENUM(DXILDebug::DeviceOpResult);
DECLARE_REFLECTION_ENUM(DXILDebug::ThreadState::PendingResultStatus);
