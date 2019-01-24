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

#include "hlsl_cbuffers.h"
#include "hlsl_texsample.h"

// compute shaders that figure out the min/max values or histogram in a texture heirarchically
// note that we have to conditionally compile this shader for float/uint/sint as doing that
// dynamically produces a shader with too many temp registers unfortunately.

RWBuffer<float4> MinMaxDestFloat : register(u0);
RWBuffer<uint4> MinMaxDestUInt : register(u1);
RWBuffer<int4> MinMaxDestInt : register(u2);

[numthreads(HGRAM_TILES_PER_BLOCK, HGRAM_TILES_PER_BLOCK, 1)] void RENDERDOC_TileMinMaxCS(
    uint3 tid
    : SV_GroupThreadID, uint3 gid
    : SV_GroupID)
{
  uint texType = SHADER_RESTYPE;

  uint3 texDim = uint3(HistogramTextureResolution);

  uint blocksX = (int)ceil(float(texDim.x) / float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

  uint2 topleft = (gid.xy * HGRAM_TILES_PER_BLOCK + tid.xy) * HGRAM_PIXELS_PER_TILE;

  uint outIdx = (tid.y * HGRAM_TILES_PER_BLOCK + tid.x) +
                (gid.y * blocksX + gid.x) * (HGRAM_TILES_PER_BLOCK * HGRAM_TILES_PER_BLOCK);

  int i = 0;

#if UINT_TEX
  {
    uint4 minval = 0;
    uint4 maxval = 0;

    for(uint y = topleft.y; y < min(texDim.y, topleft.y + HGRAM_PIXELS_PER_TILE); y++)
    {
      for(uint x = topleft.x; x < min(texDim.x, topleft.x + HGRAM_PIXELS_PER_TILE); x++)
      {
        uint4 data = SampleTextureUInt4(texType, float2(x, y) / float2(texDim.xy), HistogramSlice,
                                        HistogramMip, HistogramSample, texDim);

        if(i == 0)
        {
          minval = maxval = data;
        }
        else
        {
          minval = min(minval, data);
          maxval = max(maxval, data);
        }

        i++;
      }
    }

    MinMaxDestUInt[outIdx * 2 + 0] = minval;
    MinMaxDestUInt[outIdx * 2 + 1] = maxval;
    return;
  }
#elif SINT_TEX
  {
    int4 minval = 0;
    int4 maxval = 0;

    for(uint y = topleft.y; y < min(texDim.y, topleft.y + HGRAM_PIXELS_PER_TILE); y++)
    {
      for(uint x = topleft.x; x < min(texDim.x, topleft.x + HGRAM_PIXELS_PER_TILE); x++)
      {
        int4 data = SampleTextureInt4(texType, float2(x, y) / float2(texDim.xy), HistogramSlice,
                                      HistogramMip, HistogramSample, texDim);

        if(i == 0)
        {
          minval = maxval = data;
        }
        else
        {
          minval = min(minval, data);
          maxval = max(maxval, data);
        }

        i++;
      }
    }

    MinMaxDestInt[outIdx * 2 + 0] = minval;
    MinMaxDestInt[outIdx * 2 + 1] = maxval;
    return;
  }
#else
  {
    float4 minval = 0;
    float4 maxval = 0;

    for(uint y = topleft.y; y < min(texDim.y, topleft.y + HGRAM_PIXELS_PER_TILE); y++)
    {
      for(uint x = topleft.x; x < min(texDim.x, topleft.x + HGRAM_PIXELS_PER_TILE); x++)
      {
        float4 data = SampleTextureFloat4(texType, false, float2(x, y) / float2(texDim.xy),
                                          HistogramSlice, HistogramMip, HistogramSample, texDim,
                                          HistogramYUVDownsampleRate, HistogramYUVAChannels);

        if(i == 0)
        {
          minval = maxval = data;
        }
        else
        {
          minval = min(minval, data);
          maxval = max(maxval, data);
        }

        i++;
      }
    }

    MinMaxDestFloat[outIdx * 2 + 0] = minval;
    MinMaxDestFloat[outIdx * 2 + 1] = maxval;
    return;
  }
#endif
}

Buffer<float4> MinMaxResultSourceFloat : register(t0);
Buffer<uint4> MinMaxResultSourceUInt : register(t1);
Buffer<int4> MinMaxResultSourceInt : register(t2);

RWBuffer<float4> MinMaxResultDestFloat : register(u0);
RWBuffer<uint4> MinMaxResultDestUInt : register(u1);
RWBuffer<int4> MinMaxResultDestInt : register(u2);

[numthreads(1, 1, 1)] void RENDERDOC_ResultMinMaxCS()
{
  uint3 texDim = uint3(HistogramTextureResolution);

  uint blocksX = (int)ceil(float(texDim.x) / float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));
  uint blocksY = (int)ceil(float(texDim.y) / float(HGRAM_PIXELS_PER_TILE * HGRAM_TILES_PER_BLOCK));

#if UINT_TEX
  uint4 minvalU = MinMaxResultSourceUInt[0];
  uint4 maxvalU = MinMaxResultSourceUInt[1];
#elif SINT_TEX
  int4 minvalI = MinMaxResultSourceInt[0];
  int4 maxvalI = MinMaxResultSourceInt[1];
#else
  float4 minvalF = MinMaxResultSourceFloat[0];
  float4 maxvalF = MinMaxResultSourceFloat[1];
#endif

  // i is the tile we're looking at
  for(uint i = 1; i < blocksX * blocksY * HGRAM_TILES_PER_BLOCK * HGRAM_TILES_PER_BLOCK; i++)
  {
    uint blockIdx = i / (HGRAM_TILES_PER_BLOCK * HGRAM_TILES_PER_BLOCK);
    uint tileIdx = i % (HGRAM_TILES_PER_BLOCK * HGRAM_TILES_PER_BLOCK);

    // which block and tile is this in
    uint2 blockXY = uint2(blockIdx % blocksX, blockIdx / blocksX);
    uint2 tileXY = uint2(tileIdx % HGRAM_TILES_PER_BLOCK, tileIdx / HGRAM_TILES_PER_BLOCK);

    // if this is at least partially within the texture, include it.
    if(blockXY.x * (HGRAM_TILES_PER_BLOCK * HGRAM_PIXELS_PER_TILE) + tileXY.x * HGRAM_PIXELS_PER_TILE <
           texDim.x &&
       blockXY.y * (HGRAM_TILES_PER_BLOCK * HGRAM_PIXELS_PER_TILE) + tileXY.y * HGRAM_PIXELS_PER_TILE <
           texDim.y)
    {
#if UINT_TEX
      minvalU = min(minvalU, MinMaxResultSourceUInt[i * 2 + 0]);
      maxvalU = max(maxvalU, MinMaxResultSourceUInt[i * 2 + 1]);
#elif SINT_TEX
      minvalI = min(minvalI, MinMaxResultSourceInt[i * 2 + 0]);
      maxvalI = max(maxvalI, MinMaxResultSourceInt[i * 2 + 1]);
#else
      minvalF = min(minvalF, MinMaxResultSourceFloat[i * 2 + 0]);
      maxvalF = max(maxvalF, MinMaxResultSourceFloat[i * 2 + 1]);
#endif
    }
  }

#if UINT_TEX
  MinMaxResultDestUInt[0] = minvalU;
  MinMaxResultDestUInt[1] = maxvalU;
#elif SINT_TEX
  MinMaxResultDestInt[0] = minvalI;
  MinMaxResultDestInt[1] = maxvalI;
#else
  MinMaxResultDestFloat[0] = minvalF;
  MinMaxResultDestFloat[1] = maxvalF;
#endif
}

RWBuffer<uint> HistogramDest : register(u0);

[numthreads(HGRAM_TILES_PER_BLOCK, HGRAM_TILES_PER_BLOCK, 1)] void RENDERDOC_HistogramCS(
    uint3 tid
    : SV_GroupThreadID, uint3 gid
    : SV_GroupID)
{
  uint texType = SHADER_RESTYPE;

  uint3 texDim = uint3(HistogramTextureResolution);

  uint blocksX = (int)ceil(float(texDim.x) / float(HGRAM_PIXELS_PER_TILE * HGRAM_PIXELS_PER_TILE));

  uint2 topleft = (gid.xy * HGRAM_TILES_PER_BLOCK + tid.xy) * HGRAM_PIXELS_PER_TILE;

  int i = 0;

  for(uint y = topleft.y; y < min(texDim.y, topleft.y + HGRAM_PIXELS_PER_TILE); y++)
  {
    for(uint x = topleft.x; x < min(texDim.x, topleft.x + HGRAM_PIXELS_PER_TILE); x++)
    {
      uint bucketIdx = HGRAM_NUM_BUCKETS + 1;

#if UINT_TEX
      {
        uint4 data = SampleTextureUInt4(texType, float2(x, y) / float2(texDim.xy), HistogramSlice,
                                        HistogramMip, HistogramSample, texDim);

        float divisor = 0.0f;
        uint sum = 0;
        if(HistogramChannels & 0x1)
        {
          sum += data.x;
          divisor += 1.0f;
        }
        if(HistogramChannels & 0x2)
        {
          sum += data.y;
          divisor += 1.0f;
        }
        if(HistogramChannels & 0x4)
        {
          sum += data.z;
          divisor += 1.0f;
        }
        if(HistogramChannels & 0x8)
        {
          sum += data.w;
          divisor += 1.0f;
        }

        if(divisor > 0.0f)
        {
          float val = float(sum) / divisor;

          float normalisedVal = (val - HistogramMin) / (HistogramMax - HistogramMin);

          if(normalisedVal < 0.0f)
            normalisedVal = 2.0f;

          bucketIdx = (uint)floor(normalisedVal * HGRAM_NUM_BUCKETS);
        }
      }
#elif SINT_TEX
      {
        int4 data = SampleTextureInt4(texType, float2(x, y) / float2(texDim.xy), HistogramSlice,
                                      HistogramMip, HistogramSample, texDim);

        float divisor = 0.0f;
        int sum = 0;
        if(HistogramChannels & 0x1)
        {
          sum += data.x;
          divisor += 1.0f;
        }
        if(HistogramChannels & 0x2)
        {
          sum += data.y;
          divisor += 1.0f;
        }
        if(HistogramChannels & 0x4)
        {
          sum += data.z;
          divisor += 1.0f;
        }
        if(HistogramChannels & 0x8)
        {
          sum += data.w;
          divisor += 1.0f;
        }

        if(divisor > 0.0f)
        {
          float val = float(sum) / divisor;

          float normalisedVal = (val - HistogramMin) / (HistogramMax - HistogramMin);

          if(normalisedVal < 0.0f)
            normalisedVal = 2.0f;

          bucketIdx = (uint)floor(normalisedVal * HGRAM_NUM_BUCKETS);
        }
      }
#else
      {
        float4 data = SampleTextureFloat4(texType, false, float2(x, y) / float2(texDim.xy),
                                          HistogramSlice, HistogramMip, HistogramSample, texDim,
                                          HistogramYUVDownsampleRate, HistogramYUVAChannels);

        float divisor = 0.0f;
        float sum = 0.0f;
        if(HistogramChannels & 0x1)
        {
          sum += data.x;
          divisor += 1.0f;
        }
        if(HistogramChannels & 0x2)
        {
          sum += data.y;
          divisor += 1.0f;
        }
        if(HistogramChannels & 0x4)
        {
          sum += data.z;
          divisor += 1.0f;
        }
        if(HistogramChannels & 0x8)
        {
          sum += data.w;
          divisor += 1.0f;
        }

        if(divisor > 0.0f)
        {
          float val = sum / divisor;

          float normalisedVal = (val - HistogramMin) / (HistogramMax - HistogramMin);

          if(normalisedVal < 0.0f)
            normalisedVal = 2.0f;

          bucketIdx = (uint)floor(normalisedVal * HGRAM_NUM_BUCKETS);
        }
      }
#endif

      if(bucketIdx >= 0 && bucketIdx < HGRAM_NUM_BUCKETS)
        InterlockedAdd(HistogramDest[bucketIdx], 1);
    }
  }
}
