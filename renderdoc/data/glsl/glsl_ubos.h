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

#include "glsl_globals.h"

// the ARM driver is buggy and crashes if we declare UBOs that don't correspond to descriptors,
// even if they are completely unused. So if we're not in C++, we have #define opt-ins for each
// global UBO

#if defined(MESH_UBO) || defined(__cplusplus)

BINDING(0) uniform MeshUBOData
{
  mat4 mvp;
  mat4 invProj;
  vec4 color;
  int displayFormat;
  uint homogenousInput;
  vec2 pointSpriteSize;
  uint rawoutput;
  vec3 padding;
}
INST_NAME(Mesh);

#endif    // defined(MESH_UBO) || defined(__cplusplus)

#if defined(CHECKER_UBO) || defined(__cplusplus)

BINDING(0) uniform CheckerboardUBOData
{
  vec2 RectPosition;
  vec2 RectSize;

  vec4 PrimaryColor;
  vec4 SecondaryColor;
  vec4 InnerColor;

  float CheckerSquareDimension;
  float BorderWidth;
}
INST_NAME(checker);

#endif    // defined(CHECKER_UBO) || defined(__cplusplus)

#if defined(TEXDISPLAY_UBO) || defined(__cplusplus)

BINDING(0) uniform TexDisplayUBOData
{
  vec2 Position;
  float Scale;
  float HDRMul;

  vec4 Channels;

  float RangeMinimum;
  float InverseRangeSize;
  int MipLevel;
  int FlipY;

  vec3 TextureResolutionPS;
  int OutputDisplayFormat;

  vec2 OutputRes;
  int RawOutput;
  float Slice;

  int SampleIdx;
  float MipShift;
  int DecodeYUV;
  float Padding;

  uvec4 YUVDownsampleRate;
  uvec4 YUVAChannels;
}
INST_NAME(texdisplay);

#endif    // defined(TEXDISPLAY_UBO) || defined(__cplusplus)

#if defined(MESH_PICK_UBO) || defined(__cplusplus)

BINDING(0) uniform MeshPickUBOData
{
  vec3 rayPos;
  uint use_indices;

  vec3 rayDir;
  uint numVerts;

  vec2 coords;
  vec2 viewport;

  uint meshMode;    // triangles, triangle strip, fan, etc...
  uint unproject;
  vec2 padding;

  mat4 mvp;
}
INST_NAME(meshpick);

#endif    // defined(MESH_PICK_UBO) || defined(__cplusplus)

#if defined(FONT_UBO) || defined(__cplusplus)

BINDING(0) uniform FontUBOData
{
  vec2 TextPosition;
  float txtpadding;
  float TextSize;

  vec2 CharacterSize;
  vec2 FontScreenAspect;
}
INST_NAME(general);

struct FontGlyphData
{
  vec4 posdata;
  vec4 uvdata;
};

#define FONT_FIRST_CHAR 32
#define FONT_LAST_CHAR 126

BINDING(1) uniform GlyphUBOData
{
  FontGlyphData data[FONT_LAST_CHAR - FONT_FIRST_CHAR + 1];
}
INST_NAME(glyphs);

#define MAX_SINGLE_LINE_LENGTH 256

BINDING(2) uniform StringUBOData
{
  uvec4 chars[MAX_SINGLE_LINE_LENGTH];
}
INST_NAME(str);

#endif    // defined(FONT_UBO) || defined(__cplusplus)

#if defined(HEATMAP_UBO) || defined(__cplusplus)

BINDING(1) uniform HeatmapData
{
  int HeatmapMode;
  int DummyA;
  int DummyB;
  int DummyC;

  // must match size of colorRamp on C++ side
  vec4 ColorRamp[22];
}
INST_NAME(heatmap);

#endif    // defined(HEATMAP_UBO) || defined(__cplusplus)

#if defined(HISTOGRAM_UBO) || defined(__cplusplus)

BINDING(2) uniform HistogramUBOData
{
  uint HistogramChannels;
  float HistogramMin;
  float HistogramMax;
  uint HistogramFlags;

  float HistogramSlice;
  int HistogramMip;
  int HistogramSample;
  int HistogramNumSamples;

  vec3 HistogramTextureResolution;
  float Padding3;

  uvec4 HistogramYUVDownsampleRate;
  uvec4 HistogramYUVAChannels;
}
INST_NAME(histogram_minmax);

#endif    // defined(HISTOGRAM_UBO) || defined(__cplusplus)
