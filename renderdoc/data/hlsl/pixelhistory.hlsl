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

cbuffer cb0 : register(b0)
{
  uint4 src_coord;    // x, y, mip/sample, slice

  bool multisampled;
  bool is_float;
  bool is_uint;
  bool is_int;
};

cbuffer cb1 : register(b1)
{
  uint dst_slot;
  bool copy_depth;
  bool copy_stencil;
};

Texture2DArray<float2> copyin_depth : register(t0);
Texture2DArray<uint2> copyin_stencil : register(t1);

Texture2DMSArray<float2> copyin_depth_ms : register(t2);
Texture2DMSArray<uint2> copyin_stencil_ms : register(t3);

Texture2DArray<float4> copyin_float : register(t4);
Texture2DMSArray<float4> copyin_float_ms : register(t5);

Texture2DArray<uint4> copyin_uint : register(t6);
Texture2DMSArray<uint4> copyin_uint_ms : register(t7);

Texture2DArray<int4> copyin_int : register(t8);
Texture2DMSArray<int4> copyin_int_ms : register(t9);

RWBuffer<float4> copyout_depth : register(u0);
RWBuffer<float4> copyout_float : register(u1);
RWBuffer<uint4> copyout_uint : register(u2);
RWBuffer<int4> copyout_int : register(u3);

[numthreads(1, 1, 1)] void RENDERDOC_PixelHistoryUnused() {
  copyout_depth[dst_slot] = float4(-1.0f, -1.0f, 0.0f, 0.0f);
}

    [numthreads(1, 1, 1)] void RENDERDOC_PixelHistoryCopyPixel()
{
  if(multisampled)
  {
    if(copy_depth || copy_stencil)
    {
      float2 val =
          float2(copyin_depth_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)].r, -1.0f);

      if(copy_stencil)
        val.g = (float)copyin_stencil_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)].g;

      copyout_depth[dst_slot] = float4(val, 0.0f, 0.0f);
    }
    else
    {
      if(is_float)
      {
        copyout_float[dst_slot] =
            copyin_float_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)];
      }
      else if(is_uint)
      {
        copyout_uint[dst_slot] = copyin_uint_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)];
      }
      else if(is_int)
      {
        copyout_int[dst_slot] = copyin_int_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)];
      }
    }
  }
  else
  {
    if(copy_depth || copy_stencil)
    {
      float2 val = float2(copyin_depth.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)].r, -1.0f);

      if(copy_stencil)
        val.g = (float)copyin_stencil.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)].g;

      copyout_depth[dst_slot] = float4(val, 0.0f, 0.0f);
    }
    else
    {
      if(is_float)
      {
        copyout_float[dst_slot] = copyin_float.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)];
      }
      else if(is_uint)
      {
        copyout_uint[dst_slot] = copyin_uint.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)];
      }
      else if(is_int)
      {
        copyout_int[dst_slot] = copyin_int.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)];
      }
    }
  }
}

float4 RENDERDOC_PrimitiveIDPS(uint prim : SV_PrimitiveID) : SV_Target0
{
  return asfloat(prim).xxxx;
}
