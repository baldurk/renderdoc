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

#define MESH_UBO

#include "glsl_ubos.h"

// this allows overrides from outside to change to e.g. dvec4

#ifndef POSITION_TYPE
#define POSITION_TYPE vec4
#endif

#ifndef SECONDARY_TYPE
#define SECONDARY_TYPE vec4
#endif

IO_LOCATION(0) in POSITION_TYPE vsin_position;
IO_LOCATION(1) in SECONDARY_TYPE vsin_secondary;

IO_LOCATION(0) out vec4 vsout_secondary;
IO_LOCATION(1) out vec4 vsout_norm;

void main(void)
{
  vec2 psprite[4] =
      vec2[](vec2(-1.0f, -1.0f), vec2(-1.0f, 1.0f), vec2(1.0f, -1.0f), vec2(1.0f, 1.0f));

  vec4 pos = vec4(vsin_position);
  if(Mesh.homogenousInput == 0u)
  {
    pos = vec4(pos.xyz, 1);
  }
  else
  {
#ifdef VULKAN
    pos = vec4(pos.x, -pos.y, pos.z, pos.w);
#endif
  }

  gl_Position = Mesh.mvp * pos;
  gl_Position.xy += Mesh.pointSpriteSize.xy * 0.01f * psprite[VERTEX_ID % 4] * gl_Position.w;
  vsout_secondary = vec4(vsin_secondary);
  vsout_norm = vec4(0, 0, 1, 1);

#ifdef VULKAN
  // GL->VK conventions
  gl_Position.y = -gl_Position.y;
  if(Mesh.rawoutput == 0)
  {
    gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
  }

  gl_PointSize = 4.0f;
#endif
}
