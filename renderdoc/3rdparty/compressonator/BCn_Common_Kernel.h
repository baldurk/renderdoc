//=====================================================================
// Copyright (c) 2018    Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//=====================================================================
#ifndef _BCn_Common_Kernel_H
#define _BCn_Common_Kernel_H

#include "Common_Def.h"

#ifndef ASPM_GPU
#if defined(WIN32) || defined(_WIN64)
#define ALIGN_16 __declspec(align(16))
#else    // !WIN32 && !_WIN64
#define ALIGN_16
#endif    // !WIN32 && !_WIN64
#else
#define ALIGN_16
#endif

#define DXTC_OFFSET_ALPHA 0
#define DXTC_OFFSET_RGB 2

#define RC 2
#define GC 1
#define BC 0
#define AC 3

/*
Channel Bits
*/
#define RG 5
#define GG 6
#define BG 5

#define RGBA8888_CHANNEL_A 3
#define RGBA8888_CHANNEL_R 2
#define RGBA8888_CHANNEL_G 1
#define RGBA8888_CHANNEL_B 0
#define RGBA8888_OFFSET_A (RGBA8888_CHANNEL_A * 8)
#define RGBA8888_OFFSET_R (RGBA8888_CHANNEL_R * 8)
#define RGBA8888_OFFSET_G (RGBA8888_CHANNEL_G * 8)
#define RGBA8888_OFFSET_B (RGBA8888_CHANNEL_B * 8)

#define MAX_BLOCK 64
#define BLOCK_SIZE MAX_BLOCK

#ifndef MAX_ERROR
#define MAX_ERROR 128000.f
#endif

#define MAX_BLOCK 64
#define MAX_POINTS 16
#define BLOCK_SIZE MAX_BLOCK
#define NUM_CHANNELS 4
#define NUM_ENDPOINTS 2
#define BLOCK_SIZE_4X4 16

#define ConstructColour(r, g, b) (((r) << 11) | ((g) << 5) | (b))

// Find the first approximation of the line
// Assume there is a linear relation
//   Z = a * X_In
//   Z = b * Y_In
// Find a,b to minimize MSE between Z and Z_In
#define EPS (2.f / 255.f) * (2.f / 255.f)
#define EPS2 3.f * (2.f / 255.f) * (2.f / 255.f)

// Grid precision
#define PIX_GRID 8

#define BYTE_MASK 0x00ff

CMP_CONSTANT CGU_UINT8 nByteBitsMask[9] = {0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
CMP_CONSTANT CGU_DWORD dwRndAmount[9] = {0, 0, 0, 0, 1, 1, 2, 2, 3};

#define _INT_GRID (_bFixedRamp && _FracPrc == 0)
#define SCH_STPS 3    // number of search steps to make at each end of interval
static CMP_CONSTANT CGU_FLOAT sMvF[] = {0.f,  -1.f, 1.f,  -2.f, 2.f,  -3.f, 3.f,  -4.f, 4.f,
                                        -5.f, 5.f,  -6.f, 6.f,  -7.f, 7.f,  -8.f, 8.f};

#ifndef GBL_SCH_STEP
#define GBL_SCH_STEP_MXS 0.018f
#define GBL_SCH_EXT_MXS 0.1f
#define LCL_SCH_STEP_MXS 0.6f
#define GBL_SCH_STEP_MXQ 0.0175f
#define GBL_SCH_EXT_MXQ 0.154f
#define LCL_SCH_STEP_MXQ 0.45f

#define GBL_SCH_STEP GBL_SCH_STEP_MXS
#define GBL_SCH_EXT GBL_SCH_EXT_MXS
#define LCL_SCH_STEP LCL_SCH_STEP_MXS
#endif

typedef struct
{
  CGU_UINT32 data;
  CGU_UINT32 index;
} CMP_di;

typedef struct
{
  CGU_FLOAT data;
  CGU_UINT32 index;
} CMP_df;

typedef struct
{
  // user setable
  CGU_FLOAT m_fquality;
  CGU_FLOAT m_fChannelWeights[3];
  CGU_BOOL m_bUseChannelWeighting;
  CGU_BOOL m_bUseAdaptiveWeighting;
  CGU_BOOL m_bUseFloat;
  CGU_BOOL m_b3DRefinement;
  CGU_UINT8 m_nRefinementSteps;
  CGU_UINT8 m_nAlphaThreshold;

  CGU_BOOL m_mapDecodeRGBA;

  // ?? Remove this
  CGU_UINT32 m_src_width;
  CGU_UINT32 m_src_height;
} CMP_BC15Options;

//---------------------------------------- Common Code
//-------------------------------------------------------

static void SetDefaultBC15Options(CMP_BC15Options *BC15Options)
{
  if(BC15Options)
  {
    BC15Options->m_fquality = 1.0f;
    BC15Options->m_bUseChannelWeighting = false;
    BC15Options->m_bUseAdaptiveWeighting = false;
    BC15Options->m_fChannelWeights[0] = 0.3086f;
    BC15Options->m_fChannelWeights[1] = 0.6094f;
    BC15Options->m_fChannelWeights[2] = 0.0820f;
    BC15Options->m_nAlphaThreshold = 128;
    BC15Options->m_bUseFloat = false;
    BC15Options->m_b3DRefinement = false;
    BC15Options->m_nRefinementSteps = 1;
    BC15Options->m_src_width = 4;
    BC15Options->m_src_height = 4;
#ifdef CMP_SET_BC13_DECODER_RGBA
    BC15Options->m_mapDecodeRGBA = true;
#else
    BC15Options->m_mapDecodeRGBA = false;
#endif
  }
}

inline CGU_UINT8 minb(CGU_UINT8 a, CGU_UINT8 b)
{
  return a < b ? a : b;
}
inline CGU_FLOAT minf(CGU_FLOAT a, CGU_FLOAT b)
{
  return a < b ? a : b;
}
inline CGU_FLOAT maxf(CGU_FLOAT a, CGU_FLOAT b)
{
  return a > b ? a : b;
}

#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)
static void CalculateColourWeightings(CGU_UINT8 block[BLOCK_SIZE_4X4X4],
                                      CMP_GLOBAL CMP_BC15Options *BC15options)
{
  CGU_FLOAT fBaseChannelWeights[3] = {0.3086f, 0.6094f, 0.0820f};

  if(!BC15options->m_bUseChannelWeighting)
  {
    BC15options->m_fChannelWeights[0] = 1.0F;
    BC15options->m_fChannelWeights[1] = 1.0F;
    BC15options->m_fChannelWeights[2] = 1.0F;
    return;
  }

  if(BC15options->m_bUseAdaptiveWeighting)
  {
    float medianR = 0.0f, medianG = 0.0f, medianB = 0.0f;

    for(CGU_UINT32 k = 0; k < BLOCK_SIZE_4X4; k++)
    {
      CGU_DWORD R = (block[k] & 0xff0000) >> 16;
      CGU_DWORD G = (block[k] & 0xff00) >> 8;
      CGU_DWORD B = block[k] & 0xff;

      medianR += R;
      medianG += G;
      medianB += B;
    }

    medianR /= BLOCK_SIZE_4X4;
    medianG /= BLOCK_SIZE_4X4;
    medianB /= BLOCK_SIZE_4X4;

    // Now skew the colour weightings based on the gravity center of the block
    float largest = maxf(maxf(medianR, medianG), medianB);

    if(largest > 0)
    {
      medianR /= largest;
      medianG /= largest;
      medianB /= largest;
    }
    else
      medianR = medianG = medianB = 1.0f;

    // Scale weightings back up to 1.0f
    CGU_FLOAT fWeightScale =
        1.0f / (fBaseChannelWeights[0] + fBaseChannelWeights[1] + fBaseChannelWeights[2]);
    BC15options->m_fChannelWeights[0] = fBaseChannelWeights[0] * fWeightScale;
    BC15options->m_fChannelWeights[1] = fBaseChannelWeights[1] * fWeightScale;
    BC15options->m_fChannelWeights[2] = fBaseChannelWeights[2] * fWeightScale;
    BC15options->m_fChannelWeights[0] =
        ((BC15options->m_fChannelWeights[0] * 3 * medianR) + BC15options->m_fChannelWeights[0]) *
        0.25f;
    BC15options->m_fChannelWeights[1] =
        ((BC15options->m_fChannelWeights[1] * 3 * medianG) + BC15options->m_fChannelWeights[1]) *
        0.25f;
    BC15options->m_fChannelWeights[2] =
        ((BC15options->m_fChannelWeights[2] * 3 * medianB) + BC15options->m_fChannelWeights[2]) *
        0.25f;
    fWeightScale = 1.0f / (BC15options->m_fChannelWeights[0] + BC15options->m_fChannelWeights[1] +
                           BC15options->m_fChannelWeights[2]);
    BC15options->m_fChannelWeights[0] *= fWeightScale;
    BC15options->m_fChannelWeights[1] *= fWeightScale;
    BC15options->m_fChannelWeights[2] *= fWeightScale;
  }
  else
  {
    BC15options->m_fChannelWeights[0] = fBaseChannelWeights[0];
    BC15options->m_fChannelWeights[1] = fBaseChannelWeights[1];
    BC15options->m_fChannelWeights[2] = fBaseChannelWeights[2];
  }
}
#endif    // !BC5
#endif    // !BC4

/*------------------------------------------------------------------------------------------------
1 dim error
------------------------------------------------------------------------------------------------*/
#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)

static CGU_FLOAT RampSrchW(CGU_FLOAT _Blck[MAX_BLOCK], CGU_FLOAT _BlckErr[MAX_BLOCK],
                           CGU_FLOAT _Rpt[MAX_BLOCK], CGU_FLOAT _maxerror, CGU_FLOAT _min_ex,
                           CGU_FLOAT _max_ex, int _NmbClrs, int _block)
{
  CGU_FLOAT error = 0;
  CGU_FLOAT step = (_max_ex - _min_ex) / (_block - 1);
  CGU_FLOAT step_h = step * (CGU_FLOAT)0.5;
  CGU_FLOAT rstep = (CGU_FLOAT)1.0f / step;

  for(CGU_INT32 i = 0; i < _NmbClrs; i++)
  {
    CGU_FLOAT v;
    // Work out which value in the block this select
    CGU_FLOAT del;

    if((del = _Blck[i] - _min_ex) <= 0)
      v = _min_ex;
    else if(_Blck[i] - _max_ex >= 0)
      v = _max_ex;
    else
      v = static_cast<CGU_FLOAT>(floor((del + step_h) * rstep)) * step + _min_ex;

    // And accumulate the error
    CGU_FLOAT d = (_Blck[i] - v);
    d *= d;
    CGU_FLOAT err = _Rpt[i] * d + _BlckErr[i];
    error += err;
    if(_maxerror < error)
    {
      error = _maxerror;
      break;
    }
  }
  return error;
}
#endif    // !BC5
#endif    // BC4

/*------------------------------------------------------------------------------------------------
// this is how the end points is going to be rounded in compressed format
------------------------------------------------------------------------------------------------*/
#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)

static void MkRmpOnGrid(CGU_FLOAT _RmpF[NUM_CHANNELS][NUM_ENDPOINTS],
                        CGU_FLOAT _MnMx[NUM_CHANNELS][NUM_ENDPOINTS], CGU_FLOAT _Min, CGU_FLOAT _Max,
                        CGU_UINT8 nRedBits, CGU_UINT8 nGreenBits, CGU_UINT8 nBlueBits)
{
  CGU_FLOAT Fctrs0[3];
  CGU_FLOAT Fctrs1[3];

  Fctrs1[RC] = (CGU_FLOAT)(1 << nRedBits);
  Fctrs1[GC] = (CGU_FLOAT)(1 << nGreenBits);
  Fctrs1[BC] = (CGU_FLOAT)(1 << nBlueBits);
  Fctrs0[RC] = (CGU_FLOAT)(1 << (PIX_GRID - nRedBits));
  Fctrs0[GC] = (CGU_FLOAT)(1 << (PIX_GRID - nGreenBits));
  Fctrs0[BC] = (CGU_FLOAT)(1 << (PIX_GRID - nBlueBits));

  for(CGU_INT32 j = 0; j < 3; j++)
  {
    for(CGU_INT32 k = 0; k < 2; k++)
    {
      _RmpF[j][k] = static_cast<CGU_FLOAT> (floor(_MnMx[j][k]));
      if(_RmpF[j][k] <= _Min)
        _RmpF[j][k] = _Min;
      else
      {
        _RmpF[j][k] +=
            static_cast<CGU_FLOAT> (floor(128.f / Fctrs1[j])) - static_cast<CGU_FLOAT>(floor(_RmpF[j][k] / Fctrs1[j]));
        _RmpF[j][k] = minf(_RmpF[j][k], _Max);
      }

      _RmpF[j][k] = static_cast<CGU_FLOAT> (floor(_RmpF[j][k] / Fctrs0[j])) * Fctrs0[j];
    }
  }
}
#endif    // !BC5
#endif    // BC4

/*------------------------------------------------------------------------------------------------
// this is how the end points is going to be look like when decompressed
------------------------------------------------------------------------------------------------*/
inline void MkWkRmpPts(CGU_BOOL *_bEq, CGU_FLOAT _OutRmpPts[NUM_CHANNELS][NUM_ENDPOINTS],
                       CGU_FLOAT _InpRmpPts[NUM_CHANNELS][NUM_ENDPOINTS], CGU_UINT8 nRedBits,
                       CGU_UINT8 nGreenBits, CGU_UINT8 nBlueBits)
{
  CGU_FLOAT Fctrs[3];
  Fctrs[RC] = (CGU_FLOAT)(1 << nRedBits);
  Fctrs[GC] = (CGU_FLOAT)(1 << nGreenBits);
  Fctrs[BC] = (CGU_FLOAT)(1 << nBlueBits);

  *_bEq = TRUE;
  // find whether input ramp is flat
  for(CGU_INT32 j = 0; j < 3; j++)
    *_bEq &= (_InpRmpPts[j][0] == _InpRmpPts[j][1]);

  // end points on the integer grid
  for(CGU_INT32 j = 0; j < 3; j++)
  {
    for(CGU_INT32 k = 0; k < 2; k++)
    {
      // Apply the lower bit replication to give full dynamic range
      _OutRmpPts[j][k] = _InpRmpPts[j][k] + static_cast<CGU_FLOAT>(floor(_InpRmpPts[j][k] / Fctrs[j]));
      _OutRmpPts[j][k] = maxf((CGU_FLOAT)_OutRmpPts[j][k], 0.f);
      _OutRmpPts[j][k] = minf((CGU_FLOAT)_OutRmpPts[j][k], 255.f);
    }
  }
}

/*------------------------------------------------------------------------------------------------
1 DIM ramp
------------------------------------------------------------------------------------------------*/

inline void BldClrRmp(CGU_FLOAT _Rmp[MAX_POINTS], CGU_FLOAT _InpRmp[NUM_ENDPOINTS],
                      CGU_UINT8 dwNumPoints)
{
  // linear interpolate end points to get the ramp
  _Rmp[0] = _InpRmp[0];
  _Rmp[dwNumPoints - 1] = _InpRmp[1];
  if(dwNumPoints % 2)
    _Rmp[dwNumPoints] = 1000000.f;    // for 3 point ramp; not to select the 4th point as min
  for(CGU_INT32 e = 1; e < dwNumPoints - 1; e++)
    _Rmp[e] = static_cast<CGU_FLOAT> (floor(
        (_Rmp[0] * (dwNumPoints - 1 - e) + _Rmp[dwNumPoints - 1] * e + dwRndAmount[dwNumPoints]) /
        (CGU_FLOAT)(dwNumPoints - 1)));
}

/*------------------------------------------------------------------------------------------------
// build 3D ramp
------------------------------------------------------------------------------------------------*/
inline void BldRmp(CGU_FLOAT _Rmp[NUM_CHANNELS][MAX_POINTS],
                   CGU_FLOAT _InpRmp[NUM_CHANNELS][NUM_ENDPOINTS], CGU_UINT8 dwNumPoints)
{
  for(CGU_INT32 j = 0; j < 3; j++)
    BldClrRmp(_Rmp[j], _InpRmp[j], dwNumPoints);
}

/*------------------------------------------------------------------------------------------------
Compute cumulative error for the current cluster
------------------------------------------------------------------------------------------------*/
#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)

static CGU_FLOAT ClstrErr(CGU_FLOAT _Blk[MAX_BLOCK][NUM_CHANNELS], CGU_FLOAT _Rpt[MAX_BLOCK],
                          CGU_FLOAT _Rmp[NUM_CHANNELS][MAX_POINTS], int _NmbClrs, int _blcktp,
                          CGU_BOOL _ConstRamp, CMP_GLOBAL const CMP_BC15Options *BC15options)
{
  CGU_FLOAT fError = 0.f;
  int rmp_l = (_ConstRamp) ? 1 : _blcktp;

  // For each colour in the original block, find the closest cluster
  // and compute the comulative error
  for(CGU_INT32 i = 0; i < _NmbClrs; i++)
  {
    CGU_FLOAT fShortest = 99999999999.f;

    if(BC15options->m_bUseChannelWeighting)
      for(CGU_INT32 r = 0; r < rmp_l; r++)
      {
        // calculate the distance for each component
        CGU_FLOAT fDistance = (_Blk[i][RC] - _Rmp[RC][r]) * (_Blk[i][RC] - _Rmp[RC][r]) *
                                  BC15options->m_fChannelWeights[0] +
                              (_Blk[i][GC] - _Rmp[GC][r]) * (_Blk[i][GC] - _Rmp[GC][r]) *
                                  BC15options->m_fChannelWeights[1] +
                              (_Blk[i][BC] - _Rmp[BC][r]) * (_Blk[i][BC] - _Rmp[BC][r]) *
                                  BC15options->m_fChannelWeights[2];

        if(fDistance < fShortest)
          fShortest = fDistance;
      }
    else
      for(CGU_INT32 r = 0; r < rmp_l; r++)
      {
        // calculate the distance for each component
        CGU_FLOAT fDistance = (_Blk[i][RC] - _Rmp[RC][r]) * (_Blk[i][RC] - _Rmp[RC][r]) +
                              (_Blk[i][GC] - _Rmp[GC][r]) * (_Blk[i][GC] - _Rmp[GC][r]) +
                              (_Blk[i][BC] - _Rmp[BC][r]) * (_Blk[i][BC] - _Rmp[BC][r]);

        if(fDistance < fShortest)
          fShortest = fDistance;
      }

    // accumulate the error
    fError += fShortest * _Rpt[i];
  }

  return fError;
}
#endif    // !BC5
#endif    // !BC4

#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)
static CGU_FLOAT Refine3D(CGU_FLOAT _OutRmpPnts[NUM_CHANNELS][NUM_ENDPOINTS],
                          CGU_FLOAT _InpRmpPnts[NUM_CHANNELS][NUM_ENDPOINTS],
                          CGU_FLOAT _Blk[MAX_BLOCK][NUM_CHANNELS], CGU_FLOAT _Rpt[MAX_BLOCK],
                          int _NmrClrs, CGU_UINT8 dwNumPoints,
                          CMP_GLOBAL const CMP_BC15Options *BC15options, CGU_UINT8 nRedBits,
                          CGU_UINT8 nGreenBits, CGU_UINT8 nBlueBits, CGU_UINT8 nRefineSteps)
{
  ALIGN_16 CGU_FLOAT Rmp[NUM_CHANNELS][MAX_POINTS];

  CGU_FLOAT Blk[MAX_BLOCK][NUM_CHANNELS];
  for(CGU_INT32 i = 0; i < _NmrClrs; i++)
    for(CGU_INT32 j = 0; j < 3; j++)
      Blk[i][j] = _Blk[i][j];

  CGU_FLOAT fWeightRed = BC15options->m_fChannelWeights[0];
  CGU_FLOAT fWeightGreen = BC15options->m_fChannelWeights[1];
  CGU_FLOAT fWeightBlue = BC15options->m_fChannelWeights[2];

  // here is our grid
  CGU_FLOAT Fctrs[3];
  Fctrs[RC] = (CGU_FLOAT)(1 << (PIX_GRID - nRedBits));
  Fctrs[GC] = (CGU_FLOAT)(1 << (PIX_GRID - nGreenBits));
  Fctrs[BC] = (CGU_FLOAT)(1 << (PIX_GRID - nBlueBits));

  CGU_FLOAT InpRmp0[NUM_CHANNELS][NUM_ENDPOINTS];
  CGU_FLOAT InpRmp[NUM_CHANNELS][NUM_ENDPOINTS];
  for(CGU_INT32 k = 0; k < 2; k++)
    for(CGU_INT32 j = 0; j < 3; j++)
      InpRmp0[j][k] = InpRmp[j][k] = _OutRmpPnts[j][k] = _InpRmpPnts[j][k];

  // make ramp endpoints the way they'll going to be decompressed
  // plus check whether the ramp is flat
  CGU_BOOL Eq;
  CGU_FLOAT WkRmpPts[NUM_CHANNELS][NUM_ENDPOINTS];
  MkWkRmpPts(&Eq, WkRmpPts, InpRmp, nRedBits, nGreenBits, nBlueBits);

  // build ramp for all 3 colors
  BldRmp(Rmp, WkRmpPts, dwNumPoints);

  // clusterize for the current ramp
  CGU_FLOAT bestE = ClstrErr(Blk, _Rpt, Rmp, _NmrClrs, dwNumPoints, Eq, BC15options);
  if(bestE == 0.f || !nRefineSteps)    // if exact, we've done
    return bestE;

  // Jitter endpoints in each direction
  int nRefineStart = 0 - (minb(nRefineSteps, (CGU_UINT8)8));
  int nRefineEnd = minb(nRefineSteps, (CGU_UINT8)8);
  for(CGU_INT32 nJitterG0 = nRefineStart; nJitterG0 <= nRefineEnd; nJitterG0++)
  {
    InpRmp[GC][0] = minf(maxf(InpRmp0[GC][0] + nJitterG0 * Fctrs[GC], 0.f), 255.f);
    for(CGU_INT32 nJitterG1 = nRefineStart; nJitterG1 <= nRefineEnd; nJitterG1++)
    {
      InpRmp[GC][1] = minf(maxf(InpRmp0[GC][1] + nJitterG1 * Fctrs[GC], 0.f), 255.f);
      MkWkRmpPts(&Eq, WkRmpPts, InpRmp, nRedBits, nGreenBits, nBlueBits);
      BldClrRmp(Rmp[GC], WkRmpPts[GC], dwNumPoints);

      CGU_FLOAT RmpErrG[MAX_POINTS][MAX_BLOCK];
      for(CGU_INT32 i = 0; i < _NmrClrs; i++)
      {
        for(CGU_INT32 r = 0; r < dwNumPoints; r++)
        {
          CGU_FLOAT DistG = (Rmp[GC][r] - Blk[i][GC]);
          RmpErrG[r][i] = DistG * DistG * fWeightGreen;
        }
      }

      for(CGU_INT32 nJitterB0 = nRefineStart; nJitterB0 <= nRefineEnd; nJitterB0++)
      {
        InpRmp[BC][0] = minf(maxf(InpRmp0[BC][0] + nJitterB0 * Fctrs[BC], 0.f), 255.f);
        for(CGU_INT32 nJitterB1 = nRefineStart; nJitterB1 <= nRefineEnd; nJitterB1++)
        {
          InpRmp[BC][1] = minf(maxf(InpRmp0[BC][1] + nJitterB1 * Fctrs[BC], 0.f), 255.f);
          MkWkRmpPts(&Eq, WkRmpPts, InpRmp, nRedBits, nGreenBits, nBlueBits);
          BldClrRmp(Rmp[BC], WkRmpPts[BC], dwNumPoints);

          CGU_FLOAT RmpErr[MAX_POINTS][MAX_BLOCK];
          for(CGU_INT32 i = 0; i < _NmrClrs; i++)
          {
            for(CGU_INT32 r = 0; r < dwNumPoints; r++)
            {
              CGU_FLOAT DistB = (Rmp[BC][r] - Blk[i][BC]);
              RmpErr[r][i] = RmpErrG[r][i] + DistB * DistB * fWeightBlue;
            }
          }

          for(CGU_INT32 nJitterR0 = nRefineStart; nJitterR0 <= nRefineEnd; nJitterR0++)
          {
            InpRmp[RC][0] = minf(maxf(InpRmp0[RC][0] + nJitterR0 * Fctrs[RC], 0.f), 255.f);
            for(CGU_INT32 nJitterR1 = nRefineStart; nJitterR1 <= nRefineEnd; nJitterR1++)
            {
              InpRmp[RC][1] = minf(maxf(InpRmp0[RC][1] + nJitterR1 * Fctrs[RC], 0.f), 255.f);
              MkWkRmpPts(&Eq, WkRmpPts, InpRmp, nRedBits, nGreenBits, nBlueBits);
              BldClrRmp(Rmp[RC], WkRmpPts[RC], dwNumPoints);

              // compute cumulative error
              CGU_FLOAT mse = 0.f;
              int rmp_l = (Eq) ? 1 : dwNumPoints;
              for(CGU_INT32 k = 0; k < _NmrClrs; k++)
              {
                CGU_FLOAT MinErr = 10000000.f;
                for(CGU_INT32 r = 0; r < rmp_l; r++)
                {
                  CGU_FLOAT Dist = (Rmp[RC][r] - Blk[k][RC]);
                  CGU_FLOAT Err = RmpErr[r][k] + Dist * Dist * fWeightRed;
                  MinErr = minf(MinErr, Err);
                }
                mse += MinErr * _Rpt[k];
              }

              // save if we achieve better result
              if(mse < bestE)
              {
                bestE = mse;
                for(CGU_INT32 k = 0; k < 2; k++)
                  for(CGU_INT32 j = 0; j < 3; j++)
                    _OutRmpPnts[j][k] = InpRmp[j][k];
              }
            }
          }
        }
      }
    }
  }

  return bestE;
}
#endif    // !BC5
#endif    // BC4

#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)

static CGU_FLOAT Refine(CGU_FLOAT _OutRmpPnts[NUM_CHANNELS][NUM_ENDPOINTS],
                        CGU_FLOAT _InpRmpPnts[NUM_CHANNELS][NUM_ENDPOINTS],
                        CGU_FLOAT _Blk[MAX_BLOCK][NUM_CHANNELS], CGU_FLOAT _Rpt[MAX_BLOCK],
                        int _NmrClrs, CGU_UINT8 dwNumPoints,
                        CMP_GLOBAL const CMP_BC15Options *BC15options, CGU_UINT8 nRedBits,
                        CGU_UINT8 nGreenBits, CGU_UINT8 nBlueBits, CGU_UINT8 nRefineSteps)
{
  ALIGN_16 CGU_FLOAT Rmp[NUM_CHANNELS][MAX_POINTS];

  CGU_FLOAT Blk[MAX_BLOCK][NUM_CHANNELS];
  for(CGU_INT32 i = 0; i < _NmrClrs; i++)
    for(CGU_INT32 j = 0; j < 3; j++)
      Blk[i][j] = _Blk[i][j];

  CGU_FLOAT fWeightRed = BC15options->m_fChannelWeights[0];
  CGU_FLOAT fWeightGreen = BC15options->m_fChannelWeights[1];
  CGU_FLOAT fWeightBlue = BC15options->m_fChannelWeights[2];

  // here is our grid
  CGU_FLOAT Fctrs[3];
  Fctrs[RC] = (CGU_FLOAT)(1 << (PIX_GRID - nRedBits));
  Fctrs[GC] = (CGU_FLOAT)(1 << (PIX_GRID - nGreenBits));
  Fctrs[BC] = (CGU_FLOAT)(1 << (PIX_GRID - nBlueBits));

  CGU_FLOAT InpRmp0[NUM_CHANNELS][NUM_ENDPOINTS];
  CGU_FLOAT InpRmp[NUM_CHANNELS][NUM_ENDPOINTS];
  for(CGU_INT32 k = 0; k < 2; k++)
    for(CGU_INT32 j = 0; j < 3; j++)
      InpRmp0[j][k] = InpRmp[j][k] = _OutRmpPnts[j][k] = _InpRmpPnts[j][k];

  // make ramp endpoints the way they'll going to be decompressed
  // plus check whether the ramp is flat
  CGU_BOOL Eq;
  CGU_FLOAT WkRmpPts[NUM_CHANNELS][NUM_ENDPOINTS];
  MkWkRmpPts(&Eq, WkRmpPts, InpRmp, nRedBits, nGreenBits, nBlueBits);

  // build ramp for all 3 colors
  BldRmp(Rmp, WkRmpPts, dwNumPoints);

  // clusterize for the current ramp
  CGU_FLOAT bestE = ClstrErr(Blk, _Rpt, Rmp, _NmrClrs, dwNumPoints, Eq, BC15options);
  if(bestE == 0.f || !nRefineSteps)    // if exact, we've done
    return bestE;

  // Tweak each component in isolation and get the best values

  // precompute ramp errors for Green and Blue
  CGU_FLOAT RmpErr[MAX_POINTS][MAX_BLOCK];
  for(CGU_INT32 i = 0; i < _NmrClrs; i++)
  {
    for(CGU_INT32 r = 0; r < dwNumPoints; r++)
    {
      CGU_FLOAT DistG = (Rmp[GC][r] - Blk[i][GC]);
      CGU_FLOAT DistB = (Rmp[BC][r] - Blk[i][BC]);
      RmpErr[r][i] = DistG * DistG * fWeightGreen + DistB * DistB * fWeightBlue;
    }
  }

  // First Red
  CGU_FLOAT bstC0 = InpRmp0[RC][0];
  CGU_FLOAT bstC1 = InpRmp0[RC][1];
  int nRefineStart = 0 - (minb(nRefineSteps, (CGU_UINT8)8));
  int nRefineEnd = minb(nRefineSteps, (CGU_UINT8)8);
  for(CGU_INT32 i = nRefineStart; i <= nRefineEnd; i++)
  {
    for(CGU_INT32 j = nRefineStart; j <= nRefineEnd; j++)
    {
      // make a move; both sides of interval.
      InpRmp[RC][0] = minf(maxf(InpRmp0[RC][0] + i * Fctrs[RC], 0.f), 255.f);
      InpRmp[RC][1] = minf(maxf(InpRmp0[RC][1] + j * Fctrs[RC], 0.f), 255.f);

      // make ramp endpoints the way they'll going to be decompressed
      // plus check whether the ramp is flat
      MkWkRmpPts(&Eq, WkRmpPts, InpRmp, nRedBits, nGreenBits, nBlueBits);

      // build ramp only for red
      BldClrRmp(Rmp[RC], WkRmpPts[RC], dwNumPoints);

      // compute cumulative error
      CGU_FLOAT mse = 0.f;
      int rmp_l = (Eq) ? 1 : dwNumPoints;
      for(CGU_INT32 k = 0; k < _NmrClrs; k++)
      {
        CGU_FLOAT MinErr = 10000000.f;
        for(CGU_INT32 r = 0; r < rmp_l; r++)
        {
          CGU_FLOAT Dist = (Rmp[RC][r] - Blk[k][RC]);
          CGU_FLOAT Err = RmpErr[r][k] + Dist * Dist * fWeightRed;
          MinErr = minf(MinErr, Err);
        }
        mse += MinErr * _Rpt[k];
      }

      // save if we achieve better result
      if(mse < bestE)
      {
        bstC0 = InpRmp[RC][0];
        bstC1 = InpRmp[RC][1];
        bestE = mse;
      }
    }
  }

  // our best REDs
  InpRmp[RC][0] = bstC0;
  InpRmp[RC][1] = bstC1;

  // make ramp endpoints the way they'll going to be decompressed
  // plus check whether the ramp is flat
  MkWkRmpPts(&Eq, WkRmpPts, InpRmp, nRedBits, nGreenBits, nBlueBits);

  // build ramp only for green
  BldRmp(Rmp, WkRmpPts, dwNumPoints);

  // precompute ramp errors for Red and Blue
  for(CGU_INT32 i = 0; i < _NmrClrs; i++)
  {
    for(CGU_INT32 r = 0; r < dwNumPoints; r++)
    {
      CGU_FLOAT DistR = (Rmp[RC][r] - Blk[i][RC]);
      CGU_FLOAT DistB = (Rmp[BC][r] - Blk[i][BC]);
      RmpErr[r][i] = DistR * DistR * fWeightRed + DistB * DistB * fWeightBlue;
    }
  }

  // Now green
  bstC0 = InpRmp0[GC][0];
  bstC1 = InpRmp0[GC][1];
  for(CGU_INT32 i = nRefineStart; i <= nRefineEnd; i++)
  {
    for(CGU_INT32 j = nRefineStart; j <= nRefineEnd; j++)
    {
      InpRmp[GC][0] = minf(maxf(InpRmp0[GC][0] + i * Fctrs[GC], 0.f), 255.f);
      InpRmp[GC][1] = minf(maxf(InpRmp0[GC][1] + j * Fctrs[GC], 0.f), 255.f);

      MkWkRmpPts(&Eq, WkRmpPts, InpRmp, nRedBits, nGreenBits, nBlueBits);
      BldClrRmp(Rmp[GC], WkRmpPts[GC], dwNumPoints);

      CGU_FLOAT mse = 0.f;
      int rmp_l = (Eq) ? 1 : dwNumPoints;
      for(CGU_INT32 k = 0; k < _NmrClrs; k++)
      {
        CGU_FLOAT MinErr = 10000000.f;
        for(CGU_INT32 r = 0; r < rmp_l; r++)
        {
          CGU_FLOAT Dist = (Rmp[GC][r] - Blk[k][GC]);
          CGU_FLOAT Err = RmpErr[r][k] + Dist * Dist * fWeightGreen;
          MinErr = minf(MinErr, Err);
        }
        mse += MinErr * _Rpt[k];
      }

      if(mse < bestE)
      {
        bstC0 = InpRmp[GC][0];
        bstC1 = InpRmp[GC][1];
        bestE = mse;
      }
    }
  }

  // our best GREENs
  InpRmp[GC][0] = bstC0;
  InpRmp[GC][1] = bstC1;

  MkWkRmpPts(&Eq, WkRmpPts, InpRmp, nRedBits, nGreenBits, nBlueBits);
  BldRmp(Rmp, WkRmpPts, dwNumPoints);

  // ramp err for Red and Green
  for(CGU_INT32 i = 0; i < _NmrClrs; i++)
  {
    for(CGU_INT32 r = 0; r < dwNumPoints; r++)
    {
      CGU_FLOAT DistR = (Rmp[RC][r] - Blk[i][RC]);
      CGU_FLOAT DistG = (Rmp[GC][r] - Blk[i][GC]);
      RmpErr[r][i] = DistR * DistR * fWeightRed + DistG * DistG * fWeightGreen;
    }
  }

  bstC0 = InpRmp0[BC][0];
  bstC1 = InpRmp0[BC][1];
  // Now blue
  for(CGU_INT32 i = nRefineStart; i <= nRefineEnd; i++)
  {
    for(CGU_INT32 j = nRefineStart; j <= nRefineEnd; j++)
    {
      InpRmp[BC][0] = minf(maxf(InpRmp0[BC][0] + i * Fctrs[BC], 0.f), 255.f);
      InpRmp[BC][1] = minf(maxf(InpRmp0[BC][1] + j * Fctrs[BC], 0.f), 255.f);

      MkWkRmpPts(&Eq, WkRmpPts, InpRmp, nRedBits, nGreenBits, nBlueBits);
      BldClrRmp(Rmp[BC], WkRmpPts[BC], dwNumPoints);

      CGU_FLOAT mse = 0.f;
      int rmp_l = (Eq) ? 1 : dwNumPoints;
      for(CGU_INT32 k = 0; k < _NmrClrs; k++)
      {
        CGU_FLOAT MinErr = 10000000.f;
        for(CGU_INT32 r = 0; r < rmp_l; r++)
        {
          CGU_FLOAT Dist = (Rmp[BC][r] - Blk[k][BC]);
          CGU_FLOAT Err = RmpErr[r][k] + Dist * Dist * fWeightBlue;
          MinErr = minf(MinErr, Err);
        }
        mse += MinErr * _Rpt[k];
      }

      if(mse < bestE)
      {
        bstC0 = InpRmp[BC][0];
        bstC1 = InpRmp[BC][1];
        bestE = mse;
      }
    }
  }

  // our best BLUEs
  InpRmp[BC][0] = bstC0;
  InpRmp[BC][1] = bstC1;

  // return our best choice
  for(CGU_INT32 j = 0; j < 3; j++)
    for(CGU_INT32 k = 0; k < 2; k++)
      _OutRmpPnts[j][k] = InpRmp[j][k];

  return bestE;
}
#endif    // !BC5
#endif    //! BC4

#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)

static CGU_DWORD ConstructColor(CGU_UINT8 R, CGU_UINT8 nRedBits, CGU_UINT8 G, CGU_UINT8 nGreenBits,
                                CGU_UINT8 B, CGU_UINT8 nBlueBits)
{
  return (((R & nByteBitsMask[nRedBits]) << (nGreenBits + nBlueBits - (PIX_GRID - nRedBits))) |
          ((G & nByteBitsMask[nGreenBits]) << (nBlueBits - (PIX_GRID - nGreenBits))) |
          ((B & nByteBitsMask[nBlueBits]) >> ((PIX_GRID - nBlueBits))));
}
#endif    // !BC5
#endif    // !BC4

#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)
// Compute error and find DXTC indexes for the current cluster
static CGU_FLOAT ClstrIntnl(CGU_FLOAT _Blk[MAX_BLOCK][NUM_CHANNELS], CGU_UINT8 *_Indxs,
                            CGU_FLOAT _Rmp[NUM_CHANNELS][MAX_POINTS], int dwBlockSize,
                            CGU_UINT8 dwNumPoints, CGU_BOOL _ConstRamp,
                            CMP_GLOBAL const CMP_BC15Options *BC15options, CGU_BOOL _bUseAlpha)
{
  CGU_FLOAT Err = 0.f;
  CGU_UINT8 rmp_l = (_ConstRamp) ? 1 : dwNumPoints;

  // For each colour in the original block assign it
  // to the closest cluster and compute the cumulative error
  for(CGU_INT32 i = 0; i < dwBlockSize; i++)
  {
    if(_bUseAlpha && *((CGU_DWORD *)&_Blk[i][AC]) == 0)
      _Indxs[i] = dwNumPoints;
    else
    {
      CGU_FLOAT shortest = 99999999999.f;
      CGU_UINT8 shortestIndex = 0;
      if(BC15options)
        for(CGU_UINT8 r = 0; r < rmp_l; r++)
        {
          // calculate the distance for each component
          CGU_FLOAT distance = (_Blk[i][RC] - _Rmp[RC][r]) * (_Blk[i][RC] - _Rmp[RC][r]) *
                                   BC15options->m_fChannelWeights[0] +
                               (_Blk[i][GC] - _Rmp[GC][r]) * (_Blk[i][GC] - _Rmp[GC][r]) *
                                   BC15options->m_fChannelWeights[1] +
                               (_Blk[i][BC] - _Rmp[BC][r]) * (_Blk[i][BC] - _Rmp[BC][r]) *
                                   BC15options->m_fChannelWeights[2];

          if(distance < shortest)
          {
            shortest = distance;
            shortestIndex = r;
          }
        }
      else
        for(CGU_UINT8 r = 0; r < rmp_l; r++)
        {
          // calculate the distance for each component
          CGU_FLOAT distance = (_Blk[i][RC] - _Rmp[RC][r]) * (_Blk[i][RC] - _Rmp[RC][r]) +
                               (_Blk[i][GC] - _Rmp[GC][r]) * (_Blk[i][GC] - _Rmp[GC][r]) +
                               (_Blk[i][BC] - _Rmp[BC][r]) * (_Blk[i][BC] - _Rmp[BC][r]);

          if(distance < shortest)
          {
            shortest = distance;
            shortestIndex = r;
          }
        }

      Err += shortest;

      // We have the index of the best cluster, so assign this in the block
      // Reorder indices to match correct DXTC ordering
      if(shortestIndex == dwNumPoints - 1)
        shortestIndex = 1;
      else if(shortestIndex)
        shortestIndex++;
      _Indxs[i] = shortestIndex;
    }
  }

  return Err;
}
#endif    // !BC5
#endif    // !BC4

/*------------------------------------------------------------------------------------------------
// input ramp is on the coarse grid
------------------------------------------------------------------------------------------------*/
#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)
static CGU_FLOAT ClstrBas(CGU_UINT8 *_Indxs, CGU_FLOAT _Blk[MAX_BLOCK][NUM_CHANNELS],
                          CGU_FLOAT _InpRmp[NUM_CHANNELS][NUM_ENDPOINTS], int dwBlockSize,
                          CGU_UINT8 dwNumPoints, CMP_GLOBAL const CMP_BC15Options *BC15options,
                          CGU_BOOL _bUseAlpha, CGU_UINT8 nRedBits, CGU_UINT8 nGreenBits,
                          CGU_UINT8 nBlueBits)
{
  // make ramp endpoints the way they'll going to be decompressed
  CGU_BOOL Eq = TRUE;
  CGU_FLOAT InpRmp[NUM_CHANNELS][NUM_ENDPOINTS];
  MkWkRmpPts(&Eq, InpRmp, _InpRmp, nRedBits, nGreenBits, nBlueBits);

  // build ramp as it would be built by decompressor
  CGU_FLOAT Rmp[NUM_CHANNELS][MAX_POINTS];
  BldRmp(Rmp, InpRmp, dwNumPoints);

  // clusterize and find a cumulative error
  return ClstrIntnl(_Blk, _Indxs, Rmp, dwBlockSize, dwNumPoints, Eq, BC15options, _bUseAlpha);
}
#endif    // !BC5
#endif    // !BC4

/*------------------------------------------------------------------------------------------------
Clusterization the way it looks from the DXTC decompressor
------------------------------------------------------------------------------------------------*/
#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)
static CGU_FLOAT Clstr(CGU_UINT32 block_32[MAX_BLOCK], CGU_UINT32 dwBlockSize,
                       CGU_UINT8 nEndpoints[3][NUM_ENDPOINTS], CGU_UINT8 *pcIndices,
                       CGU_UINT8 dwNumPoints, CMP_GLOBAL const CMP_BC15Options *BC15options,
                       CGU_BOOL _bUseAlpha, CGU_UINT8 _nAlphaThreshold, CGU_UINT8 nRedBits,
                       CGU_UINT8 nGreenBits, CGU_UINT8 nBlueBits)
{
  CGU_INT32 c0 = ConstructColor(nEndpoints[RC][0], nRedBits, nEndpoints[GC][0], nGreenBits,
                                nEndpoints[BC][0], nBlueBits);
  CGU_INT32 c1 = ConstructColor(nEndpoints[RC][1], nRedBits, nEndpoints[GC][1], nGreenBits,
                                nEndpoints[BC][1], nBlueBits);
  CGU_INT32 nEndpointIndex0 = 0;
  CGU_INT32 nEndpointIndex1 = 1;
  if((!(dwNumPoints & 0x1) && c0 <= c1) || ((dwNumPoints & 0x1) && c0 > c1))
  {
    nEndpointIndex0 = 1;
    nEndpointIndex1 = 0;
  }

  CGU_FLOAT InpRmp[NUM_CHANNELS][NUM_ENDPOINTS];
  InpRmp[RC][0] = (CGU_FLOAT)nEndpoints[RC][nEndpointIndex0];
  InpRmp[RC][1] = (CGU_FLOAT)nEndpoints[RC][nEndpointIndex1];
  InpRmp[GC][0] = (CGU_FLOAT)nEndpoints[GC][nEndpointIndex0];
  InpRmp[GC][1] = (CGU_FLOAT)nEndpoints[GC][nEndpointIndex1];
  InpRmp[BC][0] = (CGU_FLOAT)nEndpoints[BC][nEndpointIndex0];
  InpRmp[BC][1] = (CGU_FLOAT)nEndpoints[BC][nEndpointIndex1];

  CGU_UINT32 dwAlphaThreshold = _nAlphaThreshold << 24;
  CGU_FLOAT Blk[MAX_BLOCK][NUM_CHANNELS];
  for(CGU_UINT32 i = 0; i < dwBlockSize; i++)
  {
    Blk[i][RC] = (CGU_FLOAT)((block_32[i] & 0xff0000) >> 16);
    Blk[i][GC] = (CGU_FLOAT)((block_32[i] & 0xff00) >> 8);
    Blk[i][BC] = (CGU_FLOAT)(block_32[i] & 0xff);
    if(_bUseAlpha)
      Blk[i][AC] = ((block_32[i] & 0xff000000) >= dwAlphaThreshold) ? 1.f : 0.f;
  }

  return ClstrBas(pcIndices, Blk, InpRmp, dwBlockSize, dwNumPoints, BC15options, _bUseAlpha,
                  nRedBits, nGreenBits, nBlueBits);
}
#endif    // !BC5
#endif    // !BC4

//----------------------------------------------------
// This function decompresses a DXT colour block
// The block is decompressed to 8 bits per channel
// Result buffer is RGBA format
//----------------------------------------------------
#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)
#ifndef ASPM_GPU
static void DecompressDXTRGB_Internal(CGU_UINT8 rgbBlock[BLOCK_SIZE_4X4X4],
                                      const CGU_UINT32 compressedBlock[2],
                                      const CMP_BC15Options *BC15options)
{
  CGU_BOOL bDXT1 = TRUE;
  CGU_UINT32 n0 = compressedBlock[0] & 0xffff;
  CGU_UINT32 n1 = compressedBlock[0] >> 16;
  CGU_UINT32 r0;
  CGU_UINT32 g0;
  CGU_UINT32 b0;
  CGU_UINT32 r1;
  CGU_UINT32 g1;
  CGU_UINT32 b1;

  r0 = ((n0 & 0xf800) >> 8);
  g0 = ((n0 & 0x07e0) >> 3);
  b0 = ((n0 & 0x001f) << 3);

  r1 = ((n1 & 0xf800) >> 8);
  g1 = ((n1 & 0x07e0) >> 3);
  b1 = ((n1 & 0x001f) << 3);

  // Apply the lower bit replication to give full dynamic range
  r0 += (r0 >> 5);
  r1 += (r1 >> 5);
  g0 += (g0 >> 6);
  g1 += (g1 >> 6);
  b0 += (b0 >> 5);
  b1 += (b1 >> 5);

  if(!BC15options->m_mapDecodeRGBA)
  {
    //--------------------------------------------------------------
    // Channel mapping output as BGRA
    //--------------------------------------------------------------
    CGU_UINT32 c0 = 0xff000000 | (r0 << 16) | (g0 << 8) | b0;
    CGU_UINT32 c1 = 0xff000000 | (r1 << 16) | (g1 << 8) | b1;

    if(!bDXT1 || n0 > n1)
    {
      CGU_UINT32 c2 = 0xff000000 | (((2 * r0 + r1 + 1) / 3) << 16) |
                      (((2 * g0 + g1 + 1) / 3) << 8) | (((2 * b0 + b1 + 1) / 3));
      CGU_UINT32 c3 = 0xff000000 | (((2 * r1 + r0 + 1) / 3) << 16) |
                      (((2 * g1 + g0 + 1) / 3) << 8) | (((2 * b1 + b0 + 1) / 3));

      for(int i = 0; i < 16; i++)
      {
        int index = (compressedBlock[1] >> (2 * i)) & 3;

        switch(index)
        {
          case 0: ((CGU_UINT32 *)rgbBlock)[i] = c0; break;
          case 1: ((CGU_UINT32 *)rgbBlock)[i] = c1; break;
          case 2: ((CGU_UINT32 *)rgbBlock)[i] = c2; break;
          case 3: ((CGU_UINT32 *)rgbBlock)[i] = c3; break;
        }
      }
    }
    else
    {
      // Transparent decode
      CGU_UINT32 c2 =
          0xff000000 | (((r0 + r1) / 2) << 16) | (((g0 + g1) / 2) << 8) | (((b0 + b1) / 2));

      for(int i = 0; i < 16; i++)
      {
        int index = (compressedBlock[1] >> (2 * i)) & 3;

        switch(index)
        {
          case 0: ((CGU_UINT32 *)rgbBlock)[i] = c0; break;
          case 1: ((CGU_UINT32 *)rgbBlock)[i] = c1; break;
          case 2: ((CGU_UINT32 *)rgbBlock)[i] = c2; break;
          case 3: ((CGU_UINT32 *)rgbBlock)[i] = 0x00000000; break;
        }
      }
    }
  }
  else
  {    // MAP_BC15_TO_ABGR
    //--------------------------------------------------------------
    // Channel mapping output as ARGB
    //--------------------------------------------------------------

    CGU_UINT32 c0 = 0xff000000 | (b0 << 16) | (g0 << 8) | r0;
    CGU_UINT32 c1 = 0xff000000 | (b1 << 16) | (g1 << 8) | r1;

    if(!bDXT1 || n0 > n1)
    {
      CGU_UINT32 c2 = 0xff000000 | (((2 * b0 + b1 + 1) / 3) << 16) |
                      (((2 * g0 + g1 + 1) / 3) << 8) | (((2 * r0 + r1 + 1) / 3));
      CGU_UINT32 c3 = 0xff000000 | (((2 * b1 + b0 + 1) / 3) << 16) |
                      (((2 * g1 + g0 + 1) / 3) << 8) | (((2 * r1 + r0 + 1) / 3));

      for(int i = 0; i < 16; i++)
      {
        int index = (compressedBlock[1] >> (2 * i)) & 3;
        switch(index)
        {
          case 0: ((CMP_GLOBAL CGU_UINT32 *)rgbBlock)[i] = c0; break;
          case 1: ((CMP_GLOBAL CGU_UINT32 *)rgbBlock)[i] = c1; break;
          case 2: ((CMP_GLOBAL CGU_UINT32 *)rgbBlock)[i] = c2; break;
          case 3: ((CMP_GLOBAL CGU_UINT32 *)rgbBlock)[i] = c3; break;
        }
      }
    }
    else
    {
      // Transparent decode
      CGU_UINT32 c2 =
          0xff000000 | (((b0 + b1) / 2) << 16) | (((g0 + g1) / 2) << 8) | (((r0 + r1) / 2));

      for(int i = 0; i < 16; i++)
      {
        int index = (compressedBlock[1] >> (2 * i)) & 3;
        switch(index)
        {
          case 0: ((CMP_GLOBAL CGU_UINT32 *)rgbBlock)[i] = c0; break;
          case 1: ((CMP_GLOBAL CGU_UINT32 *)rgbBlock)[i] = c1; break;
          case 2: ((CMP_GLOBAL CGU_UINT32 *)rgbBlock)[i] = c2; break;
          case 3: ((CMP_GLOBAL CGU_UINT32 *)rgbBlock)[i] = 0x00000000; break;
        }
      }
    }
  }    // MAP_ABGR
}
#endif    // !ASPM_GPU
#endif    // !BC5
#endif    // !BC4

#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)
static int QSortIntCmp(const void *Elem1, const void *Elem2)
{
  return (*(CGU_INT32 *)Elem1 - *(CGU_INT32 *)Elem2);
}
#endif    // !BC5
#endif    // !BC4

// Find the first approximation of the line
// Assume there is a linear relation
//   Z = a * X_In
//   Z = b * Y_In
// Find a,b to minimize MSE between Z and Z_In
#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)

static void FindAxis(CGU_FLOAT _outBlk[MAX_BLOCK][NUM_CHANNELS],
                     CGU_FLOAT fLineDirection[NUM_CHANNELS], CGU_FLOAT fBlockCenter[NUM_CHANNELS],
                     CGU_BOOL *_pbSmall, CGU_FLOAT _inpBlk[MAX_BLOCK][NUM_CHANNELS],
                     CGU_FLOAT _inpRpt[MAX_BLOCK], int nDimensions, int nNumColors)
{
  CGU_FLOAT Crrl[NUM_CHANNELS];
  CGU_FLOAT RGB2[NUM_CHANNELS];

  fLineDirection[0] = fLineDirection[1] = fLineDirection[2] = RGB2[0] = RGB2[1] = RGB2[2] =
      Crrl[0] = Crrl[1] = Crrl[2] = fBlockCenter[0] = fBlockCenter[1] = fBlockCenter[2] = 0.f;

  // sum position of all points
  CGU_FLOAT fNumPoints = 0.f;
  for(CGU_INT32 i = 0; i < nNumColors; i++)
  {
    fBlockCenter[0] += _inpBlk[i][0] * _inpRpt[i];
    fBlockCenter[1] += _inpBlk[i][1] * _inpRpt[i];
    fBlockCenter[2] += _inpBlk[i][2] * _inpRpt[i];
    fNumPoints += _inpRpt[i];
  }

  // and then average to calculate center coordinate of block
  fBlockCenter[0] /= fNumPoints;
  fBlockCenter[1] /= fNumPoints;
  fBlockCenter[2] /= fNumPoints;

  for(CGU_INT32 i = 0; i < nNumColors; i++)
  {
    // calculate output block as offsets around block center
    _outBlk[i][0] = _inpBlk[i][0] - fBlockCenter[0];
    _outBlk[i][1] = _inpBlk[i][1] - fBlockCenter[1];
    _outBlk[i][2] = _inpBlk[i][2] - fBlockCenter[2];

    // compute correlation matrix
    // RGB2 = sum of ((distance from point from center) squared)
    // Crrl = ???????. Seems to be be some calculation based on distance from
    // point center in two dimensions
    for(CGU_INT32 j = 0; j < nDimensions; j++)
    {
      RGB2[j] += _outBlk[i][j] * _outBlk[i][j] * _inpRpt[i];
      Crrl[j] += _outBlk[i][j] * _outBlk[i][(j + 1) % 3] * _inpRpt[i];
    }
  }

  // if set's diameter is small
  int i0 = 0, i1 = 1;
  CGU_FLOAT mxRGB2 = 0.f;
  int k = 0, j = 0;
  CGU_FLOAT fEPS = fNumPoints * EPS;
  for(k = 0, j = 0; j < 3; j++)
  {
    if(RGB2[j] >= fEPS)
      k++;
    else
      RGB2[j] = 0.f;

    if(mxRGB2 < RGB2[j])
    {
      mxRGB2 = RGB2[j];
      i0 = j;
    }
  }

  CGU_FLOAT fEPS2 = fNumPoints * EPS2;
  *_pbSmall = TRUE;
  for(j = 0; j < 3; j++)
    *_pbSmall &= (RGB2[j] < fEPS2);

  if(*_pbSmall)    // all are very small to avoid division on the small
                   // determinant
    return;

  if(k == 1)    // really only 1 dimension
    fLineDirection[i0] = 1.;
  else if(k == 2)    // really only 2 dimensions
  {
    i1 = (RGB2[(i0 + 1) % 3] > 0.f) ? (i0 + 1) % 3 : (i0 + 2) % 3;
    CGU_FLOAT Crl = (i1 == (i0 + 1) % 3) ? Crrl[i0] : Crrl[(i0 + 2) % 3];
    fLineDirection[i1] = Crl / RGB2[i0];
    fLineDirection[i0] = 1.;
  }
  else
  {
    CGU_FLOAT maxDet = 100000.f;
    CGU_FLOAT Cs[3];
    // select max det for precision
    for(j = 0; j < nDimensions; j++)
    {
      CGU_FLOAT Det = RGB2[j] * RGB2[(j + 1) % 3] - Crrl[j] * Crrl[j];
      Cs[j] = static_cast<CGU_FLOAT> (fabs(Crrl[j]))/ static_cast<CGU_FLOAT>(sqrt(RGB2[j] * RGB2[(j + 1) % 3]));
      if(maxDet < Det)
      {
        maxDet = Det;
        i0 = j;
      }
    }

    // inverse correl matrix
    //  --      --       --      --
    //  |  A   B |       |  C  -B |
    //  |  B   C |  =>   | -B   A |
    //  --      --       --     --
    CGU_FLOAT mtrx1[2][2];
    CGU_FLOAT vc1[2];
    CGU_FLOAT vc[2];
    vc1[0] = Crrl[(i0 + 2) % 3];
    vc1[1] = Crrl[(i0 + 1) % 3];
    // C
    mtrx1[0][0] = RGB2[(i0 + 1) % 3];
    // A
    mtrx1[1][1] = RGB2[i0];
    // -B
    mtrx1[1][0] = mtrx1[0][1] = -Crrl[i0];
    // find a solution
    vc[0] = mtrx1[0][0] * vc1[0] + mtrx1[0][1] * vc1[1];
    vc[1] = mtrx1[1][0] * vc1[0] + mtrx1[1][1] * vc1[1];
    // normalize
    vc[0] /= maxDet;
    vc[1] /= maxDet;
    // find a line direction vector
    fLineDirection[i0] = 1.;
    fLineDirection[(i0 + 1) % 3] = 1.;
    fLineDirection[(i0 + 2) % 3] = vc[0] + vc[1];
  }

  // normalize direction vector
  CGU_FLOAT Len = fLineDirection[0] * fLineDirection[0] + fLineDirection[1] * fLineDirection[1] +
                  fLineDirection[2] * fLineDirection[2];
  Len = static_cast<CGU_FLOAT> (sqrt(Len));

  for(j = 0; j < 3; j++)
    fLineDirection[j] = (Len > 0.f) ? fLineDirection[j] / Len : 0.f;
}
#endif    // !BC5
#endif    // !BC4

#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)
static void CompressRGBBlockX(CGU_FLOAT _RsltRmpPnts[NUM_CHANNELS][NUM_ENDPOINTS],
                              CGU_FLOAT _BlkIn[MAX_BLOCK][NUM_CHANNELS], CGU_FLOAT _Rpt[MAX_BLOCK],
                              int _UniqClrs, CGU_UINT8 dwNumPoints, CGU_BOOL b3DRefinement,
                              CGU_UINT8 nRefinementSteps,
                              CMP_GLOBAL const CMP_BC15Options *BC15options, CGU_UINT8 nRedBits,
                              CGU_UINT8 nGreenBits, CGU_UINT8 nBlueBits)
{
  ALIGN_16 CGU_FLOAT Prj0[MAX_BLOCK];
  ALIGN_16 CGU_FLOAT Prj[MAX_BLOCK];
  ALIGN_16 CGU_FLOAT PrjErr[MAX_BLOCK];
  ALIGN_16 CGU_FLOAT LineDir[NUM_CHANNELS];
  ALIGN_16 CGU_FLOAT RmpIndxs[MAX_BLOCK];

  CGU_FLOAT LineDirG[NUM_CHANNELS];
  CGU_FLOAT PosG[NUM_ENDPOINTS];
  CGU_FLOAT Blk[MAX_BLOCK][NUM_CHANNELS];
  CGU_FLOAT BlkSh[MAX_BLOCK][NUM_CHANNELS];
  CGU_FLOAT LineDir0[NUM_CHANNELS];
  CGU_FLOAT Mdl[NUM_CHANNELS];

  CGU_FLOAT rsltC[NUM_CHANNELS][NUM_ENDPOINTS];
  int i, j, k;

  // down to [0., 1.]
  for(i = 0; i < _UniqClrs; i++)
    for(j = 0; j < 3; j++)
      Blk[i][j] = _BlkIn[i][j] / 255.f;

  CGU_BOOL isDONE = FALSE;

  // as usual if not more then 2 different colors, we've done
  if(_UniqClrs <= 2)
  {
    for(j = 0; j < 3; j++)
    {
      rsltC[j][0] = _BlkIn[0][j];
      rsltC[j][1] = _BlkIn[_UniqClrs - 1][j];
    }
    isDONE = TRUE;
  }

  if(!isDONE)
  {
    //    This is our first attempt to find an axis we will go along.
    //    The cumulation is done to find a line minimizing the MSE from the
    //    input 3D points.
    CGU_BOOL bSmall = TRUE;
    FindAxis(BlkSh, LineDir0, Mdl, &bSmall, Blk, _Rpt, 3, _UniqClrs);

    //    While trying to find the axis we found that the diameter of the input
    //    set is quite small. Do not bother.
    if(bSmall)
    {
      for(j = 0; j < 3; j++)
      {
        rsltC[j][0] = _BlkIn[0][j];
        rsltC[j][1] = _BlkIn[_UniqClrs - 1][j];
      }
      isDONE = TRUE;
    }
  }

  // GCC is being an awful being when it comes to goto-jumps.
  // So please bear with this.
  if(!isDONE)
  {
    CGU_FLOAT ErrG = 10000000.f;
    CGU_FLOAT PrjBnd[NUM_ENDPOINTS];
    ALIGN_16 CGU_FLOAT PreMRep[MAX_BLOCK];
    for(j = 0; j < 3; j++)
      LineDir[j] = LineDir0[j];

    //    Here is the main loop.
    //    1. Project input set on the axis in consideration.
    //    2. Run 1 dimensional search (see scalar case) to find an (sub) optimal
    //    pair of end points.
    //    3. Compute the vector of indexes (or clusters) for the current
    //    approximate ramp.
    //    4. Present our color channels as 3 16DIM vectors.
    //    5. Find closest approximation of each of 16DIM color vector with the
    //    projection of the 16DIM index vector.
    //    6. Plug the projections as a new directional vector for the axis.
    //    7. Goto 1.

    //    D - is 16 dim "index" vector (or 16 DIM vector of indexes - {0, 1/3,
    //    2/3, 0, ...,}, but shifted and normalized). Ci - is a 16 dim vector of
    //    color i. for each Ci find a scalar Ai such that (Ai * D - Ci) (Ai * D
    //    - Ci) -> min , i.e distance between vector AiD and C is min. You can
    //    think of D as a unit interval(vector) "clusterizer", and Ai is a scale
    //    you need to apply to the clusterizer to approximate the Ci vector
    //    instead of the unit vector.

    //    Solution is

    //    Ai = (D . Ci) / (D . D); . - is a dot product.

    //    in 3 dim space Ai(s) represent a line direction, along which
    //    we again try to find (sub)optimal quantizer.

    //    That's what our for(;;) loop is about.
    for(;;)
    {
      //  1. Project input set on the axis in consideration.
      // From Foley & Van Dam: Closest point of approach of a line (P + v) to a
      // point (R) is
      //                            P + ((R-P).v) / (v.v))v
      // The distance along v is therefore (R-P).v / (v.v)
      // (v.v) is 1 if v is a unit vector.
      //
      PrjBnd[0] = 1000.;
      PrjBnd[1] = -1000.;
      for(i = 0; i < MAX_BLOCK; i++)
        Prj0[i] = Prj[i] = PrjErr[i] = PreMRep[i] = 0.f;

      for(i = 0; i < _UniqClrs; i++)
      {
        Prj0[i] = Prj[i] =
            BlkSh[i][0] * LineDir[0] + BlkSh[i][1] * LineDir[1] + BlkSh[i][2] * LineDir[2];

        PrjErr[i] = (BlkSh[i][0] - LineDir[0] * Prj[i]) * (BlkSh[i][0] - LineDir[0] * Prj[i]) +
                    (BlkSh[i][1] - LineDir[1] * Prj[i]) * (BlkSh[i][1] - LineDir[1] * Prj[i]) +
                    (BlkSh[i][2] - LineDir[2] * Prj[i]) * (BlkSh[i][2] - LineDir[2] * Prj[i]);

        PrjBnd[0] = minf(PrjBnd[0], Prj[i]);
        PrjBnd[1] = maxf(PrjBnd[1], Prj[i]);
      }

      //  2. Run 1 dimensional search (see scalar case) to find an (sub) optimal
      //  pair of end points.

      // min and max of the search interval
      CGU_FLOAT Scl[NUM_ENDPOINTS];
      Scl[0] = PrjBnd[0] - (PrjBnd[1] - PrjBnd[0]) * 0.125f;
      ;
      Scl[1] = PrjBnd[1] + (PrjBnd[1] - PrjBnd[0]) * 0.125f;
      ;

      // compute scaling factor to scale down the search interval to [0.,1]
      const CGU_FLOAT Scl2 = (Scl[1] - Scl[0]) * (Scl[1] - Scl[0]);
      const CGU_FLOAT overScl = 1.f / (Scl[1] - Scl[0]);

      for(i = 0; i < _UniqClrs; i++)
      {
        // scale them
        Prj[i] = (Prj[i] - Scl[0]) * overScl;
        // premultiply the scale squire to plug into error computation later
        PreMRep[i] = _Rpt[i] * Scl2;
      }

      // scale first approximation of end points
      for(k = 0; k < 2; k++)
        PrjBnd[k] = (PrjBnd[k] - Scl[0]) * overScl;

      CGU_FLOAT Err = MAX_ERROR;

      // search step
      CGU_FLOAT stp = 0.025f;

      // low Start/End; high Start/End
      const CGU_FLOAT lS = (PrjBnd[0] - 2.f * stp > 0.f) ? PrjBnd[0] - 2.f * stp : 0.f;
      const CGU_FLOAT hE = (PrjBnd[1] + 2.f * stp < 1.f) ? PrjBnd[1] + 2.f * stp : 1.f;

      // find the best endpoints
      CGU_FLOAT Pos[NUM_ENDPOINTS];
      CGU_FLOAT lP, hP;
      int l, h;
      for(l = 0, lP = lS; l < 8; l++, lP += stp)
      {
        for(h = 0, hP = hE; h < 8; h++, hP -= stp)
        {
          CGU_FLOAT err = Err;
          // compute an error for the current pair of end points.
          err = RampSrchW(Prj, PrjErr, PreMRep, err, lP, hP, _UniqClrs, dwNumPoints);

          if(err < Err)
          {
            // save better result
            Err = err;
            Pos[0] = lP;
            Pos[1] = hP;
          }
        }
      }

      // inverse the scaling
      for(k = 0; k < 2; k++)
        Pos[k] = Pos[k] * (Scl[1] - Scl[0]) + Scl[0];

      // did we find somthing better from the previous run?
      if(Err + 0.001 < ErrG)
      {
        // yes, remember it
        ErrG = Err;
        LineDirG[0] = LineDir[0];
        LineDirG[1] = LineDir[1];
        LineDirG[2] = LineDir[2];
        PosG[0] = Pos[0];
        PosG[1] = Pos[1];
        //  3. Compute the vector of indexes (or clusters) for the current
        //  approximate ramp.
        // indexes
        const CGU_FLOAT step = (Pos[1] - Pos[0]) / (CGU_FLOAT)(dwNumPoints - 1);
        const CGU_FLOAT step_h = step * (CGU_FLOAT)0.5;
        const CGU_FLOAT rstep = (CGU_FLOAT)1.0f / step;
        const CGU_FLOAT overBlkTp = 1.f / (CGU_FLOAT)(dwNumPoints - 1);

        // here the index vector is computed,
        // shifted and normalized
        CGU_FLOAT indxAvrg = (CGU_FLOAT)(dwNumPoints - 1) / 2.f;

        for(i = 0; i < _UniqClrs; i++)
        {
          CGU_FLOAT del;
          // CGU_UINT32 n = (CGU_UINT32)((b - _min_ex + (step*0.5f)) * rstep);
          if((del = Prj0[i] - Pos[0]) <= 0)
            RmpIndxs[i] = 0.f;
          else if(Prj0[i] - Pos[1] >= 0)
            RmpIndxs[i] = (CGU_FLOAT)(dwNumPoints - 1);
          else
            RmpIndxs[i] = static_cast<CGU_FLOAT> (floor((del + step_h) * rstep));
          // shift and normalization
          RmpIndxs[i] = (RmpIndxs[i] - indxAvrg) * overBlkTp;
        }

        //  4. Present our color channels as 3 16DIM vectors.
        //  5. Find closest aproximation of each of 16DIM color vector with the
        //  pojection of the 16DIM index vector.
        CGU_FLOAT Crs[3], Len, Len2;
        for(i = 0, Crs[0] = Crs[1] = Crs[2] = Len = 0.f; i < _UniqClrs; i++)
        {
          const CGU_FLOAT PreMlt = RmpIndxs[i] * _Rpt[i];
          Len += RmpIndxs[i] * PreMlt;
          for(j = 0; j < 3; j++)
            Crs[j] += BlkSh[i][j] * PreMlt;
        }

        LineDir[0] = LineDir[1] = LineDir[2] = 0.f;
        if(Len > 0.f)
        {
          LineDir[0] = Crs[0] / Len;
          LineDir[1] = Crs[1] / Len;
          LineDir[2] = Crs[2] / Len;

          //  6. Plug the projections as a new directional vector for the axis.
          //  7. Goto 1.
          Len2 = LineDir[0] * LineDir[0] + LineDir[1] * LineDir[1] + LineDir[2] * LineDir[2];
          Len2 = static_cast<CGU_FLOAT> (sqrt(Len2));

          LineDir[0] /= Len2;
          LineDir[1] /= Len2;
          LineDir[2] /= Len2;
        }
      }
      else    // We was not able to find anything better.  Drop dead.
        break;
    }

    // inverse transform to find end-points of 3-color ramp
    for(k = 0; k < 2; k++)
      for(j = 0; j < 3; j++)
        rsltC[j][k] = (PosG[k] * LineDirG[j] + Mdl[j]) * 255.f;
  }

  // We've dealt with (almost) unrestricted full precision realm.
  // Now back to the dirty digital world.

  // round the end points to make them look like compressed ones
  CGU_FLOAT inpRmpEndPts[NUM_CHANNELS][NUM_ENDPOINTS];
  MkRmpOnGrid(inpRmpEndPts, rsltC, 0.f, 255.f, nRedBits, nGreenBits, nBlueBits);

  //    This not a small procedure squeezes and stretches the ramp along each
  //    axis (R,G,B) separately while other 2 are fixed. It does it only over
  //    coarse grid - 565 that is. It tries to squeeze more precision for the
  //    real world ramp.
  if(b3DRefinement)
    Refine3D(_RsltRmpPnts, inpRmpEndPts, _BlkIn, _Rpt, _UniqClrs, dwNumPoints, BC15options,
             nRedBits, nGreenBits, nBlueBits, nRefinementSteps);
  else
    Refine(_RsltRmpPnts, inpRmpEndPts, _BlkIn, _Rpt, _UniqClrs, dwNumPoints, BC15options, nRedBits,
           nGreenBits, nBlueBits, nRefinementSteps);
}
#endif    // !BC5
#endif    // !BC4

#ifdef ASPM_GPU
void cmp_memsetfBCn(CGU_FLOAT ptr[], CGU_FLOAT value, CGU_UINT32 size)
{
  for(CGU_UINT32 i = 0; i < size; i++)
  {
    ptr[i] = value;
  }
}
#endif

#if 0
void memset(CGU_UINT8 *srcdata, CGU_UINT8 value, CGU_INT size)
{
  for(CGU_INT i = 0; i < size; i++)
    *srcdata++ = value;
}

void memcpy(CGU_UINT8 *srcdata, CGU_UINT8 *dstdata, CGU_INT size)
{
  for(CGU_INT i = 0; i < size; i++)
  {
    *srcdata = *dstdata;
    srcdata++;
    dstdata++;
  }
}

void cmp_memsetBC1(CGU_UINT8 ptr[], CGU_UINT8 value, CGU_UINT32 size)
{
  for(CGU_UINT32 i = 0; i < size; i++)
  {
    ptr[i] = value;
  }
}
#endif

#ifdef ASPM_GPU
static void sortData_UINT32(CGU_UINT32 data_ordered[BLOCK_SIZE], CGU_UINT32 projection[BLOCK_SIZE],
                            CGU_UINT32 numEntries    // max 64
                            )
{
  CMP_di what[BLOCK_SIZE];

  for(CGU_UINT32 i = 0; i < numEntries; i++)
  {
    what[i].index = i;
    what[i].data = projection[i];
  }

  CGU_UINT32 tmp_index;
  CGU_UINT32 tmp_data;

  for(CGU_UINT32 i = 1; i < numEntries; i++)
  {
    for(CGU_UINT32 j = i; j > 0; j--)
    {
      if(what[j - 1].data > what[j].data)
      {
        tmp_index = what[j].index;
        tmp_data = what[j].data;
        what[j].index = what[j - 1].index;
        what[j].data = what[j - 1].data;
        what[j - 1].index = tmp_index;
        what[j - 1].data = tmp_data;
      }
    }
  }

  for(CGU_UINT32 i = 0; i < numEntries; i++)
    data_ordered[i] = what[i].data;
};

static void sortData_FLOAT(CGU_FLOAT data_ordered[BLOCK_SIZE], CGU_FLOAT projection[BLOCK_SIZE],
                           CGU_UINT32 numEntries    // max 64
                           )
{
  CMP_df what[BLOCK_SIZE];

  for(CGU_UINT32 i = 0; i < numEntries; i++)
  {
    what[i].index = i;
    what[i].data = projection[i];
  }

  CGU_UINT32 tmp_index;
  CGU_FLOAT tmp_data;

  for(CGU_UINT32 i = 1; i < numEntries; i++)
  {
    for(CGU_UINT32 j = i; j > 0; j--)
    {
      if(what[j - 1].data > what[j].data)
      {
        tmp_index = what[j].index;
        tmp_data = what[j].data;
        what[j].index = what[j - 1].index;
        what[j].data = what[j - 1].data;
        what[j - 1].index = tmp_index;
        what[j - 1].data = tmp_data;
      }
    }
  }

  for(CGU_UINT32 i = 0; i < numEntries; i++)
    data_ordered[i] = what[i].data;
};
#endif

#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)
static CGU_FLOAT CompRGBBlock(CGU_UINT32 *block_32, CGU_UINT32 dwBlockSize, CGU_UINT8 nRedBits,
                              CGU_UINT8 nGreenBits, CGU_UINT8 nBlueBits,
                              CGU_UINT8 nEndpoints[3][NUM_ENDPOINTS], CGU_UINT8 *pcIndices,
                              CGU_UINT8 dwNumPoints, CGU_BOOL b3DRefinement,
                              CGU_UINT8 nRefinementSteps,
                              CMP_GLOBAL const CMP_BC15Options *BC15options, CGU_BOOL _bUseAlpha,
                              CGU_UINT8 _nAlphaThreshold)
{
  ALIGN_16 CGU_FLOAT Rpt[BLOCK_SIZE];
  ALIGN_16 CGU_FLOAT BlkIn[BLOCK_SIZE][NUM_CHANNELS];
#ifndef ASPM_GPU
  memset(Rpt, 0, sizeof(Rpt));
  memset(BlkIn, 0, sizeof(BlkIn));
#else
  cmp_memsetfBCn(&Rpt[0], 0, BLOCK_SIZE);
  cmp_memsetfBCn(&BlkIn[0][0], 0, BLOCK_SIZE * NUM_CHANNELS);
#endif

  CGU_UINT32 dwAlphaThreshold = _nAlphaThreshold << 24;
  CGU_UINT32 dwColors = 0;
  CGU_UINT32 dwBlk[BLOCK_SIZE];
  for(CGU_UINT32 i = 0; i < dwBlockSize; i++)
    if(!_bUseAlpha || (block_32[i] & 0xff000000) >= dwAlphaThreshold)
      dwBlk[dwColors++] = block_32[i] | 0xff000000;

  // Do we have any colors ?
  if(dwColors)
  {
    CGU_BOOL bHasAlpha = (dwColors != dwBlockSize);
    if(bHasAlpha && _bUseAlpha && !(dwNumPoints & 0x1))
      return CMP_FLOAT_MAX;

// CGU_UINT32 dwBlk_sorted[BLOCK_SIZE];
// Here we are computing an unique number of colors.
// For each unique value we compute the number of it appearences.
#ifndef ASPM_GPU
    qsort((void *)dwBlk, (size_t)dwColors, sizeof(CGU_UINT32), QSortIntCmp);
#else
    sortData_UINT32(dwBlk, dwBlk, dwColors);
#endif

    CGU_UINT32 new_p;
    CGU_UINT32 dwBlkU[BLOCK_SIZE];
    CGU_UINT32 dwUniqueColors = 0;
    new_p = dwBlkU[0] = dwBlk[0];
    Rpt[dwUniqueColors] = 1.f;
    for(CGU_UINT32 i = 1; i < dwColors; i++)
    {
      if(new_p != dwBlk[i])
      {
        dwUniqueColors++;
        new_p = dwBlkU[dwUniqueColors] = dwBlk[i];
        Rpt[dwUniqueColors] = 1.f;
      }
      else
        Rpt[dwUniqueColors] += 1.f;
    }
    dwUniqueColors++;

    // switch to float
    for(CGU_UINT32 i = 0; i < dwUniqueColors; i++)
    {
      BlkIn[i][RC] = (CGU_FLOAT)((dwBlkU[i] >> 16) & 0xff);    // R
      BlkIn[i][GC] = (CGU_FLOAT)((dwBlkU[i] >> 8) & 0xff);     // G
      BlkIn[i][BC] = (CGU_FLOAT)((dwBlkU[i] >> 0) & 0xff);     // B
      BlkIn[i][AC] = 255.f;                                    // A
    }

    CGU_FLOAT rsltC[NUM_CHANNELS][NUM_ENDPOINTS];
    CompressRGBBlockX(rsltC, BlkIn, Rpt, dwUniqueColors, dwNumPoints, b3DRefinement,
                      nRefinementSteps, BC15options, nRedBits, nGreenBits, nBlueBits);

    // return to integer realm
    for(CGU_INT32 i = 0; i < 3; i++)
      for(CGU_INT32 j = 0; j < 2; j++)
        nEndpoints[i][j] = (CGU_UINT8)rsltC[i][j];

    return Clstr(block_32, dwBlockSize, nEndpoints, pcIndices, dwNumPoints, BC15options, _bUseAlpha,
                 _nAlphaThreshold, nRedBits, nGreenBits, nBlueBits);
  }
  else
  {
    // All colors transparent
    nEndpoints[0][0] = nEndpoints[1][0] = nEndpoints[2][0] = 0;
    nEndpoints[0][1] = nEndpoints[1][1] = nEndpoints[2][1] = 0xff;
#ifndef ASPM_GPU
    memset(pcIndices, 0xff, dwBlockSize);
#else
    cmp_memsetBC1(pcIndices, 0xff, dwBlockSize);
#endif
    return 0.0;
  }
}
#endif    // !BC5
#endif    // !BC4

#if !defined(BC4_ENCODE_KERNEL_H)
#if !defined(BC5_ENCODE_KERNEL_H)
static void CompressRGBBlock(const CGU_UINT8 rgbBlock[64], CMP_GLOBAL CGU_UINT32 compressedBlock[2],
                             CMP_GLOBAL const CMP_BC15Options *BC15options, CGU_BOOL bDXT1,
                             CGU_BOOL bDXT1UseAlpha, CGU_UINT8 nDXT1AlphaThreshold)
{
  CGU_BOOL m_b3DRefinement = FALSE;
  CGU_UINT8 m_nRefinementSteps = 1;

  /*
  ARGB Channel indexes
  */
  if(bDXT1)
  {
    CGU_UINT8 nEndpoints[2][3][2];
    CGU_UINT8 nIndices[2][16];

    CGU_FLOAT fError3 = CompRGBBlock(
        (CGU_UINT32 *)rgbBlock, BLOCK_SIZE_4X4, RG, GG, BG, nEndpoints[0], nIndices[0], 3,
        m_b3DRefinement, m_nRefinementSteps, BC15options, bDXT1UseAlpha, nDXT1AlphaThreshold);
    CGU_FLOAT fError4 =
        (fError3 == 0.0)
            ? CMP_FLOAT_MAX
            : CompRGBBlock((CGU_UINT32 *)rgbBlock, BLOCK_SIZE_4X4, RG, GG, BG, nEndpoints[1],
                           nIndices[1], 4, m_b3DRefinement, m_nRefinementSteps, BC15options,
                           bDXT1UseAlpha, nDXT1AlphaThreshold);

    CGU_INT32 nMethod = (fError3 <= fError4) ? 0 : 1;
    CGU_INT32 c0 = ConstructColour((nEndpoints[nMethod][RC][0] >> (8 - RG)),
                                   (nEndpoints[nMethod][GC][0] >> (8 - GG)),
                                   (nEndpoints[nMethod][BC][0] >> (8 - BG)));
    CGU_INT32 c1 = ConstructColour((nEndpoints[nMethod][RC][1] >> (8 - RG)),
                                   (nEndpoints[nMethod][GC][1] >> (8 - GG)),
                                   (nEndpoints[nMethod][BC][1] >> (8 - BG)));
    CGU_BOOL m1 = (nMethod == 1 && c0 <= c1);
    CGU_BOOL m2 = (nMethod == 0 && c0 > c1);
    if(m1 || m2)
      compressedBlock[0] = c1 | (c0 << 16);
    else
      compressedBlock[0] = c0 | (c1 << 16);

    compressedBlock[1] = 0;
    for(CGU_INT32 i = 0; i < 16; i++)
      compressedBlock[1] |= (nIndices[nMethod][i] << (2 * i));
  }
  else
  {
    CGU_UINT8 nEndpoints[3][2];
    CGU_UINT8 nIndices[BLOCK_SIZE_4X4];

    CompRGBBlock((CGU_UINT32 *)rgbBlock, BLOCK_SIZE_4X4, RG, GG, BG, nEndpoints, nIndices, 4,
                 m_b3DRefinement, m_nRefinementSteps, BC15options, bDXT1UseAlpha,
                 nDXT1AlphaThreshold);

    CGU_INT32 c0 = ConstructColour((nEndpoints[RC][0] >> (8 - RG)), (nEndpoints[GC][0] >> (8 - GG)),
                                   (nEndpoints[BC][0] >> (8 - BG)));
    CGU_INT32 c1 = ConstructColour((nEndpoints[RC][1] >> (8 - RG)), (nEndpoints[GC][1] >> (8 - GG)),
                                   (nEndpoints[BC][1] >> (8 - BG)));
    if(c0 <= c1)
      compressedBlock[0] = c1 | (c0 << 16);
    else
      compressedBlock[0] = c0 | (c1 << 16);

    compressedBlock[1] = 0;
    for(CGU_INT32 i = 0; i < 16; i++)
      compressedBlock[1] |= (nIndices[i] << (2 * i));
  }
}
#endif    // BC5

#endif    // BC4

#if !defined(BC1_ENCODE_KERNEL_H)
#if !defined(BC2_ENCODE_KERNEL_H)
static CGU_FLOAT RmpSrch1(CGU_FLOAT _Blk[MAX_BLOCK], CGU_FLOAT _Rpt[MAX_BLOCK], CGU_FLOAT _maxerror,
                          CGU_FLOAT _min_ex, CGU_FLOAT _max_ex, CGU_INT _NmbrClrs,
                          CGU_UINT8 nNumPoints)
{
  CGU_FLOAT error = 0;
  const CGU_FLOAT step = (_max_ex - _min_ex) / (CGU_FLOAT)(nNumPoints - 1);
  const CGU_FLOAT step_h = step * 0.5f;
  const CGU_FLOAT rstep = 1.0f / step;

  for(CGU_INT i = 0; i < _NmbrClrs; i++)
  {
    CGU_FLOAT v;
    // Work out which value in the block this select
    CGU_FLOAT del;

    if((del = _Blk[i] - _min_ex) <= 0)
      v = _min_ex;
    else if(_Blk[i] - _max_ex >= 0)
      v = _max_ex;
    else
      v = (static_cast<CGU_FLOAT> (floor((del + step_h) * rstep) * step)) + _min_ex;

    // And accumulate the error
    CGU_FLOAT del2 = (_Blk[i] - v);
    error += del2 * del2 * _Rpt[i];

    // if we've already lost to the previous step bail out
    if(_maxerror < error)
    {
      error = _maxerror;
      break;
    }
  }
  return error;
}
#endif    // !BC2

#if !defined(BC2_ENCODE_KERNEL_H)
static CGU_FLOAT BlockRefine1(CGU_FLOAT _Blk[MAX_BLOCK], CGU_FLOAT _Rpt[MAX_BLOCK],
                              CGU_FLOAT _MaxError, CGU_FLOAT *_min_ex, CGU_FLOAT *_max_ex,
                              CGU_FLOAT _m_step, CGU_FLOAT _min_bnd, CGU_FLOAT _max_bnd,
                              CGU_INT _NmbrClrs, CGU_UINT8 dwNumPoints)
{
  // Start out assuming our endpoints are the min and max values we've
  // determined

  // Attempt a (simple) progressive refinement step to reduce noise in the
  // output image by trying to find a better overall match for the endpoints.

  CGU_FLOAT maxerror = _MaxError;
  CGU_FLOAT min_ex = *_min_ex;
  CGU_FLOAT max_ex = *_max_ex;

  int mode, bestmode;
  do
  {
    CGU_FLOAT cr_min0 = min_ex;
    CGU_FLOAT cr_max0 = max_ex;
    for(bestmode = -1, mode = 0; mode < SCH_STPS * SCH_STPS; mode++)
    {
      // check each move (see sStep for direction)
      CGU_FLOAT cr_min = min_ex + _m_step * sMvF[mode / SCH_STPS];
      CGU_FLOAT cr_max = max_ex + _m_step * sMvF[mode % SCH_STPS];

      cr_min = maxf(cr_min, _min_bnd);
      cr_max = minf(cr_max, _max_bnd);

      CGU_FLOAT error;
      error = RmpSrch1(_Blk, _Rpt, maxerror, cr_min, cr_max, _NmbrClrs, dwNumPoints);

      if(error < maxerror)
      {
        maxerror = error;
        bestmode = mode;
        cr_min0 = cr_min;
        cr_max0 = cr_max;
      }
    }

    if(bestmode != -1)
    {
      // make move (see sStep for direction)
      min_ex = cr_min0;
      max_ex = cr_max0;
    }
  } while(bestmode != -1);

  *_min_ex = min_ex;
  *_max_ex = max_ex;

  return maxerror;
}
#endif    //! BC2

#if !defined(BC2_ENCODE_KERNEL_H)
static int QSortFCmp(const void *Elem1, const void *Elem2)
{
  int ret = 0;

  if(*(CGU_FLOAT *)Elem1 - *(CGU_FLOAT *)Elem2 < 0.)
    ret = -1;
  else if(*(CGU_FLOAT *)Elem1 - *(CGU_FLOAT *)Elem2 > 0.)
    ret = 1;
  return ret;
}
#endif    // !BC2

#if !defined(BC2_ENCODE_KERNEL_H)
static CGU_FLOAT CompBlock1(CGU_FLOAT _RmpPnts[NUM_ENDPOINTS], CGU_FLOAT _Blk[MAX_BLOCK],
                            CGU_INT _Nmbr, CGU_UINT8 dwNumPoints, CGU_BOOL bFixedRampPoints,
                            CGU_INT _IntPrc, CGU_INT _FracPrc, CGU_BOOL _bFixedRamp)
{
  CGU_FLOAT fMaxError = 0.f;

  CGU_FLOAT Ramp[NUM_ENDPOINTS];

  CGU_FLOAT IntFctr = (CGU_FLOAT)(1 << _IntPrc);
  //    CGU_FLOAT FracFctr = (CGU_FLOAT)(1 << _FracPrc);

  ALIGN_16 CGU_FLOAT afUniqueValues[MAX_BLOCK];
  ALIGN_16 CGU_FLOAT afValueRepeats[MAX_BLOCK];
  for(int i = 0; i < MAX_BLOCK; i++)
    afUniqueValues[i] = afValueRepeats[i] = 0.f;

  // For each unique value we compute the number of it appearances.
  CGU_FLOAT fBlk[MAX_BLOCK];
#ifdef ASPM_GPU
  for(CGU_INT i = 0; i < _Nmbr; i++)
  {
    fBlk[i] = _Blk[i];
  }
#else
  memcpy(fBlk, _Blk, _Nmbr * sizeof(CGU_FLOAT));
#endif

// sort the input
#ifndef ASPM_GPU
  qsort((void *)fBlk, (size_t)_Nmbr, sizeof(CGU_FLOAT), QSortFCmp);
#else
  sortData_FLOAT(fBlk, fBlk, _Nmbr);
#endif

  CGU_FLOAT new_p = -2.;

  int N0s = 0, N1s = 0;
  CGU_UINT32 dwUniqueValues = 0;
  afUniqueValues[0] = 0.f;

  bool requiresCalculation = true;

  if(bFixedRampPoints)
  {
    for(CGU_INT i = 0; i < _Nmbr; i++)
    {
      if(new_p != fBlk[i])
      {
        new_p = fBlk[i];
        if(new_p <= 1.5 / 255.)
          N0s++;
        else if(new_p >= 253.5 / 255.)
          N1s++;
        else
        {
          afUniqueValues[dwUniqueValues] = fBlk[i];
          afValueRepeats[dwUniqueValues] = 1.f;
          dwUniqueValues++;
        }
      }
      else
      {
        if(dwUniqueValues > 0)
        {
          if(afUniqueValues[dwUniqueValues - 1] == new_p)
            afValueRepeats[dwUniqueValues - 1] += 1.f;
        }
      }
    }

    // if number of unique colors is less or eq 2 we've done either, but we know
    // that we may have 0s and/or 1s as well. To avoid for the ramp to be
    // considered flat we invented couple entries on the way.
    if(dwUniqueValues <= 2)
    {
      if(dwUniqueValues == 2)    // if 2, take them
      {
        Ramp[0] = static_cast<CGU_FLOAT> (floor(afUniqueValues[0] * (IntFctr - 1) + 0.5f));
        Ramp[1] = static_cast<CGU_FLOAT> (floor(afUniqueValues[1] * (IntFctr - 1) + 0.5f));
      }
      else if(dwUniqueValues == 1)    // if 1, add another one
      {
        Ramp[0] = static_cast<CGU_FLOAT> (floor(afUniqueValues[0] * (IntFctr - 1) + 0.5f));
        Ramp[1] = Ramp[0] + 1.f;
      }
      else    // if 0, invent them
      {
        Ramp[0] = 128.f;
        Ramp[1] = Ramp[0] + 1.f;
      }

      fMaxError = 0.f;
      requiresCalculation = false;
    }
  }
  else
  {
    for(CGU_INT i = 0; i < _Nmbr; i++)
    {
      if(new_p != fBlk[i])
      {
        afUniqueValues[dwUniqueValues] = new_p = fBlk[i];
        afValueRepeats[dwUniqueValues] = 1.f;
        dwUniqueValues++;
      }
      else
        afValueRepeats[dwUniqueValues - 1] += 1.f;
    }

    // if number of unique colors is less or eq 2, we've done
    if(dwUniqueValues <= 2)
    {
      Ramp[0] = static_cast<CGU_FLOAT> (floor(afUniqueValues[0] * (IntFctr - 1) + 0.5f));
      if(dwUniqueValues == 1)
        Ramp[1] = Ramp[0] + 1.f;
      else
        Ramp[1] = static_cast<CGU_FLOAT> (floor(afUniqueValues[1] * (IntFctr - 1) + 0.5f));
      fMaxError = 0.f;
      requiresCalculation = false;
    }
  }

  if(requiresCalculation)
  {
    CGU_FLOAT min_ex = afUniqueValues[0];
    CGU_FLOAT max_ex = afUniqueValues[dwUniqueValues - 1];
    CGU_FLOAT min_bnd = 0, max_bnd = 1.;
    CGU_FLOAT min_r = min_ex, max_r = max_ex;
    CGU_FLOAT gbl_l = 0, gbl_r = 0;
    CGU_FLOAT cntr = (min_r + max_r) / 2;

    CGU_FLOAT gbl_err = MAX_ERROR;
    // Trying to avoid unnecessary calculations. Heuristics: after some analisis
    // it appears that in integer case, if the input interval not more then 48
    // we won't get much better

    bool wantsSearch = !(_INT_GRID && max_ex - min_ex <= 48.f / IntFctr);

    if(wantsSearch)
    {
      // Search.
      // 1. take the vicinities of both low and high bound of the input
      // interval.
      // 2. setup some search step
      // 3. find the new low and high bound which provides an (sub) optimal
      // (infinite precision) clusterization.
      CGU_FLOAT gbl_llb = (min_bnd > min_r - GBL_SCH_EXT) ? min_bnd : min_r - GBL_SCH_EXT;
      CGU_FLOAT gbl_rrb = (max_bnd < max_r + GBL_SCH_EXT) ? max_bnd : max_r + GBL_SCH_EXT;
      CGU_FLOAT gbl_lrb = (cntr < min_r + GBL_SCH_EXT) ? cntr : min_r + GBL_SCH_EXT;
      CGU_FLOAT gbl_rlb = (cntr > max_r - GBL_SCH_EXT) ? cntr : max_r - GBL_SCH_EXT;
      for(CGU_FLOAT step_l = gbl_llb; step_l < gbl_lrb; step_l += GBL_SCH_STEP)
      {
        for(CGU_FLOAT step_r = gbl_rrb; gbl_rlb <= step_r; step_r -= GBL_SCH_STEP)
        {
          CGU_FLOAT sch_err;
          // an sse version is avaiable
          sch_err = RmpSrch1(afUniqueValues, afValueRepeats, gbl_err, step_l, step_r,
                             dwUniqueValues, dwNumPoints);
          if(sch_err < gbl_err)
          {
            gbl_err = sch_err;
            gbl_l = step_l;
            gbl_r = step_r;
          }
        }
      }

      min_r = gbl_l;
      max_r = gbl_r;
    }

    // This is a refinement call. The function tries to make several small
    // stretches or squashes to minimize quantization error.
    CGU_FLOAT m_step = LCL_SCH_STEP / IntFctr;
    fMaxError = BlockRefine1(afUniqueValues, afValueRepeats, gbl_err, &min_r, &max_r, m_step,
                             min_bnd, max_bnd, dwUniqueValues, dwNumPoints);

    min_ex = min_r;
    max_ex = max_r;

    max_ex *= (IntFctr - 1);
    min_ex *= (IntFctr - 1);
    /*
    this one is tricky. for the float or high fractional precision ramp it tries
    to avoid for the ramp to be collapsed into one integer number after
    rounding. Notice the condition. There is a difference between max_ex and
    min_ex but after rounding they may collapse into the same integer.

    So we try to run the same refinement procedure but with starting position on
    the integer grid and step equal 1.
    */
    if(!_INT_GRID && max_ex - min_ex > 0. && floor(min_ex + 0.5f) == floor(max_ex + 0.5f))
    {
      m_step = 1.;
      gbl_err = MAX_ERROR;
      for(CGU_UINT32 i = 0; i < dwUniqueValues; i++)
        afUniqueValues[i] *= (IntFctr - 1);

      max_ex = min_ex = static_cast<CGU_FLOAT> (floor(min_ex + 0.5f));

      gbl_err = BlockRefine1(afUniqueValues, afValueRepeats, gbl_err, &min_ex, &max_ex, m_step, 0.f,
                             255.f, dwUniqueValues, dwNumPoints);

      fMaxError = gbl_err;
    }
    Ramp[1] = static_cast<CGU_FLOAT> (floor(max_ex + 0.5f));
    Ramp[0] = static_cast<CGU_FLOAT> (floor(min_ex + 0.5f));
  }

  // Ensure that the two endpoints are not the same
  // This is legal but serves no need & can break some optimizations in the
  // compressor
  if(Ramp[0] == Ramp[1])
  {
    if(Ramp[1] < 255.f)
      Ramp[1]++;
    else
      Ramp[1]--;
  }
  _RmpPnts[0] = Ramp[0];
  _RmpPnts[1] = Ramp[1];

  return fMaxError;
}
#endif    // !BC2

#if !defined(BC2_ENCODE_KERNEL_H)
static void BldRmp1(CGU_FLOAT _Rmp[MAX_POINTS], CGU_FLOAT _InpRmp[NUM_ENDPOINTS], int nNumPoints)
{
  // for 3 point ramp; not to select the 4th point in min
  for(int e = nNumPoints; e < MAX_POINTS; e++)
    _Rmp[e] = 100000.f;

  _Rmp[0] = _InpRmp[0];
  _Rmp[1] = _InpRmp[1];
  for(int e = 1; e < nNumPoints - 1; e++)
    _Rmp[e + 1] = (_Rmp[0] * (nNumPoints - 1 - e) + _Rmp[1] * e) / (CGU_FLOAT)(nNumPoints - 1);
}
#endif    //! BC2

#if !defined(BC2_ENCODE_KERNEL_H)
static void GetRmp1(CGU_FLOAT _rampDat[MAX_POINTS], CGU_FLOAT _ramp[NUM_ENDPOINTS], int nNumPoints,
                    CGU_BOOL bFixedRampPoints, CGU_INT _intPrec, CGU_INT _fracPrec,
                    CGU_BOOL _bFixedRamp)
{
  if(_ramp[0] == _ramp[1])
    return;

  CGU_BOOL r0 = _ramp[0] <= _ramp[1];
  CGU_BOOL r1 = _ramp[0] > _ramp[1];
  if((!bFixedRampPoints && r0) || (bFixedRampPoints && r1))
  {
    CGU_FLOAT t = _ramp[0];
    _ramp[0] = _ramp[1];
    _ramp[1] = t;
  }

  _rampDat[0] = _ramp[0];
  _rampDat[1] = _ramp[1];

  CGU_FLOAT IntFctr = (CGU_FLOAT)(1 << _intPrec);
  CGU_FLOAT FracFctr = (CGU_FLOAT)(1 << _fracPrec);

  CGU_FLOAT ramp[NUM_ENDPOINTS];
  ramp[0] = _ramp[0] * FracFctr;
  ramp[1] = _ramp[1] * FracFctr;

  BldRmp1(_rampDat, ramp, nNumPoints);
  if(bFixedRampPoints)
  {
    _rampDat[nNumPoints] = 0.;
    _rampDat[nNumPoints + 1] = FracFctr * IntFctr - 1.f;
  }

  if(_bFixedRamp)
  {
    for(CGU_INT i = 0; i < nNumPoints; i++)
    {
      _rampDat[i] = static_cast<CGU_FLOAT> (floor(_rampDat[i] + 0.5f));
      _rampDat[i] /= FracFctr;
    }
  }
}
#endif

#if !defined(BC2_ENCODE_KERNEL_H)
static CGU_FLOAT Clstr1(CGU_UINT8 *pcIndices, CGU_FLOAT _blockIn[MAX_BLOCK],
                        CGU_FLOAT _ramp[NUM_ENDPOINTS], CGU_INT _NmbrClrs, CGU_INT nNumPoints,
                        CGU_BOOL bFixedRampPoints, CGU_INT _intPrec, CGU_INT _fracPrec,
                        CGU_BOOL _bFixedRamp)
{
  CGU_FLOAT Err = 0.f;
  CGU_FLOAT alpha[MAX_POINTS];

  for(CGU_INT i = 0; i < _NmbrClrs; i++)
    pcIndices[i] = 0;

  if(_ramp[0] == _ramp[1])
    return Err;

  if(!_bFixedRamp)
  {
    _intPrec = 8;
    _fracPrec = 0;
  }

  GetRmp1(alpha, _ramp, nNumPoints, bFixedRampPoints, _intPrec, _fracPrec, _bFixedRamp);

  if(bFixedRampPoints)
    nNumPoints += 2;

  const CGU_FLOAT OverIntFctr = 1.f / ((CGU_FLOAT)(1 << _intPrec) - 1.f);
  for(int i = 0; i < nNumPoints; i++)
    alpha[i] *= OverIntFctr;

  // For each colour in the original block, calculate its weighted
  // distance from each point in the original and assign it
  // to the closest cluster
  for(int i = 0; i < _NmbrClrs; i++)
  {
    CGU_FLOAT shortest = 10000000.f;

    // Get the original alpha
    CGU_FLOAT acur = _blockIn[i];

    for(CGU_UINT8 j = 0; j < nNumPoints; j++)
    {
      CGU_FLOAT adist = (acur - alpha[j]);
      adist *= adist;

      if(adist < shortest)
      {
        shortest = adist;
        pcIndices[i] = j;
      }
    }

    Err += shortest;
  }

  return Err;
}
#endif    // !BC2

#if !defined(BC2_ENCODE_KERNEL_H)
static CGU_FLOAT CompBlock1XF(CGU_FLOAT *_Blk, CGU_UINT32 dwBlockSize, CGU_UINT8 nEndpoints[2],
                              CGU_UINT8 *pcIndices, CGU_UINT8 dwNumPoints, CGU_BOOL bFixedRampPoints,
                              CGU_INT _intPrec, CGU_INT _fracPrec, CGU_BOOL _bFixedRamp)
{
  // just to make them initialized
  if(!_bFixedRamp)
  {
    _intPrec = 8;
    _fracPrec = 0;
  }

  // this one makes the bulk of the work
  CGU_FLOAT Ramp[NUM_ENDPOINTS];
  CompBlock1(Ramp, _Blk, dwBlockSize, dwNumPoints, bFixedRampPoints, _intPrec, _fracPrec,
             _bFixedRamp);

  // final clusterization applied
  CGU_FLOAT fError = Clstr1(pcIndices, _Blk, Ramp, dwBlockSize, dwNumPoints, bFixedRampPoints,
                            _intPrec, _fracPrec, _bFixedRamp);
  nEndpoints[0] = (CGU_UINT8)Ramp[0];
  nEndpoints[1] = (CGU_UINT8)Ramp[1];

  return fError;
}
#endif    //! BC2
#endif    //! BC1

#if !defined(BC1_ENCODE_KERNEL_H)
#if !defined(BC2_ENCODE_KERNEL_H)
static CGU_FLOAT CompBlock1X(const CGU_UINT8 *_Blk, CGU_UINT32 dwBlockSize, CGU_UINT8 nEndpoints[2],
                             CGU_UINT8 *pcIndices, CGU_UINT8 dwNumPoints, CGU_BOOL bFixedRampPoints,
                             CGU_INT _intPrec, CGU_INT _fracPrec, CGU_BOOL _bFixedRamp)
{
  // convert the input and call the float equivalent.
  CGU_FLOAT fBlk[MAX_BLOCK];
  for(CGU_UINT32 i = 0; i < dwBlockSize; i++)
    fBlk[i] = (CGU_FLOAT)_Blk[i] / 255.f;

  return CompBlock1XF(fBlk, dwBlockSize, nEndpoints, pcIndices, dwNumPoints, bFixedRampPoints,
                      _intPrec, _fracPrec, _bFixedRamp);
}
#endif

#if !defined(BC2_ENCODE_KERNEL_H)
static void EncodeAlphaBlock(CMP_GLOBAL CGU_UINT32 compressedBlock[2], CGU_UINT8 nEndpoints[2],
                             CGU_UINT8 nIndices[BLOCK_SIZE_4X4])
{
  compressedBlock[0] = ((CGU_UINT32)nEndpoints[0]) | (((CGU_UINT32)nEndpoints[1]) << 8);
  compressedBlock[1] = 0;

  for(CGU_INT i = 0; i < BLOCK_SIZE_4X4; i++)
  {
    if(i < 5)
      compressedBlock[0] |= (nIndices[i] & 0x7) << (16 + (i * 3));
    else if(i > 5)
      compressedBlock[1] |= (nIndices[i] & 0x7) << (2 + (i - 6) * 3);
    else
    {
      compressedBlock[0] |= (nIndices[i] & 0x1) << 31;
      compressedBlock[1] |= (nIndices[i] & 0x6) >> 1;
    }
  }
}
#endif

#endif

#if !defined(BC1_ENCODE_KERNEL_H)
#if !defined(BC2_ENCODE_KERNEL_H)
static CGU_INT32 CompressAlphaBlock(const CGU_UINT8 alphaBlock[BLOCK_SIZE_4X4],
                                    CMP_GLOBAL CGU_UINT32 compressedBlock[2])
{
  CGU_UINT8 nEndpoints[2][2];
  CGU_UINT8 nIndices[2][BLOCK_SIZE_4X4];
  CGU_FLOAT fError8 =
      CompBlock1X(alphaBlock, BLOCK_SIZE_4X4, nEndpoints[0], nIndices[0], 8, false, 8, 0, true);
  CGU_FLOAT fError6 =
      (fError8 == 0.f) ? CMP_FLOAT_MAX : CompBlock1X(alphaBlock, BLOCK_SIZE_4X4, nEndpoints[1],
                                                     nIndices[1], 6, true, 8, 0, true);
  if(fError8 <= fError6)
    EncodeAlphaBlock(compressedBlock, nEndpoints[0], nIndices[0]);
  else
    EncodeAlphaBlock(compressedBlock, nEndpoints[1], nIndices[1]);
  return CGU_CORE_OK;
}
#endif

#if !defined(BC2_ENCODE_KERNEL_H)
static void GetCompressedAlphaRamp(CGU_UINT8 alpha[8], const CGU_UINT32 compressedBlock[2])
{
  alpha[0] = (CGU_UINT8)(compressedBlock[0] & 0xff);
  alpha[1] = (CGU_UINT8)((compressedBlock[0] >> 8) & 0xff);

  if(alpha[0] > alpha[1])
  {
// 8-alpha block:  derive the other six alphas.
// Bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
#ifdef ASPM_GPU
    alpha[2] = (CGU_UINT8)((6 * alpha[0] + 1 * alpha[1] + 3) / 7);    // bit code 010
    alpha[3] = (CGU_UINT8)((5 * alpha[0] + 2 * alpha[1] + 3) / 7);    // bit code 011
    alpha[4] = (CGU_UINT8)((4 * alpha[0] + 3 * alpha[1] + 3) / 7);    // bit code 100
    alpha[5] = (CGU_UINT8)((3 * alpha[0] + 4 * alpha[1] + 3) / 7);    // bit code 101
    alpha[6] = (CGU_UINT8)((2 * alpha[0] + 5 * alpha[1] + 3) / 7);    // bit code 110
    alpha[7] = (CGU_UINT8)((1 * alpha[0] + 6 * alpha[1] + 3) / 7);    // bit code 111
#else
    alpha[2] = static_cast<CGU_UINT8>((6 * alpha[0] + 1 * alpha[1] + 3) / 7);    // bit code 010
    alpha[3] = static_cast<CGU_UINT8>((5 * alpha[0] + 2 * alpha[1] + 3) / 7);    // bit code 011
    alpha[4] = static_cast<CGU_UINT8>((4 * alpha[0] + 3 * alpha[1] + 3) / 7);    // bit code 100
    alpha[5] = static_cast<CGU_UINT8>((3 * alpha[0] + 4 * alpha[1] + 3) / 7);    // bit code 101
    alpha[6] = static_cast<CGU_UINT8>((2 * alpha[0] + 5 * alpha[1] + 3) / 7);    // bit code 110
    alpha[7] = static_cast<CGU_UINT8>((1 * alpha[0] + 6 * alpha[1] + 3) / 7);    // bit code 111
#endif
  }
  else
  {
// 6-alpha block.
// Bit code 000 = alpha_0, 001 = alpha_1, others are interpolated.
#ifdef ASPM_GPU
    alpha[2] = (CGU_UINT8)((4 * alpha[0] + 1 * alpha[1] + 2) / 5);    // Bit code 010
    alpha[3] = (CGU_UINT8)((3 * alpha[0] + 2 * alpha[1] + 2) / 5);    // Bit code 011
    alpha[4] = (CGU_UINT8)((2 * alpha[0] + 3 * alpha[1] + 2) / 5);    // Bit code 100
    alpha[5] = (CGU_UINT8)((1 * alpha[0] + 4 * alpha[1] + 2) / 5);    // Bit code 101
#else
    alpha[2] = static_cast<CGU_UINT8>((4 * alpha[0] + 1 * alpha[1] + 2) / 5);    // Bit code 010
    alpha[3] = static_cast<CGU_UINT8>((3 * alpha[0] + 2 * alpha[1] + 2) / 5);    // Bit code 011
    alpha[4] = static_cast<CGU_UINT8>((2 * alpha[0] + 3 * alpha[1] + 2) / 5);    // Bit code 100
    alpha[5] = static_cast<CGU_UINT8>((1 * alpha[0] + 4 * alpha[1] + 2) / 5);    // Bit code 101
#endif
    alpha[6] = 0;      // Bit code 110
    alpha[7] = 255;    // Bit code 111
  }
}
#endif    // !BC2

#if !defined(BC2_ENCODE_KERNEL_H)
static void DecompressAlphaBlock(CGU_UINT8 alphaBlock[BLOCK_SIZE_4X4],
                                 const CGU_UINT32 compressedBlock[2])
{
  CGU_UINT8 alpha[8];
  GetCompressedAlphaRamp(alpha, compressedBlock);

  for(int i = 0; i < BLOCK_SIZE_4X4; i++)
  {
    CGU_UINT32 index;
    if(i < 5)
      index = (compressedBlock[0] & (0x7 << (16 + (i * 3)))) >> (16 + (i * 3));
    else if(i > 5)
      index = (compressedBlock[1] & (0x7 << (2 + (i - 6) * 3))) >> (2 + (i - 6) * 3);
    else
    {
      index = (compressedBlock[0] & 0x80000000) >> 31;
      index |= (compressedBlock[1] & 0x3) << 1;
    }

    alphaBlock[i] = alpha[index];
  }
}
#endif    // !BC2
#endif    // !BC1

#endif
