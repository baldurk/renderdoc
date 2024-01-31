/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

layout(location = 0) out vec4 uvwa;

layout(push_constant) uniform PushData
{
  vec4 uvwa;
  vec4 ddx;
  vec4 ddy;
}
push;

void main(void)
{
  const vec4 verts[3] = vec4[3](vec4(-0.75, -0.75, 0.5, 1.0), vec4(1.25, -0.75, 0.5, 1.0),
                                vec4(-0.75, 1.25, 0.5, 1.0));

  gl_Position = verts[gl_VertexIndex];
  uvwa = push.uvwa;
  if(gl_VertexIndex == 1)
    uvwa.xyz += push.ddx.xyz;
  else if(gl_VertexIndex == 2)
    uvwa.xyz += push.ddy.xyz;
}
