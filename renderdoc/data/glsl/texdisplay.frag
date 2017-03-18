/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015-2017 Baldur Karlsson
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
#ifdef OPENGL_ES
// Required otherwise the shader compiler could remove the 'uv' from the vertex shader also.
layout (location = 0) in vec2 uv;
#endif

//#extension_gles GL_EXT_texture_cube_map_array : enable
//#extension_gles GL_EXT_texture_buffer : enable
//#extension_nongles GL_ARB_gpu_shader5 : enable

//#include "texsample.h" // while includes aren't supported in glslang, this will be added in code

float ConvertSRGBToLinear(float srgb)
{
	if (srgb <= 0.04045f)
		return srgb / 12.92f;
	else
		return pow(( clamp(srgb, 0.0f, 1.0f) + 0.055f) / 1.055f, 2.4f);
}

void main(void)
{
#ifdef VULKAN // vulkan combines all three types
	bool uintTex = (texdisplay.OutputDisplayFormat & TEXDISPLAY_UINT_TEX) != 0;
	bool sintTex = (texdisplay.OutputDisplayFormat & TEXDISPLAY_SINT_TEX) != 0;
#else // OPENGL

#if UINT_TEX
	const bool uintTex = true;
	const bool sintTex = false;
#elif SINT_TEX
	const bool uintTex = false;
	const bool sintTex = true;
#else
	const bool uintTex = false;
	const bool sintTex = false;
#endif

#endif

	int texType = (texdisplay.OutputDisplayFormat & TEXDISPLAY_TYPEMASK);

	vec4 col;
	uvec4 ucol;
	ivec4 scol;

	// calc screen co-ords with origin top left, modified by Position
	vec2 scr = gl_FragCoord.xy;

#ifdef OPENGL
	scr.y = texdisplay.OutputRes.y - scr.y;
#endif

	scr -= texdisplay.Position.xy;

	scr /= texdisplay.TextureResolutionPS.xy;

	scr /= texdisplay.Scale;

	scr /= vec2(texdisplay.MipShift, texdisplay.MipShift);
	vec2 scr2 = scr;

#ifdef VULKAN
	if(texType == RESTYPE_TEX1D)
#else
	if(texType == RESTYPE_TEX1D || texType == RESTYPE_TEXBUFFER || texType == RESTYPE_TEX1DARRAY)
#endif
	{
		// by convention display 1D textures as 100 high
		if(scr2.x < 0.0f || scr2.x > 1.0f || scr2.y < 0.0f || scr2.y > 100.0f)
		   discard;
	}
	else
	{
		if(scr2.x < 0.0f || scr2.y < 0.0f ||
		   scr2.x > 1.0f || scr2.y > 1.0f)
		{
			discard;
		}
	}

#ifdef VULKAN
	const int defaultFlipY = 0;
#else // OPENGL
	const int defaultFlipY = 1;
#endif

	if (texdisplay.FlipY != defaultFlipY)
		scr.y = 1.0f - scr.y;

	if(uintTex)
	{
		ucol = SampleTextureUInt4(texType, scr, texdisplay.Slice, texdisplay.MipLevel,
		                          texdisplay.SampleIdx, texdisplay.TextureResolutionPS);
	}
	else if(sintTex)
	{
		scol = SampleTextureSInt4(texType, scr, texdisplay.Slice, texdisplay.MipLevel,
		                          texdisplay.SampleIdx, texdisplay.TextureResolutionPS);
	}
	else
	{
		col = SampleTextureFloat4(texType, scr, texdisplay.Slice, texdisplay.MipLevel,
		                          texdisplay.SampleIdx, texdisplay.TextureResolutionPS);
	}
	
	if(texdisplay.RawOutput != 0)
	{
#ifdef GL_ARB_gpu_shader5
		if (uintTex)
			color_out = uintBitsToFloat(ucol);
		else if (sintTex)
			color_out = intBitsToFloat(scol);
		else
			color_out = col;
#else
		// without being able to alias bits, we won't get accurate results.
		// a cast is better than nothing though
		if (uintTex)
			color_out = vec4(ucol);
		else if (sintTex)
			color_out = vec4(scol);
		else
			color_out = col;
#endif
		return;
	}

	// RGBM encoding
	if(texdisplay.HDRMul > 0.0f)
	{
		if (uintTex)
			col = vec4(ucol.rgb * ucol.a * uint(texdisplay.HDRMul), 1.0);
		else if (sintTex)
			col = vec4(scol.rgb * scol.a * int(texdisplay.HDRMul), 1.0);
		else
			col = vec4(col.rgb * col.a * texdisplay.HDRMul, 1.0);
	}
	
	if (uintTex)
		col = vec4(ucol);
	else if (sintTex)
		col = vec4(scol);

	vec4 pre_range_col = col;

	col = ((col - texdisplay.RangeMinimum)*texdisplay.InverseRangeSize);
	
	if(texdisplay.Channels.x < 0.5f) col.x = pre_range_col.x = 0.0f;
	if(texdisplay.Channels.y < 0.5f) col.y = pre_range_col.y = 0.0f;
	if(texdisplay.Channels.z < 0.5f) col.z = pre_range_col.z = 0.0f;
	if(texdisplay.Channels.w < 0.5f) col.w = pre_range_col.w = 1.0f;
	
	if((texdisplay.OutputDisplayFormat & TEXDISPLAY_NANS) > 0)
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

		if(pre_range_col.r < 0.0f || pre_range_col.g < 0.0f || pre_range_col.b < 0.0f || pre_range_col.a < 0.0f)
		{
		   color_out = vec4(0, 0, 1, 1);
		   return;
		}
		
		col = vec4(vec3(dot(col.xyz, vec3(0.2126, 0.7152, 0.0722))), 1);
	}
	else if((texdisplay.OutputDisplayFormat & TEXDISPLAY_CLIPPING) > 0)
	{
		if(col.r < 0.0f || col.g < 0.0f || col.b < 0.0f || col.a < 0.0f)
		{
		   color_out = vec4(1, 0, 0, 1);
		   return;
		}

		if(col.r > (1.0f+FLT_EPSILON) || col.g > (1.0f+FLT_EPSILON) || col.b > (1.0f+FLT_EPSILON) || col.a > (1.0f+FLT_EPSILON))
		{
		   color_out = vec4(0, 1, 0, 1);
		   return;
		}
		
		col = vec4(vec3(dot(col.xyz, vec3(0.2126, 0.7152, 0.0722))), 1);
	}
	else
	{
		// if only one channel is selected
		if(dot(texdisplay.Channels, vec4(1.0f)) == 1.0f)
		{
			// if it's alpha, just move it into rgb
			// otherwise, select the channel that's on and replicate it across all channels
			if(texdisplay.Channels.a == 1.0f)
				col = vec4(col.aaa, 1);
			else
				col = vec4(vec3(dot(col.rgb, vec3(1.0f))), 1.0f);
		}
	}
	
	if((texdisplay.OutputDisplayFormat & TEXDISPLAY_GAMMA_CURVE) > 0)
	{
		col.rgb = vec3(ConvertSRGBToLinear(col.r), ConvertSRGBToLinear(col.g), ConvertSRGBToLinear(col.b));
	}

	color_out = col;
}
