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

RD_TEST(D3D12_Large_Buffer, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Draws a triangle over the span of a very large buffer to ensure readbacks work correctly.";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");

    uint32_t indices[3] = {0, 1000000, 2345678};

    ID3D12ResourcePtr ib = MakeBuffer().Data(indices);

    ID3D12ResourcePtr vb = MakeBuffer().Size(128 * 1024 * 1024).Upload();

    D3D12_RANGE range = {0, 0};

    byte *ptr = NULL;
    CHECK_HR(vb->Map(0, &range, (void **)&ptr));

    DefaultA2V *verts = (DefaultA2V *)ptr;

    verts[indices[0]] = DefaultTri[0];
    verts[indices[1]] = DefaultTri[1];
    verts[indices[2]] = DefaultTri[2];

    range.End = (SIZE_T)vb->GetDesc().Width;

    vb->Unmap(0, &range);

    ID3D12RootSignaturePtr sig = MakeSig({});

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);

    ID3D12ResourcePtr rtvtex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 4)
                                   .RTV()
                                   .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      ClearRenderTargetView(cmd, MakeRTV(rtvtex).CreateCPU(1), {0.2f, 0.2f, 0.2f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      IASetIndexBuffer(cmd, ib, DXGI_FORMAT_R32_UINT, 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {rtv}, {});

      cmd->DrawIndexedInstanced(3, 1, 0, 0, 0);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
