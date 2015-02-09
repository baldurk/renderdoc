/******************************************************************************
 * The MIT License (MIT)
 * 
 * Copyright (c) 2014 Baldur Karlsson
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

struct glyph
{
	vec4 posdata;
	vec4 uvdata;
};

layout (binding = 1, std140) uniform glyphdata
{
	glyph glyphs[127-32];
};

layout (binding = 2, std140) uniform stringdata
{
	uvec4 str[256];
};

out v2f
{
	vec4 tex;
	vec2 glyphuv;
} OUT;

out gl_PerVertex
{
	vec4 gl_Position;
	float gl_PointSize;
	float gl_ClipDistance[];
};

void main(void)
{
	const vec3 verts[4] = vec3[4](vec3( 0.0,  0.0, 0.5),
                                  vec3( 1.0,  0.0, 0.5),
                                  vec3( 0.0,  1.0, 0.5),
                                  vec3( 1.0,  1.0, 0.5));

	vec3 pos = verts[gl_VertexID];
	uint strindex = gl_InstanceID;
	
	vec2 charPos = vec2(strindex + pos.x + TextPosition.x, -pos.y - TextPosition.y);

	glyph G = glyphs[str[strindex].x];
	
	gl_Position = vec4(charPos.xy*2.0f*TextSize*FontScreenAspect.xy + vec2(-1, 1), 1, 1);
	OUT.glyphuv.xy = (pos.xy - G.posdata.xy) * G.posdata.zw;
	OUT.tex = G.uvdata * CharacterSize.xyxy;
}
