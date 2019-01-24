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

#include "hlsl_cbuffers.h"

struct meshV2F
{
  float4 pos : SV_Position;
  float3 norm : Normal;
  float4 secondary : Secondary;
};

struct meshA2V
{
  float4 pos : pos;
  float4 secondary : sec;
};

meshV2F RENDERDOC_MeshVS(meshA2V IN, uint vid : SV_VertexID)
{
  meshV2F OUT = (meshV2F)0;

  float2 psprite[4] = {float2(-1.0f, -1.0f), float2(-1.0f, 1.0f), float2(1.0f, -1.0f),
                       float2(1.0f, 1.0f)};

  OUT.pos = mul(IN.pos, ModelViewProj);
  OUT.pos.xy += SpriteSize.xy * 0.01f * psprite[vid % 4] * OUT.pos.w;
  OUT.norm = float3(0, 0, 1);
  OUT.secondary = IN.secondary;

  return OUT;
}

struct triSizeV2F
{
  float4 pos : SV_Position;
  float pixarea : PixArea;
};

cbuffer viewportCBuf : register(b0)
{
  float4 viewport;
};

[maxvertexcount(3)] void RENDERDOC_TriangleSizeGS(triangle meshV2F input[3],
                                                  inout TriangleStream<triSizeV2F> TriStream)
{
  triSizeV2F output;

  float2 a = input[0].pos.xy / input[0].pos.w;
  float2 b = input[1].pos.xy / input[1].pos.w;
  float2 c = input[2].pos.xy / input[2].pos.w;

  a = (a * 0.5f + 0.5f) * viewport.xy;
  b = (b * 0.5f + 0.5f) * viewport.xy;
  c = (c * 0.5f + 0.5f) * viewport.xy;

  float ab = length(a - b);
  float bc = length(b - c);
  float ca = length(c - a);

  float s = (ab + bc + ca) / 2.0f;

  float area = sqrt(s * (s - ab) * (s - bc) * (s - ca));

  for(int i = 0; i < 3; i++)
  {
    output.pos = input[i].pos;
    output.pixarea = area;
    TriStream.Append(output);
  }
  TriStream.RestartStrip();
}

float4 RENDERDOC_TriangleSizePS(triSizeV2F IN) : SV_Target0
{
  return float4(max(IN.pixarea, 0.001f).xxx, 1.0f);
}

[maxvertexcount(3)] void RENDERDOC_MeshGS(triangle meshV2F input[3],
                                          inout TriangleStream<meshV2F> TriStream)
{
  meshV2F output;

  float4 faceEdgeA = mul(input[1].pos, InvProj) - mul(input[0].pos, InvProj);
  float4 faceEdgeB = mul(input[2].pos, InvProj) - mul(input[0].pos, InvProj);
  float3 faceNormal = normalize(cross(faceEdgeA.xyz, faceEdgeB.xyz));

  for(int i = 0; i < 3; i++)
  {
    output.pos = input[i].pos;
    output.norm = faceNormal;
    output.secondary = input[i].secondary;
    TriStream.Append(output);
  }
  TriStream.RestartStrip();
}

float4 RENDERDOC_MeshPS(meshV2F IN) : SV_Target0
{
  uint type = MeshDisplayFormat;

  if(type == MESHDISPLAY_SECONDARY)
    return float4(IN.secondary.xyz, 1);
  else if(type == MESHDISPLAY_SECONDARY_ALPHA)
    return float4(IN.secondary.www, 1);
  else if(type == MESHDISPLAY_FACELIT)
  {
    float3 lightDir = normalize(float3(0, -0.3f, -1));

    return float4(MeshColour.xyz * abs(dot(lightDir, IN.norm)), 1);
  }
  else    // if(type == MESHDISPLAY_SOLID)
    return float4(MeshColour.xyz, 1);
}

Buffer<uint> index : register(t0);
Buffer<float4> vertex : register(t1);
AppendStructuredBuffer<uint4> pickresult : register(u0);

bool TriangleRayIntersect(float3 A, float3 B, float3 C, float3 RayPosition, float3 RayDirection,
                          out float3 HitPosition)
{
  bool Result = false;

  float3 v0v1 = B - A;
  float3 v0v2 = C - A;
  float3 pvec = cross(RayDirection, v0v2);
  float det = dot(v0v1, pvec);

  // if the determinant is negative the triangle is backfacing, but we still take those!
  // if the determinant is close to 0, the ray misses the triangle
  if(abs(det) > 0)
  {
    float invDet = 1 / det;

    float3 tvec = RayPosition - A;
    float3 qvec = cross(tvec, v0v1);
    float u = dot(tvec, pvec) * invDet;
    float v = dot(RayDirection, qvec) * invDet;

    if(u >= 0 && u <= 1 && v >= 0 && u + v <= 1)
    {
      float t = dot(v0v2, qvec) * invDet;
      if(t > 0)
      {
        HitPosition = RayPosition + (RayDirection * t);
        Result = true;
      }
    }
  }

  return Result;
}

void trianglePath(uint threadID)
{
  uint vertid = uint(fmod(float(threadID), float(PickNumVerts)));

  uint vertid0 = 0;
  uint vertid1 = 0;
  uint vertid2 = 0;
  switch(PickMeshMode)
  {
    case MESH_TRIANGLE_LIST:
    {
      vertid *= 3;
      vertid0 = vertid;
      vertid1 = vertid + 1;
      vertid2 = vertid + 2;
      break;
    }
    case MESH_TRIANGLE_STRIP:
    {
      vertid0 = vertid;
      vertid1 = vertid + 1;
      vertid2 = vertid + 2;
      break;
    }
    case MESH_TRIANGLE_LIST_ADJ:
    {
      vertid *= 6;
      vertid0 = vertid;
      vertid1 = vertid + 2;
      vertid2 = vertid + 4;
      break;
    }
    case MESH_TRIANGLE_STRIP_ADJ:
    {
      vertid *= 2;
      vertid0 = vertid;
      vertid1 = vertid + 2;
      vertid2 = vertid + 4;
      break;
    }
  }

  float4 pos0 = PickIdx ? vertex[index[vertid0]] : vertex[vertid0];
  float4 pos1 = PickIdx ? vertex[index[vertid1]] : vertex[vertid1];
  float4 pos2 = PickIdx ? vertex[index[vertid2]] : vertex[vertid2];

  float3 hitPosition;
  bool hit;
  if(PickUnproject == 1)
  {
    hit = TriangleRayIntersect(pos0.xyz / pos0.w, pos1.xyz / pos1.w, pos2.xyz / pos2.w, PickRayPos,
                               PickRayDir,
                               /*out*/ hitPosition);
  }
  else
  {
    hit = TriangleRayIntersect(pos0.xyz, pos1.xyz, pos2.xyz, PickRayPos, PickRayDir,
                               /*out*/ hitPosition);
  }

  // ray hit a triangle, so return the vertex that was closest
  // to the triangle/ray intersection point
  if(hit)
  {
    float dist0 = distance(pos0.xyz / pos0.w, hitPosition);
    float dist1 = distance(pos1.xyz / pos1.w, hitPosition);
    float dist2 = distance(pos2.xyz / pos2.w, hitPosition);

    uint meshVert = vertid0;
    if(dist1 < dist0 && dist1 < dist2)
    {
      meshVert = vertid1;
    }
    else if(dist2 < dist0 && dist2 < dist1)
    {
      meshVert = vertid2;
    }
    pickresult.Append(
        uint4(meshVert, asuint(hitPosition.x), asuint(hitPosition.y), asuint(hitPosition.z)));
  }
}

void defaultPath(uint threadID)
{
  uint vertid = threadID;

  if(vertid >= PickNumVerts)
    return;

  uint idx = PickIdx ? index[vertid] : vertid;

  float4 pos = vertex[idx];

  float4 wpos = mul(pos, PickMVP);

  if(PickUnproject == 1)
    wpos.xyz /= wpos.www;

  wpos.xy *= float2(1.0f, -1.0f);

  float2 scr = (wpos.xy + 1.0f) * 0.5f * PickViewport;

  // close to target co-ords? add to list
  float len = length(scr - PickCoords);
  if(len < 35.0f)
  {
    pickresult.Append(uint4(vertid, idx, asuint(len), asuint(wpos.z)));
  }
}

[numthreads(1024, 1, 1)] void RENDERDOC_MeshPickCS(uint3 tid : SV_DispatchThreadID)
{
  if(PickMeshMode == MESH_OTHER)
  {
    defaultPath(tid.x);
  }
  else
  {
    trianglePath(tid.x);
  }
}
