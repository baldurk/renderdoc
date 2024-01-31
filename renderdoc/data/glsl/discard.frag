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

#define PATTERN_WIDTH 64
#define PATTERN_HEIGHT 8

#ifdef VULKAN

#define FRAG_OUT(loc) layout(location = loc)

layout(set = 0, binding = 0, std140) uniform DiscardUBOData
{
  vec4 floatpattern[(PATTERN_WIDTH * PATTERN_HEIGHT) / 4];
  uvec4 intpattern[(PATTERN_WIDTH * PATTERN_HEIGHT) / 4];
}
Pattern;

layout(push_constant) uniform PushData
{
  uint flags;
};

#else    // !VULKAN

#if defined(OPENGL_ES)

// GLES requires shader specification of locations
#define FRAG_OUT(loc) layout(location = loc)

precision highp float;
precision highp int;

#else

// desktop GLSL requires a newer version or extensions for shader locations, but we can call
// glBindFragDataLocation
#define FRAG_OUT(loc)

#endif

// for GLES compatibility where we must match blit.vert
in vec2 uv;

uniform DiscardUBOData
{
  vec4 floatpattern[(PATTERN_WIDTH * PATTERN_HEIGHT) / 4];
  uvec4 intpattern[(PATTERN_WIDTH * PATTERN_HEIGHT) / 4];
}
Pattern;

uniform uint flags;

#endif

#if defined(SHADER_BASETYPE) && SHADER_BASETYPE == 1

#define srcpattern intpattern
#define patternType uvec4
#define valType uint

#elif defined(SHADER_BASETYPE) && SHADER_BASETYPE == 2

#define srcpattern intpattern
#define patternType ivec4
#define valType int

#else

#define srcpattern floatpattern
#define patternType vec4
#define valType float

#endif

FRAG_OUT(0) out patternType col0;
FRAG_OUT(1) out patternType col1;
FRAG_OUT(2) out patternType col2;
FRAG_OUT(3) out patternType col3;
FRAG_OUT(4) out patternType col4;
FRAG_OUT(5) out patternType col5;
FRAG_OUT(6) out patternType col6;
FRAG_OUT(7) out patternType col7;

void main()
{
  int x = int(gl_FragCoord.x) % PATTERN_WIDTH;
  int y = int(gl_FragCoord.y) % PATTERN_HEIGHT;

  // invert for 1D textures so that we read the top row first which has data on it
  if((flags & 0x10u) != 0u)
    y = PATTERN_HEIGHT - 1 - y;

  int idx = ((y * 64) + x);

  valType val = valType(Pattern.srcpattern[idx / 4][idx % 4]);

  uint stencilPass = (flags & 0xfu);

  if(stencilPass == 1u && float(val) >= 0.5f)
    discard;
  else if(stencilPass == 2u && float(val) < 0.5f)
    discard;

  gl_FragDepth = clamp(float(val), 0.0f, 1.0f);

  patternType vecval = patternType(val, val, val, val);

  col0 = col1 = col2 = col3 = col4 = col5 = col6 = col7 = vecval;
}
