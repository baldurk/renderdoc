/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2023-2026 Baldur Karlsson
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

RD_TEST(D3D12_Stripped_Shaders, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Tests shaders with their debug/reflection info stripped out and stored in separate blobs.";

  void Prepare(int argc, char **argv)
  {
    D3D12GraphicsTest::Prepare(argc, argv);

    if(!Avail.empty())
      return;

    if(!m_DXILSupport)
      Avail = "DXIL can't be compiled";
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    MakeDir((GetCWD() + "/tmp/D3D12_Stripped_Shaders").c_str());

    // only a couple of variables:
    //
    // auto-named or manual named
    // absolute or relative path

    // manual debug name will be absolute
    ID3DBlobPtr vsblob =
        Compile(D3DDefaultVertex + "\n// shader source // vertex", "main", "vs_6_0",
                CompileOptionFlags::SeparateDebug | CompileOptionFlags::ManualDebugName);
    // auto name will be relative
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel + "\n// shader source // pixel", "main", "ps_6_0",
                                 CompileOptionFlags::SeparateDebug);

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(DefaultTriSig).InputLayout().VS(vsblob).PS(psblob);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      ClearRenderTargetView(cmd, BBRTV, {0.2f, 0.2f, 0.2f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, DefaultTriVB, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(DefaultTriSig);

      SetMainWindowViewScissor(cmd);

      OMSetRenderTargets(cmd, {BBRTV}, {});

      setMarker(cmd, "test: shader source");
      setMarker(cmd, "vertex manual absolute");
      setMarker(cmd, "pixel auto relative");
      cmd->DrawInstanced(3, 1, 0, 0);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      SubmitAndPresent({cmd});
    }

    return 0;
  }
};

REGISTER_TEST();
