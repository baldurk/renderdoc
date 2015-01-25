/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2015 Baldur Karlsson
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
 
#version 420 core

layout(triangles, invocations = 1) in;
layout(triangle_strip, max_vertices = 3) out;

in v2f
{
	vec4 secondary;
	vec4 norm;
} IN[];

out v2f
{
	vec4 secondary;
	vec4 norm;
} OUT;

uniform mat4 InvProj;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

void main()
{
    vec4 faceEdgeA = (InvProj * gl_in[1].gl_Position) - (InvProj * gl_in[0].gl_Position);
    vec4 faceEdgeB = (InvProj * gl_in[2].gl_Position) - (InvProj * gl_in[0].gl_Position);
    vec3 faceNormal = normalize( cross(faceEdgeA.xyz, faceEdgeB.xyz) );

    for(int i=0; i < 3; i++)
    {
		gl_Position = gl_in[i].gl_Position;
		OUT.secondary = IN[i].secondary;
		OUT.norm = vec4(faceNormal.xyz, 1);
        EmitVertex();
    }
    EndPrimitive();
}
