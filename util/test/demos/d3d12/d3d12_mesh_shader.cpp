/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2024 Baldur Karlsson
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

#include "d3d12_test.h"

// subobject headers have to be aligned to pointer boundaries
#define SUBOBJECT_HEADER(subobj)                                               \
  D3D12_PIPELINE_STATE_SUBOBJECT_TYPE alignas(void *) CONCAT(header, subobj) = \
      CONCAT(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_, subobj);

struct GraphicsStreamData
{
  // graphics properties
  SUBOBJECT_HEADER(ROOT_SIGNATURE);
  ID3D12RootSignature *pRootSignature = NULL;
  SUBOBJECT_HEADER(VS);
  D3D12_SHADER_BYTECODE VS = {};
  SUBOBJECT_HEADER(AS);
  D3D12_SHADER_BYTECODE AS = {};
  SUBOBJECT_HEADER(MS);
  D3D12_SHADER_BYTECODE MS = {};
  SUBOBJECT_HEADER(PS);
  D3D12_SHADER_BYTECODE PS = {};
  SUBOBJECT_HEADER(DS);
  D3D12_SHADER_BYTECODE DS = {};
  SUBOBJECT_HEADER(HS);
  D3D12_SHADER_BYTECODE HS = {};
  SUBOBJECT_HEADER(GS);
  D3D12_SHADER_BYTECODE GS = {};
  SUBOBJECT_HEADER(RENDER_TARGET_FORMATS);
  D3D12_RT_FORMAT_ARRAY RTVFormats = {};
  SUBOBJECT_HEADER(DEPTH_STENCIL_FORMAT);
  DXGI_FORMAT DSVFormat = DXGI_FORMAT_UNKNOWN;
  SUBOBJECT_HEADER(PRIMITIVE_TOPOLOGY);
  D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
  SUBOBJECT_HEADER(IB_STRIP_CUT_VALUE);
  D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
  SUBOBJECT_HEADER(NODE_MASK);
  UINT NodeMask = 0;
  SUBOBJECT_HEADER(SAMPLE_MASK);
  UINT SampleMask = 0;
  SUBOBJECT_HEADER(RASTERIZER);
  D3D12_RASTERIZER_DESC RasterizerState;
  SUBOBJECT_HEADER(FLAGS);
  D3D12_PIPELINE_STATE_FLAGS Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  SUBOBJECT_HEADER(BLEND);
  D3D12_BLEND_DESC BlendState = {};
  UINT pad0;
  SUBOBJECT_HEADER(SAMPLE_DESC);
  DXGI_SAMPLE_DESC SampleDesc = {};
  UINT pad1;
};

#undef SUBOBJECT_HEADER

std::string GlobalPayload_Shaders = R"EOSHADER(

struct Payload
{
  uint tri[2];
};

groupshared Payload sPayload;

[numthreads(2, 1, 1)]
void as_amplify(uint gtid : SV_GroupThreadID, uint dtid : SV_DispatchThreadID, uint gid : SV_GroupIndex)
{
  sPayload.tri[gid] = dtid;
  DispatchMesh(2, 1, 1, sPayload);
}

struct m2f
{
  float4 pos : SV_POSITION;
  float4 col : COLOR0;
  float2 uv : TEXCOORD0;
};

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void ms_amplify(uint gtid : SV_GroupThreadID, uint dtid : SV_DispatchThreadID, in payload Payload payload, out indices uint3 triangles[128], out vertices m2f vertices[64]) 
{
  SetMeshOutputCounts(3, 1);

	uint tri = payload.tri[dtid];
	uint vertIdx = 0;
	triangles[0] = uint3(0+vertIdx, 1+vertIdx, 2+vertIdx);

	float4 org = float4(-0.65, 0.0, 0.0, 0.0) + float4(0.42, 0.0, 0.0, 0.0) * tri;
	vertices[0+vertIdx].pos = float4(-0.2, -0.2, 0.0, 1.0) + org;
	vertices[0+vertIdx].col = float4(0.0, 1.0, 0.0, 1.0);
	vertices[0+vertIdx].uv = float2(0.0, 0.0);

	vertices[1+vertIdx].pos = float4(0.0, 0.2, 0.0, 1.0) + org;
	vertices[1+vertIdx].col = float4(0.0, 1.0, 0.0, 1.0);
	vertices[1+vertIdx].uv = float2(0.0, 1.0);

	vertices[2+vertIdx].pos = float4(0.2, -0.2, 0.0, 1.0) + org;
	vertices[2+vertIdx].col = float4(0.0, 1.0, 0.0, 1.0);
	vertices[2+vertIdx].uv = float2(1.0, 0.0);
}

)EOSHADER";

std::string LocalPayload_Shaders = R"EOSHADER(

struct Payload
{
  uint tri[4];
};

[numthreads(1, 1, 1)]
void as_amplify(uint gtid : SV_GroupThreadID, uint dtid : SV_DispatchThreadID, uint gid : SV_GroupIndex)
{
  Payload sPayload;
  sPayload.tri[0] = 0;
  sPayload.tri[1] = 1;
  sPayload.tri[2] = 2;
  sPayload.tri[3] = 3;
  DispatchMesh(4, 1, 1, sPayload);
}

struct m2f
{
  float4 pos : SV_POSITION;
  float4 col : COLOR0;
  float2 uv : TEXCOORD0;
};

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void ms_amplify(uint gtid : SV_GroupThreadID, uint dtid : SV_DispatchThreadID, in payload Payload payload, out indices uint3 triangles[128], out vertices m2f vertices[64]) 
{
  SetMeshOutputCounts(3, 1);

	uint tri = payload.tri[dtid];
	uint vertIdx = 0;
	triangles[0] = uint3(0+vertIdx, 1+vertIdx, 2+vertIdx);

	float4 org = float4(-0.65, -0.65, 0.0, 0.0) + float4(0.42, 0.0, 0.0, 0.0) * tri;
	vertices[0+vertIdx].pos = float4(-0.2, -0.2, 0.0, 1.0) + org;
	vertices[0+vertIdx].col = float4(0.0, 0.0, 1.0, 1.0);
	vertices[0+vertIdx].uv = float2(0.0, 0.0);

	vertices[1+vertIdx].pos = float4(0.0, 0.2, 0.0, 1.0) + org;
	vertices[1+vertIdx].col = float4(0.0, 0.0, 1.0, 1.0);
	vertices[1+vertIdx].uv = float2(0.0, 1.0);

	vertices[2+vertIdx].pos = float4(0.2, -0.2, 0.0, 1.0) + org;
	vertices[2+vertIdx].col = float4(0.0, 0.0, 1.0, 1.0);
	vertices[2+vertIdx].uv = float2(1.0, 0.0);
}

)EOSHADER";

std::string SimpleMeshShader = R"EOSHADER(

struct m2f
{
  float4 pos : SV_POSITION;
  float4 col : COLOR0;
  float2 uv : TEXCOORD0;
};

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void ms_simple(in uint gid : SV_GroupID, out indices uint3 triangles[2], out vertices m2f vertices[6]) 
{
  SetMeshOutputCounts(6, 2);

	for (uint i = 0; i < 2; i++)
	{
		uint tri = i;
    uint vertIdx = tri * 3;
		triangles[tri] = uint3(0+vertIdx, 1+vertIdx, 2+vertIdx);
    tri += 2 * gid;

		float4 org = float4(-0.65, +0.65, 0.0, 0.0) + float4(0.42, 0.0, 0.0, 0.0) * tri;
		vertices[0+vertIdx].pos = float4(-0.2, -0.2, 0.0, 1.0) + org;
		vertices[0+vertIdx].col = float4(1.0, 0.0, 0.0, 1.0);
		vertices[0+vertIdx].uv = float2(0.0, 0.0);

		vertices[1+vertIdx].pos = float4(0.0, 0.2, 0.0, 1.0) + org;
		vertices[1+vertIdx].col = float4(1.0, 0.0, 0.0, 1.0);
		vertices[1+vertIdx].uv = float2(0.0, 1.0);

		vertices[2+vertIdx].pos = float4(0.2, -0.2, 0.0, 1.0) + org;
		vertices[2+vertIdx].col = float4(1.0, 0.0, 0.0, 1.0);
		vertices[2+vertIdx].uv = float2(1.0, 0.0);
  }
}

)EOSHADER";

RD_TEST(D3D12_Mesh_Shader, D3D12GraphicsTest)
{
  static constexpr const char *Description = "Draws geometry using mesh shader pipeline.";

  void Prepare(int argc, char **argv)
  {
    D3D12GraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    if(opts7.MeshShaderTier == D3D12_MESH_SHADER_TIER_NOT_SUPPORTED)
      Avail = "Mesh Shaders are not supported";
  }

  ID3D12PipelineStatePtr CreatePipeline(const D3D12PSOCreator &psoData) const
  {
    GraphicsStreamData graphicsStreamData;
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC &GraphicsDesc = psoData.GraphicsDesc;

    graphicsStreamData.pRootSignature = GraphicsDesc.pRootSignature;
    graphicsStreamData.VS = GraphicsDesc.VS;
    graphicsStreamData.AS = psoData.GetAS();
    graphicsStreamData.MS = psoData.GetMS();
    graphicsStreamData.PS = GraphicsDesc.PS;
    graphicsStreamData.DS = GraphicsDesc.DS;
    graphicsStreamData.HS = GraphicsDesc.HS;
    graphicsStreamData.GS = GraphicsDesc.GS;
    graphicsStreamData.BlendState = GraphicsDesc.BlendState;
    graphicsStreamData.SampleMask = GraphicsDesc.SampleMask;
    graphicsStreamData.IBStripCutValue = GraphicsDesc.IBStripCutValue;
    graphicsStreamData.PrimitiveTopologyType = GraphicsDesc.PrimitiveTopologyType;
    for(uint32_t i = 0; i < 8; ++i)
      graphicsStreamData.RTVFormats.RTFormats[i] = GraphicsDesc.RTVFormats[i];
    graphicsStreamData.RTVFormats.NumRenderTargets = GraphicsDesc.NumRenderTargets;

    graphicsStreamData.DSVFormat = GraphicsDesc.DSVFormat;
    graphicsStreamData.SampleDesc = GraphicsDesc.SampleDesc;
    graphicsStreamData.NodeMask = GraphicsDesc.NodeMask;
    graphicsStreamData.Flags = GraphicsDesc.Flags;

    graphicsStreamData.RasterizerState = GraphicsDesc.RasterizerState;

    D3D12_PIPELINE_STATE_STREAM_DESC streamDesc;
    streamDesc.pPipelineStateSubobjectStream = &graphicsStreamData;
    streamDesc.SizeInBytes = sizeof(GraphicsStreamData);

    ID3D12PipelineStatePtr pso;
    dev2->CreatePipelineState(&streamDesc, __uuidof(ID3D12PipelineState), (void **)&pso);

    return pso;
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr as_globalpayload_blob = Compile(GlobalPayload_Shaders, "as_amplify", "as_6_5");
    ID3DBlobPtr ms_globalpayload_blob = Compile(GlobalPayload_Shaders, "ms_amplify", "ms_6_5");
    ID3DBlobPtr as_localpayload_blob = Compile(LocalPayload_Shaders, "as_amplify", "as_6_5");
    ID3DBlobPtr ms_localpayload_blob = Compile(LocalPayload_Shaders, "ms_amplify", "ms_6_5");
    ID3DBlobPtr msblob = Compile(SimpleMeshShader, "ms_simple", "ms_6_5");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_6_5");

    ID3D12RootSignaturePtr sig = MakeSig({});

    ID3D12PipelineStatePtr psos[] = {
        CreatePipeline(MakePSO().RootSig(sig).InputLayout().MS(msblob).PS(psblob)),
        CreatePipeline(MakePSO()
                           .RootSig(sig)
                           .InputLayout()
                           .AS(as_globalpayload_blob)
                           .MS(ms_globalpayload_blob)
                           .PS(psblob)),
        CreatePipeline(
            MakePSO().RootSig(sig).InputLayout().AS(as_localpayload_blob).MS(ms_localpayload_blob).PS(psblob)),
    };

    while(Running())
    {
      ID3D12GraphicsCommandList6Ptr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      setMarker(cmd, "Mesh Shaders");
      for(size_t i = 0; i < ARRAY_COUNT(psos); i++)
      {
        cmd->SetPipelineState(psos[i]);
        cmd->SetGraphicsRootSignature(sig);

        RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

        OMSetRenderTargets(cmd, {rtv}, {});
        if(i < 2)
          cmd->DispatchMesh(2, 1, 1);
        else
          cmd->DispatchMesh(1, 1, 1);
      }

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
