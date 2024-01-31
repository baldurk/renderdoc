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

#define MESH_UBO

#include "glsl_ubos.h"

// this allows overrides from outside to change to e.g. dvec4

#ifndef POSITION_TYPE
#define POSITION_TYPE vec4
#endif

#ifndef SECONDARY_TYPE
#define SECONDARY_TYPE vec4
#endif

// This function is mostly duplicated between 'mesh.hlsl' and 'mesh.vert'.
// Without a convenient shared common source, changes to one should be
// reflected in the other.
void vtxExploder(in int vtxID, inout vec3 pos, inout vec3 secondary)
{
  if(Mesh.exploderScale > 0.0f)
  {
    float nonLinearVtxExplodeScale = 4.0f * Mesh.exploderScale * Mesh.vtxExploderSNorm *
                                     Mesh.vtxExploderSNorm * Mesh.vtxExploderSNorm;

    // A vertex might be coincident with our 'exploderCentre' so that, when normalized,
    // can give us INFs/NaNs that, even if multiplied by a zero 'exploderScale', can
    // leave us with bad numbers (as seems to be the case with glsl/vulkan, but not hlsl).
    // Still, we should make this case safe for when we have a non-zero 'exploderScale' -
    vec3 offset = pos - Mesh.exploderCentre;
    float offsetDistSquared = dot(offset, offset);
    vec3 safeExplodeDir = offset * inversesqrt(max(offsetDistSquared, FLT_EPSILON));

    float displacement =
        nonLinearVtxExplodeScale * ((float((vtxID >> 1) & 0xf) / 15.0f) * 1.5f - 0.5f);
    pos += (safeExplodeDir * displacement);

    // For the exploder visualisation, colour verts based on vertex ID, which we
    // store in secondary.
    //
    // Interpolate a colour gradient from 0.0 to 1.0 and back to 0.0 for vertex IDs
    // 0 to 16 to 32 respectively -
    // 1 - |          .`.
    //     |        .`   `.
    //     |      .`   |   `.
    // 0.5-|    .`           `.
    //     |  .`       |       `.     .
    //     |.`                   `. .`
    // 0.0-+-----------+-----------+---  vtx IDs
    //     0           16          32

    float vtxIDMod32Div16 = float(vtxID % 32) / 16.0f;                 // 0: 0.0  16: 1.0  31: 1.94
    float descending = floor(vtxIDMod32Div16);                         // 0..15: 0.0  16..31: 1.0
    float gradientVal = abs(vtxIDMod32Div16 - (2.0f * descending));    // 0.0..1.0

    // Use a hopefully fairly intuitive temperature gradient scheme to help visualise
    // contiguous/nearby sequences of vertices, which should also show up breaks in
    // colour where verts aren't shared between adjacent primitives.
    const vec3 gradientColours[5] =
        vec3[](vec3(0.004f, 0.002f, 0.025f),    // 0.0..0.25:  Dark blue
               vec3(0.305f, 0.001f, 0.337f),    // 0.25..0.5:  Purple
               vec3(0.665f, 0.033f, 0.133f),    // 0.5..0.75:  Purple orange
               vec3(1.000f, 0.468f, 0.000f),    // 0.75..1.0:  Orange
               vec3(1.000f, 1.000f, 1.000f)     // 1.0:  White
        );

    uint gradientSectionStartIdx = uint(gradientVal * 4.0f);
    uint gradientSectionEndIdx = min(gradientSectionStartIdx + 1u, 4u);
    vec3 gradSectionStartCol = gradientColours[gradientSectionStartIdx];
    vec3 gradSectionEndCol = gradientColours[gradientSectionEndIdx];

    float sectionLerp = gradientVal - float(gradientSectionStartIdx) * 0.25f;
    vec3 gradCol = mix(gradientColours[gradientSectionStartIdx],
                       gradientColours[gradientSectionEndIdx], sectionLerp);

    secondary = gradCol;
  }
}

IO_LOCATION(0) in POSITION_TYPE vsin_position;
IO_LOCATION(1) in SECONDARY_TYPE vsin_secondary;

IO_LOCATION(0) out vec4 vsout_secondary;
IO_LOCATION(1) out vec4 vsout_norm;

#ifdef VULKAN

uint getMeshletCountAt(uint m)
{
  uint vecIdx = m / 4;

  if((m % 4) == 0)
    return meshlet.data[vecIdx].x;
  else if((m % 4) == 1)
    return meshlet.data[vecIdx].y;
  else if((m % 4) == 2)
    return meshlet.data[vecIdx].z;
  else if((m % 4) == 3)
    return meshlet.data[vecIdx].w;
}

vec4 getMeshletColor()
{
  uint searchIdx = VERTEX_ID;

  // array of prefix summed counts accessible via getMeshletCountAt [x, x+y, x+y+z, ...] we do a
  // binary search to find which meshlet this index corresponds to

  uint first = 0, last = meshlet.meshletCount - 1;
  uint count = last - first;

  while(count > 0)
  {
    uint halfrange = count / 2;
    uint mid = first + halfrange;

    if(searchIdx < getMeshletCountAt(mid))
    {
      count = halfrange;
    }
    else
    {
      first = mid + 1;
      count -= halfrange + 1;
    }
  }

  uint meshletIndex = first;

  if(VERTEX_ID < getMeshletCountAt(meshletIndex))
  {
    meshletIndex += meshlet.meshletOffset;
    meshletIndex %= 48;
    uvec4 meshletColor = Mesh.meshletColours[meshletIndex / 4];
    if((meshletIndex % 4) == 0)
      return unpackUnorm4x8(meshletColor.x);
    else if((meshletIndex % 4) == 1)
      return unpackUnorm4x8(meshletColor.y);
    else if((meshletIndex % 4) == 2)
      return unpackUnorm4x8(meshletColor.z);
    else if((meshletIndex % 4) == 3)
      return unpackUnorm4x8(meshletColor.w);
  }

  return vec4(0, 0, 0, 1);
}
#endif

void main(void)
{
  vec2 psprite[4] =
      vec2[](vec2(-1.0f, -1.0f), vec2(-1.0f, 1.0f), vec2(1.0f, -1.0f), vec2(1.0f, 1.0f));

  vec4 pos = vec4(vsin_position);
  vec4 secondary = vec4(vsin_secondary);
  vtxExploder(VERTEX_ID, pos.xyz, secondary.xyz);

  if(Mesh.homogenousInput == 0u)
  {
    pos = vec4(pos.xyz, 1);
  }
  else
  {
#ifdef VULKAN
    pos = vec4(pos.x, -pos.y, pos.z, pos.w);
#endif
  }

  gl_Position = Mesh.mvp * pos;
  gl_Position.xy += Mesh.pointSpriteSize.xy * 0.01f * psprite[VERTEX_ID % 4] * gl_Position.w;
  vsout_secondary = vec4(secondary);
  vsout_norm = vec4(0, 0, 1, 1);

#ifdef VULKAN
  if(Mesh.displayFormat == MESHDISPLAY_MESHLET)
    vsout_secondary = getMeshletColor();

  // GL->VK conventions
  gl_Position.y = -gl_Position.y;
  if(Mesh.flipY == 1)
  {
    gl_Position.y = -gl_Position.y;
  }
  if(Mesh.rawoutput == 0)
  {
    gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
  }

  gl_PointSize = 4.0f;
#endif
}
