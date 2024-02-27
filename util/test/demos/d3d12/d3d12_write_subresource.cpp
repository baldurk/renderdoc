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

RD_TEST(D3D12_Write_Subresource, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Tests using WriteSubresource to update a mapped resource";

  std::string pixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

Texture2D<float4> intex : register(t0);
SamplerState s : register(s0);

float4 main(v2f IN) : SV_Target0
{
	return intex.Sample(s, IN.uv);
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_4_0");

    const DefaultA2V verts[4] = {
        {Vec3f(-1.0f, -1.0f, 0.0f), Vec4f(1.0f, 0.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(-1.0f, 1.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(1.0f, -1.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 1.0f)},
        {Vec3f(1.0f, 1.0f, 0.0f), Vec4f(0.0f, 0.0f, 1.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    ID3D12ResourcePtr vb = MakeBuffer().Data(verts);

    D3D12_STATIC_SAMPLER_DESC staticSamp = {};
    staticSamp.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamp.AddressU = staticSamp.AddressV = staticSamp.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamp.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    ID3D12RootSignaturePtr sig = MakeSig(
        {
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1),
        },
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, 1, &staticSamp);

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    D3D12_HEAP_PROPERTIES heap = {};
    heap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
    heap.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
    heap.Type = D3D12_HEAP_TYPE_CUSTOM;

    uint32_t *texData = new uint32_t[2048 * 2084];

    ID3D12ResourcePtr tex = MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 2048, 2048)
                                .CustomHeap(heap)
                                .Mips(1)
                                .InitialState(D3D12_RESOURCE_STATE_COMMON);

    D3D12_GPU_DESCRIPTOR_HANDLE view =
        MakeSRV(tex).Format(DXGI_FORMAT_R8G8B8A8_UNORM).PlaneSlice(0).CreateGPU(0);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      GPUSync();

      tex->Map(0, NULL, NULL);

      memset(texData, 0, 2048 * 2048 * sizeof(uint32_t));
      tex->WriteToSubresource(0, NULL, texData, 1024 * 4, 1024 * 1024);
      D3D12_BOX box = {400, 400, 0, 1600, 1600, 1};
      memset(texData, 0xff, 2048 * 2048 * sizeof(uint32_t));
      tex->WriteToSubresource(0, &box, texData, 1024 * 4, 1024 * 1024);

      tex->Unmap(0, NULL);

      GPUSync();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);
      cmd->SetGraphicsRootDescriptorTable(0, view);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {rtv}, {});

      cmd->DrawInstanced(4, 1, 0, 0);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
