/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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

layout (location = 0) out vec4 color_out;

layout (binding = 0, std140) uniform displayuniforms
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

	int   SampleIdx;
	int   NumSamples;
	vec2  Padding;
} texdisplay;

layout (binding = 1) uniform sampler2D tex;

void main(void)
{
	// calc screen co-ords with origin top left, modified by Position
	vec2 scr = gl_FragCoord.xy - texdisplay.Position.xy;

	scr /= texdisplay.Scale;

	vec2 texcoord = vec2(gl_FragCoord.xy)/512.0f.xx;
	
	if(scr.x < 0.0f || scr.y < 0.0f ||
	   scr.x > texdisplay.TextureResolutionPS.x || scr.y > texdisplay.TextureResolutionPS.y)
	{
		discard;
	}

	if (texdisplay.FlipY != 0)
		scr.y = texdisplay.TextureResolutionPS.y - scr.y;

	vec4 col = textureLod(tex, scr.xy / texdisplay.TextureResolutionPS.xy, float(texdisplay.MipLevel));

	vec4 rawcol = col;

	// RGBM encoding
	if(texdisplay.HDRMul > 0.0f)
	{
		col = vec4(col.rgb * col.a * texdisplay.HDRMul, 1.0);
	}

	vec4 pre_range_col = col;

	col = ((col - texdisplay.RangeMinimum)*texdisplay.InverseRangeSize);
	
	if(texdisplay.Channels.x < 0.5f) col.x = pre_range_col.x = 0.0f;
	if(texdisplay.Channels.y < 0.5f) col.y = pre_range_col.y = 0.0f;
	if(texdisplay.Channels.z < 0.5f) col.z = pre_range_col.z = 0.0f;
	if(texdisplay.Channels.w < 0.5f) col.w = pre_range_col.w = 1.0f;
	
	{
		// if only one channel is selected
		if(dot(texdisplay.Channels, 1.0f.xxxx) == 1.0f)
		{
			// if it's alpha, just move it into rgb
			// otherwise, select the channel that's on and replicate it across all channels
			if(texdisplay.Channels.a == 1)
				col = vec4(col.aaa, 1);
			else
				col = vec4(dot(col.rgb, 1.0f.xxx).xxx, 1.0f);
		}
	}

	color_out = (texdisplay.RawOutput != 0 ? rawcol : col);
}
