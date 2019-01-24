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

float4 RENDERDOC_FullscreenVS(uint id : SV_VertexID) : SV_Position
{
  float4 pos[] = {float4(-1.0f, 1.0f, 0.0f, 1.0f), float4(3.0f, 1.0f, 0.0f, 1.0f),
                  float4(-1.0f, -3.0f, 0.0f, 1.0f)};

  return pos[id];
}

cbuffer overlayconsts : register(b0)
{
  float4 overlaycol;
};

MultipleOutput RENDERDOC_FixedColPS()
{
  MultipleOutput OUT = (MultipleOutput)0;

  OUT.col0 = OUT.col1 = OUT.col2 = OUT.col3 = OUT.col4 = OUT.col5 = OUT.col6 = OUT.col7 = overlaycol;

  return OUT;
}

float4 RENDERDOC_CheckerboardPS(float4 pos : SV_Position) : SV_Target0
{
  float2 RectRelativePos = pos.xy - RectPosition;

  // if we have a border, and our pos is inside the border, return inner color
  if(BorderWidth >= 0.0f)
  {
    if(RectRelativePos.x >= BorderWidth && RectRelativePos.x <= RectSize.x - BorderWidth &&
       RectRelativePos.y >= BorderWidth && RectRelativePos.y <= RectSize.y - BorderWidth)
    {
      return InnerColor;
    }
  }

  float2 ab = fmod(RectRelativePos.xy, (CheckerSquareDimension * 2.0f).xx);

  bool checkerVariant = ((ab.x < CheckerSquareDimension && ab.y < CheckerSquareDimension) ||
                         (ab.x > CheckerSquareDimension && ab.y > CheckerSquareDimension));

  // otherwise return checker pattern
  return checkerVariant ? PrimaryColor : SecondaryColor;
}
