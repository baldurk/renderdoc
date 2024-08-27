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

#include "dx_debug.h"
#include "common/formatting.h"
#include "dxbc_common.h"

namespace DXDebug
{
void GatherPSInputDataForInitialValues(const rdcarray<SigParameter> &stageInputSig,
                                       const rdcarray<SigParameter> &prevStageOutputSig,
                                       const rdcarray<DXBC::InterpolationMode> &interpModes,
                                       rdcarray<PSInputElement> &initialValues,
                                       rdcarray<rdcstr> &floatInputs, rdcarray<rdcstr> &inputVarNames,
                                       rdcstr &psInputDefinition, int &structureStride)
{
  // When debugging a pixel shader, we need to get the initial values of each pixel shader
  // input for the pixel that we are debugging, from whichever the previous shader stage was
  // configured in the pipeline. This function returns the input element definitions, other
  // associated data, the HLSL definition to use when gathering pixel shader initial values,
  // and the stride of that HLSL structure.

  // This function does not provide any HLSL definitions for additional metadata that may be
  // needed for gathering initial values, such as primitive ID, and also does not provide the
  // shader function body.

  initialValues.clear();
  floatInputs.clear();
  inputVarNames.clear();
  psInputDefinition = "struct PSInput\n{\n";
  structureStride = 0;

  if(stageInputSig.empty())
  {
    psInputDefinition += "float4 input_dummy : SV_Position;\n";

    initialValues.push_back(PSInputElement(-1, 0, 4, ShaderBuiltin::Undefined, true));

    structureStride += 4;
  }

  // name, pair<start semantic index, end semantic index>
  rdcarray<rdcpair<rdcstr, rdcpair<uint32_t, uint32_t>>> arrays;

  uint32_t nextreg = 0;

  size_t numInputs = stageInputSig.size();
  inputVarNames.resize(numInputs);

  for(size_t i = 0; i < numInputs; i++)
  {
    const SigParameter &sig = stageInputSig[i];

    psInputDefinition += "  ";

    bool included = true;

    // handled specially to account for SV_ ordering
    if(sig.systemValue == ShaderBuiltin::MSAACoverage ||
       sig.systemValue == ShaderBuiltin::IsFrontFace ||
       sig.systemValue == ShaderBuiltin::MSAASampleIndex)
    {
      psInputDefinition += "//";
      included = false;
    }

    // it seems sometimes primitive ID can be included within inputs and isn't subject to the SV_
    // ordering restrictions - possibly to allow for geometry shaders to output the primitive ID as
    // an interpolant. Only comment it out if it's the last input.
    if(i + 1 == numInputs && sig.systemValue == ShaderBuiltin::PrimitiveIndex)
    {
      psInputDefinition += "//";
      included = false;
    }

    int arrayIndex = -1;

    for(size_t a = 0; a < arrays.size(); a++)
    {
      if(sig.semanticName == arrays[a].first && arrays[a].second.first <= sig.semanticIndex &&
         arrays[a].second.second >= sig.semanticIndex)
      {
        psInputDefinition += "//";
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

          if(prevStageOutputSig[os].varType == VarType::Float)
            psInputDefinition += "float";
          else if(prevStageOutputSig[os].varType == VarType::SInt)
            psInputDefinition += "int";
          else if(prevStageOutputSig[os].varType == VarType::UInt)
            psInputDefinition += "uint";
          else
            RDCERR("Unexpected input signature type: %s",
                   ToStr(prevStageOutputSig[os].varType).c_str());

          int numCols = (prevStageOutputSig[os].regChannelMask & 0x1 ? 1 : 0) +
                        (prevStageOutputSig[os].regChannelMask & 0x2 ? 1 : 0) +
                        (prevStageOutputSig[os].regChannelMask & 0x4 ? 1 : 0) +
                        (prevStageOutputSig[os].regChannelMask & 0x8 ? 1 : 0);

          structureStride += 4 * numCols;

          initialValues.push_back(PSInputElement(-1, 0, numCols, ShaderBuiltin::Undefined, true));

          rdcstr name = prevStageOutputSig[os].semanticIdxName;

          psInputDefinition += ToStr((uint32_t)numCols) + " input_" + name + " : " + name + ";\n";
        }
      }

      if(!filled)
      {
        rdcstr dummy_reg = "dummy_register";
        dummy_reg += ToStr((uint32_t)nextreg + dummy);
        psInputDefinition += "float4 var_" + dummy_reg + " : semantic_" + dummy_reg + ";\n";

        initialValues.push_back(PSInputElement(-1, 0, 4, ShaderBuiltin::Undefined, true));

        structureStride += 4 * sizeof(float);
      }
    }

    nextreg = sig.regIndex + 1;

    DXBC::InterpolationMode interpolation = interpModes[i];
    if(interpolation != DXBC::InterpolationMode::INTERPOLATION_UNDEFINED)
      psInputDefinition += ToStr(interpolation) + " ";
    psInputDefinition += ToStr(sig.varType);

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

    if(included && numCols <= 2 && (sig.regChannelMask & 0x1))
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
      structureStride += 4 * numCols * RDCMAX(1, arrayLength);
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
    if(included && arrayLength == 0 && numCols <= 2 && (sig.regChannelMask & 0x1))
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

    psInputDefinition += ToStr((uint32_t)numCols) + " input_" + name;
    if(arrayLength > 0)
      psInputDefinition += "[" + ToStr(arrayLength) + "]";
    psInputDefinition += " : " + name;

    inputVarNames[i] = "input_" + name;
    if(arrayLength > 0)
      inputVarNames[i] += StringFormat::Fmt("[%d]", RDCMAX(0, arrayIndex));

    if(included && sig.varType == VarType::Float)
    {
      if(arrayLength == 0)
      {
        floatInputs.push_back("input_" + name);
      }
      else
      {
        for(int a = 0; a < arrayLength; a++)
          floatInputs.push_back("input_" + name + "[" + ToStr(a) + "]");
      }
    }

    psInputDefinition += ";\n";

    int firstElem = sig.regChannelMask & 0x1   ? 0
                    : sig.regChannelMask & 0x2 ? 1
                    : sig.regChannelMask & 0x4 ? 2
                    : sig.regChannelMask & 0x8 ? 3
                                               : -1;

    // arrays get added all at once (because in the struct data, they are contiguous even if
    // in the input signature they're not).
    if(arrayIndex < 0)
    {
      if(arrayLength == 0)
      {
        initialValues.push_back(
            PSInputElement(sig.regIndex, firstElem, numCols, sig.systemValue, included));
      }
      else
      {
        for(int a = 0; a < arrayLength; a++)
        {
          initialValues.push_back(
              PSInputElement(sig.regIndex + a, firstElem, numCols, sig.systemValue, included));
        }
      }
    }
  }

  psInputDefinition += "};\n\n";
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
