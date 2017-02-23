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

layout (binding = 0, std140) uniform checkeruniforms
{
    vec4 lightCol;
    vec4 darkCol;
} checker;

void main(void)
{
	vec2 ab = mod(gl_FragCoord.xy, vec2(128.0f));

	if(
		(ab.x < 64.0f && ab.y < 64.0f) ||
		(ab.x > 64.0f && ab.y > 64.0f)
		)
	{
		color_out = vec4(checker.darkCol.rgb*checker.darkCol.rgb, 1);
	}
	else
	{
		color_out = vec4(checker.lightCol.rgb*checker.lightCol.rgb, 1);
	}
}
