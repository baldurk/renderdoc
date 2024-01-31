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

#include "../win32/win32_window.h"
#include "d3d12_test.h"

RD_TEST(D3D12_Swapchain_Zoo, D3D12GraphicsTest)
{
  static constexpr const char *Description = "Tests both types of swapchain that D3D12 supports.";

  void Prepare(int argc, char **argv)
  {
    D3D12GraphicsTest::Prepare(argc, argv);

    if(m_12On7)
      Avail = "True swapchains not supported on D3D12On7";
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    IDXGIFactory4Ptr factory4 = m_Factory;

    DXGI_SWAP_CHAIN_DESC1 swapDesc = {};

    swapDesc.BufferCount = 2;
    swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    swapDesc.Flags = 0;
    swapDesc.Format = backbufferFmt;
    swapDesc.Width = screenWidth;
    swapDesc.Height = screenHeight;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;
    swapDesc.Scaling = DXGI_SCALING_STRETCH;
    swapDesc.Stereo = FALSE;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

    // make second window
    Win32Window *window2 = new Win32Window(screenWidth, screenHeight, screenTitle);
    IDXGISwapChain1Ptr window2Swap;

    // make sequential swapchain with normal format
    swapDesc.Format = backbufferFmt;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    CHECK_HR(
        factory4->CreateSwapChainForHwnd(queue, window2->wnd, &swapDesc, NULL, NULL, &window2Swap));

    ID3D12ResourcePtr window2Tex[2];

    CHECK_HR(window2Swap->GetBuffer(0, __uuidof(ID3D12Resource), (void **)&window2Tex[0]));
    CHECK_HR(window2Swap->GetBuffer(1, __uuidof(ID3D12Resource), (void **)&window2Tex[1]));

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);

    ID3D12RootSignaturePtr sig = MakeSig({});

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    while(Running())
    {
      window2->Update();

      D3D12_CPU_DESCRIPTOR_HANDLE rtv;
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ResourceBarrier(cmd, window2Tex[texIdx], D3D12_RESOURCE_STATE_PRESENT,
                      D3D12_RESOURCE_STATE_RENDER_TARGET);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      rtv = MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.0f, 0.0f, 0.0f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {rtv}, {});
      setMarker(cmd, "Draw 1");
      cmd->DrawInstanced(3, 1, 0, 0);

      rtv = MakeRTV(window2Tex[texIdx]).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.0f, 0.0f, 0.0f, 1.0f});

      OMSetRenderTargets(cmd, {rtv}, {});
      setMarker(cmd, "Draw 2");
      cmd->DrawInstanced(3, 1, 0, 0);

      ResourceBarrier(cmd, window2Tex[texIdx], D3D12_RESOURCE_STATE_RENDER_TARGET,
                      D3D12_RESOURCE_STATE_PRESENT);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
      window2Swap->Present(0, 0);
    }

    delete window2;

    return 0;
  }
};

REGISTER_TEST();
