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

layout (location = 0) out vec4 color_out;

void main(void)
{
#if UINT_TEX
	bool uintTex = true;
	bool sintTex = false;
#elif SINT_TEX
	bool uintTex = false;
	bool sintTex = true;
#else
	bool uintTex = false;
	bool sintTex = false;
#endif

	vec4 col;
	uvec4 ucol;
	ivec4 scol;

	// calc screen co-ords with origin top left, modified by Position
	vec2 scr = vec2(gl_FragCoord.x, OutputRes.y - gl_FragCoord.y) - Position.xy;

	scr /= Scale;

	int texType = (OutputDisplayFormat & TEXDISPLAY_TYPEMASK);

	if(texType == RESTYPE_TEX1D || texType == RESTYPE_TEXBUFFER || texType == RESTYPE_TEX1DARRAY)
	{
		// by convention display 1D textures as 100 high
		if(scr.x < 0.0f || scr.x > TextureResolutionPS.x || scr.y < 0.0f || scr.y > 100.0f)
		   discard;
	}
	else
	{
		if(scr.x < 0.0f || scr.y < 0.0f ||
		   scr.x > TextureResolutionPS.x || scr.y > TextureResolutionPS.y)
		   discard;
	}

	// sample the texture.
#if UINT_TEX
		ucol = SampleTextureUInt4(scr, texType, FlipY == 0, int(MipLevel), Slice, SampleIdx);
#elif SINT_TEX
		scol = SampleTextureSInt4(scr, texType, FlipY == 0, int(MipLevel), Slice, SampleIdx);
#else
		col = SampleTextureFloat4(scr, texType, FlipY == 0, int(MipLevel), Slice, SampleIdx, NumSamples);
#endif

	if(RawOutput != 0)
	{
		if (uintTex)
			color_out = uintBitsToFloat(ucol);
		else if (sintTex)
			color_out = intBitsToFloat(scol);
		else
			color_out = col;
		return;
	}

	// RGBM encoding
	if(HDRMul > 0.0f)
	{
		if (uintTex)
			col = vec4(ucol.rgb * ucol.a * uint(HDRMul), 1.0);
		else if (sintTex)
			col = vec4(scol.rgb * scol.a * int(HDRMul), 1.0);
		else
			col = vec4(col.rgb * col.a * HDRMul, 1.0);
	}

	if (uintTex)
		col = vec4(ucol);
	else if (sintTex)
		col = vec4(scol);

	vec4 pre_range_col = col;

	col = ((col - RangeMinimum)*InverseRangeSize);
	
	if(Channels.x < 0.5f) col.x = pre_range_col.x = 0.0f;
	if(Channels.y < 0.5f) col.y = pre_range_col.y = 0.0f;
	if(Channels.z < 0.5f) col.z = pre_range_col.z = 0.0f;
	if(Channels.w < 0.5f) col.w = pre_range_col.w = 1.0f;
	
	// show nans, infs and negatives
	if((OutputDisplayFormat & TEXDISPLAY_NANS) > 0)
	{
		if(isnan(pre_range_col.r) || isnan(pre_range_col.g) || isnan(pre_range_col.b) || isnan(pre_range_col.a))
		{
		   color_out = vec4(1, 0, 0, 1);
		   return;
		}
		   
		if(isinf(pre_range_col.r) || isinf(pre_range_col.g) || isinf(pre_range_col.b) || isinf(pre_range_col.a))
		{
		   color_out = vec4(0, 1, 0, 1);
		   return;
		}

		if(pre_range_col.r < 0 || pre_range_col.g < 0 || pre_range_col.b < 0 || pre_range_col.a < 0)
		{
		   color_out = vec4(0, 0, 1, 1);
		   return;
		}
		
		col = vec4(dot(col.xyz, vec3(0.2126, 0.7152, 0.0722)).xxx, 1);
	}
	else if((OutputDisplayFormat & TEXDISPLAY_CLIPPING) > 0)
	{
		if(col.r < 0 || col.g < 0 || col.b < 0 || col.a < 0)
		{
		   color_out = vec4(1, 0, 0, 1);
		   return;
		}

		if(col.r > (1+FLT_EPSILON) || col.g > (1+FLT_EPSILON) || col.b > (1+FLT_EPSILON) || col.a > (1+FLT_EPSILON))
		{
		   color_out = vec4(0, 1, 0, 1);
		   return;
		}
		
		col = vec4(dot(col.xyz, vec3(0.2126, 0.7152, 0.0722)).xxx, 1);
	}
	else
	{
		// if only one channel is selected
		if(dot(Channels, 1.0f.xxxx) == 1.0f)
		{
			// if it's alpha, just move it into rgb
			// otherwise, select the channel that's on and replicate it across all channels
			if(Channels.a == 1)
				col = vec4(col.aaa, 1);
			else
				col = vec4(dot(col.rgb, 1.0f.xxx).xxx, 1.0f);
		}
	}
	
	if((OutputDisplayFormat & TEXDISPLAY_GAMMA_CURVE) > 0)
	{
		col.rgb = pow(clamp(col.rgb, 0.0f.xxx, 1.0f.xxx), 2.2f.xxx);
	}

	color_out = col;
}