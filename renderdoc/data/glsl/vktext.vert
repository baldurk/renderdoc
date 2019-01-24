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

#define FONT_UBO

#include "glsl_ubos.h"

layout(location = 0) out vec4 tex;
layout(location = 1) out vec2 glyphuv;

void main(void)
{
  const vec3 verts[6] = vec3[6](vec3(0.0, 0.0, 0.5), vec3(1.0, 0.0, 0.5), vec3(0.0, 1.0, 0.5),

                                vec3(1.0, 0.0, 0.5), vec3(0.0, 1.0, 0.5), vec3(1.0, 1.0, 0.5));

  uint vert = uint(gl_VertexIndex) % 6u;

  vec3 pos = verts[vert];
  uint strindex = uint(gl_VertexIndex) / 6u;

  vec2 charPos =
      vec2(float(strindex) + pos.x + general.TextPosition.x, pos.y + general.TextPosition.y);

  FontGlyphData G = glyphs.data[str.chars[strindex].x];

  gl_Position =
      vec4(charPos.xy * 2.0f * general.TextSize * general.FontScreenAspect.xy + vec2(-1, -1), 1, 1);
  glyphuv.xy = (pos.xy - G.posdata.xy) * G.posdata.zw;
  tex = G.uvdata * general.CharacterSize.xyxy;
}
