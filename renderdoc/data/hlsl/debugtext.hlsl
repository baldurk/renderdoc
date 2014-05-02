/******************************************************************************
 * The MIT License (MIT)
 * 
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

// text shader, used for the overlay in game so that we can pass indices in the positon stream
// and it figures out the right place in the text texture to sample.

struct a2v
{
	float3 pos : POSITION;
	uint tex : TEXCOORD0;
};

struct v2f
{
	float4 pos : SV_POSITION;
	float4 tex : TEXCOORD0;
};

v2f RENDERDOC_TextVS(a2v IN)
{
	v2f OUT = (v2f)0;

	OUT.pos = float4((float2(IN.pos.z,0) + IN.pos.xy)*TextSize*FontScreenAspect.xy + TextPosition.xy, 0, 1)-float4(1,-1,0,0);
		OUT.tex.xy = (IN.pos.xy+float2(0,1))*CharacterSize.xy + float2((IN.tex.x-1)*CharacterOffsetX, 0);

	if(IN.tex.x == 0)
		OUT.tex.xy = 0;
	return OUT;
}

SamplerState pointSample : register(s0);
SamplerState linearSample : register(s1);

Texture2D debugTexture : register(t0);

float4 RENDERDOC_TextPS(v2f IN) : SV_Target0
{
	IN.tex.y = 1 - IN.tex.y;

	float4 text = debugTexture.Sample(linearSample, IN.tex.xy).xxxx;

	return text + float4(0.0.xxx, 0.5);
}
