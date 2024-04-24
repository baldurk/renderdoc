/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2020-2024 Baldur Karlsson
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

RD_TEST(D3D12_Compute_Only, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Test that uses a compute only queue with no graphics queue";

  std::string compute = R"EOSHADER(

cbuffer blah : register(b0)
{
  uint4 mult;
};

RWStructuredBuffer<uint4> bufin : register(u0);
RWStructuredBuffer<uint4> bufout : register(u1);

[numthreads(1,1,1)]
void main()
{
  bufout[0].x += bufin[0].x * mult.x;
  bufout[0].y += bufin[0].y * mult.y;
  bufout[0].z += bufin[0].z * mult.z;
  bufout[0].w += bufin[0].w * mult.w;
}

)EOSHADER";

  int main()
  {
    headless = true;
    queueType = D3D12_COMMAND_LIST_TYPE_COMPUTE;

    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr csblob = Compile(compute, "main", "cs_5_0");

    ID3D12RootSignaturePtr sig = MakeSig({
        uavParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0),
        uavParam(D3D12_SHADER_VISIBILITY_ALL, 0, 1),
        constParam(D3D12_SHADER_VISIBILITY_ALL, 0, 0, 4),
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 2, 1, 3),
    });
    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).CS(csblob);

    ID3D12ResourcePtr bufin = MakeBuffer().Size(1024).UAV();
    ID3D12ResourcePtr bufout = MakeBuffer().Size(1024).UAV();

    bufin->SetName(L"bufin");
    bufout->SetName(L"bufout");

    ID3D12ResourcePtr tex = MakeTexture(DXGI_FORMAT_R32G32B32A32_FLOAT, 8, 8)
                                .InitialState(D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                                .UAV();

    tex->SetName(L"tex");

    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();
      Reset(cmd);

      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

      float col[] = {0.25f, 0.5f, 0.75f, 1.0f};

      D3D12_RECT rect = {};
      rect.right = rect.bottom = 8;
      cmd->ClearUnorderedAccessViewFloat(
          MakeUAV(tex).Format(DXGI_FORMAT_R32G32B32A32_FLOAT).CreateGPU(3),
          MakeUAV(tex).Format(DXGI_FORMAT_R32G32B32A32_FLOAT).CreateClearCPU(3), tex, col, 1, &rect);

      cmd->Close();
      Submit({cmd});
    }

    if(rdoc)
      rdoc->StartFrameCapture(NULL, NULL);

    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();
      Reset(cmd);

      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());

      uint32_t a[4] = {111, 111, 111, 111};
      uint32_t b[4] = {222, 222, 222, 222};

      D3D12_RECT rect = {};
      rect.right = 1024;
      rect.bottom = 1;
      cmd->ClearUnorderedAccessViewUint(
          MakeUAV(bufin).Format(DXGI_FORMAT_R32G32B32A32_UINT).CreateGPU(0),
          MakeUAV(bufin).Format(DXGI_FORMAT_R32G32B32A32_UINT).CreateClearCPU(0), bufin, a, 1, &rect);
      cmd->ClearUnorderedAccessViewUint(
          MakeUAV(bufout).Format(DXGI_FORMAT_R32G32B32A32_UINT).CreateGPU(1),
          MakeUAV(bufout).Format(DXGI_FORMAT_R32G32B32A32_UINT).CreateClearCPU(1), bufout, b, 1,
          &rect);

      setMarker(cmd, "Pre-Dispatch");

      cmd->SetComputeRootSignature(sig);
      cmd->SetPipelineState(pso);
      cmd->SetComputeRootUnorderedAccessView(0, bufin->GetGPUVirtualAddress());
      cmd->SetComputeRootUnorderedAccessView(1, bufout->GetGPUVirtualAddress());
      cmd->SetComputeRoot32BitConstant(2, 5, 0);
      cmd->SetComputeRoot32BitConstant(2, 6, 1);
      cmd->SetComputeRoot32BitConstant(2, 7, 2);
      cmd->SetComputeRoot32BitConstant(2, 8, 3);
      cmd->SetComputeRootDescriptorTable(3, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->Dispatch(1, 1, 1);

      setMarker(cmd, "Post-Dispatch");

      cmd->Close();
      Submit({cmd});
    }

    if(rdoc)
      rdoc->EndFrameCapture(NULL, NULL);

    GPUSync();

    msleep(1000);

    return 0;
  }
};

REGISTER_TEST();
