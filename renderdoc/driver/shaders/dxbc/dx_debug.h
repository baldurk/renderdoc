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

#include <map>
#include "maths/vec.h"

namespace DXBC
{
enum ResourceRetType;
enum class InterpolationMode : uint8_t;
class DXBCContainer;
};

namespace DXBCBytecode
{
enum ResourceDimension;
enum SamplerMode;
};

namespace DXDebug
{
typedef DXBC::ResourceRetType ResourceRetType;
typedef DXBCBytecode::ResourceDimension ResourceDimension;
typedef DXBCBytecode::SamplerMode SamplerMode;

struct PSLaneData
{
  uint32_t laneIndex;
  uint32_t active;
  uint32_t pad[2];

  Vec4f pixelPos;

  uint32_t isHelper;
  uint32_t quadId;
  uint32_t quadLane;
  uint32_t coverage;

  uint32_t sample;
  uint32_t primitive;
  uint32_t isFrontFace;
  uint32_t pad2;

  // user data PSInput below here
};

struct VSLaneData
{
  uint32_t laneIndex;
  uint32_t active;
  uint32_t pad2[2];

  uint32_t inst;
  uint32_t vert;
  uint32_t pad[2];

  // user data VSInput below here
};

struct CSLaneData
{
  uint32_t laneIndex;
  uint32_t active;
  uint32_t pad2[2];

  uint32_t threadid[3];
  uint32_t activeSubgroup;
};

struct DebugHit
{
  // only used in the first instance
  uint32_t numHits;
  // below here are per-hit properties
  float posx;
  float posy;
  float depth;

  float derivValid;
  uint32_t quadLaneIndex;
  uint32_t laneIndex;
  uint32_t subgroupSize;

  uint32_t sample;
  uint32_t primitive;
  uint32_t pad[2];

  Vec4u globalBallot;
  Vec4u helperBallot;

  // LaneData quad[4] below here
};

// maximum number of overdraw levels before we start losing potential pixel hits
static const uint32_t maxPixelHits = 100;

struct InputElement
{
  InputElement(int regster, int element, int numWords, ShaderBuiltin attr, bool inc)
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
  bool operator==(const SampleEvalCacheKey &o) const { return !(*this < o) && !(o < *this); }
};

struct InputFetcherConfig
{
  uint32_t x = 0, y = 0;

  uint32_t vert = 0, inst = 0;

  rdcfixedarray<uint32_t, 3> threadid = {0, 0, 0};
  rdcfixedarray<uint32_t, 3> groupid = {0, 0, 0};

  uint32_t uavslot = 0;
  uint32_t uavspace = 0;
  uint32_t maxWaveSize = 64;
  uint32_t groupSize = 0;
  uint32_t fetchWorkgroup = 0;
  bool waveOps = false;
  uint32_t outputSampleCount = 1;
};

struct InputFetcher
{
  // stride of the hit buffer
  uint32_t hitBufferStride = 0;
  // stride of the lane data buffer - if 0 then no buffer is needed and lane data is inside hits
  uint32_t laneDataBufferStride = 0;
  // number of lanes each hit has allocated - usually equal to max wave size, or explicit wave size
  uint32_t numLanesPerHit = 0;
  // members of the Input struct
  rdcarray<InputElement> inputs;

  // per-sample evaluation cache (pixel shader only)
  rdcarray<SampleEvalCacheKey> evalSampleCacheData;
  uint64_t sampleEvalRegisterMask = 0;

  rdcstr hlsl;
};

void CreateInputFetcher(const DXBC::DXBCContainer *dxbc, const DXBC::DXBCContainer *prevdxbc,
                        const InputFetcherConfig &cfg, InputFetcher &fetcher);

enum class GatherChannel : uint8_t
{
  Red = 0,
  Green = 1,
  Blue = 2,
  Alpha = 3,
};

enum class HeapDescriptorType : uint8_t
{
  NoHeap = 0,
  CBV_SRV_UAV,
  Sampler,
};

struct BindingSlot
{
  BindingSlot()
      : shaderRegister(UINT32_MAX),
        registerSpace(UINT32_MAX),
        heapType(HeapDescriptorType::NoHeap),
        descriptorIndex(UINT32_MAX)
  {
  }
  BindingSlot(uint32_t shaderReg, uint32_t regSpace)
      : shaderRegister(shaderReg),
        registerSpace(regSpace),
        heapType(HeapDescriptorType::NoHeap),
        descriptorIndex(UINT32_MAX)
  {
  }
  BindingSlot(HeapDescriptorType type, uint32_t index)
      : shaderRegister(UINT32_MAX), registerSpace(UINT32_MAX), heapType(type), descriptorIndex(index)
  {
  }
  bool operator<(const BindingSlot &o) const
  {
    if(registerSpace != o.registerSpace)
      return registerSpace < o.registerSpace;
    if(shaderRegister != o.shaderRegister)
      return shaderRegister < o.shaderRegister;
    if(heapType != o.heapType)
      return heapType < o.heapType;
    return descriptorIndex < o.descriptorIndex;
  }
  bool operator==(const BindingSlot &o) const
  {
    return registerSpace == o.registerSpace && shaderRegister == o.shaderRegister &&
           heapType == o.heapType && descriptorIndex == o.descriptorIndex;
  }
  uint32_t shaderRegister;
  uint32_t registerSpace;
  HeapDescriptorType heapType;
  uint32_t descriptorIndex;
};

struct SampleGatherResourceData
{
  ResourceDimension dim;
  ResourceRetType retType;
  int sampleCount;
  BindingSlot binding;
};

struct SampleGatherSamplerData
{
  SamplerMode mode;
  float bias;
  BindingSlot binding;
};

float dxbc_min(float a, float b);
double dxbc_min(double a, double b);
float dxbc_max(float a, float b);
double dxbc_max(double a, double b);
float round_ne(float x);
double round_ne(double x);
float flush_denorm(const float f);

uint32_t BitwiseReverseLSB16(uint32_t x);
uint32_t PopCount(uint32_t x);

void get_sample_position(uint32_t sampleIndex, uint32_t sampleCount, float *position);
};
