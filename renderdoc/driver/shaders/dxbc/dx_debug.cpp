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

#include "dx_debug.h"
#include "common/formatting.h"
#include "driver/shaders/dxil/dxil_debug.h"
#include "dxbc_bytecode.h"
#include "dxbc_common.h"
#include "dxbc_container.h"
#include "dxbc_debug.h"

namespace DXDebug
{
void GatherInputDataForInitialValues(const DXBC::DXBCContainer *dxbc, InputFetcher &fetcher,
                                     const DXBC::DXBCContainer *prevdxbc,
                                     rdcarray<rdcstr> &floatInputs, rdcarray<rdcstr> &nonfloatInputs,
                                     rdcarray<rdcstr> &inputVarNames)
{
  rdcarray<DXBC::InterpolationMode> interpModes;

  const rdcarray<SigParameter> &stageInputSig = dxbc->GetReflection()->InputSig;
  rdcarray<SigParameter> emptySig;
  const rdcarray<SigParameter> &prevStageOutputSig =
      prevdxbc ? prevdxbc->GetReflection()->OutputSig : emptySig;

  if(dxbc->GetDXBCByteCode())
    DXBCDebug::GetInterpolationModeForInputParams(stageInputSig, dxbc->GetDXBCByteCode(),
                                                  interpModes);
  else
    DXILDebug::GetInterpolationModeForInputParams(stageInputSig, dxbc->GetDXILByteCode(),
                                                  interpModes);

  // When debugging a pixel shader, we need to get the initial values of each pixel shader
  // input for the pixel that we are debugging, from whichever the previous shader stage was
  // configured in the pipeline. This function returns the input element definitions, other
  // associated data, the HLSL definition to use when gathering pixel shader initial values,
  // and the laneDataBufferStride of that HLSL structure.

  // This function does not provide any HLSL definitions for additional metadata that may be
  // needed for gathering initial values, such as primitive ID, and also does not provide the
  // shader function body.

  fetcher.inputs.clear();
  floatInputs.clear();
  nonfloatInputs.clear();
  inputVarNames.clear();
  fetcher.hlsl += "struct Inputs\n{\n";
  rdcstr defines, copyFunc;
  fetcher.laneDataBufferStride = 0;

  copyFunc =
      "void CopyInputs(out Inputs OUT, in Inputs IN) {\n"
      "  OUT = (Inputs)0;\n";

  if(stageInputSig.empty() && dxbc->m_Type == DXBC::ShaderType::Pixel)
  {
    fetcher.hlsl += "float4 input_dummy : SV_Position;\n";
    fetcher.hlsl += "#define POSITION_VAR input_dummy\n";

    fetcher.inputs.push_back(InputElement(-1, 0, 4, ShaderBuiltin::Undefined, true));

    fetcher.laneDataBufferStride += 4;
  }

  // name, pair<start semantic index, end semantic index>
  rdcarray<rdcpair<rdcstr, rdcpair<uint32_t, uint32_t>>> arrays;

  uint32_t nextreg = 0;

  size_t numInputs = stageInputSig.size();
  inputVarNames.resize(numInputs);

  for(size_t i = 0; i < numInputs; i++)
  {
    const SigParameter &sig = stageInputSig[i];

    fetcher.hlsl += "  ";

    bool included = true;

    // handled specially to account for SV_ ordering
    if(sig.systemValue == ShaderBuiltin::MSAACoverage ||
       sig.systemValue == ShaderBuiltin::IsFrontFace ||
       sig.systemValue == ShaderBuiltin::MSAASampleIndex)
    {
      fetcher.hlsl += "//";
      included = false;
    }

    // it seems sometimes primitive ID can be included within inputs and isn't subject to the SV_
    // ordering restrictions - possibly to allow for geometry shaders to output the primitive ID as
    // an interpolant. Only comment it out if it's the last input.
    if(i + 1 == numInputs && sig.systemValue == ShaderBuiltin::PrimitiveIndex)
    {
      fetcher.hlsl += "//";
      included = false;
    }

    int arrayIndex = -1;

    for(size_t a = 0; a < arrays.size(); a++)
    {
      if(sig.semanticName == arrays[a].first && arrays[a].second.first <= sig.semanticIndex &&
         arrays[a].second.second >= sig.semanticIndex)
      {
        fetcher.hlsl += "//";
        included = false;
        arrayIndex = sig.semanticIndex - arrays[a].second.first;
      }
    }

    int missingreg = int(sig.regIndex) - int(nextreg);

    // fill in holes from output sig of previous shader if possible, to try and
    // ensure the same register order
    for(int dummy = 0; dummy < missingreg; dummy++)
    {
      bool filled = false;

      size_t numPrevOutputs = prevStageOutputSig.size();
      for(size_t os = 0; os < numPrevOutputs; os++)
      {
        if(prevStageOutputSig[os].regIndex == nextreg + dummy)
        {
          filled = true;
          VarType varType = prevStageOutputSig[os].varType;
          uint32_t bytesPerColumn = (varType != VarType::Half) ? 4 : 2;

          if(varType == VarType::Float)
            fetcher.hlsl += "float";
          else if(varType == VarType::Half)
            fetcher.hlsl += "half";
          else if(varType == VarType::SInt)
            fetcher.hlsl += "int";
          else if(varType == VarType::UInt)
            fetcher.hlsl += "uint";
          else
            RDCERR("Unexpected input signature type: %s",
                   ToStr(prevStageOutputSig[os].varType).c_str());

          int numCols = (prevStageOutputSig[os].regChannelMask & 0x1 ? 1 : 0) +
                        (prevStageOutputSig[os].regChannelMask & 0x2 ? 1 : 0) +
                        (prevStageOutputSig[os].regChannelMask & 0x4 ? 1 : 0) +
                        (prevStageOutputSig[os].regChannelMask & 0x8 ? 1 : 0);

          rdcstr name = prevStageOutputSig[os].semanticIdxName;
          fetcher.hlsl += ToStr((uint32_t)numCols) + " input_" + name + " : " + name + ";\n";

          uint32_t byteSize = AlignUp4(numCols * bytesPerColumn);
          fetcher.laneDataBufferStride += byteSize;

          fetcher.inputs.push_back(InputElement(-1, 0, byteSize / 4, ShaderBuiltin::Undefined, true));
        }
      }

      if(!filled)
      {
        rdcstr dummy_reg = "dummy_register";
        dummy_reg += ToStr((uint32_t)nextreg + dummy);
        fetcher.hlsl += "float4 var_" + dummy_reg + " : semantic_" + dummy_reg + ";\n";

        fetcher.inputs.push_back(InputElement(-1, 0, 4, ShaderBuiltin::Undefined, true));

        fetcher.laneDataBufferStride += 4 * sizeof(float);
      }
    }

    nextreg = sig.regIndex + 1;

    DXBC::InterpolationMode interpolation = interpModes[i];
    if(interpolation != DXBC::InterpolationMode::INTERPOLATION_UNDEFINED)
      fetcher.hlsl += ToStr(interpolation) + " ";
    fetcher.hlsl += ToStr(sig.varType);

    int numCols = (sig.regChannelMask & 0x1 ? 1 : 0) + (sig.regChannelMask & 0x2 ? 1 : 0) +
                  (sig.regChannelMask & 0x4 ? 1 : 0) + (sig.regChannelMask & 0x8 ? 1 : 0);

    rdcstr name = sig.semanticIdxName;

    // arrays of interpolators are handled really weirdly. They use cbuffer
    // packing rules where each new value is in a new register (rather than
    // e.g. 2 x float2 in a single register), but that's pointless because
    // you can't dynamically index into input registers.
    // If we declare those elements as a non-array, the float2s or floats
    // will be packed into registers and won't match up to the previous
    // shader.
    // HOWEVER to add an extra bit of fun, fxc will happily pack other
    // parameters not in the array into spare parts of the registers.
    //
    // So I think the upshot is that we can detect arrays reliably by
    // whenever we encounter a float or float2 at the start of a register,
    // search forward to see if the next register has an element that is the
    // same semantic name and one higher semantic index. If so, there's an
    // array, so keep searching to enumerate its length.
    // I think this should be safe if the packing just happens to place those
    // registers together.

    int arrayLength = 0;

    if(included && numCols <= 3 && (sig.regChannelMask & 0x1))
    {
      uint32_t nextIdx = sig.semanticIndex + 1;

      for(size_t j = i + 1; j < numInputs; j++)
      {
        const SigParameter &jSig = stageInputSig[j];

        // if we've found the 'next' semantic
        if(sig.semanticName == jSig.semanticName && nextIdx == jSig.semanticIndex)
        {
          int jNumCols = (jSig.regChannelMask & 0x1 ? 1 : 0) + (jSig.regChannelMask & 0x2 ? 1 : 0) +
                         (jSig.regChannelMask & 0x4 ? 1 : 0) + (jSig.regChannelMask & 0x8 ? 1 : 0);

          DXBC::InterpolationMode jInterp = interpModes[j];

          // if it's the same size, type, and interpolation mode, then it could potentially be
          // packed into an array. Check if it's using the first channel component to tell whether
          // it's tightly packed with another semantic.
          if(jNumCols == numCols && interpolation == jInterp && sig.varType == jSig.varType &&
             jSig.regChannelMask & 0x1)
          {
            if(arrayLength == 0)
              arrayLength = 2;
            else
              arrayLength++;

            // continue searching now
            nextIdx++;
            j = i + 1;
            continue;
          }
        }
      }

      if(arrayLength > 0)
        arrays.push_back(
            make_rdcpair(sig.semanticName, make_rdcpair((uint32_t)sig.semanticIndex, nextIdx - 1)));
    }

    if(included)
    {
      // in UAV structs, arrays are packed tightly, so just multiply by arrayLength
      fetcher.laneDataBufferStride += 4 * numCols * RDCMAX(1, arrayLength);
    }

    // as another side effect of the above, an element declared as a 1-length array won't be
    // detected but it WILL be put in its own register (not packed together), so detect this
    // case too.
    // Note we have to search *backwards* because we need to know if this register should have
    // been packed into the previous register, but wasn't. float/float2/float3 can be packed after
    // an array just fine, so long as the sum of their components doesn't exceed a register width
    if(included && i > 0 && arrayLength == 0)
    {
      const SigParameter &prev = stageInputSig[i - 1];

      if(prev.regIndex != sig.regIndex && prev.compCount + sig.compCount <= 4)
        arrayLength = 1;
    }

    // The compiler is also really annoying and will go to great lengths to rearrange elements
    // and screw up our declaration, to pack things together. E.g.:
    // float2 a : TEXCOORD1;
    // float4 b : TEXCOORD2;
    // float4 c : TEXCOORD3;
    // float2 d : TEXCOORD4;
    // the compiler will move d up and pack it into the last two components of a.
    // To prevent this, we look forward and backward to check that we aren't expecting to pack
    // with anything, and if not then we just make it a 1-length array to ensure no packing.
    // Note the regChannelMask & 0x1 means it is using .x, so it's not the tail-end of a pack
    if(included && arrayLength == 0 && numCols <= 3 && (sig.regChannelMask & 0x1))
    {
      if(i == numInputs - 1)
      {
        // the last element is never packed
        arrayLength = 1;
      }
      else
      {
        // if the next reg is using .x, it wasn't packed with us
        if(stageInputSig[i + 1].regChannelMask & 0x1)
          arrayLength = 1;
      }
    }

    rdcstr inputName = "input_" + name;
    fetcher.hlsl += ToStr((uint32_t)numCols) + " " + inputName;
    if(arrayLength > 0)
      fetcher.hlsl += "[" + ToStr(arrayLength) + "]";
    fetcher.hlsl += " : " + name;
    // DXIL does not allow redeclaring SV_ variables, any that we might need which could already be
    // in Inputs must be obtained from there and not redeclared in our entry point
    if(included)
    {
      if(sig.systemValue == ShaderBuiltin::Position)
        defines += "#define POSITION_VAR " + inputName + rdcstr(arrayLength > 0 ? "[0]" : "") + "\n";
      else if(sig.systemValue == ShaderBuiltin::PrimitiveIndex)
        defines += "#define PRIM_VAR " + inputName + rdcstr(arrayLength > 0 ? "[0]" : "") + "\n";
      else if(sig.systemValue == ShaderBuiltin::VertexIndex)
        defines += "#define VERT_VAR " + inputName + rdcstr(arrayLength > 0 ? "[0]" : "") + "\n";
      else if(sig.systemValue == ShaderBuiltin::InstanceIndex)
        defines += "#define INST_VAR " + inputName + rdcstr(arrayLength > 0 ? "[0]" : "") + "\n";
    }

    inputVarNames[i] = inputName;
    if(arrayLength > 0)
      inputVarNames[i] += StringFormat::Fmt("[%d]", RDCMAX(0, arrayIndex));

    if(included && sig.channelUsedMask != 0)
    {
      rdcarray<rdcstr> &outArray = sig.varType == VarType::Float ? floatInputs : nonfloatInputs;
      if(arrayLength == 0)
      {
        outArray.push_back(inputName);
      }
      else
      {
        for(int a = 0; a < arrayLength; a++)
          outArray.push_back(inputName + "[" + ToStr(a) + "]");
      }

      copyFunc += StringFormat::Fmt("  OUT.%s = IN.%s;\n", inputName.c_str(), inputName.c_str());
    }

    fetcher.hlsl += ";\n";

    int firstElem = sig.regChannelMask & 0x1   ? 0
                    : sig.regChannelMask & 0x2 ? 1
                    : sig.regChannelMask & 0x4 ? 2
                    : sig.regChannelMask & 0x8 ? 3
                                               : -1;
    uint32_t bytesPerColumn = (sig.varType != VarType::Half) ? 4 : 2;
    uint32_t byteSize = AlignUp4(numCols * bytesPerColumn);

    // arrays get added all at once (because in the struct data, they are contiguous even if
    // in the input signature they're not).
    if(arrayIndex < 0)
    {
      if(arrayLength == 0)
      {
        fetcher.inputs.push_back(
            InputElement(sig.regIndex, firstElem, byteSize / 4, sig.systemValue, included));
      }
      else
      {
        for(int a = 0; a < arrayLength; a++)
        {
          fetcher.inputs.push_back(
              InputElement(sig.regIndex + a, firstElem, byteSize / 4, sig.systemValue, included));
        }
      }
    }
  }

  copyFunc += "\n};\n\n";

  fetcher.hlsl += "};\n\n" + defines + "\n\n" + copyFunc;
}

void CreateLegacyInputFetcher(const DXBC::DXBCContainer *dxbc, const InputFetcherConfig &cfg,
                              const rdcarray<rdcstr> &floatInputs,
                              const rdcarray<rdcstr> &inputVarNames, InputFetcher &fetcher)
{
  fetcher.hitBufferStride =
      sizeof(DXDebug::DebugHit) +
      (sizeof(DXDebug::PSLaneData) + fetcher.laneDataBufferStride) * cfg.maxWaveSize;
  fetcher.laneDataBufferStride = 0;

  bool dxil = dxbc->GetDXILByteCode() != NULL;

  // work around NV driver bug - it miscompiles the quad swizzle helper sometimes, so use the wave op instead
  if(!dxil || !cfg.waveOps)
    fetcher.hlsl += GetEmbeddedResource(quadswizzle_hlsl);
  else
    fetcher.hlsl +=
        "#define quadSwizzleHelper(value, quadLaneIndex, readIndex) "
        "QuadReadLaneAt(value, readIndex)\n";

  fetcher.hlsl += R"(
struct LaneData
{
  uint laneIndex;
  uint active;
  uint2 pad;

  float4 pixelPos;

  uint isHelper;
  uint quadId;
  uint quadLane;
  uint coverage;

  uint sample;
  uint primitive;
  uint isFrontFace;
  uint pad2;

  Inputs IN;
};

struct DebugHit
{
  // only used in the first instance
  uint numHits;
  float3 pos_depth; // xy position and depth

  float derivValid;
  uint quadLaneIndex;
  uint laneIndex;
  uint subgroupSize;

  uint sample;
  uint primitive;
  uint2 pad;

  uint4 globalBallot;
  uint4 helperBallot;

  LaneData lanes[4];
};

RWStructuredBuffer<DebugHit> HitBuffer : register(HITBUFFER);

// float4 is wasteful in some cases but it's easier than using ByteAddressBuffer and manual
// packing
RWBuffer<float4> EvalCacheBuffer : register(EVALCACHEBUFFER);

void ExtractInputs(Inputs IN
#ifndef POSITION_VAR
                     , float4 debug_pixelPos : SV_Position
#endif
#if USEPRIM && !defined(PRIM_VAR)
                     , uint primitive : SV_PrimitiveID
#endif
                     // sample, coverage and isFrontFace are deliberately omittted from the
                     // IN struct for SV_ ordering reasons
                     , uint sample : SV_SampleIndex
                     , uint coverage : SV_Coverage
                     , bool isFrontFace : SV_IsFrontFace)
{
#ifdef POSITION_VAR
  float4 debug_pixelPos = IN.POSITION_VAR;
#endif

#if USEPRIM && defined(PRIM_VAR)
  uint primitive = IN.PRIM_VAR;
#elif !USEPRIM
  uint primitive = 0;
#endif

  const uint quadLaneIndex = (2u * (uint(debug_pixelPos.y) & 1u)) + (uint(debug_pixelPos.x) & 1u);

  // grab our output slot
  uint idx = MAXHIT;
  if(abs(debug_pixelPos.x - DESTX) < 0.5f && abs(debug_pixelPos.y - DESTY) < 0.5f)
    InterlockedAdd(HitBuffer[0].numHits, 1, idx);
  idx = min(idx, MAXHIT);

  HitBuffer[idx].pos_depth = debug_pixelPos.xyz;

  HitBuffer[idx].derivValid = ddx(debug_pixelPos.x);

  HitBuffer[idx].primitive = primitive;

  HitBuffer[idx].sample = sample;

  HitBuffer[idx].quadLaneIndex = quadLaneIndex;
  HitBuffer[idx].laneIndex = quadLaneIndex;
  HitBuffer[idx].subgroupSize = 4;

  HitBuffer[idx].globalBallot = 0;
  HitBuffer[idx].helperBallot = 0;

  // replicate these across the quad, we assume they do not vary
  HitBuffer[idx].lanes[0].primitive = primitive;
  HitBuffer[idx].lanes[1].primitive = primitive;
  HitBuffer[idx].lanes[2].primitive = primitive;
  HitBuffer[idx].lanes[3].primitive = primitive;

  HitBuffer[idx].lanes[0].isFrontFace = isFrontFace;
  HitBuffer[idx].lanes[1].isFrontFace = isFrontFace;
  HitBuffer[idx].lanes[2].isFrontFace = isFrontFace;
  HitBuffer[idx].lanes[3].isFrontFace = isFrontFace;

  HitBuffer[idx].lanes[0].sample = sample;
  HitBuffer[idx].lanes[1].sample = sample;
  HitBuffer[idx].lanes[2].sample = sample;
  HitBuffer[idx].lanes[3].sample = sample;

  // quad pixelPos will be set with other derivatives for float inputs

  // for the simple quad case, only the desired thread is considered non-helper
  HitBuffer[idx].lanes[0].isHelper = 1u;
  HitBuffer[idx].lanes[1].isHelper = 1u;
  HitBuffer[idx].lanes[2].isHelper = 1u;
  HitBuffer[idx].lanes[3].isHelper = 1u;
  HitBuffer[idx].lanes[quadLaneIndex].isHelper = 0u;

  // and all threads are active
  HitBuffer[idx].lanes[0].active = 1u;
  HitBuffer[idx].lanes[1].active = 1u;
  HitBuffer[idx].lanes[2].active = 1u;
  HitBuffer[idx].lanes[3].active = 1u;

  // quadId is a single value that's unique for this quad and uniform across the quad. Degenerate
  // for the simple quad case
  uint quadId = 1000+quadSwizzleHelper(quadLaneIndex, quadLaneIndex, 0u);
  HitBuffer[idx].lanes[0].quadId = quadId;
  HitBuffer[idx].lanes[1].quadId = quadId;
  HitBuffer[idx].lanes[2].quadId = quadId;
  HitBuffer[idx].lanes[3].quadId = quadId;

  // per-quad lane identifier, degenerate for the simple quad case
  HitBuffer[idx].lanes[0].quadLane = 0;
  HitBuffer[idx].lanes[1].quadLane = 1;
  HitBuffer[idx].lanes[2].quadLane = 2;
  HitBuffer[idx].lanes[3].quadLane = 3;

  // coverage is handled with pixelPos as it can vary per-thread

  // start off with just copying all the inputs to all the quad. For float inputs or uints that may
  // vary across the quad we will quadSwizzle them
  CopyInputs(HitBuffer[idx].lanes[0].IN, IN);
  CopyInputs(HitBuffer[idx].lanes[1].IN, IN);
  CopyInputs(HitBuffer[idx].lanes[2].IN, IN);
  CopyInputs(HitBuffer[idx].lanes[3].IN, IN);
)";

  for(int q = 0; q < 4; q++)
  {
    fetcher.hlsl += StringFormat::Fmt(
        "  HitBuffer[idx].lanes[%i].pixelPos = "
        "quadSwizzleHelper(debug_pixelPos, quadLaneIndex, %i);\n",
        q, q);
    fetcher.hlsl += StringFormat::Fmt(
        "  HitBuffer[idx].lanes[%i].coverage = "
        "quadSwizzleHelper(coverage, quadLaneIndex, %i);\n",
        q, q);
  }

  for(size_t i = 0; i < floatInputs.size(); i++)
  {
    const rdcstr &name = floatInputs[i];
    for(int q = 0; q < 4; q++)
    {
      fetcher.hlsl += StringFormat::Fmt(
          "  HitBuffer[idx].lanes[%i].IN.%s = quadSwizzleHelper(IN.%s, quadLaneIndex, %i);\n", q,
          name.c_str(), name.c_str(), q);
    }
  }

  // if we're not rendering at MSAA, no need to fill the cache because evaluates will all return
  // the plain input anyway.
  if(cfg.outputSampleCount > 1)
  {
    if(dxbc->GetDXBCByteCode())
    {
      dxbc->GetDXBCByteCode()->CalculateEvalSampleCache(cfg, fetcher);
    }
    else
    {
      RDCWARN("TODO DXIL Pixel Shader Debugging support for MSAA Evaluate");
    }
  }

  if(!fetcher.evalSampleCacheData.empty())
  {
    fetcher.hlsl += StringFormat::Fmt("  uint stride = %zu;\n", fetcher.evalSampleCacheData.size());
    fetcher.hlsl += StringFormat::Fmt("  uint evalIdx = idx * stride * 4;\n");
    fetcher.hlsl += StringFormat::Fmt("  float4 evalCacheVal;\n");

    uint32_t evalIdx = 0;
    for(const SampleEvalCacheKey &key : fetcher.evalSampleCacheData)
    {
      uint32_t keyMask = 0;

      for(int32_t i = 0; i < key.numComponents; i++)
        keyMask |= (1 << (key.firstComponent + i));

      // find the name of the variable matching the operand, in the case of merged input variables.
      rdcstr name, swizzle = "xyzw";
      for(size_t i = 0; i < dxbc->GetReflection()->InputSig.size(); i++)
      {
        if(dxbc->GetReflection()->InputSig[i].regIndex == (uint32_t)key.inputRegisterIndex &&
           dxbc->GetReflection()->InputSig[i].systemValue == ShaderBuiltin::Undefined &&
           (dxbc->GetReflection()->InputSig[i].regChannelMask & keyMask) == keyMask)
        {
          name = inputVarNames[i];

          if(!name.empty())
            break;
        }
      }

      swizzle.resize(key.numComponents);

      if(name.empty())
      {
        RDCERR("Couldn't find matching input variable for v%d [%d:%d]", key.inputRegisterIndex,
               key.firstComponent, key.numComponents);
        fetcher.hlsl += StringFormat::Fmt("  EvalCacheBuffer[evalIdx+stride*0+%u] = 0;\n", evalIdx);
        fetcher.hlsl += StringFormat::Fmt("  EvalCacheBuffer[evalIdx+stride*1+%u] = 0;\n", evalIdx);
        fetcher.hlsl += StringFormat::Fmt("  EvalCacheBuffer[evalIdx+stride*2+%u] = 0;\n", evalIdx);
        fetcher.hlsl += StringFormat::Fmt("  EvalCacheBuffer[evalIdx+stride*3+%u] = 0;\n", evalIdx);
        evalIdx++;
        continue;
      }

      name = StringFormat::Fmt("IN.%s.%s", name.c_str(), swizzle.c_str());

      // we must write all components, so just swizzle the values - they'll be ignored later.
      rdcstr expandSwizzle = swizzle;
      while(expandSwizzle.size() < 4)
        expandSwizzle.push_back('x');

      if(key.sample >= 0)
      {
        fetcher.hlsl +=
            StringFormat::Fmt("  evalCacheVal = EvaluateAttributeAtSample(%s, %d).%s;\n",
                              name.c_str(), key.sample, expandSwizzle.c_str());
      }
      else
      {
        // we don't need to special-case EvaluateAttributeAtCentroid, since it's just a case with
        // 0,0
        fetcher.hlsl +=
            StringFormat::Fmt("  evalCacheVal = EvaluateAttributeSnapped(%s, int2(%d, %d)).%s;\n",
                              name.c_str(), key.offsetx, key.offsety, expandSwizzle.c_str());
      }

      fetcher.hlsl += StringFormat::Fmt(
          "  EvalCacheBuffer[evalIdx+stride*0+%u] = "
          "quadSwizzleHelper(evalCacheVal, quadLaneIndex, 0);\n",
          evalIdx);
      fetcher.hlsl += StringFormat::Fmt(
          "  EvalCacheBuffer[evalIdx+stride*1+%u] = "
          "quadSwizzleHelper(evalCacheVal, quadLaneIndex, 1);\n",
          evalIdx);
      fetcher.hlsl += StringFormat::Fmt(
          "  EvalCacheBuffer[evalIdx+stride*2+%u] = "
          "quadSwizzleHelper(evalCacheVal, quadLaneIndex, 2);\n",
          evalIdx);
      fetcher.hlsl += StringFormat::Fmt(
          "  EvalCacheBuffer[evalIdx+stride*3+%u] = "
          "quadSwizzleHelper(evalCacheVal, quadLaneIndex, 3);\n",
          evalIdx);

      evalIdx++;
    }
  }

  fetcher.hlsl += "\n}\n";
}

void CreateInputFetcher(const DXBC::DXBCContainer *dxbc, const DXBC::DXBCContainer *prevdxbc,
                        const InputFetcherConfig &cfg, InputFetcher &fetcher)
{
  if(cfg.fetchWorkgroup && dxbc->m_Type != DXBC::ShaderType::Compute)
  {
    RDCERR("Can only fetch workgroup inputs for Compute shaders");
    return;
  }

  bool usePrimitiveID = prevdxbc && ((prevdxbc->m_Type != DXBC::ShaderType::Geometry) &&
                                     (prevdxbc->m_Type != DXBC::ShaderType::Mesh));

  rdcarray<rdcstr> floatInputs;
  rdcarray<rdcstr> nonfloatInputs;
  rdcarray<rdcstr> inputVarNames;
  if(dxbc->m_Type == DXBC::ShaderType::Compute)
  {
    fetcher.hlsl += R"(
struct Inputs
{
};
void CopyInputs(out Inputs OUT, in Inputs IN) {}
)";
  }
  else
  {
    DXDebug::GatherInputDataForInitialValues(dxbc, fetcher, prevdxbc, floatInputs, nonfloatInputs,
                                             inputVarNames);
  }

  for(const InputElement &e : fetcher.inputs)
  {
    if(e.sysattribute == ShaderBuiltin::PrimitiveIndex)
    {
      usePrimitiveID = true;
      break;
    }
  }

  uint32_t waveSize = cfg.maxWaveSize;
  uint32_t reqWaveSize = dxbc->GetReflection()->WaveSize;
  if(reqWaveSize != 0)
  {
    if(reqWaveSize < waveSize)
      waveSize = reqWaveSize;
    else
      RDCERR("Invalid requested wave size %u vs device maximum of %u", reqWaveSize, cfg.maxWaveSize);
  }

  fetcher.numLanesPerHit = cfg.fetchWorkgroup ? cfg.groupSize : waveSize;

  fetcher.hlsl += StringFormat::Fmt(
      "#define STAGE_VS %u\n"
      "#define STAGE_PS %u\n"
      "#define STAGE_CS %u\n"
      "#define STAGE %u\n"
      "#define MAXHIT %u\n"
      "#define MAXWAVESIZE %u\n"
      "#define USEPRIM %u\n"
      "#define FETCH_WORKGROUP %u\n",
      DXBC::ShaderType::Vertex, DXBC::ShaderType::Pixel, DXBC::ShaderType::Compute, dxbc->m_Type,
      DXDebug::maxPixelHits, waveSize, usePrimitiveID ? 1 : 0, cfg.fetchWorkgroup);

  if(dxbc->m_Type == DXBC::ShaderType::Vertex)
  {
    fetcher.hlsl += StringFormat::Fmt(
        "#define DEST_VERT %u\n"
        "#define DEST_INST %u\n",
        cfg.vert, cfg.inst);
  }
  else if(dxbc->m_Type == DXBC::ShaderType::Pixel)
  {
    fetcher.hlsl += StringFormat::Fmt(
        "#define DESTX %u.5\n"
        "#define DESTY %u.5\n",
        cfg.x, cfg.y);
  }
  else if(dxbc->m_Type == DXBC::ShaderType::Compute)
  {
    fetcher.hlsl += StringFormat::Fmt(
        "#define DESTX %u\n"
        "#define DESTY %u\n"
        "#define DESTZ %u\n",
        cfg.threadid[0], cfg.threadid[1], cfg.threadid[2]);
    fetcher.hlsl += StringFormat::Fmt("#define NUMTHREADS %u,%u,%u\n",
                                      dxbc->GetReflection()->DispatchThreadsDimension[0],
                                      dxbc->GetReflection()->DispatchThreadsDimension[1],
                                      dxbc->GetReflection()->DispatchThreadsDimension[2]);
    fetcher.hlsl += StringFormat::Fmt(
        "#define GROUPX %u\n"
        "#define GROUPY %u\n"
        "#define GROUPZ %u\n",
        cfg.groupid[0], cfg.groupid[1], cfg.groupid[2]);
  }
  else
  {
    RDCERR("Unexpected type of shader");
  }

  if(cfg.uavspace == 0)
  {
    fetcher.hlsl += StringFormat::Fmt(
        "#define HITBUFFER u%u\n"
        "#define EVALCACHEBUFFER u%u\n"
        "#define LANEBUFFER u%u\n",
        cfg.uavslot, cfg.uavslot + 1, cfg.uavslot + 2);
  }
  else
  {
    fetcher.hlsl += StringFormat::Fmt(
        "#define HITBUFFER u%u, space%u\n"
        "#define EVALCACHEBUFFER u%u, space%u\n"
        "#define LANEBUFFER u%u, space%u\n",
        cfg.uavslot, cfg.uavspace, cfg.uavslot + 1, cfg.uavspace, cfg.uavslot + 2, cfg.uavspace);
  }

  fetcher.hlsl += "\n";

  if(waveSize == 4 && dxbc->m_Type == DXBC::ShaderType::Pixel)
    return CreateLegacyInputFetcher(dxbc, cfg, floatInputs, inputVarNames, fetcher);

  fetcher.hitBufferStride = sizeof(DXDebug::DebugHit);

  if(dxbc->m_Type == DXBC::ShaderType::Vertex)
    fetcher.laneDataBufferStride = sizeof(DXDebug::VSLaneData) + fetcher.laneDataBufferStride;
  else if(dxbc->m_Type == DXBC::ShaderType::Pixel)
    fetcher.laneDataBufferStride = sizeof(DXDebug::PSLaneData) + fetcher.laneDataBufferStride;
  else if(dxbc->m_Type == DXBC::ShaderType::Compute)
    fetcher.laneDataBufferStride = sizeof(DXDebug::CSLaneData) + fetcher.laneDataBufferStride;

  fetcher.hlsl += R"(
struct VSLaneData
{
#if STAGE == STAGE_VS
  uint inst;
  uint vert;
  uint2 pad;
#endif
};

struct PSLaneData
{
#if STAGE == STAGE_PS
  float4 pixelPos;

  uint isHelper;
  uint quadId;
  uint quadLane;
  uint coverage;

  uint sample;
  uint primitive;
  uint isFrontFace;
  uint pad2;
#endif
};

struct CSLaneData
{
#if STAGE == STAGE_CS
  uint3 threadid;
  uint activeSubgroup;
#endif
};

struct LaneData
{
  uint laneIndex;
  uint active;
  uint2 pad2;

  VSLaneData vs;
  PSLaneData ps;
  CSLaneData cs;

  Inputs IN;
};

struct DebugHit
{
  // only used in the first instance
  uint numHits;
  float3 pos_depth; // xy position and depth

  float derivValid;
  uint quadLaneIndex;
  uint laneIndex;
  uint subgroupSize;

  uint sample;
  uint primitive;
  uint2 pad;

  uint4 globalBallot;
  uint4 helperBallot;
};

RWStructuredBuffer<DebugHit> HitBuffer : register(HITBUFFER);
RWStructuredBuffer<LaneData> LaneBuffer : register(LANEBUFFER);

#if STAGE == STAGE_CS
[numthreads(NUMTHREADS)]
#endif

void ExtractInputs(Inputs IN

#if STAGE == STAGE_VS

  #ifndef VERT_VAR
                     , uint vert : SV_VertexID
  #endif
  #ifndef INST_VAR
                     , uint inst : SV_InstanceID
  #endif

#elif STAGE == STAGE_PS


  #ifndef POSITION_VAR
                     , float4 debug_pixelPos : SV_Position
  #endif
  #if USEPRIM && !defined(PRIM_VAR)
                     , uint primitive : SV_PrimitiveID
  #endif
                     // sample, coverage and isFrontFace are deliberately omittted from the
                     // IN struct for SV_ ordering reasons
                     , uint sample : SV_SampleIndex
                     , uint coverage : SV_Coverage
                     , bool isFrontFace : SV_IsFrontFace

#elif STAGE == STAGE_CS

                     , uint3 threadid : SV_GroupThreadID 
                     , uint3 dtid : SV_DispatchThreadID 
                     , uint3 groupid : SV_GroupID
                     , uint groupindex : SV_GroupIndex

#endif
)
{
#ifdef VERT_VAR
  uint vert = IN.VERT_VAR;
#endif

#ifdef INST_VAR
  uint inst = IN.INST_VAR;
#endif

#ifdef POSITION_VAR
  float4 debug_pixelPos = IN.POSITION_VAR;
#endif

#if USEPRIM && defined(PRIM_VAR)
  uint primitive = IN.PRIM_VAR;
#elif !USEPRIM && STAGE == STAGE_PS
  uint primitive = 0;
#endif

#if STAGE != STAGE_PS
  float4 debug_pixelPos = 0;
  uint primitive = 0;
  uint sample = 0;
  uint isFrontFace = 0;
#endif

  VSLaneData vs = (VSLaneData)0;
  PSLaneData ps = (PSLaneData)0;
  CSLaneData cs = (CSLaneData)0;

  uint isHelper = 0;
  uint quadLaneIndex = 0;
  uint quadId = 0;
  uint4 globalBallot = WaveActiveBallot(true);
  uint laneIndex = WaveGetLaneIndex();
  uint4 helperBallot = 0;
  float derivValid = 1.0f;

#if STAGE == STAGE_VS
  bool candidateThread = (vert == DEST_VERT && inst == DEST_INST);
  bool fetchWorkgroup = false;

  vs.vert = vert;
  vs.inst = inst;
#elif STAGE == STAGE_PS
  bool candidateThread = (abs(debug_pixelPos.x - DESTX) < 0.5f && abs(debug_pixelPos.y - DESTY) < 0.5f);
  bool fetchWorkgroup = false;

  quadLaneIndex = (2u * (uint(debug_pixelPos.y) & 1u)) + (uint(debug_pixelPos.x) & 1u);
  derivValid = ddx(debug_pixelPos.x);
  isHelper = IsHelperLane() ? 1 : 0;

  helperBallot = WaveActiveBallot(isHelper != 0);

  // quadId is a single value that's unique for this quad and uniform across the quad. Degenerate
  // for the simple quad case
  quadId = 1000+QuadReadLaneAt(laneIndex, 0u);

  LaneData helper0data = (LaneData)0;
  LaneData helper1data = (LaneData)0;
  LaneData helper2data = (LaneData)0;
  LaneData helper3data = (LaneData)0;

)";

  for(uint32_t q = 0; q < 4; q++)
  {
    fetcher.hlsl += StringFormat::Fmt("  // quad %u\n", q);
    fetcher.hlsl += "  {\n";
    fetcher.hlsl += StringFormat::Fmt(
        "    helper%udata.ps.pixelPos = QuadReadLaneAt(debug_pixelPos, %uu);\n", q, q);
    fetcher.hlsl +=
        StringFormat::Fmt("    helper%udata.ps.isHelper = QuadReadLaneAt(isHelper, %uu);\n", q, q);
    fetcher.hlsl += StringFormat::Fmt("    helper%udata.ps.quadId = quadId;\n", q);
    fetcher.hlsl += StringFormat::Fmt("    helper%udata.ps.quadLane = %uu;\n", q, q);
    fetcher.hlsl +=
        StringFormat::Fmt("    helper%udata.ps.coverage = QuadReadLaneAt(coverage, %uu);\n", q, q);
    fetcher.hlsl +=
        StringFormat::Fmt("    helper%udata.laneIndex = QuadReadLaneAt(laneIndex, %uu);\n", q, q);
    fetcher.hlsl += StringFormat::Fmt("    helper%udata.active = 1;\n", q, q);
    for(size_t i = 0; i < floatInputs.size(); i++)
    {
      const rdcstr &name = floatInputs[i];
      fetcher.hlsl += StringFormat::Fmt("    helper%udata.IN.%s = QuadReadLaneAt(IN.%s, %u);\n", q,
                                        name.c_str(), name.c_str(), q);
    }
    for(size_t i = 0; i < nonfloatInputs.size(); i++)
    {
      const rdcstr &name = nonfloatInputs[i];
      fetcher.hlsl += StringFormat::Fmt("    helper%udata.IN.%s = QuadReadLaneAt(IN.%s, %u);\n", q,
                                        name.c_str(), name.c_str(), q);
    }
    fetcher.hlsl += "  }\n\n";
  }

  fetcher.hlsl += R"(
  ps.pixelPos = debug_pixelPos;

  ps.isHelper = isHelper;
  ps.quadId = quadId;
  ps.quadLane = quadLaneIndex;
  ps.coverage = coverage;

  ps.sample = sample;
  ps.primitive = primitive;
  ps.isFrontFace = isFrontFace;
#elif STAGE == STAGE_CS
  bool candidateThread = (dtid.x == DESTX && dtid.y == DESTY && dtid.z == DESTZ);
  cs.threadid = threadid;
#endif

#if FETCH_WORKGROUP

#if STAGE != STAGE_CS
#error "Only compute shader fetches whole workgroup"
#endif // #if STAGE != STAGE_CS

  bool candidateGroup = (groupid.x == GROUPX && groupid.y == GROUPY && groupid.z == GROUPZ);
  if (!candidateGroup)
    return;

  bool activeSubgroup = WaveActiveAnyTrue(candidateThread);
  cs.activeSubgroup = activeSubgroup ? 1 : 0;
  if(candidateThread)
  {
    HitBuffer[0].numHits = 1;
    HitBuffer[0].pos_depth = debug_pixelPos.xyz;
    HitBuffer[0].derivValid = derivValid;
    HitBuffer[0].primitive = primitive;
    HitBuffer[0].sample = sample;
    HitBuffer[0].laneIndex = laneIndex;
    HitBuffer[0].quadLaneIndex = quadLaneIndex;
    HitBuffer[0].subgroupSize = WaveGetLaneCount();
    HitBuffer[0].globalBallot = globalBallot;
    HitBuffer[0].helperBallot = helperBallot;
  }
  // Use SV_GroupIndex as the output index
  LaneBuffer[groupindex].laneIndex = laneIndex;
  LaneBuffer[groupindex].active = 1;
  LaneBuffer[groupindex].cs = cs;
#else // #if FETCH_WORKGROUP
  bool activeSubgroup = WaveActiveAnyTrue(candidateThread);
#if STAGE == STAGE_CS
  cs.activeSubgroup = activeSubgroup ? 1 : 0;
#endif
  if (activeSubgroup)
  {
    if(isHelper == 0)
    {
      uint idx = MAXHIT;
      if(WaveIsFirstLane())
      {
        InterlockedAdd(HitBuffer[0].numHits, 1, idx);
      }
      idx = WaveReadLaneFirst(idx);
      if(idx < MAXHIT)
      {
        if(candidateThread)
        {
          HitBuffer[idx].pos_depth = debug_pixelPos.xyz;
          HitBuffer[idx].derivValid = derivValid;
          HitBuffer[idx].primitive = primitive;
          HitBuffer[idx].sample = sample;
          HitBuffer[idx].laneIndex = laneIndex;
          HitBuffer[idx].quadLaneIndex = quadLaneIndex;
          HitBuffer[idx].subgroupSize = WaveGetLaneCount();
          HitBuffer[idx].globalBallot = globalBallot;
          HitBuffer[idx].helperBallot = helperBallot;
        }

#if STAGE == STAGE_PS
        if(helper0data.ps.isHelper)
          LaneBuffer[idx*MAXWAVESIZE+helper0data.laneIndex] = helper0data;

        if(helper1data.ps.isHelper)
          LaneBuffer[idx*MAXWAVESIZE+helper1data.laneIndex] = helper1data;

        if(helper2data.ps.isHelper)
          LaneBuffer[idx*MAXWAVESIZE+helper2data.laneIndex] = helper2data;

        if(helper3data.ps.isHelper)
          LaneBuffer[idx*MAXWAVESIZE+helper3data.laneIndex] = helper3data;
#endif

        LaneBuffer[idx*MAXWAVESIZE+laneIndex].laneIndex = laneIndex;
        LaneBuffer[idx*MAXWAVESIZE+laneIndex].active = 1;
        LaneBuffer[idx*MAXWAVESIZE+laneIndex].vs = vs;
        LaneBuffer[idx*MAXWAVESIZE+laneIndex].ps = ps;
        LaneBuffer[idx*MAXWAVESIZE+laneIndex].cs = cs;
        CopyInputs(LaneBuffer[idx*MAXWAVESIZE+laneIndex].IN, IN);
      }
    }
  }
#endif // #if FETCH_WORKGROUP
}
)";
}

// "NaN has special handling. If one source operand is NaN, then the other source operand is
// returned. If both are NaN, any NaN representation is returned."

float dxbc_min(float a, float b)
{
  if(RDCISNAN(a))
    return b;

  if(RDCISNAN(b))
    return a;

  return a < b ? a : b;
}

double dxbc_min(double a, double b)
{
  if(RDCISNAN(a))
    return b;

  if(RDCISNAN(b))
    return a;

  return a < b ? a : b;
}

float dxbc_max(float a, float b)
{
  if(RDCISNAN(a))
    return b;

  if(RDCISNAN(b))
    return a;

  return a >= b ? a : b;
}

double dxbc_max(double a, double b)
{
  if(RDCISNAN(a))
    return b;

  if(RDCISNAN(b))
    return a;

  return a >= b ? a : b;
}

float round_ne(float x)
{
  if(!RDCISFINITE(x))
    return x;

  float rem = remainderf(x, 1.0f);

  return x - rem;
}

double round_ne(double x)
{
  if(!RDCISFINITE(x))
    return x;

  double rem = remainder(x, 1.0);

  return x - rem;
}

float flush_denorm(const float f)
{
  uint32_t x;
  memcpy(&x, &f, sizeof(f));

  // if any bit is set in the exponent, it's not denormal
  if(x & 0x7F800000)
    return f;

  // keep only the sign bit
  x &= 0x80000000;
  float ret;
  memcpy(&ret, &x, sizeof(ret));
  return ret;
}

uint32_t BitwiseReverseLSB16(uint32_t x)
{
  // Reverse the bits in x, then discard the lower half
  // https://graphics.stanford.edu/~seander/bithacks.html#ReverseParallel
  x = ((x >> 1) & 0x55555555) | ((x & 0x55555555) << 1);
  x = ((x >> 2) & 0x33333333) | ((x & 0x33333333) << 2);
  x = ((x >> 4) & 0x0F0F0F0F) | ((x & 0x0F0F0F0F) << 4);
  x = ((x >> 8) & 0x00FF00FF) | ((x & 0x00FF00FF) << 8);
  return x << 16;
}

uint32_t PopCount(uint32_t x)
{
  // https://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
  x = x - ((x >> 1) & 0x55555555);
  x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
  return (((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

void get_sample_position(uint32_t sampleIndex, uint32_t sampleCount, float *position)
{
  // assume standard sample pattern - this might not hold in all cases
  // http://msdn.microsoft.com/en-us/library/windows/desktop/ff476218(v=vs.85).aspx

  if(sampleIndex >= sampleCount)
  {
    // Per HLSL docs, if sampleIndex is out of bounds a zero vector is returned
    RDCWARN("sample index %u is out of bounds on resource bound to sample_pos (%u samples)",
            sampleIndex, sampleCount);
    position[0] = 0.0f;
    position[1] = 0.0f;
    position[2] = 0.0f;
    position[3] = 0.0f;
  }
  else
  {
    const float *sample_pattern = NULL;

// co-ordinates are given as (i,j) in 16ths of a pixel
#define _SMP(c) ((c) / 16.0f)

    if(sampleCount == 1)
    {
      sample_pattern = NULL;
    }
    else if(sampleCount == 2)
    {
      static const float pattern_2x[] = {
          _SMP(4.0f),
          _SMP(4.0f),
          _SMP(-4.0f),
          _SMP(-4.0f),
      };

      sample_pattern = &pattern_2x[0];
    }
    else if(sampleCount == 4)
    {
      static const float pattern_4x[] = {
          _SMP(-2.0f), _SMP(-6.0f), _SMP(6.0f), _SMP(-2.0f),
          _SMP(-6.0f), _SMP(2.0f),  _SMP(2.0f), _SMP(6.0f),
      };

      sample_pattern = &pattern_4x[0];
    }
    else if(sampleCount == 8)
    {
      static const float pattern_8x[] = {
          _SMP(1.0f),  _SMP(-3.0f), _SMP(-1.0f), _SMP(3.0f),  _SMP(5.0f),  _SMP(1.0f),
          _SMP(-3.0f), _SMP(-5.0f), _SMP(-5.0f), _SMP(5.0f),  _SMP(-7.0f), _SMP(-1.0f),
          _SMP(3.0f),  _SMP(7.0f),  _SMP(7.0f),  _SMP(-7.0f),
      };

      sample_pattern = &pattern_8x[0];
    }
    else if(sampleCount == 16)
    {
      static const float pattern_16x[] = {
          _SMP(1.0f),  _SMP(1.0f),  _SMP(-1.0f), _SMP(-3.0f), _SMP(-3.0f), _SMP(2.0f),  _SMP(4.0f),
          _SMP(-1.0f), _SMP(-5.0f), _SMP(-2.0f), _SMP(2.0f),  _SMP(5.0f),  _SMP(5.0f),  _SMP(3.0f),
          _SMP(3.0f),  _SMP(-5.0f), _SMP(-2.0f), _SMP(6.0f),  _SMP(0.0f),  _SMP(-7.0f), _SMP(-4.0f),
          _SMP(-6.0f), _SMP(-6.0f), _SMP(4.0f),  _SMP(-8.0f), _SMP(0.0f),  _SMP(7.0f),  _SMP(-4.0f),
          _SMP(6.0f),  _SMP(7.0f),  _SMP(-7.0f), _SMP(-8.0f),
      };

      sample_pattern = &pattern_16x[0];
    }
    else    // unsupported sample count
    {
      RDCERR("Unsupported sample count on resource for sample_pos: %u", sampleCount);
      sample_pattern = NULL;
    }

    if(sample_pattern == NULL)
    {
      position[0] = 0.0f;
      position[1] = 0.0f;
    }
    else
    {
      position[0] = sample_pattern[sampleIndex * 2 + 0];
      position[1] = sample_pattern[sampleIndex * 2 + 1];
    }
  }
#undef _SMP
}

};    // namespace DXDebug

#if ENABLED(ENABLE_UNIT_TESTS)

#include <limits>
#include "catch/catch.hpp"

using namespace DXDebug;

TEST_CASE("DXBC DXIL shader debugging helpers", "[program]")
{
  const float posinf = std::numeric_limits<float>::infinity();
  const float neginf = -std::numeric_limits<float>::infinity();
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float a = 1.0f;
  const float b = 2.0f;

  SECTION("dxbc_min")
  {
    CHECK(dxbc_min(neginf, neginf) == neginf);
    CHECK(dxbc_min(neginf, a) == neginf);
    CHECK(dxbc_min(neginf, posinf) == neginf);
    CHECK(dxbc_min(neginf, nan) == neginf);
    CHECK(dxbc_min(a, neginf) == neginf);
    CHECK(dxbc_min(a, b) == a);
    CHECK(dxbc_min(a, posinf) == a);
    CHECK(dxbc_min(a, nan) == a);
    CHECK(dxbc_min(posinf, neginf) == neginf);
    CHECK(dxbc_min(posinf, a) == a);
    CHECK(dxbc_min(posinf, posinf) == posinf);
    CHECK(dxbc_min(posinf, nan) == posinf);
    CHECK(dxbc_min(nan, neginf) == neginf);
    CHECK(dxbc_min(nan, a) == a);
    CHECK(dxbc_min(nan, posinf) == posinf);
    CHECK(RDCISNAN(dxbc_min(nan, nan)));
  };

  SECTION("dxbc_max")
  {
    CHECK(dxbc_max(neginf, neginf) == neginf);
    CHECK(dxbc_max(neginf, a) == a);
    CHECK(dxbc_max(neginf, posinf) == posinf);
    CHECK(dxbc_max(neginf, nan) == neginf);
    CHECK(dxbc_max(a, neginf) == a);
    CHECK(dxbc_max(a, b) == b);
    CHECK(dxbc_max(a, posinf) == posinf);
    CHECK(dxbc_max(a, nan) == a);
    CHECK(dxbc_max(posinf, neginf) == posinf);
    CHECK(dxbc_max(posinf, a) == posinf);
    CHECK(dxbc_max(posinf, posinf) == posinf);
    CHECK(dxbc_max(posinf, nan) == posinf);
    CHECK(dxbc_max(nan, neginf) == neginf);
    CHECK(dxbc_max(nan, a) == a);
    CHECK(dxbc_max(nan, posinf) == posinf);
    CHECK(RDCISNAN(dxbc_max(nan, nan)));
  };

  SECTION("test denorm flushing")
  {
    float foo = 3.141f;

    // check normal values
    CHECK(flush_denorm(0.0f) == 0.0f);
    CHECK(flush_denorm(foo) == foo);
    CHECK(flush_denorm(-foo) == -foo);

    // check NaN/inf values
    CHECK(RDCISNAN(flush_denorm(nan)));
    CHECK(flush_denorm(neginf) == neginf);
    CHECK(flush_denorm(posinf) == posinf);

    // check zero sign bit - bit more complex
    uint32_t negzero = 0x80000000U;
    float negzerof;
    memcpy(&negzerof, &negzero, sizeof(negzero));

    float flushed = flush_denorm(negzerof);
    CHECK(memcmp(&flushed, &negzerof, sizeof(negzerof)) == 0);

    // check that denormal values are flushed, preserving sign
    foo = 1.12104e-44f;
    CHECK(flush_denorm(foo) != foo);
    CHECK(flush_denorm(-foo) != -foo);
    CHECK(flush_denorm(foo) == 0.0f);
    flushed = flush_denorm(-foo);
    CHECK(memcmp(&flushed, &negzerof, sizeof(negzerof)) == 0);
  };
};

#endif    // ENABLED(ENABLE_UNIT_TESTS)
