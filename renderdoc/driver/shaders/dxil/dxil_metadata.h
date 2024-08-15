/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

#include "dxil_common.h"

namespace DXBC
{
enum class ShaderType : uint32_t;
enum class GlobalShaderFlags : int64_t;
};

namespace DXIL
{

struct PSVData
{
};

struct RDATData
{
  enum
  {
    Version1_0 = 0x10
  };

  enum class Part
  {
    Invalid = 0,
    StringBuffer = 1,
    IndexArrays = 2,
    ResourceTable = 3,
    FunctionTable = 4,
    RawBytes = 5,
    SubobjectTable = 6,
  };

  enum class ResourceFlags
  {
    None = 0x0,
    GloballyCoherent = 0x1,
    HasCounter = 0x2,
    ROV = 0x4,
    // unused dynamic indexing flag? 0x8
    Atomic64 = 0x10,
  };

  // name arbitrarily chosen to avoid the extremely generic "shader flags" naming
  enum class ShaderBehaviourFlags : uint16_t
  {
    None = 0x0,
    NodeProgramEntry = 0x1,
    SVPositionOutput = 0x2,
    SVDepthOutput = 0x4,
    SampleRate = 0x8,
    ViewID = 0x10,
  };

  struct ResourceInfo
  {
    ResourceClass nspace;    // SRV, UAV, Sampler, CB
    ResourceKind kind;       // texture type (2D, 3D, etc) or other resource binding type
    uint32_t resourceIndex;    // the 0-based ID of this resource within the class namespace (SRV, UAV, etc).
    uint32_t space;       // register space
    uint32_t regStart;    // start register (inclusive - for single register bind it's == reg)
    uint32_t regEnd;      // end register (inclusive - for single register bind it's == reg)
    rdcstr name;
    ResourceFlags flags;

    bool operator==(const ResourceInfo &o)
    {
      // use namespace and linear ID to look up resources
      return nspace == o.nspace && resourceIndex == o.resourceIndex;
    }

    bool operator==(const rdcpair<DXIL::ResourceClass, uint32_t> &o)
    {
      return nspace == o.first && resourceIndex == o.second;
    }
  };

  struct FunctionInfo
  {
    rdcstr name;
    rdcstr unmangledName;
    rdcarray<rdcpair<ResourceClass, uint32_t>> globalResources;
    rdcarray<rdcstr> functionDependencies;
    DXBC::ShaderType type;
    uint32_t payloadBytes;
    uint32_t attribBytes;
    DXBC::GlobalShaderFlags featureFlags;
    uint32_t shaderCompatMask;    // bitmask based on DXBC::ShaderType enum of stages this function could be used with.
    uint16_t minShaderModel;
    uint16_t minType;    // looks to always be equal to type above
  };

  struct FunctionInfo2 : FunctionInfo
  {
    FunctionInfo2(FunctionInfo &info) : FunctionInfo(info)
    {
      minWaveCount = maxWaveCount = 0;
      shaderBehaviourFlags = ShaderBehaviourFlags::None;
      extraInfoRef = ~0U;
    }

    uint8_t minWaveCount;
    uint8_t maxWaveCount;
    ShaderBehaviourFlags shaderBehaviourFlags;

    // below here is a stage-specific set of data containing e.g. signature elements. Currently
    // DXC does not emit RDAT except for in library targets, so this will be unused. It would be an
    // index into a table elsewhere of VSInfo, PSInfo, etc.
    uint32_t extraInfoRef;
  };

  enum class StateObjectFlags : uint32_t
  {
    None = 0x0,
    LocalDepsOnExternals = 0x1,
    ExternalDepsOnLocals = 0x2,
    AllowAdditions = 0x4,
  };

  enum class HitGroupType : uint32_t
  {
    Triangle = 0,
    ProceduralPrimitive = 1,
  };

  enum class RTPipeFlags : uint32_t
  {
    None = 0x0,
    SkipTriangles = 0x100,
    SkipProcedural = 0x200,
  };

  struct SubobjectInfo
  {
    // values match D3D12_STATE_SUBOBJECT_TYPE
    enum class SubobjectType : uint32_t
    {
      StateConfig = 0,
      GlobalRS = 1,
      LocalRS = 2,
      // missing enum values
      SubobjectToExportsAssoc = 8,
      RTShaderConfig = 9,
      RTPipeConfig = 10,
      Hitgroup = 11,
      RTPipeConfig1 = 12,
    } type;
    rdcstr name;

    // we union members where possible but several contain arrays/strings which can't be unioned.

    struct StateConfig
    {
      StateObjectFlags flags;
    };

    struct RTShaderConfig
    {
      uint32_t maxPayloadBytes;
      uint32_t maxAttribBytes;
    };

    struct RTPipeConfig1
    {
      uint32_t maxRecursion;
      RTPipeFlags flags;
    };

    union
    {
      StateConfig config;
      RTShaderConfig rtshaderconfig;
      RTPipeConfig1 rtpipeconfig;
    };

    struct RootSig
    {
      bytebuf data;
    } rs;

    struct Assoc
    {
      rdcstr subobject;
      rdcarray<rdcstr> exports;
    } assoc;

    struct Hitgroup
    {
      HitGroupType type;
      rdcstr anyHit;
      rdcstr closestHit;
      rdcstr intersection;
    } hitgroup;
  };

  enum class FunctionInfoVersion
  {
    Version1 = 1,
    Version2,
    VersionLatest = Version2,
  } functionVersion = FunctionInfoVersion::VersionLatest;

  rdcarray<ResourceInfo> resourceInfo;
  rdcarray<FunctionInfo2> functionInfo;
  rdcarray<SubobjectInfo> subobjectsInfo;
};

};

BITMASK_OPERATORS(DXIL::RDATData::ResourceFlags);
BITMASK_OPERATORS(DXIL::RDATData::ShaderBehaviourFlags);
