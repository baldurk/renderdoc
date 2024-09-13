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

#include "common/common.h"
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

typedef std::map<ShaderBuiltin, ShaderVariable> BuiltinInputs;
class DebugAPIWrapper
{
};

struct ThreadState
{
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
  static rdcstr GetResourceReferenceName(const DXIL::Program *program, DXIL::ResourceClass resClass,
                                         const BindingSlot &slot);
};

};    // namespace DXILDebug
