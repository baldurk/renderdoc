/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2022-2024 Baldur Karlsson
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

#include "hlsl_cbuffers.h"

struct OutStruct
{
  float4 outf[2];
  uint4 outu[2];
  int4 outi[2];
};

RWStructuredBuffer<OutStruct> outBuf : register(u1);

[numthreads(1, 1, 1)] void RENDERDOC_DebugMathOp() {
  switch(mathOp)
  {
    case DEBUG_SAMPLE_MATH_DXBC_RCP: outBuf[0].outf[0] = rcp(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXBC_RSQ: outBuf[0].outf[0] = rsqrt(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXBC_EXP: outBuf[0].outf[0] = exp2(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXBC_LOG: outBuf[0].outf[0] = log2(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXBC_SINCOS:
      sincos(mathInVal, outBuf[0].outf[0], outBuf[0].outf[1]);
      break;
    case DEBUG_SAMPLE_MATH_DXIL_COS: outBuf[0].outf[0] = cos(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXIL_SIN: outBuf[0].outf[0] = sin(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXIL_TAN: outBuf[0].outf[0] = tan(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXIL_ACOS: outBuf[0].outf[0] = acos(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXIL_ASIN: outBuf[0].outf[0] = asin(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXIL_ATAN: outBuf[0].outf[0] = atan(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXIL_HCOS: outBuf[0].outf[0] = cosh(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXIL_HSIN: outBuf[0].outf[0] = sinh(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXIL_HTAN: outBuf[0].outf[0] = tanh(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXIL_EXP: outBuf[0].outf[0] = exp(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXIL_LOG: outBuf[0].outf[0] = log(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXIL_SQRT: outBuf[0].outf[0] = sqrt(mathInVal); break;
    case DEBUG_SAMPLE_MATH_DXIL_RSQRT: outBuf[0].outf[0] = rsqrt(mathInVal); break;
    default: break;
  }
}

void RENDERDOC_DebugSampleVS(uint id
                             : SV_VertexID, out float4 pos
                             : SV_Position, out float4 uv
                             : UVS)
{
  if(id == 0)
    uv = debugSampleUV + debugSampleDDY * 2.0f;
  else if(id == 1)
    uv = debugSampleUV;
  else if(id == 2)
    uv = debugSampleUV + debugSampleDDX * 2.0f;
  else
    uv = 0.0f.xxxx;

  pos = float4((id == 2) ? 3.0f : -1.0f, (id == 0) ? -3.0f : 1.0f, 0.5, 1.0);
}

SamplerState s : register(s0);
SamplerComparisonState sc : register(s1);

Texture1DArray<unorm float4> t1D_unorm : register(t0);
Texture2DArray<unorm float4> t2D_unorm : register(t1);
Texture3D<unorm float4> t3D_unorm : register(t2);
Texture2DMSArray<unorm float4> tMS_unorm : register(t3);
TextureCubeArray<unorm float4> tCube_unorm : register(t4);

Texture1DArray<snorm float4> t1D_snorm : register(t5);
Texture2DArray<snorm float4> t2D_snorm : register(t6);
Texture3D<snorm float4> t3D_snorm : register(t7);
Texture2DMSArray<snorm float4> tMS_snorm : register(t8);
TextureCubeArray<snorm float4> tCube_snorm : register(t9);

Texture1DArray<int4> t1D_int : register(t10);
Texture2DArray<int4> t2D_int : register(t11);
Texture3D<int4> t3D_int : register(t12);
Texture2DMSArray<int4> tMS_int : register(t13);
TextureCubeArray<int> tCube_int : register(t14);

Texture1DArray<uint4> t1D_uint : register(t15);
Texture2DArray<uint4> t2D_uint : register(t16);
Texture3D<uint4> t3D_uint : register(t17);
Texture2DMSArray<uint4> tMS_uint : register(t18);
TextureCubeArray<uint4> tCube_uint : register(t19);

Texture1DArray<float4> t1D_float : register(t20);
Texture2DArray<float4> t2D_float : register(t21);
Texture3D<float4> t3D_float : register(t22);
Texture2DMSArray<float4> tMS_float : register(t23);
TextureCubeArray<float4> tCube_float : register(t24);

// disable implicit truncation of vector type to keep these switches simpler by dimension
#pragma warning(disable : 3206)

#ifndef debugSampleOffsets
#define debugSampleOffsets int4(0, 0, 0, 0)
#endif

float4 DoFloatOpcode(float4 uv)
{
  int4 uvInt = debugSampleUVInt;
  int opcode = debugSampleOperation;
  float4 ddx_ = debugSampleDDX;
  float4 ddy_ = debugSampleDDY;
  int4 offsets = debugSampleOffsets;
  float lod = debugSampleLodCompare;
  float compare = debugSampleLodCompare;

  if(opcode == DEBUG_SAMPLE_TEX_SAMPLE || opcode == DEBUG_SAMPLE_TEX_SAMPLE_BIAS ||
     opcode == DEBUG_SAMPLE_TEX_SAMPLE_GRAD)
  {
    switch(debugSampleTexDim)
    {
      default:
      case DEBUG_SAMPLE_TEX1D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM: return t1D_unorm.SampleGrad(s, uv.xy, ddx_.x, ddy_.x, offsets.x);
          case DEBUG_SAMPLE_SNORM: return t1D_snorm.SampleGrad(s, uv.xy, ddx_.x, ddy_.x, offsets.x);
          default: return t1D_float.SampleGrad(s, uv.xy, ddx_.x, ddy_.x, offsets.x);
        }
      }
      case DEBUG_SAMPLE_TEX2D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM:
            return t2D_unorm.SampleGrad(s, uv.xyz, ddx_.xy, ddy_.xy, offsets.xy);
          case DEBUG_SAMPLE_SNORM:
            return t2D_snorm.SampleGrad(s, uv.xyz, ddx_.xy, ddy_.xy, offsets.xy);
          default: return t2D_float.SampleGrad(s, uv.xyz, ddx_.xy, ddy_.xy, offsets.xy);
        }
      }
      case DEBUG_SAMPLE_TEX3D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM:
            return t3D_unorm.SampleGrad(s, uv.xyz, ddx_.xyz, ddy_.xyz, offsets.xyz);
          case DEBUG_SAMPLE_SNORM:
            return t3D_snorm.SampleGrad(s, uv.xyz, ddx_.xyz, ddy_.xyz, offsets.xyz);
          default: return t3D_float.SampleGrad(s, uv.xyz, ddx_.xyz, ddy_.xyz, offsets.xyz);
        }
      }
      case DEBUG_SAMPLE_TEXCUBE:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM: return tCube_unorm.SampleGrad(s, uv, ddx_.xyz, ddy_.xyz);
          case DEBUG_SAMPLE_SNORM: return tCube_snorm.SampleGrad(s, uv, ddx_.xyz, ddy_.xyz);
          default: return tCube_float.SampleGrad(s, uv, ddx_.xyz, ddy_.xyz);
        }
      }
    }
  }
  else if(opcode == DEBUG_SAMPLE_TEX_SAMPLE_LEVEL)
  {
    switch(debugSampleTexDim)
    {
      default:
      case DEBUG_SAMPLE_TEX1D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM: return t1D_unorm.SampleLevel(s, uv.xy, lod, offsets.x);
          case DEBUG_SAMPLE_SNORM: return t1D_snorm.SampleLevel(s, uv.xy, lod, offsets.x);
          default: return t1D_float.SampleLevel(s, uv.xy, lod, offsets.x);
        }
      }
      case DEBUG_SAMPLE_TEX2D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM: return t2D_unorm.SampleLevel(s, uv.xyz, lod, offsets.xy);
          case DEBUG_SAMPLE_SNORM: return t2D_snorm.SampleLevel(s, uv.xyz, lod, offsets.xy);
          default: return t2D_float.SampleLevel(s, uv.xyz, lod, offsets.xy);
        }
      }
      case DEBUG_SAMPLE_TEX3D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM: return t3D_unorm.SampleLevel(s, uv.xyz, lod, offsets.xyz);
          case DEBUG_SAMPLE_SNORM: return t3D_snorm.SampleLevel(s, uv.xyz, lod, offsets.xyz);
          default: return t3D_float.SampleLevel(s, uv.xyz, lod, offsets.xyz);
        }
      }
      case DEBUG_SAMPLE_TEXCUBE:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM: return tCube_unorm.SampleLevel(s, uv, lod);
          case DEBUG_SAMPLE_SNORM: return tCube_snorm.SampleLevel(s, uv, lod);
          default: return tCube_float.SampleLevel(s, uv, lod);
        }
      }
    }
  }
  else if(opcode == DEBUG_SAMPLE_TEX_LOAD || opcode == DEBUG_SAMPLE_TEX_LOAD_MS)
  {
    switch(debugSampleTexDim)
    {
      default:
      case DEBUG_SAMPLE_TEX1D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM: return t1D_unorm.Load(uvInt.xyz, offsets.x);
          case DEBUG_SAMPLE_SNORM: return t1D_snorm.Load(uvInt.xyz, offsets.x);
          default: return t1D_float.Load(uvInt.xyz, offsets.x);
        }
      }
      case DEBUG_SAMPLE_TEX2D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM: return t2D_unorm.Load(uvInt, offsets.xy);
          case DEBUG_SAMPLE_SNORM: return t2D_snorm.Load(uvInt, offsets.xy);
          default: return t2D_float.Load(uvInt, offsets.xy);
        }
      }
      case DEBUG_SAMPLE_TEX3D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM: return t3D_unorm.Load(uvInt, offsets.xyz);
          case DEBUG_SAMPLE_SNORM: return t3D_snorm.Load(uvInt, offsets.xyz);
          default: return t3D_float.Load(uvInt, offsets.xyz);
        }
      }
      case DEBUG_SAMPLE_TEXMS:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM:
            return tMS_unorm.Load(uvInt.xyz, debugSampleSampleIndex, offsets.xy);
          case DEBUG_SAMPLE_SNORM:
            return tMS_snorm.Load(uvInt.xyz, debugSampleSampleIndex, offsets.xy);
          default: return tMS_float.Load(uvInt.xyz, debugSampleSampleIndex, offsets.xy);
        }
      }
    }
  }
  else if(opcode == DEBUG_SAMPLE_TEX_SAMPLE_CMP)
  {
    switch(debugSampleTexDim)
    {
      default:
      case DEBUG_SAMPLE_TEX1D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM: return t1D_unorm.SampleCmp(sc, uv.xy, compare, offsets.x);
          case DEBUG_SAMPLE_SNORM: return t1D_snorm.SampleCmp(sc, uv.xy, compare, offsets.x);
          default: return t1D_float.SampleCmp(sc, uv.xy, compare, offsets.x);
        }
      }
      case DEBUG_SAMPLE_TEX2D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM: return t2D_unorm.SampleCmp(sc, uv.xyz, compare, offsets.xy);
          case DEBUG_SAMPLE_SNORM: return t2D_snorm.SampleCmp(sc, uv.xyz, compare, offsets.xy);
          default: return t2D_float.SampleCmp(sc, uv.xyz, compare, offsets.xy);
        }
      }
      case DEBUG_SAMPLE_TEXCUBE:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM: return tCube_unorm.SampleCmp(sc, uv, compare);
          case DEBUG_SAMPLE_SNORM: return tCube_snorm.SampleCmp(sc, uv, compare);
          default: return tCube_float.SampleCmp(sc, uv, compare);
        }
      }
    }
  }
  else if(opcode == DEBUG_SAMPLE_TEX_SAMPLE_CMP_LEVEL_ZERO)
  {
    switch(debugSampleTexDim)
    {
      default:
      case DEBUG_SAMPLE_TEX1D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM:
            return t1D_unorm.SampleCmpLevelZero(sc, uv.xy, compare, offsets.x);
          case DEBUG_SAMPLE_SNORM:
            return t1D_snorm.SampleCmpLevelZero(sc, uv.xy, compare, offsets.x);
          default: return t1D_float.SampleCmpLevelZero(sc, uv.xy, compare, offsets.x);
        }
      }
      case DEBUG_SAMPLE_TEX2D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM:
            return t2D_unorm.SampleCmpLevelZero(sc, uv.xyz, compare, offsets.xy);
          case DEBUG_SAMPLE_SNORM:
            return t2D_snorm.SampleCmpLevelZero(sc, uv.xyz, compare, offsets.xy);
          default: return t2D_float.SampleCmpLevelZero(sc, uv.xyz, compare, offsets.xy);
        }
      }
      case DEBUG_SAMPLE_TEXCUBE:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM: return tCube_unorm.SampleCmpLevelZero(sc, uv, compare);
          case DEBUG_SAMPLE_SNORM: return tCube_snorm.SampleCmpLevelZero(sc, uv, compare);
          default: return tCube_float.SampleCmpLevelZero(sc, uv, compare);
        }
      }
    }
  }
  else if(opcode == DEBUG_SAMPLE_TEX_LOD)
  {
    switch(debugSampleTexDim)
    {
      default:
      case DEBUG_SAMPLE_TEX1D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM:
            return float4(t1D_unorm.CalculateLevelOfDetail(s, uv.x),
                          t1D_unorm.CalculateLevelOfDetailUnclamped(s, uv.x), 0.0f, 0.0f);
          case DEBUG_SAMPLE_SNORM:
            return float4(t1D_snorm.CalculateLevelOfDetail(s, uv.x),
                          t1D_snorm.CalculateLevelOfDetailUnclamped(s, uv.x), 0.0f, 0.0f);
          case DEBUG_SAMPLE_UINT:
            return float4(t1D_uint.CalculateLevelOfDetail(s, uv.x),
                          t1D_uint.CalculateLevelOfDetailUnclamped(s, uv.x), 0.0f, 0.0f);
          case DEBUG_SAMPLE_INT:
            return float4(t1D_int.CalculateLevelOfDetail(s, uv.x),
                          t1D_int.CalculateLevelOfDetailUnclamped(s, uv.x), 0.0f, 0.0f);
          default:
            return float4(t1D_float.CalculateLevelOfDetail(s, uv.x),
                          t1D_float.CalculateLevelOfDetailUnclamped(s, uv.x), 0.0f, 0.0f);
        }
      }
      case DEBUG_SAMPLE_TEX2D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM:
            return float4(t2D_unorm.CalculateLevelOfDetail(s, uv.xy),
                          t2D_unorm.CalculateLevelOfDetailUnclamped(s, uv.xy), 0.0f, 0.0f);
          case DEBUG_SAMPLE_SNORM:
            return float4(t2D_snorm.CalculateLevelOfDetail(s, uv.xy),
                          t2D_snorm.CalculateLevelOfDetailUnclamped(s, uv.xy), 0.0f, 0.0f);
          case DEBUG_SAMPLE_UINT:
            return float4(t2D_uint.CalculateLevelOfDetail(s, uv.xy),
                          t2D_uint.CalculateLevelOfDetailUnclamped(s, uv.xy), 0.0f, 0.0f);
          case DEBUG_SAMPLE_INT:
            return float4(t2D_int.CalculateLevelOfDetail(s, uv.xy),
                          t2D_int.CalculateLevelOfDetailUnclamped(s, uv.xy), 0.0f, 0.0f);
          default:
            return float4(t2D_float.CalculateLevelOfDetail(s, uv.xy),
                          t2D_float.CalculateLevelOfDetailUnclamped(s, uv.xy), 0.0f, 0.0f);
        }
      }
      case DEBUG_SAMPLE_TEX3D:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM:
            return float4(t3D_unorm.CalculateLevelOfDetail(s, uv.xyz),
                          t3D_unorm.CalculateLevelOfDetailUnclamped(s, uv.xyz), 0.0f, 0.0f);
          case DEBUG_SAMPLE_SNORM:
            return float4(t3D_snorm.CalculateLevelOfDetail(s, uv.xyz),
                          t3D_snorm.CalculateLevelOfDetailUnclamped(s, uv.xyz), 0.0f, 0.0f);
          case DEBUG_SAMPLE_UINT:
            return float4(t3D_uint.CalculateLevelOfDetail(s, uv.xyz),
                          t3D_uint.CalculateLevelOfDetailUnclamped(s, uv.xyz), 0.0f, 0.0f);
          case DEBUG_SAMPLE_INT:
            return float4(t3D_int.CalculateLevelOfDetail(s, uv.xyz),
                          t3D_int.CalculateLevelOfDetailUnclamped(s, uv.xyz), 0.0f, 0.0f);
          default:
            return float4(t3D_float.CalculateLevelOfDetail(s, uv.xyz),
                          t3D_float.CalculateLevelOfDetailUnclamped(s, uv.xyz), 0.0f, 0.0f);
        }
      }
      case DEBUG_SAMPLE_TEXCUBE:
      {
        switch(debugSampleRetType)
        {
          case DEBUG_SAMPLE_UNORM:
            return float4(tCube_unorm.CalculateLevelOfDetail(s, uv.xyz),
                          tCube_unorm.CalculateLevelOfDetailUnclamped(s, uv.xyz), 0.0f, 0.0f);
          case DEBUG_SAMPLE_SNORM:
            return float4(tCube_snorm.CalculateLevelOfDetail(s, uv.xyz),
                          tCube_snorm.CalculateLevelOfDetailUnclamped(s, uv.xyz), 0.0f, 0.0f);
          case DEBUG_SAMPLE_UINT:
            return float4(tCube_uint.CalculateLevelOfDetail(s, uv.xyz),
                          tCube_uint.CalculateLevelOfDetailUnclamped(s, uv.xyz), 0.0f, 0.0f);
          case DEBUG_SAMPLE_INT:
            return float4(tCube_int.CalculateLevelOfDetail(s, uv.xyz),
                          tCube_int.CalculateLevelOfDetailUnclamped(s, uv.xyz), 0.0f, 0.0f);
          default:
            return float4(tCube_float.CalculateLevelOfDetail(s, uv.xyz),
                          tCube_float.CalculateLevelOfDetailUnclamped(s, uv.xyz), 0.0f, 0.0f);
        }
      }
    }
  }
  else if(opcode == DEBUG_SAMPLE_TEX_GATHER4 || opcode == DEBUG_SAMPLE_TEX_GATHER4_PO)
  {
    if(debugSampleGatherChannel == 0)
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM: return t2D_unorm.GatherRed(s, uv.xyz, offsets.xy);
            case DEBUG_SAMPLE_SNORM: return t2D_snorm.GatherRed(s, uv.xyz, offsets.xy);
            default: return t2D_float.GatherRed(s, uv.xyz, offsets.xy);
          }
        }
        case DEBUG_SAMPLE_TEXCUBE:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM: return tCube_unorm.GatherRed(s, uv);
            case DEBUG_SAMPLE_SNORM: return tCube_snorm.GatherRed(s, uv);
            default: return tCube_float.GatherRed(s, uv);
          }
        }
      }
    }
    else if(debugSampleGatherChannel == 1)
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM: return t2D_unorm.GatherGreen(s, uv.xyz, offsets.xy);
            case DEBUG_SAMPLE_SNORM: return t2D_snorm.GatherGreen(s, uv.xyz, offsets.xy);
            default: return t2D_float.GatherGreen(s, uv.xyz, offsets.xy);
          }
        }
        case DEBUG_SAMPLE_TEXCUBE:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM: return tCube_unorm.GatherGreen(s, uv);
            case DEBUG_SAMPLE_SNORM: return tCube_snorm.GatherGreen(s, uv);
            default: return tCube_float.GatherGreen(s, uv);
          }
        }
      }
    }
    else if(debugSampleGatherChannel == 2)
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM: return t2D_unorm.GatherBlue(s, uv.xyz, offsets.xy);
            case DEBUG_SAMPLE_SNORM: return t2D_snorm.GatherBlue(s, uv.xyz, offsets.xy);
            default: return t2D_float.GatherBlue(s, uv.xyz, offsets.xy);
          }
        }
        case DEBUG_SAMPLE_TEXCUBE:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM: return tCube_unorm.GatherBlue(s, uv);
            case DEBUG_SAMPLE_SNORM: return tCube_snorm.GatherBlue(s, uv);
            default: return tCube_float.GatherBlue(s, uv);
          }
        }
      }
    }
    else
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM: return t2D_unorm.GatherAlpha(s, uv.xyz, offsets.xy);
            case DEBUG_SAMPLE_SNORM: return t2D_snorm.GatherAlpha(s, uv.xyz, offsets.xy);
            default: return t2D_float.GatherAlpha(s, uv.xyz, offsets.xy);
          }
        }
        case DEBUG_SAMPLE_TEXCUBE:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM: return tCube_unorm.GatherAlpha(s, uv);
            case DEBUG_SAMPLE_SNORM: return tCube_snorm.GatherAlpha(s, uv);
            default: return tCube_float.GatherAlpha(s, uv);
          }
        }
      }
    }
  }
  else if(opcode == DEBUG_SAMPLE_TEX_GATHER4_CMP || opcode == DEBUG_SAMPLE_TEX_GATHER4_PO_CMP)
  {
    if(debugSampleGatherChannel == 0)
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM: return t2D_unorm.GatherCmpRed(sc, uv.xyz, compare, offsets.xy);
            case DEBUG_SAMPLE_SNORM: return t2D_snorm.GatherCmpRed(sc, uv.xyz, compare, offsets.xy);
            case DEBUG_SAMPLE_UINT: return t2D_uint.GatherCmpRed(sc, uv.xyz, compare, offsets.xy);
            case DEBUG_SAMPLE_INT: return t2D_int.GatherCmpRed(sc, uv.xyz, compare, offsets.xy);
            default: return t2D_float.GatherCmpRed(sc, uv.xyz, compare, offsets.xy);
          }
        }
        case DEBUG_SAMPLE_TEXCUBE:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM: return tCube_unorm.GatherCmpRed(sc, uv, compare);
            case DEBUG_SAMPLE_SNORM: return tCube_snorm.GatherCmpRed(sc, uv, compare);
            case DEBUG_SAMPLE_UINT: return tCube_uint.GatherCmpRed(sc, uv, compare);
            case DEBUG_SAMPLE_INT: return tCube_int.GatherCmpRed(sc, uv, compare);
            default: return tCube_float.GatherCmpRed(sc, uv, compare);
          }
        }
      }
    }
    else if(debugSampleGatherChannel == 1)
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM:
              return t2D_unorm.GatherCmpGreen(sc, uv.xyz, compare, offsets.xy);
            case DEBUG_SAMPLE_SNORM:
              return t2D_snorm.GatherCmpGreen(sc, uv.xyz, compare, offsets.xy);
            case DEBUG_SAMPLE_UINT: return t2D_uint.GatherCmpGreen(sc, uv.xyz, compare, offsets.xy);
            case DEBUG_SAMPLE_INT: return t2D_int.GatherCmpGreen(sc, uv.xyz, compare, offsets.xy);
            default: return t2D_float.GatherCmpGreen(sc, uv.xyz, compare, offsets.xy);
          }
        }
        case DEBUG_SAMPLE_TEXCUBE:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM: return tCube_unorm.GatherCmpGreen(sc, uv, compare);
            case DEBUG_SAMPLE_SNORM: return tCube_snorm.GatherCmpGreen(sc, uv, compare);
            case DEBUG_SAMPLE_UINT: return tCube_uint.GatherCmpGreen(sc, uv, compare);
            case DEBUG_SAMPLE_INT: return tCube_int.GatherCmpGreen(sc, uv, compare);
            default: return tCube_float.GatherCmpGreen(sc, uv, compare);
          }
        }
      }
    }
    else if(debugSampleGatherChannel == 2)
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM:
              return t2D_unorm.GatherCmpBlue(sc, uv.xyz, compare, offsets.xy);
            case DEBUG_SAMPLE_SNORM:
              return t2D_snorm.GatherCmpBlue(sc, uv.xyz, compare, offsets.xy);
            case DEBUG_SAMPLE_UINT: return t2D_uint.GatherCmpBlue(sc, uv.xyz, compare, offsets.xy);
            case DEBUG_SAMPLE_INT: return t2D_int.GatherCmpBlue(sc, uv.xyz, compare, offsets.xy);
            default: return t2D_float.GatherCmpBlue(sc, uv.xyz, compare, offsets.xy);
          }
        }
        case DEBUG_SAMPLE_TEXCUBE:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM: return tCube_unorm.GatherCmpBlue(sc, uv, compare);
            case DEBUG_SAMPLE_SNORM: return tCube_snorm.GatherCmpBlue(sc, uv, compare);
            case DEBUG_SAMPLE_UINT: return tCube_uint.GatherCmpBlue(sc, uv, compare);
            case DEBUG_SAMPLE_INT: return tCube_int.GatherCmpBlue(sc, uv, compare);
            default: return tCube_float.GatherCmpBlue(sc, uv, compare);
          }
        }
      }
    }
    else
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM:
              return t2D_unorm.GatherCmpAlpha(sc, uv.xyz, compare, offsets.xy);
            case DEBUG_SAMPLE_SNORM:
              return t2D_snorm.GatherCmpAlpha(sc, uv.xyz, compare, offsets.xy);
            case DEBUG_SAMPLE_UINT: return t2D_uint.GatherCmpAlpha(sc, uv.xyz, compare, offsets.xy);
            case DEBUG_SAMPLE_INT: return t2D_int.GatherCmpAlpha(sc, uv.xyz, compare, offsets.xy);
            default: return t2D_float.GatherCmpAlpha(sc, uv.xyz, compare, offsets.xy);
          }
        }
        case DEBUG_SAMPLE_TEXCUBE:
        {
          switch(debugSampleRetType)
          {
            case DEBUG_SAMPLE_UNORM: return tCube_unorm.GatherCmpAlpha(sc, uv, compare);
            case DEBUG_SAMPLE_SNORM: return tCube_snorm.GatherCmpAlpha(sc, uv, compare);
            case DEBUG_SAMPLE_UINT: return tCube_uint.GatherCmpAlpha(sc, uv, compare);
            case DEBUG_SAMPLE_INT: return tCube_int.GatherCmpAlpha(sc, uv, compare);
            default: return tCube_float.GatherCmpAlpha(sc, uv, compare);
          }
        }
      }
    }
  }
  else
  {
    return float4(0, 0, 0, 0);
  }
}

int4 DoIntOpcode(float4 uv)
{
  int4 uvInt = debugSampleUVInt;
  int opcode = debugSampleOperation;
  float4 ddx_ = debugSampleDDX;
  float4 ddy_ = debugSampleDDY;
  int4 offsets = debugSampleOffsets;
  float lod = debugSampleLodCompare;

  if(opcode == DEBUG_SAMPLE_TEX_LOAD || opcode == DEBUG_SAMPLE_TEX_LOAD_MS)
  {
    switch(debugSampleTexDim)
    {
      default:
      case DEBUG_SAMPLE_TEX1D: return t1D_int.Load(uvInt.xyz, offsets.x);
      case DEBUG_SAMPLE_TEX2D: return t2D_int.Load(uvInt, offsets.xy);
      case DEBUG_SAMPLE_TEX3D: return t3D_int.Load(uvInt, offsets.xyz);
      case DEBUG_SAMPLE_TEXMS: return tMS_int.Load(uvInt.xyz, debugSampleSampleIndex, offsets.xy);
    }
  }
  else if(opcode == DEBUG_SAMPLE_TEX_GATHER4 || opcode == DEBUG_SAMPLE_TEX_GATHER4_PO)
  {
    if(debugSampleGatherChannel == 0)
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D: return t2D_int.GatherRed(s, uv.xyz, offsets.xy);
        case DEBUG_SAMPLE_TEXCUBE: return tCube_int.GatherRed(s, uv);
      }
    }
    else if(debugSampleGatherChannel == 1)
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D: return t2D_int.GatherGreen(s, uv.xyz, offsets.xy);
        case DEBUG_SAMPLE_TEXCUBE: return tCube_int.GatherGreen(s, uv);
      }
    }
    else if(debugSampleGatherChannel == 2)
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D: return t2D_int.GatherBlue(s, uv.xyz, offsets.xy);
        case DEBUG_SAMPLE_TEXCUBE: return tCube_int.GatherBlue(s, uv);
      }
    }
    else
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D: return t2D_int.GatherAlpha(s, uv.xyz, offsets.xy);
        case DEBUG_SAMPLE_TEXCUBE: return tCube_int.GatherAlpha(s, uv);
      }
    }
  }
  else
  {
    return int4(0, 0, 0, 0);
  }
}

uint4 DoUIntOpcode(float4 uv)
{
  int4 uvInt = debugSampleUVInt;
  int opcode = debugSampleOperation;
  float4 ddx_ = debugSampleDDX;
  float4 ddy_ = debugSampleDDY;
  int4 offsets = debugSampleOffsets;
  float lod = debugSampleLodCompare;

  if(opcode == DEBUG_SAMPLE_TEX_LOAD || opcode == DEBUG_SAMPLE_TEX_LOAD_MS)
  {
    switch(debugSampleTexDim)
    {
      default:
      case DEBUG_SAMPLE_TEX1D: return t1D_uint.Load(uvInt.xyz, offsets.x);
      case DEBUG_SAMPLE_TEX2D: return t2D_uint.Load(uvInt, offsets.xy);
      case DEBUG_SAMPLE_TEX3D: return t3D_uint.Load(uvInt, offsets.xyz);
      case DEBUG_SAMPLE_TEXMS: return tMS_uint.Load(uvInt.xyz, debugSampleSampleIndex, offsets.xy);
    }
  }
  else if(opcode == DEBUG_SAMPLE_TEX_GATHER4 || opcode == DEBUG_SAMPLE_TEX_GATHER4_PO)
  {
    if(debugSampleGatherChannel == 0)
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D: return t2D_uint.GatherRed(s, uv.xyz, offsets.xy);
        case DEBUG_SAMPLE_TEXCUBE: return tCube_uint.GatherRed(s, uv);
      }
    }
    else if(debugSampleGatherChannel == 1)
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D: return t2D_uint.GatherGreen(s, uv.xyz, offsets.xy);
        case DEBUG_SAMPLE_TEXCUBE: return tCube_uint.GatherGreen(s, uv);
      }
    }
    else if(debugSampleGatherChannel == 2)
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D: return t2D_uint.GatherBlue(s, uv.xyz, offsets.xy);
        case DEBUG_SAMPLE_TEXCUBE: return tCube_uint.GatherBlue(s, uv);
      }
    }
    else
    {
      switch(debugSampleTexDim)
      {
        default:
        case DEBUG_SAMPLE_TEX2D: return t2D_uint.GatherAlpha(s, uv.xyz, offsets.xy);
        case DEBUG_SAMPLE_TEXCUBE: return tCube_uint.GatherAlpha(s, uv);
      }
    }
  }
  else
  {
    return float4(0, 0, 0, 0);
  }
}

void RENDERDOC_DebugSamplePS(in float4 pos : SV_Position, in float4 uv : UVS)
{
  int opcode = debugSampleOperation;

  if(opcode != DEBUG_SAMPLE_TEX_SAMPLE_CMP && opcode != DEBUG_SAMPLE_TEX_LOD)
  {
    uv = debugSampleUV;
  }

  bool forceFloat =
      (opcode == DEBUG_SAMPLE_TEX_SAMPLE_CMP || opcode == DEBUG_SAMPLE_TEX_SAMPLE_CMP_LEVEL_ZERO ||
       opcode == DEBUG_SAMPLE_TEX_GATHER4_CMP || opcode == DEBUG_SAMPLE_TEX_GATHER4_PO_CMP ||
       opcode == DEBUG_SAMPLE_TEX_LOD);

  if(!forceFloat && debugSampleRetType == DEBUG_SAMPLE_INT)
  {
    outBuf[0].outi[0] = DoIntOpcode(uv);
  }
  else if(!forceFloat && debugSampleRetType == DEBUG_SAMPLE_UINT)
  {
    outBuf[0].outu[0] = DoUIntOpcode(uv);
  }
  else if(debugSampleRetType == DEBUG_SAMPLE_UNORM || debugSampleRetType == DEBUG_SAMPLE_SNORM ||
          debugSampleRetType == DEBUG_SAMPLE_FLOAT)
  {
    outBuf[0].outf[0] = DoFloatOpcode(uv);
  }
}
