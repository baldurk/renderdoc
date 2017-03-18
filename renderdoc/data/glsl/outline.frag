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

void main(void)
{
	vec4 ret = outline.Inner_Color;

	vec2 rectPos = gl_FragCoord.xy - outline.ViewRect.xy;
	vec2 rectSize = outline.ViewRect.zw;
 
	vec2 ab = mod(rectPos.xy, vec2(32.0f));

	bool checkerVariant = (
			(ab.x < 16.0f && ab.y < 16.0f) ||
			(ab.x > 16.0f && ab.y > 16.0f)
		);

	if(outline.Scissor == 0u)
	{
		if(rectPos.x < 3.0f || rectPos.x > rectSize.x - 3.0f ||
		   rectPos.y < 3.0f || rectPos.y > rectSize.y - 3.0f)
		{
			ret = outline.Border_Color;
		}
	}
	else
	{
		if(rectPos.x < 3.0f || rectPos.x > rectSize.x - 3.0f ||
		   rectPos.y < 3.0f || rectPos.y > rectSize.y - 3.0f)
		{
			ret = checkerVariant ? vec4(1, 1, 1, 1) : vec4(0, 0, 0, 1);
		}
		else
		{
			ret = vec4(0, 0, 0, 0);
		}
	}

	color_out = ret;
}
