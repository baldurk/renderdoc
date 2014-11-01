/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2014 Baldur Karlsson
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

layout (binding = 0) uniform sampler2D tex0;
layout (location = 0) out vec4 color_out;

in v2f
{
	vec4 tex;
	vec2 glyphuv;
} IN;

void main(void)
{
	float text = 0;

	if(IN.glyphuv.x >= 0.0f && IN.glyphuv.x <= 1.0f && 
	   IN.glyphuv.y >= 0.0f && IN.glyphuv.y <= 1.0f)
	{
		vec2 uv;
		uv.x = mix(IN.tex.x, IN.tex.z, IN.glyphuv.x);
		uv.y = mix(IN.tex.y, IN.tex.w, IN.glyphuv.y);
		text = texture2D(tex0, uv.xy).x;
	}

	color_out = vec4(text.xxx, clamp(text + 0.5f, 0.0f, 1.0f));
}
