/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2019 Baldur Karlsson
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

varying vec4 tex;
varying vec2 glyphuv;

attribute vec2 pos;
attribute vec2 uv;
attribute float charidx;

// must match defines in gl_rendertext.cpp
#define FONT_FIRST_CHAR 32
#define FONT_LAST_CHAR 126

uniform vec4 posdata[FONT_LAST_CHAR - FONT_FIRST_CHAR + 1];
uniform vec4 uvdata[FONT_LAST_CHAR - FONT_FIRST_CHAR + 1];

void main(void)
{
  vec4 glyphposdata = posdata[int(charidx)];
  vec4 glyphuvdata = uvdata[int(charidx)];

  gl_Position = vec4(pos, 1, 1);
  glyphuv.xy = (uv.xy - glyphposdata.xy) * glyphposdata.zw;
  tex = glyphuvdata;
}
