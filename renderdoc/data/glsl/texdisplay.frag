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
	bool uintTex = (OutputDisplayFormat & TEXDISPLAY_UINT_TEX) != 0;
	bool sintTex = (OutputDisplayFormat & TEXDISPLAY_SINT_TEX) != 0;
	bool depthTex = (OutputDisplayFormat & TEXDISPLAY_DEPTH_TEX) != 0;

	vec4 col;
	uvec4 ucol;
	ivec4 scol;

	// calc screen co-ords with origin top left, modified by Position
	vec2 scr = vec2(gl_FragCoord.x, OutputRes.y - gl_FragCoord.y) - Position.xy;

	scr /= Scale;

	int texType = (OutputDisplayFormat & TEXDISPLAY_TYPEMASK);

	if(texType == RESTYPE_TEX1D || texType == RESTYPE_TEX1DARRAY)
	{
		if(scr.x < 0.0f || scr.x > TextureResolutionPS.x)
		   discard;
	}
	else
	{
		if(scr.x < 0.0f || scr.y < 0.0f ||
		   scr.x > TextureResolutionPS.x || scr.y > TextureResolutionPS.y)
		   discard;
	}

	// sample the texture.
	if (uintTex)
	{
		ucol = SampleTextureUInt4(scr, texType, FlipY == 0, int(MipLevel), Slice);
	}
	else if (sintTex)
	{
		scol = SampleTextureSInt4(scr, texType, FlipY == 0, int(MipLevel), Slice);
	}
	else
	{
		col = SampleTextureFloat4(scr / Scale, OutputDisplayFormat & TEXDISPLAY_TYPEMASK, FlipY == 0, (Scale < 1.0 && MipLevel == 0.0 && !depthTex), int(MipLevel), Slice);
		col = SampleTextureFloat4(scr, texType, FlipY == 0, (Scale < 1.0 && MipLevel == 0.0 && !depthTex), int(MipLevel), Slice);
	}

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