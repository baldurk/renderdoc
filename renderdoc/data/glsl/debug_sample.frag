/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2025-2026 Baldur Karlsson
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

#define DEBUGSAMPLE_UBO

#ifdef OPENGL_ES
precision highp float;
#endif

#include "glsl_ubos.h"

#if UINT_TEX

#define RESULT uvec4
#define FLOAT_CONV(x) floatBitsToUint(x)

uniform PRECISION usampler2D tex2D;
uniform PRECISION usampler3D tex3D;
uniform PRECISION usampler2DArray tex2DArray;
uniform PRECISION usamplerBuffer texBuffer;
#ifdef TEXSAMPLE_MULTISAMPLE
uniform PRECISION usampler2DMS tex2DMS;
uniform PRECISION usampler2DMSArray tex2DMSArray;
#endif
#ifdef OPENGL_CORE
uniform PRECISION usampler1D tex1D;
uniform PRECISION usampler1DArray tex1DArray;
uniform PRECISION usampler2DRect tex2DRect;
#endif

#elif SINT_TEX

#define RESULT ivec4
#define FLOAT_CONV(x) floatBitsToInt(x)

uniform PRECISION isampler2D tex2D;
uniform PRECISION isampler3D tex3D;
uniform PRECISION isampler2DArray tex2DArray;
uniform PRECISION isamplerBuffer texBuffer;
#ifdef TEXSAMPLE_MULTISAMPLE
uniform PRECISION isampler2DMS tex2DMS;
uniform PRECISION isampler2DMSArray tex2DMSArray;
#endif
#ifdef OPENGL_CORE
uniform PRECISION isampler1D tex1D;
uniform PRECISION isampler1DArray tex1DArray;
uniform PRECISION isampler2DRect tex2DRect;
#endif

#else

#define RESULT vec4
#define FLOAT_CONV(x) x

uniform PRECISION sampler2D tex2D;
uniform PRECISION sampler3D tex3D;
uniform PRECISION samplerCube texCube;
uniform PRECISION sampler2DArray tex2DArray;
#ifdef TEXSAMPLE_CUBE_ARRAY
uniform PRECISION samplerCubeArray texCubeArray;
#endif
uniform PRECISION samplerBuffer texBuffer;
#ifdef TEXSAMPLE_MULTISAMPLE
uniform PRECISION sampler2DMS tex2DMS;
uniform PRECISION sampler2DMSArray tex2DMSArray;
#endif
#ifdef OPENGL_CORE
uniform PRECISION sampler1D tex1D;
uniform PRECISION sampler1DArray tex1DArray;
uniform PRECISION sampler2DRect tex2DRect;
#endif

uniform PRECISION sampler2DShadow tex2DShadow;
uniform PRECISION samplerCubeShadow texCubeShadow;
uniform PRECISION sampler2DArrayShadow tex2DArrayShadow;
#ifdef TEXSAMPLE_CUBE_ARRAY
uniform PRECISION samplerCubeArrayShadow texCubeArrayShadow;
#endif
#ifdef OPENGL_CORE
uniform PRECISION sampler1DShadow tex1DShadow;
uniform PRECISION sampler1DArrayShadow tex1DArrayShadow;
uniform PRECISION sampler2DRectShadow tex2DRectShadow;
#endif

#endif

in vec4 v2f_uvwa;
out RESULT Output;

#ifdef GATHER_OFFSETS
const ivec2 gather_offsets[4] = GATHER_OFFSETS;
#else
const ivec2 gather_offsets[4] = ivec2[4](ivec2(0, 0), ivec2(0, 0), ivec2(0, 0), ivec2(0, 0));
#endif

#ifdef FETCH_OFFSET
const ivec3 fetch_offset = FETCH_OFFSET;
#else
const ivec3 fetch_offset = ivec3(0, 0, 0);
#endif

///////////////////////////////////
// OpImageFetch

RESULT DoFetch1D()
{
#ifdef OPENGL_CORE
  return texelFetchOffset(tex1D, debugsample.texel_uvw.x, debugsample.texel_lod, fetch_offset.x);
#else
  return RESULT(0, 0, 0, 0);
#endif
}

RESULT DoFetch2D()
{
  return texelFetchOffset(tex2D, debugsample.texel_uvw.xy, debugsample.texel_lod, fetch_offset.xy);
}

RESULT DoFetch3D()
{
  return texelFetchOffset(tex3D, debugsample.texel_uvw.xyz, debugsample.texel_lod, fetch_offset.xyz);
}

// can't do fetches from cubes
RESULT DoFetchCube()
{
  return RESULT(0, 0, 0, 0);
}

RESULT DoFetch1DArray()
{
#ifdef OPENGL_CORE
  return texelFetchOffset(tex1DArray, debugsample.texel_uvw.xy, debugsample.texel_lod,
                          fetch_offset.x);
#else
  return RESULT(0, 0, 0, 0);
#endif
}

RESULT DoFetch2DArray()
{
  return texelFetchOffset(tex2DArray, debugsample.texel_uvw.xyz, debugsample.texel_lod,
                          fetch_offset.xy);
}

// can't do fetches from cubes
RESULT DoFetchCubeArray()
{
  return RESULT(0, 0, 0, 0);
}

RESULT DoFetch2DRect()
{
#ifdef OPENGL_CORE
  return texelFetchOffset(tex2DRect, debugsample.texel_uvw.xy, fetch_offset.xy);
#else
  return RESULT(0, 0, 0, 0);
#endif
}

RESULT DoFetchBuffer()
{
  // no texelFetchOffset overload for buffers in glsl, supported in SPIR-V
  return texelFetch(texBuffer, debugsample.texel_uvw.x);
}

#ifdef TEXSAMPLE_MULTISAMPLE
RESULT DoFetch2DMS()
{
  // no texelFetchOffset overload for MS in glsl, supported in SPIR-V
  return texelFetch(tex2DMS, debugsample.texel_uvw.xy, debugsample.sampleIdx);
}

RESULT DoFetch2DMSArray()
{
  // no texelFetchOffset overload for MS in glsl, supported in SPIR-V
  return texelFetch(tex2DMSArray, debugsample.texel_uvw.xyz, debugsample.sampleIdx);
}
#endif

///////////////////////////////////
// OpImageSampleImplicitLodBias (used with bias on GLES due to inability to bias in samplers)

RESULT DoSampleBias2D()
{
  return textureOffset(tex2D, v2f_uvwa.xy, fetch_offset.xy, debugsample.gles_bias);
}

RESULT DoSampleBias3D()
{
  return textureOffset(tex3D, v2f_uvwa.xyz, fetch_offset.xyz, debugsample.gles_bias);
}

RESULT DoSampleBiasCube()
{
#if FLOAT_TEX

  // no offsets for cubes
  return texture(texCube, v2f_uvwa.xyz, debugsample.gles_bias);

#else
  // cubes are only handled on float type
  return RESULT(0, 0, 0, 0);
#endif
}

RESULT DoSampleBias2DArray()
{
  return textureOffset(tex2DArray, v2f_uvwa.xyz, fetch_offset.xy, debugsample.gles_bias);
}

#ifdef TEXSAMPLE_CUBE_ARRAY
RESULT DoSampleBiasCubeArray()
{
#if FLOAT_TEX

  // no offsets for cubes
  return texture(texCubeArray, v2f_uvwa.xyzw, debugsample.gles_bias);

#else
  // cubes are only handled on float type
  return RESULT(0, 0, 0, 0);
#endif
}
#endif

RESULT DoSampleDrefBias2D()
{
#if FLOAT_TEX

  return vec4(textureOffset(tex2DShadow, vec3(v2f_uvwa.xy, debugsample.compare), fetch_offset.xy,
                            debugsample.gles_bias),
              0, 0, 0);

#else
  // shadow samplers only for FLOAT_TEX
  return RESULT(0, 0, 0, 0);
#endif
}

///////////////////////////////////
// OpImageQueryLod

#ifdef OPENGL_CORE
RESULT DoQueryLod1D()
{
  return RESULT(FLOAT_CONV(textureQueryLod(tex1D, v2f_uvwa.x)), 0, 0);
}

RESULT DoQueryLod2D()
{
  return RESULT(FLOAT_CONV(textureQueryLod(tex2D, v2f_uvwa.xy)), 0, 0);
}

RESULT DoQueryLod3D()
{
  return RESULT(FLOAT_CONV(textureQueryLod(tex3D, v2f_uvwa.xyz)), 0, 0);
}

RESULT DoQueryLodCube()
{
#if FLOAT_TEX
  return RESULT(FLOAT_CONV(textureQueryLod(texCube, v2f_uvwa.xyz)), 0, 0);
#else
  // cubes are only handled on float type
  return RESULT(0, 0, 0, 0);
#endif
}

RESULT DoQueryLod1DArray()
{
  return RESULT(FLOAT_CONV(textureQueryLod(tex1DArray, v2f_uvwa.x)), 0, 0);
}

RESULT DoQueryLod2DArray()
{
  return RESULT(FLOAT_CONV(textureQueryLod(tex2DArray, v2f_uvwa.xy)), 0, 0);
}

#ifdef TEXSAMPLE_CUBE_ARRAY
RESULT DoQueryLodCubeArray()
{
#if FLOAT_TEX
  return RESULT(FLOAT_CONV(textureQueryLod(texCubeArray, v2f_uvwa.xyz)), 0, 0);
#else
  // cubes are only handled on float type
  return RESULT(0, 0, 0, 0);
#endif
}
#endif
#endif

///////////////////////////////////
// OpImageSampleExplicitLod (ImplicitLod is upgraded to this)

RESULT DoSample1D()
{
#ifdef OPENGL_ES
  return RESULT(0, 0, 0, 0);
#elif ENABLE_MINLOD && USE_GRAD
  return textureGradOffsetClampARB(tex1D, debugsample.uvwa.x, debugsample.ddx_uvw.x,
                                   debugsample.ddy_uvw.x, fetch_offset.x, debugsample.minlod);
#elif USE_GRAD
  return textureGradOffset(tex1D, debugsample.uvwa.x, debugsample.ddx_uvw.x, debugsample.ddy_uvw.x,
                           fetch_offset.x);
#else
  return textureLodOffset(tex1D, debugsample.uvwa.x, debugsample.lod, fetch_offset.x);
#endif
}

RESULT DoSample2D()
{
#if ENABLE_MINLOD && USE_GRAD
  return textureGradOffsetClampARB(tex2D, debugsample.uvwa.xy, debugsample.ddx_uvw.xy,
                                   debugsample.ddy_uvw.xy, fetch_offset.xy, debugsample.minlod);
#elif USE_GRAD
  return textureGradOffset(tex2D, debugsample.uvwa.xy, debugsample.ddx_uvw.xy,
                           debugsample.ddy_uvw.xy, fetch_offset.xy);
#else
  return textureLodOffset(tex2D, debugsample.uvwa.xy, debugsample.lod, fetch_offset.xy);
#endif
}

RESULT DoSample3D()
{
#if ENABLE_MINLOD && USE_GRAD
  return textureGradOffsetClampARB(tex3D, debugsample.uvwa.xyz, debugsample.ddx_uvw.xyz,
                                   debugsample.ddy_uvw.xyz, fetch_offset.xyz, debugsample.minlod);
#elif USE_GRAD
  return textureGradOffset(tex3D, debugsample.uvwa.xyz, debugsample.ddx_uvw.xyz,
                           debugsample.ddy_uvw.xyz, fetch_offset.xyz);
#else
  return textureLodOffset(tex3D, debugsample.uvwa.xyz, debugsample.lod, fetch_offset.xyz);
#endif
}

RESULT DoSampleCube()
{
#if FLOAT_TEX

  // no offsets for cubes

#if ENABLE_MINLOD && USE_GRAD
  return textureGradClampARB(texCube, debugsample.uvwa.xyz, debugsample.ddx_uvw.xyz,
                             debugsample.ddy_uvw.xyz, debugsample.minlod);
#elif USE_GRAD
  return textureGrad(texCube, debugsample.uvwa.xyz, debugsample.ddx_uvw.xyz, debugsample.ddy_uvw.xyz);
#else
  return textureLod(texCube, debugsample.uvwa.xyz, debugsample.lod);
#endif

#else
  // cubes are only handled on float type
  return RESULT(0, 0, 0, 0);
#endif
}

RESULT DoSample1DArray()
{
#ifdef OPENGL_ES
  return RESULT(0, 0, 0, 0);
#elif ENABLE_MINLOD && USE_GRAD
  return textureGradOffsetClampARB(tex1DArray, debugsample.uvwa.xy, debugsample.ddx_uvw.x,
                                   debugsample.ddy_uvw.x, fetch_offset.x, debugsample.minlod);
#elif USE_GRAD
  return textureGradOffset(tex1DArray, debugsample.uvwa.xy, debugsample.ddx_uvw.x,
                           debugsample.ddy_uvw.x, fetch_offset.x);
#else
  return textureLodOffset(tex1DArray, debugsample.uvwa.xy, debugsample.lod, fetch_offset.x);
#endif
}

RESULT DoSample2DArray()
{
#if ENABLE_MINLOD && USE_GRAD
  return textureGradOffsetClampARB(tex2DArray, debugsample.uvwa.xyz, debugsample.ddx_uvw.xy,
                                   debugsample.ddy_uvw.xy, fetch_offset.xy, debugsample.minlod);
#elif USE_GRAD
  return textureGradOffset(tex2DArray, debugsample.uvwa.xyz, debugsample.ddx_uvw.xy,
                           debugsample.ddy_uvw.xy, fetch_offset.xy);
#else
  return textureLodOffset(tex2DArray, debugsample.uvwa.xyz, debugsample.lod, fetch_offset.xy);
#endif
}

#ifdef TEXSAMPLE_CUBE_ARRAY
RESULT DoSampleCubeArray()
{
#if FLOAT_TEX

  // no offsets for cubes

#if ENABLE_MINLOD && USE_GRAD
  return textureGradClampARB(texCubeArray, debugsample.uvwa.xyzw, debugsample.ddx_uvw.xyz,
                             debugsample.ddy_uvw.xyz, debugsample.minlod);
#elif USE_GRAD
  return textureGrad(texCubeArray, debugsample.uvwa.xyzw, debugsample.ddx_uvw.xyz,
                     debugsample.ddy_uvw.xyz);
#else
  return textureLod(texCubeArray, debugsample.uvwa.xyzw, debugsample.lod);
#endif

#else
  // cubes are only handled on float type
  return RESULT(0, 0, 0, 0);
#endif
}
#endif

RESULT DoSample2DRect()
{
#ifdef OPENGL_ES
  return RESULT(0, 0, 0, 0);
#else
  // fairly degenerate, no lod or grad in use. minlod is illegal
  return textureOffset(tex2DRect, debugsample.uvwa.xy, fetch_offset.xy);
#endif
}

///////////////////////////////////
// OpImageSampleDrefExplicitLod (ImplicitLod is upgraded to this)

RESULT DoSampleDref1D()
{
#if FLOAT_TEX

#ifdef OPENGL_ES
  return RESULT(0, 0, 0, 0);
#elif ENABLE_MINLOD && USE_GRAD
  return vec4(textureGradOffsetClampARB(
                  tex1DShadow, vec3(debugsample.uvwa.x, 0, debugsample.compare),
                  debugsample.ddx_uvw.x, debugsample.ddy_uvw.x, fetch_offset.x, debugsample.minlod),
              0, 0, 0);
#elif USE_GRAD
  return vec4(textureGradOffset(tex1DShadow, vec3(debugsample.uvwa.x, 0, debugsample.compare),
                                debugsample.ddx_uvw.x, debugsample.ddy_uvw.x, fetch_offset.x),
              0, 0, 0);
#else
  return vec4(textureLodOffset(tex1DShadow, vec3(debugsample.uvwa.x, 0, debugsample.compare),
                               debugsample.lod, fetch_offset.x),
              0, 0, 0);
#endif

#else
  // shadow samplers only for FLOAT_TEX
  return RESULT(0, 0, 0, 0);
#endif
}

RESULT DoSampleDref2D()
{
#if FLOAT_TEX

#if ENABLE_MINLOD && USE_GRAD
  return vec4(textureGradOffsetClampARB(tex2DShadow, vec3(debugsample.uvwa.xy, debugsample.compare),
                                        debugsample.ddx_uvw.xy, debugsample.ddy_uvw.xy,
                                        fetch_offset.xy, debugsample.minlod),
              0, 0, 0);
#elif USE_GRAD
  return vec4(textureGradOffset(tex2DShadow, vec3(debugsample.uvwa.xy, debugsample.compare),
                                debugsample.ddx_uvw.xy, debugsample.ddy_uvw.xy, fetch_offset.xy),
              0, 0, 0);
#else
  return vec4(textureLodOffset(tex2DShadow, vec3(debugsample.uvwa.xy, debugsample.compare),
                               debugsample.lod, fetch_offset.xy),
              0, 0, 0);
#endif

#else
  // shadow samplers only for FLOAT_TEX
  return RESULT(0, 0, 0, 0);
#endif
}

RESULT DoSampleDrefCube()
{
#if FLOAT_TEX

  // no offsets for cubes
  // no overloads for LOD selection, assume LOD 0 and hardcode gradients

#if ENABLE_MINLOD && USE_GRAD
  return vec4(
      textureGradClampARB(texCubeShadow, vec4(debugsample.uvwa.xyz, debugsample.compare),
                          debugsample.ddx_uvw.xyz, debugsample.ddy_uvw.xyz, debugsample.minlod),
      0, 0, 0);
#elif USE_GRAD
  return vec4(textureGrad(texCubeShadow, vec4(debugsample.uvwa.xyz, debugsample.compare),
                          debugsample.ddx_uvw.xyz, debugsample.ddy_uvw.xyz),
              0, 0, 0);
#else
  return vec4(textureGrad(texCubeShadow, vec4(debugsample.uvwa.xyz, debugsample.compare),
                          vec3(0, 0, 0), vec3(0, 0, 0)),
              0, 0, 0);
#endif

#else
  // cubes are only handled on float type
  return RESULT(0, 0, 0, 0);
#endif
}

RESULT DoSampleDref1DArray()
{
#if FLOAT_TEX

#ifdef OPENGL_ES
  return RESULT(0, 0, 0, 0);
#elif ENABLE_MINLOD && USE_GRAD
  return vec4(textureGradOffsetClampARB(
                  tex1DArrayShadow, vec3(debugsample.uvwa.xy, debugsample.compare),
                  debugsample.ddx_uvw.x, debugsample.ddy_uvw.x, fetch_offset.x, debugsample.minlod),
              0, 0, 0);
#elif USE_GRAD
  return vec4(textureGradOffset(tex1DArrayShadow, vec3(debugsample.uvwa.xy, debugsample.compare),
                                debugsample.ddx_uvw.x, debugsample.ddy_uvw.x, fetch_offset.x),
              0, 0, 0);
#else
  return vec4(textureLodOffset(tex1DArrayShadow, vec3(debugsample.uvwa.xy, debugsample.compare),
                               debugsample.lod, fetch_offset.x),
              0, 0, 0);
#endif

#else
  // shadow samplers only for FLOAT_TEX
  return RESULT(0, 0, 0, 0);
#endif
}

RESULT DoSampleDref2DArray()
{
#if FLOAT_TEX

  // no overloads for LOD selection, assume LOD 0 and hardcode gradients

#if ENABLE_MINLOD && USE_GRAD
  return vec4(
      textureGradOffsetClampARB(tex2DArrayShadow, vec4(debugsample.uvwa.xyz, debugsample.compare),
                                debugsample.ddx_uvw.xy, debugsample.ddy_uvw.xy, fetch_offset.xy,
                                debugsample.minlod),
      0, 0, 0);
#elif USE_GRAD
  return vec4(textureGradOffset(tex2DArrayShadow, vec4(debugsample.uvwa.xyz, debugsample.compare),
                                debugsample.ddx_uvw.xy, debugsample.ddy_uvw.xy, fetch_offset.xy),
              0, 0, 0);
#else
  return vec4(textureGradOffset(tex2DArrayShadow, vec4(debugsample.uvwa.xyz, debugsample.compare),
                                vec2(0, 0), vec2(0, 0), fetch_offset.xy),
              0, 0, 0);
#endif

#else
  // shadow samplers only for FLOAT_TEX
  return RESULT(0, 0, 0, 0);
#endif
}

#ifdef TEXSAMPLE_CUBE_ARRAY
RESULT DoSampleDrefCubeArray()
{
#if FLOAT_TEX

  // no offsets for cubes
  // no overloads for LOD selection or explicit grads

#if ENABLE_MINLOD
  return vec4(textureClampARB(texCubeArrayShadow, debugsample.uvwa.xyzw, debugsample.compare,
                              debugsample.minlod),
              0, 0, 0);
#else
  return vec4(texture(texCubeArrayShadow, debugsample.uvwa.xyzw, debugsample.compare), 0, 0, 0);
#endif

#else
  // cubes are only handled on float type
  return RESULT(0, 0, 0, 0);
#endif
}
#endif

RESULT DoSampleDref2DRect()
{
#ifdef OPENGL_ES
  return RESULT(0, 0, 0, 0);
#elif FLOAT_TEX
  // fairly degenerate, no lod or grad in use. minlod is illegal
  return vec4(textureOffset(tex2DRectShadow, vec3(debugsample.uvwa.xy, debugsample.compare),
                            fetch_offset.xy),
              0, 0, 0);
#else
  // shadow samplers only for FLOAT_TEX
  return RESULT(0, 0, 0, 0);
#endif
}

///////////////////////////////////
// OpImageGatherExplicitLod (ImplicitLod is upgraded to this)

RESULT DoGather2D()
{
#if GATHER_SUPPORT == 0
  // no gather support at all
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 1
  return textureGather(tex2D, debugsample.uvwa.xy);
#elif USE_GATHER_OFFS
  return textureGatherOffsets(tex2D, debugsample.uvwa.xy, gather_offsets, GATHER_CHANNEL);
#else
  return textureGatherOffset(tex2D, debugsample.uvwa.xy, debugsample.dynoffset.xy, GATHER_CHANNEL);
#endif
}

RESULT DoGatherCube()
{
#if FLOAT_TEX
  // no offsets for cubes

#if GATHER_SUPPORT == 0
  // no gather support at all
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 1
  return textureGather(texCube, debugsample.uvwa.xyz);
#else
  return textureGather(texCube, debugsample.uvwa.xyz, GATHER_CHANNEL);
#endif

#else
  // cubes are only handled on float type
  return RESULT(0, 0, 0, 0);
#endif
}

RESULT DoGather2DArray()
{
#if GATHER_SUPPORT == 0
  // no gather support at all
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 1
  return textureGather(tex2DArray, debugsample.uvwa.xyw);
#elif USE_GATHER_OFFS
  return textureGatherOffsets(tex2DArray, debugsample.uvwa.xyw, gather_offsets, GATHER_CHANNEL);
#else
  return textureGatherOffset(tex2DArray, debugsample.uvwa.xyw, debugsample.dynoffset.xy,
                             GATHER_CHANNEL);
#endif
}

#ifdef TEXSAMPLE_CUBE_ARRAY
RESULT DoGatherCubeArray()
{
#if FLOAT_TEX
  // no offsets for cubes

#if GATHER_SUPPORT == 0
  // no gather support at all
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 1
  return textureGather(texCubeArray, debugsample.uvwa.xyzw);
#else
  return textureGather(texCubeArray, debugsample.uvwa.xyzw, GATHER_CHANNEL);
#endif

#else
  // cubes are only handled on float type
  return RESULT(0, 0, 0, 0);
#endif
}
#endif

RESULT DoGather2DRect()
{
#ifdef OPENGL_ES
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 0
  // no gather support at all
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 1
  // rects unsupported with only base gather extension
  return RESULT(0, 0, 0, 0);
#elif USE_GATHER_OFFS
  return textureGatherOffsets(tex2DRect, debugsample.uvwa.xy, gather_offsets, GATHER_CHANNEL);
#else
  return textureGatherOffset(tex2DRect, debugsample.uvwa.xy, debugsample.dynoffset.xy,
                             GATHER_CHANNEL);
#endif
}

///////////////////////////////////
// OpImageGatherDrefExplicitLod (ImplicitLod is upgraded to this)

RESULT DoGatherDref2D()
{
#if !FLOAT_TEX
  // shadow samplers only for FLOAT_TEX
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 0
  // no gather support at all
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 1
  // shadow samplers unsupported with only base gather extension
  return RESULT(0, 0, 0, 0);
#elif USE_GATHER_OFFS
  return textureGatherOffsets(tex2DShadow, debugsample.uvwa.xy, debugsample.compare, gather_offsets);
#else
  return textureGatherOffset(tex2DShadow, debugsample.uvwa.xy, debugsample.compare,
                             debugsample.dynoffset.xy);
#endif
}

RESULT DoGatherDrefCube()
{
#if FLOAT_TEX
  // no offsets for cubes

#if GATHER_SUPPORT == 0
  // no gather support at all
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 1
  // shadow samplers unsupported with only base gather extension
  return RESULT(0, 0, 0, 0);
#else
  return textureGather(texCubeShadow, debugsample.uvwa.xyz, debugsample.compare);
#endif

#else
  // cubes are only handled on float type
  return RESULT(0, 0, 0, 0);
#endif
}

RESULT DoGatherDref2DArray()
{
#if !FLOAT_TEX
  // shadow samplers only for FLOAT_TEX
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 0
  // no gather support at all
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 1
  // shadow samplers unsupported with only base gather extension
  return RESULT(0, 0, 0, 0);
#elif USE_GATHER_OFFS
  return textureGatherOffsets(tex2DArrayShadow, debugsample.uvwa.xyw, debugsample.compare,
                              gather_offsets);
#else
  return textureGatherOffset(tex2DArrayShadow, debugsample.uvwa.xyw, debugsample.compare,
                             debugsample.dynoffset.xy);
#endif
}

#ifdef TEXSAMPLE_CUBE_ARRAY
RESULT DoGatherDrefCubeArray()
{
#if FLOAT_TEX
  // no offsets for cubes

#if GATHER_SUPPORT == 0
  // no gather support at all
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 1
  // shadow samplers unsupported with only base gather extension
  return RESULT(0, 0, 0, 0);
#else
  return textureGather(texCubeArrayShadow, debugsample.uvwa.xyzw, debugsample.compare);
#endif

#else
  // cubes are only handled on float type
  return RESULT(0, 0, 0, 0);
#endif
}
#endif

RESULT DoGatherDref2DRect()
{
#ifdef OPENGL_ES
  return RESULT(0, 0, 0, 0);
#elif !FLOAT_TEX
  // shadow samplers only for FLOAT_TEX
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 0
  // no gather support at all
  return RESULT(0, 0, 0, 0);
#elif GATHER_SUPPORT == 1
  // shadow samplers unsupported with only base gather extension
  return RESULT(0, 0, 0, 0);
#elif USE_GATHER_OFFS
  return textureGatherOffsets(tex2DRectShadow, debugsample.uvwa.xy, debugsample.compare,
                              gather_offsets);
#else
  return textureGatherOffset(tex2DRectShadow, debugsample.uvwa.xy, debugsample.compare,
                             debugsample.dynoffset.xy);
#endif
}

void main()
{
  Output = OPERATION();
}
