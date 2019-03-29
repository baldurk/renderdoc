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

// text shader, used for the overlay in game so that we can pass indices in the positon stream
// and it figures out the right place in the text texture to sample.

struct glyph
{
  float4 posdata;
  float4 uvdata;
};

cbuffer glyphdata : register(b1)
{
  glyph glyphs[127 - 32];
};

cbuffer stringdata : register(b2)
{
  int4 chars[256];
};

struct v2f
{
  float4 pos : SV_Position;
  float4 tex : TEX;
  float2 glyphuv : GLYPH;
};

v2f Text(float2 pos, int charidx, int glyphidx)
{
  v2f OUT = (v2f)0;

  float2 charPos = float2(float(charidx) + pos.x + TextPosition.x, -pos.y - TextPosition.y);
  glyph G = glyphs[glyphidx];

  OUT.pos = float4(charPos.xy * 2.0f * TextSize * FontScreenAspect.xy + float2(-1, 1), 1, 1);
  OUT.glyphuv.xy = (pos.xy - G.posdata.xy) * G.posdata.zw;
  OUT.tex = G.uvdata * CharacterSize.xyxy;

  return OUT;
}

v2f RENDERDOC_TextVS(uint vid : SV_VertexID, uint inst : SV_InstanceID)
{
  // easy-mode on FL10 and up, use vertex/instance index
  float2 verts[] = {
      float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0), float2(1.0, 1.0),
  };

  return Text(verts[vid], int(inst), chars[inst].x);
}

v2f RENDERDOC_Text9VS(float4 pos : POSITION)
{
  // hard mode on FL9, fetch from vertex inputs
  return Text(pos.xy, int(pos.z + 0.1f), int(pos.w + 0.1f));
}

SamplerState pointSample : register(s0);
SamplerState linearSample : register(s1);

Texture2D fontTexture : register(t0);

float4 RENDERDOC_TextPS(v2f IN) : SV_Target0
{
  float text = 0;

  if(IN.glyphuv.x >= 0.0f && IN.glyphuv.x <= 1.0f && IN.glyphuv.y >= 0.0f && IN.glyphuv.y <= 1.0f)
  {
    float2 uv;
    uv.x = lerp(IN.tex.x, IN.tex.z, IN.glyphuv.x);
    uv.y = lerp(IN.tex.y, IN.tex.w, IN.glyphuv.y);
    text = fontTexture.Sample(linearSample, uv.xy).x;
  }

  return float4(text.xxx, saturate(text + 0.5f));
}
