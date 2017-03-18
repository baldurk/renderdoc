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

//#extension_nongles GL_ARB_shader_image_load_store : require
 
////////////////////////////////////////////////////////////////////////////////////////////
// Below shaders courtesy of Stephen Hill (@self_shadow), converted to glsl trivially
//
// http://blog.selfshadow.com/2012/11/12/counting-quads/
// https://github.com/selfshadow/demos/blob/master/QuadShading/QuadShading.fx
////////////////////////////////////////////////////////////////////////////////////////////

layout(binding = 0, r32ui) uniform PRECISION coherent uimage2DArray overdrawImage;

#define NUM_RAMP_COLOURS 128

layout(binding = 1) uniform OverdrawRampColors
{
	vec4 colors[NUM_RAMP_COLOURS];
} ramp;

vec4 ToColour(uint v)
{
	return ramp.colors[min(v, uint(NUM_RAMP_COLOURS-1))];
}

layout (location = 0) out vec4 color_out;

void main()
{
	ivec2 quad = ivec2(gl_FragCoord.xy*0.5f);

	uint overdraw = 0u;
	for(uint i = 0u; i < 4u; i++)
		overdraw += imageLoad(overdrawImage, ivec3(quad, i)).x/(i + 1u);

	color_out = ToColour(overdraw);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Above shaders courtesy of Stephen Hill (@self_shadow), converted to glsl trivially
//
// http://blog.selfshadow.com/2012/11/12/counting-quads/
// https://github.com/selfshadow/demos/blob/master/QuadShading/QuadShading.fx
////////////////////////////////////////////////////////////////////////////////////////////
