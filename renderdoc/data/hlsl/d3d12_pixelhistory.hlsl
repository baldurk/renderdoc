/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2023 Baldur Karlsson
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

cbuffer CopyPixelShaderInput : register(b0)
{
  uint4 src_coord;    // x, y, mip/sample, slice

  uint dst_slot;
  bool copy_depth;
  bool copy_stencil;

  bool multisampled;
  bool is_float;
  bool is_uint;
  bool is_int;
};

Texture2DArray<float> copyin_depth : register(t0);
Texture2DArray<uint> copyin_stencil : register(t1);

Texture2DMSArray<float> copyin_depth_ms : register(t2);
Texture2DMSArray<uint2> copyin_stencil_ms : register(t3);

Texture2DArray<float4> copyin_float : register(t4);
Texture2DMSArray<float4> copyin_float_ms : register(t5);

Texture2DArray<uint4> copyin_uint : register(t6);
Texture2DMSArray<uint4> copyin_uint_ms : register(t7);

Texture2DArray<int4> copyin_int : register(t8);
Texture2DMSArray<int4> copyin_int_ms : register(t9);

RWBuffer<float> copyout_depth : register(u0);
RWBuffer<uint> copyout_stencil : register(u1);
RWBuffer<float> copyout_float : register(u2);
RWBuffer<uint> copyout_uint : register(u3);
RWBuffer<int> copyout_int : register(u4);

[numthreads(1, 1, 1)] void RENDERDOC_PixelHistoryUnused() { copyout_depth[dst_slot] = -1.0f; }

    [numthreads(1, 1, 1)] void RENDERDOC_PixelHistoryCopyPixel()
{
  if(multisampled)
  {
    if(copy_depth)
    {
      float val = copyin_depth_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)];
      copyout_depth[dst_slot] = val;
    }
    else if(copy_stencil)
    {
      uint val = copyin_stencil_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)].g;
      copyout_stencil[dst_slot] = val;
    }
    else
    {
      if(is_float)
      {
        float4 val = copyin_float_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)];
        copyout_float[dst_slot] = val.x;
        copyout_float[dst_slot + 1] = val.y;
        copyout_float[dst_slot + 2] = val.z;
        copyout_float[dst_slot + 3] = val.w;
      }
      else if(is_uint)
      {
        uint4 val = copyin_uint_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)];
        copyout_uint[dst_slot] = val.x;
        copyout_uint[dst_slot + 1] = val.y;
        copyout_uint[dst_slot + 2] = val.z;
        copyout_uint[dst_slot + 3] = val.w;
      }
      else if(is_int)
      {
        int4 val = copyin_int_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)];
        copyout_int[dst_slot] = val.x;
        copyout_int[dst_slot + 1] = val.y;
        copyout_int[dst_slot + 2] = val.z;
        copyout_int[dst_slot + 3] = val.w;
      }
    }
  }
  else
  {
    if(copy_depth)
    {
      float val = copyin_depth.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)];
      copyout_depth[dst_slot] = val;
    }
    else if(copy_stencil)
    {
      uint val = copyin_stencil.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)];
      copyout_stencil[dst_slot] = val;
    }
    else
    {
      if(is_float)
      {
        float4 val = copyin_float.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)];
        copyout_float[dst_slot] = val.x;
        copyout_float[dst_slot + 1] = val.y;
        copyout_float[dst_slot + 2] = val.z;
        copyout_float[dst_slot + 3] = val.w;
      }
      else if(is_uint)
      {
        uint4 val = copyin_uint.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)];
        copyout_uint[dst_slot] = val.x;
        copyout_uint[dst_slot + 1] = val.y;
        copyout_uint[dst_slot + 2] = val.z;
        copyout_uint[dst_slot + 3] = val.w;
      }
      else if(is_int)
      {
        int4 val = copyin_int.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)];
        copyout_int[dst_slot] = val.x;
        copyout_int[dst_slot + 1] = val.y;
        copyout_int[dst_slot + 2] = val.z;
        copyout_int[dst_slot + 3] = val.w;
      }
    }
  }
}

float4 RENDERDOC_PrimitiveIDPS(uint prim : SV_PrimitiveID) : SV_Target0
{
  return asfloat(prim).xxxx;
}

struct MultipleOutput
{
  float4 col0 : SV_Target0;
  float4 col1 : SV_Target1;
  float4 col2 : SV_Target2;
  float4 col3 : SV_Target3;
  float4 col4 : SV_Target4;
  float4 col5 : SV_Target5;
  float4 col6 : SV_Target6;
  float4 col7 : SV_Target7;
};

MultipleOutput RENDERDOC_PixelHistoryFixedColPS()
{
  MultipleOutput OUT = (MultipleOutput)0;

  float4 color = float4(0.1f, 0.2f, 0.3f, 0.4f);
  OUT.col0 = OUT.col1 = OUT.col2 = OUT.col3 = OUT.col4 = OUT.col5 = OUT.col6 = OUT.col7 = color;

  return OUT;
}
