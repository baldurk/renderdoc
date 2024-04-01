/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2024 Baldur Karlsson
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

RD_TEST(D3D12_Descriptor_Indexing, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Tests the use of descriptor indexing at runtime to test bindless feedback";

  std::string compute = R"EOSHADER(

struct tex_ref
{
  uint tex;
  uint binding;
};

tex_ref make_tex_ref(uint tex, uint binding)
{
  tex_ref ret;
  ret.tex = tex;
  ret.binding = binding;
  return ret;
}

cbuffer rootconst
{
  uint buf_idx;
};

RWStructuredBuffer<tex_ref> bufs[32] : register(u0);

[numthreads(1,1,1)]
void main()
{
	bufs[buf_idx][0] = make_tex_ref(0, 19);
	bufs[buf_idx][1] = make_tex_ref(1, 9);
	bufs[buf_idx][2] = make_tex_ref(2, 19);
	bufs[buf_idx][3] = make_tex_ref(2, 23);
	bufs[buf_idx][4] = make_tex_ref(3, 6);
	bufs[buf_idx][5] = make_tex_ref(3, 12);
	bufs[buf_idx][6] = make_tex_ref(100, 100);
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

struct tex_ref
{
  uint tex;
  uint binding;
};

StructuredBuffer<tex_ref> buf : register(t8);

SamplerState s : register(s0);
Texture2D<float4> fixedtex : register(t12);

Texture2D<float4> texArray1[32] : register(t0, space1);
Texture2D<float4> texArray2[32] : register(t40, space1);
Texture2D<float4> texArray3[32] : register(t80, space1);

struct alias1
{
  float4 Color;
  float4 ignored;
  float4 also_ignored;
};

struct alias2
{
  float4 ignored;
  float4 also_ignored;
  float4 Color;
};

float4 main(v2f IN) : SV_Target0
{
  if(IN.uv.y < 0.1f)
  {
    return fixedtex.Sample(s, IN.uv.xy*5.0f);
  }
  else
  {
    float2 uv = IN.uv.xy;

    float4 ret = float4(1,1,1,1);
    for(int i=0; i < 100; i++)
    {
      tex_ref t = buf[i];
      if(t.tex == 100)
      {
        ret += texArray1[t.binding*100].SampleLevel(s, uv.xy, 0);
        break;
      }

      if(t.tex == 0)
      {
        ret *= texArray1[t.binding].SampleLevel(s, uv.xy, 0);
        ret *= texArray1[t.binding+1].SampleLevel(s, uv.xy, 0);
        ret *= texArray1[t.binding+2].SampleLevel(s, uv.xy, 0);
      }
      else if(t.tex == 1)
      {
        ret *= texArray2[t.binding].SampleLevel(s, uv.xy, 0);
        ret *= texArray2[t.binding+10].SampleLevel(s, uv.xy, 0);
        ret *= texArray2[20].SampleLevel(s, uv.xy, 0);
      }
      else if(t.tex == 2)
      {
        ret *= texArray3[t.binding].SampleLevel(s, uv.xy, 0);
      }

      uv *= 1.8f;
    }

    return ret;
  }
}

)EOSHADER";

  std::string pixel6_6Heap = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

struct tex_ref
{
  uint tex;
  uint binding;
};

struct CBuffer
{
  uint tex_idx;
};

struct alias1
{
  float4 Color;
  float4 ignored;
  float4 also_ignored;
};

struct alias2
{
  float4 ignored;
  float4 also_ignored;
  float4 Color;
};

float4 main(v2f IN) : SV_Target0
{
  StructuredBuffer<tex_ref> buf = ResourceDescriptorHeap[8];
  if(IN.uv.y < 0.1f)
  {
    SamplerState s = SamplerDescriptorHeap[0];
    Texture2D<float4> fixedtex = ResourceDescriptorHeap[12];
    return fixedtex.Sample(s, IN.uv.xy*5.0f);
  }
  else
  {
    float2 uv = IN.uv.xy;

    float4 ret = float4(1,1,1,1);
    for(int i=0; i < 100; i++)
    {
      tex_ref t = buf[i];
      if(t.tex == 100) break;

      if(t.tex == 0)
      {
        SamplerState s1 = SamplerDescriptorHeap[0];
        SamplerState s2 = SamplerDescriptorHeap[1];
        SamplerState s3 = SamplerDescriptorHeap[2];
        Texture2D<float4> tex1 = ResourceDescriptorHeap[t.binding];
        Texture2D<float4> tex2 = ResourceDescriptorHeap[t.binding+1];
        Texture2D<float4> tex3 = ResourceDescriptorHeap[t.binding+2];
        ret *= tex1.SampleLevel(s1, uv.xy, 0);
        ret *= tex2.SampleLevel(s2, uv.xy, 0);
        ret *= tex3.SampleLevel(s3, uv.xy, 0);
        RWStructuredBuffer<uint> uav = ResourceDescriptorHeap[10];
        uav[0] = t.binding;
      }
      else if(t.tex == 1)
      {
        SamplerState s1 = SamplerDescriptorHeap[4];
        SamplerState s2 = SamplerDescriptorHeap[5];
        SamplerState s3 = SamplerDescriptorHeap[6];
        Texture2D<float4> tex1 = ResourceDescriptorHeap[40+t.binding];
        Texture2D<float4> tex2 = ResourceDescriptorHeap[40+t.binding+10];
        ConstantBuffer<CBuffer> cbv = ResourceDescriptorHeap[9];
        Texture2D<float4> tex3 = ResourceDescriptorHeap[cbv.tex_idx];
        ret *= tex1.SampleLevel(s1, uv.xy, 0);
        ret *= tex2.SampleLevel(s2, uv.xy, 0);
        ret *= tex3.SampleLevel(s3, uv.xy, 0);
      }
      else if(t.tex == 2)
      {
        SamplerState s = SamplerDescriptorHeap[7];
        Texture2D<float4> tex = ResourceDescriptorHeap[80+t.binding];
        ret *= tex.SampleLevel(s, uv.xy, 0);
      }
      else if(t.tex == 3)
      {
        StructuredBuffer<alias1> alias1buf = ResourceDescriptorHeap[150+t.binding];
        ret *= alias1buf[0].Color;
      }
      else if(t.tex == 4)
      {
        StructuredBuffer<alias2> alias2buf = ResourceDescriptorHeap[150+t.binding];
        ret *= alias2buf[0].Color;
      }

      uv *= 1.8f;
    }

    return ret;
  }
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    bool supportSM66 = m_HighestShaderModel >= D3D_SHADER_MODEL_6_6;
    ID3DBlobPtr vsblob[3];
    ID3DBlobPtr psblob[4];
    ID3DBlobPtr csblob[3];
    vsblob[0] = Compile(D3DDefaultVertex, "main", "vs_4_0");
    psblob[0] = Compile(pixel, "main", "ps_5_1");
    csblob[0] = Compile(compute, "main", "cs_5_1");
    if(m_DXILSupport)
    {
      vsblob[1] = Compile(D3DDefaultVertex, "main", "vs_6_0");
      psblob[1] = Compile(pixel, "main", "ps_6_0");
      csblob[1] = Compile(compute, "main", "cs_6_0");
      if(supportSM66)
      {
        vsblob[2] = Compile(D3DDefaultVertex, "main", "vs_6_6");
        psblob[2] = Compile(pixel, "main", "ps_6_6");
        csblob[2] = Compile(compute, "main", "cs_6_6");

        psblob[3] = Compile(pixel6_6Heap, "main", "ps_6_6");
      }
    }

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);

    ID3D12RootSignaturePtr computesig = MakeSig({
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 32, 0),
        constParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0, 1),
    });

    D3D12_STATIC_SAMPLER_DESC staticSamp = {};
    staticSamp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamp.AddressU = staticSamp.AddressV = staticSamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    ID3D12RootSignaturePtr graphicssigs[4];
    graphicssigs[0] = graphicssigs[1] = graphicssigs[2] = MakeSig(
        {
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 20, 0),
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 150, 0),
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 15, 32, 150),
        },
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, 1, &staticSamp);
    ID3D12PipelineStatePtr graphicspso[4];
    ID3D12PipelineStatePtr computepso[4];
    computepso[0] = MakePSO().RootSig(computesig).CS(csblob[0]);
    graphicspso[0] = MakePSO().RootSig(graphicssigs[0]).InputLayout().VS(vsblob[0]).PS(psblob[0]);
    if(m_DXILSupport)
    {
      computepso[1] = MakePSO().RootSig(computesig).CS(csblob[1]);
      graphicspso[1] = MakePSO().RootSig(graphicssigs[1]).InputLayout().VS(vsblob[1]).PS(psblob[1]);
      if(supportSM66)
      {
        computepso[2] = MakePSO().RootSig(computesig).CS(csblob[2]);
        graphicspso[2] = MakePSO().RootSig(graphicssigs[2]).InputLayout().VS(vsblob[2]).PS(psblob[2]);

        computepso[3] = computepso[2];
        graphicssigs[3] = MakeSig({},
                                  D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                                      D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED |
                                      D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED,
                                  0, NULL);
        graphicspso[3] = MakePSO().RootSig(graphicssigs[3]).InputLayout().VS(vsblob[2]).PS(psblob[3]);
      }
    }

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ID3D12ResourcePtr blacktex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 16, 16)
                                     .InitialState(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                                                   D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    ID3D12ResourcePtr smiley = MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 48, 48)
                                   .Mips(1)
                                   .InitialState(D3D12_RESOURCE_STATE_COPY_DEST);

    ID3D12ResourcePtr uploadBuf = MakeBuffer().Size(1024 * 1024).Upload();
    ID3D12ResourcePtr constBuf = MakeBuffer().Size(256).Upload();
    ID3D12ResourcePtr outUAV = MakeBuffer().Size(256).UAV();
    {
      byte *mapptr = NULL;
      constBuf->Map(0, NULL, (void **)&mapptr);
      uint32_t value = 6;
      memcpy(mapptr, &value, sizeof(uint32_t));
      constBuf->Unmap(0, NULL);
    }

    {
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};

      D3D12_RESOURCE_DESC desc = smiley->GetDesc();

      dev->GetCopyableFootprints(&desc, 0, 1, 0, &layout, NULL, NULL, NULL);

      byte *srcptr = (byte *)rgba8.data.data();
      byte *mapptr = NULL;
      uploadBuf->Map(0, NULL, (void **)&mapptr);

      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      {
        D3D12_TEXTURE_COPY_LOCATION dst, src;

        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.pResource = smiley;
        dst.SubresourceIndex = 0;

        byte *dstptr = mapptr + layout.Offset;

        for(UINT row = 0; row < rgba8.height; row++)
        {
          memcpy(dstptr, srcptr, rgba8.width * sizeof(uint32_t));
          srcptr += rgba8.width * sizeof(uint32_t);
          dstptr += layout.Footprint.RowPitch;
        }

        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.pResource = uploadBuf;
        src.PlacedFootprint = layout;

        cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

        D3D12_RESOURCE_BARRIER b = {};
        b.Transition.pResource = smiley;
        b.Transition.Subresource = 0;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                                  D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        cmd->ResourceBarrier(1, &b);
      }

      cmd->Close();

      uploadBuf->Unmap(0, NULL);

      Submit({cmd});
      GPUSync();
    }

    ID3D12ResourcePtr aliasEmptyBuf = MakeBuffer().Size(192).Upload();
    ID3D12ResourcePtr alias1Buf = MakeBuffer().Size(192).Upload();
    ID3D12ResourcePtr alias2Buf = MakeBuffer().Size(192).Upload();

    // these correspond to alias1/alias2 structure types
    Vec4f aliasbuf_data[3] = {};
    {
      byte *mapptr = NULL;
      aliasEmptyBuf->Map(0, NULL, (void **)&mapptr);
      memcpy(mapptr, aliasbuf_data, sizeof(aliasbuf_data));
      aliasEmptyBuf->Unmap(0, NULL);
    }

    // first alias stores color first
    aliasbuf_data[0] = Vec4f(1.1f, 0.9f, 1.2f, 1.0f);

    {
      byte *mapptr = NULL;
      alias1Buf->Map(0, NULL, (void **)&mapptr);
      memcpy(mapptr, aliasbuf_data, sizeof(aliasbuf_data));
      alias1Buf->Unmap(0, NULL);
    }

    // second alias stores color last
    aliasbuf_data[0] = Vec4f();
    aliasbuf_data[2] = Vec4f(1.1f, 0.9f, 1.2f, 1.0f);

    {
      byte *mapptr = NULL;
      alias2Buf->Map(0, NULL, (void **)&mapptr);
      memcpy(mapptr, aliasbuf_data, sizeof(aliasbuf_data));
      alias2Buf->Unmap(0, NULL);
    }

    for(int i = 0; i < 150; i++)
    {
      MakeSRV(blacktex).CreateGPU(i);
      MakeSRV(blacktex).CreateCPU(i);
    }
    for(int i = 0; i < 32; i++)
    {
      MakeSRV(aliasEmptyBuf).StructureStride(3 * sizeof(Vec4f)).CreateGPU(150 + i);
      MakeSRV(aliasEmptyBuf).StructureStride(3 * sizeof(Vec4f)).CreateCPU(150 + i);
    }

    ID3D12ResourcePtr structBuf = MakeBuffer().UAV().Size(8192);
    D3D12_GPU_DESCRIPTOR_HANDLE structGPU =
        MakeUAV(structBuf).Format(DXGI_FORMAT_R32_UINT).CreateGPU(16);
    D3D12_CPU_DESCRIPTOR_HANDLE structCPU =
        MakeUAV(structBuf).Format(DXGI_FORMAT_R32_UINT).CreateClearCPU(16);
    MakeUAV(structBuf).StructureStride(2 * sizeof(uint32_t)).CreateGPU(15);
    MakeSRV(structBuf).StructureStride(2 * sizeof(uint32_t)).CreateGPU(8);

    MakeSRV(alias1Buf).StructureStride(3 * sizeof(Vec4f)).CreateGPU(150 + 6);
    MakeSRV(alias2Buf).StructureStride(3 * sizeof(Vec4f)).CreateGPU(150 + 12);

    MakeSRV(smiley).CreateGPU(12);
    MakeSRV(smiley).CreateGPU(19);
    MakeSRV(smiley).CreateGPU(20);
    MakeSRV(smiley).CreateGPU(21);
    MakeSRV(smiley).CreateGPU(49);
    MakeSRV(smiley).CreateGPU(59);
    MakeSRV(smiley).CreateGPU(60);
    MakeSRV(smiley).CreateGPU(99);
    MakeSRV(smiley).CreateGPU(103);
    MakeCBV(constBuf).SizeBytes(256).CreateGPU(9);
    MakeUAV(outUAV).Format(DXGI_FORMAT_R32_UINT).CreateGPU(10);

    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW =
        D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    UINT increment = dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    D3D12_CPU_DESCRIPTOR_HANDLE samplerStart = m_Sampler->GetCPUDescriptorHandleForHeapStart();
    for(int i = 0; i < 8; ++i)
      dev->CreateSampler(&samplerDesc, {samplerStart.ptr + increment * i});

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      const char *markers[] = {"Tests sm_5_1", "Tests sm_6_0", "Tests sm_6_6", "Tests sm_6_6_heap"};
      int testsToRun = supportSM66 ? 4 : 2;
      for(int i = 0; i < testsToRun; i++)
      {
        if(!computepso[i])
          continue;

        setMarker(cmd, markers[i]);

        ID3D12DescriptorHeap *heaps[] = {m_CBVUAVSRV.GetInterfacePtr(), m_Sampler.GetInterfacePtr()};
        cmd->SetDescriptorHeaps(2, heaps);

        UINT zero[4] = {};
        cmd->ClearUnorderedAccessViewUint(structGPU, structCPU, structBuf, zero, 0, NULL);
        cmd->SetPipelineState(computepso[i]);
        cmd->SetComputeRootSignature(computesig);
        cmd->SetComputeRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->SetComputeRoot32BitConstant(1, 15, 0);

        cmd->Dispatch(1, 1, 1);

        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
        cmd->SetPipelineState(graphicspso[i]);
        cmd->SetGraphicsRootSignature(graphicssigs[i]);
        if(i < 3)
        {
          cmd->SetGraphicsRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
          cmd->SetGraphicsRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        }

        RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

        OMSetRenderTargets(cmd, {rtv}, {});

        cmd->DrawInstanced(3, 1, 0, 0);
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
