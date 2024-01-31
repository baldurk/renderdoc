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

RD_TEST(D3D12_Simple_Dispatch, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Test that just does a dispatch and some copies, for checking basic compute stuff";

  std::string compute = R"EOSHADER(

Texture2D<uint> texin : register(t0);
RWTexture2D<uint> texout : register(u0);

[numthreads(1,1,1)]
void main()
{
	texout[uint2(3,4)] = texin[uint2(4,3)];
	texout[uint2(4,4)] = texin[uint2(3,3)];
	texout[uint2(4,3)] = texin[uint2(3,4)];
	texout[uint2(3,3)] = texin[uint2(4,4)];
	texout[uint2(0,0)] = texin[uint2(0,0)] + 3;
}

)EOSHADER";

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    ID3DBlobPtr csblob = Compile(compute, "main", "cs_5_0");

    ID3D12RootSignaturePtr sig = MakeSig({
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1, 0),
        tableParam(D3D12_SHADER_VISIBILITY_ALL, D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 0, 1, 1),
    });
    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).CS(csblob);

    ID3D12ResourcePtr texin = MakeTexture(DXGI_FORMAT_R32_UINT, 8, 8)
                                  .InitialState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
                                  .UAV();
    ID3D12ResourcePtr texout = MakeTexture(DXGI_FORMAT_R32_UINT, 8, 8)
                                   .InitialState(D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
                                   .UAV();
    D3D12_TEXTURE_COPY_LOCATION dstLocation;
    dstLocation.pResource = texin;
    dstLocation.SubresourceIndex = 0;
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

    MakeSRV(texin).CreateGPU(0);
    MakeUAV(texout).CreateGPU(1);

    D3D12_TEXTURE_COPY_LOCATION srcLocation;
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    D3D12_RESOURCE_DESC srcDesc = texin->GetDesc();
    dev->GetCopyableFootprints(&srcDesc, 0, 1, 0, &srcLocation.PlacedFootprint, NULL, NULL, NULL);
    UINT dataSize = srcLocation.PlacedFootprint.Footprint.RowPitch *
                    srcLocation.PlacedFootprint.Footprint.Height;
    std::vector<uint32_t> data;
    data.reserve(dataSize);
    for(size_t i = 0; i < dataSize; i++)
    {
      data.push_back(5 + rand() % 100);
    }

    ID3D12ResourcePtr copybuffer = MakeBuffer().Data(data).Upload();
    srcLocation.pResource = copybuffer;

    D3D12_RESOURCE_BARRIER toDestState[2] = {};
    toDestState[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toDestState[0].Transition.pResource = texin;
    toDestState[0].Transition.Subresource = 0;
    toDestState[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    toDestState[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    toDestState[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toDestState[1].Transition.pResource = texout;
    toDestState[1].Transition.Subresource = 0;
    toDestState[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    toDestState[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;

    D3D12_RESOURCE_BARRIER toUseState[2] = {};
    toUseState[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toUseState[0].Transition.pResource = texin;
    toUseState[0].Transition.Subresource = 0;
    toUseState[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    toUseState[0].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    toUseState[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toUseState[1].Transition.pResource = texout;
    toUseState[1].Transition.Subresource = 0;
    toUseState[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    toUseState[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();
      Reset(cmd);

      {
        cmd->ResourceBarrier(2, toDestState);
        cmd->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);
        dstLocation.pResource = texout;
        cmd->CopyTextureRegion(&dstLocation, 0, 0, 0, &srcLocation, NULL);
        cmd->ResourceBarrier(2, toUseState);
      }

      cmd->SetComputeRootSignature(sig);
      cmd->SetPipelineState(pso);
      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
      cmd->SetComputeRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetComputeRootDescriptorTable(1, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->Dispatch(1, 1, 1);

      cmd->Close();
      Submit({cmd});
      Present();
    }

    return 0;
  }
};

REGISTER_TEST();
