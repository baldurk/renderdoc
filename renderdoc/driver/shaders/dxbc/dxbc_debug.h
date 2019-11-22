/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

#include "api/replay/renderdoc_replay.h"
#include "common/common.h"
#include "dxbc_bytecode.h"

namespace DXBC
{
struct Reflection;
class DXBCContainer;
struct CBufferVariable;
}

class WrappedID3D11Device;
enum DXGI_FORMAT;

namespace ShaderDebug
{
struct GlobalState
{
public:
  GlobalState()
  {
    for(int i = 0; i < 8; i++)
    {
      uavs[i].firstElement = uavs[i].numElements = uavs[i].hiddenCounter = 0;
      uavs[i].rowPitch = uavs[i].depthPitch = 0;
      uavs[i].tex = false;
    }

    for(int i = 0; i < 128; i++)
      srvs[i].firstElement = srvs[i].numElements = 0;
  }

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

  struct
  {
    bytebuf data;
    uint32_t firstElement;
    uint32_t numElements;

    bool tex;
    uint32_t rowPitch, depthPitch;

    ViewFmt format;

    uint32_t hiddenCounter;
  } uavs[64];

  struct
  {
    bytebuf data;
    uint32_t firstElement;
    uint32_t numElements;

    ViewFmt format;
  } srvs[128];

  struct groupsharedMem
  {
    bool structured;
    uint32_t bytestride;
    uint32_t count;    // of structures (above stride), or uint32s (raw)

    bytebuf data;
  };

  std::vector<groupsharedMem> groupshared;

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
};

#define SHADER_DEBUG_WARN_THRESHOLD 100000
bool PromptDebugTimeout(uint32_t cycleCounter);

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

void ApplyDerivatives(ShaderDebug::GlobalState &global, ShaderDebugTrace traces[4], int reg,
                      int element, int numWords, float *data, float signmul, int32_t quadIdxA,
                      int32_t quadIdxB);

void ApplyAllDerivatives(ShaderDebug::GlobalState &global, ShaderDebugTrace traces[4], int destIdx,
                         const std::vector<PSInputElement> &initialValues, float *data);

void FlattenSingleVariable(uint32_t byteOffset, const std::string &basename,
                           const ShaderVariable &v, rdcarray<ShaderVariable> &outvars);
void FlattenVariables(const rdcarray<ShaderConstant> &constants,
                      const rdcarray<ShaderVariable> &invars, rdcarray<ShaderVariable> &outvars,
                      const std::string &prefix, uint32_t baseOffset);
void FlattenVariables(const rdcarray<ShaderConstant> &constants,
                      const rdcarray<ShaderVariable> &invars, rdcarray<ShaderVariable> &outvars);

void FillViewFmt(DXGI_FORMAT format, GlobalState::ViewFmt &viewFmt);

void LookupSRVFormatFromShaderReflection(const DXBC::Reflection &reflection,
                                         uint32_t shaderRegister, GlobalState::ViewFmt &viewFmt);

void GatherPSInputDataForInitialValues(const DXBC::Reflection &psDxbc,
                                       const DXBC::Reflection &prevStageDxbc,
                                       std::vector<PSInputElement> &initialValues,
                                       std::vector<std::string> &floatInputs,
                                       std::vector<std::string> &inputVarNames,
                                       std::string &psInputDefinition, int &structureStride);

struct SampleGatherResourceData
{
  DXBCBytecode::ResourceDimension dim;
  DXBC::ResourceRetType retType;
  int sampleCount;
  UINT slot;
};

struct SampleGatherSamplerData
{
  DXBCBytecode::SamplerMode mode;
  UINT slot;
  float bias;
};

enum class GatherChannel : uint8_t
{
  Red = 0,
  Green = 1,
  Blue = 2,
  Alpha = 3,
};

class DebugAPIWrapper
{
public:
  virtual void SetCurrentInstruction(uint32_t instruction) = 0;
  virtual void AddDebugMessage(MessageCategory c, MessageSeverity sv, MessageSource src,
                               std::string d) = 0;

  virtual bool CalculateMathIntrinsic(DXBCBytecode::OpcodeType opcode, const ShaderVariable &input,
                                      ShaderVariable &output1, ShaderVariable &output2) = 0;

  virtual ShaderVariable GetSampleInfo(DXBCBytecode::OperandType type, bool isAbsoluteResource,
                                       UINT slot, const char *opString) = 0;

  virtual ShaderVariable GetBufferInfo(DXBCBytecode::OperandType type, UINT slot,
                                       const char *opString) = 0;
  virtual ShaderVariable GetResourceInfo(DXBCBytecode::OperandType type, UINT slot,
                                         uint32_t mipLevel, int &dim) = 0;

  virtual bool CalculateSampleGather(DXBCBytecode::OpcodeType opcode,
                                     SampleGatherResourceData resourceData,
                                     SampleGatherSamplerData samplerData, ShaderVariable uv,
                                     ShaderVariable ddxCalc, ShaderVariable ddyCalc,
                                     const int texelOffsets[3], int multisampleIndex,
                                     float lodOrCompareValue, const uint8_t swizzle[4],
                                     GatherChannel gatherChannel, const char *opString,
                                     ShaderVariable &output) = 0;
};

class State : public ShaderDebugState
{
public:
  State()
  {
    quadIndex = 0;
    nextInstruction = 0;
    flags = ShaderEvents::NoEvent;
    done = false;
    trace = NULL;
    program = NULL;
    program = NULL;
    RDCEraseEl(semantics);
  }
  State(int quadIdx, const ShaderDebugTrace *t, const DXBC::Reflection *r,
        const DXBCBytecode::Program *p)
  {
    quadIndex = quadIdx;
    nextInstruction = 0;
    flags = ShaderEvents::NoEvent;
    done = false;
    trace = t;
    reflection = r;
    program = p;
    RDCEraseEl(semantics);
  }

  void SetTrace(int quadIdx, const ShaderDebugTrace *t)
  {
    quadIndex = quadIdx;
    trace = t;
  }

  void SetHelper() { done = true; }
  struct
  {
    uint32_t GroupID[3];
    uint32_t ThreadID[3];
    uint32_t coverage;
    uint32_t primID;
    uint32_t isFrontFace;
  } semantics;

  void Init();
  bool Finished() const;

  State GetNext(GlobalState &global, DebugAPIWrapper *apiWrapper, State quad[4]) const;

private:
  // index in the pixel quad
  int quadIndex;

  bool done;

  // validates assignment for generation of non-normal values
  bool AssignValue(ShaderVariable &dst, uint32_t dstIndex, const ShaderVariable &src,
                   uint32_t srcIndex, bool flushDenorm);
  // sets the destination operand by looking up in the register
  // file and applying any masking or swizzling
  void SetDst(const DXBCBytecode::Operand &dstoper, const DXBCBytecode::Operation &op,
              const ShaderVariable &val);

  // retrieves the value of the operand, by looking up
  // in the register file and performing any swizzling and
  // negation/abs functions
  ShaderVariable GetSrc(const DXBCBytecode::Operand &oper, const DXBCBytecode::Operation &op,
                        bool allowFlushing = true) const;

  ShaderVariable DDX(bool fine, State quad[4], const DXBCBytecode::Operand &oper,
                     const DXBCBytecode::Operation &op) const;
  ShaderVariable DDY(bool fine, State quad[4], const DXBCBytecode::Operand &oper,
                     const DXBCBytecode::Operation &op) const;

  VarType OperationType(const DXBCBytecode::OpcodeType &op) const;
  bool OperationFlushing(const DXBCBytecode::OpcodeType &op) const;

  const DXBC::Reflection *reflection;
  const DXBCBytecode::Program *program;
  const ShaderDebugTrace *trace;
};

void CreateShaderDebugStateAndTrace(ShaderDebug::State &initialState, ShaderDebugTrace &trace,
                                    int quadIdx, DXBC::DXBCContainer *dxbc,
                                    const ShaderReflection &refl, bytebuf *cbufData);

};    // namespace ShaderDebug
