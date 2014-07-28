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

#version 420 core

layout (binding = 0, std140) uniform texdisplay
{
  vec2  Position;
  float Scale;
  float HDRMul;

  vec4  Channels;

  float RangeMinimum;
  float InverseRangeSize;
  float MipLevel;
  int   FlipY;

  vec3  TextureResolutionPS;
  int   OutputDisplayFormat;

  vec2  OutputRes;
  int   RawOutput;
  float Slice;
};

layout (binding = 0) uniform sampler2D tex0;
layout (location = 0) out vec4 color_out;

void main(void)
{
	// calc screen co-ords with origin top left, modified by Position
	vec2 scr = vec2(gl_FragCoord.x, OutputRes.y - gl_FragCoord.y) - Position.xy;

	// calc UVs in texture
	vec2 uv = scr/(textureSize(tex0,0)*Scale);

	// discard if we're rendering outside input texture
	if(uv.x < 0 || uv.y < 0 || uv.x > 1 || uv.y > 1) discard;

	// sample the texture.
	// TODO: handle uint/sint/float textures (OutputDisplayFormat).
	// TODO: Use MipLevel and Slice parameters
	// TODO: Sample from a point or linear sampler depending on if we're
	//       upscaling (point) or downscaling mip 0 (linear)
	vec4 col = texture(tex0, vec2(uv.x, FlipY > 0 ? uv.y : 1.0f-uv.y));

	if(RawOutput != 0)
	{
		color_out = col;
		return;
	}

	// RGBM encoding
	if(HDRMul > 0.0f)
	{
		col = vec4(col.rgb * col.a * HDRMul, 1.0f);
	}

	col = ((col - RangeMinimum)*InverseRangeSize);

	col = mix(vec4(0,0,0,1), col, Channels);

	// TODO: check OutputDisplayFormat to see if we should highlight NaNs or clipping
	// else
	{
		// if only one channel is selected
		if(dot(Channels, 1.0f.xxxx) == 1.0f.xxxx)
		{
			// if it's alpha, just move it into rgb
			// otherwise, select the channel that's on and replicate it across all channels
			if(Channels.a == 1)
				col = vec4(col.aaa, 1);
			else
				col = vec4(dot(col.rgb, 1.0f.xxx).xxx, 1.0f);
		}
	}

	// TODO: Check OutputDisplayFormat for SRGB handling
	// TODO: Figure out SRGB in opengl at all :)

	color_out = col;
}