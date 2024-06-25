/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
 * Copyright (c) 2014 Crytek
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

#include "common/common.h"
#include "dxbc_bytecode.h"
#include "dxbcdxil_debug.h"

namespace DXBC
{
struct Reflection;
class DXBCContainer;
struct CBufferVariable;
}

namespace DXBCBytecode
{
struct Declaration;
class Program;
enum OperandType : uint8_t;
}

class WrappedID3D11Device;
enum DXGI_FORMAT;

namespace DXBCDebug
{
using namespace DXBCDXILDebug;

typedef DXBCDXILDebug::SampleGatherResourceData SampleGatherResourceData;
typedef DXBCDXILDebug::SampleGatherSamplerData SampleGatherSamplerData;
typedef DXBCDXILDebug::BindingSlot BindingSlot;
typedef DXBCDXILDebug::GatherChannel GatherChannel;

BindingSlot GetBindingSlotForDeclaration(const DXBCBytecode::Program &program,
                                         const DXBCBytecode::Declaration &decl);
BindingSlot GetBindingSlotForIdentifier(const DXBCBytecode::Program &program,
                                        DXBCBytecode::OperandType declType, uint32_t identifier);

struct GlobalState
{
public:
  GlobalState() {}
  void PopulateGroupshared(const DXBCBytecode::Program *pBytecode);

  struct ViewFmt
  {
    int byteWidth = 0;
    int numComps = 0;
    CompType fmt = CompType::Typeless;
    int stride = 0;

    int Stride()
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
  typedef std::map<BindingSlot, UAVData>::iterator UAVIterator;

  struct SRVData
  {
    SRVData() : firstElement(0), numElements(0) {}
    bytebuf data;
    uint32_t firstElement;
    uint32_t numElements;

    ViewFmt format;
  };
  std::map<BindingSlot, SRVData> srvs;
  typedef std::map<BindingSlot, SRVData>::iterator SRVIterator;

  struct groupsharedMem
  {
    bool structured;
    uint32_t bytestride;
    uint32_t count;    // of structures (above stride), or uint32s (raw)

    bytebuf data;
  };

  rdcarray<groupsharedMem> groupshared;

  struct SampleEvalCacheKey
  {
    int32_t quadIndex = -1;              // index of this thread in the quad
    int32_t inputRegisterIndex = -1;     // index of the input register
    int32_t firstComponent = 0;          // the first component in the register
    int32_t numComponents = 0;           // how many components in the register
    int32_t sample = -1;                 // -1 for offset-from-centroid lookups
    int32_t offsetx = 0, offsety = 0;    // integer offset from centroid

    bool operator<(const SampleEvalCacheKey &o) const
    {
      if(quadIndex != o.quadIndex)
        return quadIndex < o.quadIndex;

      if(inputRegisterIndex != o.inputRegisterIndex)
        return inputRegisterIndex < o.inputRegisterIndex;

      if(firstComponent != o.firstComponent)
        return firstComponent < o.firstComponent;

      if(numComponents != o.numComponents)
        return numComponents < o.numComponents;

      if(sample != o.sample)
        return sample < o.sample;

      if(offsetx != o.offsetx)
        return offsetx < o.offsetx;

      return offsety < o.offsety;
    }
  };

  // a bitmask of which registers were fetched into the cache, for quick checking
  uint64_t sampleEvalRegisterMask = 0;
  std::map<SampleEvalCacheKey, ShaderVariable> sampleEvalCache;

  // copied from the parent trace
  rdcarray<ShaderVariable> constantBlocks;
};

struct PSInputElement
{
  PSInputElement(int regster, int element, int numWords, ShaderBuiltin attr, bool inc)
  {
    reg = regster;
    elem = element;
    numwords = numWords;
    sysattribute = attr;
    included = inc;
  }

  int reg;
  int elem;
  ShaderBuiltin sysattribute;

  int numwords;

  bool included;
};

void FlattenSingleVariable(uint32_t byteOffset, const rdcstr &basename, const ShaderVariable &v,
                           rdcarray<ShaderVariable> &outvars);

void FillViewFmt(DXGI_FORMAT format, GlobalState::ViewFmt &viewFmt);

void LookupSRVFormatFromShaderReflection(const DXBC::Reflection &reflection,
                                         const BindingSlot &slot, GlobalState::ViewFmt &viewFmt);

void GatherPSInputDataForInitialValues(const DXBC::DXBCContainer *dxbc,
                                       const DXBC::Reflection &prevStageDxbc,
                                       rdcarray<PSInputElement> &initialValues,
                                       rdcarray<rdcstr> &floatInputs, rdcarray<rdcstr> &inputVarNames,
                                       rdcstr &psInputDefinition, int &structureStride);

class DebugAPIWrapper
{
public:
  virtual void SetCurrentInstruction(uint32_t instruction) = 0;
  virtual void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src, rdcstr d) = 0;

  // During shader debugging, when a new resource is encountered, this will be called to fetch the
  // data on demand. Return true if the ShaderDebug::GlobalState data for the slot is populated,
  // return false if the resource cannot be found.
  virtual void FetchSRV(const BindingSlot &slot) = 0;
  virtual void FetchUAV(const BindingSlot &slot) = 0;

  virtual bool CalculateMathIntrinsic(DXBCBytecode::OpcodeType opcode, const ShaderVariable &input,
                                      ShaderVariable &output1, ShaderVariable &output2) = 0;

  virtual ShaderVariable GetSampleInfo(DXBCBytecode::OperandType type, bool isAbsoluteResource,
                                       const BindingSlot &slot, const char *opString) = 0;

  virtual ShaderVariable GetBufferInfo(DXBCBytecode::OperandType type, const BindingSlot &slot,
                                       const char *opString) = 0;
  virtual ShaderVariable GetResourceInfo(DXBCBytecode::OperandType type, const BindingSlot &slot,
                                         uint32_t mipLevel, int &dim) = 0;

  virtual bool CalculateSampleGather(DXBCBytecode::OpcodeType opcode,
                                     SampleGatherResourceData resourceData,
                                     SampleGatherSamplerData samplerData,
                                     const ShaderVariable &uvIn, const ShaderVariable &ddxCalcIn,
                                     const ShaderVariable &ddyCalcIn, const int8_t texelOffsets[3],
                                     int multisampleIndex, float lodOrCompareValue,
                                     const uint8_t swizzle[4], GatherChannel gatherChannel,
                                     const char *opString, ShaderVariable &output) = 0;
};

class ThreadState
{
public:
  ThreadState(int workgroupIdx, GlobalState &globalState, const DXBC::DXBCContainer *dxbc);

  void SetHelper() { done = true; }
  struct
  {
    uint32_t GroupID[3];
    uint32_t ThreadID[3];
    uint32_t coverage;
    uint32_t primID;
    uint32_t isFrontFace;
  } semantics;

  uint32_t nextInstruction;
  GlobalState &global;
  rdcarray<ShaderVariable> inputs;
  rdcarray<ShaderVariable> variables;

  bool Finished() const;

  void PrepareInitial(ShaderDebugState &initial);
  void StepNext(ShaderDebugState *state, DebugAPIWrapper *apiWrapper,
                const rdcarray<ThreadState> &prevWorkgroup);

private:
  // index in the pixel quad
  int workgroupIndex;
  bool done;

  // validates assignment for generation of non-normal values
  ShaderEvents AssignValue(ShaderVariable &dst, uint32_t dstIndex, const ShaderVariable &src,
                           uint32_t srcIndex, bool flushDenorm);
  // sets the destination operand by looking up in the register
  // file and applying any masking or swizzling
  void SetDst(ShaderDebugState *state, const DXBCBytecode::Operand &dstoper,
              const DXBCBytecode::Operation &op, const ShaderVariable &val);

  void MarkResourceAccess(ShaderDebugState *state, DXBCBytecode::OperandType type,
                          const BindingSlot &slot);

  // retrieves the value of the operand, by looking up
  // in the register file and performing any swizzling and
  // negation/abs functions
  ShaderVariable GetSrc(const DXBCBytecode::Operand &oper, const DXBCBytecode::Operation &op,
                        bool allowFlushing = true) const;

  ShaderVariable DDX(bool fine, const rdcarray<ThreadState> &quad,
                     const DXBCBytecode::Operand &oper, const DXBCBytecode::Operation &op) const;
  ShaderVariable DDY(bool fine, const rdcarray<ThreadState> &quad,
                     const DXBCBytecode::Operand &oper, const DXBCBytecode::Operation &op) const;

  const DXBC::Reflection *reflection;
  const DXBCBytecode::Program *program;
  const DXBC::IDebugInfo *debug;

  rdcarray<ShaderBindIndex> m_accessedSRVs;
  rdcarray<ShaderBindIndex> m_accessedUAVs;
};

struct InterpretDebugger : public DXBCContainerDebugger
{
  InterpretDebugger() : DXBCContainerDebugger(false){};
  ShaderDebugTrace *BeginDebug(const DXBC::DXBCContainer *dxbcContainer,
                               const ShaderReflection &refl, int activeIndex);

  GlobalState global;
  uint32_t eventId;

  rdcarray<ThreadState> workgroup;

  // convenience for access to active lane
  ThreadState &activeLane() { return workgroup[activeLaneIndex]; }
  int activeLaneIndex = 0;

  int steps = 0;

  const DXBC::DXBCContainer *dxbc;

  void CalcActiveMask(rdcarray<bool> &activeMask);
  rdcarray<ShaderDebugState> ContinueDebug(DebugAPIWrapper *apiWrapper);
};

uint32_t GetLogicalIdentifierForBindingSlot(const DXBCBytecode::Program &program,
                                            DXBCBytecode::OperandType declType,
                                            const DXBCDebug::BindingSlot &slot);

void ApplyAllDerivatives(GlobalState &global, rdcarray<ThreadState> &quad, int destIdx,
                         const rdcarray<PSInputElement> &initialValues, float *data);

void AddCBufferToGlobalState(const DXBCBytecode::Program &program, GlobalState &global,
                             rdcarray<SourceVariableMapping> &sourceVars,
                             const ShaderReflection &refl, const BindingSlot &slot,
                             bytebuf &cbufData);

};    // namespace ShaderDebug
