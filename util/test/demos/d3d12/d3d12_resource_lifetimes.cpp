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

TEST(D3D12_Resource_Lifetimes, D3D12GraphicsTest)
{
  static constexpr const char *Description =
      "Test various edge-case resource lifetimes: a resource that is first dirtied within a frame "
      "so needs initial contents created for it, and a resource that is created and destroyed "
      "mid-frame (which also gets dirtied after use).";

  std::string pixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

Texture2D smiley : register(t0);
Texture2D checker : register(t1);
SamplerState samp : register(s0);

cbuffer consts : register(b0)
{
  float4 flags;
};

float4 main(v2f IN) : SV_Target0
{
  if(flags.x != 1.0f || flags.y != 2.0f || flags.z != 4.0f || flags.w != 8.0f)
    return float4(1.0f, 0.0f, 1.0f, 1.0f);

	return smiley.Sample(samp, IN.uv * 2.0f) * checker.Sample(samp, IN.uv * 5.0f);
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

    D3D12_STATIC_SAMPLER_DESC samp = {
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        0.0f,
        0,
        D3D12_COMPARISON_FUNC_ALWAYS,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
        0.0f,
        0.0f,
        0,
        0,
        D3D12_SHADER_VISIBILITY_PIXEL,
    };

    ID3D12RootSignaturePtr sig = MakeSig(
        {
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 0, 1, 0),
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, 1, 1),
            tableParam(D3D12_SHADER_VISIBILITY_PIXEL, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 0, 1, 2),
        },
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT, 1, &samp);

    ID3D12PipelineStatePtr pso = MakePSO().RootSig(sig).InputLayout().VS(vsblob).PS(psblob);

    ResourceBarrier(vb, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

    ID3D12ResourcePtr uploadBuf = MakeBuffer().Size(1024 * 1024).Upload();

    Texture rgba8;
    LoadXPM(SmileyTexture, rgba8);

    ID3D12ResourcePtr smiley = MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 48, 48)
                                   .Mips(1)
                                   .InitialState(D3D12_RESOURCE_STATE_COPY_DEST);

    {
      D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};

      D3D12_RESOURCE_DESC desc = smiley->GetDesc();

      dev->GetCopyableFootprints(&desc, 0, 1, 0, &layout, NULL, NULL, NULL);

      byte *srcptr = (byte *)rgba8.data.data();
      byte *mapptr = NULL;
      uploadBuf->Map(0, NULL, (void **)&mapptr);

      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      {
        D3D12_TEXTURE_COPY_LOCATION dst, src;

        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.pResource = smiley;
        dst.SubresourceIndex = 0;

        byte *dstptr = mapptr + layout.Offset;

        for(UINT row = 0; row < rgba8.height; row++)
        {
          memcpy(dstptr, srcptr, rgba8.width * sizeof(uint32_t));
          srcptr += rgba8.width * sizeof(uint32_t);
          dstptr += layout.Footprint.RowPitch;
        }

        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.pResource = uploadBuf;
        src.PlacedFootprint = layout;

        // copy buffer into this array slice
        cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

        // this slice now needs to be in shader-read to copy to the MSAA texture
        D3D12_RESOURCE_BARRIER b = {};
        b.Transition.pResource = smiley;
        b.Transition.Subresource = 0;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        cmd->ResourceBarrier(1, &b);
      }

      cmd->Close();

      uploadBuf->Unmap(0, NULL);

      Submit({cmd});
      GPUSync();
    }

    auto SetupBuf = [this]() {
      const Vec4f flags = {1.0f, 2.0f, 4.0f, 8.0f};

      ID3D12ResourcePtr ret = MakeBuffer().Size(1024).Upload();

      void *pData = NULL;
      ret->Map(0, NULL, &pData);
      memcpy(pData, &flags, sizeof(Vec4f));
      ret->Unmap(0, NULL);

      return ret;
    };

    auto TrashBuf = [this](ID3D12ResourcePtr &cb) {
      const Vec4f empty = {};

      void *pData = NULL;
      cb->Map(0, NULL, &pData);
      memcpy(pData, &empty, sizeof(Vec4f));
      cb->Unmap(0, NULL);

      cb = NULL;
    };

    auto SetupImg = [this, &uploadBuf]() {
      const Vec4f flags = {1.0f, 2.0f, 4.0f, 8.0f};

      ID3D12ResourcePtr ret = MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 4, 4)
                                  .Mips(1)
                                  .InitialState(D3D12_RESOURCE_STATE_COPY_DEST);

      const uint32_t checker[4 * 4] = {
          // X X O O
          0xffffffff, 0xffffffff, 0, 0,
          // X X O O
          0xffffffff, 0xffffffff, 0, 0,
          // O O X X
          0, 0, 0xffffffff, 0xffffffff,
          // O O X X
          0, 0, 0xffffffff, 0xffffffff,
      };

      {
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};

        D3D12_RESOURCE_DESC desc = ret->GetDesc();

        dev->GetCopyableFootprints(&desc, 0, 1, 0, &layout, NULL, NULL, NULL);

        byte *srcptr = (byte *)checker;
        byte *mapptr = NULL;
        uploadBuf->Map(0, NULL, (void **)&mapptr);

        ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

        Reset(cmd);

        {
          D3D12_TEXTURE_COPY_LOCATION dst, src;

          dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
          dst.pResource = ret;
          dst.SubresourceIndex = 0;

          byte *dstptr = mapptr + layout.Offset;

          for(UINT row = 0; row < 4; row++)
          {
            memcpy(dstptr, srcptr, 4 * sizeof(uint32_t));
            srcptr += 4 * sizeof(uint32_t);
            dstptr += layout.Footprint.RowPitch;
          }

          src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
          src.pResource = uploadBuf;
          src.PlacedFootprint = layout;

          // copy buffer into this array slice
          cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);

          // this slice now needs to be in shader-read to copy to the MSAA texture
          D3D12_RESOURCE_BARRIER b = {};
          b.Transition.pResource = ret;
          b.Transition.Subresource = 0;
          b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
          b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
          cmd->ResourceBarrier(1, &b);
        }

        cmd->Close();

        uploadBuf->Unmap(0, NULL);

        Submit({cmd});
        GPUSync();
      }

      return ret;
    };

    auto TrashImg = [this, &uploadBuf](ID3D12ResourcePtr &img) {
      const uint32_t empty[4 * 4] = {};

      {
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};

        D3D12_RESOURCE_DESC desc = img->GetDesc();

        dev->GetCopyableFootprints(&desc, 0, 1, 0, &layout, NULL, NULL, NULL);

        byte *srcptr = (byte *)empty;
        byte *mapptr = NULL;
        uploadBuf->Map(0, NULL, (void **)&mapptr);

        ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

        Reset(cmd);

        {
          D3D12_TEXTURE_COPY_LOCATION dst, src;

          dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
          dst.pResource = img;
          dst.SubresourceIndex = 0;

          byte *dstptr = mapptr + layout.Offset;

          for(UINT row = 0; row < 4; row++)
          {
            memcpy(dstptr, srcptr, 4 * sizeof(uint32_t));
            srcptr += 4 * sizeof(uint32_t);
            dstptr += layout.Footprint.RowPitch;
          }

          src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
          src.pResource = uploadBuf;
          src.PlacedFootprint = layout;

          D3D12_RESOURCE_BARRIER b = {};
          b.Transition.pResource = img;
          b.Transition.Subresource = 0;
          b.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
          b.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
          cmd->ResourceBarrier(1, &b);

          // copy buffer into this array slice
          cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
        }

        cmd->Close();

        uploadBuf->Unmap(0, NULL);

        Submit({cmd});
        GPUSync();
      }

      img = NULL;
    };

    auto SetupDescHeap = [this, smiley](ID3D12ResourcePtr cb, ID3D12ResourcePtr tex) {
      D3D12_DESCRIPTOR_HEAP_DESC descheapdesc;
      descheapdesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
      descheapdesc.NodeMask = 1;
      descheapdesc.NumDescriptors = 8;
      descheapdesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

      ID3D12DescriptorHeapPtr descheap;
      dev->CreateDescriptorHeap(&descheapdesc, __uuidof(ID3D12DescriptorHeap), (void **)&descheap);

      D3D12_CPU_DESCRIPTOR_HANDLE cpu = descheap->GetCPUDescriptorHandleForHeapStart();

      {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;

        dev->CreateShaderResourceView(smiley, &desc, cpu);
      }

      cpu.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

      {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipLevels = 1;

        dev->CreateShaderResourceView(tex, &desc, cpu);
      }

      cpu.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

      {
        D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
        desc.BufferLocation = cb->GetGPUVirtualAddress();
        desc.SizeInBytes = 1024;

        dev->CreateConstantBufferView(&desc, cpu);
      }

      return descheap;
    };

    auto TrashDescHeap = [this](ID3D12DescriptorHeapPtr &descheap) {
      D3D12_CPU_DESCRIPTOR_HANDLE cpu = descheap->GetCPUDescriptorHandleForHeapStart();

      D3D12_SHADER_RESOURCE_VIEW_DESC srvdesc = {};
      srvdesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      srvdesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srvdesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvdesc.Texture2D.MipLevels = 1;

      dev->CreateShaderResourceView(NULL, &srvdesc, cpu);

      cpu.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

      dev->CreateShaderResourceView(NULL, &srvdesc, cpu);

      cpu.ptr += dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

      {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvdesc = {};
        cbvdesc.BufferLocation = 0;
        cbvdesc.SizeInBytes = 1024;

        dev->CreateConstantBufferView(&cbvdesc, cpu);
      }

      descheap = NULL;
    };

    ID3D12ResourcePtr cb = SetupBuf();
    ID3D12ResourcePtr img = SetupImg();
    ID3D12DescriptorHeapPtr descheap = SetupDescHeap(cb, img);
    while(Running())
    {
      D3D12_CPU_DESCRIPTOR_HANDLE rtv;

      // acquire and clear the backbuffer
      {
        ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

        Reset(cmd);

        ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

        rtv = MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

        ClearRenderTargetView(cmd, rtv, {0.4f, 0.5f, 0.6f, 1.0f});

        cmd->Close();

        Submit({cmd});

        GPUSync();
      }

      // render with last frame's resources
      {
        ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

        Reset(cmd);

        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
        cmd->SetPipelineState(pso);
        cmd->SetGraphicsRootSignature(sig);

        RSSetViewport(cmd, {0.0f, 0.0f, 128.0f, 128.0f, 0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

        OMSetRenderTargets(cmd, {rtv}, {});

        cmd->SetDescriptorHeaps(1, &descheap.GetInterfacePtr());
        cmd->SetGraphicsRootDescriptorTable(0, descheap->GetGPUDescriptorHandleForHeapStart());
        cmd->SetGraphicsRootDescriptorTable(1, descheap->GetGPUDescriptorHandleForHeapStart());
        cmd->SetGraphicsRootDescriptorTable(2, descheap->GetGPUDescriptorHandleForHeapStart());
        cmd->DrawInstanced(3, 1, 0, 0);

        cmd->Close();

        Submit({cmd});

        GPUSync();

        TrashBuf(cb);
        TrashImg(img);
        TrashDescHeap(descheap);
      }

      // create resources mid-frame and use then trash them
      {
        cb = SetupBuf();
        img = SetupImg();
        descheap = SetupDescHeap(cb, img);

        GPUSync();

        ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

        Reset(cmd);

        cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
        cmd->SetPipelineState(pso);
        cmd->SetGraphicsRootSignature(sig);

        RSSetViewport(cmd, {128.0f, 0.0f, 128.0f, 128.0f, 0.0f, 1.0f});
        RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

        OMSetRenderTargets(cmd, {rtv}, {});

        cmd->SetDescriptorHeaps(1, &descheap.GetInterfacePtr());
        cmd->SetGraphicsRootDescriptorTable(0, descheap->GetGPUDescriptorHandleForHeapStart());
        cmd->SetGraphicsRootDescriptorTable(1, descheap->GetGPUDescriptorHandleForHeapStart());
        cmd->SetGraphicsRootDescriptorTable(2, descheap->GetGPUDescriptorHandleForHeapStart());
        cmd->DrawInstanced(3, 1, 0, 0);

        cmd->Close();

        Submit({cmd});

        GPUSync();

        TrashBuf(cb);
        TrashImg(img);
        TrashDescHeap(descheap);
      }

      // finish with the backbuffer
      {
        ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

        Reset(cmd);

        FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

        cmd->Close();

        Submit({cmd});

        GPUSync();
      }

      // set up resources for next frame
      cb = SetupBuf();
      img = SetupImg();
      descheap = SetupDescHeap(cb, img);

      Present();
    }

    TrashBuf(cb);
    TrashImg(img);
    TrashDescHeap(descheap);

    return 0;
  }
};

REGISTER_TEST();
