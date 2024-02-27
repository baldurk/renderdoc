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

RD_TEST(D3D12_RenderTarget_Binds, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Tests the different combinations of how OMSetRenderTargets can be used.";

  std::string pixel = R"EOSHADER(

cbuffer rootconsts : register(b0)
{
  float4 col1;
  float4 col2;
};

void main(out float4 out1 : SV_Target0, out float4 out2 : SV_Target1)
{
  out1 = col1;
  out2 = col2;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_4_0");

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);

    ID3D12RootSignaturePtr sig = MakeSig({
        constParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0, 8),
    });

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob).RTVs(
        {backbufferFmt, backbufferFmt});

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    // 0-3 dynamic in frame, 4-7 'static', 8 is the one we shouldn't write to
    ID3D12ResourcePtr rtvtex[9];
    wchar_t name[] = L"TextureA";

    size_t badrt = ARRAY_COUNT(rtvtex) - 1;

    for(int i = 0; i < ARRAY_COUNT(rtvtex); i++)
      rtvtex[i] = MakeTexture(backbufferFmt, screenWidth, screenHeight)
                      .RTV()
                      .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      // clear the ones we want to use to green, and the one we don't want to use to black.
      // only do this once so we don't add unintended references to the textures in-frame
      for(int i = 0; i < ARRAY_COUNT(rtvtex) - 1; i++)
      {
        ClearRenderTargetView(cmd, MakeRTV(rtvtex[i]).CreateCPU(1), {0.0f, 1.0f, 0.0f, 1.0f});
        rtvtex[i]->SetName(name);
        name[7]++;
      }

      ClearRenderTargetView(cmd, MakeRTV(rtvtex[badrt]).CreateCPU(1), {0.0f, 0.0f, 0.0f, 1.0f});
      rtvtex[badrt]->SetName(L"NoWriteTexture");

      cmd->Close();

      Submit({cmd});
    }

    // pre-configure some tests to ensure we pull in these textures correctly

    D3D12_CPU_DESCRIPTOR_HANDLE directarray_static[2];
    directarray_static[0] = MakeRTV(rtvtex[4]).CreateCPU(10);
    MakeRTV(rtvtex[badrt]).CreateCPU(11);
    directarray_static[1] = MakeRTV(rtvtex[5]).CreateCPU(12);
    MakeRTV(rtvtex[badrt]).CreateCPU(13);

    D3D12_CPU_DESCRIPTOR_HANDLE indirectarray_static[2];
    indirectarray_static[0] = MakeRTV(rtvtex[6]).CreateCPU(14);
    MakeRTV(rtvtex[7]).CreateCPU(15);
    MakeRTV(rtvtex[badrt]).CreateCPU(16);
    indirectarray_static[1] = MakeRTV(rtvtex[badrt]).CreateCPU(17);

    bbTex[0]->SetName(L"Swapchain 0");
    bbTex[1]->SetName(L"Swapchain 1");

    Vec4f col;

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      // Set null render targets to ensure that these work properly when attached
      cmd->OMSetRenderTargets(0, NULL, FALSE, NULL);
      cmd->OMSetRenderTargets(0, NULL, TRUE, NULL);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE bbrtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(30);

      ClearRenderTargetView(cmd, bbrtv, {1.0f, 0.0f, 1.0f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      D3D12_CPU_DESCRIPTOR_HANDLE directarray_dynamic[2];

      // pass array of handles, and put the RTV we shouldn't render to after each one to ensure we
      // don't mis-interpret
      directarray_dynamic[0] = MakeRTV(rtvtex[0]).CreateCPU(0);
      MakeRTV(rtvtex[badrt]).CreateCPU(1);
      directarray_dynamic[1] = MakeRTV(rtvtex[1]).CreateCPU(2);
      MakeRTV(rtvtex[badrt]).CreateCPU(3);

      cmd->OMSetRenderTargets(2, directarray_dynamic, FALSE, NULL);

      // immediately trash the used descriptors - they should be consumed in the above call
      MakeRTV(rtvtex[badrt]).CreateCPU(0);
      MakeRTV(rtvtex[badrt]).CreateCPU(2);

      // do draw
      col = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);
      cmd->SetGraphicsRoot32BitConstants(0, 4, &col, 0);
      col = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
      cmd->SetGraphicsRoot32BitConstants(0, 4, &col, 4);
      cmd->DrawInstanced(3, 1, 0, 0);

      D3D12_CPU_DESCRIPTOR_HANDLE indirectarray_dynamic[2];
      indirectarray_dynamic[0] = MakeRTV(rtvtex[2]).CreateCPU(4);
      MakeRTV(rtvtex[3]).CreateCPU(5);
      MakeRTV(rtvtex[badrt]).CreateCPU(6);
      indirectarray_dynamic[1] = MakeRTV(rtvtex[badrt]).CreateCPU(7);

      cmd->OMSetRenderTargets(2, indirectarray_dynamic, TRUE, NULL);

      MakeRTV(rtvtex[badrt]).CreateCPU(0);
      MakeRTV(rtvtex[badrt]).CreateCPU(2);

      col = Vec4f(1.0f, 1.0f, 0.0f, 1.0f);
      cmd->SetGraphicsRoot32BitConstants(0, 4, &col, 0);
      col = Vec4f(0.0f, 1.0f, 1.0f, 1.0f);
      cmd->SetGraphicsRoot32BitConstants(0, 4, &col, 4);
      cmd->DrawInstanced(3, 1, 0, 0);

      // now repeat with the static tests, without any trashing, to ensure they are referenced
      // properly.

      cmd->OMSetRenderTargets(2, directarray_static, FALSE, NULL);

      col = Vec4f(1.0f, 0.0f, 0.0f, 1.0f);
      cmd->SetGraphicsRoot32BitConstants(0, 4, &col, 0);
      col = Vec4f(0.0f, 0.0f, 1.0f, 1.0f);
      cmd->SetGraphicsRoot32BitConstants(0, 4, &col, 4);
      cmd->DrawInstanced(3, 1, 0, 0);

      cmd->OMSetRenderTargets(2, indirectarray_static, TRUE, NULL);

      col = Vec4f(1.0f, 1.0f, 0.0f, 1.0f);
      cmd->SetGraphicsRoot32BitConstants(0, 4, &col, 0);
      col = Vec4f(0.0f, 1.0f, 1.0f, 1.0f);
      cmd->SetGraphicsRoot32BitConstants(0, 4, &col, 4);
      cmd->DrawInstanced(3, 1, 0, 0);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
