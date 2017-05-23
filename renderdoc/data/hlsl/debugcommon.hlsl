/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2017 Baldur Karlsson
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

// this file provides a couple of functions that, given the basic type, will go and
// figure out which resource to sample from and load from it then return the value

struct v2f
{
	float4 pos : SV_Position;
	float4 tex : TEXCOORD0;
};

SamplerState pointSampler : register(s0);
SamplerState linearSampler : register(s1);

Texture1DArray<float4> texDisplayTex1DArray : register(t1);
Texture2DArray<float4> texDisplayTex2DArray : register(t2);
Texture3D<float4> texDisplayTex3D : register(t3);
Texture2DArray<float2> texDisplayTexDepthArray : register(t4);
Texture2DArray<uint2> texDisplayTexStencilArray : register(t5);
Texture2DMSArray<float2> texDisplayTexDepthMSArray : register(t6);
Texture2DMSArray<uint2> texDisplayTexStencilMSArray : register(t7);
Texture2DMSArray<float4> texDisplayTex2DMSArray : register(t9);

Texture1DArray<uint4> texDisplayUIntTex1DArray : register(t11);
Texture2DArray<uint4> texDisplayUIntTex2DArray : register(t12);
Texture3D<uint4> texDisplayUIntTex3D : register(t13);
Texture2DMSArray<uint4> texDisplayUIntTex2DMSArray : register(t19);

Texture1DArray<int4> texDisplayIntTex1DArray : register(t21);
Texture2DArray<int4> texDisplayIntTex2DArray : register(t22);
Texture3D<int4> texDisplayIntTex3D : register(t23);
Texture2DMSArray<int4> texDisplayIntTex2DMSArray : register(t29);

uint4 SampleTextureUInt4(uint type, float2 uv, float slice, float mip, int sample, float3 texRes)
{
	uint4 col = 0;

	if(type == RESTYPE_TEX1D)
		col = texDisplayUIntTex1DArray.Load(int3(uv.x*texRes.x, slice, mip));
	else if(type == RESTYPE_TEX3D)
		col = texDisplayUIntTex3D.Load(int4(uv.xy*texRes.xy, slice + 0.001f, mip));
	else if(type == RESTYPE_TEX2D)
		col = texDisplayUIntTex2DArray.Load(int4(uv.xy*texRes.xy, slice, mip));
	else if(type == RESTYPE_TEX2D_MS)
	{
		if(sample < 0)
			sample = 0;
		col = texDisplayUIntTex2DMSArray.Load(int3(uv.xy*texRes.xy, slice), sample);
	}

	return col;
}

int4 SampleTextureInt4(uint type, float2 uv, float slice, float mip, int sample, float3 texRes)
{
	int4 col = 0;

	if(type == RESTYPE_TEX1D)
		col = texDisplayIntTex1DArray.Load(int3(uv.x*texRes.x, slice, mip));
	else if(type == RESTYPE_TEX3D)
		col = texDisplayIntTex3D.Load(int4(uv.xy*texRes.xy, slice + 0.001f, mip));
	else if(type == RESTYPE_TEX2D)
		col = texDisplayIntTex2DArray.Load(int4(uv.xy*texRes.xy, slice, mip));
	else if(type == RESTYPE_TEX2D_MS)
	{
		if(sample < 0)
			sample = 0;
		col = texDisplayIntTex2DMSArray.Load(int3(uv.xy*texRes.xy, slice), sample);
	}

	return col;
}

float4 SampleTextureFloat4(uint type, bool linearSample, float2 uv, float slice, float mip, int sample, float3 texRes)
{
	float4 col = 0;

	if(type == RESTYPE_TEX1D)
	{
		if(linearSample)
			col = texDisplayTex1DArray.SampleLevel(linearSampler, float2(uv.x, slice), mip);
		else
			col = texDisplayTex1DArray.Load(int3(uv.x*texRes.x, slice, mip));
	}
	else if(type == RESTYPE_TEX3D)
	{
		if(linearSample)
			col = texDisplayTex3D.SampleLevel(linearSampler, float3(uv.xy, (slice + 0.001f) / texRes.z), mip);
		else
			col = texDisplayTex3D.Load(int4(uv.xy*texRes.xy, slice + 0.001f, mip));
	}
	else if(type == RESTYPE_DEPTH)
	{
		col.r = texDisplayTexDepthArray.Load(int4(uv.xy*texRes.xy, slice, mip)).r;
		col.gba = float3(0, 0, 1);
	}
	else if(type == RESTYPE_DEPTH_STENCIL)
	{
		col.r = texDisplayTexDepthArray.Load(int4(uv.xy*texRes.xy, slice, mip)).r;
		col.g = texDisplayTexStencilArray.Load(int4(uv.xy*texRes.xy, slice, mip)).g/255.0f;
		col.ba = float2(0, 1);
	}
	else if(type == RESTYPE_DEPTH_MS)
	{
		if(sample < 0)
			sample = 0;

		col.r = texDisplayTexDepthMSArray.Load(int3(uv.xy*texRes.xy, slice), sample).r;
		col.gba = float3(0, 0, 1);
	}
	else if(type == RESTYPE_DEPTH_STENCIL_MS)
	{
		if(sample < 0)
			sample = 0;

		col.r = texDisplayTexDepthMSArray.Load(int3(uv.xy*texRes.xy, slice), sample).r;
		col.g = texDisplayTexStencilMSArray.Load(int3(uv.xy*texRes.xy, slice), sample).g/255.0f;
		col.ba = float2(0, 1);
	}
	else if(type == RESTYPE_TEX2D_MS)
	{
		if(sample < 0)
		{
			int sampleCount = -sample;

			// worst resolve you've seen in your life
			for(int i=0; i < sampleCount; i++)
				col += texDisplayTex2DMSArray.Load(int3(uv.xy*texRes.xy, slice), i);

			col /= float(sampleCount);
		}
		else
		{
			col = texDisplayTex2DMSArray.Load(int3(uv.xy*texRes.xy, slice), sample);
		}
	}
	else if(type == RESTYPE_TEX2D)
	{
		if(linearSample)
			col = texDisplayTex2DArray.SampleLevel(linearSampler, float3(uv.xy, slice), mip);
		else
			col = texDisplayTex2DArray.Load(int4(uv.xy*texRes.xy, slice, mip));
	}

	return col;
}
