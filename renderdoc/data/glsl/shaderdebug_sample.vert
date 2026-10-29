/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2026 Baldur Karlsson
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

#include "glsl_globals.h"

#ifdef VULKAN
layout(location = 0)
#endif
    out vec4 v2f_uvwa;

#ifdef VULKAN

layout(push_constant) uniform PushData
{
  vec4 uvwa;
  vec4 ddx;
  vec4 ddy;
}
push;

#define in_uvwa (push.uvwa)
#define in_ddx (push.ddx)
#define in_ddy (push.ddy)

#else

uniform vec4 in_uvwa;
uniform vec4 in_ddx;
uniform vec4 in_ddy;

#endif

void main(void)
{
#ifdef VULKAN
  const vec4 verts[3] = vec4[3](vec4(-0.75, -0.75, 0.5, 1.0), vec4(1.25, -0.75, 0.5, 1.0),
                                vec4(-0.75, 1.25, 0.5, 1.0));
#else
  const vec4 verts[3] =
      vec4[3](vec4(-0.75, 0.75, 0.5, 1.0), vec4(1.25, 0.75, 0.5, 1.0), vec4(-0.75, -1.25, 0.5, 1.0));
#endif

  gl_Position = verts[VERTEX_ID];
  v2f_uvwa = in_uvwa;
  if(VERTEX_ID == 1)
    v2f_uvwa.xyz += in_ddx.xyz;
  else if(VERTEX_ID == 2)
    v2f_uvwa.xyz += in_ddy.xyz;
}
