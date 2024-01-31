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

RD_TEST(D3D12_Render_Pass, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Tests rendering with D3D12 render passes, with load and clear loadops.";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");

    const DefaultA2V tri[3] = {
        {Vec3f(-0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(0.0f, 0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.5f, -0.5f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    ID3D12ResourcePtr vb = MakeBuffer().Data(tri);

    ID3D12RootSignaturePtr sig = MakeSig({});

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);
    ID3D12PipelineStatePtr mspso =
        MakePSO().RootSig(sig).SampleCount(4).InputLayout().VS(vsblob).PS(psblob);

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ID3D12ResourcePtr rtv1tex =
        MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, screenWidth / 2, screenHeight / 2)
            .RTV()
            .InitialState(D3D12_RESOURCE_STATE_COPY_SOURCE);
    rtv1tex->SetName(L"rtv1tex");

    ID3D12ResourcePtr rtv2tex =
        MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, screenWidth / 2, screenHeight / 2)
            .Multisampled(4)
            .RTV()
            .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    rtv2tex->SetName(L"rtv2tex");
    ID3D12ResourcePtr rtv2resolve =
        MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, screenWidth / 2, screenHeight / 2)
            .RTV()
            .InitialState(D3D12_RESOURCE_STATE_COPY_SOURCE);
    rtv2resolve->SetName(L"rtv2resolve");

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ResourceBarrier(cmd, rtv1tex, D3D12_RESOURCE_STATE_COPY_SOURCE,
                      D3D12_RESOURCE_STATE_RENDER_TARGET);
      ResourceBarrier(cmd, rtv2resolve, D3D12_RESOURCE_STATE_COPY_SOURCE,
                      D3D12_RESOURCE_STATE_RESOLVE_DEST);

      pushMarker(cmd, "RP 1");

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth / 2, (float)screenHeight / 2, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth / 2, screenHeight / 2});

      ID3D12GraphicsCommandList4Ptr cmd4 = cmd;

      D3D12_RENDER_PASS_RENDER_TARGET_DESC rpRTV;
      rpRTV.cpuDescriptor = MakeRTV(rtv1tex).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);
      rpRTV.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
      rpRTV.BeginningAccess.Clear.ClearValue.Color[0] = 0.0f;
      rpRTV.BeginningAccess.Clear.ClearValue.Color[1] = 0.0f;
      rpRTV.BeginningAccess.Clear.ClearValue.Color[2] = 1.0f;
      rpRTV.BeginningAccess.Clear.ClearValue.Color[3] = 1.0f;
      rpRTV.BeginningAccess.Clear.ClearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
      rpRTV.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;

      cmd4->BeginRenderPass(1, &rpRTV, NULL, D3D12_RENDER_PASS_FLAG_NONE);

      cmd->DrawInstanced(3, 1, 0, 0);

      cmd4->EndRenderPass();

      popMarker(cmd);

      pushMarker(cmd, "RP 2");

      rpRTV.cpuDescriptor = MakeRTV(rtv2tex).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);
      rpRTV.BeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
      rpRTV.EndingAccess.Type = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE;
      rpRTV.EndingAccess.Resolve.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
      rpRTV.EndingAccess.Resolve.pDstResource = rtv2resolve;
      rpRTV.EndingAccess.Resolve.PreserveResolveSource = TRUE;
      rpRTV.EndingAccess.Resolve.pSrcResource = rtv2tex;
      D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS subParams = {};
      subParams.SrcRect.right = screenWidth / 2;
      subParams.SrcRect.bottom = screenHeight / 2;
      rpRTV.EndingAccess.Resolve.pSubresourceParameters = &subParams;
      rpRTV.EndingAccess.Resolve.ResolveMode = D3D12_RESOLVE_MODE_AVERAGE;
      rpRTV.EndingAccess.Resolve.SubresourceCount = 1;

      ClearRenderTargetView(cmd, rtv2tex, {1.0f, 0.0f, 1.0f, 1.0f});
      // ClearRenderTargetView(cmd, rtv2resolve, {1.0f, 0.0f, 1.0f, 1.0f});

      cmd4->BeginRenderPass(1, &rpRTV, NULL, D3D12_RENDER_PASS_FLAG_NONE);

      cmd->SetPipelineState(mspso);
      cmd->DrawInstanced(3, 1, 0, 0);

      cmd4->EndRenderPass();

      popMarker(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.0f, 0.0f, 0.0f, 1.0f});

      ResourceBarrier(cmd, bb, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
      ResourceBarrier(cmd, rtv1tex, D3D12_RESOURCE_STATE_RENDER_TARGET,
                      D3D12_RESOURCE_STATE_COPY_SOURCE);
      ResourceBarrier(cmd, rtv2resolve, D3D12_RESOURCE_STATE_RESOLVE_DEST,
                      D3D12_RESOURCE_STATE_COPY_SOURCE);

      D3D12_TEXTURE_COPY_LOCATION src, dst;
      src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      src.pResource = rtv1tex;
      src.SubresourceIndex = 0;

      dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      dst.pResource = bb;
      dst.SubresourceIndex = 0;

      cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

      src.pResource = rtv2resolve;

      cmd->CopyTextureRegion(&dst, screenWidth / 2, screenHeight / 2, 0, &src, NULL);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_COPY_DEST);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
