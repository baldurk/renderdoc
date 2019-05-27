/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2018-2019 Baldur Karlsson
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

TEST(D3D12_Untyped_Backbuffer_Descriptor, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "D3D12 inherits D3D11's ability to have a mutable format on a non-typeless backbuffer "
      "texture. Test that this works for both a NULL descriptor and a descriptor with an UNKNOWN "
      "format";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile("float4 main() : SV_Target0 { return 1.0f; }", "main", "ps_4_0");

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);

    ID3D12RootSignaturePtr sig = MakeSig({});

    ID3D12PipelineStatePtr pso =
        MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob).RTVs({DXGI_FORMAT_R8G8B8A8_UNORM});

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      // create a descriptor with an UNKNOWN format
      D3D12_CPU_DESCRIPTOR_HANDLE rtv = MakeRTV(bb).Format(DXGI_FORMAT_UNKNOWN).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {0.4f, 0.5f, 0.6f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth / 2.0f, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {rtv}, {});

      cmd->DrawInstanced(3, 1, 0, 0);

      // create a descriptor with no descriptor at all
      dev->CreateRenderTargetView(bb, NULL, rtv);

      OMSetRenderTargets(cmd, {rtv}, {});

      RSSetViewport(cmd, {(float)screenWidth / 2.0f, 0.0f, (float)screenWidth / 2.0f,
                          (float)screenHeight, 0.0f, 1.0f});

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
