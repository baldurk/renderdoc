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

#if defined(OPENGL_ES)
#extension GL_EXT_geometry_shader : enable
#extension GL_OES_geometry_shader : enable
#extension GL_EXT_geometry_point_size : enable
#endif

#define MESH_UBO

#include "glsl_ubos.h"

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

IO_LOCATION(0) in vec4 vsout_secondary[3];
IO_LOCATION(1) in vec4 vsout_norm[3];

IO_LOCATION(0) out vec4 gsout_secondary;
IO_LOCATION(1) out vec4 gsout_norm;

void main()
{
  vec4 faceEdgeA = (Mesh.invProj * gl_in[1].gl_Position) - (Mesh.invProj * gl_in[0].gl_Position);
  vec4 faceEdgeB = (Mesh.invProj * gl_in[2].gl_Position) - (Mesh.invProj * gl_in[0].gl_Position);
  vec3 faceNormal = normalize(cross(faceEdgeA.xyz, faceEdgeB.xyz));

  for(int i = 0; i < 3; i++)
  {
    gl_Position = gl_in[i].gl_Position;
    gsout_secondary = vsout_secondary[i];
    gsout_norm = vec4(faceNormal.xyz, 1);
    EmitVertex();
  }
  EndPrimitive();
}
