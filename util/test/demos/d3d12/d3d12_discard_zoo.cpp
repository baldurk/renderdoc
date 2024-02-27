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

RD_TEST(D3D12_Discard_Zoo, D3D12GraphicsTest)
{
  static constexpr const char *Description = "Tests texture discarding resources in D3D12.";

  std::string pixel = R"EOSHADER(

struct v2f
{
	float4 pos : SV_POSITION;
	float4 col : COLOR0;
	float2 uv : TEXCOORD0;
};

Texture2D smiley : register(t0);
SamplerState smileysamp : register(s0);

cbuffer consts : register(b0)
{
  float4 tint;
};

float4 main(v2f IN) : SV_Target0
{
	return smiley.Sample(smileysamp, IN.uv) * tint;
}

)EOSHADER";

  ID3D12ResourcePtr emptyRes;

  void Clear(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr tex)
  {
    if(!tex)
      return;

    D3D12_RESOURCE_DESC desc = tex->GetDesc();

    if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    {
      for(UINT mip = 0; mip < desc.MipLevels; mip++)
      {
        UINT slices = desc.DepthOrArraySize;
        if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
          slices = std::max(1U, slices >> mip);
        for(UINT slice = 0; slice < slices; slice++)
        {
          D3D12_CPU_DESCRIPTOR_HANDLE rtv = MakeRTV(tex)
                                                .Format(desc.Format)
                                                .FirstSlice(slice)
                                                .NumSlices(1)
                                                .FirstMip(mip)
                                                .NumMips(1)
                                                .CreateCPU(1);

          ClearRenderTargetView(cmd, rtv, {0.0f, 1.0f, 0.0f, 1.0f});
        }
      }
    }
    else if(desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    {
      for(UINT mip = 0; mip < desc.MipLevels; mip++)
      {
        for(UINT slice = 0; slice < desc.DepthOrArraySize; slice++)
        {
          D3D12_CPU_DESCRIPTOR_HANDLE dsv =
              MakeDSV(tex).FirstSlice(slice).NumSlices(1).FirstMip(mip).NumMips(1).CreateCPU(0);

          ClearDepthStencilView(cmd, dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.4f,
                                0x40);
        }
      }
    }
    else
    {
      for(UINT slice = 0; slice < desc.DepthOrArraySize; slice++)
      {
        for(UINT mip = 0; mip < desc.MipLevels; mip++)
        {
          D3D12_TEXTURE_COPY_LOCATION dst = {};
          dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
          dst.pResource = tex;
          dst.SubresourceIndex = slice * desc.MipLevels + mip;

          D3D12_TEXTURE_COPY_LOCATION src = {};
          src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
          src.pResource = emptyRes;
          dev->GetCopyableFootprints(&desc, dst.SubresourceIndex, 1, 0, &src.PlacedFootprint, NULL,
                                     NULL, NULL);

          cmd->CopyTextureRegion(&dst, 0, 0, 0, &src, NULL);
        }

        if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
          break;
      }
    }
  }

  void DiscardResource(ID3D12GraphicsCommandListPtr cmd, ID3D12ResourcePtr res, UINT firstSub = 0,
                       UINT numSub = ~0U, LONG x = 0, LONG y = 0, LONG width = 0, LONG height = 0)
  {
    D3D12_DISCARD_REGION reg = {};
    reg.FirstSubresource = firstSub;
    reg.NumSubresources = numSub;

    D3D12_RESOURCE_DESC desc = res->GetDesc();

    UINT arraySlices = desc.DepthOrArraySize;
    if(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
      arraySlices = 1;

    UINT planes = 1;
    if(desc.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT ||
       desc.Format == DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS ||
       desc.Format == DXGI_FORMAT_X32_TYPELESS_G8X24_UINT ||
       desc.Format == DXGI_FORMAT_R24G8_TYPELESS || desc.Format == DXGI_FORMAT_D24_UNORM_S8_UINT ||
       desc.Format == DXGI_FORMAT_R24_UNORM_X8_TYPELESS ||
       desc.Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT)
      planes = 2;

    reg.NumSubresources = std::min(reg.NumSubresources,
                                   (planes * desc.MipLevels * arraySlices) - reg.FirstSubresource);

    D3D12_RECT rect = {x, y, x + width, x + height};

    if(width > 0)
    {
      reg.NumRects = 1;
      reg.pRects = &rect;
    }

    cmd->DiscardResource(res, &reg);
  }

  int main()
  {
    // initialise, create window, create device, etc
    if(!Init())
      return 3;

    std::vector<byte> empty;
    empty.resize(16 * 1024 * 1024);
    {
      memset(empty.data(), 0x88, empty.size());

      emptyRes = MakeBuffer().Data(empty.data()).Size((UINT)empty.size());
    }

    ID3DBlobPtr vsblob = Compile(D3DDefaultVertex, "main", "vs_4_0");
    ID3DBlobPtr psblob = Compile(pixel, "main", "ps_4_0");

    ID3D12ResourcePtr vb = MakeBuffer().Data(DefaultTri);

    D3D12_STATIC_SAMPLER_DESC samp = {
        D3D12_FILTER_MIN_MAG_MIP_POINT,
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
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
            constParam(D3D12_SHADER_VISIBILITY_PIXEL, 0, 0, 16),
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

    MakeSRV(smiley).CreateCPU(0);

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

    ID3D12ResourcePtr buf = MakeBuffer().Data(empty).Size(1024);

    buf->SetName(L"Buffer");

    std::vector<ID3D12ResourcePtr> texs;

#define TEX_TEST(name, x)                                                                  \
  if(first)                                                                                \
  {                                                                                        \
    texs.push_back(x);                                                                     \
    Clear(cmd, texs.back());                                                               \
    texs.back()->SetName((L"Tex" + std::to_wstring(texs.size()) + L": " + +name).c_str()); \
  }                                                                                        \
  tex = texs[t++];

    ID3D12ResourcePtr tex1d = MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300)
                                  .Array(5)
                                  .Mips(3)
                                  .RTV()
                                  .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    ID3D12ResourcePtr tex3d = MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300, 15)
                                  .Mips(3)
                                  .RTV()
                                  .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    ID3D12ResourcePtr tex1drtv = MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300)
                                     .Array(5)
                                     .Mips(3)
                                     .RTV()
                                     .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);
    ID3D12ResourcePtr tex3drtv = MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300, 15)
                                     .Mips(3)
                                     .RTV()
                                     .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET);

    tex1d->SetName(L"Tex1D: DiscardAll");
    tex3d->SetName(L"Tex3D: DiscardAll");
    tex1drtv->SetName(L"Tex1D: DiscardAll Mip1 Slice1,2");
    tex3drtv->SetName(L"Tex3D: DiscardAll Mip1");

    bool first = true;

    while(Running())
    {
      if(!first)
      {
        ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

        Reset(cmd);

        pushMarker(cmd, "Clears");
        for(ID3D12ResourcePtr t : texs)
          Clear(cmd, t);

        Clear(cmd, tex1d);
        Clear(cmd, tex3d);
        Clear(cmd, tex1drtv);
        Clear(cmd, tex3drtv);
        popMarker(cmd);

        cmd->Close();

        Submit({cmd});

        SetBufferData(buf, D3D12_RESOURCE_STATE_COMMON, empty.data(), 1024);
      }

      ID3D12GraphicsCommandListPtr cmd = GetCommandBuffer();

      Reset(cmd);

      ID3D12ResourcePtr bb = StartUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      D3D12_CPU_DESCRIPTOR_HANDLE rtv =
          MakeRTV(bb).Format(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).CreateCPU(0);

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);

      RSSetViewport(cmd, {128.0f, 0.0f, 128.0f, 128.0f, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {rtv}, {});

      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      OMSetRenderTargets(cmd, {rtv}, {});

      Vec4f tint = {0.2f, 0.4f, 0.6f, 1.0f};
      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
      cmd->SetGraphicsRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetGraphicsRoot32BitConstants(1, 4, &tint, 0);

      // this is an anchor point for us to jump to and observe textures with all cleared contents
      // and no discard patterns
      setMarker(cmd, "TestStart");
      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      // discard the buffer first, we can't mess with rectangles or subresources for it
      DiscardResource(cmd, buf);

      int t = 0;
      ID3D12ResourcePtr tex;

      // test a few different formats
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R10G10B10A2_UNORM, 300, 300));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R10G10B10A2_UINT, 300, 300));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R9G9B9E5_SHAREDEXP, 300, 300));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R8G8B8A8_UNORM, 300, 300));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_BC1_UNORM, 300, 300));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_BC2_UNORM, 300, 300));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_BC3_UNORM, 300, 300));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_BC4_UNORM, 300, 300));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_BC5_UNORM, 300, 300));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_BC6H_UF16, 300, 300));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_BC7_UNORM, 300, 300));
      DiscardResource(cmd, tex);

      // test with discarding a NULL region
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300));
      cmd->DiscardResource(tex, NULL);
      // and with NULL rects
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300));
      {
        D3D12_DISCARD_REGION reg = {};
        reg.FirstSubresource = 0;
        reg.NumSubresources = 1;
        cmd->DiscardResource(tex, &reg);
      }

      // test with different mips/array sizes
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300).Mips(5));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300).Array(4));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300).Array(4).Mips(5));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 30, 5));
      DiscardResource(cmd, tex);

      // test MSAA textures
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300)
                                  .Multisampled(4)
                                  .RTV()
                                  .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300)
                                  .Multisampled(4)
                                  .Array(5)
                                  .RTV()
                                  .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_UINT, 300, 300)
                                  .Multisampled(4)
                                  .Array(5)
                                  .RTV()
                                  .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_R16G16B16A16_SINT, 300, 300)
                                  .Multisampled(4)
                                  .Array(5)
                                  .RTV()
                                  .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET));
      DiscardResource(cmd, tex);

      // test depth textures
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT, 300, 300)
                                  .DSV()
                                  .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300)
                                  .DSV()
                                  .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_D24_UNORM_S8_UINT, 300, 300)
                                  .DSV()
                                  .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT, 300, 300)
                                  .DSV()
                                  .Mips(5)
                                  .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT, 300, 300)
                                  .DSV()
                                  .Array(4)
                                  .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT, 300, 300)
                                  .DSV()
                                  .Array(4)
                                  .Mips(5)
                                  .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300)
                                  .DSV()
                                  .Mips(5)
                                  .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300)
                                  .DSV()
                                  .Array(4)
                                  .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300)
                                  .DSV()
                                  .Array(4)
                                  .Mips(5)
                                  .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300)
                                  .Multisampled(4)
                                  .DSV()
                                  .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex);
      TEX_TEST(L"DiscardAll", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300)
                                  .Multisampled(4)
                                  .Array(5)
                                  .DSV()
                                  .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex);

      // test discarding rects within a texture using DiscardView1. Only supported on RTVs and DSVs
      TEX_TEST(L"DiscardRect Mip0", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300)
                                        .RTV()
                                        .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET));
      DiscardResource(cmd, tex, 0, 1, 50, 50, 75, 75);
      TEX_TEST(L"DiscardRect Mip1", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300)
                                        .Mips(2)
                                        .RTV()
                                        .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET));
      DiscardResource(cmd, tex, 1, 1, 50, 50, 75, 75);

      TEX_TEST(L"DiscardRect Mip0", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300)
                                        .Mips(2)
                                        .DSV()
                                        .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex, 0, 1, 50, 50, 75, 75);    // depth mip0
      DiscardResource(cmd, tex, 2, 1, 50, 50, 75, 75);    // stencil mip1
      TEX_TEST(L"DiscardRect Mip1", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300)
                                        .Mips(2)
                                        .DSV()
                                        .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex, 1, 1, 50, 50, 75, 75);    // depth mip1
      DiscardResource(cmd, tex, 3, 1, 50, 50, 75, 75);    // stencil mip1

      TEX_TEST(L"DiscardAll Slice2", MakeTexture(DXGI_FORMAT_R16G16B16A16_FLOAT, 300, 300)
                                         .Multisampled(4)
                                         .Array(5)
                                         .RTV()
                                         .InitialState(D3D12_RESOURCE_STATE_RENDER_TARGET));
      DiscardResource(cmd, tex, 2, 1);

      // test discarding only depth or only stencil
      TEX_TEST(L"DiscardRect DepthOnly", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300)
                                             .DSV()
                                             .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex, 0, 1, 50, 50, 75, 75);
      TEX_TEST(L"DiscardRect StencilOnly", MakeTexture(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, 300, 300)
                                               .DSV()
                                               .InitialState(D3D12_RESOURCE_STATE_DEPTH_WRITE));
      DiscardResource(cmd, tex, 1, 1, 50, 50, 75, 75);

      // test 1D/3D textures
      DiscardResource(cmd, tex1d);
      DiscardResource(cmd, tex3d);

      DiscardResource(cmd, tex1drtv, 4, 1);    // mip 1, slice 1
      DiscardResource(cmd, tex1drtv, 7, 1);    // mip 1, slice 2
      DiscardResource(cmd, tex3drtv, 1, 1);    // mip 1

      setMarker(cmd, "TestEnd");
      ClearRenderTargetView(cmd, rtv, {0.2f, 0.2f, 0.2f, 1.0f});

      cmd->DrawInstanced(3, 1, 0, 0);

      cmd->Close();

      Submit({cmd});

      cmd = GetCommandBuffer();

      Reset(cmd);

      cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

      IASetVertexBuffer(cmd, vb, sizeof(DefaultA2V), 0);
      cmd->SetPipelineState(pso);
      cmd->SetGraphicsRootSignature(sig);

      RSSetViewport(cmd, {0.0f, 128.0f, 128.0f, 128.0f, 0.0f, 1.0f});
      RSSetScissorRect(cmd, {0, 0, screenWidth, screenHeight});

      OMSetRenderTargets(cmd, {rtv}, {});

      cmd->SetDescriptorHeaps(1, &m_CBVUAVSRV.GetInterfacePtr());
      cmd->SetGraphicsRootDescriptorTable(0, m_CBVUAVSRV->GetGPUDescriptorHandleForHeapStart());
      cmd->SetGraphicsRoot32BitConstants(1, 4, &tint, 0);

      cmd->DrawInstanced(3, 1, 0, 0);

      FinishUsingBackbuffer(cmd, D3D12_RESOURCE_STATE_RENDER_TARGET);

      cmd->Close();

      Submit({cmd});

      Present();

      first = false;
    }

    emptyRes = NULL;

    return 0;
  }
};

REGISTER_TEST();
