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

layout (binding = 1) uniform sampler1D tex1D;
layout (binding = 2) uniform sampler2D tex2D;
layout (binding = 3) uniform sampler3D tex3D;
layout (binding = 4) uniform samplerCube texCube;
layout (binding = 5) uniform sampler1DArray tex1DArray;
layout (binding = 6) uniform sampler2DArray tex2DArray;
layout (binding = 7) uniform samplerCubeArray texCubeArray;

layout (binding = 9) uniform usampler1D texUInt1D;
layout (binding = 10) uniform usampler2D texUInt2D;
layout (binding = 11) uniform usampler3D texUInt3D;
layout (binding = 13) uniform usampler1DArray texUInt1DArray;
layout (binding = 14) uniform usampler2DArray texUInt2DArray;

layout (binding = 16) uniform isampler1D texSInt1D;
layout (binding = 17) uniform isampler2D texSInt2D;
layout (binding = 18) uniform isampler3D texSInt3D;
layout (binding = 20) uniform isampler1DArray texSInt1DArray;
layout (binding = 21) uniform isampler2DArray texSInt2DArray;

layout (location = 0) out vec4 color_out;

vec3 CalcCubeCoord(vec2 uv, int face)
{
	// Map UVs to [-0.5, 0.5] and rotate
	uv -= vec2(0.5);
	vec3 coord;
	if (face == CUBEMAP_FACE_POS_X)
		coord = vec3(0.5, uv.y, -uv.x);
	else if (face == CUBEMAP_FACE_NEG_X)
		coord = vec3(-0.5, -uv.y, uv.x);
	else if (face == CUBEMAP_FACE_POS_Y)
		coord = vec3(uv.x, 0.5, uv.y);
	else if (face == CUBEMAP_FACE_NEG_Y)
		coord = vec3(uv.x, -0.5, -uv.y);
	else if (face == CUBEMAP_FACE_POS_Z)
		coord = vec3(uv.x, -uv.y, 0.5);
	else // face == CUBEMAP_FACE_NEG_Z
		coord = vec3(-uv.x, -uv.y, -0.5);
	return coord;
}

uvec4 SampleTextureUInt4(vec2 pos, int type, bool flipY, int mipLevel, float slice)
{
	uvec4 col;
	if (type == RESTYPE_TEX1D)
	{
		int size = textureSize(texUInt1D, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size) discard;

		col = texelFetch(texUInt1D, int(pos.x), mipLevel);
	}
	else if (type == RESTYPE_TEX1DARRAY)
	{
		ivec2 size = textureSize(texUInt1DArray, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x) discard;

		col = texelFetch(texUInt1DArray, ivec2(pos.x, slice), mipLevel);
	}
	else if (type == RESTYPE_TEX2D)
	{
		ivec2 size = textureSize(texUInt2D, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x || pos.y > size.y) discard;

		if (flipY)
			pos.y = size.y - pos.y;

		col = texelFetch(texUInt2D, ivec2(pos), mipLevel);
	}
	else if (type == RESTYPE_TEX2DARRAY)
	{
		ivec3 size = textureSize(texUInt2DArray, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x || pos.y > size.y) discard;
		
		if (flipY)
			pos.y = size.y - pos.y;

		col = texelFetch(texUInt2DArray, ivec3(pos, slice), mipLevel);
	}
	else // if (type == RESTYPE_TEX3D)
	{
		ivec3 size = textureSize(texUInt3D, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x || pos.y > size.y) discard;
		
		if (flipY)
			pos.y = size.y - pos.y;

		col = texelFetch(texUInt3D, ivec3(pos, slice), mipLevel);
	}
	
	return col;
}

ivec4 SampleTextureSInt4(vec2 pos, int type, bool flipY, int mipLevel, float slice)
{
	ivec4 col;
	if (type == RESTYPE_TEX1D)
	{
		int size = textureSize(texSInt1D, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size) discard;

		col = texelFetch(texSInt1D, int(pos.x), mipLevel);
	}
	else if (type == RESTYPE_TEX1DARRAY)
	{
		ivec2 size = textureSize(texSInt1DArray, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x) discard;

		col = texelFetch(texSInt1DArray, ivec2(pos.x, slice), mipLevel);
	}
	else if (type == RESTYPE_TEX2D)
	{
		ivec2 size = textureSize(texSInt2D, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x || pos.y > size.y) discard;

		if (flipY)
			pos.y = size.y - pos.y;

		col = texelFetch(texSInt2D, ivec2(pos), mipLevel);
	}
	else if (type == RESTYPE_TEX2DARRAY)
	{
		ivec3 size = textureSize(texSInt2DArray, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x || pos.y > size.y) discard;
		
		if (flipY)
			pos.y = size.y - pos.y;

		col = texelFetch(texSInt2DArray, ivec3(pos, slice), mipLevel);
	}
	else // if (type == RESTYPE_TEX3D)
	{
		ivec3 size = textureSize(texSInt3D, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x || pos.y > size.y) discard;
		
		if (flipY)
			pos.y = size.y - pos.y;

		col = texelFetch(texSInt3D, ivec3(pos, slice), mipLevel);
	}
	
	return col;
}


vec4 SampleTextureFloat4(vec2 pos, int type, bool flipY, bool linearSample, int mipLevel, float slice)
{
	vec4 col;
	if (type == RESTYPE_TEX1D)
	{
		int size = textureSize(tex1D, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size) discard;

		if (linearSample)
			col = texture(tex1D, pos.x / size);
		else
			col = texelFetch(tex1D, int(pos.x), mipLevel);
	}
	else if (type == RESTYPE_TEX1DARRAY)
	{
		ivec2 size = textureSize(tex1DArray, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x) discard;

		if (linearSample)
			col = texture(tex1DArray, vec2(pos.x / size.x, slice));
		else
			col = texelFetch(tex1DArray, ivec2(pos.x, slice), mipLevel);
	}
	else if (type == RESTYPE_TEX2D)
	{
		ivec2 size = textureSize(tex2D, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x || pos.y > size.y) discard;

		if (flipY)
			pos.y = size.y - pos.y;

		if (linearSample)
			col = texture(tex2D, pos / size);
		else
			col = texelFetch(tex2D, ivec2(pos), mipLevel);
	}
	else if (type == RESTYPE_TEX2DARRAY)
	{
		ivec3 size = textureSize(tex2DArray, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x || pos.y > size.y) discard;

		if (flipY)
			pos.y = size.y - pos.y;

		if (linearSample)
			col = texture(tex2DArray, vec3(pos / size.xy, slice));
		else
			col = texelFetch(tex2DArray, ivec3(pos, slice), mipLevel);
	}
	else if (type == RESTYPE_TEX3D)
	{
		ivec3 size = textureSize(tex3D, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x || pos.y > size.y) discard;

		if (flipY)
			pos.y = size.y - pos.y;

		if (linearSample)
			col = texture(tex3D, vec3(pos / size.xy, slice));
		else
			col = texelFetch(tex3D, ivec3(pos, slice), mipLevel);
	}
	else if (type == RESTYPE_TEXCUBE)
	{
		ivec2 size = textureSize(texCube, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x || pos.y > size.y) discard;

		if (flipY)
			pos.y = size.y - pos.y;

		vec3 cubeCoord = CalcCubeCoord(pos / size, int(slice));

		if (linearSample)
			col = texture(texCube, cubeCoord);
		else
			col = textureLod(texCube, cubeCoord, mipLevel);
	}
	else // type == RESTYPE_TEXCUBEARRAY
	{
		ivec3 size = textureSize(texCubeArray, mipLevel);
		if (pos.x < 0 || pos.y < 0 || pos.x > size.x || pos.y > size.y) discard;

		if (flipY)
			pos.y = size.y - pos.y;

		vec3 cubeCoord = CalcCubeCoord(pos / size.xy, int(slice) % 6);
		vec4 arrayCoord = vec4(cubeCoord, int(Slice) / 6);

		if (linearSample)
			col = texture(texCubeArray, arrayCoord);
		else
			col = textureLod(texCubeArray, arrayCoord, mipLevel);
	}
	
	return col;
}

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

	// sample the texture.
	if (uintTex)
	{
		ucol = SampleTextureUInt4(scr / Scale, OutputDisplayFormat & TEXDISPLAY_TYPEMASK, FlipY == 0, int(MipLevel), Slice);
	}
	else if (sintTex)
	{
		scol = SampleTextureSInt4(scr / Scale, OutputDisplayFormat & TEXDISPLAY_TYPEMASK, FlipY == 0, int(MipLevel), Slice);
	}
	else
	{
		col = SampleTextureFloat4(scr / Scale, OutputDisplayFormat & TEXDISPLAY_TYPEMASK, FlipY == 0, (Scale < 1.0 && MipLevel == 0.0 && !depthTex), int(MipLevel), Slice);
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