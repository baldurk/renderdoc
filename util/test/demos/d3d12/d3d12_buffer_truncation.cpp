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

RD_TEST(D3D12_Buffer_Truncation, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Tests using a constant buffer that is truncated by range, as well as "
      "vertex/index buffers truncated by size.";

  std::string vertex = R"EOSHADER(

struct vertin
{
	float3 pos : POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

struct v2f
{
	float4 svpos : SV_POSITION;
	float4 pos : OUTPOSITION;
	float4 col : OUTCOLOR;
};

v2f main(vertin IN)
{
	v2f OUT = (v2f)0;

	OUT.svpos = OUT.pos = float4(IN.pos.xyz, 1);
	OUT.col = IN.col;

	return OUT;
}

)EOSHADER";

  std::string pixel = R"EOSHADER(

cbuffer consts : register(b0)
{
  float4 padding[16];
  float4 outcol;
};

float4 main() : SV_Target0
{
	return outcol;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(vertex, "main", "vs_5_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_5_0");

    const DefaultA2V OffsetTri[] = {
        {Vec3f(7.7f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(7.7f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(7.7f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(9.9f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},

        {Vec3f(8.8f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},

        {Vec3f(3.3f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(3.3f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(3.3f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(3.3f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(3.3f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(3.3f, 0.0f, 0.0f), Vec4f(0.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
    };
    uint16_t indices[] = {99, 99, 99, 1, 2, 3, 4, 5, 88, 88, 88, 88, 88};
    Vec4f cbufferdata[64] = {};
    cbufferdata[32] = Vec4f(1.0f, 2.0f, 3.0f, 4.0f);

    ID3D12ResourcePtr vb = MakeBuffer().Data(OffsetTri);
    ID3D12ResourcePtr ib = MakeBuffer().Data(indices);
    ID3D12ResourcePtr cb = MakeBuffer().Data(cbufferdata);

    ID3D12RootSignaturePtr sig = MakeSig({
        tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 0, 1),
    });

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob).RTVs(
        {DXGI_FORMAT_R32G32B32A32_FLOAT});

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
    ResourceBarrier(ib, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_INDEX_BUFFER);
    ResourceBarrier(cb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbview;
    cbview.BufferLocation = cb->GetGPUVirtualAddress() + sizeof(Vec4f) * 16;
    cbview.SizeInBytes = sizeof(Vec4f) * 16;
    dev->CreateConstantBufferView(&cbview, m_CBVUAVSRV->GetCPUDescriptorHandleForHeapStart());

    ID3D12ResourcePtr rtvtex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, screenWidth, screenHeight)
                                   .RTV()
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE bbrtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, bbrtv, {0.2f, 0.2f, 0.2f, 1.0f});

      D3D12_CPU_DESCRIPTOR_HANDLE offrtv = MakeRTV(rtvtex).CreateCPU(0);

      ClearRenderTargetView(cmd, offrtv, {0.2f, 0.2f, 0.2f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      D3D12_VERTEX_BUFFER_VIEW vbview;
      vbview.BufferLocation = vb->GetGPUVirtualAddress() + sizeof(DefaultA2V) * 3;
      vbview.SizeInBytes = sizeof(DefaultA2V) * 5;
      vbview.StrideInBytes = sizeof(DefaultA2V);
      cmd->IASetVertexBuffers(0, 1, &vbview);

      D3D12_INDEX_BUFFER_VIEW ibview;
      ibview.BufferLocation = ib->GetGPUVirtualAddress() + sizeof(uint16_t) * 3;
      ibview.SizeInBytes = sizeof(uint16_t) * 5;
      ibview.Format = DXGI_FORMAT_R16_UINT;
      cmd->IASetIndexBuffer(&ibview);

      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);
      cmd->SetGraphicsRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {offrtv}, {});

      cmd->DrawIndexedInstanced(6, 1, 0, 0, 0);

      ResourceBarrier(cmd, rtvtex, D3D12_RESOURCE_STATE_RENDER_TARGET,
                      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

      blitToSwap(cmd, rtvtex, bb);

      ResourceBarrier(cmd, rtvtex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                      D3D12_RESOURCE_STATE_RENDER_TARGET);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
