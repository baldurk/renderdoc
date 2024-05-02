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

#include "../d3d11/d3d11_test.h"
#include "d3d12_test.h"

RD_TEST(D3D12_Sharing, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Tests D3D12 sharing between devices, both between D3D11 and D3D12 via shared handles "
      "as well as making sure that multiple devices created on the same adapter are implicitly "
      "identical.";

  D3D11GraphicsTest d3d11;

  void Prepare(int argc, char **argv)
  {
    d3d11.headless = true;

    d3d11.Prepare(argc, argv);

    D3D12GraphicsTest::Prepare(argc, argv);

    if(m_12On7)
      Avail = "Shared resources not implemented on D3D12On7";
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    LUID luid = dev->GetAdapterLuid();
    IDXGIAdapterPtr pDXGIAdapter;
    HRESULT hr = EnumAdapterByLuid(dev->GetAdapterLuid(), pDXGIAdapter);
    if(FAILED(hr))
      return 2;

    if(!d3d11.Init(pDXGIAdapter))
      return 4;

    ID3D12DevicePtr devB = CreateDevice({pDXGIAdapter}, D3D_FEATURE_LEVEL_11_0);
    if(!devB)
      return 2;

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(D3DDefaultPixel, "main", "ps_4_0");

    // share the VB from D3D11 to D3D12
    const DefaultA2V tri[3] = {
        {Vec3f(0.0f, 0.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 0.0f)},
        {Vec3f(3.0f, 0.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(0.0f, 1.0f)},
        {Vec3f(0.0f, -3.0f, 0.0f), Vec4f(0.0f, 1.0f, 0.0f, 1.0f), Vec2f(1.0f, 0.0f)},
    };

    ID3D11BufferPtr d3d11vb = d3d11.MakeBuffer().Shared().Data(tri);

    IDXGIResourcePtr dxgi = d3d11vb;
    HANDLE handle = NULL;
    dxgi->GetSharedHandle(&handle);

    ID3D12ResourcePtr d3d12vb;
    dev->OpenSharedHandle(handle, __uuidof(ID3D12Resource), (void **)&d3d12vb);

    ID3D12RootSignaturePtr sig = MakeSig({});

    // swap dev with devB, to force pso to be created on the 'second' device (should be identical to
    // the first if not using dynamic DLLs). This may be completely redundant as we might have two
    // identical pointers, but that's not guaranteed.
    if(!devFactory)
      std::swap(dev, devB);

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);

    // set them back
    if(!devFactory)
      std::swap(dev, devB);

    ResourceBarrier(d3d12vb, D3D12_RESOURCE_STATE_COMMON,
                    D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    // share the 'backbuffer' texture with d3d11
    ID3D12ResourcePtr d3d12tex =
        MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, screenWidth / 2, screenHeight / 2)
            .RTV()
            .InitialState(D3D12_RESOURCE_STATE_COPY_SOURCE)
            .Shared();

    dev->CreateSharedHandle(d3d12tex, NULL, GENERIC_ALL, NULL, &handle);

    ID3D11Texture2DPtr d3d11tex;
    d3d11.dev1->OpenSharedResource1(handle, __uuidof(ID3D11Texture2D), (void **)&d3d11tex);

    dev->CreateSharedHandle(m_GPUSyncFence, NULL, GENERIC_ALL, NULL, &handle);

    ID3D11FencePtr d3d11fence;
    d3d11.dev5->OpenSharedFence(handle, __uuidof(ID3D11Fence), (void **)&d3d11fence);

    while(Running())
    {
      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      ClearRenderTargetView(cmd, rtv, {1.0f, 0.0f, 0.0f, 1.0f});

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, d3d12vb, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);

      RSSetViewport(cmd, {0.0f, 0.0f, (float)screenWidth, (float)screenHeight, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {rtv}, {});

      cmd->DrawInstanced(3, 1, 0, 0);

      ResourceBarrier(cmd, d3d12tex, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON);

      cmd->Close();

      // submit (and set the fence value)
      Submit({cmd});

      // d3d11 waits on the fence, updates the offscreen texture, and signals the next fence
      // value
      d3d11.ctx4->Wait(d3d11fence, m_GPUSyncCounter);
      D3D11_BOX box = {0, 0, 0, UINT(screenWidth / 2), UINT(screenHeight / 2), 1};
      uint32_t *updateData = new uint32_t[box.right * box.bottom];
      memset(updateData, 0xff, box.right * box.bottom * sizeof(uint32_t));
      d3d11.ctx4->UpdateSubresource(d3d11tex, 0, &box, updateData, sizeof(uint32_t) * box.right,
                                    sizeof(uint32_t) * box.right * box.bottom);
      m_GPUSyncCounter++;
      d3d11.ctx4->Signal(d3d11fence, m_GPUSyncCounter);
      d3d11.ctx4->Flush();

      // wait on the fence from d3d11's work then continue
      queue->Wait(m_GPUSyncFence, m_GPUSyncCounter);

      delete[] updateData;
      updateData = NULL;

      cmd = GetCommandBuffer();

      Reset(cmd);

      ResourceBarrier(cmd, d3d12tex, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE);
      ResourceBarrier(cmd, bb, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COMMON);

      setMarker(cmd, "Copy");

      // blit texture to the backbuffer
      D3D12_TEXTURE_COPY_LOCATION dst = {bb, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
      D3D12_TEXTURE_COPY_LOCATION src = {d3d12tex, D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX};
      cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

      OMSetRenderTargets(cmd, {rtv}, {});

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_COPY_DEST);

      cmd->Close();

      Submit({cmd});

      Present();
    }

    devB = NULL;

    return 0;
  }
};

REGISTER_TEST();
