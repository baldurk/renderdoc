/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2017 Baldur Karlsson
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

#if UINT_TEX || READ_FLOAT_WRITE_UINT
layout (location = 0) out highp uvec4 uint_out;
#else
layout (location = 0) out highp vec4 float_out;
#endif

#ifdef OPENGL_ES
// Required otherwise the shader compiler could remove the 'uv' from the vertex shader also.
layout (location = 0) in vec2 uv;
#endif

//#include "texsample.h" // while includes aren't supported in glslang, this will be added in code

void main(void)
{
	int texType = (texdisplay.OutputDisplayFormat & TEXDISPLAY_TYPEMASK);

	// calc screen co-ords with origin top left
	vec2 scr = gl_FragCoord.xy;

	scr.y = texdisplay.OutputRes.y - scr.y;

	scr /= texdisplay.TextureResolutionPS.xy;

	const int defaultFlipY = 1;

	if (texdisplay.FlipY != defaultFlipY)
		scr.y = 1.0f - scr.y;

#if UINT_TEX
	highp uvec4 uint_value = SampleTextureUInt4(texType, scr, texdisplay.Slice, texdisplay.MipLevel,
												texdisplay.SampleIdx, texdisplay.TextureResolutionPS);

	uint_out = uint_value.xxxx;
#elif READ_FLOAT_WRITE_UINT
	highp vec4 float_value = SampleTextureFloat4(texType, scr, texdisplay.Slice, texdisplay.MipLevel,
												 texdisplay.SampleIdx, texdisplay.TextureResolutionPS);

	uint_out = uvec4(float_value.xxxx * texdisplay.InverseRangeSize);
#else
	highp vec4 float_value = SampleTextureFloat4(texType, scr, texdisplay.Slice, texdisplay.MipLevel,
												 texdisplay.SampleIdx, texdisplay.TextureResolutionPS);

	float_out = float_value.xxxx;
#endif
}