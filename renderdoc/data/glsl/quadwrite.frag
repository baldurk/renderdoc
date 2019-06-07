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
#extension GL_OES_shader_image_atomic : enable
#extension GL_OES_sample_variables : enable
#else
#extension GL_ARB_derivative_control : enable
#extension GL_ARB_shader_image_load_store : require
#extension GL_ARB_gpu_shader5 : require
#endif

////////////////////////////////////////////////////////////////////////////////////////////
// Below shaders courtesy of Stephen Hill (@self_shadow), converted to glsl trivially
//
// http://blog.selfshadow.com/2012/11/12/counting-quads/
// https://github.com/selfshadow/demos/blob/master/QuadShading/QuadShading.fx
////////////////////////////////////////////////////////////////////////////////////////////

#ifdef VULKAN
// descriptor set will be patched from 0 to whichever descriptor set we're using in code
layout(set = 0, binding = 0, r32ui) uniform coherent uimage2DArray overdrawImage;
#else    // OPENGL and OPENGL_ES

// if we're compiling for GL SPIR-V, give the image an explicit binding
#ifdef GL_SPIRV
layout(binding = 0, r32ui)
#else
layout(r32ui)
#endif

    uniform coherent uimage2DArray overdrawImage;

#endif
layout(early_fragment_tests) in;

void main()
{
  uint c0 = uint(gl_SampleMaskIn[0]);

  // Obtain coverage for all pixels in the quad, via 'message passing'*.
  // (* For more details, see:
  // "Shader Amortization using Pixel Quad Message Passing", Eric Penner, GPU Pro 2.)
  uvec2 p = uvec2(uint(gl_FragCoord.x) & 1u, uint(gl_FragCoord.y) & 1u);
  ivec2 sign = ivec2(p.x > 0u ? -1 : 1, p.y > 0u ? -1 : 1);
  uint c1 = c0 + uint(sign.x * int(dFdxFine(c0)));
  uint c2 = c0 + uint(sign.y * int(dFdyFine(c0)));
  uint c3 = c2 + uint(sign.x * int(dFdxFine(c2)));

  // Count the live pixels, minus 1 (zero indexing)
  uint pixelCount = c0 + c1 + c2 + c3 - 1u;

  ivec3 quad = ivec3(gl_FragCoord.xy * 0.5, pixelCount);
  imageAtomicAdd(overdrawImage, quad, 1);
}

////////////////////////////////////////////////////////////////////////////////////////////
// Above shaders courtesy of Stephen Hill (@self_shadow), converted to glsl trivially
//
// http://blog.selfshadow.com/2012/11/12/counting-quads/
// https://github.com/selfshadow/demos/blob/master/QuadShading/QuadShading.fx
////////////////////////////////////////////////////////////////////////////////////////////
