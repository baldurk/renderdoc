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

//#extension_gles GL_EXT_geometry_shader : enable
//#extension_gles GL_OES_geometry_shader : enable

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout (location = 0) in vec4 IN_secondary[3];
layout (location = 1) in vec4 IN_norm[3];

layout (location = 0) out float OUT_pixarea;

#ifndef OPENGL_ES
in gl_PerVertex
{
  vec4 gl_Position;
  float gl_PointSize;
} gl_in[];

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
};
#endif

layout(binding = 2) uniform ViewportSizeUBO
{
	vec4 size;
} viewport;

void main()
{
	vec2 a = gl_in[0].gl_Position.xy / gl_in[0].gl_Position.w;
	vec2 b = gl_in[1].gl_Position.xy / gl_in[1].gl_Position.w;
	vec2 c = gl_in[2].gl_Position.xy / gl_in[2].gl_Position.w;

	a = (a * 0.5f + 0.5f) * viewport.size.xy;
	b = (b * 0.5f + 0.5f) * viewport.size.xy;
	c = (c * 0.5f + 0.5f) * viewport.size.xy;

	float ab = length(a - b);
	float bc = length(b - c);
	float ca = length(c - a);

	float s = (ab + bc + ca) / 2.0f;

	float area = sqrt( s * (s - ab) * (s - bc) * (s - ca) );

    for(int i=0; i < 3; i++)
    {
		gl_Position = gl_in[i].gl_Position;
		OUT_pixarea = area;
        EmitVertex();
    }
    EndPrimitive();
}
