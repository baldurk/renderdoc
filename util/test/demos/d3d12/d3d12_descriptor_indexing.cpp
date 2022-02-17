/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2019-2022 Baldur Karlsson
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
	bufs[buf_idx][4] = make_tex_ref(100, 100);
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
      if(t.tex == 100) break;

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

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob[2];
    ID3DBlobPtr psblob[2];
    ID3DBlobPtr csblob[2];
    vsblob[0] = Compile(D3DDefaultVertex, "main", "vs_4_0");
    psblob[0] = Compile(pixel, "main", "ps_5_1");
    csblob[0] = Compile(compute, "main", "cs_5_1");
    if(m_DXILSupport)
    {
      vsblob[1] = Compile(D3DDefaultVertex, "main", "vs_6_0");
      psblob[1] = Compile(pixel, "main", "ps_6_0");
      csblob[1] = Compile(compute, "main", "cs_6_0");
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

    ID3D12RootSignaturePtr graphicssig = MakeSig(
        {
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 20, 0),
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 150, 0),
        },
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, 1, &staticSamp);

    ID3D12PipelineStatePtr graphicspso[2];
    ID3D12PipelineStatePtr computepso[2];
    computepso[0] = MakePSO().RootSig(computesig).CS(csblob[0]);
    graphicspso[0] = MakePSO().RootSig(graphicssig).InputLayout().VS(vsblob[0]).PS(psblob[0]);
    if(m_DXILSupport)
    {
      computepso[1] = MakePSO().RootSig(computesig).CS(csblob[1]);
      graphicspso[1] = MakePSO().RootSig(graphicssig).InputLayout().VS(vsblob[1]).PS(psblob[1]);
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

    for(int i = 0; i < 150; i++)
    {
      MakeSRV(blacktex).CreateGPU(i);
      MakeSRV(blacktex).CreateCPU(i);
    }

    ID3D12ResourcePtr structBuf = MakeBuffer().UAV().Size(8192);
    D3D12_GPU_DESCRIPTOR_HANDLE structGPU =
        MakeUAV(structBuf).Format(DXGI_FORMAT_R32_UINT).CreateGPU(15);
    D3D12_CPU_DESCRIPTOR_HANDLE structCPU =
        MakeUAV(structBuf).Format(DXGI_FORMAT_R32_UINT).CreateClearCPU(15);
    MakeSRV(structBuf).StructureStride(2 * sizeof(uint32_t)).CreateGPU(8);

    MakeSRV(smiley).CreateGPU(12);
    MakeSRV(smiley).CreateGPU(19);
    MakeSRV(smiley).CreateGPU(20);
    MakeSRV(smiley).CreateGPU(21);
    MakeSRV(smiley).CreateGPU(49);
    MakeSRV(smiley).CreateGPU(59);
    MakeSRV(smiley).CreateGPU(99);
    MakeSRV(smiley).CreateGPU(103);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      const char *markers[] = {"Tests sm_5_1", "Tests sm_6_0"};

      for(int i = 0; i < 2; i++)
      {
        if(!computepso[i])
          continue;

        setMarker(cmd, markers[i]);

        UINT zero[4] = {};
        cmd->ClearUnorderedAccessViewUint(structGPU, structCPU, structBuf, zero, 0, NULL);

        cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

        cmd->SetPipelineState(computepso[i]);
        cmd->SetComputeRootSignature(computesig);
        cmd->SetComputeRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->SetComputeRoot32BitConstant(1, 15, 0);

        cmd->Dispatch(1, 1, 1);

        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
        cmd->SetPipelineState(graphicspso[i]);
        cmd->SetGraphicsRootSignature(graphicssig);
        cmd->SetGraphicsRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
        cmd->SetGraphicsRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());

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
