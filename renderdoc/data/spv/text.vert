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

layout (binding = 0, std140) uniform fontuniforms
{
	vec2  TextPosition;
	float txtpadding;
	float TextSize;

	vec2  CharacterSize;
	vec2  FontScreenAspect;
} general;

struct glyph
{
	vec4 posdata;
	vec4 uvdata;
};

layout (binding = 1, std140) uniform glyphdata
{
	glyph data[127-32];
} glyphs;

layout (binding = 2, std140) uniform stringdata
{
	uvec4 chars[256];
} str;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

out v2f
{
	vec4 tex;
	vec2 glyphuv;
} OUT;

void main(void)
{
	// VKTODOMED temporarily drawing lists
    const vec3 verts[6] = vec3[6](vec3( 0.0,  0.0, 0.5),
                                  vec3( 1.0,  0.0, 0.5),
                                  vec3( 0.0,  1.0, 0.5),

                                  vec3( 1.0,  0.0, 0.5),
                                  vec3( 0.0,  1.0, 0.5),
                                  vec3( 1.0,  1.0, 0.5));

	vec3 pos = verts[gl_VertexID%6];
	uint strindex = (gl_VertexID/6);
	
	vec2 charPos = vec2(strindex + pos.x + general.TextPosition.x, pos.y + general.TextPosition.y);
	
	gl_Position = vec4(charPos.xy*2.0f*general.TextSize*general.FontScreenAspect.xy + vec2(-1, -1), 1, 1);

	uint chars[20] = uint[20](0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19);

	glyph G = glyphs.data[ str.chars[strindex].x ];
	
	OUT.glyphuv.xy = (pos.xy - G.posdata.xy) * G.posdata.zw;
	OUT.tex = G.uvdata * general.CharacterSize.xyxy;

	const vec3 cols[5] = vec3[5](vec3(0, 0, 0),
								 vec3(1, 0, 0),
								 vec3(0, 1, 0),
								 vec3(0, 0, 1),
								 vec3(1, 0, 1));
}