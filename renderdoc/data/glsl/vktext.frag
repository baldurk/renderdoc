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

layout(binding = 3) uniform sampler2D tex0;

layout(location = 0) out vec4 color_out;

layout(location = 0) in vec4 tex;
layout(location = 1) in vec2 glyphuv;

void main(void)
{
  float text = 0.0f;

  if(glyphuv.x >= 0.0f && glyphuv.x <= 1.0f && glyphuv.y >= 0.0f && glyphuv.y <= 1.0f)
  {
    vec2 uv;
    uv.x = mix(tex.x, tex.z, glyphuv.x);
    uv.y = mix(tex.y, tex.w, glyphuv.y);
    text = texture(tex0, uv.xy).x;
  }

  color_out = vec4(vec3(text), clamp(text + 0.5f, 0.0f, 1.0f));
}
