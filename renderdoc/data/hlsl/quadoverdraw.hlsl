/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Baldur Karlsson
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

////////////////////////////////////////////////////////////////////////////////////////////
// Below shaders courtesy of Stephen Hill (@self_shadow)
//
// http://blog.selfshadow.com/2012/11/12/counting-quads/
// https://github.com/selfshadow/demos/blob/master/QuadShading/QuadShading.fx
////////////////////////////////////////////////////////////////////////////////////////////

RWTexture2DArray<uint> overdrawUAV : register(u0);
Texture2DArray<uint> overdrawSRV : register(t0);

[earlydepthstencil] void RENDERDOC_QuadOverdrawPS(float4 vpos
                                                  : SV_Position, uint c0
                                                  : SV_Coverage) {
  // Obtain coverage for all pixels in the quad, via 'message passing'*.
  // (* For more details, see:
  // "Shader Amortization using Pixel Quad Message Passing", Eric Penner, GPU Pro 2.)
  uint2 p = uint2(vpos.xy) & 1;
  int2 sign = p ? -1 : 1;
  uint c1 = c0 + sign.x * ddx_fine(c0);
  uint c2 = c0 + sign.y * ddy_fine(c0);
  uint c3 = c2 + sign.x * ddx_fine(c2);

  // Count the live pixels, minus 1 (zero indexing)
  uint pixelCount = c0 + c1 + c2 + c3 - 1;

  uint3 quad = uint3(vpos.xy * 0.5, pixelCount);
  InterlockedAdd(overdrawUAV[quad], 1);
}

float4 RENDERDOC_QOResolvePS(float4 vpos
                             : SV_POSITION)
    : SV_Target0
{
  uint2 quad = vpos.xy * 0.5;

  uint overdraw = 0;
  for(int i = 0; i < 4; i++)
    overdraw += overdrawSRV[uint3(quad, i)] / (i + 1);

  return float(overdraw).xxxx;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Above shaders courtesy of Stephen Hill (@self_shadow)
//
// http://blog.selfshadow.com/2012/11/12/counting-quads/
// https://github.com/selfshadow/demos/blob/master/QuadShading/QuadShading.fx
////////////////////////////////////////////////////////////////////////////////////////////
