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
#include "dxbc_disassemble.h"

namespace DXBC
{
class DXBCFile;
struct CBufferVariable;
}

class WrappedID3D11Device;

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

  struct ViewFmt
  {
    int byteWidth = 0;
    int numComps = 0;
    bool reversed = false;
    CompType fmt = CompType::Typeless;
    int stride = 0;

    int Stride()
    {
      if(stride != 0)
        return stride;

      if(byteWidth == 10 || byteWidth == 11)
        return 32;    // 10 10 10 2 or 11 11 10

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
    dxbc = NULL;
    device = NULL;
    RDCEraseEl(semantics);
  }
  State(int quadIdx, const ShaderDebugTrace *t, DXBC::DXBCFile *f, WrappedID3D11Device *d)
  {
    quadIndex = quadIdx;
    nextInstruction = 0;
    flags = ShaderEvents::NoEvent;
    done = false;
    trace = t;
    dxbc = f;
    device = d;
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

  State GetNext(GlobalState &global, State quad[4]) const;

private:
  // index in the pixel quad
  int quadIndex;

  bool done;

  // validates assignment for generation of non-normal values
  bool AssignValue(ShaderVariable &dst, uint32_t dstIndex, const ShaderVariable &src,
                   uint32_t srcIndex);
  // sets the destination operand by looking up in the register
  // file and applying any masking or swizzling
  void SetDst(const DXBC::ASMOperand &dstoper, const DXBC::ASMOperation &op,
              const ShaderVariable &val);

  // retrieves the value of the operand, by looking up
  // in the register file and performing any swizzling and
  // negation/abs functions
  ShaderVariable GetSrc(const DXBC::ASMOperand &oper, const DXBC::ASMOperation &op) const;

  ShaderVariable DDX(bool fine, State quad[4], const DXBC::ASMOperand &oper,
                     const DXBC::ASMOperation &op) const;
  ShaderVariable DDY(bool fine, State quad[4], const DXBC::ASMOperand &oper,
                     const DXBC::ASMOperation &op) const;

  VarType OperationType(const DXBC::OpcodeType &op) const;

  DXBC::DXBCFile *dxbc;
  const ShaderDebugTrace *trace;
  WrappedID3D11Device *device;
};

};    // namespace ShaderDebug
