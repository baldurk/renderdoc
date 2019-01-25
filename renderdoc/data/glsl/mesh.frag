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

// this allows overrides from outside on GL where we use name-interface matching. On vulkan we can
// default to whatever name we like

#ifndef SECONDARY_NAME
#define SECONDARY_NAME fragin_secondary
#endif

#ifndef NORM_NAME
#define NORM_NAME fragin_norm
#endif

IO_LOCATION(0) in vec4 SECONDARY_NAME;
IO_LOCATION(1) in vec4 NORM_NAME;

IO_LOCATION(0) out vec4 color_out;

void main(void)
{
  int type = Mesh.displayFormat;

  if(type == MESHDISPLAY_SECONDARY)
  {
    color_out = vec4(SECONDARY_NAME.xyz, 1);
  }
  else if(type == MESHDISPLAY_SECONDARY_ALPHA)
  {
    color_out = vec4(SECONDARY_NAME.www, 1);
  }
  else if(type == MESHDISPLAY_FACELIT)
  {
    vec3 lightDir = normalize(vec3(0, -0.3f, -1));

    color_out = vec4(Mesh.color.xyz * abs(dot(lightDir, NORM_NAME.xyz)), 1);
  }
  else    // if(type == MESHDISPLAY_SOLID)
  {
    color_out = vec4(Mesh.color.xyz, 1);
  }
}
