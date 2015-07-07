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

struct wireframeV2F
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

wireframeV2F RENDERDOC_WireframeHomogVS(meshA2V IN, uint vid : SV_VertexID)
{
	wireframeV2F OUT = (wireframeV2F)0;
	OUT.pos = mul(IN.pos, ModelViewProj);

	float2 psprite[4] =
	{
		float2(-1.0f, -1.0f),
		float2(-1.0f,  1.0f),
		float2( 1.0f, -1.0f),
		float2( 1.0f,  1.0f)
	};

	OUT.pos.xy += SpriteSize.xy*0.01f*psprite[vid%4]*OUT.pos.w;
	OUT.secondary = IN.secondary;

	return OUT;
}

wireframeV2F RENDERDOC_MeshVS(meshA2V IN, uint vid : SV_VertexID)
{
	wireframeV2F OUT = (wireframeV2F)0;

	OUT.pos = mul(float4(IN.pos.xyz, 1), ModelViewProj);
	OUT.norm = float3(0, 0, 1);
	OUT.secondary = IN.secondary;

	return OUT;
}

[maxvertexcount(3)]
void RENDERDOC_MeshGS(triangle wireframeV2F input[3], inout TriangleStream<wireframeV2F> TriStream)
{
    wireframeV2F output;

    float4 faceEdgeA = mul(input[1].pos, InvProj) - mul(input[0].pos, InvProj);
    float4 faceEdgeB = mul(input[2].pos, InvProj) - mul(input[0].pos, InvProj);
    float3 faceNormal = normalize( cross(faceEdgeA.xyz, faceEdgeB.xyz) );

    for(int i=0; i<3; i++)
    {
        output.pos = input[i].pos;
        output.norm = faceNormal;
        output.secondary = input[i].secondary;
        TriStream.Append(output);
    }
    TriStream.RestartStrip();
}

float4 RENDERDOC_MeshPS(wireframeV2F IN) : SV_Target0
{
	uint type = OutputDisplayFormat;

	if(type == MESHDISPLAY_SECONDARY)
		return float4(IN.secondary.xyz, 1);
	else if(type == MESHDISPLAY_SECONDARY_ALPHA)
		return float4(IN.secondary.www, 1);
	else if(type == MESHDISPLAY_FACELIT)
	{
		float3 lightDir = normalize(float3(0, -0.3f, -1));

		return float4(WireframeColour.xyz*abs(dot(lightDir, IN.norm)), 1);
	}
	else //if(type == MESHDISPLAY_SOLID)
		return float4(WireframeColour.xyz, 1);
}

wireframeV2F RENDERDOC_WireframeVS(float3 pos : POSITION, uint vid : SV_VertexID)
{
	wireframeV2F OUT = (wireframeV2F)0;
	OUT.pos = mul(float4(pos, 1), ModelViewProj);

	float2 psprite[4] =
	{
		float2(-1.0f, -1.0f),
		float2(-1.0f,  1.0f),
		float2( 1.0f, -1.0f),
		float2( 1.0f,  1.0f)
	};

	OUT.pos.xy += SpriteSize.xy*0.01f*psprite[vid%4]*OUT.pos.w;

	return OUT;
}

Buffer<uint> index : register(t0);
Buffer<float4> vertex : register(t1);
AppendStructuredBuffer<uint4> pickresult : register(u0);

cbuffer MeshPickData : register(b0)
{
	float2 PickCoords;
	float2 PickViewport;

	row_major float4x4 PickMVP;

	uint PickIdx;
	uint PickNumVerts;
	uint2 PickPadding;
};

[numthreads(1024, 1, 1)]
void RENDERDOC_MeshPickCS(uint3 tid : SV_DispatchThreadID)
{
	uint vertid = tid.x;

	if(vertid >= PickNumVerts)
		return;

	uint idx = PickIdx ? index[vertid] : vertid;

	float4 pos = vertex[idx];

	float4 wpos = mul(pos, PickMVP);

	wpos.xyz /= wpos.w;

	float2 scr = (wpos.xy*float2(1.0f, -1.0f) + 1.0f) * 0.5f * PickViewport;

	// close to target co-ords? add to list
	float len = length(scr - PickCoords);
	if(len < 25.0f)
		pickresult.Append(uint4(vertid, idx, asuint(len), asuint(wpos.z)));
}