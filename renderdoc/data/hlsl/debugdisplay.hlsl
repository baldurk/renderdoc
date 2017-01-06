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



v2f RENDERDOC_DebugVS(uint vertID : SV_VertexID)
{
	v2f OUT = (v2f)0;

	float2 positions[] = {
		float2(0.0f,  0.0f),
		float2(0.0f, -1.0f),
		float2(1.0f,  0.0f),
		float2(1.0f, -1.0f),
		float2(0.0f,  0.0f),
	};

	float2 pos = positions[vertID];

	if(LineStrip)
	{
		float2 strippositions[] = {
			float2(0.0f, -1.0f),
			float2(1.0f, -1.0f),
			float2(1.0f,  0.0f),
			float2(0.0f,  0.0f),
			float2(0.0f, -1.1f),
		};

		pos = strippositions[vertID];
	}

	OUT.pos = float4(Position.xy + pos.xy*TextureResolution.xy*Scale*ScreenAspect.xy, 0, 1)-float4(1.0,-1.0,0,0);
	OUT.tex.xy = float2(pos.x, -pos.y);
	return OUT;
}

float ConvertSRGBToLinear(float srgb)
{
	if (srgb <= 0.04045f)
		return srgb / 12.92f;
	else
		return pow((saturate(srgb) + 0.055f) / 1.055f, 2.4f);
}

// main texture display shader, used for the texture viewer. It samples the right resource
// for the type and applies things like the range check and channel masking.
// It also does a couple of overlays that we can get 'free' like NaN/inf checks
// or range clipping
float4 RENDERDOC_TexDisplayPS(v2f IN) : SV_Target0
{
	bool uintTex = OutputDisplayFormat & TEXDISPLAY_UINT_TEX;
	bool sintTex = OutputDisplayFormat & TEXDISPLAY_SINT_TEX;

	float4 col = 0;
	uint4 ucol = 0;
	int4 scol = 0;

	float2 uvTex = IN.tex.xy;

	if(FlipY)
		uvTex.y = 1.0f - uvTex.y;

	if(uintTex)
	{
		ucol = SampleTextureUInt4(OutputDisplayFormat & TEXDISPLAY_TYPEMASK,
								  uvTex, Slice, MipLevel, SampleIdx, TextureResolutionPS);
	}
	else if(sintTex)
	{
		scol = SampleTextureInt4 (OutputDisplayFormat & TEXDISPLAY_TYPEMASK,
								  uvTex, Slice, MipLevel, SampleIdx, TextureResolutionPS);
	}
	else
	{
		col = SampleTextureFloat4(OutputDisplayFormat & TEXDISPLAY_TYPEMASK, (ScalePS < 1 && MipLevel == 0),
								  uvTex, Slice, MipLevel, SampleIdx, TextureResolutionPS);
	}
	
	if(RawOutput)
	{
		if(uintTex)
			return asfloat(ucol);
		else if(sintTex)
			return asfloat(scol);
		else
			return col;
	}

    // RGBM encoding
	if(WireframeColour.x > 0.0f)
	{
		if(uintTex)
			ucol = float4(ucol.rgb * ucol.a * (uint)(WireframeColour.x), 1.0f);
		else if(sintTex)
			scol = float4(scol.rgb * scol.a * (int)(WireframeColour.x), 1.0f);
		else
			col = float4(col.rgb * col.a * WireframeColour.x, 1.0f);
	}

	if(uintTex)
		col = (float4)(ucol);
	else if(sintTex)
		col = (float4)(scol);

	float4 pre_range_col = col;

	col = ((col - RangeMinimum)*InverseRangeSize);

	// workaround for D3DCompiler bug. For some reason it assumes texture samples can
	// never come back as NaN, so involving a cbuffer value like this here ensures
	// the below isnan()s don't get optimised out.
	if(Channels.x < 0.5f) col.x = pre_range_col.x = AlwaysZero;
	if(Channels.y < 0.5f) col.y = pre_range_col.y = AlwaysZero;
	if(Channels.z < 0.5f) col.z = pre_range_col.z = AlwaysZero;
	if(Channels.w < 0.5f) col.w = pre_range_col.w = 1.0f-AlwaysZero;

	// show nans, infs and negatives
	if(OutputDisplayFormat & TEXDISPLAY_NANS)
	{
		if(isnan(pre_range_col.r) || isnan(pre_range_col.g) || isnan(pre_range_col.b) || isnan(pre_range_col.a))
		   return float4(1, 0, 0, 1);
		   
		if(isinf(pre_range_col.r) || isinf(pre_range_col.g) || isinf(pre_range_col.b) || isinf(pre_range_col.a))
		   return float4(0, 1, 0, 1);

		if(pre_range_col.r < 0 || pre_range_col.g < 0 || pre_range_col.b < 0 || pre_range_col.a < 0)
		   return float4(0, 0, 1, 1);
		
		col = float4(dot(col.xyz, float3(0.2126, 0.7152, 0.0722)).xxx, 1);
	}
	else if(OutputDisplayFormat & TEXDISPLAY_CLIPPING)
	{
		if(col.r < 0 || col.g < 0 || col.b < 0 || col.a < 0)
		   return float4(1, 0, 0, 1);

		if(col.r > (1+FLT_EPSILON) || col.g > (1+FLT_EPSILON) || col.b > (1+FLT_EPSILON) || col.a > (1+FLT_EPSILON))
		   return float4(0, 1, 0, 1);
		
		col = float4(dot(col.xyz, float3(0.2126, 0.7152, 0.0722)).xxx, 1);
	}
	else
	{
		// if only one channel is selected
		if(dot(Channels, 1) == 1)
		{
			// if it's alpha, just move it into rgb
			// otherwise, select the channel that's on and replicate it across all channels
			if(Channels.a == 1)
				col = float4(col.aaa, 1);
			else
				col = float4(dot(col.rgb, 1).xxx, 1);
		}
	}

	if(OutputDisplayFormat & TEXDISPLAY_GAMMA_CURVE)
	{
		col.rgb = float3(ConvertSRGBToLinear(col.r), ConvertSRGBToLinear(col.g), ConvertSRGBToLinear(col.b));
	}
	
	return col;
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

struct wireframeV2F
{
	float4 pos : SV_Position;
	float3 norm : Normal;
	float4 secondary : Secondary;
};

wireframeV2F RENDERDOC_FullscreenVS(uint id : SV_VertexID)
{
	wireframeV2F OUT = (wireframeV2F)0;

	float4 pos[] = {
		float4( -1.0f,  1.0f, 0.0f, 1.0f),
		float4(  3.0f,  1.0f, 0.0f, 1.0f),
		float4( -1.0f, -3.0f, 0.0f, 1.0f)
	};
	
	float2 uv[] = {
		float2(0.0f, 0.0f),
		float2(2.0f, 0.0f),
		float2(0.0f, 2.0f)
	};

	OUT.pos = pos[id];
	OUT.secondary = float4(uv[id].xy, 0, 1);

	return OUT;
}

MultipleOutput RENDERDOC_WireframePS(wireframeV2F IN)
{
	MultipleOutput OUT = (MultipleOutput)0;

	OUT.col0 =
	OUT.col1 =
	OUT.col2 =
	OUT.col3 =
	OUT.col4 =
	OUT.col5 =
	OUT.col6 =
	OUT.col7 =
		float4(WireframeColour.xyz, 1);

	return OUT;
}

cbuffer overlayconsts : register(b0)
{
	float4 overlaycol;
};

MultipleOutput RENDERDOC_OverlayPS(float4 IN : SV_Position)
{
	MultipleOutput OUT = (MultipleOutput)0;

	OUT.col0 =
	OUT.col1 =
	OUT.col2 =
	OUT.col3 =
	OUT.col4 =
	OUT.col5 =
	OUT.col6 =
	OUT.col7 =
		overlaycol;

	return OUT;
}

float4 RENDERDOC_OutlinePS(float4 IN : SV_Position) : SV_Target0
{
	float4 ret = Channels;

	float2 rectPos = IN.xy - float2(RangeMinimum, InverseRangeSize);
	float2 rectSize = TextureResolutionPS.xy;
 
	float2 ab = fmod(rectPos.xy, 32.0.xx);

	bool checkerVariant = (
			(ab.x < 16 && ab.y < 16) ||
			(ab.x > 16 && ab.y > 16)
		);

	if(OutputDisplayFormat == 0)
	{
		if(rectPos.x < 3.0f || rectPos.x > rectSize.x - 3.0f ||
		   rectPos.y < 3.0f || rectPos.y > rectSize.y - 3.0f)
		{
			ret.rgb = WireframeColour.rgb;
			ret.a = 1.0f;
		}
	}
	else
	{
		if(rectPos.x < 3.0f || rectPos.x > rectSize.x - 3.0f ||
		   rectPos.y < 3.0f || rectPos.y > rectSize.y - 3.0f)
		{
			ret = checkerVariant ? float4(1, 1, 1, 1) : float4(0, 0, 0, 1);
		}
		else
		{
			ret = float4(0, 0, 0, 0);
		}
	}

	return ret;
}

float4 RENDERDOC_CheckerboardPS(float4 IN : SV_Position) : SV_Target0
{
	float2 ab = fmod(IN.xy, 128.0.xx);

	if(
		(ab.x < 64 && ab.y < 64) ||
		(ab.x > 64 && ab.y > 64)
		)
	{
		return float4(WireframeColour.rgb*WireframeColour.rgb, 1);
	}

	return float4(Channels.rgb*Channels.rgb, 1);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Below shaders courtesy of Stephen Hill (@self_shadow)
//
// http://blog.selfshadow.com/2012/11/12/counting-quads/
// https://github.com/selfshadow/demos/blob/master/QuadShading/QuadShading.fx
////////////////////////////////////////////////////////////////////////////////////////////

#define NUM_RAMP_COLOURS 128

RWTexture2DArray<uint> overdrawUAV  : register(u0);
Texture2DArray<uint> overdrawSRV : register(t0);
cbuffer overdrawRampCBuf : register(b0)
{
	const float4 overdrawRampColours[NUM_RAMP_COLOURS];
};

[earlydepthstencil]
void RENDERDOC_QuadOverdrawPS(float4 vpos : SV_Position, uint c0 : SV_Coverage)
{
	// Obtain coverage for all pixels in the quad, via 'message passing'*.
	// (* For more details, see:
	// "Shader Amortization using Pixel Quad Message Passing", Eric Penner, GPU Pro 2.)
	uint2 p = uint2(vpos.xy) & 1;
	int2 sign = p ? -1 : 1;
	uint c1 = c0 + sign.x*ddx_fine(c0);
	uint c2 = c0 + sign.y*ddy_fine(c0);
	uint c3 = c2 + sign.x*ddx_fine(c2);

	// Count the live pixels, minus 1 (zero indexing)
	uint pixelCount = c0 + c1 + c2 + c3 - 1;

	uint3 quad = uint3(vpos.xy*0.5, pixelCount);
	InterlockedAdd(overdrawUAV[quad], 1);
}

float4 ToColour(uint v)
{
	return overdrawRampColours[min(v, NUM_RAMP_COLOURS-1)];
}

float4 RENDERDOC_QOResolvePS(float4 vpos : SV_POSITION) : SV_Target0
{
	uint2 quad = vpos.xy*0.5;

	uint overdraw = 0;
	for(int i = 0; i < 4; i++)
		overdraw += overdrawSRV[uint3(quad, i)]/(i + 1);

	return ToColour(overdraw);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Above shaders courtesy of Stephen Hill (@self_shadow)
//
// http://blog.selfshadow.com/2012/11/12/counting-quads/
// https://github.com/selfshadow/demos/blob/master/QuadShading/QuadShading.fx
////////////////////////////////////////////////////////////////////////////////////////////

cbuffer cb0 : register(b0)
{
	uint4 src_coord; // x, y, mip/sample, slice

	bool multisampled;
	bool is_float;
	bool is_uint;
	bool is_int;
};

cbuffer cb1 : register(b1)
{
	uint2 dst_coord;
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

RWTexture2D<float2> copyout_depth : register(u0);
RWTexture2D<float4> copyout_float : register(u1);
RWTexture2D<uint4> copyout_uint : register(u2);
RWTexture2D<int4> copyout_int : register(u3);

[numthreads(1, 1, 1)]
void RENDERDOC_PixelHistoryUnused()
{
	copyout_depth[dst_coord.xy].rg = float2(-1.0f, -1.0f);
}

[numthreads(1, 1, 1)]
void RENDERDOC_PixelHistoryCopyPixel()
{
	if(multisampled)
	{
		if(copy_depth || copy_stencil)
		{
			float2 val = float2(copyin_depth_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)].r, -1.0f);

			if(copy_stencil) val.g = (float)copyin_stencil_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)].g;

			copyout_depth[dst_coord.xy].rg = val;
		}
		else
		{
			if(is_float)
			{
				copyout_float[dst_coord.xy] = copyin_float_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)];
			}
			else if(is_uint)
			{
				copyout_uint[dst_coord.xy] = copyin_uint_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)];
			}
			else if(is_int)
			{
				copyout_int[dst_coord.xy] = copyin_int_ms.sample[src_coord.z][uint3(src_coord.xy, src_coord.w)];
			}
		}
	}
	else
	{
		if(copy_depth || copy_stencil)
		{
			float2 val = float2(copyin_depth.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)].r, -1.0f);

			if(copy_stencil) val.g = (float)copyin_stencil.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)].g;

			copyout_depth[dst_coord.xy].rg = val;
		}
		else
		{
			if(is_float)
			{
				copyout_float[dst_coord.xy] = copyin_float.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)];
			}
			else if(is_uint)
			{
				copyout_uint[dst_coord.xy] = copyin_uint.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)];
			}
			else if(is_int)
			{
				copyout_int[dst_coord.xy] = copyin_int.mips[src_coord.z][uint3(src_coord.xy, src_coord.w)];
			}
		}
	}
}

float4 RENDERDOC_PrimitiveIDPS(uint prim : SV_PrimitiveID) : SV_Target0
{
	return asfloat(prim).xxxx;
}
